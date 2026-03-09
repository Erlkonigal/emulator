# UART Echo - read from terminal and echo back
# UART base: 0x10000000
# offset 0: DATA (RX read / TX write)
# offset 4: STATUS (bit 0=TX ready, bit 1=RX ready)
#
# Status values:
#   0: TX full,  no RX
#   1: TX ready, no RX  <- wait
#   2: TX full,  has RX
#   3: TX ready, has RX

    LUI r1, 0x1000
    LUI r4, 0x0000
    ORI r4, 0x0001

loop:
    LW r2, [r1, 4]
    BEQ r2, r0, wait
    BEQ r2, r4, wait
    
    LW r3, [r1, 0]
    SW r3, [r1, 0]
    SW r3, [r1, 0]
    BEQ r0, r0, loop

wait:
    BEQ r0, r0, loop