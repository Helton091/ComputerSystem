# test.s — 综合测试
# 预期输出：
# 3
# 7
# 20
# 14
# 2
# (换行)
# 55
# (换行)
# 120
# (换行)
# 42
# (换行)
# 111

# ============================================================
# _start: 程序入口
# ============================================================
_start:
    # --- 1. 基本算术 ---
    li   a0, 1
    li   a1, 2
    add  a2, a0, a1       # 1 + 2 = 3
    mv   a0, a2
    li   a7, 1
    ecall                 # print 3

    li   a0, 10
    li   a1, 3
    sub  a2, a0, a1       # 10 - 3 = 7
    mv   a0, a2
    li   a7, 1
    ecall                 # print 7

    li   a0, 4
    li   a1, 5
    mul  a2, a0, a1       # 4 * 5 = 20
    mv   a0, a2
    li   a7, 1
    ecall                 # print 20

    li   a0, 100
    li   a1, 7
    div  a2, a0, a1       # 100 / 7 = 14
    mv   a0, a2
    li   a7, 1
    ecall                 # print 14

    li   a0, 100
    li   a1, 7
    rem  a2, a0, a1       # 100 % 7 = 2
    mv   a0, a2
    li   a7, 1
    ecall                 # print 2

    # --- 2. 换行 ---
    li   a0, 10           # '\n'
    li   a7, 11
    ecall

    # --- 3. 循环：1+2+...+10 = 55 ---
    li   t0, 0            # sum = 0
    li   t1, 1            # i = 1
    li   t2, 10           # n = 10
loop_sum:
    add  t0, t0, t1       # sum += i
    addi t1, t1, 1        # i++
    # ble t1, t2, loop_sum 等价于 bge t2, t1, loop_sum
    bge  t2, t1, loop_sum # 10 >= i → 继续循环
    mv   a0, t0
    li   a7, 1
    ecall                 # print 55

    # --- 4. 换行 ---
    li   a0, 10
    li   a7, 11
    ecall

    # --- 5. 函数调用：factorial(5) = 120 ---
    li   a0, 5
    jal  ra, factorial
    li   a7, 1
    ecall                 # print 120

    # --- 6. 换行 ---
    li   a0, 10
    li   a7, 11
    ecall

    # --- 7. 内存读写：用栈空间存取 ---
    addi sp, sp, -16      # 分配栈空间
    li   t0, 42
    sw   t0, 0(sp)        # 存 42
    sw   t0, 4(sp)        # 存 42
    lw   a0, 0(sp)        # 读回来
    addi sp, sp, 16       # 恢复 sp
    li   a7, 1
    ecall                 # print 42

    # --- 8. 换行 ---
    li   a0, 10
    li   a7, 11
    ecall

    # --- 9. 条件分支：if-else ---
    # if (7 > 5) print 111; else print 999;
    li   a0, 7
    li   t0, 5
    # bgt a0, t0 → blt t0, a0
    blt  t0, a0, if_true   # 5 < 7 → true
    li   a0, 999
    j    if_end
if_true:
    li   a0, 111
if_end:
    li   a7, 1
    ecall                 # print 111

    # --- 10. 结束 ---
    li   a7, 10
    ecall

# ============================================================
# factorial: 阶乘（递归）
#   入参 a0 = n, 返回 a0 = n!
# ============================================================
factorial:
    addi sp, sp, -8
    sw   ra, 4(sp)

    # n <= 1 → 返回 1
    li   t0, 1
    # ble a0, t0 → bge t0, a0
    bge  t0, a0, fac_base

    # 保存 n，递归调用 factorial(n-1)
    addi sp, sp, -4
    sw   a0, 0(sp)
    addi a0, a0, -1
    jal  ra, factorial
    lw   t1, 0(sp)        # 恢复 n
    addi sp, sp, 4
    mul  a0, t1, a0       # n * factorial(n-1)
    j    fac_ret

fac_base:
    li   a0, 1

fac_ret:
    lw   ra, 4(sp)
    addi sp, sp, 8
    ret
