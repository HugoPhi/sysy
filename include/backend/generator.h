#ifndef GENERARATOR_H
#define GENERARATOR_H

#include <cstdint>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "backend/rv_def.h"
#include "backend/rv_inst_impl.h"
#include "ir/ir.h"

namespace backend {

    // it is a map bewteen variable and its mem addr, the mem addr of a local variable can be identified by ($sp + off)
    struct stackVarMap {
        std::map<std::string, int> _table;  // the map between variable.name and its offset

        /**
         * @brief find the addr of a ir::Operand.name
         * @return the offset
         */
        int find_operand(std::string);

        /**
         * @brief add a ir::Operand.name into current map, alloc space for this variable in memory
         * @param[in] size: the space needed(in byte)
         * @return the offset
         */
        int add_operand(std::string, uint32_t size = 4);
    };

    struct Generator {
        // ABI
        rv::rvREG t[7] = {rv::rvREG::X5, rv::rvREG::X6, rv::rvREG::X7, rv::rvREG::X28, rv::rvREG::X29, rv::rvREG::X30, rv::rvREG::X31};                                                                                    // temporary registers
        rv::rvREG a[8] = {rv::rvREG::X10, rv::rvREG::X11, rv::rvREG::X12, rv::rvREG::X13, rv::rvREG::X14, rv::rvREG::X15, rv::rvREG::X16, rv::rvREG::X17};                                                                 // argument registers
        rv::rvREG s[12] = {rv::rvREG::X8, rv::rvREG::X9, rv::rvREG::X18, rv::rvREG::X19, rv::rvREG::X20, rv::rvREG::X21, rv::rvREG::X22, rv::rvREG::X23, rv::rvREG::X24, rv::rvREG::X25, rv::rvREG::X26, rv::rvREG::X27};  // saved registers
        rv::rvREG sp = rv::rvREG::X2;                                                                                                                                                                                      // stack pointer
        rv::rvREG gp = rv::rvREG::X3;                                                                                                                                                                                      // global pointer
        rv::rvREG tp = rv::rvREG::X4;                                                                                                                                                                                      // thread pointer
        rv::rvREG ra = rv::rvREG::X1;                                                                                                                                                                                      // return address
        rv::rvREG zero = rv::rvREG::X0;                                                                                                                                                                                    // hard-wired zero register

        rv::rvREG ft[12] = {rv::rvREG::F0, rv::rvREG::F1, rv::rvREG::F2, rv::rvREG::F3, rv::rvREG::F4, rv::rvREG::F5, rv::rvREG::F6, rv::rvREG::F7, rv::rvREG::F28, rv::rvREG::F29, rv::rvREG::F30, rv::rvREG::F31};        // temporary floating-point registers
        rv::rvREG fs[12] = {rv::rvREG::F8, rv::rvREG::F9, rv::rvREG::F18, rv::rvREG::F19, rv::rvREG::F20, rv::rvREG::F21, rv::rvREG::F22, rv::rvREG::F23, rv::rvREG::F24, rv::rvREG::F25, rv::rvREG::F26, rv::rvREG::F27};  // saved floating-point registers
        rv::rvREG fa[8] = {rv::rvREG::F10, rv::rvREG::F11, rv::rvREG::F12, rv::rvREG::F13, rv::rvREG::F14, rv::rvREG::F15, rv::rvREG::F16, rv::rvREG::F17};                                                                 // argument floating-point registers

        /**
         * Reg Strategy:
         * - t0, t1, t2 for temporary operations
         * - t3-t6, s* for LRU strategy: 15
         * - a0-a7 for function arguments
         *
         * - ft0, ft1 for temporary operations
         * - ft2-ft11, fs* for LRU strategy: 18
         * - fa0-fa7 for function arguments
         */
        std::vector<rv::rvREG> ilru = {t[4], t[5], t[6], s[0], s[1], s[2], s[3], s[4], s[5], s[6], s[7], s[8], s[9], s[10], s[11]};
        std::vector<rv::rvREG> flru = {ft[2], ft[3], ft[4], ft[5], ft[6], ft[7], ft[8], ft[9], ft[10], ft[11], fs[0], fs[1], fs[2], fs[3], fs[4], fs[5], fs[6], fs[7]};

