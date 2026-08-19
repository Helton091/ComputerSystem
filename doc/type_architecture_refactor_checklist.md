# 类型架构重构 Checklist

> 目标：拆掉当前代码里到处硬编码的 `int`，让编译器从“默认所有值都是 i32”进化到“每个值都知道自己的类型，后端按类型分发”。
> 这次重构不引入任何新类型（不做 void/float/指针），只改结构；完成后，后续加入 void、float、指针、数组才会干净可控。

---

## 0. 重构原则

- **不引入新功能**：重构期间语法、语义、输出不变，所有现有测试必须继续通过。
- **显式传递类型**：每个 `Value` / AST 节点的类型必须由上层显式给出，禁止默认值导致隐式 i32。
- **类型相等用指针比较**：IR 类型继续是全局单例，`IntType::get() == IntType::get()`。
- **后端按类型分发**：IR2RISCV 里所有 load/store/算术/比较都要先查类型，再走对应指令序列。
- **fail-fast**：类型不匹配的指令在 `make_inst` 构建期直接抛异常，不要到后端才暴露。

---

## 1. AST 类型系统改造

### 1.1 文件：`src/ast.hpp`

- [ ] 给 `Type` 基类增加通用查询接口（先只放占位，具体由子类实现）：
  ```cpp
  virtual bool is_integer() const { return false; }
  virtual bool is_void() const { return false; }
  virtual bool is_pointer() const { return false; }
  virtual bool is_array() const { return false; }
  virtual bool is_float() const { return false; }
  ```
- [ ] `IntType` 覆盖 `is_integer()` 返回 `true`。
- [ ] 把 `Type::to_string()` 改为 `const` 方法（与 IR 的 `Type` 保持一致）。
- [ ] 检查所有子类 `to_string()` 是否都加了 `const`。
- [ ] 把 `Param::type` 从 `std::unique_ptr<Type>` 保持现状（类型仍由 AST 拥有），但确保 parser 里不再直接 `new IntType` 硬编码。

### 1.2 文件：`src/parser.cpp`

- [ ] 统一类型解析入口：新增私有方法 `std::unique_ptr<Type> parse_type()`。
  ```cpp
  std::unique_ptr<Type> Parser::parse_type() {
      if (match(Tok::KW_INT)) return std::make_unique<IntType>();
      // 后续扩展：void、float 都在这里加分支
      throw std::runtime_error("expected type");
  }
  ```
- [ ] `parse_declaration_statement()` 中 `int` 关键字的匹配改为调用 `parse_type()`，但**当前仍只接受 int**。
- [ ] `parse_parameters()` 中 `expect(Tok::KW_INT, ...)` 改为 `parse_type()`，当前仍只接受 int。
- [ ] `parse_function()` 中函数返回类型写死 `std::make_unique<IntType>()` 的地方，也要改为 `parse_type()`（当前仍只接受 int）。
- [ ] 这一步的关键不是支持新类型，而是**让“类型从哪里来”集中到一个函数**，以后只改这里。

---

## 2. IR 类型系统改造

### 2.1 文件：`src/IR.hpp`

- [ ] 给 `Type` 基类增加与 AST 对称的查询接口：
  ```cpp
  virtual bool is_integer() const { return false; }
  virtual bool is_void() const { return false; }
  virtual bool is_pointer() const { return false; }
  virtual bool is_array() const { return false; }
  virtual bool is_float() const { return false; }
  ```
- [ ] `IntType::is_integer()` 返回 `true`。
- [ ] `VoidType::is_void()` 返回 `true`。
- [ ] 给 `Type` 增加 `virtual int align() const = 0;`（目前 int 4 字节、4 字节对齐）。
- [ ] `ConstantInt` 保持现状，但心里明确它是“带 int 类型的常量”；后续如果要加 `ConstantFloat`，会再抽象一层 `Constant`。
- [ ] `Argument` 构造函数不要默认 `IntType::get()`，改为由调用方传入 `Type*`。

### 2.2 文件：`src/IR.cpp`

- [ ] `Function::add_arg()` 签名改为 `Argument* add_arg(Type* type)`，内部用传入的类型构造 `Argument`。
- [ ] `Module::get_const(int v)` 保持返回 `ConstantInt*`；后续 float 常量会新增 `get_const(float)` 重载或新函数。
- [ ] `make_inst` 中所有类型检查从“硬编码 i32”改为“按操作语义检查”：
  - 算术指令（ADD/SUB/MUL/DIV/REM）：操作数类型相同，结果类型与操作数相同；当前限定为 integer-like（`is_integer()`）。
  - 比较指令（LT/GT/LE/GE/EQ/NE）：操作数类型相同，结果类型必须是 `IntType::get()`（比较结果是 0/1 整数）。
  - NEG：一个操作数，结果类型与操作数相同。
  - LOAD：地址类型未来可能是指针，但目前仍限定为 ALLOCA；结果类型与 ALLOCA 指向类型相同。
  - STORE：被存值类型与地址指向类型相同。
  - CALL：返回值类型与被调函数声明的返回类型相同（目前所有函数仍返回 i32）。
