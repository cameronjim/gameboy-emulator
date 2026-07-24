#include "gameboy.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace {

std::vector<uint8_t> make_rom() {
    std::vector<uint8_t> rom(0x8000, 0);
    rom[0x0147] = 0x03;
    rom[0x0149] = 0x02;
    uint8_t sum = 0;
    for (uint16_t addr = 0x0134; addr <= 0x014C; ++addr) {
        sum = static_cast<uint8_t>(sum - rom[addr] - 1);
    }
    rom[0x014D] = sum;
    return rom;
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    static const std::vector<uint8_t> rom = make_rom();
    gb::Gameboy gameboy;
    if (!gameboy.load_rom(rom)) {
        return 0;
    }
    gameboy.load_state(std::span<const uint8_t>(data, size));
    // a load must never leave a machine that cannot run
    gameboy.run_frame();
    return 0;
}
