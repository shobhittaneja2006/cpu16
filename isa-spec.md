# CPU16 ISA Specification

## 1. Overview
   - word size, instruction width, register count, memory model (pull straight from D1-D6)
   - I will be using a word size of 16 bits, this will be the same as my imem width. This decision was purely made to keep the design simple and also the basic functionality i am aiming for fits withing the 16 bit instruction width 
   - I will have 8 registers, r0 is hard wired to always have the value 0 stored in it
   - I will be using the harvard model, this is needed to keep the cpu single cycle for every instruction 
   - Single Cycle 

   **Rationale**

   - why 16 bit word, matched to instruction width: Keeps it simple and imem and dmem is the same width which makes it simpler to understand for now 
   - why harvard and not von neuman: a single cycle can fetch and access data with harvard, this is needed to keep the functionality of my cpu core to single cycle.  
   - Why 8 registers and `r0` hardwired : 8 register are more than sufficent for the simplcity of this cpu core and harwiring r0 to the vlaue 0 helps me carry out insturctions without needed an actual opcode for them, the examples are listed below 

   | Pseudo-Instruction | Real Encoding     | cost without `r0` set to 0    |
   |--------------------|-------------------|-------------------------------|
   | MOV rA, rB         | ADD rA, rB, r0    | a funct slot (which i do have) |
   | LI rA, imm         | ADDI rA, r0, imm7 | saves an opcode               |
   | NOP (do nothing)   | ADD r0, r0, r0    | saves me a funct slot         |
   | J label            | JAL r0, imm10     | saves an opcode               |
   | BZ rA, label       | BEQ rA, r0, imm7  | saves an opcode               |
   | BNZ rA, label      | BNE rA, r0, imm7  | saves an opcode               |
   | RET                | JALR r0, r6       | saves a funct slot            |
   | NEG rA, rB         | SUB rA, r0, rB    | saves a funct slot            |





## 2. Register Set
   - table: register name | encoding | convention (r0 hardwired zero, any others reserved by convention?)

   | Register | Encoding | Convention |
   |----------|----------|------------|
   | r0 | 000 | hardwired 0 |
   | r1 | 001 | general purpose |
   | r2 | 010 | general purpose |
   | r3 | 011 | general purpose |
   | r4 | 100 | general purpose |
   | r5 | 101 | general purpose |
   | r6 | 110 | general purpose (used as `ra`, the return address) |
   | r7 | 111 | general purpose (used as `sp`, the stack pointer) |

### 2.1 Calling Convention (software convention — not hardware-enforced)

   - sp = r7. Stack grows downward: push decrements sp, pop increments sp.
   - sp is initialized to <top address of your DMEM> by every program's
   first instructions, before any call happens.
   - ra = r6, holds the return address written by JAL/JALR.
   - Every function that itself makes a call must save ra to the stack
   before that call, and restore it before returning.
   - No frame pointer — all local access is sp-relative.

## 3. Instruction Formats
   - RRR, RRI (ADDI/LW/SW/BEQ/BNE), RI (LUI/JAL) bit-layout tables
   - confirm: does JAL need a destination register field, or is the link register fixed (e.g. always r7/ra)?
   - 3 main instruction formats 
      - RRR 
      - RRI for ADDI/LW/SW/BEQ/BNE
      - RI for LUI/JAL
   - Yes the JAL needs an explicit destination register field

## 4. Opcode Table
   - opcode (binary) | mnemonic | format

   | opcode | mnemonic | format |
   |--------|----------|--------|
   | 000 | ALU-R | RRR |
   | 001 | ADDI  | RRI |
   | 010 | LW    | RRI |
   | 011 | SW    | RRI |
   | 100 | BEQ   | RRI |
   | 101 | BNE   | RRI |
   | 110 | LUI   | RI  |
   | 111 | JAL   | RI  |