- [ ] 检查 `make_inst` 的 `default: throw std::runtime_error("unknown op");` 之前，确保每个 opcode 都有 `break`。

---

## 3. AST → IR 翻译层改造

### 3.1 文件：`src/AST2IR.hpp`

- [ ] 增加类型映射工具函数声明：
  ```cpp
  Type* to_ir_type(const ::Type* ast_type);
  ```

### 3.2 文件：`src/AST2IR.cpp`

- [ ] 实现 `to_ir_type`：
  ```cpp
  Type* AST2IR::to_ir_type(const ::Type* ast_type) {
      if (ast_type->is_integer()) return IntType::get();
      // 后续扩展：void、float、pointer
      throw std::runtime_error("unsupported AST type: " + ast_type->to_string());
  }
  ```
- [ ] `translate()` 中第一遍建函数时，为每个参数调用 `func->add_arg(to_ir_type(param.type.get()))`。
- [ ] `gen_function()` 中形参的 `ALLOCA` 类型，使用 `to_ir_type` 映射后的类型（目前仍是 i32）。
- [ ] `gen_function()` 中给形参 store 时，确保 STORE 操作数类型一致。
- [ ] `gen_stmt()` 中 `DeclStmt` 的 `ALLOCA` 类型使用 `to_ir_type`（目前仍是 i32）。
- [ ] `gen_expr()` 中：
  - `NumberNode` 仍返回 `ConstantInt*`。
  - 所有 `make_inst(..., IntType::get(), ...)` 改为根据 AST 表达式上下文推导类型（目前上下文只有 int，所以仍传 i32，但要显式写出来）。
  - 给 `gen_expr` 的返回值注释明确：`Value*` 的类型由表达式语义决定。
- [ ] `CallExpr` 翻译时，检查函数返回类型并把 CALL 指令的类型设为该类型（目前所有函数仍返回 i32）。

---

## 4. 后端 IR2RISCV 类型感知改造

### 4.1 文件：`src/IR2RISCV.hpp`

- [ ] 增加按类型分发辅助函数声明：
  ```cpp
  int size_of(const IR::Type* type) const;
  void emit_load(const IR::Value* addr, const std::string& reg, const IR::Type* type);
  void emit_store(const IR::Value* addr, const std::string& reg, const IR::Type* type);
  ```
- [ ] `load_operand` / `store_result` 保持整数版本，但改名为更明确的 `load_int_operand` / `store_int_result`，或内部调用新的分发函数。

### 4.2 文件：`src/IR2RISCV.cpp`

- [ ] 实现 `size_of(type)`：目前 int 返回 4，void 返回 0（void 不应被分配栈槽）。
- [ ] `gen_function()` 中计算栈槽时，对 `ALLOCA` 和产生值的指令都调用 `size_of(inst->type)` 来递减 `next_offset_`（目前仍是 4）。
- [ ] `frame_size_` 继续按 16 字节对齐，但基于 `next_offset_` 的绝对值计算。
- [ ] 把 `gen_bb()` 里所有算术/比较指令的硬编码 `add`/`sub`/`mul`/... 改为查类型后再 emit：
  - 目前只支持 int，所以函数里可以先写 `emit_int_arith(inst)`；但 switch 分支上要留出 `if (type->is_integer())` 的判断位置。
- [ ] `LOAD` / `STORE` 的代码改为：
  ```cpp
  if (type->is_integer()) { /* lw / sw */ }
  else { throw std::runtime_error("unsupported load/store type"); }
  ```
- [ ] `RET` 加载返回值时按函数返回类型处理（目前仍是 int）。
- [ ] `CALL` 保存返回值时按函数返回类型处理（目前仍是 int）。

---

## 5. 汇编器与模拟器

### 5.1 文件：`src/assembler_macro.hpp`

- [ ] 当前不需要新增浮点指令；但心里明确这是将来加 float 时要改的地方。
- [ ] 确认 `find_inst` 对整数指令表的查找逻辑不需要改动。

### 5.2 文件：`src/assembler.cpp`

- [ ] 当前不需要改动。

### 5.3 文件：`src/Computer.cpp`

- [ ] 当前不需要改动。

> 说明：类型架构重构只影响 compiler.exe 的前中端，不触及 assembler/simulator。这正是为什么要先做它——风险范围可控。

---

## 6. 测试策略

### 6.1 重构过程中每步都要跑

- [ ] `make` 能成功编译三个可执行文件。
- [ ] `powershell tests/run_tests.ps1` 全部 41 个用例通过。
- [ ] IR 层回归测试通过：
  ```bash
  g++ -std=c++17 -Wall -Wextra -Isrc tests/AST2IRTEST.cpp \
      src/IR.cpp src/AST2IR.cpp src/lexer.cpp src/parser.cpp -o temp/ast2ir_test
  ./temp/ast2ir_test
  ```

### 6.2 新增类型架构健康检查用例（可选，写到 AST2IRTEST.cpp）

