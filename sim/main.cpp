//
// CPU16 golden model — harness.
//
//   ./cpu16sim --test                        run the self-tests
//   ./cpu16sim prog.hex [data.hex]           run a program
//   ./cpu16sim prog.hex data.hex -t out.trace   ... and write a trace
//
#include "cpu16_cpu.hpp"
#include "cpu16_isa.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace cpu16;

// ===========================================================================
// Check framework
// ===========================================================================

static int g_checks = 0;
static int g_fails  = 0;

// #cond stringifies the expression, so a failure prints the source text of
// the thing that was false, not just a line number.
#define CHECK(cond, what)                                                    \
    do {                                                                     \
        ++g_checks;                                                          \
        if (!(cond)) {                                                       \
            ++g_fails;                                                       \
            std::cout << "  FAIL  " << (what) << "\n"                        \
                      << "        " #cond "   (line " << __LINE__ << ")\n";  \
        }                                                                    \
    } while (0)

#define CHECK_EQ(got, want, what)                                            \
    do {                                                                     \
        ++g_checks;                                                          \
        const long long g_ = static_cast<long long>(got);                    \
        const long long w_ = static_cast<long long>(want);                   \
        if (g_ != w_) {                                                      \
            ++g_fails;                                                       \
            std::cout << "  FAIL  " << (what) << "\n"                        \
                      << "        got " << g_ << ", want " << w_             \
                      << "   (line " << __LINE__ << ")\n";                   \
        }                                                                    \
    } while (0)

static void section(const char* name) {
    std::cout << "[" << name << "]\n";
}

// ===========================================================================
// Hex file loading — $readmemh format
// ===========================================================================
//
// Accepts one hex word per line, blank lines, and // or # comments. This is
// the same file the SystemVerilog testbench feeds to $readmemh, which is the
// point: both models are demonstrably fed identical input.
//
static bool load_hex(const std::string& path, std::vector<uint16_t>& out) {
    std::ifstream f(path);
    if (!f) {
        std::cerr << "error: cannot open " << path << "\n";
        return false;
    }
    out.clear();
    std::string line;
    unsigned lineno = 0;
    while (std::getline(f, line)) {
        ++lineno;
        const size_t comment = line.find_first_of("/#");
        if (comment != std::string::npos) line.erase(comment);
        std::istringstream ls(line);
        std::string tok;
        while (ls >> tok) {
            char* end = nullptr;
            const unsigned long v = std::strtoul(tok.c_str(), &end, 16);
            if (end == tok.c_str() || *end != '\0' || v > 0xFFFFUL) {
                std::cerr << path << ":" << lineno
                          << ": bad hex word '" << tok << "'\n";
                return false;
            }
            out.push_back(static_cast<uint16_t>(v));
        }
    }
    return true;
}

// ===========================================================================
// AI-assisted tests: expected values come from ISA rules, not Cpu internals.
// Instruction builders below deliberately do not call the production encoder.
// All CPU tests use the public interface.
// ===========================================================================
static uint16_t rrr(unsigned fn, unsigned a, unsigned b, unsigned c) {
    return static_cast<uint16_t>(a * 1024 + b * 128 + fn * 8 + c);
}
static uint16_t rri(unsigned op, unsigned a, unsigned b, int immediate) {
    return static_cast<uint16_t>(op * 8192 + a * 1024 + b * 128
        + (static_cast<unsigned>(immediate) & 127u));
}
static uint16_t ri(unsigned op, unsigned a, int immediate) {
    return static_cast<uint16_t>(op * 8192 + a * 1024
        + (static_cast<unsigned>(immediate) & 1023u));
}
static constexpr uint16_t HALT_WORD = 0x0068;

