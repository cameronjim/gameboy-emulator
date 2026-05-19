#include "cpu.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

using gb::Cpu;
using gb::kFlagC;
using gb::kFlagH;
using gb::kFlagN;
using gb::kFlagZ;

TEST_CASE("add_sets_zero_half_and_carry") {
    REQUIRE(Cpu::alu_add(0x0F, 0x01).flags == kFlagH);
    REQUIRE(Cpu::alu_add(0x0F, 0x01).value == 0x10);
    REQUIRE(Cpu::alu_add(0xFF, 0x01).flags == (kFlagZ | kFlagH | kFlagC));
    REQUIRE(Cpu::alu_add(0x80, 0x80).flags == (kFlagZ | kFlagC));
    REQUIRE(Cpu::alu_add(0x00, 0x00).flags == kFlagZ);
    REQUIRE(Cpu::alu_add(0x12, 0x34).value == 0x46);
    REQUIRE(Cpu::alu_add(0x12, 0x34).flags == 0);
}

TEST_CASE("adc_half_carry_includes_carry_in") {
    REQUIRE(Cpu::alu_adc(0x0E, 0x01, true).value == 0x10);
    REQUIRE(Cpu::alu_adc(0x0E, 0x01, true).flags == kFlagH);
    REQUIRE(Cpu::alu_adc(0xFF, 0x00, true).flags == (kFlagZ | kFlagH | kFlagC));
    REQUIRE(Cpu::alu_adc(0x0E, 0x01, false).flags == 0);
}

TEST_CASE("sub_sets_borrow_flags") {
    REQUIRE(Cpu::alu_sub(0x10, 0x01).value == 0x0F);
    REQUIRE(Cpu::alu_sub(0x10, 0x01).flags == (kFlagN | kFlagH));
    REQUIRE(Cpu::alu_sub(0x00, 0x01).value == 0xFF);
    REQUIRE(Cpu::alu_sub(0x00, 0x01).flags == (kFlagN | kFlagH | kFlagC));
    REQUIRE(Cpu::alu_sub(0x42, 0x42).flags == (kFlagZ | kFlagN));
}

TEST_CASE("sbc_half_carry_includes_borrow_in") {
    REQUIRE(Cpu::alu_sbc(0x10, 0x0F, true).value == 0x00);
    REQUIRE(Cpu::alu_sbc(0x10, 0x0F, true).flags == (kFlagZ | kFlagN | kFlagH));
    REQUIRE(Cpu::alu_sbc(0x00, 0xFF, true).value == 0x00);
    REQUIRE(Cpu::alu_sbc(0x00, 0xFF, true).flags == (kFlagZ | kFlagN | kFlagH | kFlagC));
    REQUIRE(Cpu::alu_sbc(0x10, 0x0F, false).value == 0x01);
    REQUIRE(Cpu::alu_sbc(0x10, 0x0F, false).flags == (kFlagN | kFlagH));
}

TEST_CASE("and_or_xor_fixed_flags") {
    REQUIRE(Cpu::alu_and(0xF0, 0x0F).flags == (kFlagZ | kFlagH));
    REQUIRE(Cpu::alu_and(0xFF, 0x0F).flags == kFlagH);
    REQUIRE(Cpu::alu_or(0x00, 0x00).flags == kFlagZ);
    REQUIRE(Cpu::alu_or(0xF0, 0x0F).value == 0xFF);
    REQUIRE(Cpu::alu_or(0xF0, 0x0F).flags == 0);
    REQUIRE(Cpu::alu_xor(0xAA, 0xAA).flags == kFlagZ);
    REQUIRE(Cpu::alu_xor(0xF0, 0x0F).value == 0xFF);
}

TEST_CASE("inc_dec_preserve_carry") {
    REQUIRE(Cpu::alu_inc(0x0F, kFlagC).value == 0x10);
    REQUIRE(Cpu::alu_inc(0x0F, kFlagC).flags == (kFlagH | kFlagC));
    REQUIRE(Cpu::alu_inc(0xFF, 0).flags == (kFlagZ | kFlagH));
    REQUIRE(Cpu::alu_inc(0x41, 0).flags == 0);
    REQUIRE(Cpu::alu_dec(0x10, kFlagC).value == 0x0F);
    REQUIRE(Cpu::alu_dec(0x10, kFlagC).flags == (kFlagN | kFlagH | kFlagC));
    REQUIRE(Cpu::alu_dec(0x01, 0).flags == (kFlagZ | kFlagN));
    REQUIRE(Cpu::alu_dec(0x00, 0).value == 0xFF);
    REQUIRE(Cpu::alu_dec(0x00, 0).flags == (kFlagN | kFlagH));
}

