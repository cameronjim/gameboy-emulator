#include "timer.hpp"

#include "interrupts.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

namespace {

struct Rig {
    gb::InterruptLine irq;
    gb::Timer timer{irq};
};

} // namespace

TEST_CASE("div_increments_at_16384hz") {
    Rig rig;
    rig.timer.write_div();
    rig.timer.tick(255);
    REQUIRE(rig.timer.read_div() == 0);
    rig.timer.tick(1);
    REQUIRE(rig.timer.read_div() == 1);
    rig.timer.tick(256 * 3);
    REQUIRE(rig.timer.read_div() == 4);
}

TEST_CASE("div_write_resets_internal_counter") {
    Rig rig;
    rig.timer.tick(1000);
    rig.timer.write_div();
    REQUIRE(rig.timer.read_div() == 0);
    // whole counter cleared, not just the visible byte
    rig.timer.tick(255);
    REQUIRE(rig.timer.read_div() == 0);
    rig.timer.tick(1);
    REQUIRE(rig.timer.read_div() == 1);
}

TEST_CASE("div_write_can_tick_tima_via_falling_edge") {
    Rig rig;
    rig.timer.write_div();
    rig.timer.write_tac(0x07);
    rig.timer.tick(0x80);
    REQUIRE(rig.timer.read_tima() == 0);
    // counter bit 7 is high, so the reset is a falling edge
    rig.timer.write_div();
    REQUIRE(rig.timer.read_tima() == 1);
}

TEST_CASE("tima_rates_match_tac_select") {
    struct Row {
        uint8_t tac;
        uint32_t period;
    };
    const Row rows[] = {{0x04, 1024}, {0x05, 16}, {0x06, 64}, {0x07, 256}};
    for (const Row& row : rows) {
        Rig rig;
        rig.timer.write_div();
        rig.timer.write_tac(row.tac);
        rig.timer.tick(row.period);
        REQUIRE(rig.timer.read_tima() == 1);
        rig.timer.tick(row.period * 3);
        REQUIRE(rig.timer.read_tima() == 4);
    }
}

TEST_CASE("tima_overflow_reloads_tma_and_requests_interrupt") {
    Rig rig;
    rig.timer.write_div();
    rig.timer.write_tma(0xAB);
    rig.timer.write_tac(0x05);
    rig.timer.write_tima(0xFF);
    rig.timer.tick(16);
    REQUIRE(rig.timer.read_tima() == 0xAB);
    REQUIRE((rig.irq.read() & gb::kIntTimer) != 0);
}

TEST_CASE("disabled_tac_stops_tima_not_div") {
    Rig rig;
    rig.timer.write_div();
    rig.timer.write_tac(0x00);
    rig.timer.tick(4096);
    REQUIRE(rig.timer.read_tima() == 0);
    REQUIRE(rig.timer.read_div() == 16);
}

TEST_CASE("tac_unused_bits_read_ones") {
    Rig rig;
    rig.timer.write_tac(0x05);
    REQUIRE(rig.timer.read_tac() == 0xFD);
}
