#include "apu.hpp"

#include <algorithm>

namespace gb {

namespace {
// pandocs duty waveforms, bit index = duty position
constexpr std::array<std::array<uint8_t, 8>, 4> kDuty = {{
    {0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 1, 1, 1},
    {0, 1, 1, 1, 1, 1, 1, 0},
}};
// pandocs: register read or-masks
constexpr std::array<uint8_t, 0x17> kReadMask = {
    0x80, 0x3F, 0x00, 0xFF, 0xBF, 0xFF, 0x3F, 0x00, 0xFF, 0xBF, 0x7F, 0xFF,
    0x9F, 0xFF, 0xBF, 0xFF, 0xFF, 0x00, 0x00, 0xBF, 0x00, 0x00, 0x70,
};
// 4194304 / 48000, integer decimation
constexpr uint32_t kCyclesPerSample = 87;
constexpr uint32_t kSequencerPeriod = 8192;
// nr32 bits 6-5 to nibble shift: mute, 100%, 50%, 25%
constexpr std::array<uint8_t, 4> kWaveShift = {4, 0, 1, 2};

uint32_t noise_period(uint8_t nr43) {
    const uint8_t r = nr43 & 0x07;
    const uint32_t divisor = r == 0 ? 8 : static_cast<uint32_t>(r) * 16;
    return divisor << (nr43 >> 4);
}
} // namespace

uint16_t Apu::lfsr_step(uint16_t lfsr, bool width7) {
    const uint16_t xored = static_cast<uint16_t>((lfsr ^ (lfsr >> 1)) & 1);
    lfsr = static_cast<uint16_t>((lfsr >> 1) | (xored << 14));
    if (width7) {
        lfsr = static_cast<uint16_t>((lfsr & ~(1u << 6)) | (xored << 6));
    }
    return lfsr;
}

uint8_t Apu::debug_ch1_output() const {
    const bool dac = (reg(kRegNr12) & 0xF8) != 0;
    if (!ch1_.enabled || !dac) {
        return 0;
    }
    return kDuty[ch1_.duty][ch1_.pos] != 0 ? ch1_.volume : 0;
}

uint8_t Apu::debug_ch2_output() const {
    const bool dac = (reg(kRegNr22) & 0xF8) != 0;
    if (!ch2_.enabled || !dac) {
        return 0;
    }
    return kDuty[ch2_.duty][ch2_.pos] != 0 ? ch2_.volume : 0;
}

uint8_t Apu::debug_ch3_output() const {
    if (!ch3_.enabled || (reg(kRegNr30) & 0x80) == 0) {
        return 0;
    }
    const uint8_t byte = wave_ram_[ch3_.pos / 2];
    const uint8_t nibble = (ch3_.pos & 1) == 0 ? byte >> 4 : byte & 0x0F;
    return static_cast<uint8_t>(nibble >> kWaveShift[(reg(kRegNr32) >> 5) & 0x03]);
}

uint8_t Apu::debug_ch4_output() const {
    const bool dac = (reg(kRegNr42) & 0xF8) != 0;
    if (!ch4_.enabled || !dac) {
        return 0;
    }
    return (ch4_.lfsr & 1) == 0 ? ch4_.volume : 0;
}

void Apu::tick(uint32_t tcycles) {
    for (uint32_t i = 0; i < tcycles; ++i) {
        step_cycle();
    }
}

void Apu::step_cycle() {
    if (power_) {
        if (ch1_.enabled && ch1_.timer > 0 && --ch1_.timer == 0) {
            ch1_.timer = (2048u - ch1_.freq) * 4;
            ch1_.pos = static_cast<uint8_t>((ch1_.pos + 1) & 7);
        }
        if (ch2_.enabled && ch2_.timer > 0 && --ch2_.timer == 0) {
            ch2_.timer = (2048u - ch2_.freq) * 4;
            ch2_.pos = static_cast<uint8_t>((ch2_.pos + 1) & 7);
        }
        if (ch3_.enabled && ch3_.timer > 0 && --ch3_.timer == 0) {
            ch3_.timer = (2048u - ch3_.freq) * 2;
            ch3_.pos = static_cast<uint8_t>((ch3_.pos + 1) & 31);
        }
        if (ch4_.enabled && ch4_.timer > 0 && --ch4_.timer == 0) {
            ch4_.timer = noise_period(reg(kRegNr43));
            ch4_.lfsr = lfsr_step(ch4_.lfsr, (reg(kRegNr43) & 0x08) != 0);
        }
        if (++seq_counter_ >= kSequencerPeriod) {
            seq_counter_ = 0;
            sequencer_step();
        }
    }
    if (++sample_counter_ >= kCyclesPerSample) {
        sample_counter_ = 0;
        mix_sample();
    }
}

void Apu::sequencer_step() {
    // lengths 256hz, sweep 128hz, envelopes 64hz
    if ((seq_step_ & 1) == 0) {
        clock_lengths();
    }
    if (seq_step_ == 2 || seq_step_ == 6) {
        clock_sweep();
    }
    if (seq_step_ == 7) {
        clock_envelopes();
    }
    seq_step_ = static_cast<uint8_t>((seq_step_ + 1) & 7);
}

void Apu::clock_lengths() {
    const auto clock = [](auto& ch) {
        if (ch.length_enable && ch.length > 0 && --ch.length == 0) {
            ch.enabled = false;
        }
    };
    clock(ch1_);
    clock(ch2_);
    clock(ch3_);
    clock(ch4_);
}

void Apu::clock_envelopes() {
    const auto clock = [](auto& ch) {
        if (ch.env_period == 0) {
            return;
        }
        if (--ch.env_timer == 0) {
            ch.env_timer = ch.env_period;
            if (ch.env_add && ch.volume < 15) {
                ++ch.volume;
            } else if (!ch.env_add && ch.volume > 0) {
                --ch.volume;
            }
        }
    };
    clock(ch1_);
    clock(ch2_);
    clock(ch4_);
}

uint16_t Apu::sweep_calc() const {
    const uint8_t nr10 = reg(kRegNr10);
    const uint16_t delta = static_cast<uint16_t>(ch1_.sweep_shadow >> (nr10 & 0x07));
    if ((nr10 & 0x08) != 0) {
        return static_cast<uint16_t>(ch1_.sweep_shadow - delta);
    }
    return static_cast<uint16_t>(ch1_.sweep_shadow + delta);
}

void Apu::clock_sweep() {
    if (!ch1_.sweep_enabled) {
        return;
    }
    if (--ch1_.sweep_timer != 0) {
        return;
    }
    const uint8_t period = (reg(kRegNr10) >> 4) & 0x07;
    ch1_.sweep_timer = period != 0 ? period : 8;
    if (period == 0) {
        return;
    }
    const uint16_t next = sweep_calc();
    if (next > 2047) {
        ch1_.enabled = false;
        return;
    }
    if ((reg(kRegNr10) & 0x07) != 0) {
        ch1_.sweep_shadow = next;
        ch1_.freq = next;
        if (sweep_calc() > 2047) {
            ch1_.enabled = false;
        }
    }
}

void Apu::trigger_square(Square& ch, uint8_t nrx2) {
    ch.enabled = (nrx2 & 0xF8) != 0;
    if (ch.length == 0) {
        ch.length = 64;
    }
    ch.timer = (2048u - ch.freq) * 4;
    ch.volume = nrx2 >> 4;
    ch.env_add = (nrx2 & 0x08) != 0;
    ch.env_period = nrx2 & 0x07;
    ch.env_timer = ch.env_period != 0 ? ch.env_period : 8;
}

uint8_t Apu::read_register(uint16_t addr) const {
    if (addr >= kWaveRamStart && addr <= kWaveRamEnd) {
        return wave_ram_[addr - kWaveRamStart];
    }
    if (addr == kRegNr52) {
        uint8_t status = 0;
        status |= ch1_.enabled ? 0x01 : 0;
        status |= ch2_.enabled ? 0x02 : 0;
        status |= ch3_.enabled ? 0x04 : 0;
        status |= ch4_.enabled ? 0x08 : 0;
        return static_cast<uint8_t>(0x70 | (power_ ? 0x80 : 0x00) | status);
    }
    if (addr >= kRegNr10 && addr <= kRegNr52) {
        return static_cast<uint8_t>(reg(addr) | kReadMask[addr - kRegNr10]);
    }
    return 0xFF;
}

void Apu::write_register(uint16_t addr, uint8_t value) {
    if (addr >= kWaveRamStart && addr <= kWaveRamEnd) {
        wave_ram_[addr - kWaveRamStart] = value;
        return;
    }
    if (addr == kRegNr52) {
        const bool on = (value & 0x80) != 0;
        if (power_ && !on) {
            // power off clears every register
            regs_ = {};
            ch1_ = {};
            ch2_ = {};
            ch3_ = {};
            ch4_ = {};
            seq_step_ = 0;
        }
        power_ = on;
        return;
    }
    if (!power_ || addr < kRegNr10 || addr > kRegNr52) {
        return;
    }
    regs_[addr - kRegNr10] = value;
    switch (addr) {
    case kRegNr11:
        ch1_.duty = value >> 6;
        ch1_.length = static_cast<uint16_t>(64 - (value & 0x3F));
        break;
    case kRegNr12:
        if ((value & 0xF8) == 0) {
            ch1_.enabled = false;
        }
        break;
    case kRegNr13:
        ch1_.freq = static_cast<uint16_t>((ch1_.freq & 0x700) | value);
        break;
    case kRegNr14:
        ch1_.freq = static_cast<uint16_t>((ch1_.freq & 0xFF) | ((value & 0x07) << 8));
        ch1_.length_enable = (value & 0x40) != 0;
        if ((value & 0x80) != 0) {
            trigger_square(ch1_, reg(kRegNr12));
            // sweep shadow copy and immediate overflow check
            ch1_.sweep_shadow = ch1_.freq;
            const uint8_t period = (reg(kRegNr10) >> 4) & 0x07;
            const uint8_t shift = reg(kRegNr10) & 0x07;
            ch1_.sweep_timer = period != 0 ? period : 8;
            ch1_.sweep_enabled = period != 0 || shift != 0;
            if (shift != 0 && sweep_calc() > 2047) {
                ch1_.enabled = false;
            }
        }
        break;
    case kRegNr21:
        ch2_.duty = value >> 6;
        ch2_.length = static_cast<uint16_t>(64 - (value & 0x3F));
        break;
    case kRegNr22:
        if ((value & 0xF8) == 0) {
            ch2_.enabled = false;
        }
        break;
    case kRegNr23:
        ch2_.freq = static_cast<uint16_t>((ch2_.freq & 0x700) | value);
        break;
    case kRegNr24:
        ch2_.freq = static_cast<uint16_t>((ch2_.freq & 0xFF) | ((value & 0x07) << 8));
        ch2_.length_enable = (value & 0x40) != 0;
        if ((value & 0x80) != 0) {
            trigger_square(ch2_, reg(kRegNr22));
        }
        break;
    case kRegNr30:
        if ((value & 0x80) == 0) {
            ch3_.enabled = false;
        }
        break;
    case kRegNr31:
        ch3_.length = static_cast<uint16_t>(256 - value);
        break;
    case kRegNr33:
        ch3_.freq = static_cast<uint16_t>((ch3_.freq & 0x700) | value);
        break;
    case kRegNr34:
        ch3_.freq = static_cast<uint16_t>((ch3_.freq & 0xFF) | ((value & 0x07) << 8));
        ch3_.length_enable = (value & 0x40) != 0;
        if ((value & 0x80) != 0) {
            ch3_.enabled = (reg(kRegNr30) & 0x80) != 0;
            if (ch3_.length == 0) {
                ch3_.length = 256;
            }
            ch3_.timer = (2048u - ch3_.freq) * 2;
            ch3_.pos = 0;
        }
        break;
    case kRegNr41:
        ch4_.length = static_cast<uint16_t>(64 - (value & 0x3F));
        break;
    case kRegNr42:
        if ((value & 0xF8) == 0) {
            ch4_.enabled = false;
        }
        break;
    case kRegNr44:
        ch4_.length_enable = (value & 0x40) != 0;
        if ((value & 0x80) != 0) {
            const uint8_t nr42 = reg(kRegNr42);
            ch4_.enabled = (nr42 & 0xF8) != 0;
            if (ch4_.length == 0) {
                ch4_.length = 64;
            }
            ch4_.timer = noise_period(reg(kRegNr43));
            ch4_.lfsr = 0x7FFF;
            ch4_.volume = nr42 >> 4;
            ch4_.env_add = (nr42 & 0x08) != 0;
            ch4_.env_period = nr42 & 0x07;
            ch4_.env_timer = ch4_.env_period != 0 ? ch4_.env_period : 8;
        }
        break;
    default:
        break;
    }
}

void Apu::mix_sample() {
    if (!power_) {
        push_sample(0, 0);
        return;
    }
    // channel dac maps 0..15 to -15..+15; unrouted channels contribute nothing
    const std::array<int32_t, 4> dac = {
        (reg(kRegNr12) & 0xF8) != 0 && ch1_.enabled ? debug_ch1_output() * 2 - 15 : 0,
        (reg(kRegNr22) & 0xF8) != 0 && ch2_.enabled ? debug_ch2_output() * 2 - 15 : 0,
        (reg(kRegNr30) & 0x80) != 0 && ch3_.enabled ? debug_ch3_output() * 2 - 15 : 0,
        (reg(kRegNr42) & 0xF8) != 0 && ch4_.enabled ? debug_ch4_output() * 2 - 15 : 0,
    };
    const uint8_t nr51 = reg(kRegNr51);
    int32_t left = 0;
    int32_t right = 0;
    for (uint32_t ch = 0; ch < 4; ++ch) {
        if ((nr51 & (0x10u << ch)) != 0) {
            left += dac[ch];
        }
        if ((nr51 & (0x01u << ch)) != 0) {
            right += dac[ch];
        }
    }
    const uint8_t nr50 = reg(kRegNr50);
    const int32_t left_vol = ((nr50 >> 4) & 0x07) + 1;
    const int32_t right_vol = (nr50 & 0x07) + 1;
    push_sample(static_cast<int16_t>(left * left_vol * 32), static_cast<int16_t>(right * right_vol * 32));
}

void Apu::save_square(StateWriter& w, const Square& ch) const {
    w.b(ch.enabled);
    w.u16(ch.freq);
    w.u32(ch.timer);
    w.u8(ch.duty);
    w.u8(ch.pos);
    w.u16(ch.length);
    w.b(ch.length_enable);
    w.u8(ch.volume);
    w.b(ch.env_add);
    w.u8(ch.env_period);
    w.u8(ch.env_timer);
    w.u16(ch.sweep_shadow);
    w.u8(ch.sweep_timer);
    w.b(ch.sweep_enabled);
}

void Apu::load_square(StateReader& r, Square& ch) {
    ch.enabled = r.b();
    ch.freq = static_cast<uint16_t>(r.u16() & 0x7FF);
    ch.timer = r.u32();
    ch.duty = static_cast<uint8_t>(r.u8() & 0x03);
    ch.pos = static_cast<uint8_t>(r.u8() & 0x07);
    ch.length = r.u16();
    ch.length_enable = r.b();
    ch.volume = static_cast<uint8_t>(r.u8() & 0x0F);
    ch.env_add = r.b();
    ch.env_period = static_cast<uint8_t>(r.u8() & 0x07);
    ch.env_timer = r.u8();
    ch.sweep_shadow = static_cast<uint16_t>(r.u16() & 0x7FF);
    ch.sweep_timer = r.u8();
    ch.sweep_enabled = r.b();
}

void Apu::save_state(StateWriter& w) const {
    w.b(power_);
    w.bytes(regs_);
    w.bytes(wave_ram_);
    w.u32(seq_counter_);
    w.u8(seq_step_);
    w.u32(sample_counter_);
    for (int16_t s : ring_) {
        w.u16(static_cast<uint16_t>(s));
    }
    w.u32(static_cast<uint32_t>(ring_read_));
    w.u32(static_cast<uint32_t>(ring_count_));
    save_square(w, ch1_);
    save_square(w, ch2_);
    w.b(ch3_.enabled);
    w.u16(ch3_.freq);
    w.u32(ch3_.timer);
    w.u8(ch3_.pos);
    w.u16(ch3_.length);
    w.b(ch3_.length_enable);
    w.b(ch4_.enabled);
    w.u32(ch4_.timer);
    w.u16(ch4_.lfsr);
    w.u16(ch4_.length);
    w.b(ch4_.length_enable);
    w.u8(ch4_.volume);
    w.b(ch4_.env_add);
    w.u8(ch4_.env_period);
    w.u8(ch4_.env_timer);
}

void Apu::load_state(StateReader& r) {
    power_ = r.b();
    r.bytes(regs_);
    r.bytes(wave_ram_);
    seq_counter_ = r.u32() % 8192;
    seq_step_ = static_cast<uint8_t>(r.u8() & 0x07);
    sample_counter_ = r.u32() % 87;
    for (int16_t& s : ring_) {
        s = static_cast<int16_t>(r.u16());
    }
    ring_read_ = r.u32() % ring_.size();
    ring_count_ = std::min<size_t>(r.u32(), ring_.size()) & ~size_t{1};
    load_square(r, ch1_);
    load_square(r, ch2_);
    ch3_.enabled = r.b();
    ch3_.freq = static_cast<uint16_t>(r.u16() & 0x7FF);
    ch3_.timer = r.u32();
    ch3_.pos = static_cast<uint8_t>(r.u8() & 0x1F);
    ch3_.length = r.u16();
    ch3_.length_enable = r.b();
    ch4_.enabled = r.b();
    ch4_.timer = r.u32();
    ch4_.lfsr = static_cast<uint16_t>(r.u16() & 0x7FFF);
    ch4_.length = r.u16();
    ch4_.length_enable = r.b();
    ch4_.volume = static_cast<uint8_t>(r.u8() & 0x0F);
    ch4_.env_add = r.b();
    ch4_.env_period = static_cast<uint8_t>(r.u8() & 0x07);
    ch4_.env_timer = r.u8();
}

void Apu::push_sample(int16_t left, int16_t right) {
    if (ring_count_ + 2 > ring_.size()) {
        return;
    }
    ring_[(ring_read_ + ring_count_) % ring_.size()] = left;
    ring_[(ring_read_ + ring_count_ + 1) % ring_.size()] = right;
    ring_count_ += 2;
}

size_t Apu::read_audio(std::span<int16_t> out) {
    const size_t n = std::min(out.size(), ring_count_) & ~size_t{1};
    for (size_t i = 0; i < n; ++i) {
        out[i] = ring_[ring_read_];
        ring_read_ = (ring_read_ + 1) % ring_.size();
    }
    ring_count_ -= n;
    return n;
}

} // namespace gb
