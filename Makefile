CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -g
LDFLAGS  :=

# ===== 目标：两个独立可执行文件 =====
TARGETS := simulator.exe assembler.exe

.PHONY: all clean
all: $(TARGETS)

# ===== 模拟器（Computer.cpp 自带 main）=====
simulator.exe: Computer.cpp Computer.hpp
	$(CXX) $(CXXFLAGS) $< -o $@

# ===== 汇编器（assembler.cpp 自带 main）=====
assembler.exe: assembler.cpp assembler.hpp assembler_macro.hpp
	$(CXX) $(CXXFLAGS) $< -o $@

# ===== 清理 =====
clean:
	cmd /c "del /Q *.o $(TARGETS) 2>nul"
