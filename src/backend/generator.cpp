#include "backend/generator.h"

#include <assert.h>

#define TODO assert(0 && "todo")
#define NOPIC fout << "\t.option nopic\n"
#define TEXT  fout << "\t.text\n"
#define GLOBAL(_v) fout << "\t.globl " << (_v) << "\n"
#define ALIGN(_n) fout << "\t.align " << (_n) << "\n"
#define DATA fout << "\t.data\n"
#define WORD(_v) fout << "\t.word " << (_v) << "\n"
#define SIZE(_v) fout << "\t.size " << (_v) << ", " << (_v) << "_end - " << (_v) << "\n"
#define TYPE(_v, _t) fout << "\t.type " << (_v) << ", " << (_t) << "\n"
#define BSS fout << "\t.bss\n"

backend::Generator::Generator(ir::Program &p, std::ofstream &f) : program(p), fout(f) {}

std::string rv::toString(rvREG r)
{
    switch (r)
    {
    case rvREG::X0:
        return "zero";
    case rvREG::X1:
        return "ra";
    case rvREG::X2:
        return "sp";
    case rvREG::X3:
        return "gp";
    case rvREG::X4:
        return "tp";
    case rvREG::X5:
        return "t0";
    case rvREG::X6:
        return "t1";
    case rvREG::X7:
        return "t2";
    case rvREG::X8:
        return "s0";
    case rvREG::X9:
        return "s1";
    case rvREG::X10:
        return "a0";
    case rvREG::X11:
        return "a1";
    case rvREG::X12:
        return "a2";
    case rvREG::X13:
        return "a3";
    case rvREG::X14:
        return "a4";
    case rvREG::X15:
        return "a5";
    case rvREG::X16:
        return "a6";
    case rvREG::X17:
        return "a7";
    case rvREG::X18:
        return "s2";
    case rvREG::X19:
        return "s3";
    case rvREG::X20:
        return "s4";
    case rvREG::X21:
        return "s5";
    case rvREG::X22:
        return "s6";
    case rvREG::X23:
        return "s7";
    case rvREG::X24:
        return "s8";
    case rvREG::X25:
        return "s9";
    case rvREG::X26:
        return "s10";
    case rvREG::X27:
        return "s11";
    case rvREG::X28:
        return "t3";
    case rvREG::X29:
        return "t4";
    case rvREG::X30:
        return "t5";
    case rvREG::X31:
        return "t6";
    default:
        return "invalid";
    }
}

std::string rv::toString(rvFREG r)
{
    switch (r)
    {
    case rvFREG::F0:
        return "f0";
    case rvFREG::F1:
        return "f1";
    case rvFREG::F2:
        return "f2";
    case rvFREG::F3:
        return "f3";
    case rvFREG::F4:
        return "f4";
    case rvFREG::F5:
        return "f5";
    case rvFREG::F6:
        return "f6";
    case rvFREG::F7:
        return "f7";
    case rvFREG::F8:
        return "f8";
    case rvFREG::F9:
        return "f9";
    case rvFREG::F10:
        return "f10";
    case rvFREG::F11:
        return "f11";
    case rvFREG::F12:
        return "f12";
    case rvFREG::F13:
        return "f13";
    case rvFREG::F14:
        return "f14";
    case rvFREG::F15:
        return "f15";
    case rvFREG::F16:
        return "f16";
    case rvFREG::F17:
        return "f17";
    case rvFREG::F18:
        return "f18";
    case rvFREG::F19:
        return "f19";
    case rvFREG::F20:
        return "f20";
    case rvFREG::F21:
        return "f21";
    case rvFREG::F22:
        return "f22";
    case rvFREG::F23:
        return "f23";
    case rvFREG::F24:
        return "f24";
    case rvFREG::F25:
        return "f25";
    case rvFREG::F26:
        return "f26";
    case rvFREG::F27:
        return "f27";
    default:
        return "invalid";
    }
}

std::string rv::toString(rvOPCODE r)
{
    switch (r)
    {
    case rvOPCODE::ADD:
        return "add";
    case rvOPCODE::SUB:
        return "sub";
    case rvOPCODE::XOR:
        return "xor";
    case rvOPCODE::OR:
        return "or";
    case rvOPCODE::AND:
        return "and";
    case rvOPCODE::SLL:
        return "sll";
    case rvOPCODE::SRL:
        return "srl";
    case rvOPCODE::SRA:
        return "sra";
    case rvOPCODE::SLT:
        return "slt";
    case rvOPCODE::SLTU:
        return "sltu";
    default:
        return "invalid";
    }
}

// int backend::stackVarMap::add_operand(ir::Operand op, uint32_t size)
// {
//     if (_table.find(op) != _table.end()) // already in the map
//         return _table[op];
//     int offset = -4 * (_table.size() + size); // alloc space for this variable
//     _table[op] = offset;
//     return offset;
// }

void backend::Generator::gen()
{
    NOPIC;
    TEXT;
    // generate global variables
    
    // generate functions
    // for (const auto &f : program.functions)
    // {
    //     gen_func(f);
    // }
}