## 5. Funct Table
   - funct (binary) | mnemonic | ALU operation | 
   - all 12 ops + explicitly mark the 4 reserved codes
   - these are the instructions for RRR 

   | funct (binary) | mnemonic | ALU operation                            |
   |----------------|----------|------------------------------------------|
   | 0000           | ADD      | rA ← rB + rC                             |
   | 0001           | SUB      | rA ← rB − rC                             |
   | 0010           | SLT      | rA ← (rB < rC) ? 1 : 0  (signed)         |
   | 0011           | reserved | — (kept adjacent to SLT for future SLTU) |
   | 0100           | AND      | rA ← rB & rC                             |
   | 0101           | OR       | rA ← rB \| rC                            |
   | 0110           | XOR      | rA ← rB ^ rC                             |
   | 0111           | NOT      | rA ← ~rB   (rC unused)                   |
   | 1000           | SLL      | rA ← rB << rC[3:0]                       |
   | 1001           | SRL      | rA ← rB >> rC[3:0]  (logical, zero-fill) |
   | 1010           | SRA      | rA ← rB >>> rC[3:0] (arithmetic, sign-fill) |
   | 1011           | reserved | — (spare shift slot)                     |
   | 1100           | JALR     | rA ← PC+1 ; PC ← rB(old)  (rC unused)    |
   | 1101           | HALT     | halted ← 1 ; PC holds  (rA/rB/rC unused) |
   | 1110           | reserved | —                                        |
   | 1111           | reserved | —                                        |

   ### 5.1 Why the codes are grouped this way

   `funct[3:2]` classifies the operation, so the decoder can test two bits instead of
   comparing against scattered 4-bit values:

   | funct[3:2] | class                | members                  |
   |------------|----------------------|--------------------------|
   | `00`       | arithmetic / compare | ADD, SUB, SLT, *(SLTU)*  |
   | `01`       | bitwise              | AND, OR, XOR, NOT        |
   | `10`       | shift                | SLL, SRL, SRA, *(spare)* |
   | `11`       | **not an ALU op**    | JALR, HALT               |

   The `11` group is the one that pays for itself. JALR and HALT are the only two RRR
   instructions that are not ALU operations — HALT clears `reg_write`, JALR sets `jump`
   — which means `reg_write` and `jump` depend on `funct` as well as `opcode`. With this
   grouping that dependency reduces to a single test, `funct[3:2] == 2'b11`, with one bit
   below it separating JALR from HALT. Under a scattered assignment the control unit would
   need two full 4-bit equality comparisons instead.



   ### 5.2 Notes

   - `rC[3:0]` indicates the bottom 4 bits of the **value stored in** register rC — not
     the register numbered `rC[3:0]`. Bits `[15:4]` of that value are ignored, so the
     shift amount is always in the range 0–15 and an out-of-range shift is unreachable.
   - Spare funct codes could be used for MUL, REM or DIV, but since my CPU is single-cycle
     by design a 16×16 multiplier or a divider would dominate the critical path and slow
     the whole machine down. Multiplication and division are done in software.
   - **SRA is included** because without it the ISA has no signed division at all. DIV is
     excluded for the reason above, so division by powers of two is done by shifting — and
     SRL gets that wrong for negative values (`SRL(-8, 1)` = 32764, `SRA(-8, 1)` = -4).
     The cost is one spare funct slot; the barrel shifter from mini-project 1.3 already
     implements arithmetic shift, so there is no new datapath hardware.
   - **SLA (shift left arithmetic) is deliberately absent** because it is not a distinct
     operation: arithmetic and logical left shift produce identical results, since zeros
     fill in from the right either way. Adding it would waste a funct slot on an alias.

## 6. Per-Instruction Semantics

One subsection per mnemonic: encoding | operation | PC update | notes.

<!--
CONVENTIONS ASSUMED BY THIS SECTION (state these once, earlier in the spec,
so they are not repeated on every instruction):

  - reg[r0] always reads as 0. Writes to r0 have no observable effect.
  - Unused instruction fields: assembler emits zeros, hardware ignores them (D10).
  - Arithmetic wraps modulo 2^16. No flags are architecturally visible (D7, D10).
  - The "Effects" row lists control-unit outputs. It is a cross-reference for the
    Phase 4 control truth table, not an architectural requirement.

RESOLVED since the last draft:
  - SRA added at funct 1010; JALR moved to 1100, HALT to 1101 (see 5.1).
  - LW/SW out-of-range address: wraps within DMEM, flagged by assertion in both models.
  - Effects rows now carry `branch` / `jump` where they apply. `wb_src` is still not
    listed per-instruction — add it if the Phase 4 truth table needs it.

-->

---

### ADD

| Field | Value |
|---|---|
| **Encoding** | `[15:13]=000` `[12:10]=rA` `[9:7]=rB` `[6:3]=0000` `[2:0]=rC` |
| **Operation** | reg[rA] ← reg[rB] + reg[rC] |
| **PC** | PC ← PC + 1 |
| **Effects** | reg_write = 1. mem_write = 0. halted unchanged. |
| **Notes** | Result wraps modulo $2^{16}$; overflow is not detected or signalled (D7, D10). |

### SUB

| Field | Value |
|---|---|
| **Encoding** | `[15:13]=000` `[12:10]=rA` `[9:7]=rB` `[6:3]=0001` `[2:0]=rC` |
| **Operation** | reg[rA] ← reg[rB] − reg[rC] |
| **PC** | PC ← PC + 1 |
| **Effects** | reg_write = 1. mem_write = 0. halted unchanged. |
| **Notes** | Result wraps modulo $2^{16}$; overflow is not detected or signalled (D7, D10). `NEG rA, rB` = `SUB rA, r0, rB`. |

