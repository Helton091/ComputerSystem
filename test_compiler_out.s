main:
addi t0, zero, 2
addi sp, sp, -4
sw t0, 0(sp)
lw t0, 0(sp)
addi sp, sp, 4
sub t0, zero, t0
addi sp, sp, -4
sw t0, 0(sp)
addi t0, zero, 3
addi sp, sp, -4
sw t0, 0(sp)
addi t0, zero, 4
addi sp, sp, -4
sw t0, 0(sp)
lw t1, 0(sp)
addi sp, sp, 4
lw t0, 0(sp)
addi sp, sp, 4
mul t0, t0, t1
addi sp, sp, -4
sw t0, 0(sp)
lw t1, 0(sp)
addi sp, sp, 4
lw t0, 0(sp)
addi sp, sp, 4
add t0, t0, t1
addi sp, sp, -4
sw t0, 0(sp)
lw a0, 0(sp)
addi sp, sp, 4
li a7, 10
ecall
