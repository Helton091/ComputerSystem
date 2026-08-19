# RV32F 机器码速查与实现摘要

> 本文件基于 RISC-V 官方 ISA Manual（`riscv-isa-manual/src/unpriv/f-st-ext.adoc`）整理，
> 用于 C-- 编译器浮点扩展实现时快速查阅。
>
> 所有编码数值建议与 `riscv64-unknown-elf-objdump` 输出交叉验证。

---

## 1. RV32F 新增了什么

| 新增内容 | 说明 |
|---|---|
| 浮点寄存器文件 | `f0` ~ `f31`，共 32 个 32 位寄存器 |
| 浮点 CSR | `fcsr`（包含 `fflags` 和 `frm`） |
| 加载/存储指令 | `flw`、`fsw` |
| 算术运算指令 | `fadd.s`、`fsub.s`、`fmul.s`、`fdiv.s`、`fsqrt.s` |
| 符号注入指令 | `fsgnj.s`、`fsgnjn.s`、`fsgnjx.s` |
| 最小/最大指令 | `fmin.s`、`fmax.s` |
| 比较指令 | `feq.s`、`flt.s`、`fle.s` |
| 转换指令 | `fcvt.w.s`、`fcvt.wu.s`、`fcvt.s.w`、`fcvt.s.wu` |
| 搬运/分类指令 | `fmv.x.w`、`fmv.w.x`、`fclass.s` |
| 融合乘加指令 | `fmadd.s`、`fmsub.s`、`fnmsub.s`、`fnmadd.s`（可选） |

---

## 2. 浮点寄存器与调用约定

### 2.1 寄存器命名

| 编号 | 别名 | 用途 |
|---|---|---|
| f0~f7 | ft0~ft7 | 临时寄存器 |
| f8~f9 | fs0~fs1 | 被调用者保存 |
| f10~f17 | fa0~fa7 | 函数参数 / 返回值 |
| f18~f27 | fs2~fs11 | 被调用者保存 |
| f28~f31 | ft8~ft11 | 临时寄存器 |

### 2.2 调用约定

- 浮点函数参数：`fa0` ~ `fa7`（f10~f17）
- 浮点返回值：`fa0` ~ `fa1`（f10~f11）
- `f0` **不是**恒零寄存器，就是普通寄存器

---

## 3. fcsr 浮点控制状态寄存器

`fcsr` 是 32 位 CSR，低 8 位有效：

```
| 31:8 | 7:5 | 4:0 |
| 保留 | frm | fflags |
```

### 3.1 fflags（异常标志位，sticky）

| 位 | 名称 | 含义 |
|---|---|---|
| 0 | NX | Inexact，结果不精确 |
| 1 | UF | Underflow，下溢 |
| 2 | OF | Overflow，上溢 |
| 3 | DZ | Divide by Zero，除以零 |
| 4 | NV | Invalid Operation，非法操作 |

### 3.2 frm（舍入模式）

| 编码 | 模式 | 含义 |
|---|---|---|
| 000 | RNE | Round to Nearest, ties to Even |
| 001 | RTZ | Round toward Zero |
| 010 | RDN | Round Down（向负无穷）|
| 011 | RUP | Round Up（向正无穷）|
| 100 | RMM | Round to Nearest, ties to Max Magnitude |
| 101~111 | 保留 | 非法 |

### 3.3 访问 fcsr 的 CSR 指令

F 扩展依赖 Zicsr 扩展。常用伪指令：

```asm
frflags  rd      # 读 fflags
fsflags  rd, rs  # 写 fflags
frrm     rd      # 读 frm
fsrm     rd, rs  # 写 frm
frcsr    rd      # 读 fcsr
fscsr    rd, rs  # 写 fcsr
```

这些伪指令底层是 `csrrs` / `csrrw` 等 CSR 指令。

---

## 4. 指令格式总览

### 4.1 OP-FP 算术指令（R-type，opcode = 0x53）

```
| 31:27 | 26:25 | 24:20 | 19:15 | 14:12 | 11:7 | 6:0  |
|-------|-------|-------|-------|-------|------|------|
| funct5| fmt   | rs2   | rs1   | funct3| rd   |opcode|
```

`funct7 = {funct5, fmt}`。

### 4.2 加载 `flw`（I-type，opcode = 0x03）

```
| 31:20    | 19:15 | 14:12 | 11:7 | 6:0  |
|----------|-------|-------|------|------|
| imm[11:0]| rs1   | 010   | rd   |0000011|
```

