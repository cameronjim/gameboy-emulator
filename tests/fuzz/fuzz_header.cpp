#include "cartridge.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    const std::span<const uint8_t> bytes(data, size);
    gb::Cartridge::parse(bytes);
    return 0;
}