### SLT

| Field | Value |
|---|---|
| **Encoding** | `[15:13]=000` `[12:10]=rA` `[9:7]=rB` `[6:3]=0010` `[2:0]=rC` |
| **Operation** | reg[rA] ← (reg[rB] < reg[rC]) ? 1 : 0 — **signed** comparison |
| **PC** | PC ← PC + 1 |
| **Effects** | reg_write = 1. mem_write = 0. halted unchanged. |
| **Notes** | Both operands are interpreted as signed two's complement. Result is 0 or 1; it cannot overflow. Unsigned comparison (`SLTU`) is **not** provided — `funct = 0011` is reserved for it (D6). |

### AND

| Field | Value |
|---|---|
| **Encoding** | `[15:13]=000` `[12:10]=rA` `[9:7]=rB` `[6:3]=0100` `[2:0]=rC` |
| **Operation** | reg[rA] ← reg[rB] & reg[rC] (bitwise) |
| **PC** | PC ← PC + 1 |
| **Effects** | reg_write = 1. mem_write = 0. halted unchanged. |
| **Notes** | Bitwise operation; cannot overflow. |

### OR

| Field | Value |
|---|---|
| **Encoding** | `[15:13]=000` `[12:10]=rA` `[9:7]=rB` `[6:3]=0101` `[2:0]=rC` |
| **Operation** | reg[rA] ← reg[rB] \| reg[rC] (bitwise) |
| **PC** | PC ← PC + 1 |
| **Effects** | reg_write = 1. mem_write = 0. halted unchanged. |
| **Notes** | Bitwise operation; cannot overflow. |

### XOR

| Field | Value |
|---|---|
| **Encoding** | `[15:13]=000` `[12:10]=rA` `[9:7]=rB` `[6:3]=0110` `[2:0]=rC` |
| **Operation** | reg[rA] ← reg[rB] ^ reg[rC] (bitwise) |
| **PC** | PC ← PC + 1 |
| **Effects** | reg_write = 1. mem_write = 0. halted unchanged. |
| **Notes** | Bitwise operation; cannot overflow. |

### NOT

| Field | Value |
|---|---|
| **Encoding** | `[15:13]=000` `[12:10]=rA` `[9:7]=rB` `[6:3]=0111` `[2:0]=` unused |
| **Operation** | reg[rA] ← ~reg[rB] (bitwise complement) |
| **PC** | PC ← PC + 1 |
| **Effects** | reg_write = 1. mem_write = 0. halted unchanged. |
| **Notes** | Unary. The `rC` field is unused: the assembler emits zeros, the hardware ignores it (D10). Read port 2 still reads reg[rC]; the value is discarded. |

### SLL

| Field | Value |
|---|---|
| **Encoding** | `[15:13]=000` `[12:10]=rA` `[9:7]=rB` `[6:3]=1000` `[2:0]=rC` |
| **Operation** | reg[rA] ← reg[rB] << reg[rC][3:0] |
| **PC** | PC ← PC + 1 |
| **Effects** | reg_write = 1. mem_write = 0. halted unchanged. |
| **Notes** | Shift amount is bits `[3:0]` of the **value held in** reg[rC]; bits `[15:4]` are ignored, so the shift amount is always 0–15. Zeros shift in from the right; bits shifted past bit 15 are discarded. |

<!-- Note the notation: reg[rC][3:0] = low 4 bits of the value in rC.
     NOT reg[rC[3:0]], which would mean "the register numbered rC[3:0]". -->

### SRL

| Field | Value |
|---|---|
| **Encoding** | `[15:13]=000` `[12:10]=rA` `[9:7]=rB` `[6:3]=1001` `[2:0]=rC` |
| **Operation** | reg[rA] ← reg[rB] >> reg[rC][3:0] (logical) |
| **PC** | PC ← PC + 1 |
| **Effects** | reg_write = 1. mem_write = 0. halted unchanged. |
| **Notes** | Shift amount is bits `[3:0]` of the value held in reg[rC]; bits `[15:4]` are ignored. **Logical** shift: zeros shift in from the left regardless of sign, so the result is only meaningful for unsigned values. For signed values use `SRA`. |

### SRA

