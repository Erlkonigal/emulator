# Fibonacci - compute 40 iterations, output final result as "0xXXXXXXXX\r\n"
# fib(40) = 102334155 = 0x061A884B
# 32-bit architecture: 8 hex digits

    # r1 = fib_prev (starts 0)
    # r2 = fib_curr (starts 1)
    LUI r1, 0x0000
    LUI r2, 0x0000
    ORI r2, 0x0001

    # r3 = counter = 40
    LUI r3, 0x0000
    ORI r3, 0x0028

    # r5 = 1
    LUI r5, 0x0000
    ORI r5, 0x0001

fib_loop:
    ADD r14, r1, r2
    ADD r1, r2, r0
    ADD r2, r14, r0
    SUB r3, r3, r5
    BEQ r3, r0, output
    BEQ r0, r0, fib_loop

output:
    # r6 = UART base (0x10000000)
    LUI r6, 0x1000
    
    # Output "0x"
wait_tx_0:
    LBU r12, [r6, 4]
    ANDI r12, 0x0001
    BEQ r12, r0, wait_tx_0
    LUI r12, 0x0030
    ORI r12, 0x0030
    SB r12, [r6, 0]

wait_tx_x:
    LBU r12, [r6, 4]
    ANDI r12, 0x0001
    BEQ r12, r0, wait_tx_x
    LUI r12, 0x0078
    ORI r12, 0x0078
    SB r12, [r6, 0]

    # Output 8 hex digits for 32-bit value in r2
    # r13 = value to output
    ADD r13, r2, r0
    # r4 = digit count = 8
    LUI r4, 0x0000
    ORI r4, 0x0008

hex_loop:
    # Get top nibble (bits 31:28)
    SRLI r9, r13, 28
    # Shift right by 4 for next nibble
    SLLI r13, r13, 4
    
    # r7 = '0' = 0x30
    LUI r7, 0x0030
    ORI r7, 0x0030
    
    # r12 = nibble + '0'
    ADD r12, r9, r7

    # Check if nibble >= 10 (need A-F)
    # nibble >= 10 if (nibble & 8) && (nibble & 6)
    ADD r15, r9, r0
    ANDI r15, 0x0008
    BEQ r15, r0, output_nibble
    ADD r15, r9, r0
    ANDI r15, 0x0006
    BEQ r15, r0, output_nibble
    # nibble >= 10, add 7: 'A' = '0' + 10 + 7 = 0x41
    LUI r15, 0x0000
    ORI r15, 0x0007
    ADD r12, r12, r15

output_nibble:
wait_tx_nibble:
    LBU r15, [r6, 4]
    ANDI r15, 0x0001
    BEQ r15, r0, wait_tx_nibble
    SB r12, [r6, 0]

    SUB r4, r4, r5
    BEQ r4, r0, output_crlf
    BEQ r0, r0, hex_loop

output_crlf:
    # Output '\r'
wait_tx_cr:
    LBU r12, [r6, 4]
    ANDI r12, 0x0001
    BEQ r12, r0, wait_tx_cr
    LUI r12, 0x0000
    ORI r12, 0x000D
    SB r12, [r6, 0]

    # Output '\n'
wait_tx_lf:
    LBU r12, [r6, 4]
    ANDI r12, 0x0001
    BEQ r12, r0, wait_tx_lf
    LUI r12, 0x0000
    ORI r12, 0x000A
    SB r12, [r6, 0]

done:
    HALT