### 4.3 存储 `fsw`（S-type，opcode = 0x23）

```
| 31:25     | 24:20 | 19:15 | 14:12 | 11:7      | 6:0  |
|-----------|-------|-------|-------|-----------|------|
|imm[11:5]  | rs2   | rs1   | 010   | imm[4:0]  |0100011|
```

### 4.4 R4-type 融合乘加（opcode 0x43/0x47/0x4B/0x4F）

```
| 31:27 | 26:25 | 24:20 | 19:15 | 14:12 | 11:7 | 6:0  |
|-------|-------|-------|-------|-------|------|------|
| rs3   | fmt   | rs2   | rs1   | rm    | rd   |opcode|
```

---

## 5. 完整指令编码表

### 5.1 加载/存储

| 指令 | Format | opcode | funct3 | 说明 |
|---|---|---|---|---|
| `flw rd, imm(rs1)` | I | `0x07` | `0x2` | 从内存加载 32 位到 f[rd] |
| `fsw rs2, imm(rs1)` | S | `0x27` | `0x2` | 把 f[rs2] 存到内存 |

### 5.2 算术运算（opcode = 0x53，fmt = 00）

| 指令 | funct5 | fmt | rs2 | funct3 | funct7 | 说明 |
|---|---|---|---|---|---|---|
| `fadd.s`  | `00000` | `00` | reg | rm | `0x00` | 加 |
| `fsub.s`  | `00001` | `00` | reg | rm | `0x04` | 减 |
| `fmul.s`  | `00010` | `00` | reg | rm | `0x08` | 乘 |
| `fdiv.s`  | `00011` | `00` | reg | rm | `0x0C` | 除 |
| `fsqrt.s` | `01011` | `00` | `00000` | rm | `0x2C` | 平方根 |

`rm` 通常为 `000`（RNE）。

### 5.3 符号注入（opcode = 0x53，fmt = 00）

| 指令 | funct5 | fmt | rs2 | funct3 | funct7 |
|---|---|---|---|---|---|
| `fsgnj.s`  | `00100` | `00` | reg | `000` | `0x10` |
| `fsgnjn.s` | `00100` | `00` | reg | `001` | `0x10` |
| `fsgnjx.s` | `00100` | `00` | reg | `010` | `0x10` |

### 5.4 最小/最大（opcode = 0x53，fmt = 00）

| 指令 | funct5 | fmt | rs2 | funct3 | funct7 |
|---|---|---|---|---|---|
| `fmin.s` | `00101` | `00` | reg | `000` | `0x14` |
| `fmax.s` | `00101` | `00` | reg | `001` | `0x14` |

### 5.5 比较（opcode = 0x53，fmt = 00，结果写整数寄存器）

| 指令 | funct5 | fmt | rs2 | funct3 | funct7 |
|---|---|---|---|---|---|
| `fle.s` | `10100` | `00` | reg | `000` | `0x50` |
| `flt.s` | `10100` | `00` | reg | `001` | `0x50` |
| `feq.s` | `10100` | `00` | reg | `010` | `0x50` |

### 5.6 浮点 ↔ 整数转换（opcode = 0x53，fmt = 00）

| 指令 | funct5 | fmt | rs2 | funct3 | funct7 |
|---|---|---|---|---|---|
| `fcvt.w.s`  | `11000` | `00` | `00000` | rm | `0x60` |
| `fcvt.wu.s` | `11000` | `00` | `00001` | rm | `0x60` |
| `fcvt.s.w`  | `11010` | `00` | `00000` | rm | `0x68` |
| `fcvt.s.wu` | `11010` | `00` | `00001` | rm | `0x68` |

`rs2` 字段表示目标/源整数类型，不是寄存器。

### 5.7 搬运与分类（opcode = 0x53，fmt = 00）

| 指令 | funct5 | fmt | rs2 | funct3 | funct7 | 说明 |
|---|---|---|---|---|---|---|
| `fmv.x.w` | `11100` | `00` | `00000` | `000` | `0x70` | f → x，位模式搬运 |
| `fclass.s`| `11100` | `00` | `00000` | `001` | `0x70` | 浮点分类 |
| `fmv.w.x` | `11110` | `00` | `00000` | `000` | `0x78` | x → f，位模式搬运 |

### 5.8 融合乘加（R4-type，fmt = 00）

