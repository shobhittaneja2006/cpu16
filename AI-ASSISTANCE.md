# AI Assistance Record — CPU16

**Purpose.** This document is a deliberate, honest record of where AI assistance was and was not
used in this project. It exists so that any claim I make about this work — on a CV, in an
application, or in an interview — is specific and checkable rather than vague.

I would rather state plainly that AI helped with C++ scaffolding than imply I wrote everything
and be unable to defend a line of it under questioning.

**Maintained by:** Shobhit Taneja
**Started:** 18 August 2026
**Status:** living document — update it as work happens, not retrospectively at the end.

---

## The rule I set for myself

> All ISA design, all SystemVerilog RTL, and all instruction semantics are mine. AI assistance is
> limited to tooling scaffolding, language-level help, testbench boilerplate, and review of code
> I have already written.

The reason is not academic honesty theatre. It is that:

1. **I have to defend this in an interview.** If I did not design it, I cannot explain why it is
   the way it is, and that is immediately obvious to anyone who asks a second question.
2. **The C++ simulator is a *golden reference model*.** Its entire value is that it is an
   *independent* implementation of the ISA spec. If its semantics were written by the same source
   that helped with the RTL, a misunderstanding in my ISA would propagate identically into both,
   the trace diff would come back clean, and the CPU would be wrong in a way the tooling
   structurally could not detect. An assisted golden model is not a golden model — it is a mirror.

---

## Component breakdown

Legend: **Mine** = designed and written by me · **Assisted** = AI helped, scope stated ·
**Reviewed** = I wrote it, AI critiqued it without supplying the fix

| Component | Status | Detail |
|---|---|---|
| **ISA design** (`docs/isa-spec.md`) | Mine | Every architectural decision is mine. AI was used as a sounding board: it laid out the trade-offs for each decision (addressing mode, register count, operand style, opcode width, branch mechanism, constant synthesis) and I chose. See "ISA decision log" below. |
| **SystemVerilog RTL — all modules** | Mine | ALU, register file, memories, datapath, control unit, top level. No AI-generated RTL. |
| **RTL debugging** | Reviewed | AI names the *class* of bug and asks diagnostic questions; I locate and write the fix. |
| **Testbenches — checks and reference logic** | Mine | The actual assertions, expected-value computation and pass/fail logic. |
| **Testbenches — scaffolding** | Assisted | Clock generation, reset sequencing, file loading, boilerplate. |
| **Python assembler** | Mine (first version) | I wrote the first working version. AI assisted with structure suggestions and debugging thereafter. |
| **C++ golden model — ISA semantics** | Mine | The decode switch, every instruction's behaviour, sign-extension widths, PC update logic, halt detection, `r0` and read-during-write behaviour. |
| **C++ golden model — scaffolding** | Assisted | `main()`, CLI argument handling, `$readmemh` file parsing, struct/array layout, trace output formatting, and C++ language-level help (`<cstdint>` types, bit manipulation idioms, integer promotion and signed/unsigned conversion traps). |
| **Build tooling** | Assisted | Makefiles, Questa `.do` scripts, the trace diff script. |
| **README and documentation** | Mine | Written by me; AI reviewed for clarity. |

---

## ISA decision log

Every architectural decision below is mine. AI's role was to present the trade-offs and the
bit-budget arithmetic; the choice and the justification are mine. Recorded here because the
reasoning matters more than the outcome.

