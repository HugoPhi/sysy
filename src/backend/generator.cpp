#include "backend/generator.h"

#include <assert.h>
#include <string.h>

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
#define FLOAT(value) fout << "\t.float\t" << value << "\n"
#define LABEL(name) fout << name << ":\n"

namespace ir {
    bool operator<(const ir::Operand &lhs, const ir::Operand &rhs) { return lhs.name < rhs.name || (lhs.name == rhs.name && lhs.type < rhs.type); }
}  // namespace ir

backend::Generator::Generator(ir::Program &p, std::ofstream &f) : program(p), fout(f) {}

int backend::stackVarMap::add_operand(std::string var, int offset) {
    if (_table.find(var) != _table.end())  // already in the map
        return _table[var];

    _table[var] = offset;
    return offset;
}

int backend::stackVarMap::find_operand(std::string var) {
    if (_table.find(var) == _table.end()) {
        return -1;  // not found
    }
    return _table[var];
}

rv::rvREG backend::Generator::get_reg_from_var(ir::Operand &op) {
    if (op.type == ir::Type::Int || op.type == ir::Type::IntPtr || op.type == ir::Type::FloatPtr) {
        auto it = reg_pool.begin();
        rv::rvREG reg = *it;
        reg_pool.erase(it);
        if (!is_global(op.name)) {
            int offset = var2offset.find_operand(op.name);                                  // find the offset of this operand
            if (offset != -1) {                                                             // if the operand is in stack, load it into the register
                rv_program.push_back(make_rv_inst(reg, fp, {}, rv::rvOPCODE::LW, offset));  // lw reg, offset(sp)
            } else {
                std::cerr << "Error: Operand not found in stack: " << op.name << std::endl;
                assert(false && "Operand not found in stack");  // if not in stack, it should be a global variable
            }
        } else {
            rv_program.push_back(make_rv_inst(reg, {}, {}, rv::rvOPCODE::LA, 0, op.name));
            rv_program.push_back(make_rv_inst(reg, reg, {}, rv::rvOPCODE::LW, 0));
        }
        return reg;
    } else if (op.type == ir::Type::Float) {
        auto it = freg_pool.begin();
        rv::rvREG freg = *it;
        freg_pool.erase(it);
        if (!is_global(op.name)) {
            int offset = var2offset.find_operand(op.name);                                    // find the offset of this operand
            if (offset != -1) {                                                               // if the operand is in stack, load it into the floating-point register
                rv_program.push_back(make_rv_inst(freg, fp, {}, rv::rvOPCODE::FLW, offset));  // flw freg, offset(sp)
            } else {
                assert(false && "Operand not found in stack");  // if not in stack, it should be a global variable
            }
        } else {
            rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LA, 0, op.name));
            rv_program.push_back(make_rv_inst(freg, t[0], {}, rv::rvOPCODE::FLW, 0));
        }
        return freg;
    }
}

void backend::Generator::drop_reg(rv::rvREG reg, ir::Operand &op) {
    // drop a register from the register pool
    if (reg >= rv::rvREG::X5 && reg <= rv::rvREG::X31 || reg == rv::rvREG::X1) {
        reg_pool.insert(reg);  // integer register
        if (is_global(op.name)) {
            // get addr to t[0]
            rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LA, 0, op.name));  // la t[0], op.name
            rv_program.push_back(make_rv_inst({}, t[0], reg, rv::rvOPCODE::SW, 0));
        } else {
            int offset = var2offset.find_operand(op.name);                              // find the offset of this operand
            rv_program.push_back(make_rv_inst({}, fp, reg, rv::rvOPCODE::SW, offset));  // sw reg, offset(sp)
        }
    } else if (reg >= rv::rvREG::F0 && reg <= rv::rvREG::F31) {
        freg_pool.insert(reg);  // floating-point register
        if (is_global(op.name)) {
            // get addr to t[0]
            rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LA, 0, op.name));  // la t[0], op.name
            rv_program.push_back(make_rv_inst({}, t[0], reg, rv::rvOPCODE::FSW, 0));
        } else {
            int offset = var2offset.find_operand(op.name);                               // find the offset of this operand
            rv_program.push_back(make_rv_inst({}, fp, reg, rv::rvOPCODE::FSW, offset));  // fsw freg, offset(sp)
        }
    } else {
        std::cerr << "Error: Invalid register to drop: " << static_cast<int>(reg) << std::endl;
        assert(false && "Invalid register to drop");
    }
}

rv::rv_inst backend::Generator::make_rv_inst(rv::rvREG rd, rv::rvREG rs1, rv::rvREG rs2, rv::rvOPCODE op, int imm, const std::string &label) { return {rd, rs1, rs2, op, imm, label}; }

void backend::Generator::realloc_stack_frame() {
    rv_program.clear();  // clear the previous instructions
    // var2freg = std::vector<std::pair<std::string, rv::rvREG>>();
    // var2reg = std::vector<std::pair<std::string, rv::rvREG>>();
    reg_pool.clear();                            // clear the register pool
    freg_pool.clear();                           // clear the floating-point register pool
    reg_pool.insert(ilru.begin(), ilru.end());   // fill the register pool with integer registers
    freg_pool.insert(flru.begin(), flru.end());  // fill the floating-point register pool with floating-point registers
    var2offset = stackVarMap();

    var_in_program = std::vector<ir::Operand>();
    pc2label = std::map<int, std::string>();
    ir4pc = 0;  // reset pc for ir
    stack_size = 0;
    std::cout << "in realloc: " << stack_size << std::endl;
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
    if (name.substr(0, 3) == "s0_") {
        return true;  // global variables start with "glo"
    }
    return false;
}

