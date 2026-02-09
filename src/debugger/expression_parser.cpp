#include "emulator/cpu/cpu.h"
#include "emulator/bus/bus.h"
#include "emulator/debugger/debugger.h" // For context if needed, but not strictly for this class unless we needed types
#include "emulator/debugger/expression_parser.h"

#include <cctype>
#include <algorithm>
#include <string>

// We need MemAccess/MemResponse which are in cpu.h
// TokenType is defined in header

ExpressionParser::ExpressionParser(ICpuExecutor* cpu, MemoryBus* bus, const std::string& expr) 
    : cpu_(cpu), bus_(bus), expr_(expr), pos_(0) {
    NextToken();
}

uint64_t ExpressionParser::Parse() {
    return ParseExpr();
}

void ExpressionParser::NextToken() {
    while (pos_ < expr_.size() && std::isspace(static_cast<unsigned char>(expr_[pos_]))) {
        pos_++;
    }
    
    if (pos_ >= expr_.size()) {
        curr_.type = TokenType::End;
        return;
    }

    char c = expr_[pos_];
    if (std::isdigit(static_cast<unsigned char>(c))) {
        // Parse number (hex or dec)
        size_t nextPos = pos_;
        uint64_t val = 0;
        if (pos_ + 2 <= expr_.size() && expr_[pos_] == '0' && (expr_[pos_+1] == 'x' || expr_[pos_+1] == 'X')) {
            // Hex
            std::string sub = expr_.substr(pos_);
            try {
                val = std::stoull(sub, &nextPos, 16);
            } catch (...) { val = 0; }
            pos_ += nextPos;
        } else {
            // Dec
            std::string sub = expr_.substr(pos_);
            try {
                val = std::stoull(sub, &nextPos, 10);
            } catch (...) { val = 0; }
            pos_ += nextPos;
        }
        curr_.type = TokenType::Number;
        curr_.value = val;
    } else if (c == '$') {
        // Register
        pos_++;
        size_t start = pos_;
        while (pos_ < expr_.size() && std::isalnum(static_cast<unsigned char>(expr_[pos_]))) {
            pos_++;
        }
        curr_.type = TokenType::Register;
        curr_.text = expr_.substr(start, pos_ - start);
    } else {
        pos_++;
        switch (c) {
            case '+': curr_.type = TokenType::Plus; break;
            case '-': curr_.type = TokenType::Minus; break;
            case '*': curr_.type = TokenType::Multiply; break;
            case '/': curr_.type = TokenType::Divide; break;
            case '(': curr_.type = TokenType::LParen; break;
            case ')': curr_.type = TokenType::RParen; break;
            case '[': curr_.type = TokenType::LBracket; break;
            case ']': curr_.type = TokenType::RBracket; break;
            default: curr_.type = TokenType::Error; break;
        }
    }
}

uint64_t ExpressionParser::ParseExpr() {
    uint64_t val = ParseTerm();
    while (curr_.type == TokenType::Plus || curr_.type == TokenType::Minus) {
        TokenType op = curr_.type;
        NextToken();
        uint64_t rhs = ParseTerm();
        if (op == TokenType::Plus) val += rhs;
        else val -= rhs;
    }
    return val;
}

uint64_t ExpressionParser::ParseTerm() {
    uint64_t val = ParseFactor();
    while (curr_.type == TokenType::Multiply || curr_.type == TokenType::Divide) {
        TokenType op = curr_.type;
        NextToken();
        uint64_t rhs = ParseFactor();
        if (op == TokenType::Multiply) val *= rhs;
        else if (rhs != 0) val /= rhs;
    }
    return val;
}

uint64_t ExpressionParser::ParseFactor() {
    if (curr_.type == TokenType::Number) {
        uint64_t val = curr_.value;
        NextToken();
        return val;
    }
    if (curr_.type == TokenType::Register) {
        uint64_t val = GetRegisterValue(curr_.text);
        NextToken();
        return val;
    }
    if (curr_.type == TokenType::LParen) {
        NextToken();
        uint64_t val = ParseExpr();
        if (curr_.type == TokenType::RParen) NextToken();
        return val;
    }
    if (curr_.type == TokenType::LBracket) {
        // Dereference
        NextToken();
        uint64_t addr = ParseExpr();
        if (curr_.type == TokenType::RBracket) NextToken();
        return ReadMemory(addr);
    }
    // Handle unary minus
    if (curr_.type == TokenType::Minus) {
        NextToken();
        return -ParseFactor();
    }
    if (curr_.type == TokenType::Plus) {
        NextToken();
        return ParseFactor();
    }
    return 0;
}

uint64_t ExpressionParser::ReadMemory(uint64_t addr) {
    if (!bus_) return 0;
    MemAccess access;
    access.Address = addr;
    access.Size = 4; // Default to 32-bit word read
    access.Type = MemAccessType::Read;
    auto resp = bus_->Read(access); 
    if (resp.Success) return resp.Data;
    return 0;
}

uint64_t ExpressionParser::GetRegisterValue(const std::string& name) {
    if (!cpu_) return 0;
    
    // Handle PC
    if (name == "pc" || name == "PC") {
        return cpu_->GetPc();
    }

    // Handle $rN or $N
    std::string numPart = name;
    if (numPart.size() > 1 && (numPart[0] == 'r' || numPart[0] == 'R')) {
        numPart = numPart.substr(1);
    }
    
    try {
        size_t idx = 0;
        uint32_t regId = std::stoul(numPart, &idx);
        return cpu_->GetRegister(regId);
    } catch (...) {
        return 0;
    }
}
