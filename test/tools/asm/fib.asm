# Fibonacci sequence demo (loop version)
# Computes: 0, 1, 1, 2, 3, 5, 8, 13, 21, 34
# r1 = fib(n-2), r2 = fib(n-1), r3 = temp
# r4 = counter, r5 = result pointer, r6 = scratch

    # Initialize counter (10 iterations)
    LUI r4, 0x0000
    ORI r4, 0x000A

    # Initialize result pointer
    LUI r5, 0x0000
    ORI r5, 0x0200

    # Initialize fib(0) = 0, fib(1) = 1
    LUI r1, 0x0000
    LUI r2, 0x0000
    ORI r2, 0x0001

loop:
    # Store fib(n-2) to memory
    SW r1, [r5]

    # r3 = r1 + r2 (next fibonacci)
    ADD r3, r1, r2

    # Shift: r1 = r2, r2 = r3
    ADD r1, r2, r0
    ADD r2, r3, r0

    # Decrement counter: r4 = r4 - 1
    LUI r6, 0x0000
    ORI r6, 0x0001
    SUB r4, r4, r6

    # Advance pointer: r5 = r5 + 4
    LUI r6, 0x0000
    ORI r6, 0x0004
    ADD r5, r5, r6

    # Check if r4 != 0, continue loop
    BEQ r4, r0, done
    BEQ r0, r0, loop

done:
    HALT