void backend::Generator::gen_func(const ir::Function &func) {
    GLOBAL(func.name);  // declare the function as global
    TYPE(func.name, "@function");
    // prologue
    LABEL(func.name);

    // collect vars
    std::set<ir::Operand> var_set;  // use a set to avoid duplicates
    for (auto param : func.ParameterList) {
        if (param.type == ir::Type::Int || param.type == ir::Type::IntPtr || param.type == ir::Type::FloatPtr || param.type == ir::Type::Float) {
            var_set.insert(param);  // add the parameter to the set
        }
    }
    for (const ir::Instruction *instr : func.InstVec) {
        auto [op1, op2, des, op] = *instr;  // structured binding to get des, op1, op2, op
        if (op1.type == ir::Type::Int || op1.type == ir::Type::IntPtr || op1.type == ir::Type::FloatPtr || op1.type == ir::Type::Float) {
            if (!is_global(op1.name)) {
                var_set.insert(op1);  // add the first operand to the set
            }
        }
        if (op2.type == ir::Type::Int || op2.type == ir::Type::IntPtr || op2.type == ir::Type::FloatPtr || op2.type == ir::Type::Float) {
            if (!is_global(op2.name)) {
                var_set.insert(op2);  // add the second operand to the set
            }
        }
        if (des.type == ir::Type::Int || des.type == ir::Type::IntPtr || des.type == ir::Type::FloatPtr || des.type == ir::Type::Float) {
            if (!is_global(des.name)) {
                var_set.insert(des);  // add the destination operand to the set
            }
        }
    }

    realloc_stack_frame();  // reallocate the stack frame for this function

    stack_size += 4;  // for fp reg
    stack_size += 4;  // for ra reg
    std::cout << "in gen_func for fp reg: " << stack_size << std::endl;

    stack_size += 4 * (reg_pool.size() + freg_pool.size());  // allocate space for the registers on stack

    // add vars for var2offset
    for (const ir::Operand &var : var_set) {
        stack_size += 4;                                // allocate space for the variable on stack
        var2offset.add_operand(var.name, -stack_size);  // add the variable to the stack variable map
    }

    // scan goto labels
    ir4pc = 0;  // reset instruction pointer
    for (const ir::Instruction *instr : func.InstVec) {
        if (instr->op == ir::Operator::_goto) {
            auto ix = std::stoi(instr->des.name);
            if (pc2label.find(ir4pc + ix) == pc2label.end()) {
                pc2label[ir4pc + ix] = ".L" + std::to_string(cnt4ll++);  // add the label to the map, .L{cnt4ll} is a local label
            }
        }
        ir4pc++;
    }

    // add function parameters to reg tables
    int iparam = 0, fparam = 0;
    for (auto param : func.ParameterList) {
        if (param.type == ir::Type::Int || param.type == ir::Type::IntPtr || param.type == ir::Type::FloatPtr) {
            iparam++;
        } else if (param.type == ir::Type::Float) {
            fparam++;
        }
    }
    int extra_space = (iparam > 8 ? iparam - 8 : 0) + (fparam > 8 ? fparam - 8 : 0);  // calculate the extra space needed for the parameters

    int i1 = 0, i2 = 0;  // i1 for int params, i2 for float params
    for (auto param : func.ParameterList) {
        if (param.type == ir::Type::Int || param.type == ir::Type::IntPtr || param.type == ir::Type::FloatPtr) {
            if (i1 <= 7) {
                // stack_size += 4;                                                                     // allocate space for the parameter on stack
                // var2offset.add_operand(param.name, -stack_size);                                     // add the parameter to the stack variable map
                // rv_program.push_back(make_rv_inst({}, fp, a[i1++], rv::rvOPCODE::SW, -stack_size));  // lw t[i1], -stack_size(fp)
                int offset = var2offset.find_operand(param.name);                               // find the offset of this parameter
                rv_program.push_back(make_rv_inst({}, fp, a[i1++], rv::rvOPCODE::SW, offset));  // sw a[i1], offset(fp)
            } else {
                std::cout << "in gen func alloc func paramlist: " << stack_size << std::endl;  // allocate space for the parameter on stack
                int param_pos_on_stack = (i2 > 7 ? i2 - 7 : 0) + (i1 - 7);
                int offset = var2offset.find_operand(param.name);                                                            // find the offset of this parameter
                rv_program.push_back(make_rv_inst(t[0], fp, {}, rv::rvOPCODE::LW, 4 * (extra_space - param_pos_on_stack)));  // lw t[0], param_pos_on_stack(sp)
                rv_program.push_back(make_rv_inst({}, fp, t[0], rv::rvOPCODE::SW, offset));                                  // sw t[0], offset(fp)
                i1++;
            }
        } else if (param.type == ir::Type::Float) {
            if (i2 <= 7) {
                int offset = var2offset.find_operand(param.name);                                 // find the offset of this parameter
                rv_program.push_back(make_rv_inst({}, fp, fa[i2++], rv::rvOPCODE::FSW, offset));  // fsw f[i2], offset(fp)
            } else {
                std::cout << "in gen func, alloc func paramlist: " << stack_size << std::endl;                                 // allocate space for the parameter on stack
                int param_pos_on_stack = (i1 > 7 ? i1 - 7 : 0) + (i2 - 7);                                                     // calculate the position of the parameter on stack
                int offset = var2offset.find_operand(param.name);                                                              // find the offset of this parameter
                rv_program.push_back(make_rv_inst(ft[0], fp, {}, rv::rvOPCODE::FLW, 4 * (extra_space - param_pos_on_stack)));  // flw f[0], param_pos_on_stack(sp)
                rv_program.push_back(make_rv_inst({}, fp, ft[0], rv::rvOPCODE::FSW, offset));                                  // fsw f[0], offset(fp)
                i2++;
            }
        }
    }

    // generate function body
    ir4pc = 0;  // reset instruction pointer
    for (const ir::Instruction *instr : func.InstVec) {
        if (pc2label.find(ir4pc) != pc2label.end()) {
            rv_program.push_back(make_rv_inst({}, {}, {}, rv::rvOPCODE::NOP, 0, pc2label[ir4pc]));  // add a NOP instruction with the label
        }

        gen_instr(*instr, func);
        ir4pc++;
    }

    if (pc2label.find(ir4pc) != pc2label.end()) {
        rv_program.push_back(make_rv_inst({}, {}, {}, rv::rvOPCODE::NOP, 0, pc2label[ir4pc]));  // add a NOP instruction with the label
    }

    rv_program.push_back(make_rv_inst({}, {}, {}, rv::rvOPCODE::NOP, 0, "__ret__" + func.name));  // add a NOP instruction at the end

    int offset = 3;  // put after the fp & ra register
    for (auto reg : ilru) {
        rv_program.insert(rv_program.begin(), make_rv_inst({}, fp, reg, rv::rvOPCODE::SW, -(4 * offset++)));  // lw reg, offset(fp)
    }
    for (auto freg : flru) {
        rv_program.insert(rv_program.begin(), make_rv_inst({}, fp, freg, rv::rvOPCODE::FSW, -(4 * offset++)));  // flw freg, offset(fp)
    }

    // epilogue
    offset = 3;
    for (auto reg : ilru) {
        rv_program.push_back(make_rv_inst(reg, fp, {}, rv::rvOPCODE::LW, -(4 * offset++)));  // sw reg, offset(fp)
    }
    for (auto freg : flru) {
        rv_program.push_back(make_rv_inst(freg, fp, {}, rv::rvOPCODE::FLW, -(4 * offset++)));  // fsw freg, offset(fp)
    }

    std::cout << "Stack size for function " << func.name << ": " << stack_size << " bytes" << std::endl;
    rv_program.insert(rv_program.begin(), make_rv_inst(fp, sp, {}, rv::rvOPCODE::ADDI, stack_size));    // addi fp, sp, stack_size (set frame pointer)
    rv_program.insert(rv_program.begin(), make_rv_inst({}, sp, ra, rv::rvOPCODE::SW, stack_size - 8));  // sw ra, -4(fp) (save return address)
    rv_program.insert(rv_program.begin(), make_rv_inst({}, sp, fp, rv::rvOPCODE::SW, stack_size - 4));  // sw fp, (-stack_size + 4)(sp) (save frame pointer)
    rv_program.insert(rv_program.begin(), make_rv_inst(sp, sp, {}, rv::rvOPCODE::ADDI, -stack_size));   // addi sp, sp, -stack_size

    std::cout << "Stack size for function " << func.name << ": " << stack_size << " bytes" << std::endl;
    rv_program.push_back(make_rv_inst(ra, sp, {}, rv::rvOPCODE::LW, stack_size - 8));  // lw ra, -4(fp) (restore return address)
    rv_program.push_back(make_rv_inst(fp, sp, {}, rv::rvOPCODE::LW, stack_size - 4));  // lw fp, (-stack_size + 4)(sp) (restore frame pointer)
    rv_program.push_back(make_rv_inst(sp, sp, {}, rv::rvOPCODE::ADDI, stack_size));    // addi sp, sp, stack_size (restore stack pointer)
    rv_program.push_back(make_rv_inst({}, {}, {}, rv::rvOPCODE::RET));                 // jalr ra (return to caller)

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
            fout << "\t" << toString(inst.op) << "\n";
            break;

        // 2. 单操作数指令（立即数）
        case rv::rvOPCODE::LUI:
        case rv::rvOPCODE::AUIPC:
        case rv::rvOPCODE::LI:
            fout << "\t" << toString(inst.op) << "\t" << toString(inst.rd) << ", " << inst.imm << "\n";
            break;

        // 3. 单操作数指令（标签）
        case rv::rvOPCODE::J:
        case rv::rvOPCODE::CALL:
        case rv::rvOPCODE::TAIL:
            fout << "\t" << toString(inst.op) << "\t" << inst.label << "\n";
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
            fout << "\t" << toString(inst.op) << "\t" << toString(inst.rd) << ", " << toString(inst.rs1) << "\n";
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
            fout << "\t" << toString(inst.op) << "\t" << toString(inst.rd) << ", " << toString(inst.rs1) << ", " << inst.imm << "\n";
            break;

        // 6. 加载指令（偏移量格式）
        case rv::rvOPCODE::LA:
            fout << "\t" << toString(inst.op) << "\t" << toString(inst.rd) << ", " << inst.label << "\n";
            break;
        case rv::rvOPCODE::LB:
        case rv::rvOPCODE::LH:
        case rv::rvOPCODE::LW:
        case rv::rvOPCODE::LBU:
        case rv::rvOPCODE::LHU:
        case rv::rvOPCODE::FLW:
        case rv::rvOPCODE::FLD:
            fout << "\t" << toString(inst.op) << "\t" << toString(inst.rd) << ", " << inst.imm << "(" << toString(inst.rs1) << ")" << "\n";
            break;

        // 7. 存储指令（偏移量格式）
        case rv::rvOPCODE::SB:
        case rv::rvOPCODE::SH:
        case rv::rvOPCODE::SW:
        case rv::rvOPCODE::FSW:
        case rv::rvOPCODE::FSD:
            fout << "\t" << toString(inst.op) << "\t" << toString(inst.rs2) << ", " << inst.imm << "(" << toString(inst.rs1) << ")" << "\n";
            break;

        // 8. 分支指令（标签目标）
        case rv::rvOPCODE::BEQ:
        case rv::rvOPCODE::BNE:
        case rv::rvOPCODE::BLT:
        case rv::rvOPCODE::BGE:
        case rv::rvOPCODE::BLTU:
        case rv::rvOPCODE::BGEU:
            fout << "\t" << toString(inst.op) << "\t" << toString(inst.rs1) << ", " << toString(inst.rs2) << ", " << inst.label << "\n";
            break;

        // 9. 跳转链接指令（标签目标）
        case rv::rvOPCODE::JAL:
            fout << "\t" << toString(inst.op) << "\t" << toString(inst.rd) << ", " << inst.label << "\n";
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
            fout << "\t" << toString(inst.op) << "\t" << toString(inst.rd) << ", " << toString(inst.rs1) << ", " << toString(inst.rs2) << "\n";
            break;

        // 11. CSR指令
        case rv::rvOPCODE::CSRRW:
        case rv::rvOPCODE::CSRRS:
        case rv::rvOPCODE::CSRRC:
            fout << "\t" << toString(inst.op) << "\t" << toString(inst.rd) << ", " << inst.imm << ", " << toString(inst.rs1) << "\n";
            break;

        case rv::rvOPCODE::FCVT_W_S:
            fout << "\t" << toString(inst.op) << "\t" << toString(inst.rd) << ", " << toString(inst.rs1) << ", rtz\n";
            break;
        case rv::rvOPCODE::FCVT_S_W:
            fout << "\t" << toString(inst.op) << "\t" << toString(inst.rd) << ", " << toString(inst.rs1) << ", rtz\n";
            break;
        // 12. 默认情况（未知指令）
        default:
            fout << "\t# UNIMPLEMENTED OPCODE: " << toString(inst.op);
            break;
        }
    }
}

