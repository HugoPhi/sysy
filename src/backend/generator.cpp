#include "backend/generator.h"

#include <assert.h>

#include <iostream>

#define TODO assert(0 && "todo")
#define NOPIC() fout << "\t.option nopic\n"
#define TEXT() fout << "\t.text\n"
#define DATA() fout << "\t.data\n"
#define BSS() fout << "\t.bss\n"
#define GLOBAL(name) fout << "\t.globl\t" << name << "\n"
#define TYPE(name, type) fout << "\t.type\t" << name << ", " << type << "\n"
#define SIZE(name, size) fout << "\t.size\t" << name << ", " << size << "\n"
#define ALIGN(size) fout << "\t.align\t" << size << "\n"
#define SPACE(size) fout << "\t.space\t" << size << "\n"
#define WORD(value) fout << "\t.word\t" << value << "\n"
#define LABEL(name) fout << name << ":\n"

namespace ir {
    bool operator<(const ir::Operand &lhs, const ir::Operand &rhs) { return lhs.name < rhs.name || (lhs.name == rhs.name && lhs.type < rhs.type); }
}  // namespace ir

backend::Generator::Generator(ir::Program &p, std::ofstream &f) : program(p), fout(f) {}

int backend::stackVarMap::add_operand(std::string var, uint32_t size) {
    if (_table.find(var) != _table.end())  // already in the map
        return _table[var];
    int offset = 4 * (_table.size() + size);  // alloc space for this variable
    _table[var] = offset;
    return offset;
}

int backend::stackVarMap::find_operand(std::string var) {
    if (_table.find(var) == _table.end()) {
        return -1;  // not found
    }
    return _table[var];
}

void backend::Generator::write(ir::Operand &op, rv::rvREG reg) {
    // write register into operand
    if (op.type == ir::Type::Int) {
        int offset = var2offset.find_operand(op.name);                                  // find the offset of this operand
        if (offset != -1) {                                                             // if the operand is in stack, store the register into the stack
            rv_program.push_back(make_rv_inst({}, sp, reg, rv::rvOPCODE::SW, offset));  // sw reg, offset(sp)
        }
    } else if (op.type == ir::Type::Float) {
        int offset = var2offset.find_operand(op.name);                                   // find the offset of this operand
        if (offset != -1) {                                                              // if the operand is in stack, store the floating-point register into the stack
            rv_program.push_back(make_rv_inst({}, sp, reg, rv::rvOPCODE::FSW, offset));  // fsw freg, offset(sp)
        }
    }
}

void backend::Generator::read(ir::Operand &op, rv::rvREG reg) {
    // read operand into register
    if (op.type == ir::Type::Int) {
        int offset = var2offset.find_operand(op.name);                                  // find the offset of this operand
        if (offset != -1) {                                                             // if the operand is in stack, load it into the register
            rv_program.push_back(make_rv_inst(reg, sp, {}, rv::rvOPCODE::LW, offset));  // lw reg, offset(sp)
        }
    } else if (op.type == ir::Type::Float) {
        int offset = var2offset.find_operand(op.name);                                   // find the offset of this operand
        if (offset != -1) {                                                              // if the operand is in stack, load it into the floating-point register
            rv_program.push_back(make_rv_inst(reg, sp, {}, rv::rvOPCODE::FLW, offset));  // flw freg, offset(sp)
        }
    }
}

rv::rvREG backend::Generator::get_reg_from_var(ir::Operand &op) {
    // LRU strategy for registers
    if (op.type == ir::Type::Int || op.type == ir::Type::IntPtr || op.type == ir::Type::FloatPtr) {
        for (auto it = var2reg.begin(); it != var2reg.end(); ++it) {
            if (it->first == op.name) {
                auto pair = *it;
                var2reg.erase(it);
                var2reg.insert(var2reg.begin(), pair);
                return pair.second;
            }
        }

        // not found, allocate a new register
        auto [var, reg] = var2reg.back();  // get the least recently used register
        if (var != "") {
            auto tmp = ir::Operand(var, ir::Type::Int);  // update the operand with the variable name
            write(tmp, reg);                             // write the register for this operand
        }
        var2reg.pop_back();                                             // remove the least recently used register
        var2reg.insert(var2reg.begin(), std::make_pair(op.name, reg));  // add the new register for this operand
        read(op, reg);                                                  // read the register for this operand again
        return reg;                                                     // return the new register
    } else {
        for (auto it = var2freg.begin(); it != var2freg.end(); ++it) {
            if (it->first == op.name) {
                auto pair = *it;
                var2freg.erase(it);
                var2freg.insert(var2freg.begin(), pair);
                return pair.second;
            }
        }
        // not found, allocate a new floating-point register
        auto [var, freg] = var2freg.back();  // get the least recently used floating-point register
        if (var != "") {
            auto tmp = ir::Operand(var, ir::Type::Float);  // update the operand with the variable name
            write(tmp, freg);                              // write the floating-point register for this operand
        }
        var2freg.pop_back();                                               // remove the least recently used floating-point register
        var2freg.insert(var2freg.begin(), std::make_pair(op.name, freg));  // add the new floating-point register for this operand
        read(op, freg);                                                    // read the floating-point register for this operand again
        return freg;                                                       // return the new floating-point register
    }
}

int backend::Generator::get_stack_size() const {
    // return the size of stack needed for this function
    return (var2offset._table.size() + stack_data_size) * 4;  // each variable takes 4 bytes
}

rv::rv_inst backend::Generator::make_rv_inst(rv::rvREG rd, rv::rvREG rs1, rv::rvREG rs2, rv::rvOPCODE op, int imm, const std::string &label, int stack_size_sign) { return {rd, rs1, rs2, op, imm, label, stack_size_sign}; }

void backend::Generator::realloc_stack_frame(std::set<ir::Operand> &vars) {
    std::vector<ir::Operand> vars_vec(vars.begin(), vars.end());  // convert unordered_set to vector

    rv_program.clear();  // clear the previous instructions
    var2freg = std::vector<std::pair<std::string, rv::rvREG>>();
    var2reg = std::vector<std::pair<std::string, rv::rvREG>>();
    pc2label = std::map<int, std::string>();
    var2offset = stackVarMap();
    stack_data_size = 0;

    // fill available int registers
    int i1 = 0, i2 = 0;  // i1 for int registers, i2 for floating-point registers
    // for (int i = 0; i < vars_vec.size(); ++i) {
    //     if (vars_vec[i].type == ir::Type::Int || vars_vec[i].type == ir::Type::IntPtr || vars_vec[i].type == ir::Type::FloatPtr) {
    //         if (i1 >= ilru.size()) {
    //             continue;
    //         }
    //         var2reg.push_back(std::make_pair(vars_vec[i].name, ilru[i1++]));  // map variable to register
    //     } else if (vars_vec[i].type == ir::Type::Float) {
    //         if (i2 >= flru.size()) {
    //             continue;
    //         }
    //         var2freg.push_back(std::make_pair(vars_vec[i].name, flru[i2++]));  // map variable to floating-point register
    //     }
    // }

    while (i1 < ilru.size()) {
        var2reg.push_back(std::make_pair("", ilru[i1++]));  // fill the rest with "null"
    }
    while (i2 < flru.size()) {
        var2freg.push_back(std::make_pair("", flru[i2++]));  // fill the rest with "null"
    }

    // allocate stack space for variables
    for (const auto &var : vars_vec) {
        var2offset.add_operand(var.name);  // add the variable to the stack map
    }
}

void backend::Generator::gen() {
    NOPIC();
    // generate global variables
    DATA();
    const ir::Function &global_func = program.functions.back();  // get global

    for (const ir::Instruction *instr : global_func.InstVec) {
        auto [op1, op2, des, op] = *instr;  // structured binding to get des, op1, op2, op
        if (op == ir::Operator::mov || op == ir::Operator::fmov) {
            GLOBAL(des.name);
            TYPE(des.name, "@object");
            SIZE(des.name, 4);  // reserve 4 bytes for int or float
            ALIGN(2);
            LABEL(des.name);

            if (des.type == ir::Type::Int) {
                WORD(op1.name);
            } else if (des.type == ir::Type::Float) {
                std::string float_val = float2ieee(op1.name);  // convert float to IEEE 754 format
                WORD(float_val);                               // store the float literal in IEEE 754 format
            }
        }
    }

    std::map<std::string, ir::GlobalVal> global_arrays;
    for (const ir::GlobalVal &array_globalval : program.globalVal) {
        if (array_globalval.val.type == ir::Type::IntPtr || array_globalval.val.type == ir::Type::FloatPtr) {
            global_arrays.emplace(array_globalval.val.name, array_globalval);
        }
    }

    for (const ir::Instruction *instr : global_func.InstVec) {
        auto [op1, op2, des, op] = *instr;  // structured binding to get des, op1, op2, op
        if (op == ir::Operator::store) {
            if (global_arrays.count(op1.name)) {
                auto &array_globalval = global_arrays.find(op1.name)->second;
                int arr_len = array_globalval.maxlen;
                GLOBAL(op1.name);
                TYPE(op1.name, "@object");
                SIZE(op1.name, arr_len * 4);  // reserve space for the array
                ALIGN(2);                     // align to 4 bytes
                LABEL(op1.name);
                global_arrays.erase(op1.name);
            }

            if (des.type == ir::Type::IntLiteral) {
                WORD(des.name);  // store int literal
            } else if (des.type == ir::Type::FloatLiteral) {
                std::string float_val = float2ieee(des.name);  // convert float to IEEE 754 format
                WORD(float_val);                               // store the float literal in IEEE 754 format
            }
        }
    }

    if (global_arrays.size() > 0) {
        BSS();
        for (const auto &array_globalval : global_arrays) {
            GLOBAL(array_globalval.first);
            TYPE(array_globalval.first, "@object");
            SIZE(array_globalval.first, array_globalval.second.maxlen * 4);  // reserve space for the array
            ALIGN(2);                                                        // align to 4 bytes
            LABEL(array_globalval.first);
            SPACE(array_globalval.second.maxlen * 4);  // reserve space for the array
        }
    }

    // get into .text
    TEXT();

    // generate functions
    for (const auto &f : program.functions) {
        if (f.name == "global")
            continue;  // skip the global function
        gen_func(f);
    }
}

bool backend::Generator::is_global(const std::string &name) const {
    // check if the name is a global variable
    for (const ir::GlobalVal &global : program.globalVal) {
        if (global.val.name == name) {
            return true;
        }
    }
    return false;
}

