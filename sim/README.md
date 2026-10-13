# CPU16 C++ golden model

`main.cpp` contains both self-tests and the command-line runner. The CPU executes one instruction per `step()`. `run()` stops at HALT, a reserved instruction, or the default 100,000-instruction limit.

## PowerShell: build and test

From the `core` directory:

```powershell
g++ -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wswitch-enum -Werror sim/main.cpp sim/cpu16_isa.cpp sim/cpu16_cpu.cpp -o sim/cpu16sim.exe
.\sim\cpu16sim.exe --test
```

If `g++` is not on PATH, replace `g++` with `& 'C:/msys64/ucrt64/bin/g++.exe'`.
Rebuild after changing C++ source or headers. A successful test run reports zero failed checks and exits with code 0. The tests use the public CPU API and independently constructed instruction words; no private state is exposed.

With GNU Make installed, from `sim`, use `make -f Makefile.mk test` (the filename is not the default `Makefile`).

## Run the included program

From `core`:

```powershell
.\sim\cpu16sim.exe sim/sum-array.hex sim/sum-data.hex -t sim/sum.trace
```

Expected: `halted after 19 instructions`; `r3=0015` (hexadecimal 0x15 = decimal 21). The program sums data words 5, 7 and 9. It initialises pointer/count/sum, then repeats load, add, increment pointer, decrement count and branch, finishing with HALT.

## Run your own program

```powershell
.\sim\cpu16sim.exe path/to/prog.hex
.\sim\cpu16sim.exe path/to/prog.hex path/to/data.hex -t path/to/model.trace
```

The program image supplies instruction memory; the optional data image supplies data memory. Without a data image, data starts at zero. Files contain whitespace-separated 16-bit hexadecimal words, with optional `//` or `#` comments and blank lines. This is a sequential subset of readmemh format: address directives such as `@0100`, and X/Z values, are not supported. Maximum image size is 512 words per memory; the model currently warns and truncates longer images.

Registers print in hexadecimal. Exit codes: 0 = normal HALT/test success; 1 = failed tests, illegal instruction or cycle limit; 2 = input/usage error. In PowerShell inspect `$LASTEXITCODE` immediately after a run.

## Read the trace

Each line describes one executed instruction: `PC` and `IR` are the current PC/instruction; `RW/RD/RV` describe a register write; `MW/MA/MV` describe a memory write; `NPC` is the resulting PC; `H` is the halted flag. Addresses and values are hexadecimal; register numbers are decimal. Writes dropped at r0 have RW=0. Memory addresses retain the computed 16-bit address, before selecting the low nine bits.

A reserved instruction produces a diagnostic trace line and stops at its PC without incrementing the completed-instruction count. HALT itself counts once; subsequent steps do not change state.

When RTL is ready, run both models with identical images and compare traces in instruction order. A mismatch identifies where their observable behaviour first differs. This simulator is not synthesis or proof of RTL correctness.

## Test scope

Exhaustive sign-extension inputs and instruction decode/encode words; directed ALU boundary/alias/r0 cases; signed comparisons and masked shifts; immediate arithmetic and LUI; branches and links; memory boundaries and wraparound; reset/HALT behaviour; illegal instructions; cycle limit; a complete sum-array program; exact trace formatting. These tests do not exhaust every possible multi-instruction program.
