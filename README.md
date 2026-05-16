# C-- Compiler Frontend

这是编译原理课程的大作业工程实现。工程目标是完成一个可运行、可解释、可验证、可展示的 C-- 编译器前端：

- 手写自动机构造式词法器：NFA、DFA、最小化 DFA。
- 自动构造 SLR 分析器：FIRST/FOLLOW、LR(0) 项目集、ACTION/GOTO 表。
- SLR 归约时构造 AST。
- 语义分析：作用域、重定义、未定义、const 赋值、return 检查。
- 通过 `compiler_ir` 中端库构造 Module/Function/BasicBlock 并输出 LLVM IR。
- 提供本地 Web UI 展示词法、语法、AST、语义、IR 和自动机构造结果。

## Build

首次克隆仓库后先拉取中端库 submodule：

```bash
git submodule update --init --recursive
```

```bash
make
```

也可以使用 CMake：

```bash
cmake -S . -B build/cmake
cmake --build build/cmake
cmake --build build/cmake --target run_tests
```

Makefile 中也封装了等价入口：

```bash
make cmake-build
make cmake-test
```

## Run

```bash
./cmmc --lex tests/valid/basic.sy
./cmmc --symtab tests/valid/basic.sy
./cmmc --parse tests/valid/basic.sy
./cmmc --ast tests/valid/if_else.sy
./cmmc --semantic tests/invalid/semantic.sy
./cmmc --ir tests/ir/simple.sy -o simple.ll
./cmmc --run-tests
```

## Debug Dumps

```bash
./cmmc --dump-dfa tests/valid/basic.sy
./cmmc --dump-first-follow tests/valid/basic.sy
./cmmc --dump-lr0 tests/valid/basic.sy
./cmmc --dump-slr tests/valid/basic.sy
./cmmc --dump-all tests/valid/functions.sy -o build/out
make demo
```

`make demo` 会把展示材料输出到 `build/out/`，包括：

- `*.tokens`
- `*.symtab`
- `*.parse`
- `*.ast`
- `*.ast.dot`
- `*.dfa.csv`
- `*.first`
- `*.follow`
- `*.lr0`
- `*.slr.csv`
- `*.semantic.log`
- `*.ll`

## Test

```bash
make test
```

当前测试覆盖：

- 词法：关键字大小写、注释、浮点数、非法字符恢复。
- 语法：变量/常量、函数、参数、表达式、if/else、dangling else、规约 trace。
- 语义：未定义变量、const 赋值、函数实参数量错误、嵌套函数调用、return 类型错误。
- IR：全局/局部变量、const、全局常量表达式、表达式、if/else、函数参数与函数调用。
- 浮点 fallback：全局 float 变量、float const、局部 const、实参自动转换、混合表达式。
- LLVM 校验：如果本机存在 `clang`，测试脚本会对生成的 `.ll` 执行 `clang -c -x ir`。

## compiler_ir 说明

工程通过 Git submodule 接入 `external/compiler_ir`。首次克隆仓库后请初始化依赖：

```bash
git submodule update --init --recursive
```

现在工程同时支持 Makefile 和 CMake 两套构建方式：

- Makefile：直接编译 `external/compiler_ir/src/*.cpp` 并链接进 `cmmc`。
- CMake：在本工程内构建 `compiler_ir` 静态库，再链接到 `cmmc`。

`src/ir.cpp` 保留了清晰的 IR 后端适配层：

- `int/void` 必做子集使用 `compiler_ir` 的 `Module / Function / BasicBlock / IRBuilder / GlobalVariable / ConstantInt` 真实构造 IR。
- `float` 扩展示例自动回退到文本后端，因为给定中端库没有完整浮点常量和浮点二元指令封装。
- 两条路径共享同一个 AST 和语义分析结果，对外命令保持一致。

文法实现以实验手册附录为基础，并做了必要的工程化改写：使用 `matchedStmt / unmatchedStmt` 消除 dangling else；为展示扩展能力，额外支持 `float` 函数返回值和普通表达式中的比较/逻辑运算。默认必做的 `int/void` 子集仍走 `compiler_ir` 主路径。

## Web UI

```bash
make ui
```

启动后访问 `http://127.0.0.1:8008`。页面支持编辑 C-- 源码，一键运行 `--dump-all`，并用标签页查看 Tokens、符号表、规约过程、AST、语义日志、LLVM IR、DFA、FIRST/FOLLOW、LR(0)、SLR 表。
