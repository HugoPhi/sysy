#include "front/semantic.h"

#include <cassert>
#include <iostream>

using ir::Function;
using ir::Instruction;
using ir::Operand;
using ir::Operator;

#define TODO assert(0 && "TODO");

// 这玩意没用，因为有可能会出入更多的内容。
// #define GET_CHILD_PTR(node, type, index)                     \
//     auto node = dynamic_cast<type *>(root->children[index]); \
//     assert(node);
// #define ANALYSIS(node, type, index)                          \
//     auto node = dynamic_cast<type *>(root->children[index]); \
//     assert(node);                                            \
//     analysis##type(node, buffer);
// #define COPY_EXP_NODE(from, to)              \
//     to->is_computable = from->is_computable; \
//     to->v = from->v;                         \
//     to->t = from->t;

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
    scope_info.cnt = block_cnt;                              // id of current scope
    scope_info.name = "block" + std::to_string(block_cnt++); // scope name, block0, block1, ..., number will not reuse
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
            return id + "_" + scope_stack[i].name; // get the scoped name of the variable
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

frontend::Analyzer::Analyzer() : tmp_cnt(0), symbol_table() {}

ir::Program frontend::Analyzer::get_ir_program(CompUnit *root)
{
    symbol_table.add_scope();                                               // add global scope
    ir::Function *global_func = new ir::Function("global", ir::Type::null); // add global function to ir_program
    symbol_table.functions["global"] = global_func;

    auto lib_funcs = *get_lib_funcs(); // add lib_funcs to global symbol table
    for (auto it = lib_funcs.begin(); it != lib_funcs.end(); it++)
        symbol_table.functions[it->first] = it->second;

    analyzeCompUnit(root);

    for (auto it = symbol_table.scope_stack[0].table.begin(); it != symbol_table.scope_stack[0].table.end(); it++)
    { // add global variables to ir_program
        if (it->second.dimension.size() != 0)
        { // Array
            int arr_len = 1;
            for (int i = 0; i < it->second.dimension.size(); i++)
                arr_len *= it->second.dimension[i];
            ir_program.globalVal.push_back({{symbol_table.get_scoped_name(it->second.operand.name), it->second.operand.type}, arr_len});
        }
        else
        { // Variable
            // FloatLiteral -> Float; IntLiteral -> Int
            // if (it->second.operand.type == Type::FloatLiteral)
            //     ir_program.globalVal.push_back({{symbol_table.get_scoped_name(it->second.operand.name), Type::Float}});
            // else if (it->second.operand.type == Type::IntLiteral)
            //     ir_program.globalVal.push_back({{symbol_table.get_scoped_name(it->second.operand.name), Type::Int}});
            // else
            ir_program.globalVal.push_back({{symbol_table.get_scoped_name(it->second.operand.name), it->second.operand.type}});
        }
    }

    ir::Instruction *globalreturn = new ir::Instruction(ir::Operand(), ir::Operand(), ir::Operand(), ir::Operator::_return);
    global_func->addInst(globalreturn);
    ir_program.addFunction(*global_func);

    return ir_program;
}

// get the name of Ident
// Ident -> 'IDENFR'
string frontend::Analyzer::analyzeIdent(Term *root)
{
    return root->token.value;
}

// BType -> 'int' | 'float'
ir::Type frontend::Analyzer::analyzeBType(BType *root)
{
    Term *term = dynamic_cast<Term *>(root->children[0]);
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
    if (Decl *decl = dynamic_cast<Decl *>(root->children[0]))
    {                                                        // Decl
        vector<ir::Instruction *> insts = analyzeDecl(decl); // get IR instructions
        for (auto inst : insts)
            symbol_table.functions["global"]->addInst(inst); // add to global function, because decl is global
    }
    else if (FuncDef *funcdef = dynamic_cast<FuncDef *>(root->children[0]))
    { // FuncDef
        analyzeFuncDef(funcdef);
    }

    if (root->children.size() > 1)
    { // if size of children > 1, then there is a CompUnit
        CompUnit *sub_compunit = dynamic_cast<CompUnit *>(root->children[1]);
        analyzeCompUnit(sub_compunit);
    }
}

// Decl -> ConstDecl | VarDecl
vector<ir::Instruction *> frontend::Analyzer::analyzeDecl(Decl *root)
{
    if (ConstDecl *constdecl = dynamic_cast<ConstDecl *>(root->children[0]))
    { // ConstDecl
        return analyzeConstDecl(constdecl);
    }
    else if (VarDecl *vardecl = dynamic_cast<VarDecl *>(root->children[0]))
    { // VarDecl
        return analyzeVarDecl(vardecl);
    }

    return vector<ir::Instruction *>(); // should not reach here
}

// ConstDecl -> 'const' BType ConstDef { ',' ConstDef } ';'
vector<ir::Instruction *> frontend::Analyzer::analyzeConstDecl(ConstDecl *root)
{
    vector<ir::Instruction *> res;
    // BType
    auto btype = dynamic_cast<BType *>(root->children[1]);
    ir::Type type = analyzeBType(btype);
    // ConstDef { ',' ConstDef } ';'  <==> { ConstDef, ','|';' }
    for (int i = 2; i < root->children.size(); i += 2)
    {
        ConstDef *constdef = dynamic_cast<ConstDef *>(root->children[i]);
        vector<Instruction *> insts = analyzeConstDef(constdef, type); // get IR instructions of Const Var: type
        res.insert(res.end(), insts.begin(), insts.end());
    }

    return res;
}

// ConstDef -> Ident { '[' ConstExp ']' } '=' ConstInitVal
vector<ir::Instruction *> frontend::Analyzer::analyzeConstDef(ConstDef *root, ir::Type type)
{
    vector<ir::Instruction *> res;
    string var_flag = analyzeIdent(dynamic_cast<Term *>(root->children[0])); // var name

    // my strategy: use length of children to distinguish between different types, only 1 dim array and 0 dim array support.
    if (root->children.size() == 3)
    { // no arr， Ident '=' ConstInitVal
        STE var_ste;
        if (type == ir::Type::Int)
        {
            var_ste.operand = ir::Operand(var_flag, ir::Type::IntLiteral);
        }
        else if (type == ir::Type::Float)
        {
            var_ste.operand = ir::Operand(var_flag, ir::Type::FloatLiteral);
        }

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
            ir::Instruction *inst = new ir::Instruction(ir::Operand(std::to_string(val), ir::Type::IntLiteral), ir::Operand(), ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::Int), ir::Operator::def);
            res.push_back(inst);
        }
        else if (type == ir::Type::Float)
        {
            float val = std::stof(constexp->v);

            // ir: fdef var_flag##scope_id, "val"
            ir::Instruction *inst = new ir::Instruction(ir::Operand(std::to_string(val), ir::Type::FloatLiteral), ir::Operand(), ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::Float), ir::Operator::fdef);
            res.push_back(inst);
        }
    }
    else if (root->children.size() == 6)
    { // 1d arr: Ident '[' ConstExp ']' '=' ConstInitVal
        ConstExp *constexp = dynamic_cast<ConstExp *>(root->children[2]);
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
            ir::Instruction *inst = new ir::Instruction(ir::Operand(std::to_string(array_size), ir::Type::IntLiteral), ir::Operand(), ir::Operand(symbol_table.get_scoped_name(var_flag), curr_type), ir::Operator::alloc);
            res.push_back(inst);
        }

        ConstInitVal *constinit = dynamic_cast<ConstInitVal *>(root->children.back()); // ConstInitVal -> '{' [ ConstInitVal { ',' ConstInitVal } ] '}'
        vector<Instruction *> insts = analyzeConstInitVal(constinit, curr_type, array_size, var_flag);
        res.insert(res.end(), insts.begin(), insts.end());
    }
    else if (root->children.size() == 9)
    { // 2d arr: Ident '[' ConstExp ']' '[' ConstExp ']' '=' ConstInitVal
        ConstExp *constexp1 = dynamic_cast<ConstExp *>(root->children[2]);
        ConstExp *constexp2 = dynamic_cast<ConstExp *>(root->children[5]);

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
        { // ir: alloc var_flag##scope_id, "array_size"
            ir::Instruction *inst = new ir::Instruction(ir::Operand(std::to_string(array_size), ir::Type::IntLiteral), ir::Operand(), ir::Operand(symbol_table.get_scoped_name(var_flag), curr_type), ir::Operator::alloc);
            res.push_back(inst);
        }

        ConstInitVal *constinit = dynamic_cast<ConstInitVal *>(root->children.back());
        vector<Instruction *> insts = analyzeConstInitVal(constinit, curr_type, array_size, var_flag);
        res.insert(res.end(), insts.begin(), insts.end());
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
        ConstInitVal *child_constinit = dynamic_cast<ConstInitVal *>(root->children[i]);
        ConstExp *constexp = dynamic_cast<ConstExp *>(child_constinit->children[0]);

        analyzeConstExp(constexp);
        if (curr_type == ir::Type::IntPtr)
        { // int*
            int val;
            if (constexp->t == ir::Type::Float)
            { // up convert: Float -> Int
                val = std::stof(constexp->v);
            }
            else
            {
                val = std::stoi(constexp->v);
            }

            // ir: store var_flag##scope_id, i, "val"
            ir::Instruction *inst = new ir::Instruction(ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::IntPtr), ir::Operand(std::to_string(cnt), ir::Type::IntLiteral), ir::Operand(std::to_string(val), ir::Type::IntLiteral), ir::Operator::store);
            res.push_back(inst);
        }
        else if (curr_type == ir::Type::FloatPtr)
        { // float*
            float val = std::stof(constexp->v);

            // ir: store var_flag##scope_id, i, "val"
            ir::Instruction *inst = new ir::Instruction(ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::FloatPtr), ir::Operand(std::to_string(cnt), ir::Type::IntLiteral), ir::Operand(std::to_string(val), ir::Type::FloatLiteral), ir::Operator::store);
            res.push_back(inst);
        }
    }

    if (symbol_table.scope_stack.size() > 1)
    {
        for (int i = cnt; i < array_size; i++)
        {
            ir::Instruction *inst;
            if (curr_type == ir::Type::FloatPtr)
                // ir: store "0.0", var_flag##scope_id, i
                inst = new ir::Instruction(ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::FloatPtr), ir::Operand(std::to_string(i), ir::Type::IntLiteral), ir::Operand("0.0", ir::Type::FloatLiteral), ir::Operator::store);
            else if (curr_type == ir::Type::IntPtr)
                // ir: store "0", var_flag##scope_id, i
                inst = new ir::Instruction(ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::IntPtr), ir::Operand(std::to_string(i), ir::Type::IntLiteral), ir::Operand("0", ir::Type::IntLiteral), ir::Operator::store);

            res.push_back(inst);
        }
    }
    return res;
}

// VarDecl -> BType VarDef { ',' VarDef } ';'
vector<ir::Instruction *> frontend::Analyzer::analyzeVarDecl(VarDecl *root)
{
    vector<ir::Instruction *> res;
    auto btype = dynamic_cast<BType *>(root->children[0]);
    ir::Type type = analyzeBType(btype);

    for (int i = 1; i < root->children.size(); i += 2)
    { // VarDef { ',' VarDef } <==> { VarDef ','|';' }
        VarDef *vardef = dynamic_cast<VarDef *>(root->children[i]);
        vector<Instruction *> insts = analyzeVarDef(vardef, type);
        res.insert(res.end(), insts.begin(), insts.end());
    }
    return res;
}

