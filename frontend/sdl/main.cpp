#include "cartridge.hpp"
#include "cpu.hpp"
#include "gameboy.hpp"
#include "trace.hpp"

#include <SDL.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
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
constexpr std::array<uint32_t, 4> kPalette = {0xFFE0F8D0u, 0xFF88C070u, 0xFF346856u, 0xFF081820u};
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
    }
    return "unknown";
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
    uint64_t trace_from = 0;
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
        std::fprintf(stderr, "usage: gbemu-sdl [--doctor <path>] [--trace-from <n>] [rom]\n");
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
        gameboy.load_rom(bytes);
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

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            }
        }

        gameboy.run_frame();
        const std::span<const uint8_t> fb = gameboy.framebuffer();
        for (size_t i = 0; i < pixels.size(); ++i) {
            pixels[i] = kPalette[fb[i] & 0x3u];
        }

        SDL_UpdateTexture(texture, nullptr, pixels.data(), kWidth * 4);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
