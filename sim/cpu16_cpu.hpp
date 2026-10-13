#pragma once
//
// CPU16 — machine layer.
//
// Architectural state, and the function that advances it by one instruction.
//
#include "cpu16_isa.hpp"

#include <array>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

namespace cpu16 {

// ---------------------------------------------------------------------------
// Architectural state
// ---------------------------------------------------------------------------
//
// The rule that governs what is allowed in here:
//
//     If the RTL is not architecturally *required* to have it, the model
//     must not model it.
//
// So: no pipeline registers, no control signals, no ALU flags (D7 — the ALU
// computes zero/carry/overflow internally but none of them survive the cycle
// or are readable by any instruction), no notion of muxes or read ports.
//
// IMEM is deliberately NOT in State: nothing in CPU16 can write it (§8.2,
// Harvard), so it is a program *input*, not state. Keeping it out means
// reset() does not unload the program, and a state comparison does not drag
// 512 words of instruction memory along with it.
//
struct State {
    std::array<uint16_t, NUM_REGS>   regs{};   // §9.1: all zero after reset (D13)
    std::array<uint16_t, DMEM_WORDS> dmem{};   // §8.5: zero-filled before load
    uint16_t pc     = 0;                       // §7.1: reset to 0
    bool     halted = false;
};

bool operator==(const State& a, const State& b);
bool operator!=(const State& a, const State& b);

// ---------------------------------------------------------------------------
// Retirement record — one instruction's worth of architecturally visible change
// ---------------------------------------------------------------------------
//
// This struct *is* the trace format, and the trace is the whole verification
// strategy: the RTL testbench emits byte-identical lines and `diff` names the
// first instruction where the two machines disagree.
//
// What is in it: the instruction and where it came from, the architectural
// writes it performed, and where control went.
//
// What is deliberately out: register read values, ALU results, control
// signals. Put those in and the RTL is forced to have a particular internal
// structure just to produce a matching line, and you are no longer comparing
// against the ISA — you are comparing against one implementation of it.
//
struct Retire {
    uint16_t pc      = 0;   // PC this instruction was fetched from
    uint16_t instr   = 0;   // the 16-bit word
    uint16_t next_pc = 0;   // where control actually went

    bool     reg_written = false;
    uint8_t  reg_index   = 0;   // forced to 0 when reg_written is false
    uint16_t reg_value   = 0;   // forced to 0 when reg_written is false

    bool     mem_written = false;
    uint16_t mem_addr    = 0;   // the COMPUTED 16-bit address, not the wrapped
                                // one — so an out-of-range access (§9.4) is
                                // visible in the diff, not only in a warning
    uint16_t mem_value   = 0;   // forced to 0 when mem_written is false

    bool     halted = false;    // state of the halt flag AFTER this instruction
};

// Emits exactly one line, fixed width, no trailing newline:
//
//   PC=0001 IR=0d00 RW=1 RD=3 RV=000a MW=0 MA=0000 MV=0000 NPC=0002 H=0
//
// Lowercase hex with no 0x prefix, because that is what Questa's %h and C++'s
// std::hex both produce by default — one fewer thing to remember on each side.
// Every field is present on every line, so a `diff` never has to cope with a
// shifted column.
//
std::string format_trace(const Retire& r);

// ---------------------------------------------------------------------------
// Why this is an enum and not a bool
// ---------------------------------------------------------------------------
//
// "The model stopped" has causes that mean completely different things.
// CycleLimit in particular is the single most common symptom of a broken
// branch, and collapsing it into "not Halted" throws away the one piece of
// information you wanted.
//
enum class RunStatus {
    Running,
    Halted,              // reached HALT — normal termination
    CycleLimit,          // hung
    IllegalInstruction   // reserved funct (§9.2) — program or assembler is broken
};

// ---------------------------------------------------------------------------
class Cpu {
public:
    Cpu();                       // constructs reset — there is no un-reset CPU

    void reset();
    void load_imem(const std::vector<uint16_t>& image);   // from prog.hex
    void load_dmem(const std::vector<uint16_t>& image);   // from data.hex (D14)

    RunStatus step(Retire* rec = nullptr);
    RunStatus run(unsigned max_cycles = 100000);

    const State& state()  const { return st_; }
    unsigned     cycles() const { return cycles_; }

    // Optional trace sink. nullptr (the default) means emit nothing.
    void set_trace(std::ostream* os) { trace_ = os; }

private:
    // ALL register and memory access goes through these four functions.
    //
    // Right now they look trivial. They are where the rules live:
    //
    //   read_reg   §9.1  r0 always reads 0
    //   write_reg  §9.1  a write to r0 has no observable effect
    //   read_mem   §9.4  addr[8:0] selects; out-of-range is flagged, then
    //   write_mem        the access CONTINUES with the wrapped address
    //
    // Put a rule in one central function and getting it wrong fails every
    // test at once, loudly. Scatter it across twelve instruction cases and
    // you find out three weeks later, in the one program that uses that
    // instruction with r0.
    uint16_t read_reg(uint8_t r) const;
    void     write_reg(uint8_t r, uint16_t v);
    uint16_t read_mem(uint16_t addr) const;
    void     write_mem(uint16_t addr, uint16_t v);

    State                            st_{};
    std::array<uint16_t, IMEM_WORDS> imem_{};
    unsigned                         cycles_ = 0;
    std::ostream*                    trace_  = nullptr;
};

}  // namespace cpu16