void backend::Generator::gen_func(const ir::Function &func) {
    GLOBAL(func.name);  // declare the function as global
    TYPE(func.name, "@function");
    // prologue
    LABEL(func.name);

    // collect vars
    std::set<ir::Operand> vars;
    for (auto param : func.ParameterList) {
        vars.insert(param);  // collect the parameters
    }
    for (const ir::Instruction *instr : func.InstVec) {
        auto [op1, op2, des, op] = *instr;  // structured binding to get des, op1, op2, op
        if (!is_global(op1.name) && op1.type != ir::Type::null && op1.type != ir::Type::IntLiteral && op1.type != ir::Type::FloatLiteral) {
            vars.insert(op1);  // collect the variables
        }
        if (!is_global(op2.name) && op2.type != ir::Type::null && op2.type != ir::Type::IntLiteral && op2.type != ir::Type::FloatLiteral) {
            vars.insert(op2);  // collect the variables
        }
        if (!is_global(des.name) && des.type != ir::Type::null && des.type != ir::Type::IntLiteral && des.type != ir::Type::FloatLiteral) {
            vars.insert(des);  // collect the variables
        }
    }

    realloc_stack_frame(vars);  // reallocate the stack frame for this function

    stack_data_size = ilru.size() + flru.size();  // calculate the stack data size, each register takes 4 bytes

    // scan goto labels
    for (const ir::Instruction *instr : func.InstVec) {
        if (instr->op == ir::Operator::_goto) {
            auto ix = std::stoi(instr->des.name);
            if (pc2label.find(ir4pc + ix) == pc2label.end()) {
                pc2label[ir4pc + ix] = ".L" + std::to_string(cnt4ll++);  // add the label to the map, .L{cnt4ll} is a local label
            }
        }
        ir4pc++;
    }

    // generate function body
    ir4pc = 0;  // reset instruction pointer
    for (const ir::Instruction *instr : func.InstVec) {
        if (pc2label.find(ir4pc) != pc2label.end()) {
            // LABEL(pc2label[ir4pc]);  // generate label if it exists
            rv_program.push_back(make_rv_inst({}, {}, {}, rv::rvOPCODE::NOP, 0, pc2label[ir4pc], 0));  // add a NOP instruction with the label
        }

        gen_instr(*instr);
        ir4pc++;
    }

    // save registers to stack
    // save funcParameterList to stack
    int i1 = 0, i2 = 0;  // i1 for int params, i2 for float params
    for (auto param : func.ParameterList) {
        if (param.type == ir::Type::Int || param.type == ir::Type::IntPtr || param.type == ir::Type::FloatPtr) {
            if (i1 <= 7) {
                rv_program.insert(rv_program.begin(), make_rv_inst({}, sp, a[i1++], rv::rvOPCODE::SW, var2offset.find_operand(param.name)));  // store the parameter to stack
            } else {
                // load from stack & store to stack
                int tmp = i2 > 7 ? i2 - 8 : 0;
                rv_program.insert(rv_program.begin(), make_rv_inst(t[0], sp, {}, rv::rvOPCODE::LW, (i1 - 8 + tmp) * 4, "", 1));            // lw t[0], 4*(i1-8+tmp+finale_stack_size)(sp)
                rv_program.insert(rv_program.begin(), make_rv_inst({}, sp, t[0], rv::rvOPCODE::SW, var2offset.find_operand(param.name)));  // sw t[0], offset(sp)
                i1++;
            }
        } else if (param.type == ir::Type::Float) {
            if (i2 <= 7) {
                rv_program.insert(rv_program.begin(), make_rv_inst({}, sp, a[i2++], rv::rvOPCODE::FSW, var2offset.find_operand(param.name)));  // fsw a[i2], offset(sp)
            } else {
                // load from stack & store to stack
                int tmp = i1 > 7 ? i1 - 8 : 0;
                rv_program.insert(rv_program.begin(), make_rv_inst(ft[0], sp, {}, rv::rvOPCODE::FLW, (i2 - 8 + tmp) * 4, "", 1));            // flw ft[0], 4*(i2-8+tmp+finale_stack_size)(sp)
                rv_program.insert(rv_program.begin(), make_rv_inst({}, sp, ft[0], rv::rvOPCODE::FSW, var2offset.find_operand(param.name)));  // fsw ft[0], offset(sp)
                i2++;
            }
        }
    }

    // save LRU registers to stack
    // int offset = var2offset._table.size();  // calculate the offset for the stack
    // for (auto reg : ilru) {
    //     rv_program.insert(rv_program.begin(), make_rv_inst({}, sp, reg, rv::rvOPCODE::SW, 4 * offset++));  // sw reg, 4*offset(sp)
    // }
    // for (auto freg : flru) {
    //     rv_program.insert(rv_program.begin(), make_rv_inst({}, sp, freg, rv::rvOPCODE::FSW, 4 * offset++));  // fsw freg, 4*offset(sp)
    // }
    // stack_data_size = offset * 4;  // update the stack data size
    int offset = var2offset._table.size();  // calculate the offset for the stack
    for (auto &pair : var2reg) {
        if (pair.first != "") {                                                                                        // if the register is not empty
            rv_program.insert(rv_program.begin(), make_rv_inst({}, sp, pair.second, rv::rvOPCODE::SW, 4 * offset++));  // sw reg, offset(sp)
        }
    }

    for (auto &pair : var2freg) {
        if (pair.first != "") {                                                                                         // if the floating-point register is not empty
            rv_program.insert(rv_program.begin(), make_rv_inst({}, sp, pair.second, rv::rvOPCODE::FSW, 4 * offset++));  // fsw freg, offset(sp)
        }
    }

    // epilogue
    // restore LRU registers from stack
    // int offset = var2offset._table.size();  // calculate the offset for the stack
    // for (auto reg : ilru) {
    //     rv_program.push_back(make_rv_inst(reg, sp, {}, rv::rvOPCODE::LW, 4 * offset++));  // lw reg, 4*offset(sp)
    // }
    // for (auto freg : flru) {
    //     rv_program.push_back(make_rv_inst(freg, sp, {}, rv::rvOPCODE::FLW, 4 * offset++));  // flw freg, 4*offset(sp)
    // }
    offset = var2offset._table.size();  // calculate the offset for the stack
    for (auto &pair : var2reg) {
        if (pair.first != "") {                                                                       // if the register is not empty
            rv_program.push_back(make_rv_inst(pair.second, sp, {}, rv::rvOPCODE::LW, 4 * offset++));  // lw reg, offset(sp)
        }
    }
    for (auto &pair : var2freg) {
        if (pair.first != "") {                                                                        // if the floating-point register is not empty
            rv_program.push_back(make_rv_inst(pair.second, sp, {}, rv::rvOPCODE::FLW, 4 * offset++));  // flw freg, offset(sp)
        }
    }

    int stack_size = get_stack_size();                                                                 // get the size of the stack
    rv_program.insert(rv_program.begin(), make_rv_inst(sp, sp, {}, rv::rvOPCODE::ADDI, -stack_size));  // addi sp, sp, -stack_size
    rv_program.push_back(make_rv_inst(sp, sp, {}, rv::rvOPCODE::ADDI, stack_size));                    // addi sp, sp, stack_size (restore stack pointer)
    rv_program.push_back(make_rv_inst({}, {}, {}, rv::rvOPCODE::RET));                                 // jalr ra (return to caller)

    draw(rv_program, fout);  // draw the instructions to the output file

    // set the size of the function
    SIZE(func.name, ".-" + func.name);
    ALIGN(1);  // align to 2 bytes
}

void backend::Generator::draw(std::vector<rv::rv_inst> &rv_program, std::ofstream &fout) const {
    for (auto &inst : rv_program) {
        // 根据指令类型选择输出格式
        switch (inst.op) {
        // 1. 无操作数指令
        case rv::rvOPCODE::NOP:
            LABEL(inst.label);
            break;
        case rv::rvOPCODE::ECALL:
        case rv::rvOPCODE::EBREAK:
        case rv::rvOPCODE::FENCE:
        case rv::rvOPCODE::FENCE_I:
        case rv::rvOPCODE::RET:
            fout << "\t" << toString(inst.op);
            break;

        // 2. 单操作数指令（立即数）
        case rv::rvOPCODE::LUI:
        case rv::rvOPCODE::AUIPC:
        case rv::rvOPCODE::LI:
            fout << "\t" << toString(inst.op) << "\t" << toString(inst.rd) << ", " << inst.imm;
            break;

        // 3. 单操作数指令（标签）
        case rv::rvOPCODE::J:
        case rv::rvOPCODE::CALL:
        case rv::rvOPCODE::TAIL:
            fout << "\t" << toString(inst.op) << "\t" << inst.label;
            break;

        // 4. 双操作数指令（寄存器）
        case rv::rvOPCODE::MV:
        case rv::rvOPCODE::NOT:
        case rv::rvOPCODE::NEG:
        case rv::rvOPCODE::FMV_S:
        case rv::rvOPCODE::FMV_D:
        case rv::rvOPCODE::SEQZ:
        case rv::rvOPCODE::SNEZ:
        case rv::rvOPCODE::SLTZ:
        case rv::rvOPCODE::SGTZ:
        case rv::rvOPCODE::FSQRT_S:
        case rv::rvOPCODE::FSQRT_D:
        case rv::rvOPCODE::FCLASS_S:
        case rv::rvOPCODE::FCLASS_D:
        case rv::rvOPCODE::FMV_X_S:
        case rv::rvOPCODE::FMV_S_X:
        case rv::rvOPCODE::FMV_X_D:
        case rv::rvOPCODE::FMV_D_X:
            fout << "\t" << toString(inst.op) << "\t" << toString(inst.rd) << ", " << toString(inst.rs1);
            break;

        // 5. 双操作数指令（寄存器+立即数）
        case rv::rvOPCODE::ADDI:
        case rv::rvOPCODE::SLTI:
        case rv::rvOPCODE::SLTIU:
        case rv::rvOPCODE::XORI:
        case rv::rvOPCODE::ORI:
        case rv::rvOPCODE::ANDI:
        case rv::rvOPCODE::SLLI:
        case rv::rvOPCODE::SRLI:
        case rv::rvOPCODE::SRAI:
        case rv::rvOPCODE::JALR:
            fout << "\t" << toString(inst.op) << "\t" << toString(inst.rd) << ", " << toString(inst.rs1) << ", " << inst.imm;
            break;

        // 6. 加载指令（偏移量格式）
        case rv::rvOPCODE::LA:
        case rv::rvOPCODE::LB:
        case rv::rvOPCODE::LH:
        case rv::rvOPCODE::LW:
        case rv::rvOPCODE::LBU:
        case rv::rvOPCODE::LHU:
        case rv::rvOPCODE::FLW:
        case rv::rvOPCODE::FLD:
            fout << "\t" << toString(inst.op) << "\t" << toString(inst.rd) << ", " << inst.imm << "(" << toString(inst.rs1) << ")";
            break;

        // 7. 存储指令（偏移量格式）
        case rv::rvOPCODE::SB:
        case rv::rvOPCODE::SH:
        case rv::rvOPCODE::SW:
        case rv::rvOPCODE::FSW:
        case rv::rvOPCODE::FSD:
            fout << "\t" << toString(inst.op) << "\t" << toString(inst.rs2) << ", " << inst.imm << "(" << toString(inst.rs1) << ")";
            break;

        // 8. 分支指令（标签目标）
        case rv::rvOPCODE::BEQ:
        case rv::rvOPCODE::BNE:
        case rv::rvOPCODE::BLT:
        case rv::rvOPCODE::BGE:
        case rv::rvOPCODE::BLTU:
        case rv::rvOPCODE::BGEU:
            fout << "\t" << toString(inst.op) << "\t" << toString(inst.rs1) << ", " << toString(inst.rs2) << ", " << inst.label;
            break;

        // 9. 跳转链接指令（标签目标）
        case rv::rvOPCODE::JAL:
            fout << "\t" << toString(inst.op) << "\t" << toString(inst.rd) << ", " << inst.label;
            break;

        // 10. 三操作数指令（寄存器）
        case rv::rvOPCODE::ADD:
        case rv::rvOPCODE::SUB:
        case rv::rvOPCODE::SLL:
        case rv::rvOPCODE::SLT:
        case rv::rvOPCODE::SLTU:
        case rv::rvOPCODE::XOR:
        case rv::rvOPCODE::SRL:
        case rv::rvOPCODE::SRA:
        case rv::rvOPCODE::OR:
        case rv::rvOPCODE::AND:
        case rv::rvOPCODE::MUL:
        case rv::rvOPCODE::MULH:
        case rv::rvOPCODE::MULHSU:
        case rv::rvOPCODE::MULHU:
        case rv::rvOPCODE::DIV:
        case rv::rvOPCODE::DIVU:
        case rv::rvOPCODE::REM:
        case rv::rvOPCODE::REMU:
        case rv::rvOPCODE::FADD_S:
        case rv::rvOPCODE::FSUB_S:
        case rv::rvOPCODE::FMUL_S:
        case rv::rvOPCODE::FDIV_S:
        case rv::rvOPCODE::FSGNJ_S:
        case rv::rvOPCODE::FSGNJN_S:
        case rv::rvOPCODE::FSGNJX_S:
        case rv::rvOPCODE::FMIN_S:
        case rv::rvOPCODE::FMAX_S:
        case rv::rvOPCODE::FADD_D:
        case rv::rvOPCODE::FSUB_D:
        case rv::rvOPCODE::FMUL_D:
        case rv::rvOPCODE::FDIV_D:
        case rv::rvOPCODE::FSGNJ_D:
        case rv::rvOPCODE::FSGNJN_D:
        case rv::rvOPCODE::FSGNJX_D:
        case rv::rvOPCODE::FMIN_D:
        case rv::rvOPCODE::FMAX_D:
        case rv::rvOPCODE::FEQ_S:
        case rv::rvOPCODE::FLT_S:
        case rv::rvOPCODE::FLE_S:
        case rv::rvOPCODE::FEQ_D:
        case rv::rvOPCODE::FLT_D:
        case rv::rvOPCODE::FLE_D:
            fout << "\t" << toString(inst.op) << "\t" << toString(inst.rd) << ", " << toString(inst.rs1) << ", " << toString(inst.rs2);
            break;

        // 11. CSR指令
        case rv::rvOPCODE::CSRRW:
        case rv::rvOPCODE::CSRRS:
        case rv::rvOPCODE::CSRRC:
            fout << "\t" << toString(inst.op) << "\t" << toString(inst.rd) << ", " << inst.imm << ", " << toString(inst.rs1);
            break;

        // 12. 默认情况（未知指令）
        default:
            fout << "\t# UNIMPLEMENTED OPCODE: " << toString(inst.op);
            break;
        }
        fout << "\n";
    }
}