// VarDef -> Ident { '[' ConstExp ']' } [ '=' InitVal ]
vector<ir::Instruction *> frontend::Analyzer::analyzeVarDef(VarDef *root, ir::Type type)
{
    vector<ir::Instruction *> res;

    string var_flag = analyzeIdent(dynamic_cast<Term *>(root->children[0]));

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
            res.insert(res.end(), insts.begin(), insts.end());

            if (type == ir::Type::Int)
            { // int
                if (exp->t == ir::Type::Int || exp->t == ir::Type::IntLiteral)
                {
                    // ir: def var_flag##scope_id, "exp->v"
                    ir::Instruction *inst = new ir::Instruction(ir::Operand(exp->v, exp->t), ir::Operand(), ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::Int), ir::Operator::def);
                    res.push_back(inst);
                }
                else if (exp->t == ir::Type::Float || exp->t == ir::Type::FloatLiteral)
                { // down convert: Float -> Int
                    string tmp_intcvt_flag = "__temp_var_" + std::to_string(tmp_cnt++);
                    if (exp->t == ir::Type::Float)
                    { // Float
                        // FIXME: why int vat exp->v can be used to def var_flag##scope_id?
                        // ir: cvt_f2i __temp_var, exp->v
                        ir::Instruction *inst1 = new ir::Instruction(ir::Operand(exp->v, ir::Type::FloatLiteral), ir::Operand(), ir::Operand(tmp_intcvt_flag, ir::Type::IntLiteral), ir::Operator::cvt_f2i);
                        // ir: def var_flag##scope_id, __temp_var
                        ir::Instruction *inst2 = new ir::Instruction(ir::Operand(tmp_intcvt_flag, ir::Type::IntLiteral), ir::Operand(), ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::Int), ir::Operator::def);
                        res.push_back(inst1);
                        res.push_back(inst2);
                    }
                    else
                    { // FloatLiteral
                        int val = std::stof(exp->v);
                        // ir: def var_flag##scope_id, "val"
                        ir::Instruction *inst = new ir::Instruction(ir::Operand(std::to_string(val), ir::Type::IntLiteral), ir::Operand(), ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::Int), ir::Operator::def);
                        res.push_back(inst);
                    }
                }
            }
            else
            { // float
                if (exp->t == ir::Type::IntLiteral || exp->t == ir::Type::FloatLiteral)
                { // if constexp is a literal, then we can use it directly
                    float val = std::stof(exp->v);
                    ir::Instruction *inst = new ir::Instruction(ir::Operand(std::to_string(val), ir::Type::FloatLiteral), ir::Operand(), ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::Float), ir::Operator::fdef);
                    res.push_back(inst);
                }
                else if (exp->t == ir::Type::Int)
                { // if constexp is a variable, then we need to convert it to float
                    string curr_tmp_flag = "__temp_var_" + std::to_string(tmp_cnt++);
                    ir::Instruction *cvtInst = new ir::Instruction(ir::Operand(exp->v, ir::Type::Int), ir::Operand(), ir::Operand(curr_tmp_flag, ir::Type::Float), ir::Operator::cvt_i2f);
                    ir::Instruction *defInst = new ir::Instruction(ir::Operand(curr_tmp_flag, ir::Type::Float), ir::Operand(), ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::Float), ir::Operator::fdef);
                    res.push_back(cvtInst);
                    res.push_back(defInst);
                }
                else if (exp->t == ir::Type::Float)
                { // if constexp is a variable, then we can use it directly
                    ir::Instruction *inst = new ir::Instruction(ir::Operand(exp->v, ir::Type::Float), ir::Operand(), ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::Float), ir::Operator::fdef);
                    res.push_back(inst);
                }
            }
        }
        else
        { // Ident, init as "0" or "0.0"
            if (type == ir::Type::Int)
            {
                // ir: def var_flag##scope_id, "0"
                ir::Instruction *inst = new ir::Instruction(ir::Operand("0", ir::Type::IntLiteral), ir::Operand(), ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::Int), ir::Operator::def);
                res.push_back(inst);
            }
            else if (type == ir::Type::Float)
            {
                // ir: fdef var_flag##scope_id, "0.0"
                ir::Instruction *inst = new ir::Instruction(ir::Operand("0.0", ir::Type::FloatLiteral), ir::Operand(), ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::Float), ir::Operator::fdef);
                res.push_back(inst);
            }
        }
    }

    else if (root->children.size() == 4 || root->children.size() == 6)
    { // 1d arr: Ident '[' ConstExp ']' or Ident '[' ConstExp ']' '=' InitVal
        ConstExp *constexp = dynamic_cast<ConstExp *>(root->children[2]);
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
            ir::Instruction *allocInst = new ir::Instruction(ir::Operand(std::to_string(array_size), ir::Type::IntLiteral), ir::Operand(), ir::Operand(symbol_table.get_scoped_name(var_flag), curr_type), ir::Operator::alloc);
            res.push_back(allocInst);
        }

        if (InitVal *initval = dynamic_cast<InitVal *>(root->children.back()))
        {
            vector<Instruction *> insts = analyzeInitVal(initval, curr_type, array_size, var_flag);
            res.insert(res.end(), insts.begin(), insts.end());
        }
    }

    else if (root->children.size() == 7 || root->children.size() == 9)
    { // 2d arr: Ident '[' ConstExp ']' '[' ConstExp ']' or Ident '[' ConstExp ']' '[' ConstExp ']' '=' InitVal
        ConstExp *constexp1 = dynamic_cast<ConstExp *>(root->children[2]);
        ConstExp *constexp2 = dynamic_cast<ConstExp *>(root->children[5]);

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
            ir::Instruction *allocInst = new ir::Instruction(ir::Operand(std::to_string(array_size), ir::Type::IntLiteral), ir::Operand(), ir::Operand(symbol_table.get_scoped_name(var_flag), curr_type), ir::Operator::alloc);
            res.push_back(allocInst);
        }

        if (InitVal *initval = dynamic_cast<InitVal *>(root->children.back()))
        { // init
            vector<Instruction *> insts = analyzeInitVal(initval, curr_type, array_size, var_flag);
            res.insert(res.end(), insts.begin(), insts.end());
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
        InitVal *child_initval = dynamic_cast<InitVal *>(root->children[i]);
        Exp *exp = dynamic_cast<Exp *>(child_initval->children[0]); // InitVal -> Exp
        vector<Instruction *> insts = analyzeExp(exp);
        res.insert(res.end(), insts.begin(), insts.end());
        if (curr_type == ir::Type::IntPtr)
        { // int*
            if (exp->t == Type::IntLiteral || exp->t == Type::Int)
            {
                ir::Instruction *inst = new ir::Instruction(ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::IntPtr), ir::Operand(std::to_string(cnt), ir::Type::IntLiteral), ir::Operand(exp->v, exp->t), ir::Operator::store);
                res.push_back(inst);
            }
            else
            {
                if (exp->t == Type::FloatLiteral)
                { // FloatLiteral -> Int
                    int val = std::stof(exp->v);
                    ir::Instruction *inst = new ir::Instruction(ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::IntPtr), ir::Operand(std::to_string(cnt), ir::Type::IntLiteral), ir::Operand(std::to_string(val), Type::IntLiteral), ir::Operator::store);
                    res.push_back(inst);
                }
                else
                {
                    string tmp_floatcvt_flag = "__temp_var_";
                    tmp_floatcvt_flag += std::to_string(tmp_cnt++);
                    ir::Instruction *inst1 = new ir::Instruction(ir::Operand(exp->v, ir::Type::Float), ir::Operand(), ir::Operand(tmp_floatcvt_flag, ir::Type::Int), ir::Operator::cvt_f2i);
                    ir::Instruction *inst2 = new ir::Instruction(ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::IntPtr), ir::Operand(std::to_string(cnt), ir::Type::IntLiteral), ir::Operand(tmp_floatcvt_flag, Type::Int), ir::Operator::store);
                    res.push_back(inst1);
                    res.push_back(inst2);
                }
            }
        }
        else if (curr_type == ir::Type::FloatPtr)
        { // float*
            if (exp->t == Type::IntLiteral || exp->t == Type::FloatLiteral)
            {
                float val = std::stof(exp->v);
                ir::Instruction *storeInst = new ir::Instruction(ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::FloatPtr), ir::Operand(std::to_string(cnt), ir::Type::IntLiteral), ir::Operand(std::to_string(val), ir::Type::FloatLiteral), ir::Operator::store);
                res.push_back(storeInst);
            }
            else if (exp->t == Type::Int)
            { // Int -> Float
                string tmp_intcvt_flag = "__temp_var_";
                tmp_intcvt_flag += std::to_string(tmp_cnt++);
                ir::Instruction *inst1 = new ir::Instruction(ir::Operand(exp->v, ir::Type::Int), ir::Operand(), ir::Operand(tmp_intcvt_flag, ir::Type::Float), ir::Operator::cvt_i2f);
                ir::Instruction *inst2 = new ir::Instruction(ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::FloatPtr), ir::Operand(std::to_string(cnt), ir::Type::IntLiteral), ir::Operand(tmp_intcvt_flag, ir::Type::Float), ir::Operator::store);
                res.push_back(inst1);
                res.push_back(inst2);
            }
            else if (exp->t == Type::Float)
            {
                ir::Instruction *inst = new ir::Instruction(ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::FloatPtr), ir::Operand(std::to_string(cnt), ir::Type::IntLiteral), ir::Operand(exp->v, ir::Type::Float), ir::Operator::store);
                res.push_back(inst);
            }
        }
    }

    if (symbol_table.scope_stack.size() > 1)
    {
        for (int i = cnt; i < array_size; i++)
        {
            ir::Instruction *inst;
            if (curr_type == ir::Type::FloatPtr)
                inst = new ir::Instruction(ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::FloatPtr), ir::Operand(std::to_string(i), ir::Type::IntLiteral), ir::Operand("0.0", ir::Type::FloatLiteral), ir::Operator::store);
            else if (curr_type == ir::Type::IntPtr)
                inst = new ir::Instruction(ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::IntPtr), ir::Operand(std::to_string(i), ir::Type::IntLiteral), ir::Operand("0", ir::Type::IntLiteral), ir::Operator::store);

            res.push_back(inst);
        }
    }

    return res;
}

// FuncDef -> FuncType Ident '(' [FuncFParams] ')' Block
void frontend::Analyzer::analyzeFuncDef(FuncDef *root)
{
    ir::Type func_type = analyzeFuncType(dynamic_cast<FuncType *>(root->children[0]));
    string func_flag = analyzeIdent(dynamic_cast<Term *>(root->children[1]));

    symbol_table.add_scope(); // fuck into scope for function
    vector<ir::Operand> func_params;
    FuncFParams *funcfparams = dynamic_cast<FuncFParams *>(root->children[3]);
    if (funcfparams != nullptr)
    {
        func_params = analyzeFuncFParams(funcfparams);
    }

    ir::Function *func = new ir::Function(func_flag, func_params, func_type);

    if (func_flag == "main")
    { // if is "main", add a main() call inst in global function
        ir::CallInst *callGlobal = new ir::CallInst(ir::Operand("global", ir::Type::null), ir::Operand("t" + std::to_string(tmp_cnt++), ir::Type::null));
        func->addInst(callGlobal);
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
        Instruction *inst = new ir::Instruction(ir::Operand("0", ir::Type::IntLiteral), ir::Operand(), ir::Operand(), ir::Operator::_return);
        func->addInst(inst);
    }

    if (func_type == ir::Type::null)
    { // void function
        // ir: return  <==> return null
        Instruction *inst = new ir::Instruction(ir::Operand(), ir::Operand(), ir::Operand(), ir::Operator::_return);
        func->addInst(inst);
    }

    ir_program.addFunction(*func); // add function to ir_program
}

// FuncType -> 'void' | 'int' | 'float'
ir::Type frontend::Analyzer::analyzeFuncType(FuncType *root)
{
    Term *term = dynamic_cast<Term *>(root->children[0]);
    if (term->token.type == TokenType::VOIDTK)
    { // 'void'
        return ir::Type::null;
    }
    else if (term->token.type == TokenType::INTTK)
    { // 'int'
        return ir::Type::Int;
    }
    else if (term->token.type == TokenType::FLOATTK)
    { // 'float'
        return ir::Type::Float;
    }

    return ir::Type::null; // should not reach here
}

// FuncFParam -> BType Ident ['[' ']' { '[' Exp ']' }]
ir::Operand frontend::Analyzer::analyzeFuncFParam(FuncFParam *root)
{ // INFO: for arr param, most: arr[][$some_number]
    ir::Type param_type = analyzeBType(dynamic_cast<BType *>(root->children[0]));
    string param_flag = analyzeIdent(dynamic_cast<Term *>(root->children[1]));

    vector<int> dimension(1, -1); // {-1}, if arr >= 2dim, {-1} -> {-1, $some_number, ...}

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
            Exp *exp = dynamic_cast<Exp *>(root->children[6]);
            analyzeExp(exp);
            int exp_res = std::stoi(exp->v); // get dimension
            dimension.push_back(exp_res);
        }
    }

    // add new param to symbol table
    ir::Operand param(param_flag, param_type);
    symbol_table.scope_stack.back().table[param_flag] = {param, dimension};
    // rename the param
    ir::Operand func_param(symbol_table.get_scoped_name(param.name), param.type);
    return func_param;
}

// FuncFParams -> FuncFParam { ',' FuncFParam }
vector<ir::Operand> frontend::Analyzer::analyzeFuncFParams(FuncFParams *root)
{
    vector<ir::Operand> func_params;
    for (int i = 0; i < root->children.size(); i += 2)
    { // FuncFParam { ',' FuncFParam } <==> { FuncFParam ','|None }
        FuncFParam *funcfparam = dynamic_cast<FuncFParam *>(root->children[i]);
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
        BlockItem *blockitem = dynamic_cast<BlockItem *>(root->children[i]);
        vector<ir::Instruction *> blockitem_body = analyzeBlockItem(blockitem);
        block_body.insert(block_body.end(), blockitem_body.begin(), blockitem_body.end());
    }

    if (!is_func_block)
        symbol_table.exit_scope(); // exit scope for 'if', 'while', 'for' or 'else' block
    return block_body;
}

// BlockItem -> Decl | Stmt
vector<ir::Instruction *> frontend::Analyzer::analyzeBlockItem(BlockItem *root)
{
    if (dynamic_cast<Decl *>(root->children[0]) != nullptr)
    { // Decl
        return analyzeDecl(dynamic_cast<Decl *>(root->children[0]));
    }
    else if (dynamic_cast<Stmt *>(root->children[0]) != nullptr)
    { // Stmt
        return analyzeStmt(dynamic_cast<Stmt *>(root->children[0]));
    }

    return vector<ir::Instruction *>();
}

