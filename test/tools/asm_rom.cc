#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

enum class Op : uint8_t {
    Nop = 0x00,
    Lui = 0x01,
    Ori = 0x02,
    Lw = 0x03,
    Sw = 0x04,
    Beq = 0x05,
    Add = 0x06,
    Sub = 0x07,
    Andi = 0x08,
    Lb = 0x09,
    Lh = 0x0a,
    Lbu = 0x0b,
    Lhu = 0x0c,
    Sb = 0x0d,
    Sh = 0x0e,
    Srli = 0x0f,
    Slli = 0x10,
    And = 0x11,
    Halt = 0x7f,
};

struct Instruction {
    Op op;
    uint8_t rd;
    uint8_t rs;
    int16_t imm;
    std::string labelRef;
    int lineNum;
};

static std::string Trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::string ToUpper(const std::string& s) {
    std::string r = s;
    for (char& c : r) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return r;
}

static bool ParseRegister(const std::string& s, uint8_t* reg) {
    if (s.size() < 2 || ToUpper(s.substr(0, 1)) != "R") return false;
    std::string num = s.substr(1);
    int v = std::stoi(num);
    if (v < 0 || v > 15) return false;
    *reg = static_cast<uint8_t>(v);
    return true;
}

static bool ParseImm16(const std::string& s, int16_t* imm) {
    try {
        if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
            *imm = static_cast<int16_t>(std::stoul(s, nullptr, 16));
        } else {
            *imm = static_cast<int16_t>(std::stoi(s));
        }
        return true;
    } catch (...) {
        return false;
    }
}

static bool ParseOffset(const std::string& s, int8_t* off) {
    try {
        int v = std::stoi(s);
        if (v < -128 || v > 127) return false;
        *off = static_cast<int8_t>(v);
        return true;
    } catch (...) {
        return false;
    }
}

