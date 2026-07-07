#pragma once

#include <cstdint>
#include <span>

namespace gb {

class Mapper {
public:
    virtual ~Mapper() = default;
    virtual uint8_t read_rom(uint16_t addr) const = 0;
    virtual void write_rom(uint16_t addr, uint8_t value) = 0;
    virtual uint8_t read_ram(uint16_t addr) const = 0;
    virtual void write_ram(uint16_t addr, uint8_t value) = 0;
    // battery save path; empty when the cart has no external ram
    virtual std::span<uint8_t> external_ram() {
        return {};
    }
    // rtc seed injected by the frontend so the core stays clock-free
    virtual void set_rtc_seconds(uint64_t seconds) {
        static_cast<void>(seconds);
    }
};

} // namespace gb