void backend::Generator::gen_instr(const ir::Instruction &inst, const ir::Function &func) {
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
        gen_return(inst, func);
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
    std::cout << "gen_mov: " << des.name << " = " << op1.name << std::endl;
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
            rv_program.push_back(make_rv_inst(reg_des, reg_op1, {}, rv::rvOPCODE::MV));  // move the value from op1 to des

            drop_reg(reg_op1, op1);  // drop the register for op1
        }
        drop_reg(reg_des, des);  // drop the register for op1
    } else {
        assert(false && "Unsupported type for mov operation");  // unsupported type
    }
    std::cout << "gen_mov: " << des.name << " = " << op1.name << " done" << std::endl;
}

void backend::Generator::gen_add(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;  // structured binding to get des, op1, op2, op
    std::cout << "gen_add: " << des.name << " = " << op1.name << " + " << op2.name << std::endl;
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

        if (!op1_is_global) {
            drop_reg(op1_reg, op1);  // drop the register for op1
        }
        if (!op2_is_global) {
            drop_reg(op2_reg, op2);  // drop the register for op2
        }
        drop_reg(des_reg, des);  // drop the register for des
    } else {
        assert(false && "Unsupported type for addition");  // unsupported type
    }

    std::cout << "gen_add: " << des.name << " = " << op1.name << " + " << op2.name << " done" << std::endl;
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

        if (!op1_is_global) {
            drop_reg(op1_reg, op1);  // drop the register for op1
        }
        if (!op2_is_global) {
            drop_reg(op2_reg, op2);  // drop the register for op2
        }
        drop_reg(des_reg, des);  // drop the register for des
    } else {
        assert(false && "Unsupported type for subtraction");  // unsupported type
    }
}

