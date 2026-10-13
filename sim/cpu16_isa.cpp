#include "cpu16_isa.hpp"

namespace cpu16 {

// ---------------------------------------------------------------------------
int32_t sext7(uint16_t imm7_raw) {

    int32_t value = static_cast<int32_t>(imm7_raw & 0x7Fu);
    if ((value & 0x40) != 0) {
        return value - 128;
    }

    return value;
}


// ---------------------------------------------------------------------------
int32_t sext10(uint16_t imm10_raw) {
    int32_t value = static_cast<int32_t>(imm10_raw & 0x3FFu);
    if ((value & 0x200) != 0) {
        return value - 1024;
    }

    return value;
}

// ---------------------------------------------------------------------------
Instr decode(uint16_t word) {
    Instr in;
    in.raw = word;

    in.opcode = static_cast<uint8_t>((word>>13) & 0b111u);
    in.rA = static_cast<uint8_t>((word>>10) & 0b111u);
    //these 2 are always there

    switch (in.opcode)
    {
    case 0b000: //if ALU-R 
    in.funct = static_cast<uint8_t>((word>>3) & 0b1111u);
    in.rB = static_cast<uint8_t>((word>>7) & 0b111u);
    in.rC = static_cast<uint8_t>((word>>0) & 0b111u);

    switch (in.funct)
    {
    case 0b0011: case 0b1011: case 0b1110: case 0b1111:
        in.legal_funct = false;
        break;
    
    default:
        break;
    }
    break;

    case 0b001: case 0b010: case 0b011: case 0b100: case 0b101: // if opcode is RRI 
    in.rB = static_cast<uint8_t>((word>>7) & 0b111u);
    in.imm7 = static_cast<uint16_t>((word>>0) & 0b1111111u);
    break;

    case 0b110: case 0b111: //if opcode is RI
    in.imm10 = static_cast<uint16_t>((word>>0) & 0b1111111111u);
    break;

   
    default:
    in.legal_funct = false; 
    break;
   }

    return in;
}

// ---------------------------------------------------------------------------
uint16_t encode(const Instr& in) {

    uint16_t word = 0;
    
    word |= static_cast<uint16_t>((in.opcode & 0b111u) << 13);

    switch (in.opcode)
    {
    case 0b000:
        
        word |= static_cast<uint16_t>((in.rA & 0b111u) << 10);
        word |= static_cast<uint16_t>((in.rB & 0b111u) << 7);
        word |= static_cast<uint16_t>((in.funct & 0b1111u) << 3);
        word |= static_cast<uint16_t>((in.rC & 0b111u) << 0);    
        break;
    
    case 0b001: case 0b010: case 0b011: case 0b100: case 0b101: 

        word |= static_cast<uint16_t>((in.rA & 0b111u) << 10);
        word |= static_cast<uint16_t>((in.rB & 0b111u) << 7);
        word |= static_cast<uint16_t>((in.imm7 & 0b1111111u) << 0);  
        break;

    case 0b110: case 0b111: 
        
        word |= static_cast<uint16_t>((in.rA & 0b111u) << 10);
        word |= static_cast<uint16_t>((in.imm10 & 0b1111111111u) << 0);  

        break;


    default:
        break;
    }

    return word;
}

}  // namespace cpu16