void backend::Generator::gen_instr(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;  // structured binding to get des, op1, op2, op

    switch (op) {
    // int instructions
    case ir::Operator::mov:
        gen_mov(inst);
        break;
    case ir::Operator::add:
        gen_add(inst);
        break;
    case ir::Operator::sub:
        gen_sub(inst);
        break;
    case ir::Operator::mul:
        gen_mul(inst);
        break;
    case ir::Operator::div:
        gen_div(inst);
        break;
    case ir::Operator::mod:
        gen_mod(inst);
        break;
    case ir::Operator::lss:
        gen_lss(inst);
        break;
    case ir::Operator::leq:
        gen_leq(inst);
        break;
    case ir::Operator::gtr:
        gen_gtr(inst);
        break;
    case ir::Operator::geq:
        gen_geq(inst);
        break;
    case ir::Operator::eq:
        gen_eq(inst);
        break;
    case ir::Operator::neq:
        gen_neq(inst);
        break;
    case ir::Operator::_or:
        gen_or(inst);
        break;
    case ir::Operator::_and:
        gen_and(inst);
        break;
    case ir::Operator::_not:
        gen_not(inst);
        break;

    // float instructions
    case ir::Operator::fmov:
        gen_fmov(inst);
        break;
    case ir::Operator::fadd:
        gen_fadd(inst);
        break;
    case ir::Operator::fsub:
        gen_fsub(inst);
        break;
    case ir::Operator::fmul:
        gen_fmul(inst);
        break;
    case ir::Operator::fdiv:
        gen_fdiv(inst);
        break;
    case ir::Operator::flss:
        gen_flss(inst);
        break;
    case ir::Operator::fleq:
        gen_fleq(inst);
        break;
    case ir::Operator::fgtr:
        gen_fgtr(inst);
        break;
    case ir::Operator::fgeq:
        gen_fgeq(inst);
        break;
    case ir::Operator::feq:
        gen_feq(inst);
        break;
    case ir::Operator::fneq:
        gen_fneq(inst);
        break;

    // conversion instructions
    case ir::Operator::cvt_i2f:
        gen_cvt_i2f(inst);
        break;
    case ir::Operator::cvt_f2i:
        gen_cvt_f2i(inst);
        break;
    // control flow instructions
    case ir::Operator::_return:
        gen_return(inst);
        break;
    case ir::Operator::_goto:
        gen_goto(inst);
        break;

    // function instructions
    case ir::Operator::call:
        gen_call(inst);
        break;

    // memory instructions
    case ir::Operator::alloc:
        gen_alloc(inst);
        break;
    case ir::Operator::store:
        gen_store(inst);
        break;
    case ir::Operator::load:
        gen_load(inst);
        break;
    case ir::Operator::getptr:
        gen_getptr(inst);
        break;
    default:
        assert(false && "unknown ir");
    }
}

std::string backend::Generator::float2ieee(std::string &float_str) const {
    float value = std::stof(float_str);
    uint32_t bits;
    memcpy(&bits, &value, sizeof(float));
    return std::to_string(bits);
}

// int instructions
void backend::Generator::gen_mov(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;  // structured binding to get des, op1, op2, op
    if (des.type == ir::Type::Int || des.type == ir::Type::IntPtr || des.type == ir::Type::FloatPtr) {
        rv::rvREG reg_des = get_reg_from_var(des);
        if (is_global(op1.name)) {
            rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LA, 0, op1.name));  // la t[0], "op1.name"
            rv_program.push_back(make_rv_inst(reg_des, t[0], {}, rv::rvOPCODE::LW, 0));       // lw reg_des, 0(t[0])
        } else if (op1.type == ir::Type::IntLiteral) {
            // if op1 is an int literal, write it directly to the register
            rv_program.push_back(make_rv_inst(reg_des, {}, {}, rv::rvOPCODE::LI, std::stoi(op1.name)));  // load immediate value into register
        } else {
            rv::rvREG reg_op1 = get_reg_from_var(op1);                                   // get the register for op1
            read(op1, reg_op1);                                                          // read the value of op1 into the register
            rv_program.push_back(make_rv_inst(reg_des, reg_op1, {}, rv::rvOPCODE::MV));  // move the value from op1 to des
        }
    } else {
        assert(false && "Unsupported type for mov operation");  // unsupported type
    }
}

void backend::Generator::gen_add(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;  // structured binding to get des, op1, op2, op
    if (des.type == ir::Type::Int) {
        auto des_reg = get_reg_from_var(des);                         // get the register for des
        bool op1_is_global = is_global(op1.name);                     // check if op1 is a global variable
        bool op2_is_global = is_global(op2.name);                     // check if op2 is a global variable
        auto op1_reg = op1_is_global ? t[0] : get_reg_from_var(op1);  // get the register for op1
        auto op2_reg = op2_is_global ? t[1] : get_reg_from_var(op2);  // get the register for op2

        if (op1_is_global) {
            rv_program.push_back(make_rv_inst(op1_reg, {}, {}, rv::rvOPCODE::LA, 0, op1.name));  // load address of global variable into op1_reg
            rv_program.push_back(make_rv_inst(op1_reg, op1_reg, {}, rv::rvOPCODE::LW, 0));       // load the value from the address into op1_reg
        }
        if (op2_is_global) {
            rv_program.push_back(make_rv_inst(op2_reg, {}, {}, rv::rvOPCODE::LA, 0, op2.name));  // load address of global variable into op2_reg
            rv_program.push_back(make_rv_inst(op2_reg, op2_reg, {}, rv::rvOPCODE::LW, 0));       // load the value from the address into op2_reg
        }

        rv_program.push_back(make_rv_inst(des_reg, op1_reg, op2_reg, rv::rvOPCODE::ADD));  // add op1 and op2, store result in des
    } else {
        assert(false && "Unsupported type for addition");  // unsupported type
    }
}

void backend::Generator::gen_sub(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;  // structured binding to get des, op1, op2, op
    if (des.type == ir::Type::Int) {
        auto des_reg = get_reg_from_var(des);                         // get the register for des
        bool op1_is_global = is_global(op1.name);                     // check if op1 is a global variable
        bool op2_is_global = is_global(op2.name);                     // check if op2 is a global variable
        auto op1_reg = op1_is_global ? t[0] : get_reg_from_var(op1);  // get the register for op1
        auto op2_reg = op2_is_global ? t[1] : get_reg_from_var(op2);  // get the register for op2

        if (op1_is_global) {
            rv_program.push_back(make_rv_inst(op1_reg, {}, {}, rv::rvOPCODE::LA, 0, op1.name));  // load address of global variable into op1_reg
            rv_program.push_back(make_rv_inst(op1_reg, op1_reg, {}, rv::rvOPCODE::LW, 0));       // load the value from the address into op1_reg
        }
        if (op2_is_global) {
            rv_program.push_back(make_rv_inst(op2_reg, {}, {}, rv::rvOPCODE::LA, 0, op2.name));  // load address of global variable into op2_reg
            rv_program.push_back(make_rv_inst(op2_reg, op2_reg, {}, rv::rvOPCODE::LW, 0));       // load the value from the address into op2_reg
        }

        rv_program.push_back(make_rv_inst(des_reg, op1_reg, op2_reg, rv::rvOPCODE::SUB));  // subtract op2 from op1, store result in des
    } else {
        assert(false && "Unsupported type for subtraction");  // unsupported type
    }
}

