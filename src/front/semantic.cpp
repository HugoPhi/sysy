#include "front/semantic.h"

#include <cassert>
#include <iostream>

using ir::Function;
using ir::Instruction;
using ir::Operand;
using ir::Operator;

#define TODO assert(0 && "TODO");

// 这玩意没用，因为有可能会出入更多的内容。不如手写。
// #define ANALYSIS(node, type, index)                          \
//     auto node = dynamic_cast<type *>(root->children[index]); \
//     assert(node);                                            \
//     analysis##type(node, buffer);

#define FILL_NODE(_to, _v, _t) \
    (_to)->v = (_v);           \
    (_to)->t = (_t);
#define COPY_EXP_NODE(_to, _from) \
    (_to)->v = (_from)->v;        \
    (_to)->t = (_from)->t;
#define GET_CHILD_PTR(node, type, index) type *node = dynamic_cast<type *>(root->children[index])
#define GET_NEXT_TEMP_VAR() "__temp_var" + std::to_string(tmp_cnt++)
#define INSERT_IRS(_target, _insts) (_target).insert((_target).end(), (_insts).begin(), (_insts).end())
#define SCOPED(_id) symbol_table.get_scoped_name(_id)

map<std::string, ir::Function *> *frontend::get_lib_funcs()
{
    static map<std::string, ir::Function *> lib_funcs = {
        {"getint", new Function("getint", Type::Int)},
        {"getch", new Function("getch", Type::Int)},
        {"getfloat", new Function("getfloat", Type::Float)},
        {"getarray", new Function("getarray", {Operand("arr", Type::IntPtr)}, Type::Int)},
        {"getfarray", new Function("getfarray", {Operand("arr", Type::FloatPtr)}, Type::Int)},
        {"putint", new Function("putint", {Operand("i", Type::Int)}, Type::null)},
        {"putch", new Function("putch", {Operand("i", Type::Int)}, Type::null)},
        {"putfloat", new Function("putfloat", {Operand("f", Type::Float)}, Type::null)},
        {"putarray", new Function("putarray", {Operand("n", Type::Int), Operand("arr", Type::IntPtr)}, Type::null)},
        {"putfarray", new Function("putfarray", {Operand("n", Type::Int), Operand("arr", Type::FloatPtr)}, Type::null)},
    };
    return &lib_funcs;
}

void frontend::SymbolTable::add_scope()
{
    ScopeInfo scope_info;
    scope_info.cnt = scope_cnt;                              // id of current scope
    scope_info.name = "scope" + std::to_string(scope_cnt++); // scope name, scope0, scope1, ..., number will not reuse
    scope_stack.push_back(scope_info);
}

void frontend::SymbolTable::exit_scope()
{
    scope_stack.pop_back();
}

string frontend::SymbolTable::get_scoped_name(string id) const
{
    for (int i = scope_stack.size() - 1; i >= 0; i--)
    {
        if (scope_stack[i].table.find(id) != scope_stack[i].table.end())
            return scope_stack[i].name + id; // get the scoped name of the variable
    }

    return string(); // if not found, return empty string
}

Operand frontend::SymbolTable::get_operand(string id) const
{
    for (int i = scope_stack.size() - 1; i >= 0; i--)
    {
        auto find_res = scope_stack[i].table.find(id);
        if (find_res != scope_stack[i].table.end())
            return find_res->second.operand; // get the operand of the variable in the nearest scope
    }

    return Operand(); // if not found, return empty operand
}

frontend::STE frontend::SymbolTable::get_ste(string id) const
{
    for (int i = scope_stack.size() - 1; i >= 0; i--)
    {
        auto find_res = scope_stack[i].table.find(id);
        if (find_res != scope_stack[i].table.end())
            return find_res->second; // get the STE of the variable in the nearest scope
    }

    return STE(); // if not found, return empty STE
}

frontend::Analyzer::Analyzer() : tmp_cnt(0), symbol_table()
{
    symbol_table.add_scope();                                               // add global scope
    ir::Function *global_func = new ir::Function("global", ir::Type::null); // add global function to ir_program
    symbol_table.functions["global"] = global_func;

    auto lib_funcs = *get_lib_funcs(); // add lib_funcs to global symbol table
    for (auto it = lib_funcs.begin(); it != lib_funcs.end(); it++)
        symbol_table.functions[it->first] = it->second;
}

ir::Program frontend::Analyzer::get_ir_program(CompUnit *root)
{
    analyzeCompUnit(root); // generate IR instructions

    for (auto it = symbol_table.scope_stack[0].table.begin(); it != symbol_table.scope_stack[0].table.end(); it++)
    { // add global variables to ir_program
        if (it->second.dimension.size() != 0)
        { // Array
            int arr_len = 1;
            for (int i = 0; i < it->second.dimension.size(); i++)
                arr_len *= it->second.dimension[i];
            ir_program.globalVal.push_back({{SCOPED(it->second.operand.name), it->second.operand.type}, arr_len});
        }
        else
        { // Variable
            ir_program.globalVal.push_back({{SCOPED(it->second.operand.name), it->second.operand.type}});
        }
    }

    // add return instruction to global function
    ir::Instruction *globalreturn = new ir::Instruction(ir::Operand(), ir::Operand(), ir::Operand(), ir::Operator::_return);
    symbol_table.functions["global"]->addInst(globalreturn);
    ir_program.addFunction(*symbol_table.functions["global"]);

    return ir_program;
}

// Ident -> 'IDENFR'
string frontend::Analyzer::analyzeIdent(Term *root)
{
    return root->token.value;
}

// BType -> 'int' | 'float'
ir::Type frontend::Analyzer::analyzeBType(BType *root)
{
    GET_CHILD_PTR(term, Term, 0);
    if (term->token.type == TokenType::INTTK)
    { // 'int'
        return ir::Type::Int;
    }
    else if (term->token.type == TokenType::FLOATTK)
    { // 'float'
        return ir::Type::Float;
    }

    return ir::Type::null; // should not reach here
}

// CompUnit -> (Decl | FuncDef) [CompUnit]
void frontend::Analyzer::analyzeCompUnit(CompUnit *root)
{
    if (GET_CHILD_PTR(decl, Decl, 0))
    {                                                        // Decl
        vector<ir::Instruction *> insts = analyzeDecl(decl); // get IR instructions
        for (auto inst : insts)
            symbol_table.functions["global"]->addInst(inst); // add to global function, because decl is global
    }
    else if (GET_CHILD_PTR(funcdef, FuncDef, 0))
    { // FuncDef
        analyzeFuncDef(funcdef);
    }

    if (root->children.size() > 1)
    { // if size of children > 1, then there is a CompUnit
        GET_CHILD_PTR(sub_compunit, CompUnit, 1);
        analyzeCompUnit(sub_compunit);
    }
}

// Decl -> ConstDecl | VarDecl
vector<ir::Instruction *> frontend::Analyzer::analyzeDecl(Decl *root)
{
    if (GET_CHILD_PTR(constdecl, ConstDecl, 0))
    { // ConstDecl
        return analyzeConstDecl(constdecl);
    }
    else if (GET_CHILD_PTR(vardecl, VarDecl, 0))
    { // VarDecl
        return analyzeVarDecl(vardecl);
    }

    return vector<ir::Instruction *>(); // should not reach here
}

// ConstDecl -> 'const' BType ConstDef { ',' ConstDef } ';'
vector<ir::Instruction *> frontend::Analyzer::analyzeConstDecl(ConstDecl *root)
{
    vector<ir::Instruction *> res;
    GET_CHILD_PTR(btype, BType, 1);
    ir::Type type = analyzeBType(btype);
    for (int i = 2; i < root->children.size(); i += 2)
    { // ConstDef { ',' ConstDef } ';'  <==> { ConstDef, ','|';' }
        GET_CHILD_PTR(constdef, ConstDef, i);
        vector<Instruction *> insts = analyzeConstDef(constdef, type); // get IR instructions of Const Var: type
        INSERT_IRS(res, insts);
    }

    return res;
}

// ConstDef -> Ident { '[' ConstExp ']' } '=' ConstInitVal
vector<ir::Instruction *> frontend::Analyzer::analyzeConstDef(ConstDef *root, ir::Type type)
{
    vector<ir::Instruction *> res;
    GET_CHILD_PTR(term, Term, 0);
    string var_flag = analyzeIdent(term); // var name

    // my strategy: use length of children to distinguish between different types, only 1 dim array and 0 dim array support.
    if (root->children.size() == 3)
    { // no arr， Ident '=' ConstInitVal
        STE var_ste;
        if (type == ir::Type::Int)
            var_ste.operand = ir::Operand(var_flag, ir::Type::IntLiteral);
        else if (type == ir::Type::Float)
            var_ste.operand = ir::Operand(var_flag, ir::Type::FloatLiteral);
        else
            ;

        symbol_table.scope_stack.back().table[var_flag] = var_ste;                     // fuck into nearest scope in ST
        ConstInitVal *constinit = dynamic_cast<ConstInitVal *>(root->children.back()); // ConstInitVal
        ConstExp *constexp = dynamic_cast<ConstExp *>(constinit->children[0]);         // ConstInitVal -> ConstExp
        analyzeConstExp(constexp);                                                     // calculate v of constexp

        // ste: scope[var].v = constexp.v
        symbol_table.scope_stack.back().table[var_flag].literalVal = constexp->v;
        if (type == ir::Type::Int)
        {
            int val;
            if (constexp->t == ir::Type::FloatLiteral)
                val = std::stof(constexp->v);
            // else if (constexp->t == ir::Type::IntLiteral)  // will always true
            else
                val = std::stoi(constexp->v);

            // ir: def var_flag##scope_id, "val"
            res.push_back(new ir::Instruction(ir::Operand(std::to_string(val), ir::Type::IntLiteral), ir::Operand(), ir::Operand(SCOPED(var_flag), ir::Type::Int), ir::Operator::def));
        }
        else if (type == ir::Type::Float)
        {
            float val = std::stof(constexp->v);

            // ir: fdef var_flag##scope_id, "val"
            res.push_back(new ir::Instruction(ir::Operand(std::to_string(val), ir::Type::FloatLiteral), ir::Operand(), ir::Operand(SCOPED(var_flag), ir::Type::Float), ir::Operator::fdef));
        }
    }
    else if (root->children.size() == 6)
    { // 1d arr: Ident '[' ConstExp ']' '=' ConstInitVal
        GET_CHILD_PTR(constexp, ConstExp, 2);
        analyzeConstExp(constexp);
        int array_size = std::stoi(constexp->v);

        STE ste;
        ir::Type curr_type = type;
        ste.dimension.push_back(array_size);
        if (curr_type == ir::Type::Int)
        {
            curr_type = ir::Type::IntPtr;
        }
        else if (curr_type == ir::Type::Float)
        {
            curr_type = ir::Type::FloatPtr;
        }

        ste.operand = ir::Operand(var_flag, curr_type);
        symbol_table.scope_stack.back().table[var_flag] = ste;

        if (symbol_table.scope_stack.size() > 1)
        {
            // ir: alloc var_flag##scope_id, "array_size"
            res.push_back(new ir::Instruction(ir::Operand(std::to_string(array_size), ir::Type::IntLiteral), ir::Operand(), ir::Operand(SCOPED(var_flag), curr_type), ir::Operator::alloc));
        }

        ConstInitVal *constinit = dynamic_cast<ConstInitVal *>(root->children.back()); // ConstInitVal -> '{' [ ConstInitVal { ',' ConstInitVal } ] '}'
        vector<Instruction *> insts = analyzeConstInitVal(constinit, curr_type, array_size, var_flag);
        INSERT_IRS(res, insts);
    }
    else if (root->children.size() == 9)
    { // 2d arr: Ident '[' ConstExp ']' '[' ConstExp ']' '=' ConstInitVal
        GET_CHILD_PTR(constexp1, ConstExp, 2);
        GET_CHILD_PTR(constexp2, ConstExp, 5);

        analyzeConstExp(constexp1);
        analyzeConstExp(constexp2);
        int array_dim1 = std::stoi(constexp1->v); // first dim
        int array_dim2 = std::stoi(constexp2->v); // second dim
        int array_size = array_dim1 * array_dim2; // total size

        STE arr_ste;
        arr_ste.dimension.push_back(array_dim1);
        arr_ste.dimension.push_back(array_dim2);
        ir::Type curr_type = type;

        if (curr_type == ir::Type::Int)
            curr_type = ir::Type::IntPtr;
        else if (curr_type == ir::Type::Float)
            curr_type = ir::Type::FloatPtr;

        arr_ste.operand = ir::Operand(var_flag, curr_type);

        symbol_table.scope_stack.back().table[var_flag] = arr_ste;

        if (symbol_table.scope_stack.size() > 1)
        { // ir: alloc var_flag##scope_id, "array_size"
            res.push_back(new ir::Instruction(ir::Operand(std::to_string(array_size), ir::Type::IntLiteral), ir::Operand(), ir::Operand(SCOPED(var_flag), curr_type), ir::Operator::alloc));
        }

        GET_CHILD_PTR(constinit, ConstInitVal, root->children.size() - 1);
        vector<Instruction *> insts = analyzeConstInitVal(constinit, curr_type, array_size, var_flag);
        INSERT_IRS(res, insts);
    }
    return res;
}

// ConstInitVal -> ConstExp | '{' [ ConstInitVal { ',' ConstInitVal } ] '}'
vector<ir::Instruction *> frontend::Analyzer::analyzeConstInitVal(ConstInitVal *root, ir::Type curr_type, int array_size, string var_flag)
{
    // ConstExp will be handled in analyzeConstDef or sub ConstInitVal, so we begin with ConstInitVal -> '{' [ ConstInitVal { ',' ConstInitVal } ] '}'
    vector<ir::Instruction *> res;
    int cnt = 0; // position of current element in array
    for (int i = 1; i < root->children.size() - 1; i += 2, cnt += 1)
    { // [ ConstInitVal { ',' ConstInitVal } ] '}' <==> { ConstInitVal ','|'}' }
        GET_CHILD_PTR(child_constinit, ConstInitVal, i);
        ConstExp *constexp = dynamic_cast<ConstExp *>(child_constinit->children[0]);

        analyzeConstExp(constexp);
        if (curr_type == ir::Type::IntPtr)
        { // int*
            int val;
            if (constexp->t == ir::Type::Float)
                // up convert: Float -> Int
                val = std::stof(constexp->v);
            else
                val = std::stoi(constexp->v);

            // ir: store var_flag##scope_id, i, "val"
            res.push_back(new ir::Instruction(ir::Operand(SCOPED(var_flag), ir::Type::IntPtr), ir::Operand(std::to_string(cnt), ir::Type::IntLiteral), ir::Operand(std::to_string(val), ir::Type::IntLiteral), ir::Operator::store));
        }
        else if (curr_type == ir::Type::FloatPtr)
        { // float*
            float val = std::stof(constexp->v);

            // ir: store var_flag##scope_id, i, "val"
            res.push_back(new ir::Instruction(ir::Operand(SCOPED(var_flag), ir::Type::FloatPtr), ir::Operand(std::to_string(cnt), ir::Type::IntLiteral), ir::Operand(std::to_string(val), ir::Type::FloatLiteral), ir::Operator::store));
        }
    }

    if (symbol_table.scope_stack.size() > 1)
    {
        for (int i = cnt; i < array_size; i++)
        {
            ir::Instruction *inst;
            if (curr_type == ir::Type::FloatPtr)
                // ir: store "0.0", var_flag##scope_id, i
                inst = new ir::Instruction(ir::Operand(SCOPED(var_flag), ir::Type::FloatPtr), ir::Operand(std::to_string(i), ir::Type::IntLiteral), ir::Operand("0.0", ir::Type::FloatLiteral), ir::Operator::store);
            else if (curr_type == ir::Type::IntPtr)
                // ir: store "0", var_flag##scope_id, i
                inst = new ir::Instruction(ir::Operand(SCOPED(var_flag), ir::Type::IntPtr), ir::Operand(std::to_string(i), ir::Type::IntLiteral), ir::Operand("0", ir::Type::IntLiteral), ir::Operator::store);

            res.push_back(inst);
        }
    }
    return res;
}

