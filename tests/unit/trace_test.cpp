#include "trace.hpp"

#include "fake_bus.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>

TEST_CASE("trace_line_matches_doctor_format_exactly") {
    FakeBus bus;
    bus.mem[0x0100] = 0x00;
    bus.mem[0x0101] = 0xC3;
    bus.mem[0x0102] = 0x13;
    bus.mem[0x0103] = 0x02;
    const gb::CpuRegs regs;
    gb::Trace trace;
    std::string out;
    trace.log(regs, bus, out);
    REQUIRE(out == "A:01 F:B0 B:00 C:13 D:00 E:D8 H:01 L:4D SP:FFFE PC:0100 PCMEM:00,C3,13,02\n");
}

TEST_CASE("doctor_mode_pins_ly_to_0x90") {
    FakeBus bus;
    bus.mem[gb::kRegLy] = 0x00;
    bus.mem[0x1234] = 0x42;
    gb::DoctorMemory mem(bus);
    REQUIRE(mem.read8(gb::kRegLy) == 0x90);
    REQUIRE(mem.read8(0x1234) == 0x42);
    mem.write8(0x2000, 0x55);
    REQUIRE(bus.mem[0x2000] == 0x55);
}

TEST_CASE("trace_from_skips_n_instructions") {
    FakeBus bus;
    const gb::CpuRegs regs;
    gb::Trace trace(2);
    std::string out;
    trace.log(regs, bus, out);
    trace.log(regs, bus, out);
    REQUIRE(out.empty());
    trace.log(regs, bus, out);
    REQUIRE(trace.count() == 3);
    REQUIRE(std::count(out.begin(), out.end(), '\n') == 1);
}
