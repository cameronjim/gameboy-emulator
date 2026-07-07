#include "cartridge.hpp"
#include "cpu.hpp"
#include "gameboy.hpp"
#include "palette.hpp"
#include "trace.hpp"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr int kScale = 4;
constexpr int kWidth = static_cast<int>(gb::kLcdWidth);
constexpr int kHeight = static_cast<int>(gb::kLcdHeight);
// largest licensed gb rom is 8mb
constexpr std::streamoff kMaxRomFileSize = 8 * 1024 * 1024;

std::vector<uint8_t> read_file(const char* path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return {};
    }
    const std::streamoff size = file.tellg();
    if (size <= 0 || size > kMaxRomFileSize) {
        return {};
    }
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(bytes.data()), size);
    if (!file) {
        return {};
    }
    return bytes;
}

const char* cart_type_name(gb::CartType type) {
    switch (type) {
    case gb::CartType::RomOnly:
        return "rom_only";
    case gb::CartType::Mbc1:
        return "mbc1";
    case gb::CartType::Mbc3:
        return "mbc3";
    }
    return "unknown";
}

std::string save_path(const char* rom_path) {
    return std::string(rom_path) + ".sav";
}

void load_battery_ram(gb::Gameboy& gameboy, const char* rom_path) {
    const std::span<uint8_t> ram = gameboy.external_ram();
    if (!gameboy.has_battery() || ram.empty()) {
        return;
    }
    std::ifstream in(save_path(rom_path), std::ios::binary);
    if (!in) {
        return;
    }
    in.read(reinterpret_cast<char*>(ram.data()), static_cast<std::streamsize>(ram.size()));
    std::printf("loaded %s\n", save_path(rom_path).c_str());
}

void save_battery_ram(gb::Gameboy& gameboy, const char* rom_path) {
    const std::span<const uint8_t> ram = gameboy.external_ram();
    if (!gameboy.has_battery() || ram.empty()) {
        return;
    }
    std::ofstream out(save_path(rom_path), std::ios::binary);
    if (!out) {
        return;
    }
    out.write(reinterpret_cast<const char*>(ram.data()), static_cast<std::streamsize>(ram.size()));
}

bool print_cartridge(std::span<const uint8_t> bytes) {
    std::string reason;
    const std::optional<gb::Cartridge> cart = gb::Cartridge::parse(bytes, &reason);
    if (!cart) {
        std::fprintf(stderr, "cartridge rejected: %s\n", reason.c_str());
        return false;
    }
    std::string ram = "none";
    if (cart->ram_size() > 0) {
        ram = std::to_string(cart->ram_size() / 1024) + "KB";
    }
    const unsigned rom_kb = static_cast<unsigned>(cart->rom_size() / 1024);
    std::printf("title=%s type=%s rom=%uKB ram=%s checksum=ok\n", cart->title().c_str(),
                cart_type_name(cart->type()), rom_kb, ram.c_str());
    return true;
}

struct Options {
    const char* rom_path = nullptr;
    const char* doctor_path = nullptr;
    const char* dump_ppm_path = nullptr;
    const char* palette_name = "green";
    uint64_t trace_from = 0;
    uint64_t frames = 600;
    bool ok = true;
};

Options parse_args(int argc, char* argv[]) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (arg == "--doctor" && i + 1 < argc) {
            opt.doctor_path = argv[++i];
        } else if (arg == "--trace-from" && i + 1 < argc) {
            opt.trace_from = std::strtoull(argv[++i], nullptr, 10);
        } else if (arg == "--dump-ppm" && i + 1 < argc) {
            opt.dump_ppm_path = argv[++i];
        } else if (arg == "--frames" && i + 1 < argc) {
            opt.frames = std::strtoull(argv[++i], nullptr, 10);
        } else if (arg == "--palette" && i + 1 < argc) {
            opt.palette_name = argv[++i];
        } else if (!arg.empty() && arg[0] == '-') {
            opt.ok = false;
        } else {
            opt.rom_path = argv[i];
        }
    }
    return opt;
}

// flat scaffolding memory for doctor runs until the bus lands in milestone 04
struct FlatMemory final : gb::Memory {
    uint8_t read8(uint16_t addr) override {
        return mem[addr];
    }
    void write8(uint16_t addr, uint8_t value) override {
        mem[addr] = value;
    }

    std::array<uint8_t, 0x10000> mem{};
};

bool write_ppm(std::span<const uint8_t> fb, const Palette& palette, const char* path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        std::fprintf(stderr, "cannot open %s\n", path);
        return false;
    }
    out << "P6\n" << gb::kLcdWidth << " " << gb::kLcdHeight << "\n255\n";
    for (uint8_t index : fb) {
        const uint32_t rgb = map_shade(palette, index);
        const char px[3] = {static_cast<char>(rgb >> 16), static_cast<char>(rgb >> 8),
                            static_cast<char>(rgb)};
        out.write(px, 3);
    }
    std::printf("wrote %s\n", path);
    return true;
}

