#include "front/syntax.h"

#include <cassert>
#include <iostream>

using frontend::Parser;

// #define DEBUG_PARSER
#define TODO assert(0 && "todo")
#define CUR_TOKEN_IS(tk_type) (token_stream[index].type == TokenType::tk_type)
#define PARSE_TOKEN(tk_type) root->children.push_back(parseTerm(root, TokenType::tk_type))
#define PARSE(name, type)       \
    auto name = new type(root); \
    assert(parse##type(name));  \
    root->children.push_back(name);

Parser::Parser(const std::vector<frontend::Token> &tokens) : index(0), token_stream(tokens) {}

Parser::~Parser() {}

/**
 * Impl for subfunctions of parser
 */
frontend::Term *Parser::parseTerm(AstNode *root, TokenType tk_type) { return new Term(token_stream[index++], root); }
bool Parser::parseCompUnit(CompUnit *root) {
    // CompUnit -> (Decl | FuncDef) [CompUnit]
    log(root);
    if (((token_stream.size() - index) >= 3) && (CUR_TOKEN_IS(VOIDTK) || CUR_TOKEN_IS(INTTK) || CUR_TOKEN_IS(FLOATTK)) && token_stream[index + 1].type == TokenType::IDENFR && token_stream[index + 2].type == TokenType::LPARENT) {
        PARSE(funcdef, FuncDef);
    } else {
        PARSE(decl, Decl);
    }
    if (index < token_stream.size()) {
        PARSE(compunit, CompUnit);
    }
    return true;
}
bool Parser::parseDecl(Decl *root) {
    // Decl -> ConstDecl | VarDecl
    log(root);
    if (CUR_TOKEN_IS(CONSTTK)) {
        PARSE(constdecl, ConstDecl);
    } else {
        PARSE(vardecl, VarDecl);
    }
    return true;
}
bool Parser::parseConstDecl(ConstDecl *root) {
    // ConstDecl -> 'const' BType ConstDef { ',' ConstDef } ';'
    log(root);
    PARSE_TOKEN(CONSTTK);
    PARSE(btype, BType);
    PARSE(constdef, ConstDef);
    while (CUR_TOKEN_IS(COMMA)) {
        PARSE_TOKEN(COMMA);
        PARSE(constdef, ConstDef);
    }
    PARSE_TOKEN(SEMICN);
    return true;
}
bool Parser::parseBType(BType *root) {
    //  BType -> 'int' | 'float'
    log(root);
    if (CUR_TOKEN_IS(INTTK)) {
        PARSE_TOKEN(INTTK);
    } else {
        PARSE_TOKEN(FLOATTK);
    }
    return true;
}
bool Parser::parseConstDef(ConstDef *root) {
    // ConstDef -> Ident { '[' ConstExp ']' } '=' ConstInitVal
    log(root);
    PARSE_TOKEN(IDENFR);
    while (CUR_TOKEN_IS(LBRACK)) {
        PARSE_TOKEN(LBRACK);
        if (CUR_TOKEN_IS(RBRACK)) {
            PARSE_TOKEN(RBRACK);
        } else {
            PARSE(constexp, ConstExp);
            PARSE_TOKEN(RBRACK);
        }
    }
    PARSE_TOKEN(ASSIGN);
    PARSE(constinitval, ConstInitVal);
    return true;
}
bool Parser::parseConstInitVal(ConstInitVal *root) {
    // ConstInitVal -> ConstExp | '{' [ ConstInitVal { ',' ConstInitVal } ] '}'
    log(root);
    if (CUR_TOKEN_IS(LBRACE)) {
        PARSE_TOKEN(LBRACE);
        if (CUR_TOKEN_IS(RBRACE)) {
            PARSE_TOKEN(RBRACE);
        } else {
            PARSE(constinitval, ConstInitVal);
            while (CUR_TOKEN_IS(COMMA)) {
                PARSE_TOKEN(COMMA);
                PARSE(constinitval, ConstInitVal);
            }
            PARSE_TOKEN(RBRACE);
        }
    } else {
        PARSE(constexp, ConstExp);
    }
    return true;
}
bool Parser::parseVarDecl(VarDecl *root) {
    // VarDecl -> BType VarDef { ',' VarDef } ';'
    log(root);
    PARSE(btype, BType);
    PARSE(varDef, VarDef);
    while (CUR_TOKEN_IS(COMMA)) {
        PARSE_TOKEN(COMMA);
        PARSE(varDef, VarDef);
    }
    PARSE_TOKEN(SEMICN);
    return true;
}
bool Parser::parseVarDef(VarDef *root) {
    // VarDef -> Ident { '[' ConstExp ']' } [ '=' InitVal ]
    log(root);
    PARSE_TOKEN(IDENFR);
    while (CUR_TOKEN_IS(LBRACK)) {
        PARSE_TOKEN(LBRACK);
        if (CUR_TOKEN_IS(RBRACK)) {
            PARSE_TOKEN(RBRACK);
        } else {
            PARSE(constexp, ConstExp);
            PARSE_TOKEN(RBRACK);
        }
    }
    if (CUR_TOKEN_IS(ASSIGN)) {
        PARSE_TOKEN(ASSIGN);
        PARSE(initval, InitVal);
    }
    return true;
}
bool Parser::parseInitVal(InitVal *root) {
    // InitVal -> Exp | '{' [ InitVal { ',' InitVal } ] '}'
    log(root);
    if (CUR_TOKEN_IS(LBRACE)) {
        PARSE_TOKEN(LBRACE);
        if (CUR_TOKEN_IS(RBRACE)) {
            PARSE_TOKEN(RBRACE);
        } else {
            PARSE(initval, InitVal);
            while (CUR_TOKEN_IS(COMMA)) {
                PARSE_TOKEN(COMMA);
                PARSE(initval, InitVal);
            }
            PARSE_TOKEN(RBRACE);
        }
    } else {
        PARSE(exp, Exp);
    }

    return true;
}
bool Parser::parseFuncDef(FuncDef *root) {
    // FuncDef -> FuncType Ident '(' [FuncFParams] ')' Block
    log(root);
    PARSE(functype, FuncType);
    PARSE_TOKEN(IDENFR);
    PARSE_TOKEN(LPARENT);
    // no [FuncFParams], FuncType Ident '(' ')' Block
    if (CUR_TOKEN_IS(RPARENT)) {
        PARSE_TOKEN(RPARENT);
    }
    // FuncType Ident '(' FuncFParams ')' Block
    else {
        PARSE(node, FuncFParams);
        PARSE_TOKEN(RPARENT);
    }
    PARSE(block, Block);
    return true;
}
bool Parser::parseFuncType(FuncType *root) {
    // FuncType -> 'void' | 'int' | 'float'
    log(root);
    if (CUR_TOKEN_IS(VOIDTK)) {
        PARSE_TOKEN(VOIDTK);
    } else if (CUR_TOKEN_IS(INTTK)) {
        PARSE_TOKEN(INTTK);
    } else {
        PARSE_TOKEN(FLOATTK);
    }
    return true;
}
bool Parser::parseFuncFParam(FuncFParam *root) {
    // FuncFParam -> BType Ident ['[' ']' { '[' Exp ']' }]
    log(root);
    PARSE(btype, BType);
    PARSE_TOKEN(IDENFR);
    if (CUR_TOKEN_IS(LBRACK)) {
        PARSE_TOKEN(LBRACK);
        PARSE_TOKEN(RBRACK);
        while (CUR_TOKEN_IS(LBRACK)) {
            PARSE_TOKEN(LBRACK);
            if (CUR_TOKEN_IS(RBRACK)) {
                PARSE_TOKEN(RBRACK);
            } else {
                PARSE(exp, Exp);
                PARSE_TOKEN(RBRACK);
            }
        }
    }
    return true;
}
bool Parser::parseFuncFParams(FuncFParams *root) {
    // FuncFParams -> FuncFParam { ',' FuncFParam }
    log(root);
    PARSE(funcfparam, FuncFParam);
    while (CUR_TOKEN_IS(COMMA)) {
        PARSE_TOKEN(COMMA);
        PARSE(funcfparam, FuncFParam);
    }
    return true;
}
bool Parser::parseBlock(Block *root) {
    // Block -> '{' { BlockItem } '}'
    log(root);
    PARSE_TOKEN(LBRACE);
    while (!CUR_TOKEN_IS(RBRACE)) {
        PARSE(blockitem, BlockItem);
    }
    PARSE_TOKEN(RBRACE);
    return true;
}
bool Parser::parseBlockItem(BlockItem *root) {
    /*
    BlockItem -> Decl | Stmt
    1. Decl << 'const', 'int', 'float'
    2. Stmt << Ident, '{', 'if', 'while', 'break', 'continue', 'return', '('
    */
    log(root);
    if (CUR_TOKEN_IS(CONSTTK) || CUR_TOKEN_IS(INTTK) || CUR_TOKEN_IS(FLOATTK)) {
        PARSE(decl, Decl);
    } else {
        PARSE(stmt, Stmt);
    }
    return true;
}
bool Parser::parseStmt(Stmt *root) {
    /*
    Stmt -> LVal '=' Exp ';' | Block | 'if' '(' Cond ')' Stmt [ 'else' Stmt ] | 'while' '(' Cond ')' Stmt | 'break' ';' | 'continue' ';' | 'return' [Exp] ';' | [Exp] ';'
    1. LVal -> Ident {'[' Exp ']'} << Ident
    2. Block -> '{' { BlockItem } '}' << '{'
    3. << 'if'
    4. << 'while'
    5. << 'break'
    6. << 'continue'
    7. << 'return'
    8. << ';'
    8. [Exp] -> AddExp -> MulExp { ('+' | '-') MulExp }
            MulExp -> UnaryExp { ('*' | '/' | '%') UnaryExp }
                UnaryExp -> PrimaryExp | Ident '(' [FuncRParams] ')' | UnaryOp UnaryExp
                    PrimaryExp -> '(' Exp ')' | LVal | Number
                           << '('
                           LVal -> Ident {'[' Exp ']'} << Ident
                           Number -> IntConst | FloatConst << IntConst, FloatConst
                    << Ident + '('
                    UnaryOp -> '+' | '-' | '!' << '+', '-', '!'
    */
    log(root);
    if (CUR_TOKEN_IS(LBRACE)) {
        PARSE(block, Block);
    } else if (CUR_TOKEN_IS(IFTK)) {
        PARSE_TOKEN(IFTK);
        PARSE_TOKEN(LPARENT);
        PARSE(cond, Cond);
        PARSE_TOKEN(RPARENT);
        PARSE(stmt, Stmt);
        if (CUR_TOKEN_IS(ELSETK)) {
            PARSE_TOKEN(ELSETK);
            PARSE(stmt, Stmt);
        }
    } else if (CUR_TOKEN_IS(WHILETK)) {
        PARSE_TOKEN(WHILETK);
        PARSE_TOKEN(LPARENT);
        PARSE(cond, Cond);
        PARSE_TOKEN(RPARENT);
        PARSE(stmt, Stmt);
    } else if (CUR_TOKEN_IS(BREAKTK)) {
        PARSE_TOKEN(BREAKTK);
        PARSE_TOKEN(SEMICN);
    } else if (CUR_TOKEN_IS(CONTINUETK)) {
        PARSE_TOKEN(CONTINUETK);
        PARSE_TOKEN(SEMICN);
    } else if (CUR_TOKEN_IS(RETURNTK)) {
        PARSE_TOKEN(RETURNTK);
        if (CUR_TOKEN_IS(SEMICN)) {
            PARSE_TOKEN(SEMICN);
        } else {
            PARSE(exp, Exp);
            PARSE_TOKEN(SEMICN);
        }
    } else {
        if (CUR_TOKEN_IS(PLUS) || CUR_TOKEN_IS(MINU) || CUR_TOKEN_IS(NOT) || CUR_TOKEN_IS(INTLTR) || CUR_TOKEN_IS(FLOATLTR) || CUR_TOKEN_IS(LPARENT) || CUR_TOKEN_IS(SEMICN) || ((token_stream.size() - index) >= 2 && token_stream[index].type == TokenType::IDENFR && token_stream[index + 1].type == TokenType::LPARENT)) {
            if (CUR_TOKEN_IS(SEMICN)) {
                PARSE_TOKEN(SEMICN);
            } else {
                PARSE(exp, Exp);
                PARSE_TOKEN(SEMICN);
            }
        } else {
            PARSE(lval, LVal);
            if (CUR_TOKEN_IS(ASSIGN)) {
                PARSE_TOKEN(ASSIGN);
                PARSE(exp, Exp);
                PARSE_TOKEN(SEMICN);
            } else {
                // Exp ';' -> LVal { ('*' | '/' | '%') UnaryExp } { ('+' | '-') MulExp } ';'
                while (CUR_TOKEN_IS(MULT) || CUR_TOKEN_IS(DIV) || CUR_TOKEN_IS(MOD)) {
                    if (CUR_TOKEN_IS(MULT)) {
                        PARSE_TOKEN(MULT);
                    } else if (CUR_TOKEN_IS(DIV)) {
                        PARSE_TOKEN(DIV);
                    } else {
                        PARSE_TOKEN(MOD);
                    }
                    PARSE(unaryexp, UnaryExp);
                }

                while (CUR_TOKEN_IS(PLUS) || CUR_TOKEN_IS(MINU)) {
                    if (CUR_TOKEN_IS(PLUS)) {
                        PARSE_TOKEN(PLUS);
                    } else {
                        PARSE_TOKEN(MINU);
                    }
                    PARSE(unaryexp, UnaryExp);
                }

                PARSE_TOKEN(SEMICN);
            }
        }
    }

    return true;
}
bool Parser::parseExp(Exp *root) {
    // Exp -> AddExp
    log(root);
    PARSE(addexp, AddExp);
    return true;
}
bool Parser::parseCond(Cond *root) {
    // Cond -> LOrExp
    log(root);
    PARSE(lorexp, LOrExp);
    return true;
}
bool Parser::parseLVal(LVal *root) {
    // LVal -> Ident {'[' Exp ']'}
    log(root);
    PARSE_TOKEN(IDENFR);
    while (CUR_TOKEN_IS(LBRACK)) {
        PARSE_TOKEN(LBRACK);
        PARSE(exp, Exp);
        PARSE_TOKEN(RBRACK);
    }
    return true;
}
bool Parser::parseNumber(Number *root) {
    // Number -> IntConst | FloatConst
    log(root);
    if (CUR_TOKEN_IS(INTLTR)) {
        PARSE_TOKEN(INTLTR);
    } else {
        PARSE_TOKEN(FLOATLTR);
    }
    return true;
}
bool Parser::parsePrimaryExp(PrimaryExp *root) {
    log(root);
    /*
    PrimaryExp -> '(' Exp ')' | LVal | Number
    1. << '('
    2. LVal -> Ident {'[' Exp ']'} << Ident
    3. Number -> IntConst | FloatConst << IntConst, FloatConst
    */
    if (CUR_TOKEN_IS(LPARENT)) {
        PARSE_TOKEN(LPARENT);
        PARSE(exp, Exp);
        PARSE_TOKEN(RPARENT);
    } else if (CUR_TOKEN_IS(IDENFR)) {
        PARSE(lval, LVal);
    } else {
        PARSE(number, Number);
    }
    return true;
}
bool Parser::parseUnaryExp(UnaryExp *root) {
    /*
    UnaryExp -> PrimaryExp | Ident '(' [FuncRParams] ')' | UnaryOp UnaryExp
    1. PrimaryExp -> '(' Exp ')' | LVal | Number
        << '('
        LVal -> Ident {'[' Exp ']'} << Ident
        Number -> IntConst | FloatConst << IntConst, FloatConst
    2. << Ident + '('
    3. UnaryOp -> '+' | '-' | '!' << '+', '-', '!'
    */
    log(root);
    if (CUR_TOKEN_IS(PLUS) || CUR_TOKEN_IS(MINU) || CUR_TOKEN_IS(NOT)) {
        PARSE(unaryop, UnaryOp);
        PARSE(unaryexp, UnaryExp);
    } else if ((token_stream.size() - index) >= 2 && token_stream[index].type == TokenType::IDENFR && token_stream[index + 1].type == TokenType::LPARENT) {
        PARSE_TOKEN(IDENFR);
        PARSE_TOKEN(LPARENT);
        if (CUR_TOKEN_IS(RPARENT)) {
            PARSE_TOKEN(RPARENT);
        } else {
            PARSE(funcRParams, FuncRParams);
            PARSE_TOKEN(RPARENT);
        }
    } else {
        PARSE(primaryexp, PrimaryExp);
    }

    return true;
}
bool Parser::parseUnaryOp(UnaryOp *root) {
    // UnaryOp -> '+' | '-' | '!'
    log(root);
    if (CUR_TOKEN_IS(PLUS)) {
        PARSE_TOKEN(PLUS);
    } else if (CUR_TOKEN_IS(MINU)) {
        PARSE_TOKEN(MINU);
    } else {
        PARSE_TOKEN(NOT);
    }
    return true;
}
bool Parser::parseFuncRParams(FuncRParams *root) {
    // FuncRParams -> Exp { ',' Exp }
    log(root);
    PARSE(exp, Exp);
    while (CUR_TOKEN_IS(COMMA)) {
        PARSE_TOKEN(COMMA);
        PARSE(exp, Exp);
    }
    return true;
}
bool Parser::parseMulExp(MulExp *root) {
    // MulExp -> UnaryExp { ('*' | '/' | '%') UnaryExp }
    log(root);
    PARSE(unaryexp, UnaryExp);
    while (CUR_TOKEN_IS(MULT) || CUR_TOKEN_IS(DIV) || CUR_TOKEN_IS(MOD)) {
        if (CUR_TOKEN_IS(MULT)) {
            PARSE_TOKEN(MULT);
        } else if (CUR_TOKEN_IS(DIV)) {
            PARSE_TOKEN(DIV);
        } else {
            PARSE_TOKEN(MOD);
        }
        PARSE(unaryexp, UnaryExp);
    }
    return true;
}
bool Parser::parseAddExp(AddExp *root) {
    // AddExp -> MulExp { ('+' | '-') MulExp }
    log(root);
    PARSE(mulExp, MulExp);
    while (CUR_TOKEN_IS(PLUS) || CUR_TOKEN_IS(MINU)) {
        if (CUR_TOKEN_IS(PLUS)) {
            PARSE_TOKEN(PLUS);
        } else {
            PARSE_TOKEN(MINU);
        }
        PARSE(mulExp, MulExp);
    }

    return true;
}
bool Parser::parseRelExp(RelExp *root) {
    // RelExp -> AddExp { ('<' | '>' | '<=' | '>=') AddExp }
    log(root);
    PARSE(addexp, AddExp);
    while (CUR_TOKEN_IS(LSS) || CUR_TOKEN_IS(GTR) || CUR_TOKEN_IS(LEQ) || CUR_TOKEN_IS(GEQ)) {
        if (CUR_TOKEN_IS(LSS)) {
            PARSE_TOKEN(LSS);
        } else if (CUR_TOKEN_IS(GTR)) {
            PARSE_TOKEN(GTR);
        } else if (CUR_TOKEN_IS(LEQ)) {
            PARSE_TOKEN(LEQ);
        } else {
            PARSE_TOKEN(GEQ);
        }
        PARSE(addexp, AddExp);
    }

    return true;
}
bool Parser::parseEqExp(EqExp *root) {
    // EqExp -> RelExp { ('==' | '!=') RelExp }
    log(root);
    PARSE(relExp, RelExp);
    while (CUR_TOKEN_IS(EQL) || CUR_TOKEN_IS(NEQ)) {
        if (CUR_TOKEN_IS(EQL)) {
            PARSE_TOKEN(EQL);
        } else {
            PARSE_TOKEN(NEQ);
        }
        PARSE(relExp, RelExp);
    }
    return true;
}
bool Parser::parseLAndExp(LAndExp *root) {
    // LAndExp -> EqExp [ '&&' LAndExp ]
    log(root);
    PARSE(eqexp, EqExp);
    if (CUR_TOKEN_IS(AND)) {
        PARSE_TOKEN(AND);
        PARSE(lAndExp, LAndExp);
    }
    return true;
}
bool Parser::parseLOrExp(LOrExp *root) {
    // LOrExp -> LAndExp [ '||' LOrExp ]
    log(root);
    PARSE(lAndExp, LAndExp);
    if (CUR_TOKEN_IS(OR)) {
        PARSE_TOKEN(OR);
        PARSE(lOrExp, LOrExp);
    }

    return true;
}
bool Parser::parseConstExp(ConstExp *root) {
    // ConstExp -> AddExp
    log(root);
    PARSE(addexp, AddExp);

    return true;
}

frontend::CompUnit *Parser::get_abstract_syntax_tree() {
    auto root = new CompUnit();
    parseCompUnit(root);
    return root;
}

void Parser::log(AstNode *node) {
#ifdef DEBUG_PARSER
    std::cout << "in parse" << toString(node->type) << ", cur_token_type::" << toString(token_stream[index].type) << ", token_val::" << token_stream[index].value << '\n';
#endif
}