| Field | Value |
|---|---|
| **Encoding** | `[15:13]=000` `[12:10]=rA` `[9:7]=rB` `[6:3]=1010` `[2:0]=rC` |
| **Operation** | reg[rA] ← reg[rB] >>> reg[rC][3:0] (arithmetic) |
| **PC** | PC ← PC + 1 |
| **Effects** | reg_write = 1. mem_write = 0. halted unchanged. |
| **Notes** | Shift amount is bits `[3:0]` of the value held in reg[rC]; bits `[15:4]` are ignored. **Arithmetic** shift: bit 15 is replicated into the vacated high bits, so the sign is preserved.<br><br>**Why this instruction exists:** without it the ISA has no signed division. D6 excludes `DIV` because a single-cycle divider would dominate the critical path, so division is done in software by shifting — and `SRL` is wrong for negative values. `SRL(-8, 1)` gives `0x7FFC` = 32764; `SRA(-8, 1)` gives `0xFFFC` = −4. Costs one spare funct slot and no new datapath hardware, since the barrel shifter already implements arithmetic shift.<br><br>Note the asymmetry: there is no `SLA`, because arithmetic and logical **left** shift are the same operation. |

### JALR

| Field | Value |
|---|---|
| **Encoding** | `[15:13]=000` `[12:10]=rA` `[9:7]=rB` `[6:3]=1100` `[2:0]=` unused |
| **Operation** | reg[rA] ← PC + 1 (the link) |
| **PC** | **PC ← reg[rB]** (absolute — no PC-relative arithmetic) |
| **Effects** | reg_write = 1. mem_write = 0. halted unchanged. Sets `jump`. |
| **Notes** | The `rC` field is unused. Target is absolute, so `JALR` can reach any address; `JAL` cannot. If `rA` and `rB` name the same register, the jump uses the **old** value, because the register file reads before it writes (D10). `RET` = `JALR r0, r6` (r6 = `ra`, per §2.1). `funct[3:2] == 11` marks this as a non-ALU op (§5.1). |

### HALT

| Field | Value |
|---|---|
| **Encoding** | `[15:13]=000` `[12:10]=` unused `[9:7]=` unused `[6:3]=1101` `[2:0]=` unused |
| **Operation** | None. Asserts the top-level `halted` output. |
| **PC** | **PC ← PC** (holds indefinitely) |
| **Effects** | reg_write = 0. mem_write = 0. **halted ← 1**. |
| **Notes** | All register fields unused. Once halted, the processor makes no further architectural state changes; `halted` remains asserted until reset. Holding the PC rather than freezing the clock lets a testbench distinguish "halted" from "hung" (D10). `funct[3:2] == 11` marks this as a non-ALU op (§5.1). |

---

### ADDI

| Field | Value |
|---|---|
| **Encoding** | `[15:13]=001` `[12:10]=rA` `[9:7]=rB` `[6:0]=imm7` |
| **Operation** | reg[rA] ← reg[rB] + sext(imm7) |
| **PC** | PC ← PC + 1 |
| **Effects** | reg_write = 1. mem_write = 0. halted unchanged. |
| **Notes** | `imm7` is signed, range $[-64, +63]$. Result wraps modulo $2^{16}$; overflow not signalled. `LI rA, imm` = `ADDI rA, r0, imm`. `MOV` is `ADD rA, rB, r0`, not an ADDI form. |

### LW

| Field | Value |
|---|---|
| **Encoding** | `[15:13]=010` `[12:10]=rA` `[9:7]=rB` `[6:0]=imm7` |
| **Operation** | reg[rA] ← DMEM[ reg[rB] + sext(imm7) ] |
| **PC** | PC ← PC + 1 |
| **Effects** | reg_write = 1. mem_write = 0. halted unchanged. |
| **Notes** | `rA` is the **destination**; `rB` is the base address. Address is word-addressed (D1) — one address is one 16-bit word, no scaling. Offset range $[-64, +63]$ words from the base.<br><br>**Out-of-range address:** the computed address is 16 bits but DMEM is 512 words, so only bits `[8:0]` select the word — bits `[15:9]` are ignored and the access **wraps** within DMEM. This costs no hardware, since it is simply what happens when the upper address bits are left unconnected. Both the RTL (SVA assertion) and the C++ model (`assert`) **flag** an access with `addr[15:9] != 0`; both warn and continue with the wrapped address, so the trace diff stays aligned. |

### SW

| Field | Value |
|---|---|
| **Encoding** | `[15:13]=011` `[12:10]=rA` `[9:7]=rB` `[6:0]=imm7` |
| **Operation** | DMEM[ reg[rB] + sext(imm7) ] ← reg[rA] |
| **PC** | PC ← PC + 1 |
| **Effects** | reg_write = 0. mem_write = 1. halted unchanged. |
| **Notes** | **`rA` is a SOURCE here, not a destination** — this is the only non-branch instruction where `[12:10]` is read rather than written. `rB` is the base address. `SW` is the only instruction that asserts `mem_write` (D9).<br><br>**Out-of-range address:** same rule as `LW` — wraps within DMEM using `addr[8:0]`, flagged by an assertion in both models, which warn and continue. |

