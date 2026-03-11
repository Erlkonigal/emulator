# Demo: Store value to RAM, load it back, output to UART
# UART base: 0x10000000
# RAM base:  0x80000000

    # Set r1 = 0x80000100 (RAM address)
    LUI r1, 0x8000
    ORI r1, 0x0100
    
    # Set r2 = 0x11223344 (test value)
    LUI r2, 0x1122
    ORI r2, 0x3344
    
    # Store r2 to RAM, then load back to r3
    SW r2, [r1]
    LW r3, [r1]
    
    # Set r4 = UART base address
    LUI r4, 0x1000
    
    # Wait for UART TX ready (status bit 0 = 1)
wait_tx:
    LBU r5, [r4, 4]
    ANDI r5, 0x0001
    BEQ r5, r0, wait_tx
    
    # Output r3 to UART
    SW r3, [r4, 0]
    
    HALT

dead_loop:
    BEQ r0, r0, dead_loop