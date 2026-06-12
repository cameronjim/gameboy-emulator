#pragma once

#include <cstdint>
#include <functional>
#include <utility>

namespace gb {

inline constexpr uint16_t kRegSb = 0xFF01;
inline constexpr uint16_t kRegSc = 0xFF02;

// stub: instant transfers, test rom output channel
class Serial {
public:
    using Sink = std::function<void(uint8_t)>;

    void set_sink(Sink sink) {
        sink_ = std::move(sink);
    }
    uint8_t read_sb() const {
        return sb_;
    }
    uint8_t read_sc() const {
        // dmg: sc bits 1-6 unused, read 1
        return static_cast<uint8_t>(sc_ | 0x7E);
    }
    void write_sb(uint8_t value) {
        sb_ = value;
    }
    void write_sc(uint8_t value);

private:
    Sink sink_;
    uint8_t sb_ = 0x00;
    uint8_t sc_ = 0x00;
};

} // namespace gb