void backend::Generator::gen_mul(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;  // structured binding to get des, op1, op2, op

    std::cout << "gen_mul: " << des.name << " = " << op1.name << " * " << op2.name << std::endl;
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

        if (!op1_is_global) {
            drop_reg(op1_reg, op1);  // drop the register for op1
        }
        if (!op2_is_global) {
            drop_reg(op2_reg, op2);  // drop the register for op2
        }
        drop_reg(des_reg, des);  // drop the register for des
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

        if (!op1_is_global) {
            drop_reg(op1_reg, op1);  // drop the register for op1
        }
        if (!op2_is_global) {
            drop_reg(op2_reg, op2);  // drop the register for op2
        }
        drop_reg(des_reg, des);  // drop the register for des
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

        if (!op1_is_global) {
            drop_reg(op1_reg, op1);  // drop the register for op1
        }
        if (!op2_is_global) {
            drop_reg(op2_reg, op2);  // drop the register for op2
        }
        drop_reg(des_reg, des);  // drop the register for des
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

        if (!op1_is_global) {
            drop_reg(op1_reg, op1);  // drop the register for op1
        }
        if (!op2_is_global) {
            drop_reg(op2_reg, op2);  // drop the register for op2
        }
        drop_reg(des_reg, des);  // drop the register for des
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

        if (!op1_is_global) {
            drop_reg(op1_reg, op1);  // drop the register for op1
        }
        if (!op2_is_global) {
            drop_reg(op2_reg, op2);  // drop the register for op2
        }
        drop_reg(des_reg, des);  // drop the register for des
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

        if (!op1_is_global) {
            drop_reg(op1_reg, op1);  // drop the register for op1
        }
        if (!op2_is_global) {
            drop_reg(op2_reg, op2);  // drop the register for op2
        }
        drop_reg(des_reg, des);  // drop the register for des
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

        if (!op1_is_global) {
            drop_reg(op1_reg, op1);  // drop the register for op1
        }
        if (!op2_is_global) {
            drop_reg(op2_reg, op2);  // drop the register for op2
        }

        drop_reg(des_reg, des);  // drop the register for des
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

        if (!op1_is_global) {
            drop_reg(op1_reg, op1);  // drop the register for op1
        }
        if (!op2_is_global) {
            drop_reg(op2_reg, op2);  // drop the register for op2
        }

        drop_reg(des_reg, des);  // drop the register for des
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

        if (!op1_is_global) {
            drop_reg(op1_reg, op1);  // drop the register for op1
        }
        if (!op2_is_global) {
            drop_reg(op2_reg, op2);  // drop the register for op2
        }

        drop_reg(des_reg, des);  // drop the register for des
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

        // logic not
        rv_program.push_back(make_rv_inst(des_reg, op1_reg, {}, rv::rvOPCODE::SEQZ));  // des = (op1 == 0)

        if (!op1_is_global) {
            drop_reg(op1_reg, op1);  // drop the register for op1
        }
        drop_reg(des_reg, des);  // drop the register for des
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

            rv_program.push_back(make_rv_inst(t[0], zero, t[0], rv::rvOPCODE::SLTU));
            rv_program.push_back(make_rv_inst(t[1], zero, op2_reg, rv::rvOPCODE::SLTU));
            rv_program.push_back(make_rv_inst(des_reg, t[0], t[1], rv::rvOPCODE::OR));  // perform bitwise OR operation between t[0] and op2_reg, store result in des

            if (!op2_is_global) {
                drop_reg(op2_reg, op2);  // drop the register for op2
            }
        } else if (op2.type == ir::Type::IntLiteral) {
            rv_program.push_back(make_rv_inst(t[1], {}, {}, rv::rvOPCODE::LI, std::stoi(op2.name)));  // load immediate value into t[0]
            int op1_is_global = is_global(op1.name);                                                  // check if op1 is a global variable
            auto op1_reg = op1_is_global ? t[0] : get_reg_from_var(op1);                              // get the register for op1
            if (op1_is_global) {
                rv_program.push_back(make_rv_inst(op1_reg, {}, {}, rv::rvOPCODE::LA, 0, op1.name));  // load address of global variable into op1_reg
                rv_program.push_back(make_rv_inst(op1_reg, op1_reg, {}, rv::rvOPCODE::LW, 0));       // load the value from the address into op1_reg
            }

            rv_program.push_back(make_rv_inst(t[0], zero, op1_reg, rv::rvOPCODE::SLTU));
            rv_program.push_back(make_rv_inst(t[1], zero, t[1], rv::rvOPCODE::SLTU));
            rv_program.push_back(make_rv_inst(des_reg, t[0], t[1], rv::rvOPCODE::OR));  // perform bitwise OR operation between op1_reg and t[1], store result in des

            if (!op1_is_global) {
                drop_reg(op1_reg, op1);  // drop the register for op1
            }
        } else {
            int op1_is_global = is_global(op1.name);                      // check if op1 is a global variable
            int op2_is_global = is_global(op2.name);                      // check if op2 is a global variable
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

            rv_program.push_back(make_rv_inst(t[0], zero, op1_reg, rv::rvOPCODE::SLTU));  // t[0] = (op1 < 0)
            rv_program.push_back(make_rv_inst(t[1], zero, op2_reg, rv::rvOPCODE::SLTU));  // t[1] = (op2 < 0)
            rv_program.push_back(make_rv_inst(des_reg, t[0], t[1], rv::rvOPCODE::OR));    // perform bitwise OR operation between t[0] and t[1], store result in des

            if (!op1_is_global) {
                drop_reg(op1_reg, op1);  // drop the register for op1
            }
            if (!op2_is_global) {
                drop_reg(op2_reg, op2);  // drop the register for op2
            }
        }

        drop_reg(des_reg, des);  // drop the register for des
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

            rv_program.push_back(make_rv_inst(t[0], zero, t[0], rv::rvOPCODE::SLTU));     // t[0] = (op1 < 0)
            rv_program.push_back(make_rv_inst(t[1], zero, op2_reg, rv::rvOPCODE::SLTU));  // t[1] = (op2 < 0)
            rv_program.push_back(make_rv_inst(des_reg, t[0], t[1], rv::rvOPCODE::AND));   // perform bitwise AND operation between t[0] and op2_reg, store result in des

            if (!op2_is_global) {
                drop_reg(op2_reg, op2);  // drop the register for op2
            }

        } else if (op2.type == ir::Type::IntLiteral) {
            rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LI, std::stoi(op2.name)));  // load immediate value into t[0]
            int op1_is_global = is_global(op1.name);                                                  // check if op1 is a global variable
            auto op1_reg = op1_is_global ? t[1] : get_reg_from_var(op1);                              // get the register for op1
            if (op1_is_global) {
                rv_program.push_back(make_rv_inst(op1_reg, {}, {}, rv::rvOPCODE::LA, 0, op1.name));  // load address of global variable into op1_reg
                rv_program.push_back(make_rv_inst(op1_reg, op1_reg, {}, rv::rvOPCODE::LW, 0));       // load the value from the address into op1_reg
            }

            rv_program.push_back(make_rv_inst(t[0], zero, t[0], rv::rvOPCODE::SLTU));     // t[0] = (op2 < 0)
            rv_program.push_back(make_rv_inst(t[1], zero, op1_reg, rv::rvOPCODE::SLTU));  // t[1] = (op1 < 0)
            rv_program.push_back(make_rv_inst(des_reg, t[0], t[1], rv::rvOPCODE::AND));   // perform bitwise AND operation between op1_reg and t[0], store result in des

            if (!op1_is_global) {
                drop_reg(op1_reg, op1);  // drop the register for op1
            }
        } else {
            int op1_is_global = is_global(op1.name);                      // check if op1 is a global variable
            int op2_is_global = is_global(op2.name);                      // check if op2 is a global variable
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

            rv_program.push_back(make_rv_inst(t[0], zero, op1_reg, rv::rvOPCODE::SLTU));  // t[0] = (op1 < 0)
            rv_program.push_back(make_rv_inst(t[1], zero, op2_reg, rv::rvOPCODE::SLTU));  // t[1] = (op2 < 0)
            rv_program.push_back(make_rv_inst(des_reg, t[0], t[1], rv::rvOPCODE::AND));   // perform bitwise AND operation between t[0] and t[1], store result in des

            if (!op1_is_global) {
                drop_reg(op1_reg, op1);  // drop the register for op1
            }
            if (!op2_is_global) {
                drop_reg(op2_reg, op2);  // drop the register for op2
            }
        }

        drop_reg(des_reg, des);  // drop the register for des
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
            rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LA, 0, op1.name));  // la ft[0], "op1.name"
            rv_program.push_back(make_rv_inst(freg_des, t[0], {}, rv::rvOPCODE::FLW, 0));     // flw freg_des, 0(ft[0])
        } else if (op1.type == ir::Type::FloatLiteral) {
            // if op1 is a float literal, write it directly to the floating-point register
            std::string ieee_str = float2ieee(op1.name);
            // std::cout << "fmov: param.name: " << op1.name << ", ieee_str: " << ieee_str << std::endl;
            uint32_t float_value = std::stoul(ieee_str);
            rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LI, float_value));
            rv_program.push_back(make_rv_inst(freg_des, t[0], {}, rv::rvOPCODE::FMV_S_X));
            // rv_program.push_back(make_rv_inst(freg_des, t[0], {}, rv::rvOPCODE::FCVT_S_W));  // move the value from t[0] to freg_des
        } else {
            rv::rvREG freg_op1 = get_reg_from_var(op1);                                       // get the floating-point register for op1
            rv_program.push_back(make_rv_inst(freg_des, freg_op1, {}, rv::rvOPCODE::FMV_S));  // move the value from op1 to des

            drop_reg(freg_op1, op1);  // drop the register for op1
        }

        drop_reg(freg_des, des);  // drop the register for des
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
            rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LA, 0, op1.name));  // load address of global variable into op1_freg
            rv_program.push_back(make_rv_inst(op1_freg, t[0], {}, rv::rvOPCODE::FLW, 0));     // load the value from the address into op1_freg
        }
        if (op2_is_global) {
            rv_program.push_back(make_rv_inst(t[1], {}, {}, rv::rvOPCODE::LA, 0, op2.name));  // load address of global variable into op2_freg
            rv_program.push_back(make_rv_inst(op2_freg, t[1], {}, rv::rvOPCODE::FLW, 0));     // load the value from the address into op2_freg
        }

        rv_program.push_back(make_rv_inst(des_freg, op1_freg, op2_freg, rv::rvOPCODE::FADD_S));  // add op1 and op2, store result in des

        if (!op1_is_global) {
            drop_reg(op1_freg, op1);  // drop the register for op1
        }
        if (!op2_is_global) {
            drop_reg(op2_freg, op2);  // drop the register for op2
        }
        drop_reg(des_freg, des);  // drop the register for des
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
            rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LA, 0, op1.name));  // load address of global variable into op1_freg
            rv_program.push_back(make_rv_inst(op1_freg, t[0], {}, rv::rvOPCODE::FLW, 0));     // load the value from the address into op1_freg
        }
        if (op2_is_global) {
            rv_program.push_back(make_rv_inst(t[1], {}, {}, rv::rvOPCODE::LA, 0, op2.name));  // load address of global variable into op2_freg
            rv_program.push_back(make_rv_inst(op2_freg, t[1], {}, rv::rvOPCODE::FLW, 0));     // load the value from the address into op2_freg
        }
        rv_program.push_back(make_rv_inst(des_freg, op1_freg, op2_freg, rv::rvOPCODE::FSUB_S));  // subtract op2 from op1, store result in des

        if (!op1_is_global) {
            drop_reg(op1_freg, op1);  // drop the register for op1
        }
        if (!op2_is_global) {
            drop_reg(op2_freg, op2);  // drop the register for op2
        }
        drop_reg(des_freg, des);  // drop the register for des
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
            rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LA, 0, op1.name));  // load address of global variable into op1_freg
            rv_program.push_back(make_rv_inst(op1_freg, t[0], {}, rv::rvOPCODE::FLW, 0));     // load the value from the address into op1_freg
        }
        if (op2_is_global) {
            rv_program.push_back(make_rv_inst(t[1], {}, {}, rv::rvOPCODE::LA, 0, op2.name));  // load address of global variable into op2_freg
            rv_program.push_back(make_rv_inst(op2_freg, t[1], {}, rv::rvOPCODE::FLW, 0));     // load the value from the address into op2_freg
        }
        rv_program.push_back(make_rv_inst(des_freg, op1_freg, op2_freg, rv::rvOPCODE::FMUL_S));  // multiply op1 and op2, store result in des

        if (!op1_is_global) {
            drop_reg(op1_freg, op1);  // drop the register for op1
        }
        if (!op2_is_global) {
            drop_reg(op2_freg, op2);  // drop the register for op2
        }
        drop_reg(des_freg, des);  // drop the register for des
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
            rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LA, 0, op1.name));  // load address of global variable into op1_freg
            rv_program.push_back(make_rv_inst(op1_freg, t[0], {}, rv::rvOPCODE::FLW, 0));     // load the value from the address into op1_freg
        }
        if (op2_is_global) {
            rv_program.push_back(make_rv_inst(t[1], {}, {}, rv::rvOPCODE::LA, 0, op2.name));  // load address of global variable into op2_freg
            rv_program.push_back(make_rv_inst(op2_freg, t[1], {}, rv::rvOPCODE::FLW, 0));     // load the value from the address into op2_freg
        }
        rv_program.push_back(make_rv_inst(des_freg, op1_freg, op2_freg, rv::rvOPCODE::FDIV_S));  // divide op1 by op2, store result in des

        if (!op1_is_global) {
            drop_reg(op1_freg, op1);  // drop the register for op1
        }
        if (!op2_is_global) {
            drop_reg(op2_freg, op2);  // drop the register for op2
        }
        drop_reg(des_freg, des);  // drop the register for des
    }
}