| # | Decision | Chosen | Core reason |
|---|---|---|---|
| D1 | Memory addressing | Word-addressed | No sub-word operations in the ISA, so byte addressing would add byte-lane write enables, `LB`/`LBU`/`SB`, alignment policy and branch-offset scaling for zero benefit to my workloads. |
| D2 | Memory architecture | True Harvard | A single-cycle core must fetch and access data in the same cycle; a single-ported unified memory cannot. Also gives immunity to a stray `SW` corrupting the program during integration. |
| D3 | Register count | 8, `r0` hardwired zero | 3-bit register fields. `r0` buys `MOV`, `LI`, `NOP`, `J`, `RET`, `BZ`/`BNZ` as free pseudo-instructions. |
| D4 | Operand style | 3-operand, non-destructive | 16 registers would force 2-operand in a 16-bit word, dropping branch reach to ±16 instructions and requiring trampolines in hand-written test programs. |
| D5a | Opcode width | 3 bits | Gives `imm7` (±64 branch and load/store reach) and a 4-bit `funct` field yielding 16 ALU ops from one opcode. 4 opcode bits would halve the reach and shrink `funct` below what the op list needs. |
| D5b | Field layout | `rA` at `[12:10]`, `rB` at `[9:7]` in every format | Register file read addresses come straight off the instruction with no decode, so registers can be read before the instruction type is known. |
| D6 | Instruction list | 8 opcodes, 12 defined `funct` operations, 4 reserved codes | No `MUL`/`DIV` — a single-cycle multiplier or divider would dominate the critical path. Includes `SRA`; `0011`, `1011`, `1110`, and `1111` remain reserved. |
| D7 | Branch mechanism | `BEQ` + `BNE`, no architectural flags | Flags save no instructions on my programs (an explicit compare is needed either way), but add architectural state the golden model must match and a hazard when pipelining later. |
| D8 | Constant synthesis | `LUI` + `ADDI` | `LUI`'s `imm10` covers bits `[15:6]` and `ADDI`'s signed `imm7` has positive range `[0,63]`, exactly covering the low 6 bits — so unlike RISC-V, no `%hi`/`%lo` sign-compensation fixup is needed. |
| D9 | Encoding assignment | Opcodes 0–7: ALU-R, ADDI, LW, SW, BEQ, BNE, LUI, JAL | BEQ/BNE and LW/SW each differ by one opcode bit, simplifying shared decode. SW alone asserts `mem_write`. |
| D10 | Spec fine print | Three register read ports; PC-relative targets use PC+1; explicit reset, halt, register, and address rules (below) | Pins down behaviour needed for independent RTL and C++ implementations to agree. |
| D11 | Memory depth | IMEM and DMEM each 512 × 16 bits | JAL can reach every IMEM location from every in-range PC without wrapping. Memory selection uses nine address bits. |
| D12 | Funct grouping | `00`: arithmetic/compare; `01`: bitwise; `10`: shifts; `11`: JALR/HALT and reserved codes | Groups operations for decode. JALR is `1100`, HALT is `1101`; reserved codes must still be distinguished. |
| D13 | Register reset | Reset r1–r7 to zero | Deterministic starting state for RTL/reference-model comparison; r0 always reads zero. |
| D14 | Data initialisation | Assembler emits `prog.hex` and `data.hex` from `.text` and `.data` sections | Separate location counters and label spaces reflect Harvard memory. Both models load the same images, with unused memory zero-filled. |
| D15 | Stack and DMEM layout | Full descending stack; r7/SP starts at 512; data grows upward from address 0 | Push decrements before storing, so the first push uses address 511. Stack/data collision is a software error, not detected by hardware. |

### D10 — detailed decisions

| Item | Decision |
|---|---|
| Register-file reads | Three asynchronous read ports, addressed directly by instruction fields `[12:10]`, `[9:7]`, and `[2:0]`, avoiding decode-dependent read-address selection. |
| PC-relative targets | BEQ, BNE, and JAL use `PC + 1 + sext(imm)`; the link written by JAL/JALR is `PC + 1`. |
| JALR | Absolute target from the old value of rB, including when rA equals rB. |
| Read-during-write | An instruction consumes the old register value before its synchronous write. |
| HALT | Assert a latched `halted` output, hold PC, and prevent further architectural state changes until reset; the clock keeps running. |
| PC reset | PC starts at zero. |
| Unused instruction fields | Assembler emits zeros; hardware ignores them. |
| Arithmetic overflow | Wrap modulo 2^16, with no architectural flags. |
| Shift amount | Low four bits of the value in rC, giving amounts 0–15. |
| Out-of-range memory access | Select using the low nine address bits, warn, and continue; applies to instruction fetch and data access. |
| Memory initialisation | Zero-fill both memories before loading the program/data images. |
| Reserved funct | Flag the instruction. The exact continuation/termination policy remains unresolved between the spec, handover, and C++ skeleton; this entry does not choose one. |
| Undefined opcode | Impossible: all eight opcode encodings are assigned. |

---

## Running log

Append entries as work happens. Keep it specific — "helped with C++" is useless; "AI explained
integer promotion in bitwise expressions, I rewrote the decode masks" is a real record.

| Date | Component | What I did | What AI did |
|---|---|---|---|
| 2026-08-18 | ISA design | Made all ten architectural decisions | Presented trade-offs and bit-budget arithmetic for each decision; did not choose |
| 2026-09-17 | Decision-log reconciliation | Requested D9–D15 be brought up to date | Transcribed previously recorded decisions from the project handover and ISA spec; corrected D6's stale operation count and retained the unresolved reserved-funct policy explicitly. No RTL or instruction semantics implemented. |

---

## What I say about this project

**Accurate:**

- "I designed the ISA myself, including the encoding, and can justify every field width."
- "All SystemVerilog is mine. I used AI for testbench boilerplate and to review code I had written."
- "The C++ golden model's ISA semantics are mine; I had help with C++ scaffolding and language
  issues, because C++ is newer to me than the hardware side."

**Not accurate, and I will not claim it:**

- That the project involved no AI assistance.
- UVM, formal verification, timing closure, or synthesis results I have not produced.

## C++ test harness assistance — 26 September 2026

Due to my limited exposure to C++ and to save time, I explicitly requested AI assistance with `sim/main.cpp`. AI wrote the self-test checks and expected-value calculations, retained the program-loading/trace runner, improved argument validation, and supplied a sum-array demonstration and run instructions. These C++ tests are AI-assisted, including their reference logic; the earlier general statement that test checks are mine does not apply to this suite. I am responsible for reviewing and understanding the tests. Passing them provides useful evidence, not a proof that the implementation and specification contain no shared mistakes.