        const ir::Program &program;           // the program to gen
        std::ofstream &fout;                  // output file
        std::vector<rv::rv_inst> rv_program;  // generated instructions

        Generator(ir::Program &, std::ofstream &);

        // stack assist data structures
        std::vector<std::pair<std::string, rv::rvREG>> var2reg;   // map from variable to register
        std::vector<std::pair<std::string, rv::rvREG>> var2freg;  // map from variable to floating-point register
        stackVarMap var2offset;                                   // map from variable to offset in stack
        std::map<int, std::string> pc2label;                      // map from pc to label
        int stack_data_size = 0;                                  // size of stack data for arrays, used to calculate the offset
        int cnt4ll = 0;                                           // local label counter
        int ir4pc = 0;                                            // pc 4 ir

        rv::rvREG get_reg_from_var(ir::Operand &op);

        void realloc_stack_frame(std::set<ir::Operand> &vars);
        void read(ir::Operand &op, rv::rvREG reg);
        void write(ir::Operand &op, rv::rvREG reg);

        int get_stack_size() const;

        // generate wrapper function
        void gen();
        void gen_func(const ir::Function &);
        void gen_instr(const ir::Instruction &);

        // assist functions
        void draw(std::vector<rv::rv_inst> &, std::ofstream &) const;
        bool is_global(const std::string &name) const;
        rv::rv_inst make_rv_inst(rv::rvREG rd, rv::rvREG rs1, rv::rvREG rs2, rv::rvOPCODE op, int imm = 0, const std::string &label = "", int stack_size_sign = 0);
        std::string float2ieee(std::string &fval) const;

        // assist generation functions
        // int instructions
        void gen_mov(const ir::Instruction &inst);
        void gen_add(const ir::Instruction &inst);
        void gen_sub(const ir::Instruction &inst);
        void gen_mul(const ir::Instruction &inst);
        void gen_div(const ir::Instruction &inst);
        void gen_mod(const ir::Instruction &inst);
        void gen_lss(const ir::Instruction &inst);
        void gen_leq(const ir::Instruction &inst);
        void gen_gtr(const ir::Instruction &inst);
        void gen_geq(const ir::Instruction &inst);
        void gen_eq(const ir::Instruction &inst);
        void gen_neq(const ir::Instruction &inst);
        void gen_or(const ir::Instruction &inst);
        void gen_and(const ir::Instruction &inst);
        void gen_not(const ir::Instruction &inst);

        // float instructions
        void gen_fmov(const ir::Instruction &inst);
        void gen_fadd(const ir::Instruction &inst);
        void gen_fsub(const ir::Instruction &inst);
        void gen_fmul(const ir::Instruction &inst);
        void gen_fdiv(const ir::Instruction &inst);
        void gen_flss(const ir::Instruction &inst);
        void gen_fleq(const ir::Instruction &inst);
        void gen_fgtr(const ir::Instruction &inst);
        void gen_fgeq(const ir::Instruction &inst);
        void gen_feq(const ir::Instruction &inst);
        void gen_fneq(const ir::Instruction &inst);

        // conversion instructions
        void gen_cvt_i2f(const ir::Instruction &inst);
        void gen_cvt_f2i(const ir::Instruction &inst);

        // control flow instructions
        void gen_return(const ir::Instruction &inst);
        void gen_goto(const ir::Instruction &inst);

        // function instructions
        void gen_call(const ir::Instruction &inst);

        // memory instructions
        void gen_alloc(const ir::Instruction &inst);
        void gen_store(const ir::Instruction &inst);
        void gen_load(const ir::Instruction &inst);
        void gen_getptr(const ir::Instruction &inst);
    };

}  // namespace backend

#endif