// debug hook: run headless for n frames, then dump the framebuffer as a ppm
int dump_framebuffer_ppm(gb::Gameboy& gameboy, uint64_t frames, const Palette& palette, const char* path) {
    for (uint64_t i = 0; i < frames; ++i) {
        gameboy.run_frame();
    }
    return write_ppm(gameboy.framebuffer(), palette, path) ? 0 : 1;
}

// second window: all 384 tiles as a 16x24 grid, decoded straight from vram
class TileViewer {
public:
    bool visible() const {
        return window_ != nullptr;
    }
    uint32_t window_id() const {
        return window_ != nullptr ? SDL_GetWindowID(window_) : 0;
    }
    void toggle() {
        if (visible()) {
            close();
            return;
        }
        window_ = SDL_CreateWindow("tiles", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, kTilesW * 3,
                                   kTilesH * 3, 0);
        if (window_ == nullptr) {
            return;
        }
        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
        const uint32_t format = SDL_PIXELFORMAT_ARGB8888;
        texture_ = SDL_CreateTexture(renderer_, format, SDL_TEXTUREACCESS_STREAMING, kTilesW, kTilesH);
    }
    void close() {
        if (texture_ != nullptr) {
            SDL_DestroyTexture(texture_);
            texture_ = nullptr;
        }
        if (renderer_ != nullptr) {
            SDL_DestroyRenderer(renderer_);
            renderer_ = nullptr;
        }
        if (window_ != nullptr) {
            SDL_DestroyWindow(window_);
            window_ = nullptr;
        }
    }
    void render(std::span<const uint8_t> vram, const Palette& palette) {
        if (!visible() || renderer_ == nullptr || texture_ == nullptr) {
            return;
        }
        for (uint32_t tile = 0; tile < 384; ++tile) {
            const uint32_t tx = (tile % 16) * 8;
            const uint32_t ty = (tile / 16) * 8;
            for (uint32_t row = 0; row < 8; ++row) {
                const uint8_t lo = vram[tile * 16 + row * 2];
                const uint8_t hi = vram[tile * 16 + row * 2 + 1];
                for (uint32_t px = 0; px < 8; ++px) {
                    const uint8_t color =
                        static_cast<uint8_t>((((hi >> (7 - px)) & 1) << 1) | ((lo >> (7 - px)) & 1));
                    pixels_[(ty + row) * kTilesW + tx + px] = map_shade(palette, color);
                }
            }
        }
        SDL_UpdateTexture(texture_, nullptr, pixels_.data(), kTilesW * 4);
        SDL_RenderClear(renderer_);
        SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
        SDL_RenderPresent(renderer_);
    }

private:
    static constexpr int kTilesW = 128;
    static constexpr int kTilesH = 192;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    std::array<uint32_t, kTilesW * kTilesH> pixels_{};
};

int run_doctor(std::span<const uint8_t> rom, const char* out_path, uint64_t trace_from) {
    // cap covers the longest cpu_instrs subtest; logs get huge, flush in blocks
    constexpr uint64_t kMaxInstructions = 8000000;
    constexpr size_t kFlushBytes = 1u << 20;

    FlatMemory flat;
    const size_t n = std::min(rom.size(), flat.mem.size());
    std::copy_n(rom.begin(), n, flat.mem.begin());
    gb::DoctorMemory mem(flat);
    gb::Cpu cpu(mem);
    gb::Trace trace(trace_from);

    std::ofstream out(out_path, std::ios::binary);
    if (!out) {
        std::fprintf(stderr, "cannot open %s\n", out_path);
        return 1;
    }
    std::string buffer;
    buffer.reserve(kFlushBytes + 128);
    for (uint64_t i = 0; i < kMaxInstructions && cpu.status() == gb::CpuStatus::Running; ++i) {
        trace.log(cpu.regs(), mem, buffer);
        cpu.step();
        if (buffer.size() >= kFlushBytes) {
            out.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            buffer.clear();
        }
    }
    out.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    if (cpu.status() == gb::CpuStatus::Stopped) {
        std::fprintf(stderr, "cpu trapped at pc=%04X opcode=%02X\n", cpu.trap_pc(), cpu.trap_opcode());
    }
    std::printf("doctor log: %llu instructions\n", static_cast<unsigned long long>(trace.count()));
    return 0;
}

} // namespace

