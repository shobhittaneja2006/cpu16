#pragma once
//
// CPU16 — ISA layer.
//
// This layer answers exactly one question: *what does a 16-bit word mean?*
// It has no state and no side effects, which is what lets it be tested
// exhaustively over all 65536 possible words without running a program.
//
// Dependency rule: this file depends on nothing. cpu16_cpu.* depends on this.
// Never the other way round.
//
#include <cstdint>

namespace cpu16 {

// ---------------------------------------------------------------------------
// Architectural constants                                    (isa-spec §1, §8)
// ---------------------------------------------------------------------------

inline constexpr unsigned NUM_REGS   = 8;       // §2
inline constexpr unsigned IMEM_WORDS = 512;     // §8.3  (D11)
inline constexpr unsigned DMEM_WORDS = 512;     // §8.3  (D11)

// 512 words needs 9 address bits. §8.3: bits [15:9] of a computed address are
// ignored and the access wraps. This mask is that truncation, written once.
inline constexpr uint16_t ADDR_MASK = 0x01FF;

// Software convention only — the hardware does not know about these. §2.1
inline constexpr uint8_t REG_RA = 6;
inline constexpr uint8_t REG_SP = 7;

// ---------------------------------------------------------------------------
// Opcode and funct encodings
// ---------------------------------------------------------------------------
enum class Opcode : uint8_t {
    // Eight opcode encodings, ISA §4.
    ALU_R = 0b000,
    ADDI  = 0b001,
    LW    = 0b010,
    SW    = 0b011,
    BEQ   = 0b100,
    BNE   = 0b101,
    LUI   = 0b110,
    JAL   = 0b111,
};

enum class Funct : uint8_t {
    ADD = 0b0000,
    SUB = 0b0001,
    SLT = 0b0010,
    // 0011 reserved 
    AND = 0b0100, 
    OR  = 0b0101,
    XOR = 0b0110,
    NOT = 0b0111,
    SLL = 0b1000,
    SRL = 0b1001,
    SRA = 0b1010,
    // 1011 reserved
    JALR = 0b1100,
    HALT = 0b1101,
    //rest are the spare opcodes reserved 
};

// ---------------------------------------------------------------------------
// A decoded instruction
// ---------------------------------------------------------------------------
//
// decode() extracts fields for the selected format; other fields stay zero.
// raw preserves the complete instruction word.
//
// Note what is NOT here: sign-extended immediates. `imm10` is sign-extended
// for JAL and shifted left 6 for LUI (§6, D-findings #3), so extending it
// inside decode() would bake one instruction's interpretation into a function
// that is supposed to be format-agnostic. Keep the raw bits here; interpret
// them at the point of use.
//
struct Instr {
    uint16_t raw    = 0;   // the whole word, kept for the trace

    uint8_t  opcode = 0;   // [15:13]
    uint8_t  rA     = 0;   // [12:10]
    uint8_t  rB     = 0;   // [9:7]
    uint8_t  rC     = 0;   // [2:0]      — RRR only
    uint8_t  funct  = 0;   // [6:3]      — RRR only
    uint16_t imm7   = 0;   // [6:0]      — RRI only, RAW (not extended)
    uint16_t imm10  = 0;   // [9:0]      — RI  only, RAW (not extended)

    // §9.2: an undefined *opcode* cannot occur (all 8 are assigned), but a
    // reserved *funct* can. Set false when opcode is ALU-R and funct is one
    // of the four reserved codes.
    bool     legal_funct = true;
};

// ---------------------------------------------------------------------------
// Sign extension                                                      (§9.6)
// ---------------------------------------------------------------------------

int32_t sext7(uint16_t imm7_raw);
int32_t sext10(uint16_t imm10_raw);

// ---------------------------------------------------------------------------
// Decode / encode
// ---------------------------------------------------------------------------

Instr    decode(uint16_t word);
uint16_t encode(const Instr& in);

}  // namespace cpu16
