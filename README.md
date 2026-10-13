# CPU16

A custom 16-bit single-cycle CPU implemented in SystemVerilog, with a C++ reference simulator and directed verification testbenches.

This personal project explores processor architecture, instruction encoding, RTL implementation and simulation-based verification.

## Architecture

- 16-bit data and instruction widths.
- Single-cycle execution.
- Harvard architecture with separate instruction and data memories.
- Eight registers, with `r0` hardwired to zero.
- Three asynchronous register read ports and one synchronous write port.
- Separate instruction and data memories, each containing 512 × 16-bit words.
- Word-addressed memory, with the low nine address bits selecting a memory location.
- Synchronous register and memory writes.
- Signed immediate extension and PC-relative branches.
- Latched HALT state, cleared by reset.

Normal execution advances the program counter by one word. Branches and relative jumps calculate their targets from `PC + 1`.

The full instruction encoding and architectural behaviour are documented in [isa-spec.md](isa-spec.md).

## Supported Instructions

All instructions listed below are implemented and working in simulation.

| Category | Instructions | Function |
|---|---|---|
| Arithmetic | `ADD`, `SUB`, `SLT` | Addition, subtraction and signed comparison |
| Bitwise logic | `AND`, `OR`, `XOR`, `NOT` | Register-based logical operations |
| Shifts | `SLL`, `SRL`, `SRA` | Logical and arithmetic shifts |
| Immediate arithmetic | `ADDI` | Addition with a signed immediate |
| Memory | `LW`, `SW` | Load and store a 16-bit word |
| Conditional branches | `BEQ`, `BNE` | Branch on register equality or inequality |
| Upper immediate | `LUI` | Load a 10-bit immediate into bits `[15:6]` |
| Subroutine control | `JAL`, `JALR` | Jump and save the return address |
| Execution control | `HALT` | Stop execution until reset |

Arithmetic results wrap to 16 bits. Shift amounts use the low four bits of the source register.

## Repository Structure

```text
cpu16/
├── rtl/                 SystemVerilog CPU and component modules
├── tb/                  Component and CPU testbenches
├── sim/                 C++ reference simulator and example programs
├── isa-spec.md          Instruction-set specification
├── AI-ASSISTANCE.md      Record of AI assistance
└── README.md
```

The RTL includes the ALU, register file, program counter, instruction decoder, instruction memory, data memory and integrated CPU.

## Requirements

### RTL simulation

- ModelSim or Questa with SystemVerilog support.
- Simulator commands available in the terminal, or access to the simulator's Transcript window.

### C++ reference simulator

- A C++17-compatible compiler, such as GCC.
- GNU Make is optional.

Run the commands below from the repository root unless stated otherwise.

## Run the RTL Simulation

In the ModelSim or Questa Transcript window:

```tcl
vlib work
vmap work work
vlog -sv rtl/*.sv tb/*.sv
vsim work.cpu_tb
add wave -r sim:/cpu_tb/*
run -all
```

If the `work` library already exists, skip `vlib work`.

The current CPU testbench enables directed `JAL` and `JALR` tests, including:

- Forward and backward relative jumps.
- Return-address generation.
- Discarding writes to `r0`.
- Register-indirect calls and returns.
- Using the same register for a jump target and link destination.
- HALT behaviour following the test programs.

A successful run prints:

```text
All enabled jump tests passed
```

Earlier arithmetic, memory and branch tests are preserved in `tb/cpu_tb.sv`, but are currently commented out. They are not executed by the default run above.

### Component Testbenches

After compiling, select a component testbench:

```tcl
quit -sim
vsim work.alu_tb
run -all
```

Available testbench modules are:

| Module | Component |
|---|---|
| `alu_tb` | Arithmetic and logic unit |
| `regs_tb` | Register file |
| `pc_tb` | Program counter |
| `imem_tb` | Instruction memory |
| `dmem_tb` | Data memory |
| `decoder_tb` | Instruction decoder |
| `cpu_tb` | Integrated CPU |

Some component testbenches use stimulus and waveforms for inspection. Do not treat every simulation ending normally as an automated pass.

## Build and Test the C++ Simulator

From a PowerShell terminal with GCC available:

```powershell
g++ -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wswitch-enum -Werror sim/main.cpp sim/cpu16_isa.cpp sim/cpu16_cpu.cpp -o sim/cpu16sim.exe
.\sim\cpu16sim.exe --test
```

Successful self-tests report zero failed checks and return exit code `0`.

Alternatively, with GNU Make installed:

```powershell
cd sim
make -f Makefile.mk test
```

The simulator executes one instruction per step and supports program loading, optional data-memory images and per-instruction trace output.

## Run the Example Program

From the repository root:

```powershell
.\sim\cpu16sim.exe sim/sum-array.hex sim/sum-data.hex -t sim/sum.trace
```

The example sums the data words `5`, `7` and `9`.

Expected results:

```text
halted after 19 instructions
r3=0015
```

`0015` is hexadecimal for decimal `21`.

## Run Another Program

```powershell
.\sim\cpu16sim.exe path/to/program.hex
.\sim\cpu16sim.exe path/to/program.hex path/to/data.hex -t path/to/model.trace
```

Input files contain whitespace-separated 16-bit hexadecimal words. Blank lines and `//` or `#` comments are supported.

The simulator's input format does not support address directives such as `@0100`, or unknown `X`/`Z` values.

See [sim/README.md](sim/README.md) for detailed usage and trace-field descriptions.

## Verification

The project contains component-level and integrated CPU testbenches, alongside a C++ reference simulator.

The C++ self-test suite includes checks for:

- Instruction encoding and decoding.
- Sign extension.
- Arithmetic boundaries and signed comparisons.
- Shift behaviour.
- Immediate operations.
- Register-zero behaviour.
- Loads, stores and memory-address wrapping.
- Branches and jump-link behaviour.
- Reset, HALT and illegal-instruction handling.
- Trace formatting.
- A complete sum-array program.

RTL instruction behaviour has been exercised during development. The currently enabled integrated testbench focuses on jumps; earlier tests remain in the source.

A complete automated RTL-to-C++ trace-comparison regression is not included in this repository. Instruction implementation and full regression coverage are separate milestones.

## Current Scope and Limitations

- The project is a single-cycle CPU core.
- RTL simulation test programs currently initialise memory through the testbench.
- The RTL memory modules do not currently load external program images automatically.
- Reserved instruction encodings are not handled identically by the RTL and C++ simulator.
- FPGA deployment, synthesis utilisation and timing results are not included.
- The core does not include a pipeline, caches, interrupts or memory-mapped peripherals.

## Future Work


### Architecture Extensions
- Develop a multi-cycle implementation and compare it with the single-cycle design.
- Extend the instruction set with additional operations, such as multiplication and unsigned comparisons.
- Develop a pipelined implementation with hazard detection, forwarding and branch handling.
- Add memory-mapped peripherals, such as UART, GPIO and a timer.
- Explore interrupt support and the architectural state needed for interrupt handling.

### FPGA Implementation
- Deploy the CPU on an FPGA and demonstrate a working program.
- Measure resource utilisation and timing, and compare implementation trade-offs.

## AI Assistance

AI assistance was used for architectural explanations, datapath guidance, code review, selected code examples, testbench development and tooling.

The scope is documented in [AI-ASSISTANCE.md](AI-ASSISTANCE.md).