void backend::Generator::gen_mul(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;  // structured binding to get des, op1, op2, op
    if (des.type == ir::Type::Int) {
        auto des_reg = get_reg_from_var(des);                         // get the register for des
        bool op1_is_global = is_global(op1.name);                     // check if op1 is a global variable
        bool op2_is_global = is_global(op2.name);                     // check if op2 is a global variable
        auto op1_reg = op1_is_global ? t[0] : get_reg_from_var(op1);  // get the register for op1
        auto op2_reg = op2_is_global ? t[1] : get_reg_from_var(op2);  // get the register for op2

        if (op1_is_global) {
            rv_program.push_back(make_rv_inst(op1_reg, {}, {}, rv::rvOPCODE::LA, 0, op1.name));  // load address of global variable into op1_reg
            rv_program.push_back(make_rv_inst(op1_reg, op1_reg, {}, rv::rvOPCODE::LW, 0));       // load the value from the address into op1_reg
        }
        if (op2_is_global) {
            rv_program.push_back(make_rv_inst(op2_reg, {}, {}, rv::rvOPCODE::LA, 0, op2.name));  // load address of global variable into op2_reg
            rv_program.push_back(make_rv_inst(op2_reg, op2_reg, {}, rv::rvOPCODE::LW, 0));       // load the value from the address into op2_reg
        }

        rv_program.push_back(make_rv_inst(des_reg, op1_reg, op2_reg, rv::rvOPCODE::MUL));  // multiply op1 and op2, store result in des
    } else {
        assert(false && "Unsupported type for multiplication");  // unsupported type
    }
}
void backend::Generator::gen_div(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;  // structured binding to get des, op1, op2, op
    if (des.type == ir::Type::Int) {
        auto des_reg = get_reg_from_var(des);                         // get the register for des
        bool op1_is_global = is_global(op1.name);                     // check if op1 is a global variable
        bool op2_is_global = is_global(op2.name);                     // check if op2 is a global variable
        auto op1_reg = op1_is_global ? t[0] : get_reg_from_var(op1);  // get the register for op1
        auto op2_reg = op2_is_global ? t[1] : get_reg_from_var(op2);  // get the register for op2

        if (op1_is_global) {
            rv_program.push_back(make_rv_inst(op1_reg, {}, {}, rv::rvOPCODE::LA, 0, op1.name));  // load address of global variable into op1_reg
            rv_program.push_back(make_rv_inst(op1_reg, op1_reg, {}, rv::rvOPCODE::LW, 0));       // load the value from the address into op1_reg
        }
        if (op2_is_global) {
            rv_program.push_back(make_rv_inst(op2_reg, {}, {}, rv::rvOPCODE::LA, 0, op2.name));  // load address of global variable into op2_reg
            rv_program.push_back(make_rv_inst(op2_reg, op2_reg, {}, rv::rvOPCODE::LW, 0));       // load the value from the address into op2_reg
        }

        rv_program.push_back(make_rv_inst(des_reg, op1_reg, op2_reg, rv::rvOPCODE::DIV));  // divide op1 by op2, store result in des
    } else {
        assert(false && "Unsupported type for division");  // unsupported type
    }
}

void backend::Generator::gen_mod(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;  // structured binding to get des, op1, op2, op
    if (des.type == ir::Type::Int) {
        auto des_reg = get_reg_from_var(des);                         // get the register for des
        bool op1_is_global = is_global(op1.name);                     // check if op1 is a global variable
        bool op2_is_global = is_global(op2.name);                     // check if op2 is a global variable
        auto op1_reg = op1_is_global ? t[0] : get_reg_from_var(op1);  // get the register for op1
        auto op2_reg = op2_is_global ? t[1] : get_reg_from_var(op2);  // get the register for op2

        if (op1_is_global) {
            rv_program.push_back(make_rv_inst(op1_reg, {}, {}, rv::rvOPCODE::LA, 0, op1.name));  // load address of global variable into op1_reg
            rv_program.push_back(make_rv_inst(op1_reg, op1_reg, {}, rv::rvOPCODE::LW, 0));       // load the value from the address into op1_reg
        }
        if (op2_is_global) {
            rv_program.push_back(make_rv_inst(op2_reg, {}, {}, rv::rvOPCODE::LA, 0, op2.name));  // load address of global variable into op2_reg
            rv_program.push_back(make_rv_inst(op2_reg, op2_reg, {}, rv::rvOPCODE::LW, 0));       // load the value from the address into op2_reg
        }

        rv_program.push_back(make_rv_inst(des_reg, op1_reg, op2_reg, rv::rvOPCODE::REM));  // compute remainder of op1 divided by op2, store result in des
    } else {
        assert(false && "Unsupported type for modulus operation");  // unsupported type
    }
}

void backend::Generator::gen_lss(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;  // structured binding to get des, op1, op2, op
    if (des.type == ir::Type::Int) {
        auto des_reg = get_reg_from_var(des);                         // get the register for des
        bool op1_is_global = is_global(op1.name);                     // check if op1 is a global variable
        bool op2_is_global = is_global(op2.name);                     // check if op2 is a global variable
        auto op1_reg = op1_is_global ? t[0] : get_reg_from_var(op1);  // get the register for op1
        auto op2_reg = op2_is_global ? t[1] : get_reg_from_var(op2);  // get the register for op2

        if (op1_is_global) {
            rv_program.push_back(make_rv_inst(op1_reg, {}, {}, rv::rvOPCODE::LA, 0, op1.name));  // load address of global variable into op1_reg
            rv_program.push_back(make_rv_inst(op1_reg, op1_reg, {}, rv::rvOPCODE::LW, 0));       // load the value from the address into op1_reg
        }
        if (op2_is_global) {
            rv_program.push_back(make_rv_inst(op2_reg, {}, {}, rv::rvOPCODE::LA, 0, op2.name));  // load address of global variable into op2_reg
            rv_program.push_back(make_rv_inst(op2_reg, op2_reg, {}, rv::rvOPCODE::LW, 0));       // load the value from the address into op2_reg
        }

        rv_program.push_back(make_rv_inst(des_reg, op1_reg, op2_reg, rv::rvOPCODE::SLT));  // compare if op1 < op2 and store result in des
    } else {
        assert(false && "Unsupported type for less than operation");  // unsupported type
    }
}
void backend::Generator::gen_leq(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;  // structured binding to get des, op1, op2, op
    if (des.type == ir::Type::Int) {
        auto des_reg = get_reg_from_var(des);                         // get the register for des
        bool op1_is_global = is_global(op1.name);                     // check if op1 is a global variable
        bool op2_is_global = is_global(op2.name);                     // check if op2 is a global variable
        auto op1_reg = op1_is_global ? t[0] : get_reg_from_var(op1);  // get the register for op1
        auto op2_reg = op2_is_global ? t[1] : get_reg_from_var(op2);  // get the register for op2

        if (op1_is_global) {
            rv_program.push_back(make_rv_inst(op1_reg, {}, {}, rv::rvOPCODE::LA, 0, op1.name));  // load address of global variable into op1_reg
            rv_program.push_back(make_rv_inst(op1_reg, op1_reg, {}, rv::rvOPCODE::LW, 0));       // load the value from the address into op1_reg
        }
        if (op2_is_global) {
            rv_program.push_back(make_rv_inst(op2_reg, {}, {}, rv::rvOPCODE::LA, 0, op2.name));  // load address of global variable into op2_reg
            rv_program.push_back(make_rv_inst(op2_reg, op2_reg, {}, rv::rvOPCODE::LW, 0));       // load the value from the address into op2_reg
        }

        rv_program.push_back(make_rv_inst(t[2], op2_reg, op1_reg, rv::rvOPCODE::SLT));  // t[2] = (op2 < op1)
        rv_program.push_back(make_rv_inst(des_reg, t[2], {}, rv::rvOPCODE::XORI, 1));   // des = !(op2 < op1)
    } else {
        assert(false && "Unsupported type for less than or equal operation");  // unsupported type
    }
}
void backend::Generator::gen_gtr(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;  // structured binding to get des, op1, op2, op
    if (des.type == ir::Type::Int) {
        auto des_reg = get_reg_from_var(des);                         // get the register for des
        bool op1_is_global = is_global(op1.name);                     // check if op1 is a global variable
        bool op2_is_global = is_global(op2.name);                     // check if op2 is a global variable
        auto op1_reg = op1_is_global ? t[0] : get_reg_from_var(op1);  // get the register for op1
        auto op2_reg = op2_is_global ? t[1] : get_reg_from_var(op2);  // get the register for op2

        if (op1_is_global) {
            rv_program.push_back(make_rv_inst(op1_reg, {}, {}, rv::rvOPCODE::LA, 0, op1.name));  // load address of global variable into op1_reg
            rv_program.push_back(make_rv_inst(op1_reg, op1_reg, {}, rv::rvOPCODE::LW, 0));       // load the value from the address into op1_reg
        }
        if (op2_is_global) {
            rv_program.push_back(make_rv_inst(op2_reg, {}, {}, rv::rvOPCODE::LA, 0, op2.name));  // load address of global variable into op2_reg
            rv_program.push_back(make_rv_inst(op2_reg, op2_reg, {}, rv::rvOPCODE::LW, 0));       // load the value from the address into op2_reg
        }

        rv_program.push_back(make_rv_inst(des_reg, op2_reg, op1_reg, rv::rvOPCODE::SLT));  // des = (op2 < op1)
    } else {
        assert(false && "Unsupported type for greater than operation");  // unsupported type
    }
}

