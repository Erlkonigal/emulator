# UART Echo - read from terminal and echo back
# UART base: 0x10000000
# offset 0: DATA (RX read / TX write)
# offset 4: STATUS (bit 0=TX ready, bit 1=RX ready)
#
# Status values:
#   0: TX full,  no RX
#   1: TX ready, no RX
#   2: TX full,  has RX
#   3: TX ready, has RX  <- safe to echo

    LUI r1, 0x1000      # r1 = UART base (0x10000000)
    LUI r4, 0x0000
    ORI r4, 0x0003      # r4 = 3 (TX ready AND RX ready)

loop:
    LBU r2, [r1, 4]     # r2 = status (byte read)
    BEQ r2, r4, echo    # if status == 3, proceed to echo
    BEQ r0, r0, loop    # else, keep polling

echo:
    LBU r3, [r1, 0]     # read RX byte
    SB r3, [r1, 0]      # write to TX (echo)
    BEQ r0, r0, loop    # continue polling