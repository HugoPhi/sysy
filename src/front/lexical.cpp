#include"front/lexical.h"

#include<map>
#include<cassert>
#include<string>

#define TODO assert(0 && "todo")

std::string frontend::toString(State s) {
    switch (s) {
        case State::Empty: return "Empty";
        case State::Ident: return "Ident";
        case State::IntLiteral: return "IntLiteral";
        case State::FloatLiteral: return "FloatLiteral";
        case State::op: return "op";
        default: 
            assert(0 && "invalid State");
    }
    return "";
}

std::set<std::string> frontend::keywords= {
    "const", "int", "float", "if", "else", "while", "continue", "break", "return", "void"
};

/**
 * Assist Functions Impl
 */
frontend::TokenType frontend::assist::get_keyword_type(const std::string& str) {
    // State: Ident -> if not 'const', 'int', 'float', ... keywords predefined, else your own defined signals such as 'a0, a1, ...'
    static const std::map<std::string, TokenType> keyword_map = {
        {"const", TokenType::CONSTTK},
        {"int", TokenType::INTTK},
        {"float", TokenType::FLOATTK},
        {"if", TokenType::IFTK},
        {"else", TokenType::ELSETK},
        {"while", TokenType::WHILETK},
        {"continue", TokenType::CONTINUETK},
        {"break", TokenType::BREAKTK},
        {"return", TokenType::RETURNTK},
        {"void", TokenType::VOIDTK}
    };
    auto it = keyword_map.find(str);
    return it != keyword_map.end() ? it->second : TokenType::IDENFR;
}

frontend::TokenType frontend::assist::get_op_type(const std::string& str) {
    // State: op -> '+', '-', ...
    static const std::map<std::string, TokenType> op_map = {
        {"+", TokenType::PLUS},
        {"-", TokenType::MINU},
        {"*", TokenType::MULT},
        {"/", TokenType::DIV},
        {"%", TokenType::MOD},
        {"<", TokenType::LSS},
        {"<=", TokenType::LEQ},
        {">", TokenType::GTR},
        {">=", TokenType::GEQ},
        {"==", TokenType::EQL},
        {"!=", TokenType::NEQ},
        {"=", TokenType::ASSIGN},
        {"!", TokenType::NOT},
        {"&&", TokenType::AND},
        {"||", TokenType::OR},
        {";", TokenType::SEMICN},
        {",", TokenType::COMMA},
        {"(", TokenType::LPARENT},
        {")", TokenType::RPARENT},
        {"[", TokenType::LBRACK},
        {"]", TokenType::RBRACK},
        {"{", TokenType::LBRACE},
        {"}", TokenType::RBRACE}
    };
    auto it = op_map.find(str);
    // return it != op_map.end() ? it->second : TokenType::UNKNOWN;
    return it->second;
}

bool frontend::assist::is_double_char_op(const std::string& s) {
    static const std::set<std::string> double_ops = {
        "==", "!=", "<=", ">=", "&&", "||"
    };
    return double_ops.count(s);
}

/**
 * DFA Impl
 */
frontend::DFA::DFA(): cur_state(frontend::State::Empty), cur_str() {}

frontend::DFA::~DFA() {}

bool frontend::DFA::next(char input, Token& buf) {
#ifdef DEBUG_DFA
    std::cout << "in state [" << toString(cur_state) << "], input = \'" << input 
              << "\', str = " << cur_str << "\tlookahead=" << (lookahead?lookahead:' ') << std::endl;
#endif

    // 处理缓存的超前字符
    if(lookahead != 0) {
        char temp = lookahead;
        lookahead = 0;
        bool ret = next(temp, buf);
        if(ret) { // 如果生成token，缓存当前input
            lookahead = input;
#ifdef DEBUG_DFA
            std::cout << "[Lookahand cached] '" << input << "'" << std::endl;
#endif
        }
        return ret;
    }

    bool token_generated = false;
    
    switch(cur_state) {
        case State::Empty:
            if(isalpha(input) || input == '_') {
                cur_state = State::Ident;
                cur_str += input;
            } else if(isdigit(input)) {
                cur_state = State::IntLiteral;
                cur_str += input;
            } else if(input == '.') {
                cur_state = State::FloatLiteral;
                cur_str += input;
            } else if(!isspace(input)) {
                cur_state = State::op;
                cur_str += input;
            }
            break;

        case State::Ident:
            if(!isalnum(input) && input != '_') {
                token_generated = true;
                lookahead = input; // 回退字符
            } else {
                cur_str += input;
            }
            break;

        case State::IntLiteral:
            if(input == '.') {
                cur_state = State::FloatLiteral;
                cur_str += input;
            } else if(!isdigit(input) && input != 'x' && input != 'b' && input != 'a' && input != 'c' && input != 'd' && input != 'e' && input != 'f') {
                token_generated = true;
                lookahead = input;
            } else {
                cur_str += input;
            }
            break;

        case State::FloatLiteral:
            if(!isdigit(input)) {
                token_generated = true;
                lookahead = input;
            } else {
                cur_str += input;
            }
            break;

        case State::op:
            // token_generated = true;
            // lookahead = input;
            std::string possible_op = cur_str + input;
            if (frontend::assist::is_double_char_op(possible_op)) {
                // 组成有效双字符操作符
                cur_str = possible_op;
                token_generated = true;
            } else {
                // 无法组成双字符操作符，生成单字符Token并缓存当前输入
                lookahead = input;
                token_generated = true;
            }
            break;
    }

    // 生成token逻辑
    if(token_generated) {
        buf = create_token();
        reset();
#ifdef DEBUG_DFA
        std::cout << ">> Token generated: " << toString(buf.type) << " -> " << buf.value << std::endl;
#endif
        return true;
    }
    
    return false;
}

