/**
 * @file semantic.h
 * @author Yuntao Dai (d1581209858@live.com)
 * @brief
 * @version 0.1
 * @date 2023-01-06
 *
 * a Analyzer should
 * @copyright Copyright (c) 2023
 *
 */

#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "ir/ir.h"
#include "front/abstract_syntax_tree.h"

#include <map>
#include <string>
#include <vector>
using std::map;
using std::string;
using std::vector;

namespace frontend
{

    struct STE
    {
        ir::Operand operand;
        vector<int> dimension;
        string literalVal;
    };

    using map_str_ste = map<string, STE>;

    struct ScopeInfo
    {
        int cnt;
        string name;
        map_str_ste table;
    };

    map<std::string, ir::Function *> *get_lib_funcs();

    struct SymbolTable
    {
        vector<ScopeInfo> scope_stack;
        map<std::string, ir::Function *> functions;
        int scope_cnt = 0;

        void add_scope();
        void exit_scope();

        string get_scoped_name(string id) const;

        ir::Operand get_operand(string id) const;

        STE get_ste(string id) const;
    };

    // singleton class
    struct Analyzer
    {
        int tmp_cnt;
        vector<ir::Instruction *> g_init_inst;
        SymbolTable symbol_table;
        ir::Program ir_program;
        ir::Function *curr_function = nullptr;

        Analyzer();
        Analyzer(const Analyzer &) = delete;
        Analyzer &operator=(const Analyzer &) = delete;

        ir::Program get_ir_program(CompUnit *);

        ir::Type analyzeBType(BType *);

        void analyzeCompUnit(CompUnit *);
        vector<ir::Instruction *> analyzeDecl(Decl *);
        vector<ir::Instruction *> analyzeConstDecl(ConstDecl *);
        vector<ir::Instruction *> analyzeConstDef(ConstDef *, ir::Type);
        vector<ir::Instruction *> analyzeConstInitVal(ConstInitVal *, ir::Type, int, string, int base, int dim);
        vector<ir::Instruction *> analyzeVarDecl(VarDecl *);
        vector<ir::Instruction *> analyzeVarDef(VarDef *, ir::Type);
        vector<ir::Instruction *> analyzeInitVal(InitVal *, ir::Type, int, string, int base, int dim);

        void analyzeFuncDef(FuncDef *);
        ir::Operand analyzeFuncFParam(FuncFParam *root);
        vector<ir::Operand> analyzeFuncFParams(FuncFParams *);
        vector<ir::Instruction *> analyzeBlock(Block *, bool);
        vector<ir::Instruction *> analyzeBlockItem(BlockItem *);
        vector<ir::Instruction *> analyzeStmt(Stmt *);

        /**
         * @brief analyze expression, fill the value of expression into v & type into t
         * there is a simple table for {type: value}
         * Int, value is: a name of a variable after analysis
         * Float, value is: a name of a variable
         * IntPtr, value is: a name of a variable
         * FloatPtr, value is: a name of a variable
         * IntLiteral, value is: a number string, such as "123"
         * FloatLiteral, value is: a number string, such as "123.456"
         */
        vector<ir::Instruction *> analyzeExp(Exp *);
        vector<ir::Instruction *> analyzeAddExp(AddExp *);
        vector<ir::Instruction *> analyzeConstExp(ConstExp *);
        vector<ir::Instruction *> analyzeMulExp(MulExp *);
        vector<ir::Instruction *> analyzeUnaryExp(UnaryExp *);
        vector<ir::Instruction *> analyzeFuncRParams(FuncRParams *, vector<ir::Operand> &, vector<ir::Operand> &);
        vector<ir::Instruction *> analyzePrimaryExp(PrimaryExp *);
        vector<ir::Instruction *> analyzeLVal(LVal *);
        vector<ir::Instruction *> analyzeNumber(Number *);
        vector<ir::Instruction *> analyzeCond(Cond *);
        vector<ir::Instruction *> analyzeLOrExp(LOrExp *);
        vector<ir::Instruction *> analyzeLAndExp(LAndExp *);
        vector<ir::Instruction *> analyzeEqExp(EqExp *);
        vector<ir::Instruction *> analyzeRelExp(RelExp *);
    };
}

#endif