// VarDecl -> BType VarDef { ',' VarDef } ';'
vector<ir::Instruction *> frontend::Analyzer::analyzeVarDecl(VarDecl *root)
{
    vector<ir::Instruction *> res;
    GET_CHILD_PTR(btype, BType, 0);
    ir::Type type = analyzeBType(btype);

    for (int i = 1; i < root->children.size(); i += 2)
    { // VarDef { ',' VarDef } <==> { VarDef ','|';' }
        GET_CHILD_PTR(vardef, VarDef, i);
        vector<Instruction *> insts = analyzeVarDef(vardef, type);
        INSERT_IRS(res, insts);
    }
    return res;
}

// VarDef -> Ident { '[' ConstExp ']' } [ '=' InitVal ]
vector<ir::Instruction *> frontend::Analyzer::analyzeVarDef(VarDef *root, ir::Type type)
{
    vector<ir::Instruction *> res;
    GET_CHILD_PTR(term, Term, 0);
    string var_flag = analyzeIdent(term);

    if (root->children.size() == 1 || root->children.size() == 3)
    { // 0-dim array, Ident or Ident '=' InitVal
        STE ste;
        ste.operand = ir::Operand(var_flag, type);
        symbol_table.scope_stack.back().table[var_flag] = ste;

        if (InitVal *initval = dynamic_cast<InitVal *>(root->children.back()))
        { // Indent '=' InitVal
            // InitVal -> Exp
            Exp *exp = dynamic_cast<Exp *>(initval->children[0]);
            vector<Instruction *> insts = analyzeExp(exp);
            INSERT_IRS(res, insts);

            if (type == ir::Type::Int)
            { // int
                if (exp->t == ir::Type::Int || exp->t == ir::Type::IntLiteral)
                {
                    // ir: def var_flag##scope_id, "exp->v"
                    res.push_back(new ir::Instruction(ir::Operand(exp->v, exp->t), ir::Operand(), ir::Operand(SCOPED(var_flag), ir::Type::Int), ir::Operator::def));
                }
                else if (exp->t == ir::Type::Float || exp->t == ir::Type::FloatLiteral)
                { // down convert: Float -> Int
                    string tmp_intcvt_flag = GET_NEXT_TEMP_VAR();
                    if (exp->t == ir::Type::Float)
                    { // Float
                        // ir: cvt_f2i var_flag##scope_id, exp->v
                        res.push_back(new ir::Instruction(ir::Operand(exp->v, ir::Type::Float), ir::Operand(), ir::Operand(SCOPED(var_flag), ir::Type::Int), ir::Operator::cvt_f2i));
                    }
                    else
                    { // FloatLiteral
                        int val = std::stof(exp->v);
                        // ir: def var_flag##scope_id, "val"
                        res.push_back(new ir::Instruction(ir::Operand(std::to_string(val), ir::Type::IntLiteral), ir::Operand(), ir::Operand(SCOPED(var_flag), ir::Type::Int), ir::Operator::def));
                    }
                }
            }
            else
            { // float
                if (exp->t == ir::Type::IntLiteral || exp->t == ir::Type::FloatLiteral)
                { // if constexp is a literal, then we can use it directly
                    float val = std::stof(exp->v);
                    res.push_back(new ir::Instruction(ir::Operand(std::to_string(val), ir::Type::FloatLiteral), ir::Operand(), ir::Operand(SCOPED(var_flag), ir::Type::Float), ir::Operator::fdef));
                }
                else if (exp->t == ir::Type::Int)
                { // if constexp is a variable, then we need to convert it to float
                    string curr_tmp_flag = GET_NEXT_TEMP_VAR();
                    // ir: cvt_i2f var_flag##scope_id, exp->v
                    res.push_back(new ir::Instruction(ir::Operand(exp->v, ir::Type::Int), ir::Operand(), ir::Operand(SCOPED(var_flag), ir::Type::Float), ir::Operator::cvt_i2f));
                }
                else if (exp->t == ir::Type::Float)
                { // if constexp is a variable, then we can use it directly
                    res.push_back(new ir::Instruction(ir::Operand(exp->v, ir::Type::Float), ir::Operand(), ir::Operand(SCOPED(var_flag), ir::Type::Float), ir::Operator::fdef));
                }
            }
        }
        else
        { // Ident, init as "0" or "0.0"
            if (type == ir::Type::Int)
            {
                // ir: def var_flag##scope_id, "0"
                res.push_back(new ir::Instruction(ir::Operand("0", ir::Type::IntLiteral), ir::Operand(), ir::Operand(SCOPED(var_flag), ir::Type::Int), ir::Operator::def));
            }
            else if (type == ir::Type::Float)
            {
                // ir: fdef var_flag##scope_id, "0.0"
                res.push_back(new ir::Instruction(ir::Operand("0.0", ir::Type::FloatLiteral), ir::Operand(), ir::Operand(SCOPED(var_flag), ir::Type::Float), ir::Operator::fdef));
            }
        }
    }

    else if (root->children.size() == 4 || root->children.size() == 6)
    { // 1d arr: Ident '[' ConstExp ']' or Ident '[' ConstExp ']' '=' InitVal
        GET_CHILD_PTR(constexp, ConstExp, 2);
        analyzeConstExp(constexp);
        int array_size = std::stoi(constexp->v);

        STE arr_ste;
        arr_ste.dimension.push_back(array_size);
        ir::Type curr_type = type;
        if (curr_type == ir::Type::Int)
        {
            curr_type = ir::Type::IntPtr;
        }
        else if (curr_type == ir::Type::Float)
        {
            curr_type = ir::Type::FloatPtr;
        }

        arr_ste.operand = ir::Operand(var_flag, curr_type);
        symbol_table.scope_stack.back().table[var_flag] = arr_ste;

        if (symbol_table.scope_stack.size() > 1)
        {
            res.push_back(new ir::Instruction(ir::Operand(std::to_string(array_size), ir::Type::IntLiteral), ir::Operand(), ir::Operand(SCOPED(var_flag), curr_type), ir::Operator::alloc));
        }

        if (InitVal *initval = dynamic_cast<InitVal *>(root->children.back()))
        {
            vector<Instruction *> insts = analyzeInitVal(initval, curr_type, array_size, var_flag);
            INSERT_IRS(res, insts);
        }
    }

    else if (root->children.size() == 7 || root->children.size() == 9)
    { // 2d arr: Ident '[' ConstExp ']' '[' ConstExp ']' or Ident '[' ConstExp ']' '[' ConstExp ']' '=' InitVal
        GET_CHILD_PTR(constexp1, ConstExp, 2);
        GET_CHILD_PTR(constexp2, ConstExp, 5);

        analyzeConstExp(constexp1);
        analyzeConstExp(constexp2);
        int array_dim1 = std::stoi(constexp1->v); // dim 1
        int array_dim2 = std::stoi(constexp2->v); // dim 2
        int array_size = array_dim1 * array_dim2; // total size

        STE arr_ste;
        arr_ste.dimension.push_back(array_dim1);
        arr_ste.dimension.push_back(array_dim2);
        ir::Type curr_type = type;
        if (curr_type == ir::Type::Int)
        {
            curr_type = ir::Type::IntPtr;
        }
        else if (curr_type == ir::Type::Float)
        {
            curr_type = ir::Type::FloatPtr;
        }
        arr_ste.operand = ir::Operand(var_flag, curr_type);

        symbol_table.scope_stack.back().table[var_flag] = arr_ste;

        if (symbol_table.scope_stack.size() > 1)
        {
            res.push_back(new ir::Instruction(ir::Operand(std::to_string(array_size), ir::Type::IntLiteral), ir::Operand(), ir::Operand(SCOPED(var_flag), curr_type), ir::Operator::alloc));
        }

        if (InitVal *initval = dynamic_cast<InitVal *>(root->children.back()))
        { // init
            vector<Instruction *> insts = analyzeInitVal(initval, curr_type, array_size, var_flag);
            INSERT_IRS(res, insts);
        }
    }
    return res;
}

// InitVal -> Exp | '{' [ InitVal { ',' InitVal } ] '}'
vector<ir::Instruction *> frontend::Analyzer::analyzeInitVal(InitVal *root, ir::Type curr_type, int array_size, string var_flag)
{
    vector<ir::Instruction *> res;
    int cnt = 0; // number of elements in array
    for (int i = 1; i < root->children.size() - 1; i += 2, cnt += 1)
    { // [ InitVal { ',' InitVal } ] '}' <==> { InitVal ','|'}' }
        GET_CHILD_PTR(child_initval, InitVal, i);
        Exp *exp = dynamic_cast<Exp *>(child_initval->children[0]); // InitVal -> Exp
        vector<Instruction *> insts = analyzeExp(exp);
        INSERT_IRS(res, insts);
        if (curr_type == ir::Type::IntPtr)
        { // int*
            if (exp->t == Type::IntLiteral || exp->t == Type::Int)
            {
                res.push_back(new ir::Instruction(ir::Operand(SCOPED(var_flag), ir::Type::IntPtr), ir::Operand(std::to_string(cnt), ir::Type::IntLiteral), ir::Operand(exp->v, exp->t), ir::Operator::store));
            }
            else
            {
                if (exp->t == Type::FloatLiteral)
                { // FloatLiteral -> Int
                    int val = std::stof(exp->v);
                    res.push_back(new ir::Instruction(ir::Operand(SCOPED(var_flag), ir::Type::IntPtr), ir::Operand(std::to_string(cnt), ir::Type::IntLiteral), ir::Operand(std::to_string(val), Type::IntLiteral), ir::Operator::store));
                }
                else
                {
                    string tmp_floatcvt_flag = GET_NEXT_TEMP_VAR();
                    res.push_back(new ir::Instruction(ir::Operand(exp->v, ir::Type::Float), ir::Operand(), ir::Operand(tmp_floatcvt_flag, ir::Type::Int), ir::Operator::cvt_f2i));
                    res.push_back(new ir::Instruction(ir::Operand(SCOPED(var_flag), ir::Type::IntPtr), ir::Operand(std::to_string(cnt), ir::Type::IntLiteral), ir::Operand(tmp_floatcvt_flag, Type::Int), ir::Operator::store));
                }
            }
        }
        else if (curr_type == ir::Type::FloatPtr)
        { // float*
            if (exp->t == Type::IntLiteral || exp->t == Type::FloatLiteral)
            {
                float val = std::stof(exp->v);
                res.push_back(new ir::Instruction(ir::Operand(SCOPED(var_flag), ir::Type::FloatPtr), ir::Operand(std::to_string(cnt), ir::Type::IntLiteral), ir::Operand(std::to_string(val), ir::Type::FloatLiteral), ir::Operator::store));
            }
            else if (exp->t == Type::Int)
            { // Int -> Float
                string tmp_intcvt_flag = GET_NEXT_TEMP_VAR();
                res.push_back(new ir::Instruction(ir::Operand(exp->v, ir::Type::Int), ir::Operand(), ir::Operand(tmp_intcvt_flag, ir::Type::Float), ir::Operator::cvt_i2f));
                res.push_back(new ir::Instruction(ir::Operand(SCOPED(var_flag), ir::Type::FloatPtr), ir::Operand(std::to_string(cnt), ir::Type::IntLiteral), ir::Operand(tmp_intcvt_flag, ir::Type::Float), ir::Operator::store));
            }
            else if (exp->t == Type::Float)
            {
                res.push_back(new ir::Instruction(ir::Operand(SCOPED(var_flag), ir::Type::FloatPtr), ir::Operand(std::to_string(cnt), ir::Type::IntLiteral), ir::Operand(exp->v, ir::Type::Float), ir::Operator::store));
            }
        }
    }

    if (symbol_table.scope_stack.size() > 1)
    {
        for (int i = cnt; i < array_size; i++)
        {
            res.push_back(
                curr_type == ir::Type::FloatPtr
                    ? new ir::Instruction(ir::Operand(SCOPED(var_flag), ir::Type::FloatPtr), ir::Operand(std::to_string(i), ir::Type::IntLiteral), ir::Operand("0.0", ir::Type::FloatLiteral), ir::Operator::store)
                    : new ir::Instruction(ir::Operand(SCOPED(var_flag), ir::Type::IntPtr), ir::Operand(std::to_string(i), ir::Type::IntLiteral), ir::Operand("0", ir::Type::IntLiteral), ir::Operator::store));
        }
    }

    return res;
}

// FuncDef -> FuncType Ident '(' [FuncFParams] ')' Block
void frontend::Analyzer::analyzeFuncDef(FuncDef *root)
{
    GET_CHILD_PTR(_func_type, FuncType, 0);
    GET_CHILD_PTR(term, Term, 1);
    ir::Type func_type = analyzeFuncType(_func_type);
    string func_flag = analyzeIdent(term);

    symbol_table.add_scope(); // fuck into scope for function
    vector<ir::Operand> func_params;
    GET_CHILD_PTR(funcfparams, FuncFParams, 3);
    if (funcfparams != nullptr)
        func_params = analyzeFuncFParams(funcfparams);

    ir::Function *func = new ir::Function(func_flag, func_params, func_type);

    if (func_flag == "main")
    { // if is "main", add a main() call inst in global function
        func->addInst(new ir::CallInst(ir::Operand("global", ir::Type::null), ir::Operand(GET_NEXT_TEMP_VAR(), ir::Type::null)));
    }

    symbol_table.functions[func_flag] = func;

    curr_function = func;
    vector<ir::Instruction *> func_body = analyzeBlock(dynamic_cast<Block *>(root->children.back()), true); // get into block and return insts for this function

    symbol_table.exit_scope();
    for (auto inst : func_body)
    {
        func->addInst(inst);
    }

    if (func_flag == "main")
    { // main function
        // ir: return 0
        func->addInst(new ir::Instruction(ir::Operand("0", ir::Type::IntLiteral), ir::Operand(), ir::Operand(), ir::Operator::_return));
    }

    if (func_type == ir::Type::null)
    { // void function
        // ir: return  <==> return null
        func->addInst(new ir::Instruction(ir::Operand(), ir::Operand(), ir::Operand(), ir::Operator::_return));
    }

    ir_program.addFunction(*func); // add function to ir_program
}

