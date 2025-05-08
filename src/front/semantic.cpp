#include"front/semantic.h"

#include<cassert>

using ir::Instruction;
using ir::Function;
using ir::Operand;
using ir::Operator;

#define TODO assert(0 && "TODO");

#define GET_CHILD_PTR(node, type, index) auto node = dynamic_cast<type*>(root->children[index]); assert(node); 
#define ANALYSIS(node, type, index) auto node = dynamic_cast<type*>(root->children[index]); assert(node); analysis##type(node, buffer);
#define COPY_EXP_NODE(from, to) to->is_computable = from->is_computable; to->v = from->v; to->t = from->t;

map<std::string,ir::Function*>* frontend::get_lib_funcs() {
    static map<std::string,ir::Function*> lib_funcs = {
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

void frontend::SymbolTable::add_scope(Block* node) {
    auto new_scope = ScopeInfo();
    new_scope.cnt = scope_stack.size();
    new_scope.name = "block" + std::to_string(new_scope.cnt);  // block name: block0, block1, ...
    new_scope.table = map_str_ste();
    scope_stack.push_back(new_scope);
    return;
}

void frontend::SymbolTable::exit_scope() {
    if (scope_stack.empty()) {
        assert(0 && "exit scope error: no scope to exit");
    }

    scope_stack.pop_back();
    return;
}

// (name, oprand) -> (name_in_scope, oprand), oprand should in scope_stack
string frontend::SymbolTable::get_scoped_name(string id) const {
    if (scope_stack.empty()) {
        return id;
    }

    string scoped_name = id;
    for (int i = scope_stack.size() - 1; i >= 0; --i) {
        auto& scope = scope_stack[i];
        if (scope.table.find(id) != scope.table.end()) {
            return scope.name + "_" + id;
        }
    }

    assert(0 && "get_scoped_name error: no scope has id");
}

Operand frontend::SymbolTable::get_operand(string id) const {
    if (scope_stack.empty()) {
        assert(0 && "get_operand error: no scope");
    }

    for (int i = scope_stack.size() - 1; i >= 0; --i) {
        auto& scope = scope_stack[i];
        auto it = scope.table.find(id);
        if (it != scope.table.end()) {
            return it->second.operand;
        }
    }

    assert(0 && "get_operand error: no Oprand in symbol table");
}

frontend::STE frontend::SymbolTable::get_ste(string id) const {  // why this ????
    if (scope_stack.empty()) {
        assert(0 && "get_ste error: no scope");
    }

    for (int i = scope_stack.size() - 1; i >= 0; --i) {
        auto& scope = scope_stack[i];
        auto it = scope.table.find(id);
        if (it != scope.table.end()) {
            return it->second;
        }
    }

    assert(0 && "get_ste error: no STE in symbol table");
}

frontend::Analyzer::Analyzer(): tmp_cnt(0), symbol_table() {
    for (auto& [name, func] : *get_lib_funcs()) {
        auto func_ptr = new Function(name, func->ParameterList, func->returnType);
        symbol_table.functions[name] = func_ptr;
    }
}

ir::Program frontend::Analyzer::get_ir_program(CompUnit* root) {}

// ir::Program frontend::Analyzer::get_ir_program(CompUnit* root) {
//     ir::Program buffer;
//     for (auto& func : symbol_table.functions) {
//         buffer.addFunction(*(func.second));  // add lib functions to program
//     }
    
//     // add function: global
//     auto func_ptr = new Function("global", Type::null);
//     symbol_table.functions["global"] = func_ptr;
//     buffer.addFunction(*func_ptr);  // add global function to program

//     ANALYSIS(comp, CompUnit, 0);  // Vals & Functions will added in this function.
// }

// void frontend::Analyzer::analysisCompUnit(CompUnit* root, ir::Program& program) {
//     // add global variables
//     for (int i = 0; i < root->children.size(); ++i) {
//         auto child = root->children[i];
//         if (child->type == NodeType::DECL) {
//             std::vector<ir::Instruction*> buffer;
//             ANALYSIS(decl, Decl, i);
//             for (auto& inst : buffer) {
//                 program.functions['global'].addInst(inst);  // add to global function
//                 symbol_table.functions["global"]->addInst(inst);  // add to symbol table
//                 g_init_inst.push_back(inst);  // add to global init instructions
//             }
//         } else if (child->type == NodeType::FUNCDEF) {
//             ir::Function buffer;
//             ANALYSIS(funcdef, FuncDef, i);
//             program.addFunction(buffer);  // add to program
//             symbol_table.functions[buffer.name] = new ir::Function(buffer);  // add to symbol table
//         } else {
//             assert(0 && "analysisCompUnit error: unknown child type");
//         }
//     }
// }

// // Decl -> ConstDecl | VarDecl
// void frontend::Analyzer::analysisDecl(Decl* root, std::vector<ir::Instruction*>& buffer) {
//     for (int i = 0; i < root->children.size(); ++i) {
//         auto child = root->children[i];
//         if (child->type == NodeType::CONSTDECL) {
//             ANALYSIS(constdecl, ConstDecl, i);
//         } else if (child->type == NodeType::VARDECL) {
//             ANALYSIS(vardecl, VarDecl, i);
//         } else {
//             assert(0 && "analysisDecl error: unknown child type");
//         }
//     }
//     return;
// }

// // ConstDecl -> 'const' BType ConstDef { ',' ConstDef } ';'
// void frontend::Analyzer::analysisConstDecl(ConstDecl* root, std::vector<ir::Instruction*>& buffer) {
//     GET_CHILD_PTR(btype, BType, 0);
//     auto type = dynamic_cast<Term*>(btype->children[0])->token.type;  // type: int/float
//     // ir::Type ir_type = (type == TokenType::INTTK) ? ir::Type::Int : ir::Type::Float;

//     for (int i = 2; i < root->children.size(); ++i) {
//         auto child = root->children[i];
//         if (child->type == NodeType::CONSTDEF) {
//             ANALYSIS(constdef, ConstDef, i);
//             constdef->arr_name = symbol_table.get_scoped_name(constdef->arr_name);  // add scope infomation to name
//             i++; // pass ',' or ';'
//         } else {
//             assert(0 && "analysisConstDecl error: unknown child type");
//         }
//     }
//     return;
// }

// // ConstDef -> IDENFR [ '[' ConstExp ']' ] '=' ConstInitVal