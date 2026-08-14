#pragma once
// ============================================================
// IR.hpp —— value-based IR 的核心数据结构
// 设计文档：ir_design_report.md
//
// 所有权约定（报告第 2 章）：
//   拥有关系是树：Module → Function → BasicBlock → Instruction，
//   一律 unique_ptr 持有；
//   交叉引用是图：operands / users / parent 等一律裸指针观察，
//   只许看，不许 delete。
// ============================================================
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <iostream>
#include <stdexcept>

namespace IR {

// ---------- 前向声明 ----------
struct Value;
struct User;
struct Instruction;
struct BasicBlock;
struct Function;
struct Module;

// ============================================================
// 类型系统
// IR 自带的 Type，与 ast.hpp 的 Type 无关（报告第 2 章）。
// 类型实例是全局单例：整个进程里 i32 只有一份，
// "类型相等"直接比指针即可。
// ============================================================
class Type {
public:
    virtual ~Type() = default;
    virtual int size() const = 0;               // 占字节数
    virtual std::string to_string() const = 0;  // dump 用
};

class VoidType : public Type {
public:
    static VoidType* get() { static VoidType t; return &t; }
    int size() const override { return 0; }
    std::string to_string() const override { return "void"; }
private:
    VoidType() = default;  // 单例：外部不许 new
};

class IntType : public Type {
public:
    static IntType* get() { static IntType t; return &t; }
    int size() const override { return 4; }
    std::string to_string() const override { return "i32"; }
private:
    IntType() = default;
};

// 预留：FloatType / PtrType（报告第 8 章）

// ============================================================
// 指令操作码（报告第 3 章）
// ============================================================
enum class Opcode {
    // 内存
    ALLOCA, LOAD, STORE,
    // 算术
    ADD, SUB, MUL, DIV, REM, NEG,
    // 比较（结果是 i32 的 0/1）
    LT, GT, LE, GE, EQ, NE,
    // 控制流
    BR,      // 条件跳转：operands = [cond, true_bb, false_bb]
    JMP,     // 无条件跳转：operands = [target_bb]
    RET,     // operands = [retval]
    // 函数
    CALL,    // operands = [callee, arg0, arg1, ...]
    // 预留：mem2reg 之前不应出现，见到必须报错
    PHI,
};

// ============================================================
// Value / User：一切皆是值
// ============================================================
struct Value {
    Type* type;
    std::string name;            // 调试/dump 用，可为空
    std::vector<User*> users;    // 观察者：谁在用我（use 链）

    Value(Type* t, std::string n = "") : type(t), name(std::move(n)) {}
    virtual ~Value() = default;

    void add_use(User* u) { users.push_back(u); }
    void remove_use(User* u);
    // 让所有使用者改指 v（删指令前的清场动作之一）
    void replace_all_uses_with(Value* v);
};

struct User : Value {
    std::vector<Value*> operands;  // 观察者：我用了谁（def 链）

    User(Type* t, std::string n = "") : Value(t, std::move(n)) {}
    ~User() override { drop_operands(); }  // 自动清理反向链

    void add_operand(Value* v);
    void drop_operands();  // 从每个操作数的 users 里摘掉自己
};

// ============================================================
// 具体的 Value
// ============================================================

// 常量：整个程序同一个数字只有一份（Module 常量池）
struct ConstantInt : Value {
    int i_val;
    explicit ConstantInt(int v) : Value(IntType::get()), i_val(v) {}
};

// 函数形参
struct Argument : Value {
    int arg_no;
    explicit Argument(int no)
        : Value(IntType::get(), "arg" + std::to_string(no)), arg_no(no) {}
};

// 指令：既产出一个值（是 Value），又使用别的值（是 User）
struct Instruction : User {
    Opcode op;
    BasicBlock* parent = nullptr;  // 观察者：我属于哪个块

    Instruction(Opcode o, Type* t, std::string n = "")
        : User(t, std::move(n)), op(o) {}

    // 删除一条指令的唯一合法入口（报告第 2 章规则 3）
    void erase_from_parent();
};

// 基本块：只能从头进、从尾出的指令序列
struct BasicBlock : Value {
    std::vector<std::unique_ptr<Instruction>> insts;  // 拥有
    Function* parent = nullptr;                        // 观察者

    explicit BasicBlock(std::string n)
        : Value(VoidType::get(), std::move(n)) {}

    // 把指令挂到块尾（所有权转入），返回观察者指针。
    // 块已被 RET/BR/JMP 终结后再塞指令会抛异常（报告附录纪律）。
    Instruction* add_inst(std::unique_ptr<Instruction> inst);
    bool is_terminated() const;
};

// 函数
struct Function : Value {
    std::vector<std::unique_ptr<Argument>> args;      // 拥有
    std::vector<std::unique_ptr<BasicBlock>> blocks;  // 拥有
    BasicBlock* entry = nullptr;                       // 观察者 = blocks[0]

    explicit Function(std::string n)
        : Value(VoidType::get(), std::move(n)) {}

    Argument* add_arg();                       // 追加一个 i32 形参
    BasicBlock* add_block(const std::string& name);  // 追加一个基本块
};

// 整个程序
struct Module {
    std::vector<std::unique_ptr<Function>> functions;  // 拥有

    Function* add_function(const std::string& name);
    Function* find_function(const std::string& name) const;

    // 常量池：取数字 v 的唯一 ConstantInt，没有就建一个
    ConstantInt* get_const(int v);

    void dump(std::ostream& out) const;

private:
    std::vector<std::unique_ptr<ConstantInt>> const_pool_;  // 拥有
    std::unordered_map<int, ConstantInt*> const_map_;        // 观察者索引
};

// ============================================================
// 指令工厂：造指令的唯一入口
// 集中做构建期检查（操作数个数、类型），检查不过直接抛异常——
// 让 IR 错误在构建时爆炸，而不是到后端才生成出错误汇编。
// ============================================================
Instruction* make_inst(BasicBlock* bb, Opcode op, Type* type,
                       const std::string& name,
                       std::initializer_list<Value*> operands = {});

} // namespace IR