void backend::Generator::gen_geq(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;  // structured binding to get des, op1, op2, op
    if (des.type == ir::Type::Int) {
        auto des_reg = get_reg_from_var(des);                         // get the register for des
        bool op1_is_global = is_global(op1.name);                     // check if op1 is a global variable
        bool op2_is_global = is_global(op2.name);                     // check if op2 is a global variable
        auto op1_reg = op1_is_global ? t[0] : get_reg_from_var(op1);  // get the register for op1
        auto op2_reg = op2_is_global ? t[1] : get_reg_from_var(op2);  // get the register for op2

        if (op1_is_global) {
            rv_program.push_back(make_rv_inst(op1_reg, {}, {}, rv::rvOPCODE::LA, 0, op1.name));  // load address of global variable into op1_reg
            rv_program.push_back(make_rv_inst(op1_reg, op1_reg, {}, rv::rvOPCODE::LW, 0));       // load the value from the address into op1_reg
        }
        if (op2_is_global) {
            rv_program.push_back(make_rv_inst(op2_reg, {}, {}, rv::rvOPCODE::LA, 0, op2.name));  // load address of global variable into op2_reg
            rv_program.push_back(make_rv_inst(op2_reg, op2_reg, {}, rv::rvOPCODE::LW, 0));       // load the value from the address into op2_reg
        }

        rv_program.push_back(make_rv_inst(t[2], op1_reg, op2_reg, rv::rvOPCODE::SLT));  // t[2] = (op1 < op2)
        rv_program.push_back(make_rv_inst(des_reg, t[2], {}, rv::rvOPCODE::XORI, 1));   // des = !(op1 < op2)
    } else {
        assert(false && "Unsupported type for greater than or equal operation");  // unsupported type
    }
}
void backend::Generator::gen_eq(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;  // structured binding to get des, op1, op2, op
    if (des.type == ir::Type::Int) {
        auto des_reg = get_reg_from_var(des);                         // get the register for des
        bool op1_is_global = is_global(op1.name);                     // check if op1 is a global variable
        bool op2_is_global = is_global(op2.name);                     // check if op2 is a global variable
        auto op1_reg = op1_is_global ? t[0] : get_reg_from_var(op1);  // get the register for op1
        auto op2_reg = op2_is_global ? t[1] : get_reg_from_var(op2);  // get the register for op2

        if (op1_is_global) {
            rv_program.push_back(make_rv_inst(op1_reg, {}, {}, rv::rvOPCODE::LA, 0, op1.name));  // load address of global variable into op1_reg
            rv_program.push_back(make_rv_inst(op1_reg, op1_reg, {}, rv::rvOPCODE::LW, 0));       // load the value from the address into op1_reg
        }
        if (op2_is_global) {
            rv_program.push_back(make_rv_inst(op2_reg, {}, {}, rv::rvOPCODE::LA, 0, op2.name));  // load address of global variable into op2_reg
            rv_program.push_back(make_rv_inst(op2_reg, op2_reg, {}, rv::rvOPCODE::LW, 0));       // load the value from the address into op2_reg
        }

        rv_program.push_back(make_rv_inst(t[2], op1_reg, op2_reg, rv::rvOPCODE::XOR));  // t[2] = op1 ^ op2
        rv_program.push_back(make_rv_inst(des_reg, t[2], {}, rv::rvOPCODE::SEQZ));      // des = (t[2] == 0)
    } else {
        assert(false && "Unsupported type for equality operation");  // unsupported type
    }
}
void backend::Generator::gen_neq(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;  // structured binding to get des, op1, op2, op
    if (des.type == ir::Type::Int) {
        auto des_reg = get_reg_from_var(des);                         // get the register for des
        bool op1_is_global = is_global(op1.name);                     // check if op1 is a global variable
        bool op2_is_global = is_global(op2.name);                     // check if op2 is a global variable
        auto op1_reg = op1_is_global ? t[0] : get_reg_from_var(op1);  // get the register for op1
        auto op2_reg = op2_is_global ? t[1] : get_reg_from_var(op2);  // get the register for op2

        if (op1_is_global) {
            rv_program.push_back(make_rv_inst(op1_reg, {}, {}, rv::rvOPCODE::LA, 0, op1.name));  // load address of global variable into op1_reg
            rv_program.push_back(make_rv_inst(op1_reg, op1_reg, {}, rv::rvOPCODE::LW, 0));       // load the value from the address into op1_reg
        }
        if (op2_is_global) {
            rv_program.push_back(make_rv_inst(op2_reg, {}, {}, rv::rvOPCODE::LA, 0, op2.name));  // load address of global variable into op2_reg
            rv_program.push_back(make_rv_inst(op2_reg, op2_reg, {}, rv::rvOPCODE::LW, 0));       // load the value from the address into op2_reg
        }

        rv_program.push_back(make_rv_inst(t[2], op1_reg, op2_reg, rv::rvOPCODE::XOR));  // t[2] = op1 ^ op2
        rv_program.push_back(make_rv_inst(des_reg, t[2], {}, rv::rvOPCODE::SNEZ));      // des = (t[2] != 0)
    } else {
        assert(false && "Unsupported type for inequality operation");  // unsupported type
    }
}
void backend::Generator::gen_not(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;  // structured binding to get des, op1, op
    if (des.type == ir::Type::Int) {
        auto des_reg = get_reg_from_var(des);                         // get the register for des
        bool op1_is_global = is_global(op1.name);                     // check if op1 is a global variable
        auto op1_reg = op1_is_global ? t[0] : get_reg_from_var(op1);  // get the register for op1

        if (op1_is_global) {
            rv_program.push_back(make_rv_inst(op1_reg, {}, {}, rv::rvOPCODE::LA, 0, op1.name));  // load address of global variable into op1_reg
            rv_program.push_back(make_rv_inst(op1_reg, op1_reg, {}, rv::rvOPCODE::LW, 0));       // load the value from the address into op1_reg
        }

        rv_program.push_back(make_rv_inst(des_reg, op1_reg, {}, rv::rvOPCODE::XORI, -1));  // des = ~op1 (bitwise NOT operation)
    } else {
        assert(false && "Unsupported type for NOT operation");  // unsupported type
    }
}
void backend::Generator::gen_or(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;  // structured binding to get des, op1, op2, op
    if (des.type == ir::Type::Int) {
        auto des_reg = get_reg_from_var(des);  // get the register for des
        if (op1.type == ir::Type::IntLiteral) {
            rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LI, std::stoi(op1.name)));  // load immediate value into t[0]
            int op2_is_global = is_global(op2.name);                                                  // check if op2 is a global variable
            auto op2_reg = op2_is_global ? t[1] : get_reg_from_var(op2);                              // get the register for op2
            if (op2_is_global) {
                rv_program.push_back(make_rv_inst(op2_reg, {}, {}, rv::rvOPCODE::LA, 0, op2.name));  // load address of global variable into op2_reg
                rv_program.push_back(make_rv_inst(op2_reg, op2_reg, {}, rv::rvOPCODE::LW, 0));       // load the value from the address into op2_reg
            }

            rv_program.push_back(make_rv_inst(des_reg, t[0], op2_reg, rv::rvOPCODE::OR));  // perform bitwise OR operation between t[0] and op2_reg, store result in des
        } else if (op2.type == ir::Type::IntLiteral) {
            rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LI, std::stoi(op2.name)));  // load immediate value into t[0]
            int op1_is_global = is_global(op1.name);                                                  // check if op1 is a global variable
            auto op1_reg = op1_is_global ? t[1] : get_reg_from_var(op1);                              // get the register for op1
            if (op1_is_global) {
                rv_program.push_back(make_rv_inst(op1_reg, {}, {}, rv::rvOPCODE::LA, 0, op1.name));  // load address of global variable into op1_reg
                rv_program.push_back(make_rv_inst(op1_reg, op1_reg, {}, rv::rvOPCODE::LW, 0));       // load the value from the address into op1_reg
            }

            rv_program.push_back(make_rv_inst(des_reg, op1_reg, t[0], rv::rvOPCODE::OR));  // perform bitwise OR operation between op1_reg and t[0], store result in des
        } else {
            auto op1_reg = get_reg_from_var(op1);  // get the register for op1
            auto op2_reg = get_reg_from_var(op2);  // get the register for op2

            rv_program.push_back(make_rv_inst(des_reg, op1_reg, op2_reg, rv::rvOPCODE::OR));  // perform bitwise OR operation between op1 and op2, store result in des
        }
    }
}
void backend::Generator::gen_and(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;  // structured binding to get des, op1, op2, op
    if (des.type == ir::Type::Int) {
        auto des_reg = get_reg_from_var(des);  // get the register for des
        if (op1.type == ir::Type::IntLiteral) {
            rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LI, std::stoi(op1.name)));  // load immediate value into t[0]
            int op2_is_global = is_global(op2.name);                                                  // check if op2 is a global variable
            auto op2_reg = op2_is_global ? t[1] : get_reg_from_var(op2);                              // get the register for op2
            if (op2_is_global) {
                rv_program.push_back(make_rv_inst(op2_reg, {}, {}, rv::rvOPCODE::LA, 0, op2.name));  // load address of global variable into op2_reg
                rv_program.push_back(make_rv_inst(op2_reg, op2_reg, {}, rv::rvOPCODE::LW, 0));       // load the value from the address into op2_reg
            }

            rv_program.push_back(make_rv_inst(des_reg, t[0], op2_reg, rv::rvOPCODE::AND));  // perform bitwise AND operation between t[0] and op2_reg, store result in des
        } else if (op2.type == ir::Type::IntLiteral) {
            rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LI, std::stoi(op2.name)));  // load immediate value into t[0]
            int op1_is_global = is_global(op1.name);                                                  // check if op1 is a global variable
            auto op1_reg = op1_is_global ? t[1] : get_reg_from_var(op1);                              // get the register for op1
            if (op1_is_global) {
                rv_program.push_back(make_rv_inst(op1_reg, {}, {}, rv::rvOPCODE::LA, 0, op1.name));  // load address of global variable into op1_reg
                rv_program.push_back(make_rv_inst(op1_reg, op1_reg, {}, rv::rvOPCODE::LW, 0));       // load the value from the address into op1_reg
            }
            rv_program.push_back(make_rv_inst(des_reg, op1_reg, t[0], rv::rvOPCODE::AND));  // perform bitwise AND operation between op1_reg and t[0], store result in des
        } else {
            auto op1_reg = get_reg_from_var(op1);  // get the register for op1
            auto op2_reg = get_reg_from_var(op2);  // get the register for op2

            rv_program.push_back(make_rv_inst(des_reg, op1_reg, op2_reg, rv::rvOPCODE::AND));  // perform bitwise AND operation between op1 and op2, store result in des
        }
    } else {
        assert(false && "Unsupported type for AND operation");  // unsupported type
    }
}

// float instructions
void backend::Generator::gen_fmov(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;  // structured binding to get des, op1, op
    if (des.type == ir::Type::Float) {
        rv::rvREG freg_des = get_reg_from_var(des);
        if (is_global(op1.name)) {
            rv_program.push_back(make_rv_inst(ft[0], {}, {}, rv::rvOPCODE::LA, 0, op1.name));  // la ft[0], "op1.name"
            rv_program.push_back(make_rv_inst(freg_des, ft[0], {}, rv::rvOPCODE::FLW, 0));     // flw freg_des, 0(ft[0])
        } else if (op1.type == ir::Type::FloatLiteral) {
            // if op1 is a float literal, write it directly to the floating-point register
            std::string ieee_str = float2ieee(op1.name);
            std::cout << "fmov: param.name: " << op1.name << ", ieee_str: " << ieee_str << std::endl;
            uint32_t float_value = std::stoul(ieee_str);                                      // convert binary string to uint32_t
            rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LI, float_value));  // load immediate value into floating-point register
            rv_program.push_back(make_rv_inst(freg_des, t[0], {}, rv::rvOPCODE::FMV_X_S));    // move the value from t[0] to freg_des
        } else {
            rv::rvREG freg_op1 = get_reg_from_var(op1);                                       // get the floating-point register for op1
            rv_program.push_back(make_rv_inst(freg_des, freg_op1, {}, rv::rvOPCODE::FMV_S));  // move the value from op1 to des
        }
    } else {
        assert(false && "Unsupported type for floating-point move operation");  // unsupported type
    }
}

void backend::Generator::gen_fadd(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;  // structured binding to get des, op1, op2, op
    if (des.type == ir::Type::Float) {
        auto des_freg = get_reg_from_var(des);                          // get the floating-point register for des
        bool op1_is_global = is_global(op1.name);                       // check if op1 is a global variable
        bool op2_is_global = is_global(op2.name);                       // check if op2 is a global variable
        auto op1_freg = op1_is_global ? ft[0] : get_reg_from_var(op1);  // get the floating-point register for op1
        auto op2_freg = op2_is_global ? ft[1] : get_reg_from_var(op2);  // get the floating-point register for op2

        if (op1_is_global) {
            rv_program.push_back(make_rv_inst(op1_freg, {}, {}, rv::rvOPCODE::LA, 0, op1.name));  // load address of global variable into op1_freg
            rv_program.push_back(make_rv_inst(op1_freg, op1_freg, {}, rv::rvOPCODE::FLW, 0));     // load the value from the address into op1_freg
        }
        if (op2_is_global) {
            rv_program.push_back(make_rv_inst(op2_freg, {}, {}, rv::rvOPCODE::LA, 0, op2.name));  // load address of global variable into op2_freg
            rv_program.push_back(make_rv_inst(op2_freg, op2_freg, {}, rv::rvOPCODE::FLW, 0));     // load the value from the address into op2_freg
        }

        rv_program.push_back(make_rv_inst(des_freg, op1_freg, op2_freg, rv::rvOPCODE::FADD_S));  // add op1 and op2, store result in des
    } else {
        assert(false && "Unsupported type for floating-point addition");  // unsupported type
    }
}

