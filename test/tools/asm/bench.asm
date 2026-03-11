# Performance Benchmark - ~1 minute runtime
# Mixes computation and memory access to stress CPU pipeline and RAM bandwidth
# No UART output to avoid I/O overhead affecting measurements

# Register usage:
# r1  = outer loop counter
# r2  = inner loop counter  
# r3  = RAM base address (0x80000000)
# r4  = constant 1
# r5-r10 = working registers for computation

    # r1 = outer loop count = 10000
    LUI r1, 0x0000
    ORI r1, 0x2710

    # r2 = inner loop count = 4000
    LUI r2, 0x0000
    ORI r2, 0x0FA0

    # r3 = RAM base (0x80000000)
    LUI r3, 0x8000

    # r4 = constant 1
    LUI r4, 0x0000
    ORI r4, 0x0001

    # Initialize RAM with test values
    LUI r5, 0xDEAD
    ORI r5, 0xBEEF
    SW r5, [r3, 0]
    SW r5, [r3, 4]
    SW r5, [r3, 8]
    SW r5, [r3, 12]

outer_loop:
    # Reset inner loop counter
    LUI r2, 0x0000
    ORI r2, 0x0FA0

inner_loop:
    # Memory read phase - load from RAM
    LW r5, [r3, 0]
    LW r6, [r3, 4]
    LW r7, [r3, 8]
    LW r8, [r3, 12]

    # Computation phase - arithmetic and logical operations
    ADD r9, r5, r6
    SUB r10, r9, r7
    AND r11, r5, r8
    ORI r12, r5, 0x00FF
    ANDI r13, r6, 0xFF00

    # Shift operations
    SRLI r14, r9, 4
    SLLI r15, r10, 2

    # More arithmetic
    ADD r5, r14, r15
    SUB r6, r5, r11
    AND r7, r12, r13
    ADD r8, r9, r10

    # Memory write phase - store results back
    SW r5, [r3, 0]
    SW r6, [r3, 4]
    SW r7, [r3, 8]
    SW r8, [r3, 12]

    # Inner loop decrement
    SUB r2, r2, r4
    BEQ r2, r0, check_outer
    BEQ r0, r0, inner_loop

check_outer:
    # Outer loop decrement
    SUB r1, r1, r4
    BEQ r1, r0, done
    BEQ r0, r0, outer_loop

done:
    HALT