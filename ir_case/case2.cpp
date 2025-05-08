#include "front/lexical.h"
#include "front/semantic.h"
#include "front/syntax.h"
#include "ir/ir.h"
#include "tools/ir_executor.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using std::string;
using std::vector;

int main(int argc, char **argv) {
    ir::Program program;
    ir::Function globalFunc("global", ir::Type::null);
    
    // 分配二维数组空间 (10x10 = 100个元素)
    ir::Instruction allocInst(ir::Operand("100", ir::Type::IntLiteral),
                             ir::Operand(), 
                             ir::Operand("a", ir::Type::IntPtr),
                             ir::Operator::alloc);
    
    ir::Instruction globalreturn(ir::Operand(), ir::Operand(), ir::Operand(),
                               ir::Operator::_return);
    
    globalFunc.addInst(&allocInst);
    globalFunc.addInst(&globalreturn);
    
    // 全局变量声明 (100个元素的数组)
    program.globalVal.emplace_back(ir::Operand("a", ir::Type::IntPtr), 100);
    
    program.addFunction(globalFunc);
    
    // main函数
    ir::Function mainFunction("main", ir::Type::Int);
    
    // 调用全局初始化函数
    ir::CallInst callGlobal(ir::Operand("global", ir::Type::null),
                            ir::Operand("t0", ir::Type::null));
    
    // 返回0
    ir::Instruction returnInst(ir::Operand("0", ir::Type::IntLiteral), 
                              ir::Operand(),
                              ir::Operand(), 
                              ir::Operator::_return);
    
    mainFunction.addInst(&callGlobal);
    mainFunction.addInst(&returnInst);
    
    program.addFunction(mainFunction);
    
    std::string src = "res";
    auto output_file_name = src + "out";
    auto input_file_name = src + "in";
    ir::reopen_output_file = fopen(output_file_name.c_str(), "w");
    ir::reopen_input_file = fopen(input_file_name.c_str(), "r");
    
    auto executor = ir::Executor(&program);
    std::cout << program.draw()
              << "--------------------------- Executor::run() "
                 "---------------------------"
              << std::endl;
    fprintf(ir::reopen_output_file, "\n%d\n", (uint8_t)executor.run());
    
    return 0;
}