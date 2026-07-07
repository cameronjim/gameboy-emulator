#pragma once

#include "mapper.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace gb {

class MapperMbc3 final : public Mapper {
public:
    MapperMbc3(std::vector<uint8_t> rom, uint32_t ram_size);
    uint8_t read_rom(uint16_t addr) const override;
    void write_rom(uint16_t addr, uint8_t value) override;
    uint8_t read_ram(uint16_t addr) const override;
    void write_ram(uint16_t addr, uint8_t value) override;
    std::span<uint8_t> external_ram() override {
        return ram_;
    }
    void set_rtc_seconds(uint64_t seconds) override;

private:
    uint32_t rom_bank_count() const {
        return static_cast<uint32_t>(rom_.size() / 0x4000);
    }

    std::vector<uint8_t> rom_;
    std::vector<uint8_t> ram_;
    bool ram_enable_ = false;
    uint8_t rom_bank_ = 1;
    // 0-3 selects a ram bank, 0x08-0x0c an rtc register
    uint8_t select_ = 0;
    uint8_t last_latch_ = 0xFF;
    // s, m, h, day low, day high
    std::array<uint8_t, 5> rtc_{};
    std::array<uint8_t, 5> rtc_latched_{};
};

} // namespace gb