void backend::Generator::gen_fsub(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;  // structured binding to get des, op1, op2, op
    if (des.type == ir::Type::Float) {
        auto des_freg = get_reg_from_var(des);                          // get the floating-point register for des
        bool op1_is_global = is_global(op1.name);                       // check if op1 is a global variable
        bool op2_is_global = is_global(op2.name);                       // check if op2 is a global variable
        auto op1_freg = op1_is_global ? ft[0] : get_reg_from_var(op1);  // get the floating-point register for op1
        auto op2_freg = op2_is_global ? ft[1] : get_reg_from_var(op2);  // get the floating-point register for op2
        if (op1_is_global) {
            rv_program.push_back(make_rv_inst(op1_freg, {}, {}, rv::rvOPCODE::LA, 0, op1.name));  // load address of global variable into op1_freg
            rv_program.push_back(make_rv_inst(op1_freg, op1_freg, {}, rv::rvOPCODE::FLW, 0));     // load the value from the address into op1_freg
        }
        if (op2_is_global) {
            rv_program.push_back(make_rv_inst(op2_freg, {}, {}, rv::rvOPCODE::LA, 0, op2.name));  // load address of global variable into op2_freg
            rv_program.push_back(make_rv_inst(op2_freg, op2_freg, {}, rv::rvOPCODE::FLW, 0));     // load the value from the address into op2_freg
        }
        rv_program.push_back(make_rv_inst(des_freg, op1_freg, op2_freg, rv::rvOPCODE::FSUB_S));  // subtract op2 from op1, store result in des
    } else {
        assert(false && "Unsupported type for floating-point subtraction");  // unsupported type
    }
}

void backend::Generator::gen_fmul(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;  // structured binding to get des, op1, op2, op
    if (des.type == ir::Type::Float) {
        auto des_freg = get_reg_from_var(des);                          // get the floating-point register for des
        bool op1_is_global = is_global(op1.name);                       // check if op1 is a global variable
        bool op2_is_global = is_global(op2.name);                       // check if op2 is a global variable
        auto op1_freg = op1_is_global ? ft[0] : get_reg_from_var(op1);  // get the floating-point register for op1
        auto op2_freg = op2_is_global ? ft[1] : get_reg_from_var(op2);  // get the floating-point register for op2
        if (op1_is_global) {
            rv_program.push_back(make_rv_inst(op1_freg, {}, {}, rv::rvOPCODE::LA, 0, op1.name));  // load address of global variable into op1_freg
            rv_program.push_back(make_rv_inst(op1_freg, op1_freg, {}, rv::rvOPCODE::FLW, 0));     // load the value from the address into op1_freg
        }
        if (op2_is_global) {
            rv_program.push_back(make_rv_inst(op2_freg, {}, {}, rv::rvOPCODE::LA, 0, op2.name));  // load address of global variable into op2_freg
            rv_program.push_back(make_rv_inst(op2_freg, op2_freg, {}, rv::rvOPCODE::FLW, 0));     // load the value from the address into op2_freg
        }
        rv_program.push_back(make_rv_inst(des_freg, op1_freg, op2_freg, rv::rvOPCODE::FMUL_S));  // multiply op1 and op2, store result in des
    } else {
        assert(false && "Unsupported type for floating-point multiplication");  // unsupported type
    }
}

void backend::Generator::gen_fdiv(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;  // structured binding to get des, op1, op2, op
    if (des.type == ir::Type::Float) {
        auto des_freg = get_reg_from_var(des);                          // get the floating-point register for des
        bool op1_is_global = is_global(op1.name);                       // check if op1 is a global variable
        bool op2_is_global = is_global(op2.name);                       // check if op2 is a global variable
        auto op1_freg = op1_is_global ? ft[0] : get_reg_from_var(op1);  // get the floating-point register for op1
        auto op2_freg = op2_is_global ? ft[1] : get_reg_from_var(op2);  // get the floating-point register for op2
        if (op1_is_global) {
            rv_program.push_back(make_rv_inst(op1_freg, {}, {}, rv::rvOPCODE::LA, 0, op1.name));  // load address of global variable into op1_freg
            rv_program.push_back(make_rv_inst(op1_freg, op1_freg, {}, rv::rvOPCODE::FLW, 0));     // load the value from the address into op1_freg
        }
        if (op2_is_global) {
            rv_program.push_back(make_rv_inst(op2_freg, {}, {}, rv::rvOPCODE::LA, 0, op2.name));  // load address of global variable into op2_freg
            rv_program.push_back(make_rv_inst(op2_freg, op2_freg, {}, rv::rvOPCODE::FLW, 0));     // load the value from the address into op2_freg
        }
        rv_program.push_back(make_rv_inst(des_freg, op1_freg, op2_freg, rv::rvOPCODE::FDIV_S));  // divide op1 by op2, store result in des
    }
}

void backend::Generator::gen_flss(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;
    rv::rvREG reg_des = get_reg_from_var(des);
    rv::rvREG freg_op1 = get_reg_from_var(op1);
    rv::rvREG freg_op2 = get_reg_from_var(op2);

    if (is_global(op1.name)) {
        rv_program.push_back(make_rv_inst(ft[0], {}, {}, rv::rvOPCODE::LA, 0, op1.name));
        rv_program.push_back(make_rv_inst(freg_op1, ft[0], {}, rv::rvOPCODE::FLW, 0));
    }

    if (is_global(op2.name)) {
        rv_program.push_back(make_rv_inst(ft[1], {}, {}, rv::rvOPCODE::LA, 0, op2.name));
        rv_program.push_back(make_rv_inst(freg_op2, ft[1], {}, rv::rvOPCODE::FLW, 0));
    }

    rv_program.push_back(make_rv_inst(t[0], freg_op1, freg_op2, rv::rvOPCODE::FLT_S));
    rv_program.push_back(make_rv_inst(reg_des, t[0], {}, rv::rvOPCODE::ADDI, 0));
}

void backend::Generator::gen_fleq(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;
    rv::rvREG reg_des = get_reg_from_var(des);
    rv::rvREG freg_op1 = get_reg_from_var(op1);
    rv::rvREG freg_op2 = get_reg_from_var(op2);

    if (is_global(op1.name)) {
        rv_program.push_back(make_rv_inst(ft[0], {}, {}, rv::rvOPCODE::LA, 0, op1.name));
        rv_program.push_back(make_rv_inst(freg_op1, ft[0], {}, rv::rvOPCODE::FLW, 0));
    }

    if (is_global(op2.name)) {
        rv_program.push_back(make_rv_inst(ft[1], {}, {}, rv::rvOPCODE::LA, 0, op2.name));
        rv_program.push_back(make_rv_inst(freg_op2, ft[1], {}, rv::rvOPCODE::FLW, 0));
    }

    rv_program.push_back(make_rv_inst(t[0], freg_op1, freg_op2, rv::rvOPCODE::FLE_S));
    rv_program.push_back(make_rv_inst(reg_des, t[0], {}, rv::rvOPCODE::ADDI, 0));
}

void backend::Generator::gen_fgtr(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;
    rv::rvREG reg_des = get_reg_from_var(des);
    rv::rvREG freg_op1 = get_reg_from_var(op1);
    rv::rvREG freg_op2 = get_reg_from_var(op2);

    if (is_global(op1.name)) {
        rv_program.push_back(make_rv_inst(ft[0], {}, {}, rv::rvOPCODE::LA, 0, op1.name));
        rv_program.push_back(make_rv_inst(freg_op1, ft[0], {}, rv::rvOPCODE::FLW, 0));
    }

    if (is_global(op2.name)) {
        rv_program.push_back(make_rv_inst(ft[1], {}, {}, rv::rvOPCODE::LA, 0, op2.name));
        rv_program.push_back(make_rv_inst(freg_op2, ft[1], {}, rv::rvOPCODE::FLW, 0));
    }

    rv_program.push_back(make_rv_inst(t[0], freg_op2, freg_op1, rv::rvOPCODE::FLT_S));
    rv_program.push_back(make_rv_inst(reg_des, t[0], {}, rv::rvOPCODE::ADDI, 0));
}

void backend::Generator::gen_fgeq(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;
    rv::rvREG reg_des = get_reg_from_var(des);
    rv::rvREG freg_op1 = get_reg_from_var(op1);
    rv::rvREG freg_op2 = get_reg_from_var(op2);

    if (is_global(op1.name)) {
        rv_program.push_back(make_rv_inst(ft[0], {}, {}, rv::rvOPCODE::LA, 0, op1.name));
        rv_program.push_back(make_rv_inst(freg_op1, ft[0], {}, rv::rvOPCODE::FLW, 0));
    }

    if (is_global(op2.name)) {
        rv_program.push_back(make_rv_inst(ft[1], {}, {}, rv::rvOPCODE::LA, 0, op2.name));
        rv_program.push_back(make_rv_inst(freg_op2, ft[1], {}, rv::rvOPCODE::FLW, 0));
    }

    rv_program.push_back(make_rv_inst(t[0], freg_op2, freg_op1, rv::rvOPCODE::FLE_S));
    rv_program.push_back(make_rv_inst(reg_des, t[0], {}, rv::rvOPCODE::ADDI, 0));
}

void backend::Generator::gen_feq(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;
    rv::rvREG reg_des = get_reg_from_var(des);
    rv::rvREG freg_op1 = get_reg_from_var(op1);
    rv::rvREG freg_op2 = get_reg_from_var(op2);

    if (is_global(op1.name)) {
        rv_program.push_back(make_rv_inst(ft[0], {}, {}, rv::rvOPCODE::LA, 0, op1.name));
        rv_program.push_back(make_rv_inst(freg_op1, ft[0], {}, rv::rvOPCODE::FLW, 0));
    }

    if (is_global(op2.name)) {
        rv_program.push_back(make_rv_inst(ft[1], {}, {}, rv::rvOPCODE::LA, 0, op2.name));
        rv_program.push_back(make_rv_inst(freg_op2, ft[1], {}, rv::rvOPCODE::FLW, 0));
    }

    rv_program.push_back(make_rv_inst(t[0], freg_op1, freg_op2, rv::rvOPCODE::FEQ_S));
    rv_program.push_back(make_rv_inst(reg_des, t[0], {}, rv::rvOPCODE::ADDI, 0));
}