### BEQ

| Field | Value |
|---|---|
| **Encoding** | `[15:13]=100` `[12:10]=rA` `[9:7]=rB` `[6:0]=imm7` |
| **Operation** | None — no register or memory write |
| **PC** | if reg[rA] == reg[rB]: PC ← PC + 1 + sext(imm7)<br>else: PC ← PC + 1 |
| **Effects** | reg_write = 0. mem_write = 0. halted unchanged. Sets `branch`. |
| **Notes** | `rA` and `rB` are both **sources**. Comparison is bitwise equality, so signedness is irrelevant. Target is relative to PC+1 (D10); the assembler computes `imm = target − (branch_addr + 1)`. Range $[-64, +63]$ instructions. `BZ rA, label` = `BEQ rA, r0, imm7`. |

### BNE

| Field | Value |
|---|---|
| **Encoding** | `[15:13]=101` `[12:10]=rA` `[9:7]=rB` `[6:0]=imm7` |
| **Operation** | None — no register or memory write |
| **PC** | if reg[rA] != reg[rB]: PC ← PC + 1 + sext(imm7)<br>else: PC ← PC + 1 |
| **Effects** | reg_write = 0. mem_write = 0. halted unchanged. Sets `branch`. |
| **Notes** | Identical to `BEQ` but with the condition inverted. Differs from `BEQ` in opcode bit `[13]` only, so the condition sense is a single instruction bit (D9). `BNZ rA, label` = `BNE rA, r0, imm7`. |

---

### LUI

