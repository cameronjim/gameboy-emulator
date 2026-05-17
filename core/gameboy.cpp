#include "gameboy.hpp"

namespace gb {

namespace {
constexpr uint32_t kBandWidth = kLcdWidth / 4;
} // namespace

bool Gameboy::load_rom(std::span<const uint8_t> bytes) {
    rom_.assign(bytes.begin(), bytes.end());
    return true;
}

void Gameboy::run_frame() {
    // fixed four-band test pattern until the ppu exists
    for (uint32_t y = 0; y < kLcdHeight; ++y) {
        for (uint32_t x = 0; x < kLcdWidth; ++x) {
            framebuffer_[y * kLcdWidth + x] = static_cast<uint8_t>(x / kBandWidth);
        }
    }
}

std::span<const uint8_t> Gameboy::framebuffer() const {
    return framebuffer_;
}

void Gameboy::set_button(Button, bool) {}

} // namespace gb