// FuncType -> 'void' | 'int' | 'float'
ir::Type frontend::Analyzer::analyzeFuncType(FuncType *root)
{
    GET_CHILD_PTR(term, Term, 0);
    if (term->token.type == TokenType::VOIDTK)
        // 'void'
        return ir::Type::null;
    else if (term->token.type == TokenType::INTTK)
        // 'int'
        return ir::Type::Int;
    else if (term->token.type == TokenType::FLOATTK)
        // 'float'
        return ir::Type::Float;

    return ir::Type::null; // should not reach here
}

// FuncFParam -> BType Ident ['[' ']' { '[' Exp ']' }]
ir::Operand frontend::Analyzer::analyzeFuncFParam(FuncFParam *root)
{ // INFO: for arr param, most: arr[][$some_number]
    GET_CHILD_PTR(btype, BType, 0);
    GET_CHILD_PTR(term, Term, 1);
    ir::Type param_type = analyzeBType(btype);
    string param_flag = analyzeIdent(term); // param name

    vector<int> dimension = {-1}; // {-1}, if arr >= 2dim, {-1} -> {-1, $some_number, ...}

    if (root->children.size() > 2)
    { // { '[' ']' { '[' Exp ']' }
        if (param_type == ir::Type::Int)
        {
            param_type = ir::Type::IntPtr;
        }
        else if (param_type == ir::Type::Float)
        {
            param_type = ir::Type::FloatPtr;
        }

        if (root->children.size() == 4)
            ; // 1d arr

        else if (root->children.size() == 7)
        { // 2d arr
            GET_CHILD_PTR(exp, Exp, 6);
            analyzeExp(exp);
            int exp_res = std::stoi(exp->v); // get dimension
            dimension.push_back(exp_res);
        }
    }

    // add new param to symbol table
    ir::Operand param(param_flag, param_type);
    symbol_table.scope_stack.back().table[param_flag] = {param, dimension};
    // rename the param
    ir::Operand func_param(SCOPED(param.name), param.type);
    return func_param;
}

// FuncFParams -> FuncFParam { ',' FuncFParam }
vector<ir::Operand> frontend::Analyzer::analyzeFuncFParams(FuncFParams *root)
{
    vector<ir::Operand> func_params;
    for (int i = 0; i < root->children.size(); i += 2)
    { // FuncFParam { ',' FuncFParam } <==> { FuncFParam ','|None }
        GET_CHILD_PTR(funcfparam, FuncFParam, i);
        ir::Operand func_param = analyzeFuncFParam(funcfparam);
        func_params.push_back(func_param);
    }
    return func_params;
}

// Block -> '{' { BlockItem } '}'
vector<ir::Instruction *> frontend::Analyzer::analyzeBlock(Block *root, bool is_func_block)
{
    if (!is_func_block)
        symbol_table.add_scope(); // 'if', 'while', 'for' or 'else' block, add a new scope

    // { BlockItem }
    vector<ir::Instruction *> block_body;
    for (int i = 1; i < int(root->children.size()) - 1; i++)
    {
        GET_CHILD_PTR(blockitem, BlockItem, i);
        vector<ir::Instruction *> blockitem_body = analyzeBlockItem(blockitem);
        INSERT_IRS(block_body, blockitem_body);
    }

    if (!is_func_block)
        symbol_table.exit_scope(); // exit scope for 'if', 'while', 'for' or 'else' block
    return block_body;
}

// BlockItem -> Decl | Stmt
vector<ir::Instruction *> frontend::Analyzer::analyzeBlockItem(BlockItem *root)
{
    if (GET_CHILD_PTR(_, Decl, 0))
        // Decl
        return analyzeDecl(dynamic_cast<Decl *>(root->children[0]));
    else if (GET_CHILD_PTR(_, Stmt, 0))
        // Stmt
        return analyzeStmt(dynamic_cast<Stmt *>(root->children[0]));

    return vector<ir::Instruction *>();
}