void backend::Generator::gen_flss(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;
    rv::rvREG reg_des = get_reg_from_var(des);
    std::cout << "des.type: " << ir::toString(des.type) << std::endl;
    rv::rvREG freg_op1 = get_reg_from_var(op1);
    rv::rvREG freg_op2 = get_reg_from_var(op2);

    if (is_global(op1.name)) {
        rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LA, 0, op1.name));
        rv_program.push_back(make_rv_inst(freg_op1, t[0], {}, rv::rvOPCODE::FLW, 0));
    }

    if (is_global(op2.name)) {
        rv_program.push_back(make_rv_inst(t[1], {}, {}, rv::rvOPCODE::LA, 0, op2.name));
        rv_program.push_back(make_rv_inst(freg_op2, t[1], {}, rv::rvOPCODE::FLW, 0));
    }

    rv_program.push_back(make_rv_inst(t[0], freg_op1, freg_op2, rv::rvOPCODE::FLT_S));
    // rv_program.push_back(make_rv_inst(reg_des, t[0], {}, rv::rvOPCODE::FMV_S_X));  // sign-extend the result to 32 bits
    rv_program.push_back(make_rv_inst(reg_des, t[0], {}, rv::rvOPCODE::FCVT_S_W));  // move the result to reg_des

    drop_reg(freg_op1, op1);  // drop the register for op1
    drop_reg(freg_op2, op2);  // drop the register for op2
    drop_reg(reg_des, des);   // drop the register for des
}

void backend::Generator::gen_fleq(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;
    rv::rvREG reg_des = get_reg_from_var(des);
    rv::rvREG freg_op1 = get_reg_from_var(op1);
    rv::rvREG freg_op2 = get_reg_from_var(op2);

    if (is_global(op1.name)) {
        rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LA, 0, op1.name));
        rv_program.push_back(make_rv_inst(freg_op1, t[0], {}, rv::rvOPCODE::FLW, 0));
    }

    if (is_global(op2.name)) {
        rv_program.push_back(make_rv_inst(t[1], {}, {}, rv::rvOPCODE::LA, 0, op2.name));
        rv_program.push_back(make_rv_inst(freg_op2, t[1], {}, rv::rvOPCODE::FLW, 0));
    }

    rv_program.push_back(make_rv_inst(t[0], freg_op1, freg_op2, rv::rvOPCODE::FLE_S));
    rv_program.push_back(make_rv_inst(reg_des, t[0], {}, rv::rvOPCODE::FCVT_S_W));  // move the result to reg_des
    // rv_program.push_back(make_rv_inst(reg_des, t[0], {}, rv::rvOPCODE::FMV_S_X));  // sign-extend the result to 32 bits

    drop_reg(freg_op1, op1);  // drop the register for op1
    drop_reg(freg_op2, op2);  // drop the register for op2
    drop_reg(reg_des, des);   // drop the register for des
}

void backend::Generator::gen_fgtr(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;
    rv::rvREG reg_des = get_reg_from_var(des);
    std::cout << "des.type: " << ir::toString(des.type) << std::endl;
    rv::rvREG freg_op1 = get_reg_from_var(op1);
    rv::rvREG freg_op2 = get_reg_from_var(op2);

    if (is_global(op1.name)) {
        rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LA, 0, op1.name));
        rv_program.push_back(make_rv_inst(freg_op1, t[0], {}, rv::rvOPCODE::FLW, 0));
    }

    if (is_global(op2.name)) {
        rv_program.push_back(make_rv_inst(t[1], {}, {}, rv::rvOPCODE::LA, 0, op2.name));
        rv_program.push_back(make_rv_inst(freg_op2, t[1], {}, rv::rvOPCODE::FLW, 0));
    }

    rv_program.push_back(make_rv_inst(t[0], freg_op2, freg_op1, rv::rvOPCODE::FLT_S));
    rv_program.push_back(make_rv_inst(reg_des, t[0], {}, rv::rvOPCODE::FCVT_S_W));  // move the result to reg_des

    drop_reg(freg_op1, op1);  // drop the register for op1
    drop_reg(freg_op2, op2);  // drop the register for op2
    drop_reg(reg_des, des);   // drop the register for des
}

