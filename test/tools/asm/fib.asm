# Fibonacci sequence demo (loop version)
# Computes: 0, 1, 1, 2, 3, 5, 8, 13, 21, 34
# Outputs each result via UART
# UART base: 0x10000000
# RAM base:  0x80000000

    # Initialize counter (10 iterations)
    LUI r4, 0x0000
    ORI r4, 0x000A

    # Initialize result pointer (RAM address)
    LUI r5, 0x8000
    ORI r5, 0x0200

    # Initialize fib(0) = 0, fib(1) = 1
    LUI r1, 0x0000
    LUI r2, 0x0000
    ORI r2, 0x0001
    
    # Set r6 = UART base address
    LUI r6, 0x1000

loop:
    # Store fib(n-2) to memory
    SW r1, [r5]

    # Output fib(n-2) to UART
    # Wait for UART TX ready (status bit 0 = 1)
wait_tx:
    LW r7, [r6, 4]
    ANDI r7, 0x0001
    BEQ r7, r0, wait_tx
    
    # Output r1 to UART
    SW r1, [r6, 0]

    # r3 = r1 + r2 (next fibonacci)
    ADD r3, r1, r2

    # Shift: r1 = r2, r2 = r3
    ADD r1, r2, r0
    ADD r2, r3, r0

    # Decrement counter: r4 = r4 - 1
    LUI r7, 0x0000
    ORI r7, 0x0001
    SUB r4, r4, r7

    # Advance pointer: r5 = r5 + 4
    LUI r7, 0x0000
    ORI r7, 0x0004
    ADD r5, r5, r7

    # Check if r4 != 0, continue loop
    BEQ r4, r0, done
    BEQ r0, r0, loop

done:
    HALT

dead_loop:
    BEQ r0, r0, dead_loop