// Stmt -> LVal '=' Exp ';' | Block | 'if' '(' Cond ')' Stmt [ 'else' Stmt ] | 'while' '(' Cond ')' Stmt | 'break' ';' | 'continue' ';' | 'return' [Exp] ';' | [Exp] ';'
vector<ir::Instruction *> frontend::Analyzer::analyzeStmt(Stmt *root)
{
    vector<ir::Instruction *> res;

    if (GET_CHILD_PTR(lval, LVal, 0))
    { // LVal '=' Exp ';'
        GET_CHILD_PTR(exp, Exp, 2);
        vector<ir::Instruction *> insts = analyzeExp(exp);
        INSERT_IRS(res, insts);

        string var_flag = analyzeIdent(dynamic_cast<Term *>(lval->children[0])); // LVal -> Ident { '[' Exp ']' }
        STE ident_ste = symbol_table.get_ste(var_flag);

        if (lval->children.size() == 1)
        { // LVal -> Ident
            if (ident_ste.operand.type == Type::Int)
            { // Float/Int/FloatLiteral/IntLiteral -> Int
                if (exp->t == Type::Int)
                { // ir: mov var_flag##scope_id, "exp->v"
                    res.push_back(new ir::Instruction(ir::Operand(exp->v, ir::Type::Int), ir::Operand(), ir::Operand(SCOPED(var_flag), ir::Type::Int), ir::Operator::mov));
                }
                else if (exp->t == Type::IntLiteral)
                { // ir: mov var_flag##scope_id, "exp->v"
                    res.push_back(new ir::Instruction(ir::Operand(exp->v, ir::Type::IntLiteral), ir::Operand(), ir::Operand(SCOPED(var_flag), ir::Type::Int), ir::Operator::mov));
                }
                else if (exp->t == Type::Float)
                { // ir: cvt_f2i var_flag##scope_id, "exp->v"
                    res.push_back(new ir::Instruction(ir::Operand(exp->v, ir::Type::Float), ir::Operand(), ir::Operand(SCOPED(var_flag), ir::Type::Int), ir::Operator::cvt_f2i));
                }
                else if (exp->t == Type::FloatLiteral)
                { // ir: mov var_flag##scope_id, "exp->v"
                    int val = std::stof(exp->v);
                    res.push_back(new ir::Instruction(ir::Operand(std::to_string(val), ir::Type::IntLiteral), ir::Operand(), ir::Operand(SCOPED(var_flag), ir::Type::Int), ir::Operator::mov));
                }
            }
            // Float/Int/FloatLiteral/IntLiteral -> Float
            else if (ident_ste.operand.type == Type::Float)
            {
                if (exp->t == Type::Int)
                { // ir: cvt_i2f var_flag##scope_id, exp->v
                    res.push_back(new ir::Instruction(ir::Operand(exp->v, ir::Type::Int), ir::Operand(), ir::Operand(SCOPED(var_flag), ir::Type::Float), ir::Operator::cvt_i2f));
                }
                else if (exp->t == Type::IntLiteral)
                { // ir: fmov var_flag##scope_id, "exp->v"
                    float val = std::stoi(exp->v);
                    res.push_back(new ir::Instruction(ir::Operand(std::to_string(val), ir::Type::FloatLiteral), ir::Operand(), ir::Operand(SCOPED(var_flag), ir::Type::Float), ir::Operator::fmov));
                }
                else if (exp->t == Type::Float)
                { // ir: fmov var_flag##scope_id, exp->v
                    res.push_back(new ir::Instruction(ir::Operand(exp->v, ir::Type::Float), ir::Operand(), ir::Operand(SCOPED(var_flag), ir::Type::Float), ir::Operator::fmov));
                }
                else if (exp->t == Type::FloatLiteral)
                { // ir: fmov var_flag##scope_id, "exp->v"
                    res.push_back(new ir::Instruction(ir::Operand(exp->v, ir::Type::FloatLiteral), ir::Operand(), ir::Operand(SCOPED(var_flag), ir::Type::Float), ir::Operator::fmov));
                }
            }
        }

        else if (lval->children.size() == 4)
        { // 1d arr: LVal -> Ident '[' Exp ']' <==> { Ident '[' Exp ']' '=' Exp ';' }, here we need temp var to store the offset
            Exp *offset = dynamic_cast<Exp *>(lval->children[2]);
            vector<ir::Instruction *> insts = analyzeExp(offset);
            INSERT_IRS(res, insts);

            if (ident_ste.operand.type == Type::IntPtr)
            {
                if (exp->t == Type::Int || exp->t == Type::IntLiteral)
                { // ir: store var_flag##scope_id, offset, "exp->v" or exp->v
                    res.push_back(new ir::Instruction(ir::Operand(SCOPED(var_flag), ir::Type::IntPtr), ir::Operand(offset->v, offset->t), ir::Operand(exp->v, exp->t), ir::Operator::store));
                }
                else if (exp->t == Type::Float)
                {
                    // ir: cvt_f2i __temp_var, exp->v
                    // ir: store var_flag##scope_id, offset, __temp_var
                    string tmp_f2i_flag = GET_NEXT_TEMP_VAR();
                    res.push_back(new ir::Instruction(ir::Operand(exp->v, ir::Type::Float), ir::Operand(), ir::Operand(tmp_f2i_flag, ir::Type::Int), ir::Operator::cvt_f2i));
                    res.push_back(new ir::Instruction(ir::Operand(SCOPED(var_flag), ir::Type::IntPtr), ir::Operand(offset->v, offset->t), ir::Operand(tmp_f2i_flag, Type::Int), ir::Operator::store));
                }
                else if (exp->t == Type::FloatLiteral)
                { // ir: store var_flag##scope_id, offset, "exp->v"
                    int val = std::stof(exp->v);
                    res.push_back(new ir::Instruction(ir::Operand(SCOPED(var_flag), ir::Type::IntPtr), ir::Operand(offset->v, offset->t), ir::Operand(std::to_string(val), Type::IntLiteral), ir::Operator::store));
                }
            }
            else if (ident_ste.operand.type == Type::FloatPtr)
            {
                if (exp->t == Type::Int)
                {
                    // ir: cvt_i2f __temp_var, exp->v
                    // ir: store var_flag##scope_id, offset, __temp_var
                    string tmp_i2f_flag = GET_NEXT_TEMP_VAR();
                    res.push_back(new ir::Instruction(ir::Operand(exp->v, ir::Type::Int), ir::Operand(), ir::Operand(tmp_i2f_flag, ir::Type::Float), ir::Operator::cvt_i2f));
                    res.push_back(new ir::Instruction(ir::Operand(SCOPED(var_flag), ir::Type::FloatPtr), ir::Operand(offset->v, offset->t), ir::Operand(tmp_i2f_flag, Type::Float), ir::Operator::store));
                }
                else if (exp->t == Type::IntLiteral)
                { // ir: mov var_flag##scope_id, "exp->v"
                    float val = std::stoi(exp->v);
                    res.push_back(new ir::Instruction(ir::Operand(SCOPED(var_flag), ir::Type::FloatPtr), ir::Operand(offset->v, offset->t), ir::Operand(std::to_string(val), Type::FloatLiteral), ir::Operator::store));
                }
                else if (exp->t == Type::Float || exp->t == Type::FloatLiteral)
                { // ir: store var_flag##scope_id, offset, exp->v or "exp->v"
                    res.push_back(new ir::Instruction(ir::Operand(SCOPED(var_flag), ir::Type::FloatPtr), ir::Operand(offset->v, offset->t), ir::Operand(exp->v, exp->t), ir::Operator::store));
                }
            }
        }

        else if (lval->children.size() == 7)
        { // 2d arr: LVal -> Ident '[' Exp ']' '[' Exp ']' <==> { Ident '[' Exp ']' '[' Exp ']' '=' Exp ';' }
            Exp *dim1_exp = dynamic_cast<Exp *>(lval->children[2]);
            vector<ir::Instruction *> cal1_insts = analyzeExp(dim1_exp);
            INSERT_IRS(res, cal1_insts);
            Exp *dim2_exp = dynamic_cast<Exp *>(lval->children[5]);
            vector<ir::Instruction *> cal2_insts = analyzeExp(dim2_exp);
            INSERT_IRS(res, cal2_insts);

            string tmp_dim1_flag = GET_NEXT_TEMP_VAR();
            string tmp_dim2_flag = GET_NEXT_TEMP_VAR();
            string tmp_col_len_flag = GET_NEXT_TEMP_VAR();
            string tmp_lineoffset_flag = GET_NEXT_TEMP_VAR();
            string tmp_totaloffset_flag = GET_NEXT_TEMP_VAR();

            // irs here: total offset = dim1 * col_len + dim2
            // def tmp_dim1_flag, dim1_exp->v
            // def tmp_dim2_flag, dim2_exp->v
            // def tmp_col_len_flag, ident_ste.dimension[1]
            // def tmp_lineoffset_flag, tmp_dim1_flag * tmp_col_len_flag
            // def tmp_totaloffset_flag, tmp_lineoffset_flag + tmp_dim2_flag
            // store var_flag##scope_id, tmp_totaloffset_flag, exp->v
            // store var_flag##scope_id, tmp_totaloffset_flag, exp->v
            // store var_flag##scope_id, tmp_totaloffset_flag, "exp->v"

            res.push_back(new ir::Instruction(ir::Operand(dim1_exp->v, dim1_exp->t), ir::Operand(), ir::Operand(tmp_dim1_flag, Type::Int), ir::Operator::def));
            res.push_back(new ir::Instruction(ir::Operand(dim2_exp->v, dim2_exp->t), ir::Operand(), ir::Operand(tmp_dim2_flag, Type::Int), ir::Operator::def));
            res.push_back(new ir::Instruction(ir::Operand(std::to_string(ident_ste.dimension[1]), Type::IntLiteral), ir::Operand(), ir::Operand(tmp_col_len_flag, Type::Int), ir::Operator::def));
            res.push_back(new ir::Instruction(ir::Operand(tmp_dim1_flag, Type::Int), ir::Operand(tmp_col_len_flag, Type::Int), ir::Operand(tmp_lineoffset_flag, Type::Int), ir::Operator::mul));
            res.push_back(new ir::Instruction(ir::Operand(tmp_lineoffset_flag, Type::Int), ir::Operand(tmp_dim2_flag, Type::Int), ir::Operand(tmp_totaloffset_flag, Type::Int), ir::Operator::add));

            if (ident_ste.operand.type == Type::IntPtr)
            { // IntPtr
                if (exp->t == Type::Int || exp->t == Type::IntLiteral)
                {
                    res.push_back(new ir::Instruction(ir::Operand(SCOPED(var_flag), ir::Type::IntPtr), ir::Operand(tmp_totaloffset_flag, Type::Int), ir::Operand(exp->v, exp->t), ir::Operator::store));
                }
                else if (exp->t == Type::Float)
                {
                    string tmp_f2i_flag = GET_NEXT_TEMP_VAR();
                    res.push_back(new ir::Instruction(ir::Operand(exp->v, ir::Type::Float), ir::Operand(), ir::Operand(tmp_f2i_flag, ir::Type::Int), ir::Operator::cvt_f2i));
                    res.push_back(new ir::Instruction(ir::Operand(SCOPED(var_flag), ir::Type::IntPtr), ir::Operand(tmp_totaloffset_flag, Type::Int), ir::Operand(tmp_f2i_flag, Type::Int), ir::Operator::store));
                }
                else if (exp->t == Type::FloatLiteral)
                {
                    int val = std::stof(exp->v);
                    res.push_back(new ir::Instruction(ir::Operand(SCOPED(var_flag), ir::Type::IntPtr), ir::Operand(tmp_totaloffset_flag, Type::Int), ir::Operand(std::to_string(val), Type::IntLiteral), ir::Operator::store));
                }
            }
            else if (ident_ste.operand.type == Type::FloatPtr)
            {
                if (exp->t == Type::Int)
                {
                    string tmp_i2f_flag = GET_NEXT_TEMP_VAR();
                    res.push_back(new ir::Instruction(ir::Operand(exp->v, ir::Type::Int), ir::Operand(), ir::Operand(tmp_i2f_flag, ir::Type::Float), ir::Operator::cvt_i2f));
                    res.push_back(new ir::Instruction(ir::Operand(SCOPED(var_flag), ir::Type::FloatPtr), ir::Operand(tmp_totaloffset_flag, Type::Int), ir::Operand(tmp_i2f_flag, Type::Float), ir::Operator::store));
                }
                else if (exp->t == Type::IntLiteral)
                {
                    float val = std::stoi(exp->v);
                    res.push_back(new ir::Instruction(ir::Operand(SCOPED(var_flag), ir::Type::FloatPtr), ir::Operand(tmp_totaloffset_flag, Type::Int), ir::Operand(std::to_string(val), Type::FloatLiteral), ir::Operator::store));
                }
                else if (exp->t == Type::Float || exp->t == Type::FloatLiteral)
                {
                    res.push_back(new ir::Instruction(ir::Operand(SCOPED(var_flag), ir::Type::FloatPtr), ir::Operand(tmp_totaloffset_flag, Type::Int), ir::Operand(exp->v, exp->t), ir::Operator::store));
                }
            }
        }
        return res;
    }

    if (GET_CHILD_PTR(block, Block, 0))
        // Block
        // if/while/for/else block
        return analyzeBlock(block, false);

    GET_CHILD_PTR(term, Term, 0);

    /**
     * my strategy here: check first term
     * 'break' ';'
     * 'continue' ';'
     * 'while' '(' Cond ')' Stmt
     * 'if' '(' Cond ')' Stmt [ 'else' Stmt ]
     * 'return' [Exp] ';'
     * but [Exp] ';' is not a term, so we need to check the first child
     * ';' (that is, [Exp] ';' does not take Exp)
     */

    if (term == nullptr)
    { // Exp ';'
        GET_CHILD_PTR(exp, Exp, 0);
        return analyzeExp(exp);
    }

    if (term->token.type == TokenType::SEMICN)
    { // ';
        return res;
    }

    if (term->token.type == TokenType::RETURNTK)
    { // 'return' [Exp] ';'
        if (root->children.size() == 3)
        { // retunr Exp ';'
            GET_CHILD_PTR(exp, Exp, 1);
            vector<ir::Instruction *> exp_insts = analyzeExp(exp);
            INSERT_IRS(res, exp_insts);

            if (curr_function->returnType == Type::Int)
            {
                if (exp->t == Type::Int || exp->t == Type::IntLiteral)
                { // Int or IntLiteral
                    // ir: return "exp->v"
                    res.push_back(new ir::Instruction(ir::Operand(exp->v, exp->t), ir::Operand(), ir::Operand(), ir::Operator::_return));
                }
                else if (exp->t == Type::FloatLiteral)
                { // Float or FloatLiteral
                    int val = std::stof(exp->v);
                    // ir: return "val"
                    res.push_back(new ir::Instruction(ir::Operand(std::to_string(val), Type::IntLiteral), ir::Operand(), ir::Operand(), ir::Operator::_return));
                }
                else if (exp->t == Type::Float)
                {
                    string tmp_f2i_flag = GET_NEXT_TEMP_VAR();
                    // ir: cvt_f2i __temp_var, exp->v
                    // ir: return __temp_var
                    res.push_back(new ir::Instruction(ir::Operand(exp->v, Type::Float), ir::Operand(), ir::Operand(tmp_f2i_flag, Type::Int), ir::Operator::cvt_f2i));
                    res.push_back(new ir::Instruction(ir::Operand(tmp_f2i_flag, Type::Int), ir::Operand(), ir::Operand(), ir::Operator::_return));
                }
            }
            else if (curr_function->returnType == Type::Float)
            {
                if (exp->t == Type::Float || exp->t == Type::FloatLiteral)
                { // Float or FloatLiteral
                    // ir: return "exp->v"
                    res.push_back(new ir::Instruction(ir::Operand(exp->v, exp->t), ir::Operand(), ir::Operand(), ir::Operator::_return));
                }
                else if (exp->t == Type::IntLiteral)
                { // Int or IntLiteral
                    float val = std::stoi(exp->v);
                    // ir: return "val"
                    res.push_back(new ir::Instruction(ir::Operand(std::to_string(val), Type::FloatLiteral), ir::Operand(), ir::Operand(), ir::Operator::_return));
                }
                else if (exp->t == Type::Int)
                {
                    string tmp_i2f_flag = GET_NEXT_TEMP_VAR();
                    // ir: cvt_i2f __temp_var, exp->v
                    // ir: return __temp_var
                    res.push_back(new ir::Instruction(ir::Operand(exp->v, Type::Int), ir::Operand(), ir::Operand(tmp_i2f_flag, Type::Float), ir::Operator::cvt_i2f));
                    res.push_back(new ir::Instruction(ir::Operand(tmp_i2f_flag, Type::Float), ir::Operand(), ir::Operand(), ir::Operator::_return));
                }
            }
        }
        else
        { // 'return' ';'
            res.push_back(new ir::Instruction(ir::Operand(), ir::Operand(), ir::Operand(), ir::Operator::_return));
        }
        return res;
    }

    if (term->token.type == TokenType::IFTK)
    { // 'if' '(' Cond ')' Stmt 'else' Stmt
        GET_CHILD_PTR(cond, Cond, 2);
        vector<ir::Instruction *> insts = analyzeCond(cond);
        if (cond->t == Type::Float || cond->t == Type::FloatLiteral)
        {
            if (cond->t == Type::FloatLiteral)
            {
                float val = std::stof(cond->v);
                FILL_NODE(cond, std::to_string(val != 0), Type::IntLiteral);
            }
            else
            {
                // ir: cvt_f2i __temp_var, cond->v
                // ir: __temp_var = 0.0
                string tmp1_flag = GET_NEXT_TEMP_VAR();
                string tmp2_flag = GET_NEXT_TEMP_VAR();
                insts.push_back(new ir::Instruction(ir::Operand(cond->v, Type::Float), ir::Operand("0.0", Type::FloatLiteral), ir::Operand(tmp1_flag, Type::Float), ir::Operator::fneq));
                insts.push_back(new ir::Instruction(ir::Operand(tmp1_flag, Type::Float), ir::Operand(), ir::Operand(tmp2_flag, Type::Int), ir::Operator::cvt_f2i));
                FILL_NODE(cond, tmp2_flag, Type::Int);
            }
        }
        INSERT_IRS(res, insts);

        /**
         * code structure:
         * 1. with else
         * goto_if_Inst:
         *      x: if(xxx) goto [pc, 2]
         * goto_else_Inst:
         *      x+1: goto [pc, len(stmt_after_if_insts) + 1]
         * goto_if_last_Inst:
         *      x+2: ...
         *      goto [pc, len(stmt_after_else_insts) + 1]
         * stmt_after_if_insts:
         *      ...
         * __unuse__
         * 2. without else:
         * goto_if_Inst:
         *      x: goto [pc, 2]
         * goto_else_Inst:
         *     x+1: goto [pc, len(stmt_after_if_insts) + 1]
         * goto_if_last_Inst:
         *     x+2: ...
         *     stmt_after_if_insts:
         *     ...
         * __unuse__
         */
        // goto [pc, 2], to skip next goto ir
        res.push_back(new ir::Instruction(ir::Operand(cond->v, cond->t), ir::Operand(), ir::Operand("2", Type::IntLiteral), ir::Operator::_goto));

        symbol_table.add_scope(); // fuck into scope for if
        GET_CHILD_PTR(_stmt, Stmt, 4);
        vector<ir::Instruction *> stmt_after_if_insts = analyzeStmt(_stmt);
        symbol_table.exit_scope();

        if (root->children.size() == 7)
        {                             // [ 'else' Stmt ]
            symbol_table.add_scope(); // fuck into scope for else
            GET_CHILD_PTR(_stmt, Stmt, 6);
            vector<ir::Instruction *> stmt_after_else_insts = analyzeStmt(_stmt);
            symbol_table.exit_scope();

            // GOTO End after 'if'
            // ir: goto [pc, len(stmt_after_else_insts) + 1]
            stmt_after_if_insts.push_back(new ir::Instruction(ir::Operand(), ir::Operand(), ir::Operand(std::to_string(stmt_after_else_insts.size() + 1), Type::IntLiteral), ir::Operator::_goto));

            // GOTO Begin for 'else'
            // ir: goto [pc, len(stmt_after_if_insts) + 1]
            res.push_back(new ir::Instruction(ir::Operand(), ir::Operand(), ir::Operand(std::to_string(stmt_after_if_insts.size() + 1), Type::IntLiteral), ir::Operator::_goto));

            // insert if stmt
            INSERT_IRS(res, stmt_after_if_insts);
            // insert else stmt
            INSERT_IRS(res, stmt_after_else_insts);
            // unuse stmt, waiting for end of 'if' & 'else'
            res.push_back(new ir::Instruction(ir::Operand(), ir::Operand(), ir::Operand(), ir::Operator::__unuse__));
        }
        else
        { // 'if' '(' Cond ')' Stmt
            // goto [pc, len(stmt_after_if_insts) + 1]
            res.push_back(new ir::Instruction(ir::Operand(), ir::Operand(), ir::Operand(std::to_string(stmt_after_if_insts.size() + 1), Type::IntLiteral), ir::Operator::_goto));
            INSERT_IRS(res, stmt_after_if_insts);
            res.push_back(new ir::Instruction(ir::Operand(), ir::Operand(), ir::Operand(), ir::Operator::__unuse__));
        }
        return res;
    }

    if (term->token.type == TokenType::WHILETK)
    { // 'while' '(' Cond ')' Stmt
        GET_CHILD_PTR(cond, Cond, 2);
        vector<Instruction *> insts = analyzeCond(cond);

        if (cond->t == Type::Float || cond->t == Type::FloatLiteral)
        { // convert to Int
            if (cond->t == Type::FloatLiteral)
            {
                float val = std::stof(cond->v);
                FILL_NODE(cond, std::to_string(val != 0), Type::IntLiteral);
            }
            else
            {
                string tmp1_flag = GET_NEXT_TEMP_VAR();
                string tmp2_flag = GET_NEXT_TEMP_VAR();
                insts.push_back(new ir::Instruction(ir::Operand(cond->v, Type::Float), ir::Operand("0.0", Type::FloatLiteral), ir::Operand(tmp1_flag, Type::Float), ir::Operator::fneq));
                insts.push_back(new ir::Instruction(ir::Operand(tmp1_flag, Type::Float), ir::Operand(), ir::Operand(tmp2_flag, Type::Int), ir::Operator::cvt_f2i));
                FILL_NODE(cond, tmp2_flag, Type::Int);
            }
        }

        symbol_table.add_scope(); // fuck into scope for whil
        GET_CHILD_PTR(_stmt, Stmt, 4);
        vector<Instruction *> stmt_insts = analyzeStmt(_stmt);
        symbol_table.exit_scope();

        /**
         * my strategy here:
         * ... (conds)
         * if (cond): goto [pc, 2]
         * else: goto [pc, len(stmt_insts) + 1]  // goto end
         * ...
         * 1. break: goto [pc, len(stmt_insts) - i]  // goto end
         * 2. continue: goto [pc, -(len(stmt_insts) + i + 2)]  // goto cond begin
         * ...
         * __unuse__;
         */

        stmt_insts.push_back(new ir::Instruction(ir::Operand("continue", Type::null), ir::Operand(), ir::Operand(), ir::Operator::__unuse__));

        for (int i = 0; i < stmt_insts.size(); i++)
        { // replace break/continue as goto
            if (stmt_insts[i]->op == Operator::__unuse__ && stmt_insts[i]->op1.type == Type::null)
            {
                if (stmt_insts[i]->op1.name == "break")
                {
                    stmt_insts[i] = new ir::Instruction(ir::Operand(), ir::Operand(), ir::Operand(std::to_string(int(stmt_insts.size()) - i), Type::IntLiteral), ir::Operator::_goto);
                }
                else if (stmt_insts[i]->op1.name == "continue")
                {
                    stmt_insts[i] = new ir::Instruction(ir::Operand(), ir::Operand(), ir::Operand(std::to_string(-(2 + i + int(insts.size()))), Type::IntLiteral), ir::Operator::_goto);
                }
            }
        }

        INSERT_IRS(res, insts);
        res.push_back(new ir::Instruction(ir::Operand(cond->v, cond->t), ir::Operand(), ir::Operand("2", Type::IntLiteral), ir::Operator::_goto));
        res.push_back(new ir::Instruction(ir::Operand(), ir::Operand(), ir::Operand(std::to_string(stmt_insts.size() + 1), Type::IntLiteral), ir::Operator::_goto));
        INSERT_IRS(res, stmt_insts);

        // ir: __unuse__
        res.push_back(new ir::Instruction(ir::Operand(), ir::Operand(), ir::Operand(), ir::Operator::__unuse__));
        return res;
    }

    if (term->token.type == TokenType::BREAKTK)
    { // 'break' ';'
        res.push_back(new Instruction(Operand("break", Type::null), Operand(), Operand(), Operator::__unuse__));
        return res;
    }

    if (term->token.type == TokenType::CONTINUETK)
    { // 'continue' ';'
        res.push_back(new Instruction(Operand("continue", Type::null), Operand(), Operand(), Operator::__unuse__));
        return res;
    }

    return res;
}