| Field | Value |
|---|---|
| **Encoding** | `[15:13]=110` `[12:10]=rA` `[9:0]=imm10` |
| **Operation** | reg[rA] ← { imm10, 6'b0 } — `imm10` occupies bits `[15:6]`, bits `[5:0]` cleared |
| **PC** | PC ← PC + 1 |
| **Effects** | reg_write = 1. mem_write = 0. halted unchanged. |
| **Notes** | Reads no registers. `imm10` is **shifted, not sign-extended**. Overwrites `rA` completely — it does not merge with the previous contents. Bypasses the ALU and drives the writeback mux directly, because no pass-through `funct` op exists. Pairs with `ADDI` to build a full 16-bit constant: `imm10` covers bits `[15:6]`, `ADDI`'s positive range $[0,63]$ covers bits `[5:0]` exactly, so no `%hi`/`%lo` fixup is needed (D8). |

### JAL

| Field | Value |
|---|---|
| **Encoding** | `[15:13]=111` `[12:10]=rA` `[9:0]=imm10` |
| **Operation** | reg[rA] ← PC + 1 (the link — the address of the *next* instruction, not the target) |
| **PC** | **PC ← PC + 1 + sext(imm10)** |
| **Effects** | reg_write = 1. mem_write = 0. halted unchanged. Sets `jump`. |
| **Notes** | Reads no registers. `imm10` is **sign-extended** here, unlike `LUI` — same field, different immediate-generator mode. Range $[-512, +511]$, which covers the whole 512-word IMEM from any address, so `JAL` never needs a `JALR` trampoline (D11). `J label` = `JAL r0, imm10`. A function call is `JAL r6, label` (r6 = `ra`, per §2.1). |

---


## 7. Control Flow & Addressing

### 7.1 Default behaviour

The PC is a 16-bit register. Unless an instruction overrides it, every cycle:

$$PC \leftarrow PC + 1$$

`+1` rather than `+4` because memory is word-addressed (D1) — one address is one 16-bit
word. **Reset sets PC to 0**, because `$readmemh` loads IMEM starting at address 0 (D10).

### 7.2 Next-PC selection

Exactly one source drives the PC each cycle, in this priority order:

| Priority | Condition | Next PC | Notes |
|---|---|---|---|
| 1 | `reset` asserted | `0` | Synchronous reset |
| 2 | `halted` already set | `PC` (holds) | Latched state — see §7.6 |
| 3 | `JAL` (opcode `111`) | `PC + 1 + sext(imm10)` | PC-relative |
| 3 | `JALR` (opcode `000`, funct `1100`) | `reg[rB]` | **Absolute** |
| 4 | `BEQ`/`BNE` with condition true | `PC + 1 + sext(imm7)` | PC-relative |
| 5 | everything else | `PC + 1` | Includes branches not taken |

Priorities 3 and 4 are mutually exclusive by construction — no instruction is both a jump
and a branch — so the next-PC mux needs three data inputs (`PC+1`, the PC-relative target,
`reg[rB]`) and a two-bit select.

### 7.3 PC-relative targets — branches and `JAL`

Both use the same base:

$$PC_{\text{next}} = PC + 1 + \text{sext}(imm)$$

`PC + 1` is already computed every cycle for the default case, so reusing it as the branch
base costs no extra adder input. Computing `PC + imm` instead would need a second adder fed
from the raw PC. This also matches MIPS and RISC-V, so published reference material lines up
with this machine (D10).

**The assembler computes the immediate as:**

$$imm = \text{target} - (\text{instruction address} + 1)$$

This formula must be identical in the assembler, the C++ golden model and the RTL. It is
the single most likely off-by-one in the whole project, and it fails in a way that looks like
a decode bug: every call and branch lands one instruction early.

Worked example — `JAL r6, double` sitting at address 1, with `double` at address 4:

```
imm    = 4 − (1 + 1) = 2
link   : r6 ← PC + 1 = 2
jump   : PC ← 1 + 1 + 2 = 4        
```

| Instruction | Immediate | Range |
|---|---|---|
| `BEQ`, `BNE` | `imm7` signed | $[-64, +63]$ instructions |
| `JAL` | `imm10` signed | $[-512, +511]$ instructions |

`JAL`'s ±512 range covers the entire 512-word IMEM from any address (D11), so `JAL` never
needs a `LUI`/`ADDI`/`JALR` trampoline to reach a target.

### 7.4 Absolute targets — `JALR`

`JALR` takes its target from a register, used directly as an address with no arithmetic:

$$PC_{\text{next}} = reg[rB]$$

This exists because a return address is not known at assemble time — a function called from
two sites must return to two different places, so the target has to come from a register the
caller filled in. `JAL` writes that register; `JALR` consumes it.

Since the target is a full 16-bit value, `JALR` can reach anywhere in the address space, and
an arbitrary address is built with `LUI` + `ADDI` (D8). That capability is not needed for
program flow in this machine, given §7.3, but it is what makes computed jumps possible.

**If `rA` and `rB` name the same register**, the jump uses the register's *old* value: the
register file reads before it writes (D10).

### 7.5 Link semantics

Both `JAL` and `JALR` write `PC + 1` — the address of the instruction *after* the jump, i.e.
where execution would have continued — into `reg[rA]`.

| Written value | Not |
|---|---|
| `PC + 1`, the return address | the jump target |

Writing the link to `r0` discards it, which is how `J label` (= `JAL r0, imm10`) and
`RET` (= `JALR r0, r6`) are expressed without spending opcodes on them (D3).

**Nesting.** There is one `ra` (`r6`), so a function that itself makes a call overwrites its
own return address. Nested and recursive calls must save `ra` to the stack before calling and
restore it before returning — see §2.1. The hardware provides no stack and no automatic save;
this is entirely software convention.

### 7.6 `HALT`

`HALT` sets a latched `halted` flip-flop and the PC holds its value. `halted` remains asserted
until reset. The clock is *not* stopped — holding the PC rather than gating the clock keeps the
design observable in a waveform and lets a testbench distinguish "reached HALT" from "hung in a
loop" (D10).

### 7.7 PC out of range

The PC is 16 bits but IMEM is 512 words, so only `PC[8:0]` selects an instruction and execution
wraps. Both models flag `PC[15:9] != 0` and continue — see §9.

---

## 8. Memory Model

### 8.1 Addressing

**Word-addressed** (D1). One address selects one 16-bit word. There is no byte addressing, no
scaling of offsets, and therefore **no alignment rule at all** — every address is by definition
aligned, because a word is the smallest addressable unit.

The ISA has no sub-word operations: there is no `LB`, `LBU` or `SB`. Adding them would have
required byte-lane write enables, an alignment policy, and offset scaling on branches, for no
benefit to these workloads (D1).

### 8.2 Harvard split

**Two physically separate memories with two separate address spaces** (D2):

| | IMEM | DMEM |
|---|---|---|
| Holds | instructions | data |
| Width | 16 bits | 16 bits |
| Depth | 512 words | 512 words |
| Addressed by | `PC` | `reg[rB] + sext(imm7)` |
| Written by | nothing — read-only to the program | `SW` only |
| Loaded from | `prog.hex` | `data.hex` |

Address 100 in IMEM and address 100 in DMEM are **different locations**. The two spaces are
numerically unrelated.

Harvard is required by the single-cycle design: an instruction fetch and a data access must
both complete in the same cycle, which a single-ported unified memory cannot do. It also means
a stray `SW` cannot corrupt the running program — useful during integration, when a wild store
would otherwise turn into an instruction-decode mystery (D2).

**No instruction writes IMEM.** There is no self-modifying code and no mechanism to produce it.

### 8.3 Depth and the address bits

Both memories are **512 words** (D11). 512 is the largest depth for which `JAL`'s ±512 range
covers the whole of IMEM from any address, so any `JAL` reaches any instruction.

A 512-word memory needs 9 address bits, but the computed address is 16 bits. **Bits `[15:9]`
are ignored and the access wraps.** This costs nothing — it is what happens when the upper
address bits are left unconnected. Both models flag an out-of-range access and continue; see §9.

### 8.4 DMEM memory map

Nothing in hardware enforces this layout. It is a software convention that the assembler and
the test programs cooperate on:

```
  addr 511  ┌──────────────────────────┐
            │  stack                   │  grows DOWN
            │  (sp starts at 512)      │
            │            ↓             │
            ├ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─┤  ← collision is a program bug,
            │            ↑             │     not detected by hardware
            │  data section            │  grows UP
            │  (.fill, from data.hex)  │
   addr 0   └──────────────────────────┘
```

Stack overflow into the data section, and data growing up into the stack, are **not detected**.
There is no bound register and no protection. A program that does this corrupts itself silently.

### 8.5 Initialisation

**Both memories are zero-filled before `$readmemh` runs.**

This is not cosmetic. `$readmemh` only writes the addresses present in the hex file; everything
else retains its initial value, which for `logic` is `X`. Two consequences:

| Memory | Why zero-fill matters |
|---|---|
| **IMEM** | A runaway PC fetches `0x0000`, which decodes as `ADD r0, r0, r0` — a `NOP`. Execution stays benign and the PC assertion fires cleanly, instead of `X` propagating through the entire datapath and producing a waveform that says nothing. This property depends on `ADD` being `funct 0000` and `r0` being register 0 — **do not break it by renumbering.** |
| **DMEM** | A read of an unwritten location returns 0 in the RTL *and* 0 in the C++ model. Without zero-fill it is `X` in one and `0` in the other, and the trace diff reports a mismatch that has nothing to do with the CPU. |

### 8.6 Toolchain: two address spaces, two output files

The assembler emits two files from one source:

| Directive | Location counter | Output | Labels resolve to |
|---|---|---|---|
| `.text` (default) | IMEM counter | `prog.hex` | IMEM addresses |
| `.data` | DMEM counter | `data.hex` | DMEM addresses |

`.fill <value>` places one initialised word in the data section.

Because the spaces are unrelated, the assembler tracks which space each label belongs to and
**errors on cross-space use**:

| Usage | Legal? |
|---|---|
| `LI r1, array` where `array` is a `.data` label | ✅ — a DMEM address as an immediate |
| `BEQ r1, r2, loop` where `loop` is a `.text` label | ✅ — an IMEM address as a branch target |
| `BEQ r1, r2, array` | ❌ — branching to a data address |
| `LI r1, loop` | ❌ — loading a code address as data |

That check is worth the extra assembler logic: without it, `BEQ r1, r2, array` assembles
cleanly and jumps into nowhere.

**Both the testbench and the C++ model load both files.** The C++ model takes them as two
CLI arguments so that RTL and reference model are demonstrably fed identical inputs.

### 8.7 Not in scope for L1

No memory-mapped I/O, no bus, no address decoder, no peripherals. All of that is L2 (autumn),
and none of it changes this ISA.

---

## 9. Edge Cases and Undefined Behaviour

Every case below has a **defined** answer. Anything that could be left implementation-defined
has been pinned down instead, because the entire verification strategy is diffing the RTL trace
against the C++ golden model — and two implementations can only be compared if they are
supposed to agree.

### 9.1 Register file

| Case | Behaviour | Reason |
|---|---|---|
| Reading `r0` | Always 0 | Hardwired (D3) |
| Writing `r0` | No observable effect | The write may physically land in an invisible location(I chose to discard it in the golden model); the read path forces 0 regardless |
| Read-during-write (same register read and written in one cycle) | The read returns the **old** value | Forced by the design: asynchronous reads, synchronous write. Otherwise `ADD r1, r1, r2` would be circular |
| `JALR rA, rB` where `rA` and `rB` are the same register | Jumps to the register's **old** value | Same mechanism as above |
| `r1`–`r7` after reset | All 0 | Chosen for determinism across both models — see §2 |
| Reading a register never written | Returns 0 | Follows from reset behaviour. A use-before-write bug shows as a wrong *value*, not as `X` |

### 9.2 Instruction decode

| Case | Behaviour | Reason |
|---|---|---|
| Undefined opcode | **Cannot occur** — all 8 opcodes are assigned (D9) | 3 opcode bits, 8 opcodes, none spare |
| Reserved `funct` (`0011`, `1011`, `1110`, `1111`) | Undefined, but **flagged** | RTL uses `unique case`, so Questa warns at runtime. C++ model asserts and prints PC and instruction word. Fails loudly rather than silently computing a wrong answer |
| Unused instruction fields | Assembler emits zeros; hardware ignores them | Cheapest possible rule (D10). Read ports still read the corresponding register; the value is discarded |
| Instruction word `0x0000` | Decodes as `ADD r0, r0, r0` = `NOP` | Consequence of `ADD` at `funct 0000` and `r0` being register 0. Relied on by the IMEM zero-fill behaviour in §8.5 |

### 9.3 Arithmetic

| Case | Behaviour | Reason |
|---|---|---|
| Overflow (signed or unsigned) | **Wraps silently**, modulo $2^{16}$ | The register file is 16 bits wide; there is nowhere for a 17th bit to go. D7 chose no architectural flags, so nothing records that it happened |
| Carry / overflow flags | Computed inside the ALU, **not architecturally visible** | The ALU generates zero, negative, carry and overflow. `zero` is consumed by the branch comparison within the same cycle. None persist across instructions and no instruction reads them (D7) |
| `SLT` with operands of different signs | Signed comparison, always correct | `SLT` interprets both operands as two's complement |
| Unsigned comparison | **Not available.** `SLTU` is reserved at `funct 0011` but not implemented | A consequence of D6/D7 worth knowing: without flags *and* without `SLTU`, software carry detection for multi-precision arithmetic is not currently possible |
| Shift amount ≥ 16 | **Unreachable by construction** | The shift amount is `reg[rC][3:0]`; bits `[15:4]` are ignored, so it is always 0–15 (D10). No saturation rule is needed, and none would have been correct for both `SRL` and `SRA` |
| `SRL` on a negative value | Zero-fills — the result is not the signed quotient | Use `SRA` for signed values. This asymmetry is the reason `SRA` exists (D6) |

 **The overflow trap in testing.** The RTL and the C++ model both wrap, identically, so an
overflow *never* appears as a trace mismatch — it appears as a plausible-looking wrong answer.
Keep test inputs in range: Fibonacci is safe to 28657 signed, and recursive factorial only to
$7! = 5040$. $9! = 362880$ silently becomes 51840.

### 9.4 Memory

| Case | Behaviour | Reason |
|---|---|---|
| DMEM address ≥ 512 | Only `addr[8:0]` selects the word; the access **wraps**. Both models flag it and **continue** | Truncation costs nothing — it is what unconnected upper address bits do. Suppressing the access instead would need a comparator plus a further decision about what an out-of-range read returns |
| PC ≥ 512 | Same: `PC[8:0]` selects, execution wraps, both models flag and continue | Same reasoning |
| Reading unwritten DMEM | Returns 0 | Both memories are zero-filled before `$readmemh` (§8.5) |
| Stack overflowing into the data section | **Not detected.** The program corrupts itself | No bound register, no protection hardware. Out of scope for L1 |
| Writing to IMEM | Impossible — no instruction can do it | Harvard split (D2) |

 **Both models must warn and continue, not abort.** An aborting C++ model against a
continuing RTL reports the mismatch at the wrong point in the trace and sends you hunting a
bug that isn't there.

### 9.5 Halt

| Case | Behaviour |
|---|---|
| `HALT` executed | `halted` ← 1, PC holds, no further architectural state changes |
| After `HALT` | `halted` stays asserted until reset. The clock keeps running |
| Testbench use | A run that ends with `halted` asserted terminated normally; a run that hits the cycle limit without it was hung |

### 9.6 Implementation notes for the golden model

Not architecture — these are places where **C++ will not naturally do what SystemVerilog does**,
and where the two models will silently diverge if written carelessly.

| Trap | What goes wrong | The fix |
|---|---|---|
| **Integer promotion** | `int16_t a + int16_t b` promotes both to 32-bit `int`; the sum does not wrap at 16 bits. And signed overflow in C++ is *undefined behaviour*, so results can differ between `-O0` and `-O2` | Do all arithmetic in `uint16_t` / `uint32_t` and mask to 16 bits. Unsigned wrapping is fully defined. Reinterpret as signed only for display and for `SLT` |
| **Shift by ≥ width** | Shifting a 16-bit value by ≥ 16 is undefined behaviour in C++, even though the mask makes it look unreachable | Write the mask explicitly: `val << (shamt & 0xF)` |
| **Sign extension** | Hand-rolled sign extension is easy to get wrong by one bit | Write one helper for `sext7` and one for `sext10`, use them everywhere, and unit-test both against known values including the boundaries $-64$, $+63$, $-512$, $+511$ |
| **Signed right shift** | `>>` on a signed type is implementation-defined before C++20 | Implement `SRA` explicitly rather than relying on the operator |
| **Trace format** | A formatting difference produces a diff on every line | RTL and C++ must emit byte-identical trace lines. Fix the format once, in the spec, before writing either |