// Stmt -> LVal '=' Exp ';' | Block | 'if' '(' Cond ')' Stmt [ 'else' Stmt ] | 'while' '(' Cond ')' Stmt | 'break' ';' | 'continue' ';' | 'return' [Exp] ';' | [Exp] ';'
vector<ir::Instruction *> frontend::Analyzer::analyzeStmt(Stmt *root)
{
    vector<ir::Instruction *> res;

    if (LVal *lval = dynamic_cast<LVal *>(root->children[0]))
    { // LVal '=' Exp ';'
        Exp *exp = dynamic_cast<Exp *>(root->children[2]);
        vector<ir::Instruction *> insts = analyzeExp(exp);
        res.insert(res.end(), insts.begin(), insts.end());

        string var_flag = analyzeIdent(dynamic_cast<Term *>(lval->children[0])); // LVal -> Ident { '[' Exp ']' }
        STE ident_ste = symbol_table.get_ste(var_flag);

        if (lval->children.size() == 1)
        { // LVal -> Ident
            if (ident_ste.operand.type == Type::Int)
            { // Float/Int/FloatLiteral/IntLiteral -> Int
                if (exp->t == Type::Int)
                { // ir: mov var_flag##scope_id, "exp->v"
                    ir::Instruction *inst = new ir::Instruction(ir::Operand(exp->v, ir::Type::Int), ir::Operand(), ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::Int), ir::Operator::mov);
                    res.push_back(inst);
                }
                else if (exp->t == Type::IntLiteral)
                { // ir: mov var_flag##scope_id, "exp->v"
                    ir::Instruction *inst = new ir::Instruction(ir::Operand(exp->v, ir::Type::IntLiteral), ir::Operand(), ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::Int), ir::Operator::mov);
                    res.push_back(inst);
                }
                else if (exp->t == Type::Float)
                { // ir: cvt_f2i var_flag##scope_id, "exp->v"
                    ir::Instruction *inst = new ir::Instruction(ir::Operand(exp->v, ir::Type::Float), ir::Operand(), ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::Int), ir::Operator::cvt_f2i);
                    res.push_back(inst);
                }
                else if (exp->t == Type::FloatLiteral)
                { // ir: mov var_flag##scope_id, "exp->v"
                    int val = std::stof(exp->v);
                    ir::Instruction *inst = new ir::Instruction(ir::Operand(std::to_string(val), ir::Type::IntLiteral), ir::Operand(), ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::Int), ir::Operator::mov);
                    res.push_back(inst);
                }
            }
            // Float/Int/FloatLiteral/IntLiteral -> Float
            else if (ident_ste.operand.type == Type::Float)
            {
                if (exp->t == Type::Int)
                { // ir: cvt_i2f var_flag##scope_id, exp->v
                    ir::Instruction *inst = new ir::Instruction(ir::Operand(exp->v, ir::Type::Int), ir::Operand(), ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::Float), ir::Operator::cvt_i2f);
                    res.push_back(inst);
                }
                else if (exp->t == Type::IntLiteral)
                { // ir: mov var_flag##scope_id, "exp->v"
                    float val = std::stoi(exp->v);
                    ir::Instruction *inst = new ir::Instruction(ir::Operand(std::to_string(val), ir::Type::FloatLiteral), ir::Operand(), ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::Float), ir::Operator::fmov);
                    res.push_back(inst);
                }
                else if (exp->t == Type::Float)
                { // ir: mov var_flag##scope_id, exp->v
                    ir::Instruction *inst = new ir::Instruction(ir::Operand(exp->v, ir::Type::Float), ir::Operand(), ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::Float), ir::Operator::fmov);
                    res.push_back(inst);
                }
                else if (exp->t == Type::FloatLiteral)
                { // ir: mov var_flag##scope_id, "exp->v"
                    ir::Instruction *inst = new ir::Instruction(ir::Operand(exp->v, ir::Type::FloatLiteral), ir::Operand(), ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::Float), ir::Operator::fmov);
                    res.push_back(inst);
                }
            }
        }

        else if (lval->children.size() == 4)
        { // 1d arr: LVal -> Ident '[' Exp ']' <==> { Ident '[' Exp ']' '=' Exp ';' }, here we need temp var to store the offset
            Exp *offset = dynamic_cast<Exp *>(lval->children[2]);
            vector<ir::Instruction *> insts = analyzeExp(offset);
            res.insert(res.end(), insts.begin(), insts.end());

            if (ident_ste.operand.type == Type::IntPtr)
            {
                if (exp->t == Type::Int || exp->t == Type::IntLiteral)
                { // ir: store var_flag##scope_id, offset, "exp->v" or exp->v
                    ir::Instruction *storeInst = new ir::Instruction(ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::IntPtr), ir::Operand(offset->v, offset->t), ir::Operand(exp->v, exp->t), ir::Operator::store);
                    res.push_back(storeInst);
                }
                else if (exp->t == Type::Float)
                {
                    // ir: cvt_f2i __temp_var, exp->v
                    // ir: store var_flag##scope_id, offset, __temp_var
                    string tmp_f2i_flag = "__temp_var_" + std::to_string(tmp_cnt++);
                    ir::Instruction *f2iInst = new ir::Instruction(ir::Operand(exp->v, ir::Type::Float), ir::Operand(), ir::Operand(tmp_f2i_flag, ir::Type::Int), ir::Operator::cvt_f2i);
                    ir::Instruction *storeInst = new ir::Instruction(ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::IntPtr), ir::Operand(offset->v, offset->t), ir::Operand(tmp_f2i_flag, Type::Int), ir::Operator::store);
                    res.push_back(f2iInst);
                    res.push_back(storeInst);
                }
                else if (exp->t == Type::FloatLiteral)
                { // ir: store var_flag##scope_id, offset, "exp->v"
                    int val = std::stof(exp->v);
                    ir::Instruction *inst = new ir::Instruction(ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::IntPtr), ir::Operand(offset->v, offset->t), ir::Operand(std::to_string(val), Type::IntLiteral), ir::Operator::store);
                    res.push_back(inst);
                }
            }
            else if (ident_ste.operand.type == Type::FloatPtr)
            {
                if (exp->t == Type::Int)
                {
                    // ir: cvt_i2f __temp_var, exp->v
                    // ir: store var_flag##scope_id, offset, __temp_var
                    string tmp_i2f_flag = "__temp_var_" + std::to_string(tmp_cnt++);
                    ir::Instruction *i2fInst = new ir::Instruction(ir::Operand(exp->v, ir::Type::Int), ir::Operand(), ir::Operand(tmp_i2f_flag, ir::Type::Float), ir::Operator::cvt_i2f);
                    ir::Instruction *storeInst = new ir::Instruction(ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::FloatPtr), ir::Operand(offset->v, offset->t), ir::Operand(tmp_i2f_flag, Type::Float), ir::Operator::store);
                    res.push_back(i2fInst);
                    res.push_back(storeInst);
                }
                else if (exp->t == Type::IntLiteral)
                { // ir: mov var_flag##scope_id, "exp->v"
                    float val = std::stoi(exp->v);
                    ir::Instruction *inst = new ir::Instruction(ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::FloatPtr), ir::Operand(offset->v, offset->t), ir::Operand(std::to_string(val), Type::FloatLiteral), ir::Operator::store);
                    res.push_back(inst);
                }
                else if (exp->t == Type::Float || exp->t == Type::FloatLiteral)
                { // ir: store var_flag##scope_id, offset, exp->v or "exp->v"
                    ir::Instruction *inst = new ir::Instruction(ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::FloatPtr), ir::Operand(offset->v, offset->t), ir::Operand(exp->v, exp->t), ir::Operator::store);
                    res.push_back(inst);
                }
            }
        }

        else if (lval->children.size() == 7)
        { // 2d arr: LVal -> Ident '[' Exp ']' '[' Exp ']' <==> { Ident '[' Exp ']' '[' Exp ']' '=' Exp ';' }
            Exp *dim1_exp = dynamic_cast<Exp *>(lval->children[2]);
            vector<ir::Instruction *> cal1_insts = analyzeExp(dim1_exp);
            res.insert(res.end(), cal1_insts.begin(), cal1_insts.end());
            Exp *dim2_exp = dynamic_cast<Exp *>(lval->children[5]);
            vector<ir::Instruction *> cal2_insts = analyzeExp(dim2_exp);
            res.insert(res.end(), cal2_insts.begin(), cal2_insts.end());

            string tmp_dim1_flag = "__temp_var" + std::to_string(tmp_cnt++);
            string tmp_dim2_flag = "__temp_var" + std::to_string(tmp_cnt++);
            Instruction *def1Inst = new ir::Instruction(ir::Operand(dim1_exp->v, dim1_exp->t), ir::Operand(), ir::Operand(tmp_dim1_flag, Type::Int), ir::Operator::def);
            Instruction *def2Inst = new ir::Instruction(ir::Operand(dim2_exp->v, dim2_exp->t), ir::Operand(), ir::Operand(tmp_dim2_flag, Type::Int), ir::Operator::def);
            string tmp_col_len_flag = "__temp_var_" + std::to_string(tmp_cnt++);
            Instruction *def3Inst = new ir::Instruction(ir::Operand(std::to_string(ident_ste.dimension[1]), Type::IntLiteral), ir::Operand(), ir::Operand(tmp_col_len_flag, Type::Int), ir::Operator::def);
            string tmp_lineoffset_flag = "__temp_var_" + std::to_string(tmp_cnt++);

            Instruction *mulOffsetInst = new ir::Instruction(ir::Operand(tmp_dim1_flag, Type::Int), ir::Operand(tmp_col_len_flag, Type::Int), ir::Operand(tmp_lineoffset_flag, Type::Int), ir::Operator::mul);
            string tmp_totaloffset_flag = "__temp_var_" + std::to_string(tmp_cnt++);
            Instruction *addOffsetInst = new ir::Instruction(ir::Operand(tmp_lineoffset_flag, Type::Int), ir::Operand(tmp_dim2_flag, Type::Int), ir::Operand(tmp_totaloffset_flag, Type::Int), ir::Operator::add);

            // irs here: total offset = dim1 * col_len + dim2
            // def tmp_dim1_flag, dim1_exp->v
            // def tmp_dim2_flag, dim2_exp->v
            // def tmp_col_len_flag, ident_ste.dimension[1]
            // def tmp_lineoffset_flag, tmp_dim1_flag * tmp_col_len_flag
            // def tmp_totaloffset_flag, tmp_lineoffset_flag + tmp_dim2_flag
            // store var_flag##scope_id, tmp_totaloffset_flag, exp->v
            // store var_flag##scope_id, tmp_totaloffset_flag, exp->v
            // store var_flag##scope_id, tmp_totaloffset_flag, "exp->v"

            res.push_back(def1Inst);
            res.push_back(def2Inst);
            res.push_back(def3Inst);
            res.push_back(mulOffsetInst);
            res.push_back(addOffsetInst);

            if (ident_ste.operand.type == Type::IntPtr)
            { // IntPtr
                if (exp->t == Type::Int || exp->t == Type::IntLiteral)
                {
                    ir::Instruction *inst = new ir::Instruction(ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::IntPtr), ir::Operand(tmp_totaloffset_flag, Type::Int), ir::Operand(exp->v, exp->t), ir::Operator::store);
                    res.push_back(inst);
                }
                else if (exp->t == Type::Float)
                {
                    string tmp_f2i_flag = "__temp_var_" + std::to_string(tmp_cnt++);
                    ir::Instruction *f2iInst = new ir::Instruction(ir::Operand(exp->v, ir::Type::Float), ir::Operand(), ir::Operand(tmp_f2i_flag, ir::Type::Int), ir::Operator::cvt_f2i);
                    ir::Instruction *storeInst = new ir::Instruction(ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::IntPtr), ir::Operand(tmp_totaloffset_flag, Type::Int), ir::Operand(tmp_f2i_flag, Type::Int), ir::Operator::store);
                    res.push_back(f2iInst);
                    res.push_back(storeInst);
                }
                else if (exp->t == Type::FloatLiteral)
                {
                    int val = std::stof(exp->v);
                    ir::Instruction *inst = new ir::Instruction(ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::IntPtr), ir::Operand(tmp_totaloffset_flag, Type::Int), ir::Operand(std::to_string(val), Type::IntLiteral), ir::Operator::store);
                    res.push_back(inst);
                }
            }
            else if (ident_ste.operand.type == Type::FloatPtr)
            {
                if (exp->t == Type::Int)
                {
                    string tmp_i2f_flag = "__temp_var_" + std::to_string(tmp_cnt++);
                    ir::Instruction *i2fInst = new ir::Instruction(ir::Operand(exp->v, ir::Type::Int), ir::Operand(), ir::Operand(tmp_i2f_flag, ir::Type::Float), ir::Operator::cvt_i2f);
                    ir::Instruction *storeInst = new ir::Instruction(ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::FloatPtr), ir::Operand(tmp_totaloffset_flag, Type::Int), ir::Operand(tmp_i2f_flag, Type::Float), ir::Operator::store);
                    res.push_back(i2fInst);
                    res.push_back(storeInst);
                }
                else if (exp->t == Type::IntLiteral)
                {
                    float val = std::stoi(exp->v);
                    ir::Instruction *inst = new ir::Instruction(ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::FloatPtr), ir::Operand(tmp_totaloffset_flag, Type::Int), ir::Operand(std::to_string(val), Type::FloatLiteral), ir::Operator::store);
                    res.push_back(inst);
                }
                else if (exp->t == Type::Float || exp->t == Type::FloatLiteral)
                {
                    ir::Instruction *inst = new ir::Instruction(ir::Operand(symbol_table.get_scoped_name(var_flag), ir::Type::FloatPtr), ir::Operand(tmp_totaloffset_flag, Type::Int), ir::Operand(exp->v, exp->t), ir::Operator::store);
                    res.push_back(inst);
                }
            }
        }
        return res;
    }

    if (Block *block = dynamic_cast<Block *>(root->children[0]))
    { // Block
        // if/while/for/else block
        return analyzeBlock(block, false);
    }

    Term *term = dynamic_cast<Term *>(root->children[0]);

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
        Exp *exp = dynamic_cast<Exp *>(root->children[0]);
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
            Exp *exp = dynamic_cast<Exp *>(root->children[1]);
            vector<ir::Instruction *> exp_insts = analyzeExp(exp);
            res.insert(res.end(), exp_insts.begin(), exp_insts.end());

            if (curr_function->returnType == Type::Int)
            {
                if (exp->t == Type::Int || exp->t == Type::IntLiteral)
                { // Int or IntLiteral
                    // ir: return "exp->v"
                    ir::Instruction *inst = new ir::Instruction(ir::Operand(exp->v, exp->t), ir::Operand(), ir::Operand(), ir::Operator::_return);
                    res.push_back(inst);
                }
                else if (exp->t == Type::FloatLiteral)
                { // Float or FloatLiteral
                    int val = std::stof(exp->v);
                    // ir: return "val"
                    ir::Instruction *inst = new ir::Instruction(ir::Operand(std::to_string(val), Type::IntLiteral), ir::Operand(), ir::Operand(), ir::Operator::_return);
                    res.push_back(inst);
                }
                else if (exp->t == Type::Float)
                {
                    string tmp_f2i_flag = "__temp_var_" + std::to_string(tmp_cnt++);
                    // ir: cvt_f2i __temp_var, exp->v
                    // ir: return __temp_var
                    ir::Instruction *cvtInst = new ir::Instruction(ir::Operand(exp->v, Type::Float), ir::Operand(), ir::Operand(tmp_f2i_flag, Type::Int), ir::Operator::cvt_f2i);
                    ir::Instruction *retInst = new ir::Instruction(ir::Operand(tmp_f2i_flag, Type::Int), ir::Operand(), ir::Operand(), ir::Operator::_return);
                    res.push_back(cvtInst);
                    res.push_back(retInst);
                }
            }
            else if (curr_function->returnType == Type::Float)
            {
                if (exp->t == Type::Float || exp->t == Type::FloatLiteral)
                { // Float or FloatLiteral
                    // ir: return "exp->v"
                    ir::Instruction *inst = new ir::Instruction(ir::Operand(exp->v, exp->t), ir::Operand(), ir::Operand(), ir::Operator::_return);
                    res.push_back(inst);
                }
                else if (exp->t == Type::IntLiteral)
                { // Int or IntLiteral
                    float val = std::stoi(exp->v);
                    // ir: return "val"
                    ir::Instruction *inst = new ir::Instruction(ir::Operand(std::to_string(val), Type::FloatLiteral), ir::Operand(), ir::Operand(), ir::Operator::_return);
                    res.push_back(inst);
                }
                else if (exp->t == Type::Int)
                {
                    string tmp_i2f_flag = "__temp_var_" + std::to_string(tmp_cnt++);
                    // ir: cvt_i2f __temp_var, exp->v
                    // ir: return __temp_var
                    ir::Instruction *cvtInst = new ir::Instruction(ir::Operand(exp->v, Type::Int), ir::Operand(), ir::Operand(tmp_i2f_flag, Type::Float), ir::Operator::cvt_i2f);
                    ir::Instruction *retInst = new ir::Instruction(ir::Operand(tmp_i2f_flag, Type::Float), ir::Operand(), ir::Operand(), ir::Operator::_return);
                    res.push_back(cvtInst);
                    res.push_back(retInst);
                }
            }
        }
        else
        { // 'return' ';'
            ir::Instruction *inst = new ir::Instruction(ir::Operand(), ir::Operand(), ir::Operand(), ir::Operator::_return);
            res.push_back(inst);
        }
        return res;
    }

    if (term->token.type == TokenType::IFTK)
    { // 'if' '(' Cond ')' Stmt 'else' Stmt
        Cond *cond = dynamic_cast<Cond *>(root->children[2]);
        vector<ir::Instruction *> insts = analyzeCond(cond);
        if (cond->t == Type::Float || cond->t == Type::FloatLiteral)
        {
            if (cond->t == Type::FloatLiteral)
            {
                float val = std::stof(cond->v);
                cond->v = std::to_string(val != 0);
                cond->t = Type::IntLiteral;
            }
            else
            {
                // ir: cvt_f2i __temp_var, cond->v
                // ir: __temp_var = 0.0
                string tmp1_flag = "__temp_var_" + std::to_string(tmp_cnt++);
                ir::Instruction *cvt1Inst = new ir::Instruction(ir::Operand(cond->v, Type::Float), ir::Operand("0.0", Type::FloatLiteral), ir::Operand(tmp1_flag, Type::Float), ir::Operator::fneq);
                string tmp2_flag = "__temp_var_" + std::to_string(tmp_cnt++);
                ir::Instruction *cvt2Inst = new ir::Instruction(ir::Operand(tmp1_flag, Type::Float), ir::Operand(), ir::Operand(tmp2_flag, Type::Int), ir::Operator::cvt_f2i);
                insts.push_back(cvt1Inst);
                insts.push_back(cvt2Inst);
                cond->v = tmp2_flag;
                cond->t = Type::Int;
            }
        }
        res.insert(res.end(), insts.begin(), insts.end());

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
        ir::Instruction *goto_if_Inst = new ir::Instruction(ir::Operand(cond->v, cond->t), ir::Operand(), ir::Operand("2", Type::IntLiteral), ir::Operator::_goto);
        res.push_back(goto_if_Inst);

        symbol_table.add_scope(); // fuck into scope for if
        vector<ir::Instruction *> stmt_after_if_insts = analyzeStmt(dynamic_cast<Stmt *>(root->children[4]));
        symbol_table.exit_scope();

        if (root->children.size() == 7)
        {                             // [ 'else' Stmt ]
            symbol_table.add_scope(); // fuck into scope for else
            vector<ir::Instruction *> stmt_after_else_insts = analyzeStmt(dynamic_cast<Stmt *>(root->children[6]));
            symbol_table.exit_scope();

            // GOTO End after 'if'
            // ir: goto [pc, len(stmt_after_else_insts) + 1]
            ir::Instruction *goto_if_last_Inst = new ir::Instruction(ir::Operand(), ir::Operand(), ir::Operand(std::to_string(stmt_after_else_insts.size() + 1), Type::IntLiteral), ir::Operator::_goto);
            stmt_after_if_insts.push_back(goto_if_last_Inst);

            // GOTO Begin for 'else'
            // ir: goto [pc, len(stmt_after_if_insts) + 1]
            ir::Instruction *goto_else_Inst = new ir::Instruction(ir::Operand(), ir::Operand(), ir::Operand(std::to_string(stmt_after_if_insts.size() + 1), Type::IntLiteral), ir::Operator::_goto);
            res.push_back(goto_else_Inst);

            // insert if stmt
            res.insert(res.end(), stmt_after_if_insts.begin(), stmt_after_if_insts.end());
            // insert else stmt
            res.insert(res.end(), stmt_after_else_insts.begin(), stmt_after_else_insts.end());
            // unuse stmt, waiting for end of 'if' & 'else'
            Instruction *unuse_Inst = new ir::Instruction(ir::Operand(), ir::Operand(), ir::Operand(), ir::Operator::__unuse__);
            res.push_back(unuse_Inst);
        }
        else
        { // 'if' '(' Cond ')' Stmt
            // goto [pc, len(stmt_after_if_insts) + 1]
            ir::Instruction *goto_else_Inst = new ir::Instruction(ir::Operand(), ir::Operand(), ir::Operand(std::to_string(stmt_after_if_insts.size() + 1), Type::IntLiteral), ir::Operator::_goto);
            res.push_back(goto_else_Inst);
            res.insert(res.end(), stmt_after_if_insts.begin(), stmt_after_if_insts.end());
            Instruction *unuse_Inst = new ir::Instruction(ir::Operand(), ir::Operand(), ir::Operand(), ir::Operator::__unuse__);
            res.push_back(unuse_Inst);
        }
        return res;
    }

    if (term->token.type == TokenType::WHILETK)
    { // 'while' '(' Cond ')' Stmt
        Cond *cond = dynamic_cast<Cond *>(root->children[2]);
        vector<Instruction *> insts = analyzeCond(cond);

        if (cond->t == Type::Float || cond->t == Type::FloatLiteral)
        { // convert to Int
            if (cond->t == Type::FloatLiteral)
            {
                float val = std::stof(cond->v);
                cond->v = std::to_string(val != 0);
                cond->t = Type::IntLiteral;
            }
            else
            {
                string tmp1_flag = "__temp_var_" + std::to_string(tmp_cnt++);
                ir::Instruction *cvt1Inst = new ir::Instruction(ir::Operand(cond->v, Type::Float), ir::Operand("0.0", Type::FloatLiteral), ir::Operand(tmp1_flag, Type::Float), ir::Operator::fneq);
                string tmp2_flag = "__temp_var_" + std::to_string(tmp_cnt++);
                ir::Instruction *cvt2Inst = new ir::Instruction(ir::Operand(tmp1_flag, Type::Float), ir::Operand(), ir::Operand(tmp2_flag, Type::Int), ir::Operator::cvt_f2i);
                insts.push_back(cvt1Inst);
                insts.push_back(cvt2Inst);
                cond->v = tmp2_flag;
                cond->t = Type::Int;
            }
        }

        symbol_table.add_scope(); // fuck into scope for while
        vector<Instruction *> stmt_insts = analyzeStmt(dynamic_cast<Stmt *>(root->children[4]));
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
        Instruction *goto_while_Inst = new ir::Instruction(ir::Operand(cond->v, cond->t), ir::Operand(), ir::Operand("2", Type::IntLiteral), ir::Operator::_goto);

        Instruction *goto_return_begin_mark = new ir::Instruction(ir::Operand("continue", Type::null), ir::Operand(), ir::Operand(), ir::Operator::__unuse__);
        stmt_insts.push_back(goto_return_begin_mark);

        Instruction *goto_exit_while_Inst = new ir::Instruction(ir::Operand(), ir::Operand(), ir::Operand(std::to_string(stmt_insts.size() + 1), Type::IntLiteral), ir::Operator::_goto);
        for (int i = 0; i < stmt_insts.size(); i++)
        { // replace break/continue as goto
            if (stmt_insts[i]->op == Operator::__unuse__ && stmt_insts[i]->op1.type == Type::null)
            {
                if (stmt_insts[i]->op1.name == "break")
                {
                    Instruction *replace_break_inst = new ir::Instruction(ir::Operand(), ir::Operand(), ir::Operand(std::to_string(int(stmt_insts.size()) - i), Type::IntLiteral), ir::Operator::_goto);
                    stmt_insts[i] = replace_break_inst;
                }
                else if (stmt_insts[i]->op1.name == "continue")
                {
                    Instruction *replace_continue_inst = new ir::Instruction(ir::Operand(), ir::Operand(), ir::Operand(std::to_string(-(2 + i + int(insts.size()))), Type::IntLiteral), ir::Operator::_goto);
                    stmt_insts[i] = replace_continue_inst;
                }
            }
        }

        res.insert(res.end(), insts.begin(), insts.end());
        res.push_back(goto_while_Inst);
        res.push_back(goto_exit_while_Inst);
        res.insert(res.end(), stmt_insts.begin(), stmt_insts.end());

        Instruction *unuse_Inst = new ir::Instruction(ir::Operand(), ir::Operand(), ir::Operand(), ir::Operator::__unuse__);
        res.push_back(unuse_Inst);
        return res;
    }

    if (term->token.type == TokenType::BREAKTK)
    { // 'break' ';'
        Instruction *inst = new Instruction(Operand("break", Type::null), Operand(), Operand(), Operator::__unuse__);
        res.push_back(inst);
        return res;
    }

    if (term->token.type == TokenType::CONTINUETK)
    { // 'continue' ';'
        Instruction *inst = new Instruction(Operand("continue", Type::null), Operand(), Operand(), Operator::__unuse__);
        res.push_back(inst);
        return res;
    }

    return res;
}

