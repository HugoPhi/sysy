#ifndef RV_DEF_H
#define RV_DEF_H

#include <string>

namespace rv
{

    // rv interger registers
    enum class rvREG
    {
        /* Xn       ABI name       Purpose & Calling Convention */
        X0,  // zero         Hard-wired zero, immutable
        X1,  // ra           Return address (caller-saved)
        X2,  // sp           Stack pointer (callee-saved)
        X3,  // gp           Global pointer (not preserved across calls)
        X4,  // tp           Thread pointer (not preserved across calls)
        X5,  // t0           Temporary register (caller-saved)
        X6,  // t1           Temporary register (caller-saved)
        X7,  // t2           Temporary register (caller-saved)
        X8,  // s0/fp        Saved register/Frame pointer (callee-saved)
        X9,  // s1           Saved register (callee-saved)
        X10, // a0           Function argument/return value (caller-saved)
        X11, // a1           Function argument/return value (caller-saved)
        X12, // a2           Function argument (caller-saved)
        X13, // a3           Function argument (caller-saved)
        X14, // a4           Function argument (caller-saved)
        X15, // a5           Function argument (caller-saved)
        X16, // a6           Function argument (caller-saved)
        X17, // a7           Function argument (caller-saved)
        X18, // s2           Saved register (callee-saved)
        X19, // s3           Saved register (callee-saved)
        X20, // s4           Saved register (callee-saved)
        X21, // s5           Saved register (callee-saved)
        X22, // s6           Saved register (callee-saved)
        X23, // s7           Saved register (callee-saved)
        X24, // s8           Saved register (callee-saved)
        X25, // s9           Saved register (callee-saved)
        X26, // s10          Saved register (callee-saved)
        X27, // s11          Saved register (callee-saved)
        X28, // t3           Temporary register (caller-saved)
        X29, // t4           Temporary register (caller-saved)
        X30, // t5           Temporary register (caller-saved)
        X31, // t6           Temporary register (caller-saved)
    };
    std::string toString(rvREG r); // implement this in ur own way

    enum class rvFREG
    {
        F0,
        F1,
        F2,
        F3,
        F4,
        F5,
        F6,
        F7,
        F8,
        F9,
        F10,
        F11,
        F12,
        F13,
        F14,
        F15,
        F16,
        F17,
        F18,
        F19,
        F20,
        F21,
        F22,
        F23,
        F24,
        F25,
        F26,
        F27,
        F28,
        F29,
        F30,
        F31,
    };
    std::string toString(rvFREG r); // implement this in ur own way

    // rv32i instructions
    // add instruction u need here!
    enum class rvOPCODE
    {
        // RV32I Base Integer Instructions
        ADD,
        SUB,
        XOR,
        OR,
        AND,
        SLL,
        SRL,
        SRA,
        SLT,
        SLTU, // arithmetic & logic
        ADDI,
        XORI,
        ORI,
        ANDI,
        SLLI,
        SRLI,
        SRAI,
        SLTI,
        SLTIU, // immediate
        LW,
        SW, // load & store
        BEQ,
        BNE,
        BLT,
        BGE,
        BLTU,
        BGEU, // conditional branch
        JAL,
        JALR, // jump

        // RV32M Multiply Extension

        // RV32F / D Floating-Point Extensions

        // Pseudo Instructions
        LA,
        LI,
        MOV,
        J, // ...
    };
    std::string toString(rvOPCODE r); // implement this in ur own way

} // namespace rv

#endif