/**
 * Generate token from cur_state & cur_str
 */
frontend::Token frontend::DFA::create_token() const {
    Token tk;
    tk.value = cur_str;

    switch(cur_state) {
        case State::Ident:
            tk.type = frontend::keywords.count(cur_str) ? frontend::assist::get_keyword_type(cur_str) : frontend::TokenType::IDENFR;
            break;
        case State::IntLiteral:
            tk.type = TokenType::INTLTR;
            break;
        case State::FloatLiteral:
            tk.type = TokenType::FLOATLTR;
            break;
        case State::op:
            tk.type = frontend::assist::get_op_type(cur_str);
            break;
        default:
            assert(false && "Invalid token creation state");
    }
    return tk;
}


void frontend::DFA::reset() {
    cur_state = State::Empty;
    cur_str = "";
}

bool frontend::DFA::flush(Token& buf) {
    if (cur_str.empty()) return false;
    buf = create_token();
    reset();
    return true;
}

/**
 * Scanner Impl
 */
enum class FilterState {  // Filter out comment, Yunming@2025.5.1
    NORMAL,
    BLOCK_COMMENT,
    LINE_COMMENT,
    MAYBE_END_BLOCK
};

std::string filter_comments(std::istream& input) {  // Filter out comment, Yunming@2025.5.1
    std::string filtered;
    FilterState state = FilterState::NORMAL;
    char c;

    while(input.get(c)) {
        switch(state) {
            case FilterState::NORMAL:
                if(c == '/') {
                    char next = input.peek();
                    if(next == '*') {
                        state = FilterState::BLOCK_COMMENT;
                        input.get(); // 跳过'*'
                    } else if(next == '/') {
                        state = FilterState::LINE_COMMENT;
                        input.get(); // 跳过第二个'/'
                    } else {
                        filtered += c;
                    }
                } else {
                    filtered += c;
                }
                break;

            case FilterState::BLOCK_COMMENT:
                if(c == '*') {
                    state = FilterState::MAYBE_END_BLOCK;
                }
                break;

            case FilterState::MAYBE_END_BLOCK:
                if(c == '/') {
                    state = FilterState::NORMAL;
                } else {
                    state = FilterState::BLOCK_COMMENT;
                }
                break;

            case FilterState::LINE_COMMENT:
                if(c == '\n') {
                    filtered += c;  // 保留换行符
                    state = FilterState::NORMAL;
                }
                break;
        }
    }

    return filtered;
}


frontend::Scanner::Scanner(std::string filename): fin(filename) {
    if(!fin.is_open()) {
        assert(0 && "in Scanner constructor, input file cannot open");
    }
}

frontend::Scanner::~Scanner() {
    fin.close();
}

std::vector<frontend::Token> frontend::Scanner::run() {
    std::vector<Token> tokens;
    DFA dfa;
    size_t pos = 0;

    // 先过滤注释
    std::string filtered_source = filter_comments(fin);
    
    while(pos < filtered_source.size()) {
        char c = filtered_source[pos++];
        
#ifdef DEBUG_SCANNER
        std::cout << "\nProcessing char: '" << c << "' (0x" 
                  << std::hex << (int)c << ")" << std::endl;
#endif

        Token tk;
        while(true) {
            if(dfa.next(c, tk)) {
                tokens.push_back(tk);
#ifdef DEBUG_SCANNER
                std::cout << ">> Accepted token: " << toString(tk.type)
                          << " -> " << tk.value << std::endl;
#endif
                c = dfa.lookahead; // 获取回退字符
                dfa.lookahead = 0;
                if(c == 0) break;
            } else {
                break;
            }
        }
    }

    // 处理文件末尾残留
    Token tk;
    if(dfa.flush(tk)) {
        tokens.push_back(tk);
#ifdef DEBUG_SCANNER
        std::cout << ">> Flushing final token: " << toString(tk.type)
                  << " -> " << tk.value << std::endl;
#endif
    }

    return tokens;
}