/**
 * Exps
 */

// Exp -> AddExp
vector<ir::Instruction *> frontend::Analyzer::analyzeExp(Exp *root)
{
    AddExp *addexp = dynamic_cast<AddExp *>(root->children[0]);
    vector<ir::Instruction *> insts = analyzeAddExp(addexp);
    /**
     * Exp.v = AddExp.v
     * Exp.t = AddExp.t
     */
    root->v = addexp->v;
    root->t = addexp->t;
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
        MulExp *mulexp = dynamic_cast<MulExp *>(root->children[i]);
        vector<Instruction *> insts = analyzeMulExp(mulexp);
        res.insert(res.end(), insts.begin(), insts.end());

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

    MulExp *mulexp0 = dynamic_cast<MulExp *>(root->children[0]); // first MulExp res, val: type
    root->t = mulexp0->t;
    root->v = mulexp0->v;

    if (root->children.size() == 1)
        return res; // complete analyzeAddExp

    if (target_type != root->t)
    { // type conversion for root
        if (target_type == Type::Int)
        { // IntLiteral -> Int
            string tmp_intcvt_flag = "__temp_var_" + std::to_string(tmp_cnt++);
            Instruction *inst = new Instruction(ir::Operand(root->v, ir::Type::IntLiteral), ir::Operand(), ir::Operand(tmp_intcvt_flag, ir::Type::Int), ir::Operator::def);
            res.push_back(inst);
            root->v = tmp_intcvt_flag;
            root->t = Type::Int;
        }
        else if (target_type == Type::FloatLiteral)
        { // IntLiteral -> FloatLiteral
            float val = std::stoi(root->v);
            root->v = std::to_string(val);
            root->t = Type::FloatLiteral;
        }
        else if (target_type == Type::Float)
        { // IntLiteral -> Float, Int -> Float, FloatLiteral -> Float
            if (root->t == Type::IntLiteral)
            {
                float val = std::stof(root->v);
                string tmp_i2f_flag = "__temp_var_" + std::to_string(tmp_cnt++);
                Instruction *inst = new Instruction(ir::Operand(std::to_string(val), ir::Type::FloatLiteral), ir::Operand(), ir::Operand(tmp_i2f_flag, ir::Type::Float), ir::Operator::fdef);
                res.push_back(inst);
                root->v = tmp_i2f_flag;
                root->t = Type::Float;
            }
            else if (root->t == Type::Int)
            {
                string tmp_i2f_flag = "__temp_var_" + std::to_string(tmp_cnt++);
                Instruction *inst = new Instruction(ir::Operand(root->v, ir::Type::Int), ir::Operand(), ir::Operand(tmp_i2f_flag, ir::Type::Float), ir::Operator::cvt_i2f);
                res.push_back(inst);
                root->v = tmp_i2f_flag;
                root->t = Type::Float;
            }
            else if (root->t == Type::FloatLiteral)
            {
                string tmp_fl2f_flag = "__temp_var_" + std::to_string(tmp_cnt++);
                Instruction *inst = new Instruction(ir::Operand(root->v, ir::Type::FloatLiteral), ir::Operand(), ir::Operand(tmp_fl2f_flag, ir::Type::Float), ir::Operator::fdef);
                res.push_back(inst);
                root->v = tmp_fl2f_flag;
                root->t = Type::Float;
            }
        }
    }

    for (int i = 2; i < root->children.size(); i += 2)
    { // {'+' | '-'} MulExp, from left to right

        MulExp *mulexp = dynamic_cast<MulExp *>(root->children[i]);
        Term *op_term = dynamic_cast<Term *>(root->children[i - 1]); // '+' | '-'

        if (target_type != mulexp->t)
        { // type conversion for mulexp
            if (target_type == Type::Int)
            {
                string tmp_intcvt_flag = "__temp_var_" + std::to_string(tmp_cnt++);
                Instruction *inst = new Instruction(ir::Operand(mulexp->v, ir::Type::IntLiteral), ir::Operand(), ir::Operand(tmp_intcvt_flag, ir::Type::Int), ir::Operator::def);
                res.push_back(inst);
                mulexp->v = tmp_intcvt_flag;
                mulexp->t = Type::Int;
            }
            else if (target_type == Type::FloatLiteral)
            {
                float val = std::stoi(mulexp->v);
                mulexp->v = std::to_string(val);
                mulexp->t = Type::FloatLiteral;
            }
            else if (target_type == Type::Float)
            {
                if (mulexp->t == Type::IntLiteral)
                {
                    float val = std::stof(mulexp->v);
                    string tmp_i2f_flag = "__temp_var_" + std::to_string(tmp_cnt++);
                    Instruction *cvtInst = new Instruction(ir::Operand(std::to_string(val), ir::Type::FloatLiteral), ir::Operand(), ir::Operand(tmp_i2f_flag, ir::Type::Float), ir::Operator::fdef);
                    res.push_back(cvtInst);
                    mulexp->v = tmp_i2f_flag;
                    mulexp->t = Type::Float;
                }
                else if (mulexp->t == Type::Int)
                {
                    string tmp_i2f_flag = "__temp_var_" + std::to_string(tmp_cnt++);
                    Instruction *cvtInst = new Instruction(ir::Operand(mulexp->v, ir::Type::Int), ir::Operand(), ir::Operand(tmp_i2f_flag, ir::Type::Float), ir::Operator::cvt_i2f);
                    res.push_back(cvtInst);
                    mulexp->v = tmp_i2f_flag;
                    mulexp->t = Type::Float;
                }
                else if (mulexp->t == Type::FloatLiteral)
                {
                    string tmp_fl2f_flag = "__temp_var_" + std::to_string(tmp_cnt++);
                    Instruction *cvtInst = new Instruction(ir::Operand(mulexp->v, ir::Type::FloatLiteral), ir::Operand(), ir::Operand(tmp_fl2f_flag, ir::Type::Float), ir::Operator::fdef);
                    res.push_back(cvtInst);
                    mulexp->v = tmp_fl2f_flag;
                    mulexp->t = Type::Float;
                }
            }
        }

        if (target_type == Type::IntLiteral)
        { // compute it if target_type is IntLiteral
            int val1 = std::stoi(root->v);
            int val2 = std::stoi(mulexp->v);
            if (op_term->token.type == TokenType::PLUS)
                root->v = std::to_string(val1 + val2);
            else if (op_term->token.type == TokenType::MINU)
                root->v = std::to_string(val1 - val2);
        }
        else if (target_type == Type::FloatLiteral)
        { // compute it if target_type is FloatLiteral
            float val1 = std::stof(root->v);
            float val2 = std::stof(mulexp->v);
            if (op_term->token.type == TokenType::PLUS)
                root->v = std::to_string(val1 + val2);
            else if (op_term->token.type == TokenType::MINU)
                root->v = std::to_string(val1 - val2);
        }
        else if (target_type == Type::Int)
        {
            // add new temp var, to replace the res
            // ir: add/sub root->v, mulexp->v, tmp_cal_flag
            string tmp_cal_flag = "__temp_var_" + std::to_string(tmp_cnt++);
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
            string tmp_cal_flag = "__temp_var_" + std::to_string(tmp_cnt++);
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
    AddExp *addexp = dynamic_cast<AddExp *>(root->children[0]);
    vector<ir::Instruction *> insts = analyzeAddExp(addexp);
    root->v = addexp->v;
    root->t = addexp->t;
    return insts;
}

// MulExp -> UnaryExp { ('*' | '/' | '%') UnaryExp }, very similar to AddExp
vector<ir::Instruction *> frontend::Analyzer::analyzeMulExp(MulExp *root)
{
    vector<Instruction *> res;

    Type target_type = Type::IntLiteral;

    for (int i = 0; i < root->children.size(); i += 2)
    {
        UnaryExp *unaryexp = dynamic_cast<UnaryExp *>(root->children[i]);
        vector<Instruction *> insts = analyzeUnaryExp(unaryexp);
        res.insert(res.end(), insts.begin(), insts.end());

        if (unaryexp->t == ir::Type::Float)
            target_type = ir::Type::Float;
        else if (unaryexp->t == ir::Type::Int && target_type == ir::Type::IntLiteral)
            target_type = ir::Type::Int;
        else if (unaryexp->t == ir::Type::FloatLiteral && target_type == ir::Type::IntLiteral)
            target_type = ir::Type::FloatLiteral;
        else if ((unaryexp->t == ir::Type::FloatLiteral && target_type == ir::Type::Int) || (target_type == ir::Type::FloatLiteral && unaryexp->t == ir::Type::Int)) // 提升没有顺序
            target_type = ir::Type::Float;
    }

    UnaryExp *unaryexp0 = dynamic_cast<UnaryExp *>(root->children[0]);
    root->t = unaryexp0->t;
    root->v = unaryexp0->v;

    if (root->children.size() == 1)
        return res;

    if (target_type != root->t)
    {
        if (target_type == Type::Int)
        {
            string tmp_intcvt_flag = "__temp_var_" + std::to_string(tmp_cnt++);
            Instruction *inst = new Instruction(ir::Operand(root->v, ir::Type::IntLiteral), ir::Operand(), ir::Operand(tmp_intcvt_flag, ir::Type::Int), ir::Operator::def);
            res.push_back(inst);
            root->v = tmp_intcvt_flag;
            root->t = Type::Int;
        }
        else if (target_type == Type::FloatLiteral)
        {
            float val = std::stoi(root->v);
            root->v = std::to_string(val);
            root->t = Type::FloatLiteral;
        }
        else if (target_type == Type::Float)
        {
            if (root->t == Type::IntLiteral)
            {
                float val = std::stof(root->v);
                string tmp_i2f_flag = "__temp_var_" + std::to_string(tmp_cnt++);
                Instruction *inst = new Instruction(ir::Operand(std::to_string(val), ir::Type::FloatLiteral), ir::Operand(), ir::Operand(tmp_i2f_flag, ir::Type::Float), ir::Operator::fdef);
                res.push_back(inst);
                root->v = tmp_i2f_flag;
                root->t = Type::Float;
            }
            else if (root->t == Type::Int)
            {
                string tmp_i2f_flag = "__temp_var_" + std::to_string(tmp_cnt++);
                Instruction *inst = new Instruction(ir::Operand(root->v, ir::Type::Int), ir::Operand(), ir::Operand(tmp_i2f_flag, ir::Type::Float), ir::Operator::cvt_i2f);
                res.push_back(inst);
                root->v = tmp_i2f_flag;
                root->t = Type::Float;
            }
            else if (root->t == Type::FloatLiteral)
            {
                string tmp_fl2f_flag = "__temp_var_" + std::to_string(tmp_cnt++);
                Instruction *inst = new Instruction(ir::Operand(root->v, ir::Type::FloatLiteral), ir::Operand(), ir::Operand(tmp_fl2f_flag, ir::Type::Float), ir::Operator::fdef);
                res.push_back(inst);
                root->v = tmp_fl2f_flag;
                root->t = Type::Float;
            }
        }
    }

    for (int i = 2; i < root->children.size(); i += 2)
    {
        UnaryExp *unaryexp = dynamic_cast<UnaryExp *>(root->children[i]);

        Term *op_term = dynamic_cast<Term *>(root->children[i - 1]);

        if (target_type != unaryexp->t)
        {
            if (target_type == Type::Int)
            {
                string tmp_intcvt_flag = "__temp_var_" + std::to_string(tmp_cnt++);
                Instruction *inst = new Instruction(ir::Operand(unaryexp->v, ir::Type::IntLiteral), ir::Operand(), ir::Operand(tmp_intcvt_flag, ir::Type::Int), ir::Operator::def);
                res.push_back(inst);
                unaryexp->v = tmp_intcvt_flag;
                unaryexp->t = Type::Int;
            }
            else if (target_type == Type::FloatLiteral)
            {
                float val = std::stoi(unaryexp->v);
                unaryexp->t = Type::FloatLiteral;
                unaryexp->v = std::to_string(val);
            }
            else if (target_type == Type::Float)
            {
                if (unaryexp->t == Type::IntLiteral)
                {
                    float val = std::stof(unaryexp->v);
                    string tmp_i2f_flag = "__temp_var_" + std::to_string(tmp_cnt++);
                    Instruction *inst = new Instruction(ir::Operand(std::to_string(val), ir::Type::FloatLiteral), ir::Operand(), ir::Operand(tmp_i2f_flag, ir::Type::Float), ir::Operator::fdef);
                    res.push_back(inst);
                    unaryexp->t = Type::Float;
                    unaryexp->v = tmp_i2f_flag;
                }
                else if (unaryexp->t == Type::Int)
                {
                    string tmp_i2f_flag = "__temp_var_" + std::to_string(tmp_cnt++);
                    Instruction *inst = new Instruction(ir::Operand(unaryexp->v, ir::Type::Int), ir::Operand(), ir::Operand(tmp_i2f_flag, ir::Type::Float), ir::Operator::cvt_i2f);
                    res.push_back(inst);
                    unaryexp->t = Type::Float;
                    unaryexp->v = tmp_i2f_flag;
                }
                else if (unaryexp->t == Type::FloatLiteral)
                {
                    string tmp_fl2f_flag = "__temp_var_" + std::to_string(tmp_cnt++);
                    Instruction *inst = new Instruction(ir::Operand(unaryexp->v, ir::Type::FloatLiteral), ir::Operand(), ir::Operand(tmp_fl2f_flag, ir::Type::Float), ir::Operator::fdef);
                    res.push_back(inst);
                    unaryexp->v = tmp_fl2f_flag;
                    unaryexp->t = Type::Float;
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
            string tmp_cal_flag = "__temp_var_" + std::to_string(tmp_cnt++);
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
            string tmp_cal_flag = "__temp_var_" + std::to_string(tmp_cnt++);
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

    PrimaryExp *primaryexp = dynamic_cast<PrimaryExp *>(root->children[0]);
    if (primaryexp != nullptr)
    { // PrimaryExp
        vector<Instruction *> insts = analyzePrimaryExp(primaryexp);
        res.insert(res.end(), insts.begin(), insts.end());
        root->t = primaryexp->t;
        root->v = primaryexp->v;
        return res;
    }

    Term *term = dynamic_cast<Term *>(root->children[0]);
    if (term != nullptr && term->token.type == TokenType::IDENFR)
    { // Ident '(' [FuncRParams] ')'
        string func_flag = term->token.value;
        Function *func = symbol_table.functions[func_flag]; // get function
        vector<Operand> paraVec;                            // list of parameters of function while calling
        FuncRParams *funcrparams = dynamic_cast<FuncRParams *>(root->children[2]);
        if (funcrparams != nullptr)
        {
            vector<Operand> func_para_type = func->ParameterList;
            vector<ir::Instruction *> insts = analyzeFuncRParams(funcrparams, func_para_type, paraVec); // get real parameters
            res.insert(res.end(), insts.begin(), insts.end());
        }

        string return_value = "__temp_var_" + std::to_string(tmp_cnt++); // make a new temp var for return value
        // ir: call __temp_var, func_flag(paraVec)
        ir::CallInst *inst = new ir::CallInst(ir::Operand(func->name, func->returnType), paraVec, ir::Operand(return_value, func->returnType));
        res.push_back(inst);

        root->v = return_value; // set return value
        root->t = func->returnType;
        return res;
    }

    UnaryOp *unaryop = dynamic_cast<UnaryOp *>(root->children[0]);
    if (unaryop != nullptr)
    { // UnaryOp UnaryExp
        UnaryExp *unaryexp = dynamic_cast<UnaryExp *>(root->children[1]);
        vector<ir::Instruction *> insts = analyzeUnaryExp(unaryexp); // calculate UnaryExp
        res.insert(res.end(), insts.begin(), insts.end());

        Term *unaryop_term = dynamic_cast<Term *>(unaryop->children[0]);
        if (unaryop_term->token.type == TokenType::PLUS)
        { // Yehn! nothing to do 🤣
            root->t = unaryexp->t;
            root->v = unaryexp->v;
        }
        else if (unaryop_term->token.type == TokenType::MINU)
        {
            if (unaryexp->t == Type::IntLiteral || unaryexp->t == Type::FloatLiteral)
            {
                root->t = unaryexp->t;
                root->v = std::to_string(-std::stof(unaryexp->v));
            }
            else if (unaryexp->t == Type::Int || unaryexp->t == Type::Float)
            {
                /**
                 * My strategy here:
                 * -x = 0 - x (int)
                 * -x = 0.0 - x (float)
                 */

                string tmp_zero = "__temp_var_" + std::to_string(tmp_cnt++);
                string operand_flag = (unaryexp->t == Type::Float) ? "0.0" : "0";
                ir::Type operand_type = (unaryexp->t == Type::Float) ? (ir::Type::FloatLiteral) : (ir::Type::IntLiteral);
                ir::Operator operator_defname = (unaryexp->t == Type::Float) ? (ir::Operator::fdef) : (ir::Operator::def);
                ir::Operator operator_flag = (unaryexp->t == Type::Float) ? (ir::Operator::fsub) : (ir::Operator::sub);
                ir::Instruction *def_inst = new ir::Instruction(ir::Operand(operand_flag, operand_type), ir::Operand(), ir::Operand(tmp_zero, unaryexp->t), operator_defname);

                string tmp_minu = "__temp_var_" + std::to_string(tmp_cnt++);
                ir::Instruction *minu_inst = new ir::Instruction(ir::Operand(tmp_zero, unaryexp->t), ir::Operand(unaryexp->v, unaryexp->t), ir::Operand(tmp_minu, unaryexp->t), operator_flag);

                res.push_back(def_inst);
                res.push_back(minu_inst);

                root->t = unaryexp->t;
                root->v = tmp_minu;
            }
        }
        else if (unaryop_term->token.type == TokenType::NOT)
        { // !
            if (unaryexp->t == Type::IntLiteral || unaryexp->t == Type::FloatLiteral)
            {
                int tmp = (unaryexp->t == Type::FloatLiteral) ? (!std::stof(unaryexp->v)) : (!std::stoi(unaryexp->v));
                root->v = std::to_string(tmp);
                root->t = Type::IntLiteral; // must be int
            }
            else if (unaryexp->t == Type::Int || unaryexp->t == Type::Float)
            {
                string tmp_not = "__temp_var_" + std::to_string(tmp_cnt++);
                // ir: not tmp_not, unaryexp->v
                ir::Instruction *inst = new ir::Instruction(ir::Operand(unaryexp->v, unaryexp->t), ir::Operand(), ir::Operand(tmp_not, ir::Type::Int), ir::Operator::_not);
                res.push_back(inst);
                root->v = tmp_not;
                root->t = Type::Int;
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
        Exp *exp = dynamic_cast<Exp *>(root->children[i]);
        vector<ir::Instruction *> insts = analyzeExp(exp);
        res.insert(res.end(), insts.begin(), insts.end());

        if (func_para_type[cnt].type == ir::Type::Float)
        { // type conversion of func_para_type[cnt] into float
            if (exp->t == Type::Int)
            {
                string tmp_i2f_flag = "__temp_var_" + std::to_string(tmp_cnt++);
                ir::Instruction *inst = new Instruction(ir::Operand(exp->v, ir::Type::Int), ir::Operand(), ir::Operand(tmp_i2f_flag, ir::Type::Float), ir::Operator::cvt_i2f);
                res.push_back(inst);
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
                string tmp_f2i_flag = "__temp_var_" + std::to_string(tmp_cnt++);
                ir::Instruction *cvtInst = new Instruction(ir::Operand(exp->v, ir::Type::Float), ir::Operand(), ir::Operand(tmp_f2i_flag, ir::Type::Int), ir::Operator::cvt_f2i);
                res.push_back(cvtInst);
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
        Exp *exp = dynamic_cast<Exp *>(root->children[1]);
        res = analyzeExp(exp);
        root->v = exp->v;
        root->t = exp->t;
        return res;
    }
    else
    {
        if (LVal *lval = dynamic_cast<LVal *>(root->children[0]))
        { // PrimaryExp -> LVal
            res = analyzeLVal(lval);
            root->t = lval->t;
            root->v = lval->v;
            return res;
        }
        else if (Number *number = dynamic_cast<Number *>(root->children[0]))
        { // PrimaryExp -> Number
            analyzeNumber(number);
            root->t = number->t;
            root->v = number->v;
            return res;
        }
    }

    return res;
}

// LVal -> Ident {'[' Exp ']'}
vector<Instruction *> frontend::Analyzer::analyzeLVal(LVal *root)
{
    vector<Instruction *> res;
    Term *term = dynamic_cast<Term *>(root->children[0]);
    string var_flag = term->token.value; // current variable name

    if (root->children.size() == 1)
    {                                                     // 0-dim variable: Int, Float, IntPtr, FloatPtr, IntLiteral, FloatLiteral
        STE operand_ste = symbol_table.get_ste(var_flag); // get symbol table entry
        root->t = operand_ste.operand.type;
        if (root->t == Type::IntLiteral || root->t == Type::FloatLiteral)
            root->v = operand_ste.literalVal;
        else
            root->v = symbol_table.get_scoped_name(var_flag);
        return res;
    }

    else if (root->children.size() == 4)
    { // 1-dim variable: Int, Float, IntPtr, FloatPtr
        // here we need to calculate the address of the variable
        // if var[exp].dim = 0, it's a variable
        // if var[exp].dim = 1, it's a pointer
        Exp *exp = dynamic_cast<Exp *>(root->children[2]);
        vector<Instruction *> exp_res = analyzeExp(exp);
        res.insert(res.end(), exp_res.begin(), exp_res.end());

        STE operand_ste = symbol_table.get_ste(var_flag);
        if (operand_ste.dimension.size() == 1)
        {                                                                                          // Int or Float
            Type target_type = operand_ste.operand.type == Type::IntPtr ? Type::Int : Type::Float; // if ste type is IntPtr, then target type is Int else Float

            string tmp_var_flag = "__temp_var_" + std::to_string(tmp_cnt++);
            // ir: load var_flag[exp], tmp_var_flag
            Instruction *inst = new ir::Instruction(ir::Operand(symbol_table.get_scoped_name(var_flag), operand_ste.operand.type), ir::Operand(exp->v, exp->t), ir::Operand(tmp_var_flag, target_type), ir::Operator::load);
            res.push_back(inst);
            root->v = tmp_var_flag;
            root->t = target_type;
            return res;
        }
        else
        {                                                // IntPtr or FloatPtr
            Type target_type = operand_ste.operand.type; // same as the operand type

            string tmp_var_flag = "__temp_var_" + std::to_string(tmp_cnt++);
            if (exp->t == Type::IntLiteral)
            { // offset = exp->v * operand_ste.dimension[1]
                int val = std::stoi(exp->v) * operand_ste.dimension[1];
                // getptr var_flag[exp], tmp_var_flag
                Instruction *inst = new ir::Instruction(ir::Operand(symbol_table.get_scoped_name(var_flag), operand_ste.operand.type), ir::Operand(std::to_string(val), Type::IntLiteral), ir::Operand(tmp_var_flag, target_type), ir::Operator::getptr);
                res.push_back(inst);
            }
            else
            { // offset is need to be calculated by ir
                string tmp_offset_flag = "__temp_var_" + std::to_string(tmp_cnt++);
                string tmp_len_flag = "__temp_var_" + std::to_string(tmp_cnt++);
                /**
                 * My strategy here:
                 * - offset = exp->v * operand_ste.dimension[1]
                 */

                // ir: def tmp_len_flag, operand_ste.dimension[1]
                // ir: mul exp->v, tmp_len_flag, tmp_offset_flag
                // ir: getptr var_flag[exp], tmp_offset_flag, tmp_var_flag
                Instruction *def_inst = new ir::Instruction(ir::Operand(std::to_string(operand_ste.dimension[1]), Type::IntLiteral), ir::Operand(), ir::Operand(tmp_len_flag, Type::Int), ir::Operator::def);
                Instruction *mul_inst = new ir::Instruction(ir::Operand(exp->v, Type::Int), ir::Operand(tmp_len_flag, Type::Int), ir::Operand(tmp_offset_flag, Type::Int), ir::Operator::mul);
                Instruction *getptr_inst = new ir::Instruction(ir::Operand(symbol_table.get_scoped_name(var_flag), operand_ste.operand.type), ir::Operand(tmp_offset_flag, Type::Int), ir::Operand(tmp_var_flag, target_type), ir::Operator::getptr);
                res.push_back(def_inst);
                res.push_back(mul_inst);
                res.push_back(getptr_inst);
            }
            root->t = target_type;
            root->v = tmp_var_flag;
            return res;
        }
    }
    else if (root->children.size() == 7)
    { // 2-dim variable: Int, Float, IntPtr, FloatPtr
        Exp *exp1 = dynamic_cast<Exp *>(root->children[2]);
        Exp *exp2 = dynamic_cast<Exp *>(root->children[5]);
        vector<Instruction *> exp1_res = analyzeExp(exp1);
        vector<Instruction *> exp2_res = analyzeExp(exp2);
        res.insert(res.end(), exp1_res.begin(), exp1_res.end());
        res.insert(res.end(), exp2_res.begin(), exp2_res.end());
        STE operand_ste = symbol_table.get_ste(var_flag);

        Type target_type = operand_ste.operand.type == Type::IntPtr ? Type::Int : Type::Float; // must be Int or Float

        string tmp_dim1_flag = "__temp_var_" + std::to_string(tmp_cnt++);
        string tmp_dim2_flag = "__temp_var_" + std::to_string(tmp_cnt++);
        Instruction *def1_inst = new ir::Instruction(ir::Operand(exp1->v, exp1->t), ir::Operand(), ir::Operand(tmp_dim1_flag, Type::Int), ir::Operator::def);
        Instruction *def2_inst = new ir::Instruction(ir::Operand(exp2->v, exp2->t), ir::Operand(), ir::Operand(tmp_dim2_flag, Type::Int), ir::Operator::def);
        string tmp_col_len = "__temp_var_" + std::to_string(tmp_cnt++);
        Instruction *def3_inst = new ir::Instruction(ir::Operand(std::to_string(operand_ste.dimension[1]), Type::IntLiteral), ir::Operand(), ir::Operand(tmp_col_len, Type::Int), ir::Operator::def);
        string tmp_line_offset = "__temp_var_" + std::to_string(tmp_cnt++);
        Instruction *mul_offset_inst = new ir::Instruction(ir::Operand(tmp_dim1_flag, Type::Int), ir::Operand(tmp_col_len, Type::Int), ir::Operand(tmp_line_offset, Type::Int), ir::Operator::mul);
        string tmp_total_offset = "__temp_var_" + std::to_string(tmp_cnt++);
        Instruction *add_offset_inst = new ir::Instruction(ir::Operand(tmp_line_offset, Type::Int), ir::Operand(tmp_dim2_flag, Type::Int), ir::Operand(tmp_total_offset, Type::Int), ir::Operator::add);

        string tmp_load_val = "__temp_var_" + std::to_string(tmp_cnt++);
        Instruction *load_inst = new ir::Instruction(ir::Operand(symbol_table.get_scoped_name(var_flag), operand_ste.operand.type), ir::Operand(tmp_total_offset, Type::Int), ir::Operand(tmp_load_val, target_type), ir::Operator::load);

        // ir herre:
        // def tmp_dim1_flag, exp1->v
        // def tmp_dim2_flag, exp2->v
        // def tmp_col_len, operand_ste.dimension[1]
        // def tmp_line_offset, tmp_dim1_flag * tmp_col_len
        // def tmp_total_offset, tmp_line_offset + tmp_dim2_flag  // tmp_total_offset = tmp_dim1_flag * tmp_col_len + tmp_dim2_flag
        // load tmp_load_val, var_flag[tmp_total_offset]
        res.push_back(def1_inst);
        res.push_back(def2_inst);
        res.push_back(def3_inst);
        res.push_back(mul_offset_inst);
        res.push_back(add_offset_inst);
        res.push_back(load_inst);
        root->t = target_type;
        root->v = tmp_load_val;
        return res;
    }

    return res;
}

// Number -> IntConst | floatConst
// Just update the type and value of Number, do not generate ir code
vector<ir::Instruction *> frontend::Analyzer::analyzeNumber(Number *root)
{
    Term *term = dynamic_cast<Term *>(root->children[0]);

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
    { // Yehn! nothing to do 🤣
        root->t = Type::FloatLiteral;
        root->v = term->token.value;
    }

    return std::vector<ir::Instruction *>();
}

// Cond -> LOrExp
vector<ir::Instruction *> frontend::Analyzer::analyzeCond(Cond *root)
{
    LOrExp *lorexp = dynamic_cast<LOrExp *>(root->children[0]);
    vector<ir::Instruction *> insts = analyzeLOrExp(lorexp);
    root->v = lorexp->v;
    root->t = lorexp->t;
    return insts;
}

// LOrExp -> LAndExp [ '||' LOrExp ]
vector<ir::Instruction *> frontend::Analyzer::analyzeLOrExp(LOrExp *root)
{
    vector<ir::Instruction *> res;

    LAndExp *landexp = dynamic_cast<LAndExp *>(root->children[0]);
    vector<ir::Instruction *> insts = analyzeLAndExp(landexp); // get LAndExp
    res.insert(res.end(), insts.begin(), insts.end());
    root->v = landexp->v;
    root->t = landexp->t;

    if (root->children.size() == 1)
    { // only LAndExp, you can return now
        return res;
    }
    else
    { // LAndExp '||' LOrExp
        LOrExp *lorexp = dynamic_cast<LOrExp *>(root->children[2]);
        vector<ir::Instruction *> insts = analyzeLOrExp(lorexp); // wait for result of LAndExp

        if (root->t == Type::Float)
        { // root = (LAndExp != 0), into int
            string tmp = "__temp_var_" + std::to_string(tmp_cnt++);
            ir::Instruction *inst = new ir::Instruction(ir::Operand(root->v, Type::Float), ir::Operand("0.0", Type::FloatLiteral), ir::Operand(tmp, Type::Int), ir::Operator::fneq);
            res.push_back(inst);
            root->v = tmp;
            root->t = Type::Int;
        }
        else if (root->t == Type::FloatLiteral)
        {
            float val = std::stof(root->v);
            root->t = Type::IntLiteral;
            root->v = std::to_string(val != 0);
        }

        if (lorexp->t == Type::Float)
        { // lorexp = (LOrExp != 0), into int
            string tmp = "__temp_var_" + std::to_string(tmp_cnt++);
            ir::Instruction *inst = new ir::Instruction(ir::Operand(lorexp->v, Type::Float), ir::Operand("0.0", Type::FloatLiteral), ir::Operand(tmp, Type::Int), ir::Operator::fneq);
            insts.push_back(inst);
            lorexp->v = tmp;
            lorexp->t = Type::Int;
        }
        else if (lorexp->t == Type::FloatLiteral)
        {
            float val = std::stof(lorexp->v);
            lorexp->t = Type::IntLiteral;
            lorexp->v = std::to_string(val != 0);
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
            string tmp_cal_flag = "__temp_var_" + std::to_string(tmp_cnt++); // to record the result of LOrExp
            Instruction *inst = new Instruction(ir::Operand(root->v, root->t), ir::Operand(lorexp->v, lorexp->t), ir::Operand(tmp_cal_flag, ir::Type::Int), ir::Operator::_or);
            insts.push_back(inst);

            Instruction *root_true_goto = new Instruction(ir::Operand(root->v, root->t), ir::Operand(), ir::Operand("2", Type::IntLiteral), ir::Operator::_goto);
            Instruction *root_false_goto = new Instruction(ir::Operand(), ir::Operand(), ir::Operand("3", Type::IntLiteral), ir::Operator::_goto);
            Instruction *root_true_assign = new Instruction(ir::Operand("1", Type::IntLiteral), ir::Operand(), ir::Operand(tmp_cal_flag, ir::Type::Int), ir::Operator::mov);
            Instruction *true_logic_goto = new Instruction(ir::Operand(), ir::Operand(), ir::Operand(std::to_string(insts.size() + 1), Type::IntLiteral), ir::Operator::_goto);

            res.push_back(root_true_goto);
            res.push_back(root_false_goto);
            res.push_back(root_true_assign);
            res.push_back(true_logic_goto);
            res.insert(res.end(), insts.begin(), insts.end());

            root->v = tmp_cal_flag;
            root->t = Type::Int;
        }
        return res;
    }
}

// LAndExp -> EqExp [ '&&' LAndExp ]
vector<ir::Instruction *> frontend::Analyzer::analyzeLAndExp(LAndExp *root)
{
    vector<ir::Instruction *> res;
    EqExp *eqexp = dynamic_cast<EqExp *>(root->children[0]);
    vector<ir::Instruction *> insts = analyzeEqExp(eqexp);
    res.insert(res.end(), insts.begin(), insts.end());
    root->v = eqexp->v;
    root->t = eqexp->t;

    if (root->children.size() == 1)
    { // only EqExp, you can return now
        return res;
    }
    else
    {
        LAndExp *landexp = dynamic_cast<LAndExp *>(root->children[2]);
        vector<ir::Instruction *> insts = analyzeLAndExp(landexp);

        if (root->t == Type::Float)
        {
            string tmp = "__temp_var_" + std::to_string(tmp_cnt++);
            ir::Instruction *inst = new ir::Instruction(ir::Operand(root->v, Type::Float), ir::Operand("0.0", Type::FloatLiteral), ir::Operand(tmp, Type::Int), ir::Operator::fneq);
            res.push_back(inst);
            root->v = tmp;
            root->t = Type::Int;
        }
        else if (root->t == Type::FloatLiteral)
        { // root->v = (EqExp != 0), into int
            float val = std::stof(root->v);
            root->t = Type::IntLiteral;
            root->v = std::to_string(val != 0);
        }

        if (landexp->t == Type::Float)
        {
            string tmp = "__temp_var_" + std::to_string(tmp_cnt++);
            ir::Instruction *inst = new ir::Instruction(ir::Operand(landexp->v, Type::Float), ir::Operand("0.0", Type::FloatLiteral), ir::Operand(tmp, Type::Int), ir::Operator::fneq);
            insts.push_back(inst);
            landexp->v = tmp;
            landexp->t = Type::Int;
        }
        else if (landexp->t == Type::FloatLiteral)
        {
            float val = std::stof(landexp->v);
            landexp->t = Type::IntLiteral;
            landexp->v = std::to_string(val != 0);
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
            string tmp_cal_flag = "__temp_var_" + std::to_string(tmp_cnt++);

            Instruction *root_true_goto = new Instruction(ir::Operand(root->v, root->t), ir::Operand(), ir::Operand("2", Type::IntLiteral), ir::Operator::_goto);
            res.push_back(root_true_goto);

            Instruction *inst = new Instruction(ir::Operand(root->v, root->t), ir::Operand(landexp->v, landexp->t), ir::Operand(tmp_cal_flag, ir::Type::Int), ir::Operator::_and);
            Instruction *true_logic_goto = new Instruction(ir::Operand(), ir::Operand(), ir::Operand("2", Type::IntLiteral), ir::Operator::_goto);
            insts.push_back(inst);
            insts.push_back(true_logic_goto);

            Instruction *root_false_goto = new Instruction(ir::Operand(), ir::Operand(), ir::Operand(std::to_string(insts.size() + 1), Type::IntLiteral), ir::Operator::_goto);
            Instruction *root_false_assign = new Instruction(ir::Operand("0", Type::IntLiteral), ir::Operand(), ir::Operand(tmp_cal_flag, ir::Type::Int), ir::Operator::mov);
            res.push_back(root_false_goto);
            res.insert(res.end(), insts.begin(), insts.end());
            res.push_back(root_false_assign);

            root->v = tmp_cal_flag;
            root->t = Type::Int;
        }
        return res;
    }
}

// EqExp -> RelExp { ('==' | '!=') RelExp }
vector<ir::Instruction *> frontend::Analyzer::analyzeEqExp(EqExp *root)
{
    // 可能的类型: Int or IntLiteral or Float or FloatLiteral (Float or FloatLiteral: 仅有一个RelExp, 不进行关系运算)
    vector<Instruction *> res;

    for (int i = 0; i < root->children.size(); i += 2)
    {
        RelExp *relexp = dynamic_cast<RelExp *>(root->children[i]);
        vector<Instruction *> cal_insts = analyzeRelExp(relexp);
        res.insert(res.end(), cal_insts.begin(), cal_insts.end());
    }

    // 计算表达式的结果
    // 默认为第一个RelExp的值
    RelExp *firstRelExp = dynamic_cast<RelExp *>(root->children[0]);
    root->t = firstRelExp->t;
    root->v = firstRelExp->v;

    // RelExp
    if (root->children.size() == 1)
    {
        return res;
    }

    // { ('==' | '!=') RelExp }
    for (int i = 2; i < root->children.size(); i += 2)
    {
        RelExp *relexp = dynamic_cast<RelExp *>(root->children[i]);
        Term *op_term = dynamic_cast<Term *>(root->children[i - 1]);
        Type target_type = root->t;
        // 在计算之前，先将两个操作数的类型变为相同的
        if (root->t != relexp->t)
        {
            // 确定类型提升
            if (relexp->t == ir::Type::Float)
                target_type = ir::Type::Float;
            else if (relexp->t == ir::Type::Int && target_type == ir::Type::IntLiteral)
                target_type = ir::Type::Int;
            else if (relexp->t == ir::Type::FloatLiteral && target_type == ir::Type::IntLiteral)
                target_type = ir::Type::FloatLiteral;
            else if ((relexp->t == ir::Type::FloatLiteral && target_type == ir::Type::Int) || (target_type == ir::Type::FloatLiteral && relexp->t == ir::Type::Int)) // 提升没有顺序
                target_type = ir::Type::Float;

            // 执行类型转换
            if (target_type == Type::Int)
            { // IntLiteral -> Int
                IntLiteral2Int(root, relexp, frontend::NodeType::RELEXP, res);
            }
            else if (target_type == Type::FloatLiteral)
            { // IntLiteral -> FloatLiteral
                IntLiteral2FloatLiteral(root, relexp, frontend::NodeType::RELEXP);
            }
            else if (target_type == Type::Float)
            { // IntLiteral -> Float, Int -> Float, FloatLiteral -> Float
                IntLiteral2Float(root, relexp, frontend::NodeType::RELEXP, res);
                Int2Float(root, relexp, frontend::NodeType::RELEXP, res);
                FloatLiteral2Float(root, relexp, frontend::NodeType::RELEXP, res);
            }
        }
        // 已经化为相同类型，可以开始计算
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
            // 无法在编译期确定结果，生成指令
            string tmp_cal_flag = "t" + std::to_string(tmp_cnt++);
            Instruction *calInst;
            if (op_term->token.type == TokenType::EQL)
                calInst = new Instruction(ir::Operand(root->v, ir::Type::Int), ir::Operand(relexp->v, ir::Type::Int), ir::Operand(tmp_cal_flag, ir::Type::Int), ir::Operator::eq);
            else if (op_term->token.type == TokenType::NEQ)
                calInst = new Instruction(ir::Operand(root->v, ir::Type::Int), ir::Operand(relexp->v, ir::Type::Int), ir::Operand(tmp_cal_flag, ir::Type::Int), ir::Operator::neq);
            res.push_back(calInst);
            root->v = tmp_cal_flag;
            root->t = Type::Int;
        }
        else if (target_type == Type::Float)
        {
            // 无法在编译期确定结果，生成指令
            string tmp_cal_flag = "t" + std::to_string(tmp_cnt++);
            Instruction *calInst;
            if (op_term->token.type == TokenType::LSS)
                calInst = new Instruction(ir::Operand(root->v, ir::Type::Float), ir::Operand(relexp->v, ir::Type::Float), ir::Operand(tmp_cal_flag, ir::Type::Float), ir::Operator::feq);
            else if (op_term->token.type == TokenType::GTR)
                calInst = new Instruction(ir::Operand(root->v, ir::Type::Float), ir::Operand(relexp->v, ir::Type::Float), ir::Operand(tmp_cal_flag, ir::Type::Float), ir::Operator::fneq);
            res.push_back(calInst);
            root->v = tmp_cal_flag;
            root->t = Type::Float;
        }
    }
    return res;
}

// RelExp -> AddExp { ('<' | '>' | '<=' | '>=') AddExp }
vector<ir::Instruction *> frontend::Analyzer::analyzeRelExp(RelExp *root)
{
    // 可能的类型: Int or IntLiteral or Float or FloatLiteral (Float or FloatLiteral: 仅有一个AddExp, 不进行关系运算)

    // 类似AddExp与MulExp, 需要将所有参与计算的值统一才可以进行计算
    vector<Instruction *> res;

    // 深度优先遍历, 先计算其所有子AddExp
    for (int i = 0; i < root->children.size(); i += 2)
    {
        AddExp *addexp = dynamic_cast<AddExp *>(root->children[i]);
        assert(addexp != nullptr);
        vector<Instruction *> cal_insts = analyzeAddExp(addexp);
        res.insert(res.end(), cal_insts.begin(), cal_insts.end());
    }

    // 计算表达式的结果
    // 默认为第一个AddExp的值
    AddExp *firstAddExp = dynamic_cast<AddExp *>(root->children[0]);
    assert(firstAddExp);
    root->t = firstAddExp->t;
    root->v = firstAddExp->v;

    // AddExp
    if (root->children.size() == 1)
    {
        return res;
    }

    // { ('<' | '>' | '<=' | '>=') AddExp }
    for (int i = 2; i < root->children.size(); i += 2)
    {
        AddExp *addexp = dynamic_cast<AddExp *>(root->children[i]);
        Term *op_term = dynamic_cast<Term *>(root->children[i - 1]);
        Type target_type = root->t;

        // 在计算之前，先将两个操作数的类型变为相同的
        if (root->t != addexp->t)
        {
            // 确定类型提升
            if (addexp->t == ir::Type::Float)
                target_type = ir::Type::Float;
            else if (addexp->t == ir::Type::Int && target_type == ir::Type::IntLiteral)
                target_type = ir::Type::Int;
            else if (addexp->t == ir::Type::FloatLiteral && target_type == ir::Type::IntLiteral)
                target_type = ir::Type::FloatLiteral;
            else if ((addexp->t == ir::Type::FloatLiteral && target_type == ir::Type::Int) || (target_type == ir::Type::FloatLiteral && addexp->t == ir::Type::Int)) // 提升没有顺序
                target_type = ir::Type::Float;

            // 执行类型转换
            if (target_type == Type::Int)
            { // IntLiteral -> Int
                IntLiteral2Int(root, addexp, frontend::NodeType::ADDEXP, res);
            }
            else if (target_type == Type::FloatLiteral)
            { // IntLiteral -> FloatLiteral
                IntLiteral2FloatLiteral(root, addexp, frontend::NodeType::ADDEXP);
            }
            else if (target_type == Type::Float)
            { // IntLiteral -> Float, Int -> Float, FloatLiteral -> Float
                IntLiteral2Float(root, addexp, frontend::NodeType::ADDEXP, res);
                Int2Float(root, addexp, frontend::NodeType::ADDEXP, res);
                FloatLiteral2Float(root, addexp, frontend::NodeType::ADDEXP, res);
            }
            else
                assert(0 && "Error");
        }

        // 已经化为相同类型，可以开始计算
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
            else
                assert(0 && "Invalid Op");
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
            else
                assert(0 && "Invalid Op");

            root->t = Type::IntLiteral;
        }
        else if (target_type == Type::Int)
        {
            // 无法在编译期确定结果，生成指令
            string tmp_cal_flag = "t" + std::to_string(tmp_cnt++);
            Instruction *calInst;
            if (op_term->token.type == TokenType::LSS)
                calInst = new Instruction(ir::Operand(root->v, ir::Type::Int),
                                          ir::Operand(addexp->v, ir::Type::Int),
                                          ir::Operand(tmp_cal_flag, ir::Type::Int), ir::Operator::lss);
            else if (op_term->token.type == TokenType::GTR)
                calInst = new Instruction(ir::Operand(root->v, ir::Type::Int),
                                          ir::Operand(addexp->v, ir::Type::Int),
                                          ir::Operand(tmp_cal_flag, ir::Type::Int), ir::Operator::gtr);
            else if (op_term->token.type == TokenType::LEQ)
                calInst = new Instruction(ir::Operand(root->v, ir::Type::Int),
                                          ir::Operand(addexp->v, ir::Type::Int),
                                          ir::Operand(tmp_cal_flag, ir::Type::Int), ir::Operator::leq);
            else if (op_term->token.type == TokenType::GEQ)
                calInst = new Instruction(ir::Operand(root->v, ir::Type::Int),
                                          ir::Operand(addexp->v, ir::Type::Int),
                                          ir::Operand(tmp_cal_flag, ir::Type::Int), ir::Operator::geq);
            else
                assert(0 && "Invalid Op");
            res.push_back(calInst);
            root->v = tmp_cal_flag;
            root->t = Type::Int;
        }
        else if (target_type == Type::Float)
        {
            // 无法在编译期确定结果，生成指令
            string tmp_cal_flag = "t" + std::to_string(tmp_cnt++);
            Instruction *calInst;
            if (op_term->token.type == TokenType::LSS)
                calInst = new Instruction(ir::Operand(root->v, ir::Type::Float),
                                          ir::Operand(addexp->v, ir::Type::Float),
                                          ir::Operand(tmp_cal_flag, ir::Type::Float), ir::Operator::flss);
            else if (op_term->token.type == TokenType::GTR)
                calInst = new Instruction(ir::Operand(root->v, ir::Type::Float),
                                          ir::Operand(addexp->v, ir::Type::Float),
                                          ir::Operand(tmp_cal_flag, ir::Type::Float), ir::Operator::fgtr);
            else if (op_term->token.type == TokenType::LEQ)
                calInst = new Instruction(ir::Operand(root->v, ir::Type::Float),
                                          ir::Operand(addexp->v, ir::Type::Float),
                                          ir::Operand(tmp_cal_flag, ir::Type::Float), ir::Operator::fleq);
            else if (op_term->token.type == TokenType::GEQ)
                calInst = new Instruction(ir::Operand(root->v, ir::Type::Float),
                                          ir::Operand(addexp->v, ir::Type::Float),
                                          ir::Operand(tmp_cal_flag, ir::Type::Float), ir::Operator::fgeq);
            else
                assert(0 && "Invalid Op");
            res.push_back(calInst);
            root->v = tmp_cal_flag;
            root->t = Type::Float;
        }
        else
            assert(0 && "Error");
    }
    return res;
}

// ---------- 辅助函数，在计算表达式的值的时候进行类型转换 ----------
// 一开始是为了重用代码而将其抽象成函数，但由于C++中无法对变量进行原地类型转换，因此下面的函数中重用的代码还是需要写两遍，并没有减少代码量

void frontend::Analyzer::IntLiteral2Int(AstNode *root, AstNode *child, frontend::NodeType type, vector<Instruction *> &res)
{
    if (type == frontend::NodeType::RELEXP)
    {
        EqExp *rt = dynamic_cast<EqExp *>(root);
        RelExp *chd = dynamic_cast<RelExp *>(child);
        assert(rt != nullptr && chd != nullptr);
        if (chd->t == Type::IntLiteral)
        {
            string tmp_intcvt_flag = "t" + std::to_string(tmp_cnt++);
            Instruction *cvtInst = new Instruction(ir::Operand(chd->v, ir::Type::IntLiteral),
                                                   ir::Operand(),
                                                   ir::Operand(tmp_intcvt_flag, ir::Type::Int), ir::Operator::def);
            res.push_back(cvtInst);
            chd->v = tmp_intcvt_flag;
            chd->t = Type::Int;
        }
        if (rt->t == Type::IntLiteral)
        {
            string tmp_intcvt_flag = "t" + std::to_string(tmp_cnt++);
            Instruction *cvtInst = new Instruction(ir::Operand(rt->v, ir::Type::IntLiteral),
                                                   ir::Operand(),
                                                   ir::Operand(tmp_intcvt_flag, ir::Type::Int), ir::Operator::def);
            res.push_back(cvtInst);
            rt->v = tmp_intcvt_flag;
            rt->t = Type::Int;
        }
    }
    else if (type == frontend::NodeType::ADDEXP)
    {
        RelExp *rt = dynamic_cast<RelExp *>(root);
        AddExp *chd = dynamic_cast<AddExp *>(child);
        assert(rt != nullptr && chd != nullptr);
        if (chd->t == Type::IntLiteral)
        {
            string tmp_intcvt_flag = "t" + std::to_string(tmp_cnt++);
            Instruction *cvtInst = new Instruction(ir::Operand(chd->v, ir::Type::IntLiteral),
                                                   ir::Operand(),
                                                   ir::Operand(tmp_intcvt_flag, ir::Type::Int), ir::Operator::def);
            res.push_back(cvtInst);
            chd->v = tmp_intcvt_flag;
            chd->t = Type::Int;
        }
        if (rt->t == Type::IntLiteral)
        {
            string tmp_intcvt_flag = "t" + std::to_string(tmp_cnt++);
            Instruction *cvtInst = new Instruction(ir::Operand(rt->v, ir::Type::IntLiteral),
                                                   ir::Operand(),
                                                   ir::Operand(tmp_intcvt_flag, ir::Type::Int), ir::Operator::def);
            res.push_back(cvtInst);
            rt->v = tmp_intcvt_flag;
            rt->t = Type::Int;
        }
    }
    else
    {
        assert(0 && "NodeType Error");
    }
    return;
}

void frontend::Analyzer::IntLiteral2FloatLiteral(AstNode *root, AstNode *child, frontend::NodeType type)
{
    if (type == frontend::NodeType::RELEXP)
    {
        EqExp *rt = dynamic_cast<EqExp *>(root);
        RelExp *chd = dynamic_cast<RelExp *>(child);
        assert(rt != nullptr && chd != nullptr);
        if (chd->t == Type::IntLiteral)
        {
            float val = std::stoi(chd->v);
            chd->v = std::to_string(val);
            chd->t = Type::FloatLiteral;
        }
        if (rt->t == Type::IntLiteral)
        {
            float val = std::stoi(rt->v);
            rt->v = std::to_string(val);
            rt->t = Type::FloatLiteral;
        }
    }
    else if (type == frontend::NodeType::ADDEXP)
    {
        RelExp *rt = dynamic_cast<RelExp *>(root);
        AddExp *chd = dynamic_cast<AddExp *>(child);
        assert(rt != nullptr && chd != nullptr);
        if (chd->t == Type::IntLiteral)
        {
            float val = std::stoi(chd->v);
            chd->v = std::to_string(val);
            chd->t = Type::FloatLiteral;
        }
        if (rt->t == Type::IntLiteral)
        {
            float val = std::stoi(rt->v);
            rt->v = std::to_string(val);
            rt->t = Type::FloatLiteral;
        }
    }
    else
    {
        assert(0 && "NodeType Error");
    }
    return;
}

void frontend::Analyzer::IntLiteral2Float(AstNode *root, AstNode *child, frontend::NodeType type, vector<Instruction *> &res)
{
    if (type == frontend::NodeType::RELEXP)
    {
        EqExp *rt = dynamic_cast<EqExp *>(root);
        RelExp *chd = dynamic_cast<RelExp *>(child);
        assert(rt != nullptr && chd != nullptr);
        if (chd->t == Type::IntLiteral)
        {
            float val = std::stof(chd->v);
            string tmp_intcvt_flag = "t" + std::to_string(tmp_cnt++);
            Instruction *cvtInst = new Instruction(ir::Operand(std::to_string(val), ir::Type::FloatLiteral),
                                                   ir::Operand(),
                                                   ir::Operand(tmp_intcvt_flag, ir::Type::Float), ir::Operator::fdef);
            res.push_back(cvtInst);
            chd->v = tmp_intcvt_flag;
            chd->t = Type::Float;
        }
        if (rt->t == Type::IntLiteral)
        {
            float val = std::stof(rt->v);
            string tmp_intcvt_flag = "t" + std::to_string(tmp_cnt++);
            Instruction *cvtInst = new Instruction(ir::Operand(std::to_string(val), ir::Type::FloatLiteral),
                                                   ir::Operand(),
                                                   ir::Operand(tmp_intcvt_flag, ir::Type::Float), ir::Operator::fdef);
            res.push_back(cvtInst);
            rt->v = tmp_intcvt_flag;
            rt->t = Type::Float;
        }
    }
    else if (type == frontend::NodeType::ADDEXP)
    {
        RelExp *rt = dynamic_cast<RelExp *>(root);
        AddExp *chd = dynamic_cast<AddExp *>(child);
        assert(rt != nullptr && chd != nullptr);
        if (chd->t == Type::IntLiteral)
        { // IntLiteral -> Float
            float val = std::stof(chd->v);
            string tmp_intcvt_flag = "t" + std::to_string(tmp_cnt++);
            Instruction *cvtInst = new Instruction(ir::Operand(std::to_string(val), ir::Type::FloatLiteral),
                                                   ir::Operand(),
                                                   ir::Operand(tmp_intcvt_flag, ir::Type::Float), ir::Operator::fdef);
            res.push_back(cvtInst);
            chd->v = tmp_intcvt_flag;
            chd->t = Type::Float;
        }
        if (rt->t == Type::IntLiteral)
        {
            float val = std::stof(rt->v);
            string tmp_intcvt_flag = "t" + std::to_string(tmp_cnt++);
            Instruction *cvtInst = new Instruction(ir::Operand(std::to_string(val), ir::Type::FloatLiteral),
                                                   ir::Operand(),
                                                   ir::Operand(tmp_intcvt_flag, ir::Type::Float), ir::Operator::fdef);
            res.push_back(cvtInst);
            rt->v = tmp_intcvt_flag;
            rt->t = Type::Float;
        }
    }
    else
    {
        assert(0 && "NodeType Error");
    }
    return;
}

void frontend::Analyzer::Int2Float(AstNode *root, AstNode *child, frontend::NodeType type, vector<Instruction *> &res)
{
    if (type == frontend::NodeType::RELEXP)
    {
        EqExp *rt = dynamic_cast<EqExp *>(root);
        RelExp *chd = dynamic_cast<RelExp *>(child);
        assert(rt != nullptr && chd != nullptr);
        if (chd->t == Type::Int)
        {
            string tmp_intcvt_flag = "t" + std::to_string(tmp_cnt++);
            Instruction *cvtInst = new Instruction(ir::Operand(chd->v, ir::Type::Int),
                                                   ir::Operand(),
                                                   ir::Operand(tmp_intcvt_flag, ir::Type::Float), ir::Operator::cvt_i2f);
            res.push_back(cvtInst);
            chd->v = tmp_intcvt_flag;
            chd->t = Type::Float;
        }
        if (rt->t == Type::Int)
        {
            float val = std::stof(rt->v);
            string tmp_intcvt_flag = "t" + std::to_string(tmp_cnt++);
            Instruction *cvtInst = new Instruction(ir::Operand(rt->v, ir::Type::Int),
                                                   ir::Operand(),
                                                   ir::Operand(tmp_intcvt_flag, ir::Type::Float), ir::Operator::cvt_i2f);
            res.push_back(cvtInst);
            rt->v = tmp_intcvt_flag;
            rt->t = Type::Float;
        }
    }
    else if (type == frontend::NodeType::ADDEXP)
    {
        RelExp *rt = dynamic_cast<RelExp *>(root);
        AddExp *chd = dynamic_cast<AddExp *>(child);
        assert(rt != nullptr && chd != nullptr);
        if (chd->t == Type::Int)
        {
            string tmp_intcvt_flag = "t" + std::to_string(tmp_cnt++);
            Instruction *cvtInst = new Instruction(ir::Operand(chd->v, ir::Type::Int),
                                                   ir::Operand(),
                                                   ir::Operand(tmp_intcvt_flag, ir::Type::Float), ir::Operator::cvt_i2f);
            res.push_back(cvtInst);
            chd->v = tmp_intcvt_flag;
            chd->t = Type::Float;
        }
        if (rt->t == Type::Int)
        {
            float val = std::stof(rt->v);
            string tmp_intcvt_flag = "t" + std::to_string(tmp_cnt++);
            Instruction *cvtInst = new Instruction(ir::Operand(rt->v, ir::Type::Int),
                                                   ir::Operand(),
                                                   ir::Operand(tmp_intcvt_flag, ir::Type::Float), ir::Operator::cvt_i2f);
            res.push_back(cvtInst);
            rt->v = tmp_intcvt_flag;
            rt->t = Type::Float;
        }
    }
    else
    {
        assert(0 && "NodeType Error");
    }
    return;
}

void frontend::Analyzer::FloatLiteral2Float(AstNode *root, AstNode *child, frontend::NodeType type, vector<Instruction *> &res)
{
    if (type == frontend::NodeType::RELEXP)
    {
        EqExp *rt = dynamic_cast<EqExp *>(root);
        RelExp *chd = dynamic_cast<RelExp *>(child);
        assert(rt != nullptr && chd != nullptr);
        if (chd->t == Type::FloatLiteral)
        {
            string tmp_intcvt_flag = "t" + std::to_string(tmp_cnt++);
            Instruction *cvtInst = new Instruction(ir::Operand(chd->v, ir::Type::FloatLiteral),
                                                   ir::Operand(),
                                                   ir::Operand(tmp_intcvt_flag, ir::Type::Float), ir::Operator::fdef);
            res.push_back(cvtInst);
            chd->v = tmp_intcvt_flag;
            chd->t = Type::Float;
        }
        if (rt->t == Type::FloatLiteral)
        {
            string tmp_intcvt_flag = "t" + std::to_string(tmp_cnt++);
            Instruction *cvtInst = new Instruction(ir::Operand(rt->v, ir::Type::FloatLiteral),
                                                   ir::Operand(),
                                                   ir::Operand(tmp_intcvt_flag, ir::Type::Float), ir::Operator::fdef);
            res.push_back(cvtInst);
            rt->v = tmp_intcvt_flag;
            rt->t = Type::Float;
        }
    }
    else if (type == frontend::NodeType::ADDEXP)
    {
        RelExp *rt = dynamic_cast<RelExp *>(root);
        AddExp *chd = dynamic_cast<AddExp *>(child);
        assert(rt != nullptr && chd != nullptr);
        if (chd->t == Type::FloatLiteral)
        {
            string tmp_intcvt_flag = "t" + std::to_string(tmp_cnt++);
            Instruction *cvtInst = new Instruction(ir::Operand(chd->v, ir::Type::FloatLiteral),
                                                   ir::Operand(),
                                                   ir::Operand(tmp_intcvt_flag, ir::Type::Float), ir::Operator::fdef);
            res.push_back(cvtInst);
            chd->v = tmp_intcvt_flag;
            chd->t = Type::Float;
        }
        if (rt->t == Type::FloatLiteral)
        {
            string tmp_intcvt_flag = "t" + std::to_string(tmp_cnt++);
            Instruction *cvtInst = new Instruction(ir::Operand(rt->v, ir::Type::FloatLiteral),
                                                   ir::Operand(),
                                                   ir::Operand(tmp_intcvt_flag, ir::Type::Float), ir::Operator::fdef);
            res.push_back(cvtInst);
            rt->v = tmp_intcvt_flag;
            rt->t = Type::Float;
        }
    }
    return;
}