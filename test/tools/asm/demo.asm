LUI r1, 0x0000
ORI r1, 0x0100
LUI r2, 0x1122
ORI r2, 0x3344
SW r2, [r1]
LW r3, [r1]
LUI r4, 0x0001
loop:
    BEQ r0, r0, done
    ORI r4, 0x0001
done:
    HALT