void backend::Generator::gen_fneq(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;
    rv::rvREG reg_des = get_reg_from_var(des);
    rv::rvREG freg_op1 = get_reg_from_var(op1);
    rv::rvREG freg_op2 = get_reg_from_var(op2);

    if (is_global(op1.name)) {
        rv_program.push_back(make_rv_inst(ft[0], {}, {}, rv::rvOPCODE::LA, 0, op1.name));
        rv_program.push_back(make_rv_inst(freg_op1, ft[0], {}, rv::rvOPCODE::FLW, 0));
    }

    if (is_global(op2.name)) {
        rv_program.push_back(make_rv_inst(ft[1], {}, {}, rv::rvOPCODE::LA, 0, op2.name));
        rv_program.push_back(make_rv_inst(freg_op2, ft[1], {}, rv::rvOPCODE::FLW, 0));
    }

    rv_program.push_back(make_rv_inst(t[0], freg_op1, freg_op2, rv::rvOPCODE::FEQ_S));
    rv_program.push_back(make_rv_inst(t[0], t[0], {}, rv::rvOPCODE::XORI, 1));
    rv_program.push_back(make_rv_inst(reg_des, t[0], {}, rv::rvOPCODE::ADDI, 0));
}

// conversion instructions
void backend::Generator::gen_cvt_i2f(const ir::Instruction &inst) {
    // int var to float var
    auto [op1, op2, des, op] = inst;  // structured binding to get des, op1, op2, op, des is obj, op1 is int var

    auto des_freg = get_reg_from_var(des);  // get the floating-point register for des
    if (is_global(op1.name)) {
        rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LA, 0, op1.name));  // load address of global variable into ft[0]
        rv_program.push_back(make_rv_inst(des_freg, t[0], {}, rv::rvOPCODE::FMV_X_S));    // move the value from ft[0] to A0
    } else {
        auto op1_reg = get_reg_from_var(op1);                                              // get the register for op1
        rv_program.push_back(make_rv_inst(des_freg, op1_reg, {}, rv::rvOPCODE::FMV_X_S));  // move the value from op1 to t[0]
    }
}
void backend::Generator::gen_cvt_f2i(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;       // structured binding to get des, op1, op2, op, des is obj, op1 is float var
    auto des_reg = get_reg_from_var(des);  // get the register for des
    if (is_global(op1.name)) {
        rv_program.push_back(make_rv_inst(ft[0], {}, {}, rv::rvOPCODE::LA, 0, op1.name));  // load address of global variable into ft[0]
        rv_program.push_back(make_rv_inst(des_reg, ft[0], {}, rv::rvOPCODE::FMV_S_X));     // move the value from ft[0] to des
    } else {
        auto op1_freg = get_reg_from_var(op1);                                             // get the floating-point register for op1
        rv_program.push_back(make_rv_inst(des_reg, op1_freg, {}, rv::rvOPCODE::FMV_S_X));  // move the value from op1 to des
    }
}

// control flow instructions
void backend::Generator::gen_return(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;  // structured binding to get des, op1, op2, op
    if (op1.type == ir::Type::IntLiteral) {
        rv_program.push_back(make_rv_inst(a[0], {}, {}, rv::rvOPCODE::LI, std::stoi(op1.name)));  // load immediate value into A0
    } else if (op1.type == ir::Type::Int) {
        auto op1_reg = get_reg_from_var(op1);                                     // get the register for op1
        rv_program.push_back(make_rv_inst(a[0], op1_reg, {}, rv::rvOPCODE::MV));  // move the value from op1 to A0
    } else if (op1.type == ir::Type::FloatLiteral) {
        std::string ieee_str = float2ieee(op1.name);
        std::cout << "return: param.name: " << op1.name << ", ieee_str: " << ieee_str << std::endl;
        uint32_t float_value = std::stoul(ieee_str);
        rv_program.push_back(make_rv_inst(ft[0], {}, {}, rv::rvOPCODE::LI, float_value));  // load immediate value into ft[0]
        rv_program.push_back(make_rv_inst(fa[0], ft[0], {}, rv::rvOPCODE::FMV_S));         // move the value from ft[0] to A0
    } else if (op1.type == ir::Type::Float) {
        auto op1_freg = get_reg_from_var(op1);                                         // get the floating-point register for op1
        rv_program.push_back(make_rv_inst(fa[0], op1_freg, {}, rv::rvOPCODE::FMV_S));  // move the value from op1 to A0
    } else if (op1.type == ir::Type::null) {
        // do nothing
    } else {
        assert(false && "Unsupported type for return operation");  // unsupported type
    }
}

void backend::Generator::gen_goto(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;  // structured binding to get des, op1, op2, op, des is obj, op1 is cond
    int offset = std::stoi(des.name);
    std::string label = pc2label[ir4pc + offset];  // get the label corresponding to the offset
    if (op1.type == ir::Type::IntLiteral) {
        int jumpORnot = std::stoi(op1.name);  // if op1 is an integer literal, use it directly
        if (jumpORnot) {
            rv_program.push_back(make_rv_inst({}, {}, {}, rv::rvOPCODE::J, 0, label));  // unconditional jump to the label
        }
    } else if (op1.type == ir::Type::Int) {
        rv::rvREG cond_reg = get_reg_from_var(op1);
        if (is_global(op1.name)) {
            rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LA, 0, op1.name));
            rv_program.push_back(make_rv_inst(cond_reg, t[0], {}, rv::rvOPCODE::LW, 0));
        }

        rv_program.push_back(make_rv_inst({}, cond_reg, rv::rvREG::X0, rv::rvOPCODE::BNE, 0, label));
    } else if (op1.type == ir::Type::null) {
        rv_program.push_back(make_rv_inst({}, {}, {}, rv::rvOPCODE::J, 0, label));  // unconditional jump to the label
    } else {
        std::cout << "Unsupported type for goto operation: " << ir::toString(op1.type) << std::endl;
        assert(false && "Unsupported type for goto operation");  // unsupported type
    }
}

// function instructions
void backend::Generator::gen_call(const ir::Instruction &inst) {
    auto call_inst = dynamic_cast<const ir::CallInst &>(inst);
    auto op1 = call_inst.op1;
    auto op2 = call_inst.op2;
    auto des = call_inst.des;
    auto op = call_inst.op;
    auto paramList = call_inst.argumentList;

    if (op1.name == "main" || op1.name == "global") {
        return;
    }

    int extra_stack_space = 0;
    int int_arg_count = 0;
    int float_arg_count = 0;

    for (auto &param : paramList) {
        if (param.type == ir::Type::Int || param.type == ir::Type::IntLiteral || param.type == ir::Type::FloatPtr || param.type == ir::Type::IntPtr) {
            if (int_arg_count < 8) {
                if (param.type == ir::Type::IntLiteral) {
                    rv_program.push_back(make_rv_inst(a[int_arg_count], {}, {}, rv::rvOPCODE::LI, std::stoi(param.name)));
                } else if (is_global(param.name)) {
                    rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LA, 0, param.name));
                    rv_program.push_back(make_rv_inst(a[int_arg_count], t[0], {}, rv::rvOPCODE::LW, 0));
                } else {
                    rv::rvREG param_reg = get_reg_from_var(param);
                    rv_program.push_back(make_rv_inst(a[int_arg_count], param_reg, {}, rv::rvOPCODE::MV));
                }
            } else {
                extra_stack_space += 4;
                if (param.type == ir::Type::IntLiteral) {
                    rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LI, std::stoi(param.name)));
                    rv_program.push_back(make_rv_inst(sp, sp, {}, rv::rvOPCODE::ADDI, -4));
                    rv_program.push_back(make_rv_inst(sp, t[0], {}, rv::rvOPCODE::SW, 0));
                } else if (is_global(param.name)) {
                    rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LA, 0, param.name));
                    rv_program.push_back(make_rv_inst(t[1], t[0], {}, rv::rvOPCODE::LW, 0));
                    rv_program.push_back(make_rv_inst(sp, sp, {}, rv::rvOPCODE::ADDI, -4));
                    rv_program.push_back(make_rv_inst(sp, t[1], {}, rv::rvOPCODE::SW, 0));
                } else {
                    rv::rvREG param_reg = get_reg_from_var(param);
                    rv_program.push_back(make_rv_inst(sp, sp, {}, rv::rvOPCODE::ADDI, -4));
                    rv_program.push_back(make_rv_inst(sp, param_reg, {}, rv::rvOPCODE::SW, 0));
                }
            }
            int_arg_count++;
        } else if (param.type == ir::Type::Float || param.type == ir::Type::FloatLiteral) {
            if (float_arg_count < 8) {
                if (param.type == ir::Type::FloatLiteral) {
                    std::string ieee_str = float2ieee(param.name);
                    std::cout << "call: param.name: " << param.name << ", ieee_str: " << ieee_str << std::endl;
                    uint32_t float_value = std::stoul(ieee_str);
                    rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LI, float_value));
                    rv_program.push_back(make_rv_inst(fa[float_arg_count], t[0], {}, rv::rvOPCODE::FMV_X_S));
                } else if (is_global(param.name)) {
                    rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LA, 0, param.name));
                    rv_program.push_back(make_rv_inst(fa[float_arg_count], t[0], {}, rv::rvOPCODE::FLW, 0));
                } else {
                    rv::rvREG fparam_reg = get_reg_from_var(param);
                    rv_program.push_back(make_rv_inst(fa[float_arg_count], fparam_reg, {}, rv::rvOPCODE::FMV_S));
                }
            } else {
                extra_stack_space += 4;
                if (param.type == ir::Type::FloatLiteral) {
                    std::string ieee_str = float2ieee(param.name);
                    std::cout << "call: param.name: " << param.name << ", ieee_str: " << ieee_str << std::endl;
                    uint32_t float_value = std::stoul(ieee_str);
                    rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LI, float_value));
                    rv_program.push_back(make_rv_inst(sp, sp, {}, rv::rvOPCODE::ADDI, -4));
                    rv_program.push_back(make_rv_inst(sp, t[0], {}, rv::rvOPCODE::SW, 0));
                } else if (is_global(param.name)) {
                    rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LA, 0, param.name));
                    rv_program.push_back(make_rv_inst(t[1], t[0], {}, rv::rvOPCODE::LW, 0));
                    rv_program.push_back(make_rv_inst(sp, sp, {}, rv::rvOPCODE::ADDI, -4));
                    rv_program.push_back(make_rv_inst(sp, t[1], {}, rv::rvOPCODE::SW, 0));
                } else {
                    rv::rvREG fparam_reg = get_reg_from_var(param);
                    rv_program.push_back(make_rv_inst(sp, sp, {}, rv::rvOPCODE::ADDI, -4));
                    rv_program.push_back(make_rv_inst(sp, fparam_reg, {}, rv::rvOPCODE::FSW, 0));
                }
            }
            float_arg_count++;
        } else {
            std::cout << "Unsupported type for function call parameter: " << ir::toString(param.type) << std::endl;
            assert(false && "Unsupported type for function call parameter");  // unsupported type
        }
    }

    rv_program.push_back(make_rv_inst({}, {}, {}, rv::rvOPCODE::CALL, 0, op1.name));

    if (extra_stack_space > 0) {
        rv_program.push_back(make_rv_inst(sp, sp, {}, rv::rvOPCODE::ADDI, extra_stack_space));
    }

    if (des.type != ir::Type::null) {
        if (des.type == ir::Type::Int || des.type == ir::Type::IntPtr || des.type == ir::Type::FloatPtr) {
            rv::rvREG des_reg = get_reg_from_var(des);
            rv_program.push_back(make_rv_inst(des_reg, a[0], {}, rv::rvOPCODE::MV));
        } else if (des.type == ir::Type::Float) {
            rv::rvREG fdes_reg = get_reg_from_var(des);
            rv_program.push_back(make_rv_inst(fdes_reg, fa[0], {}, rv::rvOPCODE::FMV_S));
        }
    }
}

