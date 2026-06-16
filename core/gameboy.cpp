#include "gameboy.hpp"

#include <utility>

namespace gb {

namespace {
constexpr uint32_t kBandWidth = kLcdWidth / 4;
// dmg frame is 70224 t-cycles
constexpr uint32_t kFrameCycles = 70224;
} // namespace

bool Gameboy::load_rom(std::span<const uint8_t> bytes) {
    std::optional<Cartridge> cart = Cartridge::parse(bytes);
    if (!cart.has_value()) {
        return false;
    }
    cart_ = std::move(cart);
    bus_.attach_mapper(cart_->mapper());
    return true;
}

void Gameboy::run_frame() {
    if (cart_.has_value()) {
        uint32_t elapsed = 0;
        while (elapsed < kFrameCycles) {
            const uint32_t t = cpu_.step();
            if (t == 0) {
                // cpu trapped an unknown opcode and stopped
                break;
            }
            timer_.tick(t);
            elapsed += t;
        }
        cycles_ += elapsed;
    }
    render_test_pattern();
}

std::span<const uint8_t> Gameboy::framebuffer() const {
    return framebuffer_;
}

void Gameboy::set_button(Button, bool) {}

void Gameboy::set_serial_sink(Serial::Sink sink) {
    serial_.set_sink(std::move(sink));
}

void Gameboy::render_test_pattern() {
    // fixed four-band test pattern until the ppu exists
    for (uint32_t y = 0; y < kLcdHeight; ++y) {
        for (uint32_t x = 0; x < kLcdWidth; ++x) {
            framebuffer_[y * kLcdWidth + x] = static_cast<uint8_t>(x / kBandWidth);
        }
    }
}

} // namespace gb
