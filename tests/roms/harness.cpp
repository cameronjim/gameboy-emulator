#include "gameboy.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

// headless test rom runner; drives the gameboy facade exactly like a frontend
namespace {

std::vector<uint8_t> read_file(const char* path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return {};
    }
    const std::streamoff size = file.tellg();
    if (size <= 0 || size > 8 * 1024 * 1024) {
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

int run_serial(gb::Gameboy& gameboy, const std::string& expect, uint64_t budget, const char* rom_path) {
    std::string output;
    gameboy.set_serial_sink([&output](uint8_t b) { output.push_back(static_cast<char>(b)); });
    uint64_t last_cycles = ~0ull;
    while (gameboy.cycles() < budget && gameboy.cycles() != last_cycles) {
        last_cycles = gameboy.cycles();
        gameboy.run_frame();
        if (output.find(expect) != std::string::npos) {
            std::printf("PASS %s\n", rom_path);
            return 0;
        }
        if (output.find("Failed") != std::string::npos) {
            break;
        }
    }
    std::fprintf(stderr, "FAIL %s\nserial output:\n%s\n", rom_path, output.c_str());
    return 1;
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: gbrom_harness <rom> <channel> <expect> [budget_tcycles]\n");
        return 2;
    }
    const char* rom_path = argv[1];
    const std::string channel = argv[2];
    const std::string expect = argv[3];
    const uint64_t budget = argc > 4 ? std::strtoull(argv[4], nullptr, 10) : 2000000000ull;

    const std::vector<uint8_t> rom = read_file(rom_path);
    if (rom.empty()) {
        std::fprintf(stderr, "cannot read %s\n", rom_path);
        return 2;
    }
    gb::Gameboy gameboy;
    if (!gameboy.load_rom(rom)) {
        std::fprintf(stderr, "load_rom rejected %s\n", rom_path);
        return 2;
    }
    if (channel == "serial") {
        return run_serial(gameboy, expect, budget, rom_path);
    }
    // fibonacci and framebuffer channels arrive with milestones 06 and 10
    std::fprintf(stderr, "channel %s not implemented\n", channel.c_str());
    return 2;
}