// Exp -> AddExp
vector<ir::Instruction *> frontend::Analyzer::analyzeExp(Exp *root)
{
    GET_CHILD_PTR(addexp, AddExp, 0);
    vector<ir::Instruction *> insts = analyzeAddExp(addexp);
    COPY_EXP_NODE(root, addexp);
    return insts;
}

// AddExp -> MulExp { ('+' | '-') MulExp }
vector<ir::Instruction *> frontend::Analyzer::analyzeAddExp(AddExp *root)
{
    vector<Instruction *> res;
    /**
     * Type upgrade:
     * Int + Float -> Float
     * Literal + Var -> Var
     */

    Type target_type = Type::IntLiteral; // target type, default is IntLiteral

    for (int i = 0; i < root->children.size(); i += 2)
    {
        GET_CHILD_PTR(mulexp, MulExp, i);
        vector<Instruction *> insts = analyzeMulExp(mulexp);
        INSERT_IRS(res, insts);

        // upgrade type to the highest type
        if (mulexp->t == ir::Type::Float)
            target_type = ir::Type::Float;
        else if (mulexp->t == ir::Type::Int && target_type == ir::Type::IntLiteral)
            target_type = ir::Type::Int;
        else if (mulexp->t == ir::Type::FloatLiteral && target_type == ir::Type::IntLiteral)
            target_type = ir::Type::FloatLiteral;
        else if ((mulexp->t == ir::Type::FloatLiteral && target_type == ir::Type::Int) || (target_type == ir::Type::FloatLiteral && mulexp->t == ir::Type::Int)) // 提升没有顺序
            target_type = ir::Type::Float;
    }

    GET_CHILD_PTR(mulexp0, MulExp, 0); // first MulExp res, val: type
    COPY_EXP_NODE(root, mulexp0);

    if (root->children.size() == 1)
        return res; // complete analyzeAddExp

    if (target_type != root->t)
    { // type conversion for root
        if (target_type == Type::Int)
        { // IntLiteral -> Int
            string tmp_intcvt_flag = GET_NEXT_TEMP_VAR();
            res.push_back(new Instruction(ir::Operand(root->v, ir::Type::IntLiteral), ir::Operand(), ir::Operand(tmp_intcvt_flag, ir::Type::Int), ir::Operator::def));
            FILL_NODE(root, tmp_intcvt_flag, Type::Int);
        }
        else if (target_type == Type::FloatLiteral)
        { // IntLiteral -> FloatLiteral
            float val = std::stoi(root->v);
            FILL_NODE(root, std::to_string(val), Type::FloatLiteral);
        }
        else if (target_type == Type::Float)
        { // IntLiteral -> Float, Int -> Float, FloatLiteral -> Float
            if (root->t == Type::IntLiteral)
            {
                float val = std::stof(root->v);
                string tmp_i2f_flag = GET_NEXT_TEMP_VAR();
                res.push_back(new Instruction(ir::Operand(std::to_string(val), ir::Type::FloatLiteral), ir::Operand(), ir::Operand(tmp_i2f_flag, ir::Type::Float), ir::Operator::fdef));
                FILL_NODE(root, tmp_i2f_flag, Type::Float);
            }
            else if (root->t == Type::Int)
            {
                string tmp_i2f_flag = GET_NEXT_TEMP_VAR();
                res.push_back(new Instruction(ir::Operand(root->v, ir::Type::Int), ir::Operand(), ir::Operand(tmp_i2f_flag, ir::Type::Float), ir::Operator::cvt_i2f));
                FILL_NODE(root, tmp_i2f_flag, Type::Float);
            }
            else if (root->t == Type::FloatLiteral)
            {
                string tmp_fl2f_flag = GET_NEXT_TEMP_VAR();
                res.push_back(new Instruction(ir::Operand(root->v, ir::Type::FloatLiteral), ir::Operand(), ir::Operand(tmp_fl2f_flag, ir::Type::Float), ir::Operator::fdef));
                FILL_NODE(root, tmp_fl2f_flag, Type::Float);
            }
        }
    }

    for (int i = 2; i < root->children.size(); i += 2)
    { // {'+' | '-'} MulExp, from left to right
        GET_CHILD_PTR(mulexp, MulExp, i);
        GET_CHILD_PTR(op_term, Term, i - 1); // '+' | '-'

        if (target_type != mulexp->t)
        { // type conversion for mulexp
            if (target_type == Type::Int)
            {
                string tmp_intcvt_flag = GET_NEXT_TEMP_VAR();
                res.push_back(new Instruction(ir::Operand(mulexp->v, ir::Type::IntLiteral), ir::Operand(), ir::Operand(tmp_intcvt_flag, ir::Type::Int), ir::Operator::def));
                FILL_NODE(mulexp, tmp_intcvt_flag, Type::Int);
            }
            else if (target_type == Type::FloatLiteral)
            {
                float val = std::stoi(mulexp->v);
                FILL_NODE(mulexp, std::to_string(val), Type::FloatLiteral);
            }
            else if (target_type == Type::Float)
            {
                if (mulexp->t == Type::IntLiteral)
                {
                    float val = std::stof(mulexp->v);
                    string tmp_i2f_flag = GET_NEXT_TEMP_VAR();
                    res.push_back(new Instruction(ir::Operand(std::to_string(val), ir::Type::FloatLiteral), ir::Operand(), ir::Operand(tmp_i2f_flag, ir::Type::Float), ir::Operator::fdef));
                    FILL_NODE(mulexp, tmp_i2f_flag, Type::Float);
                }
                else if (mulexp->t == Type::Int)
                {
                    string tmp_i2f_flag = GET_NEXT_TEMP_VAR();
                    res.push_back(new Instruction(ir::Operand(mulexp->v, ir::Type::Int), ir::Operand(), ir::Operand(tmp_i2f_flag, ir::Type::Float), ir::Operator::cvt_i2f));
                    FILL_NODE(mulexp, tmp_i2f_flag, Type::Float);
                }
                else if (mulexp->t == Type::FloatLiteral)
                {
                    string tmp_fl2f_flag = GET_NEXT_TEMP_VAR();
                    res.push_back(new Instruction(ir::Operand(mulexp->v, ir::Type::FloatLiteral), ir::Operand(), ir::Operand(tmp_fl2f_flag, ir::Type::Float), ir::Operator::fdef));
                    FILL_NODE(mulexp, tmp_fl2f_flag, Type::Float);
                }
            }
        }

        if (target_type == Type::IntLiteral)
        { // compute it if target_type is IntLiteral
            root->v = op_term->token.type == TokenType::PLUS ? std::to_string(std::stoi(root->v) + std::stoi(mulexp->v)) : std::to_string(std::stoi(root->v) - std::stoi(mulexp->v));
        }
        else if (target_type == Type::FloatLiteral)
        { // compute it if target_type is FloatLiteral
            root->v = op_term->token.type == TokenType::PLUS ? std::to_string(std::stof(root->v) + std::stof(mulexp->v)) : std::to_string(std::stof(root->v) - std::stof(mulexp->v));
        }
        else if (target_type == Type::Int)
        {
            // add new temp var, to replace the res
            // ir: add/sub root->v, mulexp->v, tmp_cal_flag
            string tmp_cal_flag = GET_NEXT_TEMP_VAR();
            Instruction *inst;
            if (op_term->token.type == TokenType::PLUS)
                inst = new Instruction(ir::Operand(root->v, ir::Type::Int), ir::Operand(mulexp->v, ir::Type::Int), ir::Operand(tmp_cal_flag, ir::Type::Int), ir::Operator::add);
            else if (op_term->token.type == TokenType::MINU)
                inst = new Instruction(ir::Operand(root->v, ir::Type::Int), ir::Operand(mulexp->v, ir::Type::Int), ir::Operand(tmp_cal_flag, ir::Type::Int), ir::Operator::sub);
            res.push_back(inst);
            root->v = tmp_cal_flag;
        }
        else if (target_type == Type::Float)
        {
            // add new temp var, to replace the res
            // ir: fadd/fsub root->v, mulexp->v, tmp_cal_flag
            string tmp_cal_flag = GET_NEXT_TEMP_VAR();
            Instruction *inst;
            if (op_term->token.type == TokenType::PLUS)
                inst = new Instruction(ir::Operand(root->v, ir::Type::Float), ir::Operand(mulexp->v, ir::Type::Float), ir::Operand(tmp_cal_flag, ir::Type::Float), ir::Operator::fadd);
            else if (op_term->token.type == TokenType::MINU)
                inst = new Instruction(ir::Operand(root->v, ir::Type::Float), ir::Operand(mulexp->v, ir::Type::Float), ir::Operand(tmp_cal_flag, ir::Type::Float), ir::Operator::fsub);
            res.push_back(inst);
            root->v = tmp_cal_flag;
        }
    }
    return res;
}

// ConstExp -> AddExp
vector<ir::Instruction *> frontend::Analyzer::analyzeConstExp(ConstExp *root)
{
    GET_CHILD_PTR(addexp, AddExp, 0);
    vector<ir::Instruction *> insts = analyzeAddExp(addexp);
    COPY_EXP_NODE(root, addexp);
    return insts;
}

// MulExp -> UnaryExp { ('*' | '/' | '%') UnaryExp }, very similar to AddExp
vector<ir::Instruction *> frontend::Analyzer::analyzeMulExp(MulExp *root)
{
    vector<Instruction *> res;

    Type target_type = Type::IntLiteral;

    for (int i = 0; i < root->children.size(); i += 2)
    {
        GET_CHILD_PTR(unaryexp, UnaryExp, i);
        vector<Instruction *> insts = analyzeUnaryExp(unaryexp);
        INSERT_IRS(res, insts);

        if (unaryexp->t == ir::Type::Float)
            target_type = ir::Type::Float;
        else if (unaryexp->t == ir::Type::Int && target_type == ir::Type::IntLiteral)
            target_type = ir::Type::Int;
        else if (unaryexp->t == ir::Type::FloatLiteral && target_type == ir::Type::IntLiteral)
            target_type = ir::Type::FloatLiteral;
        else if ((unaryexp->t == ir::Type::FloatLiteral && target_type == ir::Type::Int) || (target_type == ir::Type::FloatLiteral && unaryexp->t == ir::Type::Int)) // 提升没有顺序
            target_type = ir::Type::Float;
    }

    GET_CHILD_PTR(unaryexp0, UnaryExp, 0); // first UnaryExp res, val: type
    COPY_EXP_NODE(root, unaryexp0);

    if (root->children.size() == 1)
        return res;

    if (target_type != root->t)
    {
        if (target_type == Type::Int)
        {
            string tmp_intcvt_flag = GET_NEXT_TEMP_VAR();
            res.push_back(new Instruction(ir::Operand(root->v, ir::Type::IntLiteral), ir::Operand(), ir::Operand(tmp_intcvt_flag, ir::Type::Int), ir::Operator::def));
            FILL_NODE(root, tmp_intcvt_flag, Type::Int);
        }
        else if (target_type == Type::FloatLiteral)
        {
            float val = std::stoi(root->v);
            FILL_NODE(root, std::to_string(val), Type::FloatLiteral);
        }
        else if (target_type == Type::Float)
        {
            if (root->t == Type::IntLiteral)
            {
                float val = std::stof(root->v);
                string tmp_i2f_flag = GET_NEXT_TEMP_VAR();
                res.push_back(new Instruction(ir::Operand(std::to_string(val), ir::Type::FloatLiteral), ir::Operand(), ir::Operand(tmp_i2f_flag, ir::Type::Float), ir::Operator::fdef));
                FILL_NODE(root, tmp_i2f_flag, Type::Float);
            }
            else if (root->t == Type::Int)
            {
                string tmp_i2f_flag = GET_NEXT_TEMP_VAR();
                res.push_back(new Instruction(ir::Operand(root->v, ir::Type::Int), ir::Operand(), ir::Operand(tmp_i2f_flag, ir::Type::Float), ir::Operator::cvt_i2f));
                FILL_NODE(root, tmp_i2f_flag, Type::Float);
            }
            else if (root->t == Type::FloatLiteral)
            {
                string tmp_fl2f_flag = GET_NEXT_TEMP_VAR();
                res.push_back(new Instruction(ir::Operand(root->v, ir::Type::FloatLiteral), ir::Operand(), ir::Operand(tmp_fl2f_flag, ir::Type::Float), ir::Operator::fdef));
                FILL_NODE(root, tmp_fl2f_flag, Type::Float);
            }
        }
    }

    for (int i = 2; i < root->children.size(); i += 2)
    {
        GET_CHILD_PTR(unaryexp, UnaryExp, i);
        GET_CHILD_PTR(op_term, Term, i - 1); // '*' | '/' | '%'

        if (target_type != unaryexp->t)
        {
            if (target_type == Type::Int)
            {
                string tmp_intcvt_flag = GET_NEXT_TEMP_VAR();
                res.push_back(new Instruction(ir::Operand(unaryexp->v, ir::Type::IntLiteral), ir::Operand(), ir::Operand(tmp_intcvt_flag, ir::Type::Int), ir::Operator::def));
                FILL_NODE(unaryexp, tmp_intcvt_flag, Type::Int);
            }
            else if (target_type == Type::FloatLiteral)
            {
                float val = std::stoi(unaryexp->v);
                FILL_NODE(unaryexp, std::to_string(val), Type::FloatLiteral);
            }
            else if (target_type == Type::Float)
            {
                if (unaryexp->t == Type::IntLiteral)
                {
                    float val = std::stof(unaryexp->v);
                    string tmp_i2f_flag = GET_NEXT_TEMP_VAR();
                    res.push_back(new Instruction(ir::Operand(std::to_string(val), ir::Type::FloatLiteral), ir::Operand(), ir::Operand(tmp_i2f_flag, ir::Type::Float), ir::Operator::fdef));
                    FILL_NODE(unaryexp, tmp_i2f_flag, Type::Float);
                }
                else if (unaryexp->t == Type::Int)
                {
                    string tmp_i2f_flag = GET_NEXT_TEMP_VAR();
                    res.push_back(new Instruction(ir::Operand(unaryexp->v, ir::Type::Int), ir::Operand(), ir::Operand(tmp_i2f_flag, ir::Type::Float), ir::Operator::cvt_i2f));
                    FILL_NODE(unaryexp, tmp_i2f_flag, Type::Float);
                }
                else if (unaryexp->t == Type::FloatLiteral)
                {
                    string tmp_fl2f_flag = GET_NEXT_TEMP_VAR();
                    res.push_back(new Instruction(ir::Operand(unaryexp->v, ir::Type::FloatLiteral), ir::Operand(), ir::Operand(tmp_fl2f_flag, ir::Type::Float), ir::Operator::fdef));
                    FILL_NODE(unaryexp, tmp_fl2f_flag, Type::Float);
                }
            }
        }

        if (target_type == Type::IntLiteral)
        {
            int val1 = std::stoi(root->v);
            int val2 = std::stoi(unaryexp->v);
            if (op_term->token.type == TokenType::MULT)
                root->v = std::to_string(val1 * val2);
            else if (op_term->token.type == TokenType::DIV)
                root->v = std::to_string(val1 / val2);
            else if (op_term->token.type == TokenType::MOD)
                root->v = std::to_string(val1 % val2);
        }
        else if (target_type == Type::FloatLiteral)
        {
            float val1 = std::stof(root->v);
            float val2 = std::stof(unaryexp->v);
            if (op_term->token.type == TokenType::MULT)
                root->v = std::to_string(val1 * val2);
            else if (op_term->token.type == TokenType::DIV)
                root->v = std::to_string(val1 / val2);
        }
        else if (target_type == Type::Int)
        {
            string tmp_cal_flag = GET_NEXT_TEMP_VAR();
            Instruction *inst;
            if (op_term->token.type == TokenType::MULT)
                inst = new Instruction(ir::Operand(root->v, ir::Type::Int), ir::Operand(unaryexp->v, ir::Type::Int), ir::Operand(tmp_cal_flag, ir::Type::Int), ir::Operator::mul);
            else if (op_term->token.type == TokenType::DIV)
                inst = new Instruction(ir::Operand(root->v, ir::Type::Int), ir::Operand(unaryexp->v, ir::Type::Int), ir::Operand(tmp_cal_flag, ir::Type::Int), ir::Operator::div);
            else if (op_term->token.type == TokenType::MOD)
                inst = new Instruction(ir::Operand(root->v, ir::Type::Int), ir::Operand(unaryexp->v, ir::Type::Int), ir::Operand(tmp_cal_flag, ir::Type::Int), ir::Operator::mod);
            res.push_back(inst);
            root->v = tmp_cal_flag;
        }
        else if (target_type == Type::Float)
        {
            string tmp_cal_flag = GET_NEXT_TEMP_VAR();
            Instruction *inst;
            if (op_term->token.type == TokenType::MULT)
                inst = new Instruction(ir::Operand(root->v, ir::Type::Float), ir::Operand(unaryexp->v, ir::Type::Float), ir::Operand(tmp_cal_flag, ir::Type::Float), ir::Operator::fmul);
            else if (op_term->token.type == TokenType::DIV)
                inst = new Instruction(ir::Operand(root->v, ir::Type::Float), ir::Operand(unaryexp->v, ir::Type::Float), ir::Operand(tmp_cal_flag, ir::Type::Float), ir::Operator::fdiv);
            res.push_back(inst);
            root->v = tmp_cal_flag;
        }
    }
    return res;
}

