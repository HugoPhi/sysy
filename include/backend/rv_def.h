#ifndef RV_DEF_H
#define RV_DEF_H

#include <string>

namespace rv {

    // rv interger registers
    enum class rvREG {
        /* Xn       ABI name       Purpose & Calling Convention */
        X0,   // zero         Hard-wired zero, immutable
        X1,   // ra           Return address (caller-saved)
        X2,   // sp           Stack pointer (callee-saved)
        X3,   // gp           Global pointer (not preserved across calls)
        X4,   // tp           Thread pointer (not preserved across calls)
        X5,   // t0           Temporary register (caller-saved)
        X6,   // t1           Temporary register (caller-saved)
        X7,   // t2           Temporary register (caller-saved)
        X8,   // s0/fp        Saved register/Frame pointer (callee-saved)
        X9,   // s1           Saved register (callee-saved)
        X10,  // a0           Function argument/return value (caller-saved)
        X11,  // a1           Function argument/return value (caller-saved)
        X12,  // a2           Function argument (caller-saved)
        X13,  // a3           Function argument (caller-saved)
        X14,  // a4           Function argument (caller-saved)
        X15,  // a5           Function argument (caller-saved)
        X16,  // a6           Function argument (caller-saved)
        X17,  // a7           Function argument (caller-saved)
        X18,  // s2           Saved register (callee-saved)
        X19,  // s3           Saved register (callee-saved)
        X20,  // s4           Saved register (callee-saved)
        X21,  // s5           Saved register (callee-saved)
        X22,  // s6           Saved register (callee-saved)
        X23,  // s7           Saved register (callee-saved)
        X24,  // s8           Saved register (callee-saved)
        X25,  // s9           Saved register (callee-saved)
        X26,  // s10          Saved register (callee-saved)
        X27,  // s11          Saved register (callee-saved)
        X28,  // t3           Temporary register (caller-saved)
        X29,  // t4           Temporary register (caller-saved)
        X30,  // t5           Temporary register (caller-saved)
        X31,  // t6           Temporary register (caller-saved)

        // Floating-point registers
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
    std::string toString(rvREG r);  // implement this in ur own way

    // rv32i instructions
    // add instruction u need here!
    enum class rvOPCODE {
        // RV32I Base Instruction Set
        LUI,      // Load Upper Immediate
        AUIPC,    // Add Upper Immediate to PC
        JAL,      // Jump and Link
        JALR,     // Jump and Link Register
        BEQ,      // Branch Equal
        BNE,      // Branch Not Equal
        BLT,      // Branch Less Than
        BGE,      // Branch Greater or Equal
        BLTU,     // Branch Less Than Unsigned
        BGEU,     // Branch Greater or Equal Unsigned
        LB,       // Load Byte
        LH,       // Load Halfword
        LW,       // Load Word
        LBU,      // Load Byte Unsigned
        LHU,      // Load Halfword Unsigned
        SB,       // Store Byte
        SH,       // Store Halfword
        SW,       // Store Word
        ADDI,     // Add Immediate
        SLTI,     // Set Less Than Immediate
        SLTIU,    // Set Less Than Immediate Unsigned
        XORI,     // XOR Immediate
        ORI,      // OR Immediate
        ANDI,     // AND Immediate
        SLLI,     // Shift Left Logical Immediate
        SRLI,     // Shift Right Logical Immediate
        SRAI,     // Shift Right Arithmetic Immediate
        ADD,      // Add
        SUB,      // Subtract
        SLL,      // Shift Left Logical
        SLT,      // Set Less Than
        SLTU,     // Set Less Than Unsigned
        XOR,      // XOR
        SRL,      // Shift Right Logical
        SRA,      // Shift Right Arithmetic
        OR,       // OR
        AND,      // AND
        FENCE,    // Fence
        FENCE_I,  // Fence Instruction
        ECALL,    // Environment Call
        EBREAK,   // Environment Break
        CSRRW,    // CSR Read/Write
        CSRRS,    // CSR Read and Set
        CSRRC,    // CSR Read and Clear
        CSRRWI,   // CSR Read/Write Immediate
        CSRRSI,   // CSR Read and Set Immediate
        CSRRCI,   // CSR Read and Clear Immediate

        // RV32M Standard Extension for Integer Multiply/Divide
        MUL,     // Multiply
        MULH,    // Multiply High Signed
        MULHSU,  // Multiply High Signed-Unsigned
        MULHU,   // Multiply High Unsigned
        DIV,     // Divide Signed
        DIVU,    // Divide Unsigned
        REM,     // Remainder Signed
        REMU,    // Remainder Unsigned