// memory instructions
void backend::Generator::gen_alloc(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;  // structured binding to get des, op1, op2, op
    if (des.type == ir::Type::IntPtr || des.type == ir::Type::FloatPtr) {
        int size = std::stoi(op1.name);
        auto des_reg = get_reg_from_var(des);  // get the register for des
        rv_program.push_back(make_rv_inst(des_reg, {}, {}, rv::rvOPCODE::LI, get_stack_size()));
        stack_data_size += size;
        rv_program.push_back(make_rv_inst(sp, sp, {}, rv::rvOPCODE::ADDI, -size * 4));  // allocate space on the stack
    } else {
        assert(false && "Unsupported type for allocation");  // unsupported type
    }
}

void backend::Generator::gen_store(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;
    rv::rvREG addr_reg = t[0];
    rv::rvREG index_reg = t[1];
    rv::rvREG value_reg;

    if (op2.type == ir::Type::IntLiteral) {
        rv_program.push_back(make_rv_inst(index_reg, {}, {}, rv::rvOPCODE::LI, std::stoi(op2.name)));
    } else {
        if (is_global(op2.name)) {
            rv_program.push_back(make_rv_inst(t[2], {}, {}, rv::rvOPCODE::LA, 0, op2.name));
            rv_program.push_back(make_rv_inst(index_reg, t[2], {}, rv::rvOPCODE::LW, 0));
        } else {
            index_reg = get_reg_from_var(op2);
        }
    }

    if (is_global(op1.name)) {
        rv_program.push_back(make_rv_inst(addr_reg, {}, {}, rv::rvOPCODE::LA, 0, op1.name));
    } else {
        addr_reg = get_reg_from_var(op1);
    }

    rv_program.push_back(make_rv_inst(index_reg, index_reg, {}, rv::rvOPCODE::SLLI, 2));
    rv_program.push_back(make_rv_inst(addr_reg, addr_reg, index_reg, rv::rvOPCODE::ADD));

    if (des.type == ir::Type::Int || des.type == ir::Type::IntLiteral) {
        if (des.type == ir::Type::IntLiteral) {
            rv_program.push_back(make_rv_inst(t[2], {}, {}, rv::rvOPCODE::LI, std::stoi(des.name)));
            rv_program.push_back(make_rv_inst(addr_reg, t[2], {}, rv::rvOPCODE::SW, 0));
        } else if (is_global(des.name)) {
            rv_program.push_back(make_rv_inst(t[2], {}, {}, rv::rvOPCODE::LA, 0, des.name));
            rv_program.push_back(make_rv_inst(t[3], t[2], {}, rv::rvOPCODE::LW, 0));
            rv_program.push_back(make_rv_inst(addr_reg, t[3], {}, rv::rvOPCODE::SW, 0));
        } else {
            value_reg = get_reg_from_var(des);
            rv_program.push_back(make_rv_inst(addr_reg, value_reg, {}, rv::rvOPCODE::SW, 0));
        }
    } else if (des.type == ir::Type::Float || des.type == ir::Type::FloatLiteral) {
        if (des.type == ir::Type::FloatLiteral) {
            std::string ieee_str = float2ieee(des.name);
            std::cout << "store: des.name: " << des.name << ", ieee_str: " << ieee_str << std::endl;
            uint32_t float_value = std::stoul(ieee_str);
            rv_program.push_back(make_rv_inst(ft[0], {}, {}, rv::rvOPCODE::LI, float_value));
            rv_program.push_back(make_rv_inst(addr_reg, ft[0], {}, rv::rvOPCODE::FSW, 0));
        } else if (is_global(des.name)) {
            rv_program.push_back(make_rv_inst(ft[0], {}, {}, rv::rvOPCODE::LA, 0, des.name));
            rv_program.push_back(make_rv_inst(ft[1], ft[0], {}, rv::rvOPCODE::FLW, 0));
            rv_program.push_back(make_rv_inst(addr_reg, ft[1], {}, rv::rvOPCODE::FSW, 0));
        } else {
            rv::rvREG fvalue_reg = get_reg_from_var(des);
            rv_program.push_back(make_rv_inst(addr_reg, fvalue_reg, {}, rv::rvOPCODE::FSW, 0));
        }
    } else {
        assert(false && "Unsupported type for store operation");
    }
}

void backend::Generator::gen_load(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;
    rv::rvREG addr_reg = t[0];
    rv::rvREG index_reg = t[1];
    rv::rvREG des_reg;

    if (op2.type == ir::Type::IntLiteral) {
        rv_program.push_back(make_rv_inst(index_reg, {}, {}, rv::rvOPCODE::LI, std::stoi(op2.name)));
    } else {
        if (is_global(op2.name)) {
            rv_program.push_back(make_rv_inst(t[2], {}, {}, rv::rvOPCODE::LA, 0, op2.name));
            rv_program.push_back(make_rv_inst(index_reg, t[2], {}, rv::rvOPCODE::LW, 0));
        } else {
            index_reg = get_reg_from_var(op2);
        }
    }

    if (is_global(op1.name)) {
        rv_program.push_back(make_rv_inst(addr_reg, {}, {}, rv::rvOPCODE::LA, 0, op1.name));
    } else {
        addr_reg = get_reg_from_var(op1);
    }

    rv_program.push_back(make_rv_inst(index_reg, index_reg, {}, rv::rvOPCODE::SLLI, 2));
    rv_program.push_back(make_rv_inst(addr_reg, addr_reg, index_reg, rv::rvOPCODE::ADD));

    if (des.type == ir::Type::Int || des.type == ir::Type::IntPtr) {
        des_reg = get_reg_from_var(des);
        rv_program.push_back(make_rv_inst(des_reg, addr_reg, {}, rv::rvOPCODE::LW, 0));
    } else if (des.type == ir::Type::Float) {
        rv::rvREG fdes_reg = get_reg_from_var(des);
        rv_program.push_back(make_rv_inst(fdes_reg, addr_reg, {}, rv::rvOPCODE::FLW, 0));
    } else if (des.type == ir::Type::FloatPtr) {
        des_reg = get_reg_from_var(des);
        rv_program.push_back(make_rv_inst(des_reg, addr_reg, {}, rv::rvOPCODE::LW, 0));
    } else {
        assert(false && "Unsupported type for load operation");
    }
}

void backend::Generator::gen_getptr(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;
    rv::rvREG addr_reg = t[0];
    rv::rvREG index_reg = t[1];
    rv::rvREG des_reg = get_reg_from_var(des);

    if (op2.type == ir::Type::IntLiteral) {
        rv_program.push_back(make_rv_inst(index_reg, {}, {}, rv::rvOPCODE::LI, std::stoi(op2.name)));
    } else {
        if (is_global(op2.name)) {
            rv_program.push_back(make_rv_inst(t[2], {}, {}, rv::rvOPCODE::LA, 0, op2.name));
            rv_program.push_back(make_rv_inst(index_reg, t[2], {}, rv::rvOPCODE::LW, 0));
        } else {
            index_reg = get_reg_from_var(op2);  // get the register for op2
        }
    }

    if (is_global(op1.name)) {
        rv_program.push_back(make_rv_inst(addr_reg, {}, {}, rv::rvOPCODE::LA, 0, op1.name));
    } else {
        addr_reg = get_reg_from_var(op1);
    }

    rv_program.push_back(make_rv_inst(index_reg, index_reg, {}, rv::rvOPCODE::SLLI, 2));
    rv_program.push_back(make_rv_inst(addr_reg, addr_reg, index_reg, rv::rvOPCODE::ADD));
    rv_program.push_back(make_rv_inst(des_reg, addr_reg, {}, rv::rvOPCODE::MV));
}

// toString functions for rvOPCODE and rvREG
namespace rv {

    std::string toString(rvREG r) {
        int reg = static_cast<int>(r);
        if (reg >= 0 && reg <= 31) {
            static const char *abi_names[32] = {"zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2", "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"};
            return abi_names[reg];
        } else if (reg >= 32 && reg <= 63) {
            return "f" + std::to_string(reg - 32);
        }
        return "unknown_register";
    }

    std::string toString(rvOPCODE op) {
        static const char *opcodeStrings[] = {// RV32I Base Instruction Set (47 instructions)
                                              "lui", "auipc", "jal", "jalr", "beq", "bne", "blt", "bge", "bltu", "bgeu", "lb", "lh", "lw", "lbu", "lhu", "sb", "sh", "sw", "addi", "slti", "sltiu", "xori", "ori", "andi", "slli", "srli", "srai", "add", "sub", "sll", "slt", "sltu", "xor", "srl", "sra", "or", "and", "fence", "fence.i", "ecall", "ebreak", "csrrw", "csrrs", "csrrc", "csrrwi", "csrrsi", "csrrci",

                                              // RV32M Standard Extension (8 instructions)
                                              "mul", "mulh", "mulhsu", "mulhu", "div", "divu", "rem", "remu",

                                              // RV32F Standard Extension (32 instructions)
                                              "flw", "fsw", "fmadd.s", "fmsub.s", "fnmsub.s", "fnmadd.s", "fadd.s", "fsub.s", "fmul.s", "fdiv.s", "fsqrt.s", "fsgnj.s", "fsgnjn.s", "fsgnjx.s", "fmin.s", "fmax.s", "fcvt.w.s", "fcvt.wu.s", "fcvt.s.w", "fcvt.s.wu", "fmv.x.s", "fmv.s.x", "feq.s", "flt.s", "fle.s", "fclass.s",

                                              // RV32D Standard Extension (32 instructions)
                                              "fld", "fsd", "fmadd.d", "fmsub.d", "fnmsub.d", "fnmadd.d", "fadd.d", "fsub.d", "fmul.d", "fdiv.d", "fsqrt.d", "fsgnj.d", "fsgnjn.d", "fsgnjx.d", "fmin.d", "fmax.d", "fcvt.s.d", "fcvt.d.s", "fcvt.w.d", "fcvt.wu.d", "fcvt.d.w", "fcvt.d.wu", "fmv.x.d", "fmv.d.x", "feq.d", "flt.d", "fle.d", "fclass.d",

                                              // Pseudo-instructions (30 instructions)
                                              "nop", "li", "la", "mv", "not", "neg", "j", "ret", "call", "tail", "seqz", "snez", "sltz", "sgtz", "fmv.s", "fmv.d", "frcsr", "fscsr", "frrm", "fsrm", "frflags", "fsflags",

                                              // Special value (1 instruction)
                                              "unknown"};

        int index = static_cast<int>(op);
        if (index < 0 || index >= 150) {
            return "unknown";
        }
        return opcodeStrings[index];
    }

}  // namespace rv