int main(int argc, char* argv[]) {
    const Options opt = parse_args(argc, argv);
    if (!opt.ok || (opt.doctor_path != nullptr && opt.rom_path == nullptr)) {
        std::fprintf(stderr, "usage: gbemu-sdl [--doctor <path>] [--trace-from <n>] [--dump-ppm <path>] "
                             "[--frames <n>] [--palette green|gray] [rom]\n");
        return 1;
    }

    gb::Gameboy gameboy;
    if (opt.rom_path != nullptr) {
        const std::vector<uint8_t> bytes = read_file(opt.rom_path);
        if (bytes.empty()) {
            std::fprintf(stderr, "cannot read %s\n", opt.rom_path);
            return 1;
        }
        if (opt.doctor_path != nullptr) {
            // doctor scaffolding runs on flat memory, so mbc-typed test roms are fine
            return run_doctor(bytes, opt.doctor_path, opt.trace_from);
        }
        if (!print_cartridge(bytes)) {
            return 1;
        }
        if (!gameboy.load_rom(bytes)) {
            std::fprintf(stderr, "load_rom failed\n");
            return 1;
        }
        // test rom output channel
        gameboy.set_serial_sink([](uint8_t b) { std::fputc(b, stdout); });
        // host time injected once; the core stays clock-free
        gameboy.set_rtc_seconds(static_cast<uint64_t>(std::time(nullptr)));
        load_battery_ram(gameboy, opt.rom_path);
        if (opt.dump_ppm_path != nullptr) {
            return dump_framebuffer_ppm(gameboy, opt.frames, palette_by_name(opt.palette_name),
                                        opt.dump_ppm_path);
        }
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "sdl init failed: %s\n", SDL_GetError());
        return 1;
    }

    const int pos = SDL_WINDOWPOS_CENTERED;
    SDL_Window* window = SDL_CreateWindow("gbemu", pos, pos, kWidth * kScale, kHeight * kScale, 0);
    if (window == nullptr) {
        std::fprintf(stderr, "sdl window failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    const uint32_t renderer_flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, renderer_flags);
    if (renderer == nullptr) {
        std::fprintf(stderr, "sdl renderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    const uint32_t texture_format = SDL_PIXELFORMAT_ARGB8888;
    const int texture_access = SDL_TEXTUREACCESS_STREAMING;
    SDL_Texture* texture = SDL_CreateTexture(renderer, texture_format, texture_access, kWidth, kHeight);
    if (texture == nullptr) {
        std::fprintf(stderr, "sdl texture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    std::array<uint32_t, gb::kLcdWidth * gb::kLcdHeight> pixels{};

    const Palette& palette = palette_by_name(opt.palette_name);
    TileViewer tile_viewer;
    uint64_t frame_count = 0;
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            if ((event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) && event.key.repeat == 0) {
                const bool down = event.type == SDL_KEYDOWN;
                switch (event.key.keysym.sym) {
                case SDLK_RIGHT:
                    gameboy.set_button(gb::Button::Right, down);
                    break;
                case SDLK_LEFT:
                    gameboy.set_button(gb::Button::Left, down);
                    break;
                case SDLK_UP:
                    gameboy.set_button(gb::Button::Up, down);
                    break;
                case SDLK_DOWN:
                    gameboy.set_button(gb::Button::Down, down);
                    break;
                case SDLK_z:
                    gameboy.set_button(gb::Button::A, down);
                    break;
                case SDLK_x:
                    gameboy.set_button(gb::Button::B, down);
                    break;
                case SDLK_RETURN:
                    gameboy.set_button(gb::Button::Start, down);
                    break;
                case SDLK_RSHIFT:
                    gameboy.set_button(gb::Button::Select, down);
                    break;
                default:
                    break;
                }
            }
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                }
                if (event.key.keysym.sym == SDLK_p) {
                    write_ppm(gameboy.framebuffer(), palette, "framebuffer.ppm");
                }
                if (event.key.keysym.sym == SDLK_t) {
                    tile_viewer.toggle();
                }
            }
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE) {
                if (tile_viewer.visible() && event.window.windowID == tile_viewer.window_id()) {
                    tile_viewer.close();
                } else {
                    running = false;
                }
            }
        }

        gameboy.run_frame();
        const std::span<const uint8_t> fb = gameboy.framebuffer();
        for (size_t i = 0; i < pixels.size(); ++i) {
            pixels[i] = map_shade(palette, fb[i]);
        }

        SDL_UpdateTexture(texture, nullptr, pixels.data(), kWidth * 4);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
        tile_viewer.render(gameboy.debug_vram(), palette);

        // battery save every ~30s of frames
        ++frame_count;
        if (opt.rom_path != nullptr && frame_count % 1800 == 0) {
            save_battery_ram(gameboy, opt.rom_path);
        }
    }

    if (opt.rom_path != nullptr) {
        save_battery_ram(gameboy, opt.rom_path);
    }
    tile_viewer.close();
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
