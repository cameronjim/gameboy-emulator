#include "apu.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

// sequencer fires step k at cycle 8192*(k+1): lengths on 0/2/4/6, sweep 2/6, envelope 7
constexpr uint32_t kLengthFirst = 8192;
constexpr uint32_t kSweepFirst = 8192 * 3;
constexpr uint32_t kEnvelopeFirst = 8192 * 8;

gb::Apu powered_apu() {
    gb::Apu apu;
    apu.write_register(gb::kRegNr52, 0x80);
    apu.write_register(gb::kRegNr50, 0x77);
    apu.write_register(gb::kRegNr51, 0xFF);
    return apu;
}

void trigger_ch2(gb::Apu& apu, uint8_t duty, uint8_t nr22, uint8_t length_bits, bool length_enable) {
    apu.write_register(gb::kRegNr21, static_cast<uint8_t>((duty << 6) | length_bits));
    apu.write_register(gb::kRegNr22, nr22);
    apu.write_register(gb::kRegNr23, 0xFF);
    apu.write_register(gb::kRegNr24, static_cast<uint8_t>(0x87 | (length_enable ? 0x40 : 0x00)));
}

uint8_t ch2_status(gb::Apu& apu) {
    return static_cast<uint8_t>(apu.read_register(gb::kRegNr52) & 0x02);
}

} // namespace

TEST_CASE("frame_sequencer_rates_256_128_64") {
    // length at 256hz
    gb::Apu apu = powered_apu();
    trigger_ch2(apu, 2, 0xF0, 62, true);
    apu.tick(kLengthFirst);
    REQUIRE(ch2_status(apu) != 0);
    apu.tick(8192 * 2);
    REQUIRE(ch2_status(apu) == 0);

    // sweep at 128hz
    gb::Apu apu2 = powered_apu();
    apu2.write_register(gb::kRegNr10, 0x11);
    apu2.write_register(gb::kRegNr12, 0xF0);
    apu2.write_register(gb::kRegNr13, 0x00);
    apu2.write_register(gb::kRegNr14, 0x82);
    REQUIRE(apu2.debug_ch1_freq() == 0x200);
    apu2.tick(kSweepFirst - 1);
    REQUIRE(apu2.debug_ch1_freq() == 0x200);
    apu2.tick(1);
    REQUIRE(apu2.debug_ch1_freq() == 0x300);

    // envelope at 64hz
    gb::Apu apu3 = powered_apu();
    trigger_ch2(apu3, 2, 0xA1, 0, false);
    apu3.tick(kEnvelopeFirst - 1);
    REQUIRE(apu3.debug_ch2_volume() == 10);
    apu3.tick(1);
    REQUIRE(apu3.debug_ch2_volume() == 9);
}

TEST_CASE("square_duty_patterns_exact") {
    constexpr std::array<std::array<uint8_t, 8>, 4> expected = {{
        {0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 0, 1, 1, 1},
        {0, 1, 1, 1, 1, 1, 1, 0},
    }};
    for (uint8_t duty = 0; duty < 4; ++duty) {
        gb::Apu apu = powered_apu();
        // freq 2047: duty position advances every 4 cycles
        trigger_ch2(apu, duty, 0xF0, 0, false);
        for (uint8_t step = 0; step < 8; ++step) {
            REQUIRE(apu.debug_ch2_output() == expected[duty][step] * 15);
            apu.tick(4);
        }
    }
}

TEST_CASE("envelope_steps_at_64hz_and_clamps") {
    gb::Apu apu = powered_apu();
    // volume 14, increasing, period 1
    trigger_ch2(apu, 2, 0xE9, 0, false);
    apu.tick(kEnvelopeFirst);
    REQUIRE(apu.debug_ch2_volume() == 15);
    apu.tick(8192 * 8);
    REQUIRE(apu.debug_ch2_volume() == 15);
}

TEST_CASE("sweep_updates_and_overflow_disables_ch1") {
    gb::Apu apu = powered_apu();
    // period 1, add, shift 1, freq 1024: first sweep lands on 1536, the next calc overflows
    apu.write_register(gb::kRegNr10, 0x11);
    apu.write_register(gb::kRegNr12, 0xF0);
    apu.write_register(gb::kRegNr13, 0x00);
    apu.write_register(gb::kRegNr14, 0x84);
    REQUIRE((apu.read_register(gb::kRegNr52) & 0x01) != 0);
    apu.tick(kSweepFirst);
    REQUIRE(apu.debug_ch1_freq() == 1536);
    REQUIRE((apu.read_register(gb::kRegNr52) & 0x01) == 0);

    // immediate overflow check on trigger when shift is nonzero
    gb::Apu apu2 = powered_apu();
    apu2.write_register(gb::kRegNr10, 0x01);
    apu2.write_register(gb::kRegNr12, 0xF0);
    apu2.write_register(gb::kRegNr13, 0xD0);
    apu2.write_register(gb::kRegNr14, 0x87);
    REQUIRE((apu2.read_register(gb::kRegNr52) & 0x01) == 0);
}