// UnaryExp -> PrimaryExp | Ident '(' [FuncRParams] ')' | UnaryOp UnaryExp
vector<ir::Instruction *> frontend::Analyzer::analyzeUnaryExp(UnaryExp *root)
{
    vector<ir::Instruction *> res;

    GET_CHILD_PTR(primaryexp, PrimaryExp, 0);
    if (primaryexp != nullptr)
    { // PrimaryExp
        vector<Instruction *> insts = analyzePrimaryExp(primaryexp);
        INSERT_IRS(res, insts);
        COPY_EXP_NODE(root, primaryexp);
        return res;
    }

    GET_CHILD_PTR(term, Term, 0);
    if (term != nullptr && term->token.type == TokenType::IDENFR)
    { // Ident '(' [FuncRParams] ')'
        string func_flag = term->token.value;
        Function *func = symbol_table.functions[func_flag]; // get function
        vector<Operand> paraVec;                            // list of parameters of function while calling
        GET_CHILD_PTR(funcrparams, FuncRParams, 2);
        if (funcrparams != nullptr)
        {
            vector<Operand> func_para_type = func->ParameterList;
            vector<ir::Instruction *> insts = analyzeFuncRParams(funcrparams, func_para_type, paraVec);
            INSERT_IRS(res, insts);
        }

        string return_value = GET_NEXT_TEMP_VAR(); // make a new temp var for return value
        // ir: call __temp_var, func_flag(paraVec)
        res.push_back(new ir::CallInst(ir::Operand(func->name, func->returnType), paraVec, ir::Operand(return_value, func->returnType)));

        FILL_NODE(root, return_value, func->returnType); // fill node
        return res;
    }

    GET_CHILD_PTR(unaryop, UnaryOp, 0);
    if (unaryop != nullptr)
    { // UnaryOp UnaryExp
        GET_CHILD_PTR(unaryexp, UnaryExp, 1);
        vector<ir::Instruction *> insts = analyzeUnaryExp(unaryexp); // calculate UnaryExp
        INSERT_IRS(res, insts);

        Term *unaryop_term = dynamic_cast<Term *>(unaryop->children[0]);
        if (unaryop_term->token.type == TokenType::PLUS)
        { // Yehn! nothing to do 🤣
            COPY_EXP_NODE(root, unaryexp);
        }
        else if (unaryop_term->token.type == TokenType::MINU)
        {
            if (unaryexp->t == Type::IntLiteral || unaryexp->t == Type::FloatLiteral)
            {
                FILL_NODE(root, std::to_string(-std::stof(unaryexp->v)), unaryexp->t); // fill node
            }
            else if (unaryexp->t == Type::Int || unaryexp->t == Type::Float)
            {
                /**
                 * My strategy here:
                 * -x = 0 - x (int)
                 * -x = 0.0 - x (float)
                 */

                string tmp_zero = GET_NEXT_TEMP_VAR();
                string operand_flag = (unaryexp->t == Type::Float) ? "0.0" : "0";
                ir::Type operand_type = (unaryexp->t == Type::Float) ? (ir::Type::FloatLiteral) : (ir::Type::IntLiteral);
                ir::Operator operator_defname = (unaryexp->t == Type::Float) ? (ir::Operator::fdef) : (ir::Operator::def);
                ir::Operator operator_flag = (unaryexp->t == Type::Float) ? (ir::Operator::fsub) : (ir::Operator::sub);

                string tmp_minu = GET_NEXT_TEMP_VAR();

                res.push_back(new ir::Instruction(ir::Operand(operand_flag, operand_type), ir::Operand(), ir::Operand(tmp_zero, unaryexp->t), operator_defname));
                res.push_back(new ir::Instruction(ir::Operand(tmp_zero, unaryexp->t), ir::Operand(unaryexp->v, unaryexp->t), ir::Operand(tmp_minu, unaryexp->t), operator_flag));
                FILL_NODE(root, tmp_minu, unaryexp->t); // fill node
            }
        }
        else if (unaryop_term->token.type == TokenType::NOT)
        { // !
            if (unaryexp->t == Type::IntLiteral || unaryexp->t == Type::FloatLiteral)
            {
                int tmp = (unaryexp->t == Type::FloatLiteral) ? (!std::stof(unaryexp->v)) : (!std::stoi(unaryexp->v));
                FILL_NODE(root, std::to_string(tmp), Type::IntLiteral); // fill node
            }
            else if (unaryexp->t == Type::Int || unaryexp->t == Type::Float)
            {
                string tmp_not = GET_NEXT_TEMP_VAR();
                // ir: not tmp_not, unaryexp->v
                res.push_back(new ir::Instruction(ir::Operand(unaryexp->v, unaryexp->t), ir::Operand(), ir::Operand(tmp_not, ir::Type::Int), ir::Operator::_not));
                FILL_NODE(root, tmp_not, Type::Int); // fill node
            }
        }
        return res;
    }
    return res;
}

// FuncRParams -> Exp { ',' Exp }
vector<Instruction *> frontend::Analyzer::analyzeFuncRParams(FuncRParams *root, vector<Operand> &func_para_type, vector<Operand> &paraVec)
{
    vector<ir::Instruction *> res;

    for (int i = 0, cnt = 0; i < root->children.size(); i += 2, cnt += 1)
    { // { Exp, ','|None }
        GET_CHILD_PTR(exp, Exp, i);
        vector<ir::Instruction *> insts = analyzeExp(exp);
        INSERT_IRS(res, insts);

        if (func_para_type[cnt].type == ir::Type::Float)
        { // type conversion of func_para_type[cnt] into float
            if (exp->t == Type::Int)
            {
                string tmp_i2f_flag = GET_NEXT_TEMP_VAR();
                res.push_back(new Instruction(ir::Operand(exp->v, ir::Type::Int), ir::Operand(), ir::Operand(tmp_i2f_flag, ir::Type::Float), ir::Operator::cvt_i2f));
                paraVec.push_back(Operand(tmp_i2f_flag, ir::Type::Float));
            }
            else if (exp->t == Type::IntLiteral)
            {
                float val = std::stoi(exp->v);
                paraVec.push_back(Operand(std::to_string(val), ir::Type::FloatLiteral));
            }
            else
            {
                paraVec.push_back(Operand(exp->v, exp->t));
            }
        }
        else if (func_para_type[cnt].type == ir::Type::Int)
        {
            if (exp->t == Type::Float)
            {
                string tmp_f2i_flag = GET_NEXT_TEMP_VAR();
                res.push_back(new Instruction(ir::Operand(exp->v, ir::Type::Float), ir::Operand(), ir::Operand(tmp_f2i_flag, ir::Type::Int), ir::Operator::cvt_f2i));
                paraVec.push_back(Operand(tmp_f2i_flag, ir::Type::Int));
            }
            else if (exp->t == Type::FloatLiteral)
            {
                int val = std::stoi(exp->v);
                paraVec.push_back(Operand(std::to_string(val), ir::Type::IntLiteral));
            }
            else
            {
                paraVec.push_back(Operand(exp->v, exp->t));
            }
        }
        else
            // Yehn! nothing to do 🤣
            paraVec.push_back(Operand(exp->v, exp->t));
    }
    return res;
}

// PrimaryExp -> '(' Exp ')' | LVal | Number
vector<Instruction *> frontend::Analyzer::analyzePrimaryExp(PrimaryExp *root)
{
    vector<Instruction *> res;

    if (root->children.size() == 3)
    { // PrimaryExp -> '(' Exp ')'
        GET_CHILD_PTR(exp, Exp, 1);
        res = analyzeExp(exp);
        COPY_EXP_NODE(root, exp);
        return res;
    }
    else
    {
        if (GET_CHILD_PTR(lval, LVal, 0))
        { // PrimaryExp -> LVal
            res = analyzeLVal(lval);
            COPY_EXP_NODE(root, lval);
            return res;
        }
        else if (GET_CHILD_PTR(number, Number, 0))
        { // PrimaryExp -> Number
            analyzeNumber(number);
            COPY_EXP_NODE(root, number);
            return res;
        }
    }

    return res;
}

// LVal -> Ident {'[' Exp ']'}
vector<Instruction *> frontend::Analyzer::analyzeLVal(LVal *root)
{
    vector<Instruction *> res;
    GET_CHILD_PTR(term, Term, 0);
    string var_flag = term->token.value; // current variable name

    if (root->children.size() == 1)
    {                                                     // 0-dim variable: Int, Float, IntPtr, FloatPtr, IntLiteral, FloatLiteral
        STE operand_ste = symbol_table.get_ste(var_flag); // get symbol table entry
        root->t = operand_ste.operand.type;
        if (root->t == Type::IntLiteral || root->t == Type::FloatLiteral)
            root->v = operand_ste.literalVal;
        else
            root->v = SCOPED(var_flag);
        return res;
    }

    else if (root->children.size() == 4)
    { // 1-dim variable: Int, Float, IntPtr, FloatPtr
        // here we need to calculate the address of the variable
        // if var[exp].dim = 0, it's a variable
        // if var[exp].dim = 1, it's a pointer
        GET_CHILD_PTR(exp, Exp, 2);
        vector<Instruction *> exp_res = analyzeExp(exp);
        INSERT_IRS(res, exp_res);

        STE operand_ste = symbol_table.get_ste(var_flag);
        if (operand_ste.dimension.size() == 1)
        {                                                                                          // Int or Float
            Type target_type = operand_ste.operand.type == Type::IntPtr ? Type::Int : Type::Float; // if ste type is IntPtr, then target type is Int else Float

            string tmp_var_flag = GET_NEXT_TEMP_VAR();
            // ir: load var_flag[exp], tmp_var_flag
            res.push_back(new ir::Instruction(ir::Operand(SCOPED(var_flag), operand_ste.operand.type), ir::Operand(exp->v, exp->t), ir::Operand(tmp_var_flag, target_type), ir::Operator::load));
            FILL_NODE(root, tmp_var_flag, target_type); // fill the node with the temp var flag and type
            return res;
        }
        else
        {                                                // IntPtr or FloatPtr
            Type target_type = operand_ste.operand.type; // same as the operand type

            string tmp_var_flag = GET_NEXT_TEMP_VAR();
            if (exp->t == Type::IntLiteral)
            { // offset = exp->v * operand_ste.dimension[1]
                int val = std::stoi(exp->v) * operand_ste.dimension[1];
                // getptr var_flag[exp], tmp_var_flag
                Instruction *inst = new ir::Instruction(ir::Operand(SCOPED(var_flag), operand_ste.operand.type), ir::Operand(std::to_string(val), Type::IntLiteral), ir::Operand(tmp_var_flag, target_type), ir::Operator::getptr);
                res.push_back(inst);
            }
            else
            { // offset is need to be calculated by ir
                string tmp_offset_flag = GET_NEXT_TEMP_VAR();
                string tmp_len_flag = GET_NEXT_TEMP_VAR();
                /**
                 * My strategy here:
                 * offset = exp->v * operand_ste.dimension[1]
                 */

                // ir: def tmp_len_flag, operand_ste.dimension[1]
                // ir: mul exp->v, tmp_len_flag * tmp_offset_flag
                // ir: getptr var_flag[exp], tmp_offset_flag, tmp_var_flag
                res.push_back(new ir::Instruction(ir::Operand(std::to_string(operand_ste.dimension[1]), Type::IntLiteral), ir::Operand(), ir::Operand(tmp_len_flag, Type::Int), ir::Operator::def));
                res.push_back(new ir::Instruction(ir::Operand(exp->v, Type::Int), ir::Operand(tmp_len_flag, Type::Int), ir::Operand(tmp_offset_flag, Type::Int), ir::Operator::mul));
                res.push_back(new ir::Instruction(ir::Operand(SCOPED(var_flag), operand_ste.operand.type), ir::Operand(tmp_offset_flag, Type::Int), ir::Operand(tmp_var_flag, target_type), ir::Operator::getptr));
            }
            FILL_NODE(root, tmp_var_flag, target_type);
            return res;
        }
    }
    else if (root->children.size() == 7)
    { // 2-dim variable: Int, Float, IntPtr, FloatPtr
        GET_CHILD_PTR(exp1, Exp, 2);
        GET_CHILD_PTR(exp2, Exp, 5);

        vector<Instruction *> exp1_res = analyzeExp(exp1);
        vector<Instruction *> exp2_res = analyzeExp(exp2);
        INSERT_IRS(res, exp1_res);
        INSERT_IRS(res, exp2_res);
        STE operand_ste = symbol_table.get_ste(var_flag);

        Type target_type = operand_ste.operand.type == Type::IntPtr ? Type::Int : Type::Float; // must be Int or Float

        string tmp_dim1_flag = GET_NEXT_TEMP_VAR();
        string tmp_dim2_flag = GET_NEXT_TEMP_VAR();
        string tmp_col_len = GET_NEXT_TEMP_VAR();
        string tmp_line_offset = GET_NEXT_TEMP_VAR();
        string tmp_total_offset = GET_NEXT_TEMP_VAR();
        string tmp_load_val = GET_NEXT_TEMP_VAR();

        // ir herre:
        // def tmp_dim1_flag, "exp1->v"
        // def tmp_dim2_flag, "exp2->v"
        // def tmp_col_len, "operand_ste.dimension[1]"
        // mul tmp_line_offset, tmp_dim1_flag * tmp_col_len
        // add tmp_total_offset, tmp_line_offset + tmp_dim2_flag  // tmp_total_offset = tmp_dim1_flag * tmp_col_len + tmp_dim2_flag
        // load tmp_load_val, var_flag[tmp_total_offset]
        res.push_back(new ir::Instruction(ir::Operand(exp1->v, exp1->t), ir::Operand(), ir::Operand(tmp_dim1_flag, Type::Int), ir::Operator::def));
        res.push_back(new ir::Instruction(ir::Operand(exp2->v, exp2->t), ir::Operand(), ir::Operand(tmp_dim2_flag, Type::Int), ir::Operator::def));
        res.push_back(new ir::Instruction(ir::Operand(std::to_string(operand_ste.dimension[1]), Type::IntLiteral), ir::Operand(), ir::Operand(tmp_col_len, Type::Int), ir::Operator::def));
        res.push_back(new ir::Instruction(ir::Operand(tmp_dim1_flag, Type::Int), ir::Operand(tmp_col_len, Type::Int), ir::Operand(tmp_line_offset, Type::Int), ir::Operator::mul));
        res.push_back(new ir::Instruction(ir::Operand(tmp_line_offset, Type::Int), ir::Operand(tmp_dim2_flag, Type::Int), ir::Operand(tmp_total_offset, Type::Int), ir::Operator::add));
        res.push_back(new ir::Instruction(ir::Operand(SCOPED(var_flag), operand_ste.operand.type), ir::Operand(tmp_total_offset, Type::Int), ir::Operand(tmp_load_val, target_type), ir::Operator::load));
        FILL_NODE(root, tmp_load_val, target_type);
        return res;
    }

    return res;
}

