#ifndef TEST_TOY_ISA_H
#define TEST_TOY_ISA_H

#include <cstdint>
#include <vector>

namespace toy {

enum class Op : uint8_t {
    Nop = 0x00,
    Lui = 0x01,
    Ori = 0x02,
    Lw = 0x03,
    Sw = 0x04,
    Beq = 0x05,
    Halt = 0x7f,
};

inline uint32_t EncodeRImm16(Op op, uint8_t rd, uint16_t imm) {
    return (static_cast<uint32_t>(op) << 24) | (static_cast<uint32_t>(rd) << 16) |
        static_cast<uint32_t>(imm);
}

inline uint32_t EncodeMem(Op op, uint8_t r0, uint8_t r1, int8_t off) {
    // op r0, [r1 + off]
    // For SW: store r0 -> [r1+off]
    // For LW: load [r1+off] -> r0
    return (static_cast<uint32_t>(op) << 24) | (static_cast<uint32_t>(r0) << 16) |
        (static_cast<uint32_t>(r1) << 8) | static_cast<uint8_t>(off);
}

inline uint32_t EncodeBranch(Op op, uint8_t r0, uint8_t r1, int8_t off) {
    return EncodeMem(op, r0, r1, off);
}

inline uint32_t Nop() {
    return static_cast<uint32_t>(Op::Nop) << 24;
}

inline uint32_t Halt() {
    return static_cast<uint32_t>(Op::Halt) << 24;
}

inline uint32_t Lui(uint8_t rd, uint16_t imm16) {
    return EncodeRImm16(Op::Lui, rd, imm16);
}

inline uint32_t Ori(uint8_t rd, uint16_t imm16) {
    return EncodeRImm16(Op::Ori, rd, imm16);
}

inline uint32_t Lw(uint8_t rd, uint8_t rs, int8_t off) {
    return EncodeMem(Op::Lw, rd, rs, off);
}

inline uint32_t Sw(uint8_t rs, uint8_t rd, int8_t off) {
    return EncodeMem(Op::Sw, rs, rd, off);
}

inline uint32_t Beq(uint8_t r0, uint8_t r1, int8_t off) {
    return EncodeBranch(Op::Beq, r0, r1, off);
}

inline void Emit(std::vector<uint32_t>* prog, uint32_t inst) {
    if (prog == nullptr) {
        return;
    }
    prog->push_back(inst);
}

} // namespace toy

#endif