TEST_CASE("length_counter_disables_channel") {
    gb::Apu apu = powered_apu();
    trigger_ch2(apu, 2, 0xF0, 63, true);
    REQUIRE(ch2_status(apu) != 0);
    apu.tick(kLengthFirst);
    REQUIRE(ch2_status(apu) == 0);
}

TEST_CASE("trigger_reloads_length_and_envelope") {
    gb::Apu apu = powered_apu();
    trigger_ch2(apu, 2, 0xF1, 63, true);
    apu.tick(kLengthFirst);
    REQUIRE(ch2_status(apu) == 0);
    // envelope decayed before retrigger
    apu.tick(kEnvelopeFirst);
    // retrigger with expired length reloads 64 and resets the envelope
    apu.write_register(gb::kRegNr24, 0xC7);
    REQUIRE(ch2_status(apu) != 0);
    REQUIRE(apu.debug_ch2_volume() == 15);
    apu.tick(kLengthFirst);
    REQUIRE(ch2_status(apu) != 0);
}

TEST_CASE("ch4_lfsr_sequence_15bit_and_7bit") {
    REQUIRE(gb::Apu::lfsr_step(0x7FFF, false) == 0x3FFF);
    REQUIRE(gb::Apu::lfsr_step(0x3FFF, false) == 0x1FFF);
    REQUIRE(gb::Apu::lfsr_step(0x0001, false) == 0x4000);
    REQUIRE(gb::Apu::lfsr_step(0x0002, false) == 0x4001);
    REQUIRE(gb::Apu::lfsr_step(0x0001, true) == 0x4040);
}

TEST_CASE("ch3_reads_wave_ram_nibbles_in_order") {
    gb::Apu apu = powered_apu();
    apu.write_register(gb::kWaveRamStart, 0xAB);
    apu.write_register(gb::kRegNr30, 0x80);
    apu.write_register(gb::kRegNr32, 0x20);
    apu.write_register(gb::kRegNr33, 0xFF);
    apu.write_register(gb::kRegNr34, 0x87);
    // high nibble first
    REQUIRE(apu.debug_ch3_output() == 0x0A);
    apu.tick(2);
    REQUIRE(apu.debug_ch3_output() == 0x0B);
    // 50% volume shifts right once
    apu.write_register(gb::kRegNr32, 0x40);
    REQUIRE(apu.debug_ch3_output() == 0x05);
}

TEST_CASE("nr52_power_off_clears_registers") {
    gb::Apu apu = powered_apu();
    apu.write_register(gb::kRegNr11, 0xBF);
    trigger_ch2(apu, 2, 0xF0, 0, false);
    apu.write_register(gb::kRegNr52, 0x00);
    REQUIRE(apu.read_register(gb::kRegNr52) == 0x70);
    // writes ignored while off
    apu.write_register(gb::kRegNr21, 0x80);
    apu.write_register(gb::kRegNr52, 0x80);
    REQUIRE(apu.read_register(gb::kRegNr11) == 0x3F);
    REQUIRE(apu.read_register(gb::kRegNr21) == 0x3F);
    REQUIRE(apu.read_register(gb::kRegNr50) == 0x00);
}

TEST_CASE("mixer_panning_nr51") {
    gb::Apu apu = powered_apu();
    // ch2 routed left only
    apu.write_register(gb::kRegNr51, 0x20);
    trigger_ch2(apu, 3, 0xF0, 0, false);
    std::array<int16_t, 256> buf{};
    apu.tick(87 * 100);
    const size_t n = apu.read_audio(buf);
    REQUIRE(n > 0);
    bool left_nonzero = false;
    bool right_nonzero = false;
    for (size_t i = 0; i + 1 < n; i += 2) {
        left_nonzero = left_nonzero || buf[i] != 0;
        right_nonzero = right_nonzero || buf[i + 1] != 0;
    }
    REQUIRE(left_nonzero);
    REQUIRE(!right_nonzero);
}

TEST_CASE("golden_register_script_hash") {
    gb::Apu apu = powered_apu();
    // 440hz square plus noise, one emulated second
    apu.write_register(gb::kRegNr21, 0x80);
    apu.write_register(gb::kRegNr22, 0xF3);
    apu.write_register(gb::kRegNr23, 0xD6);
    apu.write_register(gb::kRegNr24, 0x86);
    apu.write_register(gb::kRegNr42, 0xF2);
    apu.write_register(gb::kRegNr43, 0x24);
    apu.write_register(gb::kRegNr44, 0x80);
    uint64_t hash = 1469598103934665603ull;
    std::array<int16_t, 512> buf{};
    for (uint32_t i = 0; i < 512; ++i) {
        apu.tick(8192);
        size_t n;
        while ((n = apu.read_audio(buf)) > 0) {
            for (size_t j = 0; j < n; ++j) {
                const uint16_t s = static_cast<uint16_t>(buf[j]);
                hash = (hash ^ (s & 0xFF)) * 1099511628211ull;
                hash = (hash ^ (s >> 8)) * 1099511628211ull;
            }
        }
    }
    // regression detector: recorded from the implementation the golden run was verified on
    REQUIRE(hash == 18364134320615243907ull);
}