// Number -> IntConst | floatConst
// Just update the type and value of Number, do not generate ir code
vector<ir::Instruction *> frontend::Analyzer::analyzeNumber(Number *root)
{
    GET_CHILD_PTR(term, Term, 0);

    if (term->token.type == TokenType::INTLTR)
    { // IntConst
        root->t = Type::IntLiteral;
        const string &token_val = term->token.value; // Integer literal
        if (token_val.length() >= 3 && token_val[0] == '0' && (token_val[1] == 'x' || token_val[1] == 'X'))
        { // hexadecimal
            root->v = std::to_string(std::stoi(token_val, nullptr, 16));
        }
        else if (token_val.length() >= 3 && token_val[0] == '0' && (token_val[1] == 'b' || token_val[1] == 'B'))
        { // binary
            root->v = std::to_string(std::stoi(token_val.substr(2), nullptr, 2));
        }
        else if (token_val.length() >= 2 && token_val[0] == '0')
        { // octal
            root->v = std::to_string(std::stoi(token_val, nullptr, 8));
        }
        else
            root->v = token_val;
    }

    else if (term->token.type == TokenType::FLOATLTR)
    {
        FILL_NODE(root, term->token.value, Type::FloatLiteral);
        const string &token_val = term->token.value; // Float literal
        if (token_val.find('e') != string::npos || token_val.find('E') != string::npos || token_val[0] == '.')
        { // scientific notation
            root->v = std::to_string(std::stof(token_val));
        }
        else
        {
            root->v = token_val;
        }
    }

    return std::vector<ir::Instruction *>();
}

// Cond -> LOrExp
vector<ir::Instruction *> frontend::Analyzer::analyzeCond(Cond *root)
{
    GET_CHILD_PTR(lorexp, LOrExp, 0);
    vector<ir::Instruction *> insts = analyzeLOrExp(lorexp);
    COPY_EXP_NODE(root, lorexp);
    return insts;
}

// LOrExp -> LAndExp [ '||' LOrExp ]
vector<ir::Instruction *> frontend::Analyzer::analyzeLOrExp(LOrExp *root)
{
    vector<ir::Instruction *> res;

    GET_CHILD_PTR(landexp, LAndExp, 0);
    vector<ir::Instruction *> insts = analyzeLAndExp(landexp); // get LAndExp
    INSERT_IRS(res, insts);
    COPY_EXP_NODE(root, landexp);

    if (root->children.size() == 1)
    { // only LAndExp, you can return now
        return res;
    }
    else
    { // LAndExp '||' LOrExp
        GET_CHILD_PTR(lorexp, LOrExp, 2);
        vector<ir::Instruction *> insts = analyzeLOrExp(lorexp); // wait for result of LAndExp

        if (root->t == Type::Float)
        { // root = (LAndExp != 0), into int
            string tmp = GET_NEXT_TEMP_VAR();
            res.push_back(new ir::Instruction(ir::Operand(root->v, Type::Float), ir::Operand("0.0", Type::FloatLiteral), ir::Operand(tmp, Type::Int), ir::Operator::fneq));
            FILL_NODE(root, tmp, Type::Int);
        }
        else if (root->t == Type::FloatLiteral)
        {
            float val = std::stof(root->v);
            FILL_NODE(root, std::to_string(val != 0), Type::IntLiteral);
        }

        if (lorexp->t == Type::Float)
        { // lorexp = (LOrExp != 0), into int
            string tmp = GET_NEXT_TEMP_VAR();
            insts.push_back(new ir::Instruction(ir::Operand(lorexp->v, Type::Float), ir::Operand("0.0", Type::FloatLiteral), ir::Operand(tmp, Type::Int), ir::Operator::fneq));
            FILL_NODE(lorexp, tmp, Type::Int);
        }
        else if (lorexp->t == Type::FloatLiteral)
        {
            float val = std::stof(lorexp->v);
            FILL_NODE(lorexp, std::to_string(val != 0), Type::IntLiteral);
        }

        if (root->t == Type::IntLiteral && lorexp->t == Type::IntLiteral)
        {
            root->v = std::to_string(std::stoi(root->v) || std::stoi(lorexp->v));
        }
        else
        {
            /**
             * Short-circuit evaluation:
             * get LAndExp
             * if (LAndExp == 0) goto [pc, 2]
             * goto [pc, 3]  << goto analyze_lorexp
             * tmp_cal_flag = 1
             * goto [pc, len(insts) + 1] << goto (END)
             *  get LOrExp
             *  tmp_cal_flag = LAndExp || LOrExp
             * (END)
             */
            string tmp_cal_flag = GET_NEXT_TEMP_VAR(); // to record the result of LOrExp
            insts.push_back(new Instruction(ir::Operand(root->v, root->t), ir::Operand(lorexp->v, lorexp->t), ir::Operand(tmp_cal_flag, ir::Type::Int), ir::Operator::_or));

            res.push_back(new Instruction(ir::Operand(root->v, root->t), ir::Operand(), ir::Operand("2", Type::IntLiteral), ir::Operator::_goto));
            res.push_back(new Instruction(ir::Operand(), ir::Operand(), ir::Operand("3", Type::IntLiteral), ir::Operator::_goto));
            res.push_back(new Instruction(ir::Operand("1", Type::IntLiteral), ir::Operand(), ir::Operand(tmp_cal_flag, ir::Type::Int), ir::Operator::mov));
            res.push_back(new Instruction(ir::Operand(), ir::Operand(), ir::Operand(std::to_string(insts.size() + 1), Type::IntLiteral), ir::Operator::_goto));
            INSERT_IRS(res, insts);
            FILL_NODE(root, tmp_cal_flag, Type::Int); // set root->v = tmp_cal_flag
        }
        return res;
    }
}

// LAndExp -> EqExp [ '&&' LAndExp ]
vector<ir::Instruction *> frontend::Analyzer::analyzeLAndExp(LAndExp *root)
{
    vector<ir::Instruction *> res;
    GET_CHILD_PTR(eqexp, EqExp, 0);
    vector<ir::Instruction *> insts = analyzeEqExp(eqexp);
    INSERT_IRS(res, insts);
    COPY_EXP_NODE(root, eqexp); // copy EqExp to LAndExp

    if (root->children.size() == 1)
    { // only EqExp, you can return now
        return res;
    }
    else
    {
        GET_CHILD_PTR(landexp, LAndExp, 2);
        vector<ir::Instruction *> insts = analyzeLAndExp(landexp);

        if (root->t == Type::Float)
        {
            string tmp = GET_NEXT_TEMP_VAR();
            res.push_back(new ir::Instruction(ir::Operand(root->v, Type::Float), ir::Operand("0.0", Type::FloatLiteral), ir::Operand(tmp, Type::Int), ir::Operator::fneq));
            FILL_NODE(landexp, tmp, Type::Int);
        }
        else if (root->t == Type::FloatLiteral)
        { // root->v = (EqExp != 0), into int
            float val = std::stof(root->v);
            FILL_NODE(root, std::to_string(val != 0), Type::IntLiteral);
        }

        if (landexp->t == Type::Float)
        {
            string tmp = GET_NEXT_TEMP_VAR();
            insts.push_back(new ir::Instruction(ir::Operand(landexp->v, Type::Float), ir::Operand("0.0", Type::FloatLiteral), ir::Operand(tmp, Type::Int), ir::Operator::fneq));
            FILL_NODE(landexp, tmp, Type::Int);
        }
        else if (landexp->t == Type::FloatLiteral)
        {
            float val = std::stof(landexp->v);
            FILL_NODE(landexp, std::to_string(val != 0), Type::IntLiteral);
        }

        if (root->t == Type::IntLiteral && landexp->t == Type::IntLiteral)
        {
            root->v = std::to_string(std::stoi(root->v) && std::stoi(landexp->v));
        }
        else
        {
            /**
             * Short-circuit evaluation:
             * get EqExp
             * if (EqExp == 0) goto [pc, 2]
             * goto [pc, len(insts) + 1]  << goto analyze_landexp 👉
             *  LAndExp
             *  tmp_cal_flag = EqExp && LAndExp
             *  goto [pc, 2]
             *  tmp_cal_flag = 0 << (last pc + length of insts) 👈
             * (END)
             */
            string tmp_cal_flag = GET_NEXT_TEMP_VAR();

            res.push_back(new Instruction(ir::Operand(root->v, root->t), ir::Operand(), ir::Operand("2", Type::IntLiteral), ir::Operator::_goto));
            insts.push_back(new Instruction(ir::Operand(root->v, root->t), ir::Operand(landexp->v, landexp->t), ir::Operand(tmp_cal_flag, ir::Type::Int), ir::Operator::_and));
            insts.push_back(new Instruction(ir::Operand(), ir::Operand(), ir::Operand("2", Type::IntLiteral), ir::Operator::_goto));

            res.push_back(new Instruction(ir::Operand(), ir::Operand(), ir::Operand(std::to_string(insts.size() + 1), Type::IntLiteral), ir::Operator::_goto));
            INSERT_IRS(res, insts);
            res.push_back(new Instruction(ir::Operand("0", Type::IntLiteral), ir::Operand(), ir::Operand(tmp_cal_flag, ir::Type::Int), ir::Operator::mov));

            FILL_NODE(root, tmp_cal_flag, Type::Int);
        }
        return res;
    }
}

