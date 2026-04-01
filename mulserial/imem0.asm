# mulserial core0: C = A*B, A base=0x000, B base=0x100, C base=0x200

        add $r2,  $zero, $imm, 0x000      # r2 = baseA
        add $r3,  $zero, $imm, 0x100      # r3 = baseB
        add $r4,  $zero, $imm, 0x200      # r4 = baseC
        add $r5,  $zero, $imm, 16         # r5 = 16

        add $r6,  $zero, $zero, 0         # i=0

L_i:
        blt $imm, $r6, $r5, L_i_body
        beq $imm, $zero, $zero, L_end

L_i_body:
        add $r7,  $zero, $zero, 0         # j=0

L_j:
        blt $imm, $r7, $r5, L_j_body
        beq $imm, $zero, $zero, L_i_inc

L_j_body:
        add $r9,  $zero, $zero, 0         # acc=0
        add $r8,  $zero, $zero, 0         # k=0

L_k:
        blt $imm, $r8, $r5, L_k_body
        beq $imm, $zero, $zero, L_store

L_k_body:
        # addrA = baseA + (i*16 + k)
        mul $r10, $r6,   $r5,   0         # r10 = i*16
        add $r10, $r10,  $r8,   0         # r10 = i*16 + k
        add $r11, $r2,   $r10,  0         # r11 = addrA
        lw  $r12, $r11,  $zero, 0         # r12 = A[i][k]

        # addrB = baseB + (k*16 + j)
        mul $r13, $r8,   $r5,   0         # r13 = k*16
        add $r13, $r13,  $r7,   0         # r13 = k*16 + j
        add $r13, $r3,   $r13,  0         # r13 = addrB
        lw  $r14, $r13,  $zero, 0         # r14 = B[k][j]

        # acc += A[i][k] * B[k][j]
        mul $r15, $r12,  $r14,  0
        add $r9,  $r9,   $r15,  0

        add $r8,  $r8,   $imm,  1         # k++
        beq $imm, $zero, $zero, L_k

L_store:
        # addrC = baseC + (i*16 + j)
        mul $r10, $r6,   $r5,   0         # r10 = i*16
        add $r10, $r10,  $r7,   0         # r10 = i*16 + j
        add $r11, $r4,   $r10,  0         # r11 = addrC
        sw  $r9,  $r11,  $zero, 0         # C[i][j] = acc

        add $r7,  $r7,   $imm,  1         # j++
        beq $imm, $zero, $zero, L_j

L_i_inc:
        add $r6,  $r6,   $imm,  1         # i++
        beq $imm, $zero, $zero, L_i

L_end:
        halt $zero, $zero, $zero, 0
