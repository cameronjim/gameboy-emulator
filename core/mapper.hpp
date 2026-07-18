#pragma once

#include "state.hpp"

#include <cstddef>
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
    // save state section; rom-only carts have none
    virtual size_t state_size() const {
        return 0;
    }
    virtual void save_state(StateWriter& w) const {
        static_cast<void>(w);
    }
    virtual void load_state(StateReader& r) {
        static_cast<void>(r);
    }
};

} // namespace gb