void backend::Generator::gen_fgeq(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;
    rv::rvREG reg_des = get_reg_from_var(des);
    rv::rvREG freg_op1 = get_reg_from_var(op1);
    rv::rvREG freg_op2 = get_reg_from_var(op2);

    if (is_global(op1.name)) {
        rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LA, 0, op1.name));
        rv_program.push_back(make_rv_inst(freg_op1, t[0], {}, rv::rvOPCODE::FLW, 0));
    }

    if (is_global(op2.name)) {
        rv_program.push_back(make_rv_inst(t[1], {}, {}, rv::rvOPCODE::LA, 0, op2.name));
        rv_program.push_back(make_rv_inst(freg_op2, t[1], {}, rv::rvOPCODE::FLW, 0));
    }

    rv_program.push_back(make_rv_inst(t[0], freg_op2, freg_op1, rv::rvOPCODE::FLE_S));
    rv_program.push_back(make_rv_inst(reg_des, t[0], {}, rv::rvOPCODE::FCVT_S_W));  // move the result to reg_des

    drop_reg(freg_op1, op1);  // drop the register for op1
    drop_reg(freg_op2, op2);  // drop the register for op2
    drop_reg(reg_des, des);   // drop the register for des
}

void backend::Generator::gen_feq(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;
    rv::rvREG reg_des = get_reg_from_var(des);
    rv::rvREG freg_op1 = get_reg_from_var(op1);
    rv::rvREG freg_op2 = get_reg_from_var(op2);

    if (is_global(op1.name)) {
        rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LA, 0, op1.name));
        rv_program.push_back(make_rv_inst(freg_op1, t[0], {}, rv::rvOPCODE::FLW, 0));
    }

    if (is_global(op2.name)) {
        rv_program.push_back(make_rv_inst(t[1], {}, {}, rv::rvOPCODE::LA, 0, op2.name));
        rv_program.push_back(make_rv_inst(freg_op2, t[1], {}, rv::rvOPCODE::FLW, 0));
    }

    rv_program.push_back(make_rv_inst(t[0], freg_op1, freg_op2, rv::rvOPCODE::FEQ_S));
    rv_program.push_back(make_rv_inst(reg_des, t[0], {}, rv::rvOPCODE::FCVT_S_W));  // move the result to reg_des

    drop_reg(freg_op1, op1);  // drop the register for op1
    drop_reg(freg_op2, op2);  // drop the register for op2
    drop_reg(reg_des, des);   // drop the register for des
}

void backend::Generator::gen_fneq(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;
    rv::rvREG reg_des = get_reg_from_var(des);
    rv::rvREG freg_op1 = get_reg_from_var(op1);
    rv::rvREG freg_op2 = get_reg_from_var(op2);

    if (is_global(op1.name)) {
        rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LA, 0, op1.name));
        rv_program.push_back(make_rv_inst(freg_op1, t[0], {}, rv::rvOPCODE::FLW, 0));
    }

    if (is_global(op2.name)) {
        rv_program.push_back(make_rv_inst(t[1], {}, {}, rv::rvOPCODE::LA, 0, op2.name));
        rv_program.push_back(make_rv_inst(freg_op2, t[1], {}, rv::rvOPCODE::FLW, 0));
    }

    rv_program.push_back(make_rv_inst(t[0], freg_op1, freg_op2, rv::rvOPCODE::FEQ_S));
    rv_program.push_back(make_rv_inst(t[1], t[0], {}, rv::rvOPCODE::XORI, 1));
    rv_program.push_back(make_rv_inst(reg_des, t[1], {}, rv::rvOPCODE::FCVT_S_W));  // move the result to reg_des

    drop_reg(freg_op1, op1);  // drop the register for op1
    drop_reg(freg_op2, op2);  // drop the register for op2
    drop_reg(reg_des, des);   // drop the register for des
}

// conversion instructions
void backend::Generator::gen_cvt_i2f(const ir::Instruction &inst) {
    // int var to float var
    auto [op1, op2, des, op] = inst;  // structured binding to get des, op1, op2, op, des is obj, op1 is int var

    auto des_freg = get_reg_from_var(des);  // get the floating-point register for des
    if (is_global(op1.name)) {
        rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LA, 0, op1.name));  // load address of global variable into ft[0]
        rv_program.push_back(make_rv_inst(t[1], t[0], {}, rv::rvOPCODE::LW, 0));          // load the value from the address into t[0]
        rv_program.push_back(make_rv_inst(des_freg, t[1], {}, rv::rvOPCODE::FCVT_W_S));
    } else {
        auto op1_reg = get_reg_from_var(op1);                                               // get the register for op1
        rv_program.push_back(make_rv_inst(des_freg, op1_reg, {}, rv::rvOPCODE::FCVT_S_W));  // move the value from op1 to t[0]

        drop_reg(op1_reg, op1);  // drop the register for op1
    }

    drop_reg(des_freg, des);  // drop the register for des
}
void backend::Generator::gen_cvt_f2i(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;       // structured binding to get des, op1, op2, op, des is obj, op1 is float var
    auto des_reg = get_reg_from_var(des);  // get the register for des
    if (is_global(op1.name)) {
        rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LA, 0, op1.name));  // load address of global variable into ft[0]
        rv_program.push_back(make_rv_inst(ft[0], t[0], {}, rv::rvOPCODE::FLW, 0));        // load the value from the address into ft[0]
        rv_program.push_back(make_rv_inst(des_reg, ft[0], {}, rv::rvOPCODE::FCVT_W_S));   // move the value from ft[0] to des
    } else {
        auto op1_freg = get_reg_from_var(op1);                                              // get the floating-point register for op1
        rv_program.push_back(make_rv_inst(des_reg, op1_freg, {}, rv::rvOPCODE::FCVT_W_S));  // move the value from op1 to des

        drop_reg(op1_freg, op1);  // drop the register for op1
    }
    drop_reg(des_reg, des);  // drop the register for des
}