| 指令 | opcode | 说明 |
|---|---|---|
| `fmadd.s`  | `0x43` | rd = rs1*rs2 + rs3 |
| `fmsub.s`  | `0x47` | rd = rs1*rs2 - rs3 |
| `fnmsub.s` | `0x4B` | rd = -(rs1*rs2 - rs3) |
| `fnmadd.s` | `0x4F` | rd = -(rs1*rs2 + rs3) |

C-- 教学实现可先跳过这组。

---

## 6. 机器码计算公式

### 6.1 R-type OP-FP

```cpp
uint32_t encode_fp_r(uint8_t funct5, uint8_t fmt, uint8_t rs2,
                     uint8_t rs1, uint8_t funct3, uint8_t rd) {
    uint8_t funct7 = (funct5 << 2) | fmt;
    return (funct7 << 25) | (rs2 << 20) | (rs1 << 15)
         | (funct3 << 12) | (rd << 7) | 0x53;
}
```

### 6.2 `flw`

```cpp
uint32_t encode_flw(int32_t imm, uint8_t rs1, uint8_t rd) {
    return ((imm & 0xFFF) << 20) | (rs1 << 15) | (0x2 << 12) | (rd << 7) | 0x07;
}
```

### 6.3 `fsw`

```cpp
uint32_t encode_fsw(int32_t imm, uint8_t rs2, uint8_t rs1) {
    uint32_t imm11_5 = (imm >> 5) & 0x7F;
    uint32_t imm4_0  = imm & 0x1F;
    return (imm11_5 << 25) | (rs2 << 20) | (rs1 << 15)
         | (0x2 << 12) | (imm4_0 << 7) | 0x27;
}
```

### 6.4 `fcvt` 转换

```cpp
uint32_t encode_fcvt(uint8_t funct5, uint8_t rs2, uint8_t rs1,
                     uint8_t rm, uint8_t rd) {
    uint8_t funct7 = (funct5 << 2) | 0x0;   // fmt = 00 for S
    return (funct7 << 25) | (rs2 << 20) | (rs1 << 15)
         | (rm << 12) | (rd << 7) | 0x53;
}
```

---

## 7. C-- 最小实现集

对于 C-- 支持 `float` 类型，建议先实现以下指令：

**必需：**
- `flw`、`fsw`
- `fadd.s`、`fsub.s`、`fmul.s`、`fdiv.s`
- `feq.s`、`flt.s`、`fle.s`
- `fcvt.s.w`、`fcvt.w.s`
- `fmv.w.x`、`fmv.x.w`

**可选（后续加）：**
- `fsqrt.s`
- `fmin.s`、`fmax.s`
- `fsgnj.s` 系列
- `fclass.s`
- `fmadd.s` 系列

---

## 8. 实现注意事项

1. **浮点寄存器存 raw bits**：用 `uint32_t FRegFile[32]`，不要直接存 `float`。
2. **`float` ↔ `uint32_t` 用 `std::memcpy`**，不要用 `reinterpret_cast`。
3. **比较结果写整数寄存器**：`feq.s` / `flt.s` / `fle.s` 的 `rd` 是整数寄存器。
4. **`f0` 不是恒零**：可以直接读写。
5. **异常标志可先简化**：`fflags` 全部置 0 也能跑大多数程序。
6. **舍入模式可先只支持 RNE**：`rm = 000`。
7. **NaN payload 传播**：教学项目可先统一返回 canonical NaN `0x7FC00000`。

---

## 9. 参考文件

- 官方 F 扩展文档（已复制）：`doc/riscv-f-st-ext.adoc`
- 官方 CSR 指令文档（已复制）：`doc/riscv-zicsr.adoc`
- 在线 HTML 版：[RISC-V F Extension](https://docs.riscv.org/reference/isa/v20250508/unpriv/f-st-ext.html)
- 在线 CSR 文档：[RISC-V Zicsr Extension](https://docs.riscv.org/reference/isa/v20250508/unpriv/zicsr.html)

---

## 10. 验证编码示例

用 `riscv64-unknown-elf-gcc` 编译一段汇编：

```asm
fadd.s  f3, f1, f2
flw     f3, 4(x1)
fsw     f3, 4(x1)
fcvt.s.w f3, x1
```

```bash
riscv64-unknown-elf-gcc -march=rv32imf -mabi=ilp32f -c test.s -o test.o
riscv64-unknown-elf-objdump -d test.o
```

预期输出（供参考）：

```
00000000 <.text>:
   0:   002090d3            fadd.s  f3,f1,f2
   4:   00409187            flw     f3,4(x1)
   8:   003091a7            fsw     f3,4(x1)
   c:   d00090d3            fcvt.s.w f3,x1
```