TEST_CASE("daa_table_post_add") {
    struct Row {
        uint8_t a;
        uint8_t flags_in;
        uint8_t expect_a;
        bool expect_c;
    };
    const Row rows[] = {
        {0x3C, 0, 0x42, false}, {0x11, kFlagH, 0x17, false}, {0x20, kFlagC, 0x80, true},
        {0x9A, 0, 0x00, true},  {0x00, 0, 0x00, false},      {0x99, 0, 0x99, false},
        {0xA0, 0, 0x00, true},
    };
    for (const Row& row : rows) {
        const gb::AluResult r = Cpu::alu_daa(row.a, row.flags_in);
        REQUIRE(r.value == row.expect_a);
        REQUIRE(((r.flags & kFlagC) != 0) == row.expect_c);
        REQUIRE(((r.flags & kFlagZ) != 0) == (row.expect_a == 0));
        REQUIRE((r.flags & kFlagH) == 0);
        REQUIRE((r.flags & kFlagN) == 0);
    }
}

TEST_CASE("daa_table_post_sub") {
    struct Row {
        uint8_t a;
        uint8_t flags_in;
        uint8_t expect_a;
        bool expect_c;
    };
    const Row rows[] = {
        {0x2D, kFlagN | kFlagH, 0x27, false},
        {0x70, kFlagN | kFlagC, 0x10, true},
        {0x00, kFlagN, 0x00, false},
        {0xFA, kFlagN | kFlagH | kFlagC, 0x94, true},
    };
    for (const Row& row : rows) {
        const gb::AluResult r = Cpu::alu_daa(row.a, row.flags_in);
        REQUIRE(r.value == row.expect_a);
        REQUIRE(((r.flags & kFlagC) != 0) == row.expect_c);
        REQUIRE((r.flags & kFlagN) == kFlagN);
        REQUIRE((r.flags & kFlagH) == 0);
    }
}

TEST_CASE("rotates_set_carry_from_edge_bit") {
    REQUIRE(Cpu::alu_rlc(0x80).value == 0x01);
    REQUIRE(Cpu::alu_rlc(0x80).flags == kFlagC);
    REQUIRE(Cpu::alu_rlc(0x00).flags == kFlagZ);
    REQUIRE(Cpu::alu_rrc(0x01).value == 0x80);
    REQUIRE(Cpu::alu_rrc(0x01).flags == kFlagC);
    REQUIRE(Cpu::alu_rl(0x80, false).value == 0x00);
    REQUIRE(Cpu::alu_rl(0x80, false).flags == (kFlagZ | kFlagC));
    REQUIRE(Cpu::alu_rl(0x00, true).value == 0x01);
    REQUIRE(Cpu::alu_rr(0x01, false).flags == (kFlagZ | kFlagC));
    REQUIRE(Cpu::alu_rr(0x00, true).value == 0x80);
}

TEST_CASE("shifts_and_swap") {
    REQUIRE(Cpu::alu_sla(0x80).flags == (kFlagZ | kFlagC));
    REQUIRE(Cpu::alu_sla(0x41).value == 0x82);
    REQUIRE(Cpu::alu_sra(0x81).value == 0xC0);
    REQUIRE(Cpu::alu_sra(0x81).flags == kFlagC);
    REQUIRE(Cpu::alu_srl(0x81).value == 0x40);
    REQUIRE(Cpu::alu_srl(0x81).flags == kFlagC);
    REQUIRE(Cpu::alu_swap(0xF0).value == 0x0F);
    REQUIRE(Cpu::alu_swap(0xF0).flags == 0);
    REQUIRE(Cpu::alu_swap(0x00).flags == kFlagZ);
}

TEST_CASE("bit_preserves_carry_sets_half") {
    REQUIRE(Cpu::alu_bit(0x80, 7, 0) == kFlagH);
    REQUIRE(Cpu::alu_bit(0x00, 3, 0) == (kFlagZ | kFlagH));
    REQUIRE(Cpu::alu_bit(0x00, 3, kFlagC) == (kFlagZ | kFlagH | kFlagC));
}

TEST_CASE("add16_preserves_z_carries_from_bit11_and_15") {
    REQUIRE(Cpu::alu_add16(0x0FFF, 0x0001, kFlagZ).value == 0x1000);
    REQUIRE(Cpu::alu_add16(0x0FFF, 0x0001, kFlagZ).flags == (kFlagZ | kFlagH));
    REQUIRE(Cpu::alu_add16(0xFFFF, 0x0001, 0).flags == (kFlagH | kFlagC));
    REQUIRE(Cpu::alu_add16(0x1000, 0x1000, 0).flags == 0);
}

TEST_CASE("add_sp_e8_flags_from_low_byte") {
    REQUIRE(Cpu::alu_add_sp_e8(0xFFF8, 8).value == 0x0000);
    REQUIRE(Cpu::alu_add_sp_e8(0xFFF8, 8).flags == (kFlagH | kFlagC));
    REQUIRE(Cpu::alu_add_sp_e8(0x0001, -1).value == 0x0000);
    REQUIRE(Cpu::alu_add_sp_e8(0x0001, -1).flags == (kFlagH | kFlagC));
    REQUIRE(Cpu::alu_add_sp_e8(0x0000, -1).value == 0xFFFF);
    REQUIRE(Cpu::alu_add_sp_e8(0x0000, -1).flags == 0);
    REQUIRE(Cpu::alu_add_sp_e8(0x000F, 1).flags == kFlagH);
}
