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

    // 符号表条目（Symbol Table Entry），即符号表中的一条记录
    struct STE
    {
        ir::Operand operand;   // 操作数（变量名+变量类型）
        vector<int> dimension; // 当操作数为数组时存储数组维数，dimension大小表示维数，每个元素表示各维度上的大小
        string literalVal;     // 字面量（即该变量的值）
    };

    using map_str_ste = map<string, STE>;

    struct ScopeInfo
    {
        int cnt;           // 作用域编号
        string name;       // 作用域名称（block cnt）
        map_str_ste table; // 该作用域内操作数的原始变量名->符号表条目（map_str_ste即map<string, STE>）
    };

    // IO库函数名称到对应库函数的映射
    map<std::string, ir::Function *> *get_lib_funcs();

    // 符号表（Symbol Table）
    struct SymbolTable
    {
        vector<ScopeInfo> scope_stack;              // 一张符号表中可以存储多个作用域，因为作用域可以嵌套。scope_stack大小为1时表示在全局作用域
        map<std::string, ir::Function *> functions; // 函数名称到对应函数的映射
        int block_cnt = 0;                          // 记录当前作用域序号（虽然作用域i结束后会被弹出scope_stack，但作用域名称中的编号是只增不减的）

        void add_scope();
        void exit_scope();

        // 根据操作数的原始变量名查找最近作用域内的相应变量并返回其在该前作用域下重命名后的名字{变量名, 作用域名}
        string get_scoped_name(string id) const;

        // 根据操作数的原始变量名查找最近作用域内的相应变量并返回该操作数（重命名后的变量名+变量类型）
        ir::Operand get_operand(string id) const;

        // 根据操作数的原始变量名查找最近作用域内的符号表条目
        STE get_ste(string id) const;
    };

    // singleton class
    struct Analyzer
    {
        int tmp_cnt;                           // 临时变量计数器
        vector<ir::Instruction *> g_init_inst; // 全局变量初始化指令
        SymbolTable symbol_table;              // 全局符号表
        ir::Program ir_program;                // 程序
        ir::Function *curr_function = nullptr; // 当前函数指针

        Analyzer();
        Analyzer(const Analyzer &) = delete;
        Analyzer &operator=(const Analyzer &) = delete;

        ir::Program get_ir_program(CompUnit *);

        ir::Type analyzeBType(BType *);
        string analyzeIdent(Term *);
        
        void analyzeCompUnit(CompUnit *);
        vector<ir::Instruction *> analyzeDecl(Decl *);
        vector<ir::Instruction *> analyzeConstDecl(ConstDecl *);
        vector<ir::Instruction *> analyzeConstDef(ConstDef *, ir::Type);
        vector<ir::Instruction *> analyzeConstInitVal(ConstInitVal *, ir::Type, int, string);
        vector<ir::Instruction *> analyzeVarDecl(VarDecl *);
        vector<ir::Instruction *> analyzeVarDef(VarDef *, ir::Type);
        vector<ir::Instruction *> analyzeInitVal(InitVal *, ir::Type, int, string);

        void analyzeFuncDef(FuncDef *);
        ir::Type analyzeFuncType(FuncType *);
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

        void IntLiteral2Int(AstNode *, AstNode *, frontend::NodeType, vector<ir::Instruction *> &);
        void IntLiteral2FloatLiteral(AstNode *, AstNode *, frontend::NodeType);
        void IntLiteral2Float(AstNode *, AstNode *, frontend::NodeType, vector<ir::Instruction *> &);
        void Int2Float(AstNode *, AstNode *, frontend::NodeType, vector<ir::Instruction *> &);
        void FloatLiteral2Float(AstNode *, AstNode *, frontend::NodeType, vector<ir::Instruction *> &);
    };

}

#endif