// control flow instructions
void backend::Generator::gen_return(const ir::Instruction &inst, const ir::Function &func) {
    auto [op1, op2, des, op] = inst;  // structured binding to get des, op1, op2, op
    if (op1.type == ir::Type::IntLiteral) {
        rv_program.push_back(make_rv_inst(a[0], {}, {}, rv::rvOPCODE::LI, std::stoi(op1.name)));  // load immediate value into A0

        // j to the return label
        rv_program.push_back(make_rv_inst({}, {}, {}, rv::rvOPCODE::J, 0, "__ret__" + func.name));  // unconditional jump to the return label
    } else if (op1.type == ir::Type::Int) {
        auto op1_reg = get_reg_from_var(op1);                                     // get the register for op1
        rv_program.push_back(make_rv_inst(a[0], op1_reg, {}, rv::rvOPCODE::MV));  // move the value from op1 to A0
        rv_program.push_back(make_rv_inst({}, {}, {}, rv::rvOPCODE::J, 0, "__ret__" + func.name));

        drop_reg(op1_reg, op1);  // drop the register for op1
    } else if (op1.type == ir::Type::FloatLiteral) {
        std::string ieee_str = float2ieee(op1.name);
        // std::cout << "return: param.name: " << op1.name << ", ieee_str: " << ieee_str << std::endl;
        uint32_t float_value = std::stoul(ieee_str);
        rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LI, float_value));  // load immediate value into ft[0]
        rv_program.push_back(make_rv_inst(fa[0], t[0], {}, rv::rvOPCODE::FMV_S));         // move the value from ft[0] to A0
        rv_program.push_back(make_rv_inst({}, {}, {}, rv::rvOPCODE::J, 0, "__ret__" + func.name));
    } else if (op1.type == ir::Type::Float) {
        auto op1_freg = get_reg_from_var(op1);                                         // get the floating-point register for op1
        rv_program.push_back(make_rv_inst(fa[0], op1_freg, {}, rv::rvOPCODE::FMV_S));  // move the value from op1 to A0
        rv_program.push_back(make_rv_inst({}, {}, {}, rv::rvOPCODE::J, 0, "__ret__" + func.name));

        drop_reg(op1_freg, op1);  // drop the register for op1
    } else if (op1.type == ir::Type::null) {
        // do nothing
        rv_program.push_back(make_rv_inst({}, {}, {}, rv::rvOPCODE::J, 0, "__ret__" + func.name));
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

        drop_reg(cond_reg, op1);  // drop the register for op1
    } else if (op1.type == ir::Type::null) {
        rv_program.push_back(make_rv_inst({}, {}, {}, rv::rvOPCODE::J, 0, label));  // unconditional jump to the label
    } else {
        // std::cout << "Unsupported type for goto operation: " << ir::toString(op1.type) << std::endl;
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
                } else if (param.type == ir::Type::Int) {
                    if (is_global(param.name)) {
                        rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LA, 0, param.name));
                        rv_program.push_back(make_rv_inst(a[int_arg_count], t[0], {}, rv::rvOPCODE::LW, 0));
                    } else {
                        rv::rvREG param_reg = get_reg_from_var(param);
                        rv_program.push_back(make_rv_inst(a[int_arg_count], param_reg, {}, rv::rvOPCODE::MV));

                        drop_reg(param_reg, param);  // drop the register for param
                    }
                } else if (param.type == ir::Type::FloatPtr || param.type == ir::Type::IntPtr) {
                    if (is_global(param.name)) {
                        rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LA, 0, param.name));
                        rv_program.push_back(make_rv_inst(a[int_arg_count], t[0], {}, rv::rvOPCODE::MV, 0));
                    } else {
                        rv::rvREG param_reg = get_reg_from_var(param);
                        rv_program.push_back(make_rv_inst(a[int_arg_count], param_reg, {}, rv::rvOPCODE::MV));

                        drop_reg(param_reg, param);  // drop the register for param
                    }
                }
            } else {
                extra_stack_space += 4;
                if (param.type == ir::Type::IntLiteral) {
                    rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LI, std::stoi(param.name)));
                    rv_program.push_back(make_rv_inst(sp, sp, {}, rv::rvOPCODE::ADDI, -4));
                    rv_program.push_back(make_rv_inst({}, sp, t[0], rv::rvOPCODE::SW, 0));
                } else if (param.type == ir::Type::Int) {
                    if (is_global(param.name)) {
                        rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LA, 0, param.name));
                        rv_program.push_back(make_rv_inst(t[0], t[0], {}, rv::rvOPCODE::LW, 0));
                        rv_program.push_back(make_rv_inst(sp, sp, {}, rv::rvOPCODE::ADDI, -4));
                        rv_program.push_back(make_rv_inst({}, sp, t[0], rv::rvOPCODE::SW, 0));
                    } else {
                        rv::rvREG param_reg = get_reg_from_var(param);
                        rv_program.push_back(make_rv_inst(sp, sp, {}, rv::rvOPCODE::ADDI, -4));
                        rv_program.push_back(make_rv_inst({}, sp, param_reg, rv::rvOPCODE::SW, 0));

                        drop_reg(param_reg, param);  // drop the register for param
                    }
                } else if (param.type == ir::Type::FloatPtr || param.type == ir::Type::IntPtr) {
                    if (is_global(param.name)) {
                        rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LA, 0, param.name));
                        rv_program.push_back(make_rv_inst(sp, sp, {}, rv::rvOPCODE::ADDI, -4));
                        rv_program.push_back(make_rv_inst({}, sp, t[0], rv::rvOPCODE::SW, 0));
                    } else {
                        rv::rvREG param_reg = get_reg_from_var(param);
                        rv_program.push_back(make_rv_inst(sp, sp, {}, rv::rvOPCODE::ADDI, -4));
                        rv_program.push_back(make_rv_inst({}, sp, param_reg, rv::rvOPCODE::SW, 0));

                        drop_reg(param_reg, param);  // drop the register for param
                    }
                }
            }
            int_arg_count++;
        } else if (param.type == ir::Type::Float || param.type == ir::Type::FloatLiteral) {
            if (float_arg_count < 8) {
                if (param.type == ir::Type::FloatLiteral) {
                    std::string ieee_str = float2ieee(param.name);
                    // std::cout << "call: param.name: " << param.name << ", ieee_str: " << ieee_str << std::endl;
                    uint32_t float_value = std::stoul(ieee_str);
                    rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LI, float_value));
                    // rv_program.push_back(make_rv_inst(fa[float_arg_count], t[0], {}, rv::rvOPCODE::FCVT_S_W));
                    rv_program.push_back(make_rv_inst(fa[float_arg_count], t[0], {}, rv::rvOPCODE::FMV_S_X));
                } else if (is_global(param.name)) {
                    rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LA, 0, param.name));
                    rv_program.push_back(make_rv_inst(fa[float_arg_count], t[0], {}, rv::rvOPCODE::FLW, 0));
                } else {
                    rv::rvREG fparam_reg = get_reg_from_var(param);
                    rv_program.push_back(make_rv_inst(fa[float_arg_count], fparam_reg, {}, rv::rvOPCODE::FMV_S));

                    drop_reg(fparam_reg, param);  // drop the register for param
                }
            } else {
                extra_stack_space += 4;
                if (param.type == ir::Type::FloatLiteral) {
                    std::string ieee_str = float2ieee(param.name);
                    // std::cout << "call: param.name: " << param.name << ", ieee_str: " << ieee_str << std::endl;
                    uint32_t float_value = std::stoul(ieee_str);
                    rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LI, float_value));
                    rv_program.push_back(make_rv_inst(sp, sp, {}, rv::rvOPCODE::ADDI, -4));
                    rv_program.push_back(make_rv_inst({}, sp, t[0], rv::rvOPCODE::SW, 0));
                } else if (is_global(param.name)) {
                    rv_program.push_back(make_rv_inst(t[0], {}, {}, rv::rvOPCODE::LA, 0, param.name));
                    rv_program.push_back(make_rv_inst(t[1], t[0], {}, rv::rvOPCODE::LW, 0));
                    rv_program.push_back(make_rv_inst(sp, sp, {}, rv::rvOPCODE::ADDI, -4));
                    rv_program.push_back(make_rv_inst({}, sp, t[1], rv::rvOPCODE::SW, 0));
                } else {
                    rv::rvREG fparam_reg = get_reg_from_var(param);
                    rv_program.push_back(make_rv_inst(sp, sp, {}, rv::rvOPCODE::ADDI, -4));
                    rv_program.push_back(make_rv_inst({}, sp, fparam_reg, rv::rvOPCODE::FSW, 0));

                    drop_reg(fparam_reg, param);  // drop the register for param
                }
            }
            float_arg_count++;
        } else {
            // std::cout << "Unsupported type for function call parameter: " << ir::toString(param.type) << std::endl;
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

            drop_reg(des_reg, des);  // drop the register for des
        } else if (des.type == ir::Type::Float) {
            rv::rvREG fdes_reg = get_reg_from_var(des);
            rv_program.push_back(make_rv_inst(fdes_reg, fa[0], {}, rv::rvOPCODE::FMV_S));

            drop_reg(fdes_reg, des);  // drop the register for des
        }
    }
}