static std::vector<std::string> Tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::string current;
    bool inBracket = false;

    for (char c : line) {
        if (c == '[') {
            inBracket = true;
            if (!current.empty()) {
                tokens.push_back(Trim(current));
                current.clear();
            }
            tokens.push_back("[");
        } else if (c == ']') {
            inBracket = false;
            if (!current.empty()) {
                tokens.push_back(Trim(current));
                current.clear();
            }
            tokens.push_back("]");
        } else if (c == ',' && !inBracket) {
            if (!current.empty()) {
                tokens.push_back(Trim(current));
                current.clear();
            }
        } else if ((c == ' ' || c == '\t') && !inBracket) {
            if (!current.empty()) {
                tokens.push_back(Trim(current));
                current.clear();
            }
        } else if (c == ',' && inBracket) {
            if (!current.empty()) {
                tokens.push_back(Trim(current));
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        tokens.push_back(Trim(current));
    }
    return tokens;
}

static bool ParseLine(const std::string& line, Instruction* inst, std::string* label, int lineNum) {
    std::string l = Trim(line);
    if (l.empty() || l[0] == '#' || l[0] == ';') {
        return false;
    }

    size_t colonPos = l.find(':');
    if (colonPos != std::string::npos) {
        *label = Trim(l.substr(0, colonPos));
        l = Trim(l.substr(colonPos + 1));
        if (l.empty()) {
            return false;
        }
    }

    std::vector<std::string> tokens = Tokenize(l);
    if (tokens.empty()) return false;

    std::string opName = ToUpper(tokens[0]);
    inst->lineNum = lineNum;
    inst->labelRef.clear();

    if (opName == "NOP") {
        inst->op = Op::Nop;
        return true;
    }
    if (opName == "HALT") {
        inst->op = Op::Halt;
        return true;
    }
    if (opName == "LUI") {
        if (tokens.size() < 3) return false;
        if (!ParseRegister(tokens[1], &inst->rd)) return false;
        std::string immStr = tokens[2];
        if (!immStr.empty() && immStr[0] == '#') immStr = immStr.substr(1);
        if (!ParseImm16(immStr, &inst->imm)) return false;
        inst->op = Op::Lui;
        return true;
    }
    if (opName == "ORI") {
        if (tokens.size() < 3) return false;
        if (!ParseRegister(tokens[1], &inst->rd)) return false;
        std::string immStr = tokens[2];
        if (!immStr.empty() && immStr[0] == '#') immStr = immStr.substr(1);
        if (!ParseImm16(immStr, &inst->imm)) return false;
        inst->op = Op::Ori;
        return true;
    }
    if (opName == "ANDI") {
        if (tokens.size() < 3) return false;
        if (!ParseRegister(tokens[1], &inst->rd)) return false;
        std::string immStr = tokens[2];
        if (!immStr.empty() && immStr[0] == '#') immStr = immStr.substr(1);
        if (!ParseImm16(immStr, &inst->imm)) return false;
        inst->op = Op::Andi;
        return true;
    }
    if (opName == "LW") {
        if (tokens.size() < 5) return false;
        if (!ParseRegister(tokens[1], &inst->rd)) return false;
        if (tokens[2] != "[") return false;
        if (!ParseRegister(tokens[3], &inst->rs)) return false;
        if (tokens[4] != "]") {
            if (!ParseOffset(tokens[4], reinterpret_cast<int8_t*>(&inst->imm))) return false;
            if (tokens.size() < 6 || tokens[5] != "]") return false;
        } else {
            inst->imm = 0;
        }
        inst->op = Op::Lw;
        return true;
    }
    if (opName == "SW") {
        if (tokens.size() < 5) return false;
        if (!ParseRegister(tokens[1], &inst->rd)) return false;
        if (tokens[2] != "[") return false;
        if (!ParseRegister(tokens[3], &inst->rs)) return false;
        if (tokens[4] != "]") {
            if (!ParseOffset(tokens[4], reinterpret_cast<int8_t*>(&inst->imm))) return false;
            if (tokens.size() < 6 || tokens[5] != "]") return false;
        } else {
            inst->imm = 0;
        }
        inst->op = Op::Sw;
        return true;
    }
    if (opName == "LB") {
        if (tokens.size() < 5) return false;
        if (!ParseRegister(tokens[1], &inst->rd)) return false;
        if (tokens[2] != "[") return false;
        if (!ParseRegister(tokens[3], &inst->rs)) return false;
        if (tokens[4] != "]") {
            if (!ParseOffset(tokens[4], reinterpret_cast<int8_t*>(&inst->imm))) return false;
            if (tokens.size() < 6 || tokens[5] != "]") return false;
        } else {
            inst->imm = 0;
        }
        inst->op = Op::Lb;
        return true;
    }
    if (opName == "LH") {
        if (tokens.size() < 5) return false;
        if (!ParseRegister(tokens[1], &inst->rd)) return false;
        if (tokens[2] != "[") return false;
        if (!ParseRegister(tokens[3], &inst->rs)) return false;
        if (tokens[4] != "]") {
            if (!ParseOffset(tokens[4], reinterpret_cast<int8_t*>(&inst->imm))) return false;
            if (tokens.size() < 6 || tokens[5] != "]") return false;
        } else {
            inst->imm = 0;
        }
        inst->op = Op::Lh;
        return true;
    }
    if (opName == "LBU") {
        if (tokens.size() < 5) return false;
        if (!ParseRegister(tokens[1], &inst->rd)) return false;
        if (tokens[2] != "[") return false;
        if (!ParseRegister(tokens[3], &inst->rs)) return false;
        if (tokens[4] != "]") {
            if (!ParseOffset(tokens[4], reinterpret_cast<int8_t*>(&inst->imm))) return false;
            if (tokens.size() < 6 || tokens[5] != "]") return false;
        } else {
            inst->imm = 0;
        }
        inst->op = Op::Lbu;
        return true;
    }
    if (opName == "LHU") {
        if (tokens.size() < 5) return false;
        if (!ParseRegister(tokens[1], &inst->rd)) return false;
        if (tokens[2] != "[") return false;
        if (!ParseRegister(tokens[3], &inst->rs)) return false;
        if (tokens[4] != "]") {
            if (!ParseOffset(tokens[4], reinterpret_cast<int8_t*>(&inst->imm))) return false;
            if (tokens.size() < 6 || tokens[5] != "]") return false;
        } else {
            inst->imm = 0;
        }
        inst->op = Op::Lhu;
        return true;
    }
    if (opName == "SB") {
        if (tokens.size() < 5) return false;
        if (!ParseRegister(tokens[1], &inst->rd)) return false;
        if (tokens[2] != "[") return false;
        if (!ParseRegister(tokens[3], &inst->rs)) return false;
        if (tokens[4] != "]") {
            if (!ParseOffset(tokens[4], reinterpret_cast<int8_t*>(&inst->imm))) return false;
            if (tokens.size() < 6 || tokens[5] != "]") return false;
        } else {
            inst->imm = 0;
        }
        inst->op = Op::Sb;
        return true;
    }
    if (opName == "SH") {
        if (tokens.size() < 5) return false;
        if (!ParseRegister(tokens[1], &inst->rd)) return false;
        if (tokens[2] != "[") return false;
        if (!ParseRegister(tokens[3], &inst->rs)) return false;
        if (tokens[4] != "]") {
            if (!ParseOffset(tokens[4], reinterpret_cast<int8_t*>(&inst->imm))) return false;
            if (tokens.size() < 6 || tokens[5] != "]") return false;
        } else {
            inst->imm = 0;
        }
        inst->op = Op::Sh;
        return true;
    }
    if (opName == "BEQ") {
        if (tokens.size() < 4) return false;
        if (!ParseRegister(tokens[1], &inst->rd)) return false;
        if (!ParseRegister(tokens[2], &inst->rs)) return false;
        inst->labelRef = tokens[3];
        inst->op = Op::Beq;
        return true;
    }
    if (opName == "ADD") {
        if (tokens.size() < 4) return false;
        if (!ParseRegister(tokens[1], &inst->rd)) return false;
        if (!ParseRegister(tokens[2], &inst->rs)) return false;
        uint8_t rt = 0;
        if (!ParseRegister(tokens[3], &rt)) return false;
        inst->imm = static_cast<int16_t>(rt);
        inst->op = Op::Add;
        return true;
    }
    if (opName == "SUB") {
        if (tokens.size() < 4) return false;
        if (!ParseRegister(tokens[1], &inst->rd)) return false;
        if (!ParseRegister(tokens[2], &inst->rs)) return false;
        uint8_t rt = 0;
        if (!ParseRegister(tokens[3], &rt)) return false;
        inst->imm = static_cast<int16_t>(rt);
        inst->op = Op::Sub;
        return true;
    }
    if (opName == "SRLI") {
        if (tokens.size() < 4) return false;
        if (!ParseRegister(tokens[1], &inst->rd)) return false;
        if (!ParseRegister(tokens[2], &inst->rs)) return false;
        std::string immStr = tokens[3];
        if (!immStr.empty() && immStr[0] == '#') immStr = immStr.substr(1);
        int shamt = 0;
        try {
            shamt = std::stoi(immStr);
        } catch (...) {
            return false;
        }
        if (shamt < 0 || shamt > 63) return false;
        inst->imm = static_cast<int16_t>(shamt);
        inst->op = Op::Srli;
        return true;
    }
    if (opName == "SLLI") {
        if (tokens.size() < 4) return false;
        if (!ParseRegister(tokens[1], &inst->rd)) return false;
        if (!ParseRegister(tokens[2], &inst->rs)) return false;
        std::string immStr = tokens[3];
        if (!immStr.empty() && immStr[0] == '#') immStr = immStr.substr(1);
        int shamt = 0;
        try {
            shamt = std::stoi(immStr);
        } catch (...) {
            return false;
        }
        if (shamt < 0 || shamt > 63) return false;
        inst->imm = static_cast<int16_t>(shamt);
        inst->op = Op::Slli;
        return true;
    }
    if (opName == "AND") {
        if (tokens.size() < 4) return false;
        if (!ParseRegister(tokens[1], &inst->rd)) return false;
        if (!ParseRegister(tokens[2], &inst->rs)) return false;
        uint8_t rt = 0;
        if (!ParseRegister(tokens[3], &rt)) return false;
        inst->imm = static_cast<int16_t>(rt);
        inst->op = Op::And;
        return true;
    }

    return false;
}

static uint32_t EncodeInstruction(const Instruction& inst, const std::map<std::string, int>& labels, int currentIdx) {
    uint32_t op = static_cast<uint32_t>(inst.op);
    
    switch (inst.op) {
        case Op::Nop:
            return op << 24;
        case Op::Halt:
            return op << 24;
        case Op::Lui:
        case Op::Ori:
        case Op::Andi:
            return (op << 24) | (static_cast<uint32_t>(inst.rd) << 16) | 
                   (static_cast<uint16_t>(inst.imm));
        case Op::Lw:
        case Op::Sw:
        case Op::Lb:
        case Op::Lh:
        case Op::Lbu:
        case Op::Lhu:
        case Op::Sb:
        case Op::Sh:
        case Op::Srli:
        case Op::Slli:
            return (op << 24) | (static_cast<uint32_t>(inst.rd) << 16) |
                   (static_cast<uint32_t>(inst.rs) << 8) |
                   static_cast<uint8_t>(inst.imm);
        case Op::Beq: {
            int targetIdx = currentIdx;
            auto it = labels.find(inst.labelRef);
            if (it != labels.end()) {
                targetIdx = it->second;
            }
            int8_t offset = static_cast<int8_t>(targetIdx - currentIdx);
            return (op << 24) | (static_cast<uint32_t>(inst.rd) << 16) |
                   (static_cast<uint32_t>(inst.rs) << 8) |
                   static_cast<uint8_t>(offset);
        }
        case Op::Add:
        case Op::Sub:
        case Op::And:
            return (op << 24) | (static_cast<uint32_t>(inst.rd) << 16) |
                   (static_cast<uint32_t>(inst.rs) << 8) |
                   static_cast<uint8_t>(inst.imm);
    }
    return 0;
}

static bool WriteRom(const std::string& path, const std::vector<uint32_t>& insts) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        std::cerr << "Error: Cannot open output file: " << path << "\n";
        return false;
    }
    for (uint32_t w : insts) {
        uint8_t b[4];
        b[0] = static_cast<uint8_t>(w & 0xff);
        b[1] = static_cast<uint8_t>((w >> 8) & 0xff);
        b[2] = static_cast<uint8_t>((w >> 16) & 0xff);
        b[3] = static_cast<uint8_t>((w >> 24) & 0xff);
        out.write(reinterpret_cast<const char*>(b), 4);
    }
    return out.good();
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input.asm> <output.rom>\n";
        return 1;
    }

    std::string inputPath = argv[1];
    std::string outputPath = argv[2];

    std::ifstream in(inputPath);
    if (!in.is_open()) {
        std::cerr << "Error: Cannot open input file: " << inputPath << "\n";
        return 1;
    }

    std::vector<Instruction> instructions;
    std::map<std::string, int> labels;
    std::string line;
    int lineNum = 0;
    int instIdx = 0;

    while (std::getline(in, line)) {
        lineNum++;
        Instruction inst = {};
        std::string label;

        if (!ParseLine(line, &inst, &label, lineNum)) {
            if (!label.empty()) {
                labels[label] = instIdx;
            }
            continue;
        }

        if (!label.empty()) {
            labels[label] = instIdx;
        }

        instructions.push_back(inst);
        instIdx++;
    }

    std::vector<uint32_t> rom;
    for (size_t i = 0; i < instructions.size(); i++) {
        uint32_t encoded = EncodeInstruction(instructions[i], labels, static_cast<int>(i));
        rom.push_back(encoded);
    }

    if (!WriteRom(outputPath, rom)) {
        return 1;
    }

    std::cout << "Assembled " << instructions.size() << " instructions to " << outputPath << "\n";
    return 0;
}