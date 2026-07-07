#pragma once

#include "mapper.hpp"

#include <cstdint>
#include <vector>

namespace gb {

class MapperMbc1 final : public Mapper {
public:
    MapperMbc1(std::vector<uint8_t> rom, uint32_t ram_size);
    uint8_t read_rom(uint16_t addr) const override;
    void write_rom(uint16_t addr, uint8_t value) override;
    uint8_t read_ram(uint16_t addr) const override;
    void write_ram(uint16_t addr, uint8_t value) override;
    std::span<uint8_t> external_ram() override {
        return ram_;
    }

private:
    uint32_t rom_bank_count() const {
        return static_cast<uint32_t>(rom_.size() / 0x4000);
    }

    std::vector<uint8_t> rom_;
    std::vector<uint8_t> ram_;
    bool ram_enable_ = false;
    uint8_t bank1_ = 1;
    uint8_t bank2_ = 0;
    uint8_t mode_ = 0;
};

} // namespace gb
