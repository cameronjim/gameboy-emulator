#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace gb {

class StateWriter {
public:
    explicit StateWriter(std::vector<uint8_t>& out) : out_(out) {}
    void u8(uint8_t v) {
        out_.push_back(v);
    }
    void b(bool v) {
        u8(v ? 1 : 0);
    }
    void u16(uint16_t v) {
        u8(static_cast<uint8_t>(v & 0xFF));
        u8(static_cast<uint8_t>(v >> 8));
    }
    void u32(uint32_t v) {
        u16(static_cast<uint16_t>(v & 0xFFFF));
        u16(static_cast<uint16_t>(v >> 16));
    }
    void bytes(std::span<const uint8_t> data) {
        out_.insert(out_.end(), data.begin(), data.end());
    }

private:
    std::vector<uint8_t>& out_;
};

// section sizes are validated before any apply; reads past the end return zero defensively
class StateReader {
public:
    explicit StateReader(std::span<const uint8_t> data) : data_(data) {}
    uint8_t u8() {
        return pos_ < data_.size() ? data_[pos_++] : 0;
    }
    bool b() {
        return u8() != 0;
    }
    uint16_t u16() {
        const uint8_t lo = u8();
        return static_cast<uint16_t>((u8() << 8) | lo);
    }
    uint32_t u32() {
        const uint16_t lo = u16();
        return (static_cast<uint32_t>(u16()) << 16) | lo;
    }
    void bytes(std::span<uint8_t> dst) {
        for (uint8_t& v : dst) {
            v = u8();
        }
    }

private:
    std::span<const uint8_t> data_;
    size_t pos_ = 0;
};

} // namespace gb