// Set a register through LUI + ADDI, so no private state access is required.
static void append_constant(std::vector<uint16_t>& program, unsigned reg, uint16_t value) {
    program.push_back(ri(6, reg, value / 64));
    program.push_back(rri(1, reg, reg, value % 64));
}
static void step_running(Cpu& cpu) {
    CHECK(cpu.step() == RunStatus::Running, "setup instruction runs");
}
static void test_sext() {
    section("sign extension: every uint16_t input");
    for (unsigned w = 0; w < 65536; ++w) {
        CHECK_EQ(sext7(static_cast<uint16_t>(w)), static_cast<int>((w + 64) % 128) - 64, "sext7");
        CHECK_EQ(sext10(static_cast<uint16_t>(w)), static_cast<int>((w + 512) % 1024) - 512, "sext10");
    }
}
static void test_decode() {
    section("decode/encode: every instruction word");
    for (unsigned w = 0; w < 65536; ++w) {
        const auto d = decode(static_cast<uint16_t>(w));
        const unsigned op = w / 8192, f = (w / 8) % 16;
        CHECK_EQ(d.raw, w, "raw"); CHECK_EQ(d.opcode, op, "opcode");
        CHECK_EQ(d.rA, (w / 1024) % 8, "rA");
        CHECK_EQ(d.rB, op < 6 ? (w / 128) % 8 : 0, "rB");
        CHECK_EQ(d.rC, op == 0 ? w % 8 : 0, "rC");
        CHECK_EQ(d.funct, op == 0 ? f : 0, "funct");
        CHECK_EQ(d.imm7, op >= 1 && op <= 5 ? w % 128 : 0, "imm7");
        CHECK_EQ(d.imm10, op >= 6 ? w % 1024 : 0, "imm10");
        CHECK(d.legal_funct == (op != 0 || (f != 3 && f != 11 && f != 14 && f != 15)), "reserved detection");
        CHECK_EQ(encode(d), w, "round trip");
        Instr fields;
        fields.opcode = static_cast<uint8_t>(op);
        fields.rA = static_cast<uint8_t>((w / 1024) % 8);
        fields.rB = static_cast<uint8_t>((w / 128) % 8);
        fields.rC = static_cast<uint8_t>(w % 8);
        fields.funct = static_cast<uint8_t>(f);
        fields.imm7 = static_cast<uint16_t>(w % 128);
        fields.imm10 = static_cast<uint16_t>(w % 1024);
        CHECK_EQ(encode(fields), w, "independently constructed fields");
    }
    CHECK_EQ(encode(decode(0x2505)), 0x2505, "ADDI r1,r2,5");
    CHECK_EQ(static_cast<unsigned>(Opcode::JAL), 7, "JAL opcode");
    CHECK_EQ(static_cast<unsigned>(Funct::HALT), 13, "HALT funct");
}
static void test_alu() {
    section("all ALU operations, boundaries, aliases and traces");
    const uint16_t values[] = {0, 1, 2, 15, 16, 31, 0x7FFF, 0x8000, 0xFFF8, 0xFFFF};
    for (unsigned fn : {0u, 1u, 2u, 4u, 5u, 6u, 7u, 8u, 9u, 10u}) {
        for (uint16_t b : values) for (uint16_t c : values) for (unsigned dest : {0u, 1u, 2u, 3u}) {
            std::vector<uint16_t> p;
            append_constant(p, 2, b); append_constant(p, 3, c);
            p.push_back(rrr(fn, dest, 2, 3));
            Cpu cpu; cpu.load_imem(p);
            for (unsigned n = 0; n < 4; ++n) step_running(cpu);
            const State before = cpu.state();
            const int sb = b < 32768 ? b : static_cast<int>(b) - 65536;
            const int sc = c < 32768 ? c : static_cast<int>(c) - 65536;
            const unsigned divisor = 1u << (c % 16);
            uint16_t expected = 0;
            switch (fn) {
                case 0: expected = static_cast<uint16_t>(static_cast<unsigned>(b) + c); break;
                case 1: expected = static_cast<uint16_t>(static_cast<int>(b) - c); break;
                case 2: expected = sb < sc ? 1 : 0; break;
                case 4: expected = static_cast<uint16_t>(b & c); break;
                case 5: expected = static_cast<uint16_t>(b | c); break;
                case 6: expected = static_cast<uint16_t>(b ^ c); break;
                case 7: expected = static_cast<uint16_t>(65535u - b); break;
                case 8: expected = static_cast<uint16_t>(static_cast<unsigned>(b) * divisor); break;
                case 9: expected = static_cast<uint16_t>(b / divisor); break;
                case 10: {
                    const int div = static_cast<int>(divisor);
                    expected = static_cast<uint16_t>(sb >= 0 ? sb / div : -((-sb + div - 1) / div));
                    break;
                }
                default: break;
            }
            Retire r; CHECK(cpu.step(&r) == RunStatus::Running, "ALU runs");
            for (unsigned reg = 0; reg < NUM_REGS; ++reg)
                CHECK_EQ(cpu.state().regs[reg], reg == dest && dest != 0 ? expected : before.regs[reg], "ALU register effects");
            CHECK(cpu.state().dmem == before.dmem, "ALU preserves memory");
            CHECK_EQ(cpu.state().pc, 5, "ALU PC"); CHECK_EQ(cpu.cycles(), 5, "ALU count");
            CHECK(r.reg_written == (dest != 0), "ALU write enable");
            CHECK_EQ(r.reg_index, dest, "ALU trace destination");
            CHECK_EQ(r.reg_value, dest ? expected : 0, "ALU trace value");
            CHECK(!r.mem_written && !r.halted, "ALU trace flags");
        }
    }
}
static void test_r0() {
    section("r0, immediate arithmetic and LUI");
    for (unsigned dest : {0u, 1u}) for (uint16_t base : {uint16_t{0}, uint16_t{32768}, uint16_t{65535}})
        for (int imm : {-64, -1, 0, 1, 63}) {
            std::vector<uint16_t> p; append_constant(p, 1, base);
            p.push_back(rri(1, dest, 1, imm)); Cpu cpu; cpu.load_imem(p);
            step_running(cpu); step_running(cpu); step_running(cpu);
            const auto expected = static_cast<uint16_t>(static_cast<int>(base) + imm);
            CHECK_EQ(cpu.state().regs[dest], dest ? expected : 0, "ADDI wrap/r0/alias");

        }
    for (unsigned imm = 0; imm < 1024; ++imm) for (unsigned dest : {0u, 1u}) {
        Cpu cpu; cpu.load_imem({ri(6, dest, static_cast<int>(imm))}); Retire r;
        CHECK(cpu.step(&r) == RunStatus::Running, "LUI runs");
        CHECK_EQ(cpu.state().regs[dest], dest ? imm * 64 : 0, "LUI result");
        CHECK_EQ(r.reg_value, dest ? imm * 64 : 0, "LUI trace");
        CHECK(r.reg_written == (dest != 0), "LUI r0");
    }
}
static void test_control_flow() {
    section("branches, jumps, halt, illegal instructions and PC wrapping");
    for (unsigned op : {4u, 5u}) for (bool equal : {false, true}) for (int offset : {-64, -1, 0, 63}) {
        Cpu cpu; cpu.load_imem({rri(1, 1, 0, 1), rri(1, 2, 0, equal ? 1 : 2), rri(op, 1, 2, offset)});
        step_running(cpu); step_running(cpu); const State before = cpu.state(); Retire r;
        CHECK(cpu.step(&r) == RunStatus::Running, "branch runs");
        const bool taken = op == 4 ? equal : !equal;
        CHECK_EQ(cpu.state().pc, static_cast<uint16_t>(taken ? 3 + offset : 3), "branch target");
        CHECK(cpu.state().regs == before.regs && !r.reg_written && !r.mem_written, "branch preserves registers");
        CHECK_EQ(r.next_pc, cpu.state().pc, "branch trace");
    }
    for (int offset : {-512, -1, 0, 511}) for (unsigned dest : {0u, 1u}) {
        Cpu cpu; cpu.load_imem({ri(7, dest, offset)}); Retire r;
        CHECK(cpu.step(&r) == RunStatus::Running, "JAL runs");
        CHECK_EQ(cpu.state().pc, static_cast<uint16_t>(1 + offset), "JAL target");
        CHECK_EQ(cpu.state().regs[dest], dest ? 1 : 0, "JAL link");
        CHECK_EQ(r.reg_value, dest ? 1 : 0, "JAL trace link");
    }
    for (unsigned dest : {0u, 1u, 2u}) {
        Cpu cpu; cpu.load_imem({rri(1, 1, 0, 20), rrr(12, dest, 1, 7)});
        step_running(cpu); Retire r; CHECK(cpu.step(&r) == RunStatus::Running, "JALR runs");
        CHECK_EQ(cpu.state().pc, 20, "JALR old target");
        CHECK_EQ(cpu.state().regs[dest], dest ? 2 : 0, "JALR link");
        CHECK_EQ(r.reg_value, dest ? 2 : 0, "JALR trace");
    }
    Cpu halted; halted.load_imem({HALT_WORD}); Retire halt;
    CHECK(halted.step(&halt) == RunStatus::Halted, "HALT status");
    CHECK(halt.halted && halt.next_pc == 0 && !halt.reg_written && !halt.mem_written, "HALT trace");
    const auto stopped = halted.state();
    CHECK(halted.step() == RunStatus::Halted && halted.state() == stopped && halted.cycles() == 1, "HALT stable");
    halted.reset(); CHECK(!halted.state().halted && halted.cycles() == 0 && halted.state().pc == 0, "reset resumes");
    for (unsigned fn : {3u, 11u, 14u, 15u}) {
        Cpu cpu; cpu.load_imem({rrr(fn, 1, 2, 3)}); Retire r;
        std::ostringstream warning; auto* saved = std::cerr.rdbuf(warning.rdbuf());
        const auto status = cpu.step(&r); std::cerr.rdbuf(saved);
        CHECK(status == RunStatus::IllegalInstruction && !warning.str().empty(), "illegal flagged");
        CHECK(cpu.state().halted && cpu.state().pc == 0 && cpu.cycles() == 0 && r.halted, "illegal stops without retirement");
    }
    Cpu loop; loop.load_imem({ri(7, 0, -1)});
    CHECK(loop.run(5) == RunStatus::CycleLimit && loop.cycles() == 5, "cycle limit");
    std::vector<uint16_t> p(512, 0); p[0] = ri(7, 0, -2); p[511] = rri(1, 1, 0, 7);
    Cpu wrap; wrap.load_imem(p); step_running(wrap);
    std::ostringstream warning; auto* saved = std::cout.rdbuf(warning.rdbuf());
    const auto status = wrap.step(); std::cout.rdbuf(saved);
    CHECK(status == RunStatus::Running && wrap.state().regs[1] == 7 && wrap.state().pc == 0, "fetch and PC wrap");
    CHECK(!warning.str().empty(), "fetch warning");
}
static void test_memory() {
    section("loads/stores, address boundaries, wrapping and traces");
    for (uint16_t base : {uint16_t{0}, uint16_t{255}, uint16_t{256}, uint16_t{511}, uint16_t{512}, uint16_t{65535}})
        for (int offset : {-64, -1, 0, 63}) {
            std::vector<uint16_t> p; append_constant(p, 2, base); append_constant(p, 1, 0xBEEF);
            p.push_back(rri(3, 1, 2, offset)); p.push_back(rri(2, 3, 2, offset));
            p.push_back(rri(3, 0, 2, offset)); p.push_back(rri(2, 0, 2, offset));
            Cpu cpu; cpu.load_imem(p); cpu.load_dmem({});
            for (unsigned i = 0; i < 4; ++i) step_running(cpu);
            const uint16_t address = static_cast<uint16_t>(static_cast<int>(base) + offset);
            Retire store, load, zeroStore, zeroLoad;
            std::ostringstream warning; auto* saved = std::cout.rdbuf(warning.rdbuf());
            cpu.step(&store); cpu.step(&load); cpu.step(&zeroStore); cpu.step(&zeroLoad);
            std::cout.rdbuf(saved);
            CHECK_EQ(cpu.state().regs[3], 0xBEEF, "load value");
            CHECK(store.mem_written && store.mem_addr == address && store.mem_value == 0xBEEF, "store trace unmasked address");
            CHECK(load.reg_written && load.reg_value == 0xBEEF && !load.mem_written, "load trace");
            CHECK(zeroStore.mem_written && zeroStore.mem_value == 0 && !zeroLoad.reg_written, "r0 memory semantics");
            CHECK_EQ(cpu.state().dmem[address % 512], 0, "zero stored");
            CHECK(warning.str().empty() == (address < 512), "memory warnings");
        }
    Cpu cpu; cpu.load_imem({rri(2, 1, 0, 0), rri(2, 2, 0, 1)}); cpu.load_dmem({123});
    step_running(cpu); step_running(cpu);
    CHECK_EQ(cpu.state().regs[1], 123, "loaded data image");
    CHECK_EQ(cpu.state().regs[2], 0, "unused data zero filled");
    cpu.reset(); CHECK_EQ(cpu.state().dmem[0], 123, "reset preserves data");
    for (auto value : cpu.state().regs) CHECK_EQ(value, 0, "reset clears registers");
}
static void test_program() {
    section("sum-array program and exact trace");
    // r1=pointer, r2=count, r3=sum, r4=loaded value. Loop addresses 3..7.
    Cpu cpu; cpu.load_imem({0x2400,0x2803,0x2C00,0x5080,0x0D84,0x2481,0x297F,0xA87B,HALT_WORD});
    cpu.load_dmem({5,7,9});
    CHECK(cpu.run(100) == RunStatus::Halted, "sum program halts");
    CHECK_EQ(cpu.state().regs[3], 21, "array sum"); CHECK_EQ(cpu.cycles(), 19, "sum instruction count");
    Cpu tiny; tiny.load_imem({0x2405,HALT_WORD}); std::ostringstream trace; tiny.set_trace(&trace);
    CHECK(tiny.run() == RunStatus::Halted, "trace program halts");
    CHECK(trace.str() == "PC=0000 IR=2405 RW=1 RD=1 RV=0005 MW=0 MA=0000 MV=0000 NPC=0001 H=0\n"
                         "PC=0001 IR=0068 RW=0 RD=0 RV=0000 MW=0 MA=0000 MV=0000 NPC=0001 H=1\n", "exact trace format");
}
static int run_tests() {
    test_sext();
    test_decode();
    test_alu();
    test_r0();
    test_control_flow();
    test_memory();
    test_program();

    std::cout << "\n" << (g_checks - g_fails) << "/" << g_checks
              << " checks passed\n";
    return g_fails == 0 ? 0 : 1;   // makes `make test` fail the build
}