// EqExp -> RelExp { ('==' | '!=') RelExp }
vector<ir::Instruction *> frontend::Analyzer::analyzeEqExp(EqExp *root)
{
    vector<Instruction *> res;

    for (int i = 0; i < root->children.size(); i += 2)
    { // get RelExp
        GET_CHILD_PTR(relexp, RelExp, i);
        vector<Instruction *> insts = analyzeRelExp(relexp);
        INSERT_IRS(res, insts);
    }

    GET_CHILD_PTR(relexp0, RelExp, 0);
    COPY_EXP_NODE(root, relexp0);

    if (root->children.size() == 1)
        return res;

    for (int i = 2; i < root->children.size(); i += 2)
    {
        GET_CHILD_PTR(op_term, Term, i - 1);
        GET_CHILD_PTR(relexp, RelExp, i);
        Type target_type = root->t;

        if (root->t != relexp->t)
        {
            if (relexp->t == ir::Type::Float)
                target_type = ir::Type::Float;
            else if (relexp->t == ir::Type::Int && target_type == ir::Type::IntLiteral)
                target_type = ir::Type::Int;
            else if (relexp->t == ir::Type::FloatLiteral && target_type == ir::Type::IntLiteral)
                target_type = ir::Type::FloatLiteral;
            else if ((relexp->t == ir::Type::FloatLiteral && target_type == ir::Type::Int) || (target_type == ir::Type::FloatLiteral && relexp->t == ir::Type::Int))
                target_type = ir::Type::Float;

            if (target_type == Type::Int)
            {
                EqExp *rt = dynamic_cast<EqExp *>(root);
                RelExp *chd = dynamic_cast<RelExp *>(relexp);
                if (chd->t == Type::IntLiteral)
                {
                    string tmp_intcvt_flag = GET_NEXT_TEMP_VAR();
                    res.push_back(new Instruction(ir::Operand(chd->v, ir::Type::IntLiteral), ir::Operand(), ir::Operand(tmp_intcvt_flag, ir::Type::Int), ir::Operator::def));
                    FILL_NODE(chd, tmp_intcvt_flag, Type::Int);
                }
                if (rt->t == Type::IntLiteral)
                {
                    string tmp_intcvt_flag = GET_NEXT_TEMP_VAR();
                    res.push_back(new Instruction(ir::Operand(rt->v, ir::Type::IntLiteral), ir::Operand(), ir::Operand(tmp_intcvt_flag, ir::Type::Int), ir::Operator::def));
                    FILL_NODE(rt, tmp_intcvt_flag, Type::Int);
                }
            }
            else if (target_type == Type::FloatLiteral)
            {
                EqExp *rt = dynamic_cast<EqExp *>(root);
                RelExp *chd = dynamic_cast<RelExp *>(relexp);
                if (chd->t == Type::IntLiteral)
                {
                    float val = std::stoi(chd->v);
                    FILL_NODE(chd, std::to_string(val), Type::FloatLiteral);
                }
                if (rt->t == Type::IntLiteral)
                {
                    float val = std::stoi(rt->v);
                    FILL_NODE(rt, std::to_string(val), Type::FloatLiteral);
                }
            }
            else if (target_type == Type::Float)
            {
                EqExp *rt = dynamic_cast<EqExp *>(root);
                RelExp *chd = dynamic_cast<RelExp *>(relexp);
                if (chd->t == Type::IntLiteral)
                {
                    float val = std::stof(chd->v);
                    string tmp_intcvt_flag = GET_NEXT_TEMP_VAR();
                    res.push_back(new Instruction(ir::Operand(std::to_string(val), ir::Type::FloatLiteral), ir::Operand(), ir::Operand(tmp_intcvt_flag, ir::Type::Float), ir::Operator::fdef));
                    FILL_NODE(chd, tmp_intcvt_flag, Type::Float);
                }
                else if (chd->t == Type::Int)
                {
                    string tmp_intcvt_flag = GET_NEXT_TEMP_VAR();
                    res.push_back(new Instruction(ir::Operand(chd->v, ir::Type::Int), ir::Operand(), ir::Operand(tmp_intcvt_flag, ir::Type::Float), ir::Operator::cvt_i2f));
                    FILL_NODE(chd, tmp_intcvt_flag, Type::Float);
                }
                else if (chd->t == Type::FloatLiteral)
                {
                    string tmp_intcvt_flag = GET_NEXT_TEMP_VAR();
                    res.push_back(new Instruction(ir::Operand(chd->v, ir::Type::FloatLiteral), ir::Operand(), ir::Operand(tmp_intcvt_flag, ir::Type::Float), ir::Operator::fdef));
                    FILL_NODE(chd, tmp_intcvt_flag, Type::Float);
                }

                if (rt->t == Type::IntLiteral)
                {
                    float val = std::stof(rt->v);
                    string tmp_intcvt_flag = GET_NEXT_TEMP_VAR();
                    res.push_back(new Instruction(ir::Operand(std::to_string(val), ir::Type::FloatLiteral), ir::Operand(), ir::Operand(tmp_intcvt_flag, ir::Type::Float), ir::Operator::fdef));
                    FILL_NODE(rt, tmp_intcvt_flag, Type::Float);
                }
                else if (rt->t == Type::Int)
                {
                    string tmp_intcvt_flag = GET_NEXT_TEMP_VAR();
                    res.push_back(new Instruction(ir::Operand(rt->v, ir::Type::Int), ir::Operand(), ir::Operand(tmp_intcvt_flag, ir::Type::Float), ir::Operator::cvt_i2f));
                    FILL_NODE(root, tmp_intcvt_flag, Type::Float);
                }
                else if (rt->t == Type::FloatLiteral)
                {
                    string tmp_intcvt_flag = GET_NEXT_TEMP_VAR();
                    res.push_back(new Instruction(ir::Operand(rt->v, ir::Type::FloatLiteral), ir::Operand(), ir::Operand(tmp_intcvt_flag, ir::Type::Float), ir::Operator::fdef));
                    FILL_NODE(root, tmp_intcvt_flag, Type::Float);
                }
            }
        }

        if (target_type == Type::IntLiteral)
        {
            int val1 = std::stoi(root->v);
            int val2 = std::stoi(relexp->v);
            if (op_term->token.type == TokenType::EQL)
                root->v = std::to_string(val1 == val2);
            else if (op_term->token.type == TokenType::NEQ)
                root->v = std::to_string(val1 != val2);
        }
        else if (target_type == Type::FloatLiteral)
        {
            float val1 = std::stof(root->v);
            float val2 = std::stof(relexp->v);
            if (op_term->token.type == TokenType::EQL)
                root->v = std::to_string(val1 == val2);
            else if (op_term->token.type == TokenType::NEQ)
                root->v = std::to_string(val1 != val2);
            root->t = Type::IntLiteral;
        }
        else if (target_type == Type::Int)
        {
            string tmp_cal_flag = GET_NEXT_TEMP_VAR();
            res.push_back(
                op_term->token.type == TokenType::EQL
                    ? new Instruction(ir::Operand(root->v, ir::Type::Int), ir::Operand(relexp->v, ir::Type::Int), ir::Operand(tmp_cal_flag, ir::Type::Int), ir::Operator::eq)
                    : new Instruction(ir::Operand(root->v, ir::Type::Int), ir::Operand(relexp->v, ir::Type::Int), ir::Operand(tmp_cal_flag, ir::Type::Int), ir::Operator::neq));
            FILL_NODE(root, tmp_cal_flag, Type::Int);
        }
        else if (target_type == Type::Float)
        {
            string tmp_cal_flag = GET_NEXT_TEMP_VAR();
            res.push_back(
                op_term->token.type == TokenType::EQL
                    ? new Instruction(ir::Operand(root->v, ir::Type::Float), ir::Operand(relexp->v, ir::Type::Float), ir::Operand(tmp_cal_flag, ir::Type::Int), ir::Operator::feq)
                    : new Instruction(ir::Operand(root->v, ir::Type::Float), ir::Operand(relexp->v, ir::Type::Float), ir::Operand(tmp_cal_flag, ir::Type::Int), ir::Operator::fneq));
            FILL_NODE(root, tmp_cal_flag, Type::Int);
        }
    }
    return res;
}

vector<ir::Instruction *> frontend::Analyzer::analyzeRelExp(RelExp *root)
{
    vector<Instruction *> res;

    for (int i = 0; i < root->children.size(); i += 2)
    {
        GET_CHILD_PTR(addexp, AddExp, i);
        vector<Instruction *> insts = analyzeAddExp(addexp);
        INSERT_IRS(res, insts);
    }

    GET_CHILD_PTR(addexp0, AddExp, 0);
    COPY_EXP_NODE(root, addexp0);

    if (root->children.size() == 1)
        return res;

    for (int i = 2; i < root->children.size(); i += 2)
    {
        GET_CHILD_PTR(op_term, Term, i - 1);
        GET_CHILD_PTR(addexp, AddExp, i);
        Type target_type = root->t;

        if (root->t != addexp->t)
        {
            if (addexp->t == ir::Type::Float)
                target_type = ir::Type::Float;
            else if (addexp->t == ir::Type::Int && target_type == ir::Type::IntLiteral)
                target_type = ir::Type::Int;
            else if (addexp->t == ir::Type::FloatLiteral && target_type == ir::Type::IntLiteral)
                target_type = ir::Type::FloatLiteral;
            else if ((addexp->t == ir::Type::FloatLiteral && target_type == ir::Type::Int) || (target_type == ir::Type::FloatLiteral && addexp->t == ir::Type::Int))
                target_type = ir::Type::Float;

            if (target_type == Type::Int)
            {
                RelExp *rt = dynamic_cast<RelExp *>(root);
                AddExp *chd = dynamic_cast<AddExp *>(addexp);
                if (chd->t == Type::IntLiteral)
                {
                    string tmp_intcvt_flag = GET_NEXT_TEMP_VAR();
                    res.push_back(new Instruction(ir::Operand(chd->v, ir::Type::IntLiteral), ir::Operand(), ir::Operand(tmp_intcvt_flag, ir::Type::Int), ir::Operator::def));
                    FILL_NODE(chd, tmp_intcvt_flag, Type::Int);
                }
                if (rt->t == Type::IntLiteral)
                {
                    string tmp_intcvt_flag = GET_NEXT_TEMP_VAR();
                    res.push_back(new Instruction(ir::Operand(rt->v, ir::Type::IntLiteral), ir::Operand(), ir::Operand(tmp_intcvt_flag, ir::Type::Int), ir::Operator::def));
                    FILL_NODE(rt, tmp_intcvt_flag, Type::Int);
                }
            }
            else if (target_type == Type::FloatLiteral)
            {
                RelExp *rt = dynamic_cast<RelExp *>(root);
                AddExp *chd = dynamic_cast<AddExp *>(addexp);
                if (chd->t == Type::IntLiteral)
                {
                    FILL_NODE(chd, std::to_string((float)(std::stoi(chd->v))), Type::FloatLiteral);
                }
                if (rt->t == Type::IntLiteral)
                {
                    FILL_NODE(rt, std::to_string((float)(std::stoi(rt->v))), Type::FloatLiteral);
                }
            }
            else if (target_type == Type::Float)
            {
                RelExp *rt = dynamic_cast<RelExp *>(root);
                AddExp *chd = dynamic_cast<AddExp *>(addexp);
                if (chd->t == Type::IntLiteral)
                {
                    string tmp_intcvt_flag = GET_NEXT_TEMP_VAR();
                    res.push_back(new Instruction(ir::Operand(std::to_string((float)(std::stof(chd->v))), ir::Type::FloatLiteral), ir::Operand(), ir::Operand(tmp_intcvt_flag, ir::Type::Float), ir::Operator::fdef));
                    FILL_NODE(chd, tmp_intcvt_flag, Type::Float);
                }
                else if (chd->t == Type::Int)
                {
                    string tmp_intcvt_flag = GET_NEXT_TEMP_VAR();
                    res.push_back(new Instruction(ir::Operand(chd->v, ir::Type::Int), ir::Operand(), ir::Operand(tmp_intcvt_flag, ir::Type::Float), ir::Operator::cvt_i2f));
                    FILL_NODE(chd, tmp_intcvt_flag, Type::Float);
                }
                else if (chd->t == Type::FloatLiteral)
                {
                    string tmp_intcvt_flag = GET_NEXT_TEMP_VAR();
                    res.push_back(new Instruction(ir::Operand(chd->v, ir::Type::FloatLiteral), ir::Operand(), ir::Operand(tmp_intcvt_flag, ir::Type::Float), ir::Operator::fdef));
                    FILL_NODE(chd, tmp_intcvt_flag, Type::Float);
                }

                if (rt->t == Type::IntLiteral)
                {
                    float val = std::stof(rt->v);
                    string tmp_intcvt_flag = GET_NEXT_TEMP_VAR();
                    res.push_back(new Instruction(ir::Operand(std::to_string(val), ir::Type::FloatLiteral), ir::Operand(), ir::Operand(tmp_intcvt_flag, ir::Type::Float), ir::Operator::fdef));
                    FILL_NODE(rt, tmp_intcvt_flag, Type::Float);
                }
                else if (rt->t == Type::Int)
                {
                    string tmp_intcvt_flag = GET_NEXT_TEMP_VAR();
                    res.push_back(new Instruction(ir::Operand(rt->v, ir::Type::Int), ir::Operand(), ir::Operand(tmp_intcvt_flag, ir::Type::Float), ir::Operator::cvt_i2f));
                    FILL_NODE(rt, tmp_intcvt_flag, Type::Float);
                }
                else if (rt->t == Type::FloatLiteral)
                {
                    string tmp_intcvt_flag = GET_NEXT_TEMP_VAR();
                    res.push_back(new Instruction(ir::Operand(rt->v, ir::Type::FloatLiteral), ir::Operand(), ir::Operand(tmp_intcvt_flag, ir::Type::Float), ir::Operator::fdef));
                    FILL_NODE(root, tmp_intcvt_flag, Type::Float);
                }
            }
        }

        if (target_type == Type::IntLiteral)
        {
            int val1 = std::stoi(root->v);
            int val2 = std::stoi(addexp->v);
            if (op_term->token.type == TokenType::LSS)
                root->v = std::to_string(val1 < val2);
            else if (op_term->token.type == TokenType::GTR)
                root->v = std::to_string(val1 > val2);
            else if (op_term->token.type == TokenType::LEQ)
                root->v = std::to_string(val1 <= val2);
            else if (op_term->token.type == TokenType::GEQ)
                root->v = std::to_string(val1 >= val2);
        }
        else if (target_type == Type::FloatLiteral)
        {
            float val1 = std::stof(root->v);
            float val2 = std::stof(addexp->v);
            if (op_term->token.type == TokenType::LSS)
                root->v = std::to_string(val1 < val2);
            else if (op_term->token.type == TokenType::GTR)
                root->v = std::to_string(val1 > val2);
            else if (op_term->token.type == TokenType::LEQ)
                root->v = std::to_string(val1 <= val2);
            else if (op_term->token.type == TokenType::GEQ)
                root->v = std::to_string(val1 >= val2);
            root->t = Type::IntLiteral;
        }
        else if (target_type == Type::Int)
        {
            string tmp_cal_flag = GET_NEXT_TEMP_VAR();
            Instruction *inst;
            if (op_term->token.type == TokenType::LSS)
                inst = new Instruction(ir::Operand(root->v, ir::Type::Int), ir::Operand(addexp->v, ir::Type::Int), ir::Operand(tmp_cal_flag, ir::Type::Int), ir::Operator::lss);
            else if (op_term->token.type == TokenType::GTR)
                inst = new Instruction(ir::Operand(root->v, ir::Type::Int), ir::Operand(addexp->v, ir::Type::Int), ir::Operand(tmp_cal_flag, ir::Type::Int), ir::Operator::gtr);
            else if (op_term->token.type == TokenType::LEQ)
                inst = new Instruction(ir::Operand(root->v, ir::Type::Int), ir::Operand(addexp->v, ir::Type::Int), ir::Operand(tmp_cal_flag, ir::Type::Int), ir::Operator::leq);
            else if (op_term->token.type == TokenType::GEQ)
                inst = new Instruction(ir::Operand(root->v, ir::Type::Int), ir::Operand(addexp->v, ir::Type::Int), ir::Operand(tmp_cal_flag, ir::Type::Int), ir::Operator::geq);
            res.push_back(inst);
            FILL_NODE(root, tmp_cal_flag, Type::Int);
        }
        else if (target_type == Type::Float)
        {
            string tmp_cal_flag = GET_NEXT_TEMP_VAR();
            Instruction *inst;
            if (op_term->token.type == TokenType::LSS)
                inst = new Instruction(ir::Operand(root->v, ir::Type::Float), ir::Operand(addexp->v, ir::Type::Float), ir::Operand(tmp_cal_flag, ir::Type::Float), ir::Operator::flss);
            else if (op_term->token.type == TokenType::GTR)
                inst = new Instruction(ir::Operand(root->v, ir::Type::Float), ir::Operand(addexp->v, ir::Type::Float), ir::Operand(tmp_cal_flag, ir::Type::Float), ir::Operator::fgtr);
            else if (op_term->token.type == TokenType::LEQ)
                inst = new Instruction(ir::Operand(root->v, ir::Type::Float), ir::Operand(addexp->v, ir::Type::Float), ir::Operand(tmp_cal_flag, ir::Type::Float), ir::Operator::fleq);
            else if (op_term->token.type == TokenType::GEQ)
                inst = new Instruction(ir::Operand(root->v, ir::Type::Float), ir::Operand(addexp->v, ir::Type::Float), ir::Operand(tmp_cal_flag, ir::Type::Float), ir::Operator::fgeq);
            res.push_back(inst);
            FILL_NODE(root, tmp_cal_flag, Type::Float);
        }
    }
    return res;
}