        // RV32F Standard Extension for Single-Precision Floating-Point
        FLW,        // Floating-point Load Word
        FSW,        // Floating-point Store Word
        FMADD_S,    // Fused Multiply-Add Single
        FMSUB_S,    // Fused Multiply-Subtract Single
        FNMSUB_S,   // Fused Negative Multiply-Subtract Single
        FNMADD_S,   // Fused Negative Multiply-Add Single
        FADD_S,     // Floating-point Add Single
        FSUB_S,     // Floating-point Subtract Single
        FMUL_S,     // Floating-point Multiply Single
        FDIV_S,     // Floating-point Divide Single
        FSQRT_S,    // Floating-point Square Root Single
        FSGNJ_S,    // Floating-point Sign Injection Single
        FSGNJN_S,   // Floating-point Sign Injection Negative Single
        FSGNJX_S,   // Floating-point Sign Injection XOR Single
        FMIN_S,     // Floating-point Minimum Single
        FMAX_S,     // Floating-point Maximum Single
        FCVT_W_S,   // Convert Float to Word Single
        FCVT_WU_S,  // Convert Float to Unsigned Word Single
        FCVT_S_W,   // Convert Word to Float Single
        FCVT_S_WU,  // Convert Unsigned Word to Float Single
        FMV_X_S,    // Move Float to Integer Single
        FMV_S_X,    // Move Integer to Float Single
        FEQ_S,      // Floating-point Equal Single
        FLT_S,      // Floating-point Less Than Single
        FLE_S,      // Floating-point Less or Equal Single
        FCLASS_S,   // Floating-point Classify Single

        // RV32D Standard Extension for Double-Precision Floating-Point
        FLD,        // Floating-point Load Double
        FSD,        // Floating-point Store Double
        FMADD_D,    // Fused Multiply-Add Double
        FMSUB_D,    // Fused Multiply-Subtract Double
        FNMSUB_D,   // Fused Negative Multiply-Subtract Double
        FNMADD_D,   // Fused Negative Multiply-Add Double
        FADD_D,     // Floating-point Add Double
        FSUB_D,     // Floating-point Subtract Double
        FMUL_D,     // Floating-point Multiply Double
        FDIV_D,     // Floating-point Divide Double
        FSQRT_D,    // Floating-point Square Root Double
        FSGNJ_D,    // Floating-point Sign Injection Double
        FSGNJN_D,   // Floating-point Sign Injection Negative Double
        FSGNJX_D,   // Floating-point Sign Injection XOR Double
        FMIN_D,     // Floating-point Minimum Double
        FMAX_D,     // Floating-point Maximum Double
        FCVT_S_D,   // Convert Single to Double
        FCVT_D_S,   // Convert Double to Single
        FCVT_W_D,   // Convert Double to Word
        FCVT_WU_D,  // Convert Double to Unsigned Word
        FCVT_D_W,   // Convert Word to Double
        FCVT_D_WU,  // Convert Unsigned Word to Double
        FMV_X_D,    // Move Double to Integer
        FMV_D_X,    // Move Integer to Double
        FEQ_D,      // Floating-point Equal Double
        FLT_D,      // Floating-point Less Than Double
        FLE_D,      // Floating-point Less or Equal Double
        FCLASS_D,   // Floating-point Classify Double

        // Pseudo-instructions
        NOP,      // No Operation (pseudo)
        LI,       // Load Immediate (pseudo)
        LA,       // Load Address (pseudo)
        MV,       // Move (pseudo)
        NOT,      // Bitwise NOT (pseudo)
        NEG,      // Negate (pseudo)
        J,        // Jump (pseudo)
        RET,      // Return (pseudo)
        CALL,     // Call (pseudo)
        TAIL,     // Tail call (pseudo)
        SEQZ,     // Set if Equal to Zero (pseudo)
        SNEZ,     // Set if Not Equal to Zero (pseudo)
        SLTZ,     // Set if Less Than Zero (pseudo)
        SGTZ,     // Set if Greater Than Zero (pseudo)
        FMV_S,    // Floating-point Move Single (pseudo)
        FMV_D,    // Floating-point Move Double (pseudo)
        FRCSR,    // Read FP CSR (pseudo)
        FSCSR,    // Write FP CSR (pseudo)
        FRRM,     // Read FP Rounding Mode (pseudo)
        FSRM,     // Write FP Rounding Mode (pseudo)
        FRFLAGS,  // Read FP Flags (pseudo)
        FSFLAGS,  // Write FP Flags (pseudo)

        // Special value
        UNKNOWN,  // Unknown or invalid opcode
    };
    std::string toString(rvOPCODE r);  // implement this in ur own way

}  // namespace rv

#endif