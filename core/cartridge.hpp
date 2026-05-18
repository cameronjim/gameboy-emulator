#pragma once

#include "mapper.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>

namespace gb {

enum class CartType : uint8_t { RomOnly };

class Cartridge {
public:
    static std::optional<Cartridge> parse(std::span<const uint8_t> bytes,
                                          std::string* reject_reason = nullptr);

    const std::string& title() const {
        return title_;
    }
    CartType type() const {
        return type_;
    }
    uint32_t rom_size() const {
        return rom_size_;
    }
    uint32_t ram_size() const {
        return ram_size_;
    }
    Mapper& mapper() {
        return *mapper_;
    }
    const Mapper& mapper() const {
        return *mapper_;
    }

private:
    Cartridge() = default;

    std::string title_;
    CartType type_ = CartType::RomOnly;
    uint32_t rom_size_ = 0;
    uint32_t ram_size_ = 0;
    std::unique_ptr<Mapper> mapper_;
};

} // namespace gb
