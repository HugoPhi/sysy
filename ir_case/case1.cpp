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

  ir::Instruction assignInst(ir::Operand("0", ir::Type::IntLiteral),
                             ir::Operand(), ir::Operand("a", ir::Type::Int),
                             ir::Operator::def);
  globalFunc.addInst(&assignInst);

  ir::Instruction allocInst(ir::Operand("2", ir::Type::IntLiteral),
                            ir::Operand(), ir::Operand("arr", ir::Type::IntPtr),
                            ir::Operator::alloc);
  globalFunc.addInst(&allocInst);

  ir::Instruction storeInst(ir::Operand("arr", ir::Type::IntPtr),
                            ir::Operand("0", ir::Type::IntLiteral),
                            ir::Operand("2", ir::Type::IntLiteral),
                            ir::Operator::store);
  ir::Instruction storeInst1(ir::Operand("arr", ir::Type::IntPtr),
                             ir::Operand("1", ir::Type::IntLiteral),
                             ir::Operand("4", ir::Type::IntLiteral),
                             ir::Operator::store);
  ir::Instruction globalreturn(ir::Operand(), ir::Operand(), ir::Operand(),
                               ir::Operator::_return);
  globalFunc.addInst(&storeInst);
  globalFunc.addInst(&storeInst1);
  globalFunc.addInst(&globalreturn);

  program.globalVal.emplace_back(ir::Operand("a", ir::Type::Int));
  program.globalVal.emplace_back(ir::Operand("arr", ir::Type::IntPtr), 2);

  program.addFunction(globalFunc);

  std::vector<ir::Operand> paraVec = {ir::Operand("p", ir::Type::Int)};
  ir::Function funcFunction("func", paraVec, ir::Type::Int);

  ir::Instruction subInst(ir::Operand("p", ir::Type::Int),
                          ir::Operand("1", ir::Type::IntLiteral),
                          ir::Operand("t1", ir::Type::Int), ir::Operator::subi);
  ir::Instruction movInst(ir::Operand("t1", ir::Type::Int), ir::Operand(),
                          ir::Operand("p", ir::Type::Int), ir::Operator::mov);
  ir::Instruction returnInst(ir::Operand("p", ir::Type::Int), ir::Operand(),
                             ir::Operand(), ir::Operator::_return);
  funcFunction.addInst(&subInst);
  funcFunction.addInst(&movInst);
  funcFunction.addInst(&returnInst);

  program.addFunction(funcFunction);
  ir::Function mainFunction("main", ir::Type::Int);
  ir::CallInst callGlobal(ir::Operand("global", ir::Type::null),
                          ir::Operand("t0", ir::Type::null));
  ir::Instruction defInst(ir::Operand("0", ir::Type::IntLiteral), ir::Operand(),
                          ir::Operand("b", ir::Type::Int), ir::Operator::def);
  ir::Instruction loadInst(ir::Operand("arr", ir::Type::IntPtr),
                           ir::Operand("1", ir::Type::IntLiteral),
                           ir::Operand("t2", ir::Type::Int),
                           ir::Operator::load);
  ir::Instruction movInst1(ir::Operand("t2", ir::Type::Int), ir::Operand(),
                           ir::Operand("a", ir::Type::Int), ir::Operator::mov);

  std::vector<ir::Operand> paraVec1 = {ir::Operand("a", ir::Type::Int)};
  ir::CallInst callInst(ir::Operand("func", ir::Type::Int), paraVec1,
                        ir::Operand("t2", ir::Type::Int));
  ir::Instruction movInst2(ir::Operand("t2", ir::Type::Int), ir::Operand(),
                           ir::Operand("b", ir::Type::Int), ir::Operator::mov);
  ir::Instruction lssInst(ir::Operand("b", ir::Type::Int),
                          ir::Operand("a", ir::Type::Int),
                          ir::Operand("t3", ir::Type::Int), ir::Operator::lss);
  ir::Instruction gotoInst(ir::Operand("t3", ir::Type::Int), ir::Operand(),
                           ir::Operand("2", ir::Type::IntLiteral),
                           ir::Operator::_goto);
  ir::Instruction gotoInst1(ir::Operand(), ir::Operand(),
                            ir::Operand("4", ir::Type::IntLiteral),
                            ir::Operator::_goto);
  ir::Instruction defInst2(ir::Operand("2", ir::Type::IntLiteral),
                           ir::Operand(), ir::Operand("t4", ir::Type::Int),
                           ir::Operator::def);
  ir::Instruction mulInst(ir::Operand("b", ir::Type::Int),
                          ir::Operand("t4", ir::Type::Int),
                          ir::Operand("t5", ir::Type::Int), ir::Operator::mul);
  ir::Instruction movInst3(ir::Operand("t5", ir::Type::Int), ir::Operand(),
                           ir::Operand("b", ir::Type::Int), ir::Operator::mov);
  ir::Instruction returnInst1(ir::Operand("b", ir::Type::Int), ir::Operand(),
                              ir::Operand(), ir::Operator::_return);
  mainFunction.addInst(&callGlobal);
  mainFunction.addInst(&defInst);
  mainFunction.addInst(&loadInst);
  mainFunction.addInst(&movInst1);
  mainFunction.addInst(&callInst);
  mainFunction.addInst(&movInst2);
  mainFunction.addInst(&lssInst);
  mainFunction.addInst(&gotoInst);
  mainFunction.addInst(&gotoInst1);
  mainFunction.addInst(&defInst2);
  mainFunction.addInst(&mulInst);
  mainFunction.addInst(&movInst3);
  mainFunction.addInst(&returnInst1);
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
