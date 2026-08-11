CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -g
LDFLAGS  :=

# ===== 目标：三个独立可执行文件 =====
TARGETS := simulator.exe assembler.exe compiler.exe

.PHONY: all clean
all: $(TARGETS)

# ===== 模拟器（Computer.cpp 自带 main）=====
simulator.exe: Computer.cpp Computer.hpp
	$(CXX) $(CXXFLAGS) $< -o $@

# ===== 汇编器（assembler.cpp 自带 main）=====
assembler.exe: assembler.cpp assembler.hpp assembler_macro.hpp
	$(CXX) $(CXXFLAGS) $< -o $@

# ===== 编译器（多文件编译）=====
compiler.exe: main.cpp compiler.cpp compiler.hpp \
              lexer.cpp lexer.hpp \
              parser.cpp parser.hpp \
              codegen.cpp codegen.hpp \
              ir.cpp ir.hpp \
              ast.hpp token.hpp
	$(CXX) $(CXXFLAGS) main.cpp compiler.cpp lexer.cpp parser.cpp codegen.cpp ir.cpp -o $@

# ===== 清理 =====
clean:
	cmd /c "del /Q *.o $(TARGETS) 2>nul"
