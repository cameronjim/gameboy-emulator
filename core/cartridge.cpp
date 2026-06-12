#include "cartridge.hpp"

#include "mapper_rom.hpp"

#include <cstddef>
#include <utility>
#include <vector>

namespace gb {

namespace {
constexpr uint16_t kHeaderTitleStart = 0x0134;
constexpr uint16_t kHeaderTitleEnd = 0x0143;
constexpr uint16_t kHeaderType = 0x0147;
constexpr uint16_t kHeaderRomSize = 0x0148;
constexpr uint16_t kHeaderRamSize = 0x0149;
constexpr uint16_t kHeaderChecksumLast = 0x014C;
constexpr uint16_t kHeaderChecksum = 0x014D;
constexpr size_t kMinRomSize = 0x0150;
} // namespace

std::optional<Cartridge> Cartridge::parse(std::span<const uint8_t> bytes, std::string* reject_reason) {
    const auto reject = [reject_reason](const char* why) {
        if (reject_reason != nullptr) {
            *reject_reason = why;
        }
        return std::nullopt;
    };

    if (bytes.size() < kMinRomSize) {
        return reject("file smaller than header");
    }

    uint8_t sum = 0;
    for (uint16_t addr = kHeaderTitleStart; addr <= kHeaderChecksumLast; ++addr) {
        // pandocs: checksum folds each byte as x = x - byte - 1
        sum = static_cast<uint8_t>(sum - bytes[addr] - 1);
    }
    if (sum != bytes[kHeaderChecksum]) {
        return reject("header checksum mismatch");
    }

    const uint8_t rom_size_byte = bytes[kHeaderRomSize];
    if (rom_size_byte > 0x08) {
        return reject("unknown rom size");
    }
    // pandocs: rom size is 32 kib << value
    const uint32_t declared_rom = 32768u << rom_size_byte;
    if (static_cast<size_t>(declared_rom) != bytes.size()) {
        return reject("rom size mismatch");
    }

    const uint8_t type = bytes[kHeaderType];
    if (type != 0x00) {
        const bool mbc1 = type >= 0x01 && type <= 0x03;
        const bool mbc3 = type >= 0x0F && type <= 0x13;
        if (!mbc1 && !mbc3) {
            return reject("unknown cartridge type");
        }
        if (rom_size_byte != 0x00) {
            return reject("unsupported mapper");
        }
        // 32kb mbc-typed roms fit unbanked; real mbc support lands in milestone 11
    }

    uint32_t ram_size = 0;
    switch (bytes[kHeaderRamSize]) {
    case 0x00:
        ram_size = 0;
        break;
    case 0x02:
        ram_size = 8192;
        break;
    case 0x03:
        ram_size = 32768;
        break;
    case 0x04:
        ram_size = 131072;
        break;
    case 0x05:
        ram_size = 65536;
        break;
    default:
        return reject("unknown ram size");
    }

    std::string title;
    for (uint16_t addr = kHeaderTitleStart; addr <= kHeaderTitleEnd; ++addr) {
        if (bytes[addr] == 0) {
            break;
        }
        title.push_back(static_cast<char>(bytes[addr]));
    }

    Cartridge cart;
    cart.title_ = std::move(title);
    cart.type_ = CartType::RomOnly;
    cart.rom_size_ = declared_rom;
    cart.ram_size_ = ram_size;
    cart.mapper_ = std::make_unique<MapperRom>(std::vector<uint8_t>(bytes.begin(), bytes.end()));
    return cart;
}

} // namespace gb