重构完成后，可以加几个纯结构测试：

- [ ] 确认 `1 + 2` 的 `%t1 = add i32 1, 2` 中每个 `Value` 的 type 都是 `IntType::get()`。
- [ ] 确认 `void` 类型的 `JMP`/`STORE`/`BR`/`RET` 指令的 type 是 `VoidType::get()`。
- [ ] 确认 `Function` 的返回类型可以不是 i32（需要等到真正支持 void 时才测）。

---

## 7. 验收标准

- [ ] 全部 41 个端到端测试通过。
- [ ] AST2IRTEST 全部通过。
- [ ] `make` 无警告（`-Wall -Wextra`）。
- [ ] 代码中没有“隐式默认 i32”的构造函数调用；所有 `Value`/`Instruction`/`Argument` 的类型来源都可追溯。
- [ ] `src/IR2RISCV.cpp` 中至少有一处显式按类型分发（即使当前只有 int 分支）。
- [ ] `AST2IR` 中存在 `to_ir_type` 函数，作为 AST 与 IR 类型系统的唯一翻译点。
- [ ] 文档：在本文件里勾选所有完成项，并记录任何偏离设计的地方。

---

## 8. 重构完成后，下一个 feature 的入口

### 8.1 加 void 函数时要做的事（预览）

- `token.hpp`：加 `KW_VOID`。
- `ast.hpp`：加 `VoidType`，覆盖 `is_void()`。
- `parser.cpp`：`parse_type()` 支持 `void`；函数返回类型可 void；`return;` 允许无表达式。
- `IR.hpp`：已经有 `VoidType`，直接复用。
- `AST2IR.cpp`：void 函数的 `RET` 操作数为空；void 调用不消费返回值。
- `IR.cpp`：`RET` 允许 0 个操作数（当当前函数返回 void 时）；`CALL` 允许返回 void。
- `IR2RISCV.cpp`：void 函数无返回值，prologue/epilogue 不变；void 调用不调 `store_result`。

### 8.2 加 float 时要做的事（预览）

- `token.hpp`：浮点数字面量 token（如 `FLOAT`）。
- `ast.hpp`：`FloatType`。
- `IR.hpp`：`FloatType`；`ConstantFloat`。
- `AST2IR.cpp`：`to_ir_type` 增加 float 分支；`NumberNode` 需要区分 int/float 字面量。
- `IR2RISCV.cpp`：浮点运算走 `fadd.s`/`fsub.s`/...，load/store 走 `flw`/`fsw`，传参用 `fa0-fa7`。
- `assembler_macro.hpp`：增加 F 扩展指令表。
- `Computer.cpp`：增加浮点寄存器文件 `f0-f31` 和浮点指令执行逻辑。

### 8.3 加指针/数组时要做的事（预览）

- `ast.hpp`：`PtrType`、`ArrayType`；`AddressOfExpr`、`DerefExpr`、`ArrayIndexExpr`。
- `IR.hpp`：`PtrType`；`GEP` 指令。
- `AST2IR.cpp`：`&a` 返回对应 alloca 指令（地址即值）；`*p` 生成 LOAD；`a[i]` 生成 GEP 再 LOAD/STORE。
- `IR.cpp`：放宽 LOAD/STORE 地址约束为“ALLOCA 或 GEP”；实现 GEP 的类型检查。
- `IR2RISCV.cpp`：GEP 翻译成地址加法；指针即 32 位整数。

---

## 9. 风险提示

- **不要顺手实现新类型**：重构期间只做“让代码能容纳新类型”的工作，不要真的去解析 `float` 或 `void` 关键字。否则测试会爆炸，debug 面会失控。
- **注意 `replace_all_uses_with` 的类型安全**：当前 `Value::replace_all_uses_with` 直接把 `this` 的 users 挪给 `v`，未来如果类型不同还要额外检查；重构阶段先不改它，但要意识到这个函数以后会需要类型校验。
- **汇编器和模拟器不要动**：这次重构把它们划为禁区，确保出了问题范围只在 compiler.exe。
- **AST Type 与 IR Type 的映射是单点**：所有类型转换必须经过 `AST2IR::to_ir_type`，不要在中端其他地方引用 `ast.hpp` 的 Type。

---

## 10. 推荐执行顺序

1. 先改 `src/ast.hpp` 的 `Type` 接口（加 `is_xxx`、`const`）。
2. 改 `src/parser.cpp`，引入 `parse_type()`。
3. 改 `src/IR.hpp` 的 `Type` 接口和 `Argument` 构造函数。
4. 改 `src/IR.cpp`，`Function::add_arg()` 传类型，`make_inst` 类型检查通用化。
5. 改 `src/AST2IR.cpp/hpp`，加 `to_ir_type`，所有类型走映射。
6. 改 `src/IR2RISCV.cpp/hpp`，让后端按类型分发（目前只有 int 分支）。
7. 跑 `make` + `run_tests.ps1` + `AST2IRTEST`。
8. 勾选本 checklist，记录偏差。
