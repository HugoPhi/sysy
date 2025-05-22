# Sys-y

## 一、语言概述
Sys-y是专为编译原理课程设计的教学型编程语言，具备以下核心特性：
- **C风格语法**：类C语法结构便于学生迁移知识
- **分层文法**：模块化文法设计支持分阶段实现编译器
- **教学友好**：支持常量传播、控制流分析等典型编译优化
- **类型安全**：显式类型系统支持基础类型检查
- **多维数组**：支持编译期确定维度的数组类型
- **多进制支持**：二进制/八进制/十进制/十六进制字面量

## 二、核心语言特性
### 1. 词法规范
```txt
/* 多进制字面量 */
/* 基础元素 */
WHITESPACE   = \s+                    # 空白字符
IDENTIFIER   = [a-zA-Z_][a-zA-Z0-9_]* # 标识符

/* 关键字 */
KEYWORD      = \b(int|float|void|const|if|else|while|return|break|continue)\b

/* 字面量 */
INT_LITERAL  = 0[bB][01]+             # 二进制
            | 0[0-7]+                 # 八进制
            | 0[xX][0-9a-fA-F]+       # 十六进制
            | [1-9][0-9]*|0           # 十进制

FLOAT_LITERAL= [0-9]+\.[0-9]*([eE][+-]?[0-9]+?)  # 浮点数

/* 运算符 */
OPERATOR     = [+\-*/%]               # 算术
            | [=!<>]=?                # 比较
            | &&|\|\|                 # 逻辑
            | !                       # 单目

/* 分隔符 */
DELIMITER    = [;,(){}[\]]            # 标点符号

/* 注释 */
COMMENT      = //.*                   # 单行注释
            | /\*.*?\*/               # 多行注释
```

### 2. CFG文法
```bnf
// 编译单元结构
<CompUnit> ::= ( <Decl> | <FuncDef> ) [ <CompUnit> ]

// 声明系统
<Decl> ::= <ConstDecl> | <VarDecl>
<ConstDecl> ::= "const" <BType> <ConstDef> { "," <ConstDef> } ";"
<VarDecl> ::= <BType> <VarDef> { "," <VarDef> } ";"

// 函数定义
<FuncDef> ::= <FuncType> Ident "(" [ <FuncFParams> ] ")" <Block>
<FuncFParam> ::= <BType> Ident [ "[" "]" { "[" Exp "]" } ]  // 支持多维数组参数

// 控制结构
<Stmt> ::= <LVal> "=" <Exp> ";"         // 赋值语句
         | "if" "(" <Cond> ")" <Stmt> [ "else" <Stmt> ]  // 条件分支
         | "while" "(" <Cond> ")" <Stmt>  // 循环结构

// 表达式体系
<Exp> ::= <AddExp>
<Cond> ::= <LOrExp>                    // 条件表达式
<LVal> ::= Ident { "[" <Exp> "]" }     // 左值可带下标

// 类型系统
<BType> ::= "int" | "float"
<FuncType> ::= "void" | <BType>
```

### 3. 运算符优先级（从高到低）
| 优先级 | 运算符                          | 结合性   |
|--------|---------------------------------|----------|
| 1      | `() []`                         | 左结合   |
| 2      | `! + -` (一元)                  | 右结合   |
| 3      | `* / %`                         | 左结合   |
| 4      | `+ -` (二元)                    | 左结合   |
| 5      | `< <= > >=`                     | 左结合   |
| 6      | `== !=`                         | 左结合   |
| 7      | `&&`                            | 左结合   |
| 8      | `||`                            | 左结合   |

## 三、语义约束
1. **常量传播**：
   - `ConstExp`必须能在编译期求值
   - 数组维度声明仅接受常量表达式
   ```sysy
   const int N = 5;
   int arr[N][2*N];  // 合法
   int x = 3;
   float arr2[x];     // 非法（x非常量）
   ```

2. **类型兼容性**：
   - 赋值语句左右值类型必须严格匹配
   - 函数返回值类型与声明一致
   ```sysy
   float foo() {
       int a = 3;
       return a;      // 合法（隐式类型转换）
   }
   ```

3. **控制流约束**：
   - `break/continue`必须位于循环体内
   - 函数返回值类型为void时不能带返回表达式
   ```sysy
   void func() {
       return 5;      // 非法
   }
   ```

## 四、典型代码示例
```sysy
// 快速排序分区函数
int partition(int[] arr, int low, int high) {
    const int pivot = arr[high];
    int i = low - 1;
    
    for (int j = low; j < high; j = j + 1) {
        if (arr[j] <= pivot) {
            i = i + 1;
            // 交换元素
            int temp = arr[i];
            arr[i] = arr[j];
            arr[j] = temp;
        }
    }
    // 最终交换
    int temp = arr[i+1];
    arr[i+1] = arr[high];
    arr[high] = temp;
    return i + 1;
}
```

## 五、编译器实现建议
1. **阶段分解**：
   - 词法分析：处理多进制和嵌套注释
   - 语法分析：构建带类型标注的AST
   - 语义分析：实施常量传播和类型检查
   - 中间代码：生成类LLVM IR或三地址码

2. **难点解析**：
   - 数组维度推导：处理`int[][3]`类声明
   - 短路求值：`&&`和`||`的逻辑优化
   - 隐式类型转换：int到float的自动提升