// ===========================================================================
int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);

    if (args.empty()) {
        std::cerr << "usage: cpu16sim --test\n"
                  << "       cpu16sim prog.hex [data.hex] [-t trace.txt]\n";
        return 2;
    }
    if (args[0] == "--test") {
        if (args.size() != 1) { std::cerr << "error: --test takes no arguments\n"; return 2; }
        return run_tests();
    }

    std::string prog_path, data_path, trace_path;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "-t") {
            if (i + 1 >= args.size() || !trace_path.empty()) {
                std::cerr << "error: supply -t once, followed by a trace filename\n"; return 2;
            }
            trace_path = args[++i];
        } else if (!args[i].empty() && args[i][0] == '-') {
            std::cerr << "error: unknown option " << args[i] << "\n"; return 2;
        } else if (prog_path.empty()) {
            prog_path = args[i];
        } else if (data_path.empty()) {
            data_path = args[i];
        } else {
            std::cerr << "error: unexpected argument '" << args[i] << "'\n";
            return 2;
        }
    }

    std::vector<uint16_t> prog, data;
    if (!load_hex(prog_path, prog)) return 2;
    if (!data_path.empty() && !load_hex(data_path, data)) return 2;

    Cpu cpu;
    cpu.load_imem(prog);
    cpu.load_dmem(data);          // empty vector is fine — zero-fills DMEM

    std::ofstream tf;
    if (!trace_path.empty()) {
        // Binary mode on purpose. In text mode on Windows, '\n' becomes
        // "\r\n" here — and if Questa writes bare '\n', every single line of
        // the diff differs for a reason that has nothing to do with the CPU.
        // Check once with: cat -A model.trace | head -2
        tf.open(trace_path, std::ios::binary);
        if (!tf) { std::cerr << "error: cannot write " << trace_path << "\n"; return 2; }
        cpu.set_trace(&tf);
    }

    const RunStatus s = cpu.run();

    const char* why = "?";
    switch (s) {
        case RunStatus::Halted:             why = "halted";              break;
        case RunStatus::CycleLimit:         why = "CYCLE LIMIT (hung)";  break;
        case RunStatus::IllegalInstruction: why = "ILLEGAL INSTRUCTION"; break;
        case RunStatus::Running:            why = "still running";       break;
    }
    std::cout << why << " after " << cpu.cycles() << " instructions\n";

    const State& st = cpu.state();
    for (unsigned i = 0; i < NUM_REGS; ++i) {
        std::printf("r%u=%04x%s", i, st.regs[i], (i % 4 == 3) ? "\n" : "  ");
    }

    return s == RunStatus::Halted ? 0 : 1;
}