// memory instructions
void backend::Generator::gen_alloc(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;  // structured binding to get des, op1, op2, op
    if (des.type == ir::Type::IntPtr || des.type == ir::Type::FloatPtr) {
        int size = std::stoi(op1.name);
        auto des_reg = get_reg_from_var(des);  // get the register for des
        stack_size += size * 4;
        std::cout << "in gen alloc: " << stack_size << "arr len: " << size << std::endl;
        rv_program.push_back(make_rv_inst(des_reg, fp, {}, rv::rvOPCODE::ADDI, -stack_size));

        drop_reg(des_reg, des);  // drop the register for des
        std::cout << "quit alloc" << std::endl;
    } else {
        assert(false && "Unsupported type for allocation");  // unsupported type
    }
}

void backend::Generator::gen_store(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;
    rv::rvREG addr_reg = t[1], index_reg = t[0];
    rv::rvREG value_reg;

    std::cout << "gen_store: op1: " << op1.name << ", op2: " << op2.name << ", des: " << des.name << std::endl;
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

    rv_program.push_back(make_rv_inst(t[0], index_reg, {}, rv::rvOPCODE::SLLI, 2));  // t[0] = index_reg << 2
    rv_program.push_back(make_rv_inst(t[0], addr_reg, t[0], rv::rvOPCODE::ADD));     // t[0] = addr_reg + t[0]

    if (des.type == ir::Type::Int || des.type == ir::Type::IntLiteral || des.type == ir::Type::IntPtr || des.type == ir::Type::FloatPtr) {
        if (des.type == ir::Type::IntLiteral) {
            rv_program.push_back(make_rv_inst(t[1], {}, {}, rv::rvOPCODE::LI, std::stoi(des.name)));
            rv_program.push_back(make_rv_inst({}, t[0], t[1], rv::rvOPCODE::SW, 0));
        } else if (is_global(des.name)) {
            rv_program.push_back(make_rv_inst(t[1], {}, {}, rv::rvOPCODE::LA, 0, des.name));
            rv_program.push_back(make_rv_inst(t[1], t[1], {}, rv::rvOPCODE::LW, 0));
            rv_program.push_back(make_rv_inst({}, t[0], t[1], rv::rvOPCODE::SW, 0));
        } else {
            value_reg = get_reg_from_var(des);
            rv_program.push_back(make_rv_inst({}, t[0], value_reg, rv::rvOPCODE::SW, 0));

            drop_reg(value_reg, des);  // drop the register for des
        }
    } else if (des.type == ir::Type::Float || des.type == ir::Type::FloatLiteral) {
        if (des.type == ir::Type::FloatLiteral) {
            std::string ieee_str = float2ieee(des.name);
            // std::cout << "store: des.name: " << des.name << ", ieee_str: " << ieee_str << std::endl;
            uint32_t float_value = std::stoul(ieee_str);
            rv_program.push_back(make_rv_inst(t[1], {}, {}, rv::rvOPCODE::LI, float_value));
            // rv_program.push_back(make_rv_inst(ft[0], t[1], {}, rv::rvOPCODE::FCVT_S_W, 0));
            rv_program.push_back(make_rv_inst(ft[0], t[1], {}, rv::rvOPCODE::FMV_S_X, 0));
            rv_program.push_back(make_rv_inst({}, t[0], ft[0], rv::rvOPCODE::FSW, 0));
        } else if (is_global(des.name)) {
            rv_program.push_back(make_rv_inst(t[1], {}, {}, rv::rvOPCODE::LA, 0, des.name));
            rv_program.push_back(make_rv_inst(ft[0], t[1], {}, rv::rvOPCODE::FLW, 0));
            rv_program.push_back(make_rv_inst({}, t[0], ft[0], rv::rvOPCODE::FSW, 0));
        } else {
            rv::rvREG fvalue_reg = get_reg_from_var(des);
            rv_program.push_back(make_rv_inst({}, t[0], fvalue_reg, rv::rvOPCODE::FSW, 0));

            drop_reg(fvalue_reg, des);  // drop the register for des
        }
    } else {
        assert(false && "Unsupported type for store operation");
    }

    if (!is_global(op1.name)) {
        drop_reg(addr_reg, op1);  // drop the register for op1
    }
    if (!is_global(op2.name) && op2.type != ir::Type::IntLiteral) {
        drop_reg(index_reg, op2);  // drop the register for op2
    }
    std::cout << "quit gen_store" << std::endl;
}

void backend::Generator::gen_load(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;
    rv::rvREG addr_reg = t[1], index_reg = t[0];
    rv::rvREG des_reg;

    std::cout << "gen_load: op1: " << op1.name << ", op2: " << op2.name << ", des: " << des.name << std::endl;
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

    rv_program.push_back(make_rv_inst(t[0], index_reg, {}, rv::rvOPCODE::SLLI, 2));  // t[0] = index_reg << 2
    rv_program.push_back(make_rv_inst(t[0], addr_reg, t[0], rv::rvOPCODE::ADD));     // t[0] = addr_reg + t[0]

    if (des.type == ir::Type::Int || des.type == ir::Type::IntPtr || des.type == ir::Type::FloatPtr) {
        des_reg = get_reg_from_var(des);
        rv_program.push_back(make_rv_inst(des_reg, t[0], {}, rv::rvOPCODE::LW, 0));

        drop_reg(des_reg, des);  // drop the register for des
    } else if (des.type == ir::Type::Float) {
        rv::rvREG fdes_reg = get_reg_from_var(des);
        rv_program.push_back(make_rv_inst(fdes_reg, t[0], {}, rv::rvOPCODE::FLW, 0));

        drop_reg(fdes_reg, des);  // drop the register for des
    } else {
        assert(false && "Unsupported type for load operation");
    }

    if (!is_global(op1.name)) {
        drop_reg(addr_reg, op1);  // drop the register for op1
    }
    if (!is_global(op2.name) && op2.type != ir::Type::IntLiteral) {
        drop_reg(index_reg, op2);  // drop the register for op2
    }
}

void backend::Generator::gen_getptr(const ir::Instruction &inst) {
    auto [op1, op2, des, op] = inst;
    rv::rvREG addr_reg = t[1], index_reg = t[0];
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

    rv_program.push_back(make_rv_inst(t[0], index_reg, {}, rv::rvOPCODE::SLLI, 2));  // t[0] = index_reg << 2
    rv_program.push_back(make_rv_inst(t[0], addr_reg, t[0], rv::rvOPCODE::ADD));     // t[0] = addr_reg + t[0]
    rv_program.push_back(make_rv_inst(des_reg, t[0], {}, rv::rvOPCODE::MV));         // move the address to des_reg

    if (!is_global(op1.name)) {
        drop_reg(addr_reg, op1);  // drop the register for op1
    }
    if (!is_global(op2.name) && op2.type != ir::Type::IntLiteral) {
        drop_reg(index_reg, op2);  // drop the register for op2
    }
    drop_reg(des_reg, des);  // drop the register for des
}

// toString functions for rvOPCODE and rvREG
namespace rv {

    std::string toString(rvREG r) {
        int reg = static_cast<int>(r);
        if (reg >= 0 && reg <= 31) {
            static const char *abi_names[32] = {"zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2", "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"};
            return abi_names[reg];
        } else if (reg >= 32 && reg <= 63) {
            static const char *freg_names[32] = {"ft0", "ft1", "ft2", "ft3", "ft4", "ft5", "ft6", "ft7", "fs0", "fs1", "fa0", "fa1", "fa2", "fa3", "fa4", "fa5", "fa6", "fa7", "fs2", "fs3", "fs4", "fs5", "fs6", "fs7", "fs8", "fs9", "fs10", "fs11"};
            return freg_names[reg - 32];
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