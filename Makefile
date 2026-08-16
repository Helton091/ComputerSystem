CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -g
LDFLAGS  :=

# ===== 目标：三个独立可执行文件（输出在根目录）=====
TARGETS := simulator.exe assembler.exe compiler.exe

.PHONY: all clean
all: $(TARGETS)

# ===== 模拟器 =====
simulator.exe: src/Computer.cpp src/Computer.hpp
	$(CXX) $(CXXFLAGS) $< -o $@

# ===== 汇编器 =====
assembler.exe: src/assembler.cpp src/assembler.hpp src/assembler_macro.hpp
	$(CXX) $(CXXFLAGS) $< -o $@

# ===== 编译器（多文件编译）=====
compiler.exe: src/main.cpp src/compiler.cpp src/compiler.hpp \
              src/lexer.cpp src/lexer.hpp \
              src/parser.cpp src/parser.hpp \
              src/IR.cpp src/IR.hpp \
              src/AST2IR.cpp src/AST2IR.hpp \
              src/IR2RISCV.cpp src/IR2RISCV.hpp \
              src/ast.hpp src/token.hpp
	$(CXX) $(CXXFLAGS) src/main.cpp src/compiler.cpp src/lexer.cpp src/parser.cpp src/IR.cpp src/AST2IR.cpp src/IR2RISCV.cpp -o $@

# ===== 清理 =====
clean:
	cmd /c "del /Q *.o $(TARGETS) 2>nul"
