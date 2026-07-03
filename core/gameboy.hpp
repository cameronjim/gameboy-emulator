#pragma once

#include "bus.hpp"
#include "cartridge.hpp"
#include "cpu.hpp"
#include "interrupts.hpp"
#include "ppu.hpp"
#include "serial.hpp"
#include "timer.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace gb {

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
    // debug accessor for the frontend tile viewer
    std::span<const uint8_t> debug_vram() const {
        return ppu_.vram();
    }

private:
    void render_test_pattern();

    Serial serial_;
    InterruptLine irq_;
    Timer timer_{irq_};
    Ppu ppu_{irq_};
    Bus bus_{serial_, timer_, ppu_, irq_};
    Cpu cpu_{bus_};
    std::optional<Cartridge> cart_;
    uint64_t cycles_ = 0;
    std::array<uint8_t, kLcdWidth * kLcdHeight> pattern_{};
};

} // namespace gb
