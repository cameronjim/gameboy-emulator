#pragma once

#include "bus.hpp"
#include "cartridge.hpp"
#include "cpu.hpp"
#include "serial.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace gb {

inline constexpr uint32_t kLcdWidth = 160;
inline constexpr uint32_t kLcdHeight = 144;

enum class Button : uint8_t { Right, Left, Up, Down, A, B, Select, Start };

class Gameboy {
public:
    bool load_rom(std::span<const uint8_t> bytes);
    void run_frame();
    std::span<const uint8_t> framebuffer() const;
    void set_button(Button b, bool pressed);
    void set_serial_sink(Serial::Sink sink);
    uint64_t cycles() const {
        return cycles_;
    }

private:
    void render_test_pattern();

    Serial serial_;
    Bus bus_{serial_};
    Cpu cpu_{bus_};
    std::optional<Cartridge> cart_;
    uint64_t cycles_ = 0;
    std::array<uint8_t, kLcdWidth * kLcdHeight> framebuffer_{};
};

} // namespace gb
