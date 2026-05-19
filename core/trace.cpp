#include "trace.hpp"

#include <cstdio>

namespace gb {

uint8_t DoctorMemory::read8(uint16_t addr) {
    if (addr == kRegLy) {
        return 0x90;
    }
    return inner_.read8(addr);
}

void DoctorMemory::write8(uint16_t addr, uint8_t value) {
    inner_.write8(addr, value);
}

void Trace::log(const CpuRegs& regs, Memory& bus, std::string& out) {
    const uint64_t index = count_++;
    if (index < skip_count_) {
        return;
    }
    char line[80];
    std::snprintf(line, sizeof(line),
                  "A:%02X F:%02X B:%02X C:%02X D:%02X E:%02X H:%02X L:%02X SP:%04X PC:%04X "
                  "PCMEM:%02X,%02X,%02X,%02X\n",
                  regs.a, regs.f, regs.b, regs.c, regs.d, regs.e, regs.h, regs.l, regs.sp, regs.pc,
                  bus.read8(regs.pc), bus.read8(static_cast<uint16_t>(regs.pc + 1)),
                  bus.read8(static_cast<uint16_t>(regs.pc + 2)),
                  bus.read8(static_cast<uint16_t>(regs.pc + 3)));
    out.append(line);
}

} // namespace gb
