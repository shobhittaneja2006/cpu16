#include "cpu16_cpu.hpp"

#include <cstdio>
#include <iostream>

namespace cpu16 {

// ===========================================================================
// Scaffolding — written for you. Nothing below this line up to the TODO
// markers encodes any ISA semantics.
// ===========================================================================

bool operator==(const State& a, const State& b) {
    return a.regs == b.regs
        && a.dmem == b.dmem
        && a.pc == b.pc
        && a.halted == b.halted;
}

bool operator!=(const State& a, const State& b) { return !(a == b); }

// ---------------------------------------------------------------------------
std::string format_trace(const Retire& r) {
    char buf[128];
    std::snprintf(buf, sizeof buf,
                  "PC=%04x IR=%04x RW=%u RD=%u RV=%04x "
                  "MW=%u MA=%04x MV=%04x NPC=%04x H=%u",
                  static_cast<unsigned>(r.pc),
                  static_cast<unsigned>(r.instr),
                  static_cast<unsigned>(r.reg_written ? 1 : 0),
                  static_cast<unsigned>(r.reg_written ? r.reg_index : 0),
                  static_cast<unsigned>(r.reg_written ? r.reg_value : 0),
                  static_cast<unsigned>(r.mem_written ? 1 : 0),
                  static_cast<unsigned>(r.mem_written ? r.mem_addr : 0),
                  static_cast<unsigned>(r.mem_written ? r.mem_value : 0),
                  static_cast<unsigned>(r.next_pc),
                  static_cast<unsigned>(r.halted ? 1 : 0));
    return std::string(buf);
}
//
// Note the ternaries: when an enable is 0 the index and value fields are
// FORCED to zero rather than printed as whatever happened to be there.
//
// This is not tidiness. In your RTL, when reg_write == 0 the writeback bus
// still carries something — usually the ALU result. If the testbench prints
// it and this model prints 0, every non-writing instruction mismatches and
// none of them are bugs. The rule has to be identical on both sides, so it
// belongs in the spec as a sentence, not in your head.

// ---------------------------------------------------------------------------
Cpu::Cpu() { reset(); }

void Cpu::reset() {
    st_.regs.fill(0);      // D13 — r1..r7 cleared, so an unwritten register
                           // reads 0 here AND 0 in the RTL, rather than 0 in
                           // one and X in the other
    st_.pc     = 0;        // §7.1
    st_.halted = false;
    cycles_    = 0;
    // dmem_ is NOT cleared here: it is loaded per-program by load_dmem().
}

void Cpu::load_imem(const std::vector<uint16_t>& image) {
    imem_.fill(0);         // §8.5 — zero-fill BEFORE loading, so a runaway PC
                           // fetches 0x0000 = ADD r0,r0,r0 = NOP rather than X
    const size_t n = image.size() < imem_.size() ? image.size() : imem_.size();
    if (image.size() > imem_.size()) {
        std::cerr << "warning: program is " << image.size()
                  << " words, IMEM is " << imem_.size() << " — truncated\n";
    }
    for (size_t i = 0; i < n; ++i) imem_[i] = image[i];
}

void Cpu::load_dmem(const std::vector<uint16_t>& image) {
    st_.dmem.fill(0);      // §8.5 — an unwritten location reads 0 in both models
    const size_t n = image.size() < st_.dmem.size() ? image.size() : st_.dmem.size();
    if (image.size() > st_.dmem.size()) {
        std::cerr << "warning: data image is " << image.size()
                  << " words, DMEM is " << st_.dmem.size() << " — truncated\n";
    }
    for (size_t i = 0; i < n; ++i) st_.dmem[i] = image[i];
}

// ---------------------------------------------------------------------------
RunStatus Cpu::run(unsigned max_cycles) {
    for (unsigned i = 0; i < max_cycles; ++i) {
        const RunStatus s = step(nullptr);
        if (s != RunStatus::Running) return s;
    }
    return RunStatus::CycleLimit;
}

// ===========================================================================
// Everything below is yours.
// ===========================================================================

// ---------------------------------------------------------------------------
uint16_t Cpu::read_reg(uint8_t r) const {

    if(r == 0){
        return 0;
    }else return st_.regs[r];

}

void Cpu::write_reg(uint8_t r, uint16_t v) {
    // r0 is hardwired zero: discard writes.
    if(r == 0){
        return;
    }else {st_.regs[r] = v;}
}

uint16_t Cpu::read_mem(uint16_t addr) const {

    uint16_t index = addr & ADDR_MASK;

    if(addr >= DMEM_WORDS){
        std::cout << "Warning out of range read so wrapping around\n";
    } 
    return st_.dmem[index];

}

void Cpu::write_mem(uint16_t addr, uint16_t v) {
    uint16_t index = addr & ADDR_MASK;

    if(addr >= DMEM_WORDS){
        std::cout << "Warning out of range write so wrapping around\n";
    } 
    st_.dmem[index] = v;
    return;
}

// ---------------------------------------------------------------------------
RunStatus Cpu::step(Retire* rec) {
    if (st_.halted) return RunStatus::Halted;

    const uint16_t cur_pc = st_.pc;
    Retire r{};
    r.pc = cur_pc;

    // -- 1. FETCH -----------------------------------------------------------
    // §7.7 — the PC is 16 bits but IMEM is 512 words, so only
    // PC[8:0] selects. Flag PC[15:9] != 0 and continue.

    if(cur_pc >= IMEM_WORDS){
        std::cout << "Warning fetch is out of range, so wrapping around\n";
    } 

    uint16_t index = cur_pc & ADDR_MASK; // Mask for wrap around 

    const uint16_t word = imem_[index];
    r.instr = word;

    // -- 2. DECODE ----------------------------------------------------------
    const Instr in = decode(word);
    // Reserved instructions stop at the faulting PC without retiring.
    // Emit a diagnostic record, but do not increment the instruction count.
    if (!in.legal_funct) {
        std::cerr << "Illegal instruction at PC=" << cur_pc
                  << " word=" << word << '\n';
        st_.halted = true;
        r.next_pc = cur_pc;
        r.halted = true;
        if (rec) *rec = r;
        if (trace_) *trace_ << format_trace(r) << '\n';
        return RunStatus::IllegalInstruction;
    }

    // -- 3. EXECUTE ---------------------------------------------------------
    //
    // next_pc is computed ONCE, up front, with the default already correct.
    // Instructions that redirect control overwrite it. If instead each case
    // assigned st_.pc itself, a case that forgot would leave the PC unchanged
    // — an infinite loop that looks like a hang rather than like a missing
    // line. With this shape, forgetting is impossible.
    //
    // The static_cast is load-bearing: cur_pc + 1 promotes to int, so the
    // addition happens in 32 bits and the cast is what makes 0xffff wrap to
    // 0x0000. That wrap is architectural, so removing the cast changes the
    // machine, not just the warning count.
    uint16_t next_pc = static_cast<uint16_t>(cur_pc + 1);

    // Dispatch on opcode, then funct for ALU-R.
    //
    // Two disciplines to hold to, both of which mirror the hardware:
    //
    //   READ EVERYTHING FIRST, WRITE AT THE END.
    //     const uint16_t a = read_reg(in.rB);
    //     const uint16_t b = read_reg(in.rC);
    //     ... compute into locals ...
    //     write_reg(in.rA, result);
    //   ADD r1, r1, r2 reads r1 and writes r1 in the same instruction. In
    //   hardware the old value is what gets read, and that falls out of async
    //   reads plus a sync write (§9.1). In C++ it does not fall out of
    //   anything — you have to be deliberate about it.
    //
    //   ALL ARITHMETIC IN uint16_t / uint32_t, MASKED TO 16 BITS. §9.6:
    //   int16_t + int16_t promotes to 32-bit int and does not wrap, and
    //   signed overflow is undefined behaviour in C++, so the result can
    //   differ between -O0 and -O2. Unsigned wrapping is fully defined.
    //   Reinterpret as signed only for SLT and for display.
    //
    // Fill r.reg_written / r.reg_index / r.reg_value and the mem_* fields as
    // you go, and overwrite next_pc for JAL, JALR and taken branches.
    //
    // If in.legal_funct is false: report it, set halted, return
    // RunStatus::IllegalInstruction.

    switch (in.opcode)
    {
    case 0b000:

        switch (in.funct)
        {
        case 0b0000:{
            //ADD 
            uint16_t B = read_reg(in.rB);
            uint16_t C = read_reg(in.rC);

            uint32_t sum = static_cast<uint32_t>(B) + C;
            uint16_t result = static_cast<uint16_t>(sum);

            write_reg(in.rA, result);   

            if (in.rA != 0) {
            r.reg_written = true;
            r.reg_index = in.rA;
            r.reg_value = result;
            }
            break;
        }

        case 0b0001:{
            //SUB
            uint32_t B = read_reg(in.rB);
            uint16_t C = read_reg(in.rC);

            uint32_t difference = B - C;
            uint16_t result = static_cast<uint16_t>(difference);

            write_reg(in.rA, result);

            if (in.rA != 0) {
            r.reg_written = true;
            r.reg_index = in.rA;
            r.reg_value = result;
            }
            break;
        }
        case 0b0010:{
            //SLT
            int16_t B = static_cast<int16_t>(read_reg(in.rB));
            int16_t C = static_cast<int16_t>(read_reg(in.rC));
            uint16_t result{};


            if(B < C){
                write_reg(in.rA, 1);
                result = 1;
            }else {write_reg(in.rA, 0); result = 0;}


            if (in.rA != 0) {
            r.reg_written = true;
            r.reg_index = in.rA;
            r.reg_value = result;
            }

            break;
        }
        
        case 0b0100:{
            //AND
            uint16_t B = read_reg(in.rB);
            uint16_t C = read_reg(in.rC);

            uint16_t AND = B & C; 

            write_reg(in.rA, AND);

            if (in.rA != 0) {
            r.reg_written = true;
            r.reg_index = in.rA;
            r.reg_value = AND;
            }

            break;
        }
        case 0b0101:{
            //OR

            uint16_t B = read_reg(in.rB);
            uint16_t C = read_reg(in.rC);

            uint16_t OR = B | C; 
            write_reg(in.rA, OR);

            if (in.rA != 0) {
            r.reg_written = true;
            r.reg_index = in.rA;
            r.reg_value = OR;
            }


            break;
        }
        case 0b0110:{
            //XOR
            uint16_t B = read_reg(in.rB);
            uint16_t C = read_reg(in.rC);

            uint16_t XOR = B ^ C; 
            write_reg(in.rA, XOR);

            if (in.rA != 0) {
            r.reg_written = true;
            r.reg_index = in.rA;
            r.reg_value = XOR;
            }
            break;
        }
        
        case 0b0111:{
            //NOT
            uint16_t B = read_reg(in.rB);

            uint16_t NOT = ~B; 
            write_reg(in.rA, NOT);

            if (in.rA != 0) {
            r.reg_written = true;
            r.reg_index = in.rA;
            r.reg_value = NOT;
            }
            break;
        }
        case 0b1000:{
            //SLL
            uint16_t B = read_reg(in.rB);
            uint16_t amount = read_reg(in.rC) & 0xFu;

            uint16_t SLL = static_cast<uint16_t>(static_cast<uint32_t>(B) << amount);
            write_reg(in.rA, SLL);

            if (in.rA != 0) {
            r.reg_written = true;
            r.reg_index = in.rA;
            r.reg_value = SLL;
            }
            break;
        }
        case 0b1001:{
            //SRL
            uint16_t B = read_reg(in.rB);
            uint16_t amount = read_reg(in.rC) & 0xFu;

            uint16_t SRL = static_cast<uint16_t>(static_cast<uint32_t>(B) >> amount);
            write_reg(in.rA, SRL);

            if (in.rA != 0) {
            r.reg_written = true;
            r.reg_index = in.rA;
            r.reg_value = SRL;
            }
            break;
        }
        
        case 0b1010:{
            //SRA
            uint16_t B = read_reg(in.rB);
            uint16_t amount = read_reg(in.rC) & 0xFu;

            // Extend the sign into an unsigned 32-bit value before shifting.
            // This avoids implementation-defined negative signed shifts in C++17.
            uint32_t extended = B;
            if ((B & 0x8000u) != 0) {
                extended |= 0xFFFF0000u;
            }
            uint16_t SRA = static_cast<uint16_t>(extended >> amount);
            write_reg(in.rA, SRA);

            if (in.rA != 0) {
            r.reg_written = true;
            r.reg_index = in.rA;
            r.reg_value = SRA;
            }
            break;
        }
        
        case 0b1100:{
            //JALR

            uint16_t target = read_reg(in.rB);
            uint16_t hold = next_pc;

            write_reg(in.rA, hold);
            next_pc = target;

            if (in.rA != 0) {
            r.reg_written = true;
            r.reg_index = in.rA;
            r.reg_value = hold;
            }

            break;
        }
        case 0b1101:{
            //HALT
            st_.halted = true;
            next_pc = cur_pc;

            break;
        }
        default:
            break;
        }

        break;
    
    case 0b001:{
        //ADDI
        uint16_t B = read_reg(in.rB);

        uint16_t sum = static_cast<uint16_t>(static_cast<int32_t>(B) + sext7(in.imm7));
        write_reg(in.rA, sum);

        if (in.rA != 0) {
            r.reg_written = true;
            r.reg_index = in.rA;
            r.reg_value = sum;
            }

        break;
    }
    case 0b010: {
        //LW
        uint16_t B = read_reg(in.rB);

        uint16_t sum = static_cast<uint16_t>(static_cast<int32_t>(B) + sext7(in.imm7));
        uint16_t loaded = read_mem(sum);
        write_reg(in.rA, loaded);

        if (in.rA != 0) {
            r.reg_written = true;
            r.reg_index = in.rA;
            r.reg_value = loaded;
            }

        break;
        }
    case 0b011:{
        //SW

        uint16_t A = read_reg(in.rA);
        uint16_t B = read_reg(in.rB);
        uint16_t sum = static_cast<uint16_t>(static_cast<int32_t>(B) + sext7(in.imm7));


        write_mem(sum, A);

        r.mem_written = true;
        r.mem_addr = sum;
        r.mem_value = A;

        break;
    }
    case 0b100:{
        //BEQ
        uint16_t A = read_reg(in.rA);
        uint16_t B = read_reg(in.rB);

        if(A == B){
            next_pc = static_cast<uint16_t>(
        static_cast<int32_t>(cur_pc) + 1 + sext7(in.imm7));
        }

        break;
    }
    case 0b101:{
        //BNE
        uint16_t A = read_reg(in.rA);
        uint16_t B = read_reg(in.rB);

        if(A != B){
            next_pc = static_cast<uint16_t>(
        static_cast<int32_t>(cur_pc) + 1 + sext7(in.imm7));
        }
        break;
    }
    case 0b110:{
        //LUI

        uint16_t result = static_cast<uint16_t>(static_cast<uint32_t>(in.imm10) << 6);
        write_reg(in.rA, result);

        if (in.rA != 0) {
            r.reg_written = true;
            r.reg_index = in.rA;
            r.reg_value = result;
        }
        break;

    }
    case 0b111:{
        //JAL
        uint16_t link = static_cast<uint16_t>(cur_pc + 1);
        write_reg(in.rA, link);
        next_pc = static_cast<uint16_t>(static_cast<int32_t>(cur_pc) + 1 + sext10(in.imm10));

        if (in.rA != 0) {
            r.reg_written = true;
            r.reg_index = in.rA;
            r.reg_value = link;
            }

        break;
        }
    
    default:
        break;
    }

    // -- 4. COMMIT ----------------------------------------------------------
    st_.pc     = next_pc;
    r.next_pc  = next_pc;
    r.halted   = st_.halted;
    ++cycles_;

    if (rec)    *rec = r;
    if (trace_) *trace_ << format_trace(r) << '\n';

    return st_.halted ? RunStatus::Halted : RunStatus::Running;
}

}  // namespace cpu16
