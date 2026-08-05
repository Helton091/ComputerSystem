# test.s: 打印 42 然后退出
_start:
    li   a0, 42
    li   a1, 12
    add  a0, a1, a0
    li   a7, 1
    ecall
    addi a0, a1, -13
    ecall
    li   a7, 10
    ecall
