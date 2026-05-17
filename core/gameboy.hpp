#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

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

private:
    std::vector<uint8_t> rom_;
    std::array<uint8_t, kLcdWidth * kLcdHeight> framebuffer_{};
};

} // namespace gb
