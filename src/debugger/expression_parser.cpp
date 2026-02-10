#include "emulator/cpu/cpu.h"
#include "emulator/bus/bus.h"
#include "emulator/debugger/debugger.h"
#include "emulator/debugger/expression_parser.h"

#include <cctype>
#include <algorithm>
#include <string>

ExpressionParser::ExpressionParser(ICpuExecutor* cpu, MemoryBus* bus, const std::string& expr) 
    : mCpu(cpu), mBus(bus), mExpr(expr), mPos(0) {
    nextToken();
}

uint64_t ExpressionParser::parse() {
    return parseExpr();
}

void ExpressionParser::nextToken() {
    while (mPos < mExpr.size() && std::isspace(static_cast<unsigned char>(mExpr[mPos]))) {
        mPos++;
    }
    
    if (mPos >= mExpr.size()) {
        mCurr.type = TokenType::End;
        return;
    }

    char c = mExpr[mPos];
    if (std::isdigit(static_cast<unsigned char>(c))) {
        size_t nextPos = mPos;
        uint64_t val = 0;
        if (mPos + 2 <= mExpr.size() && mExpr[mPos] == '0' && (mExpr[mPos+1] == 'x' || mExpr[mPos+1] == 'X')) {
            std::string sub = mExpr.substr(mPos);
            try {
                val = std::stoull(sub, &nextPos, 16);
            } catch (...) { val = 0; }
            mPos += nextPos;
        } else {
            std::string sub = mExpr.substr(mPos);
            try {
                val = std::stoull(sub, &nextPos, 10);
            } catch (...) { val = 0; }
            mPos += nextPos;
        }
        mCurr.type = TokenType::Number;
        mCurr.value = val;
    } else if (c == '$') {
        mPos++;
        size_t start = mPos;
        while (mPos < mExpr.size() && std::isalnum(static_cast<unsigned char>(mExpr[mPos]))) {
            mPos++;
        }
        mCurr.type = TokenType::Register;
        mCurr.text = mExpr.substr(start, mPos - start);
    } else {
        mPos++;
        switch (c) {
            case '+': mCurr.type = TokenType::Plus; break;
            case '-': mCurr.type = TokenType::Minus; break;
            case '*': mCurr.type = TokenType::Multiply; break;
            case '/': mCurr.type = TokenType::Divide; break;
            case '(': mCurr.type = TokenType::LParen; break;
            case ')': mCurr.type = TokenType::RParen; break;
            case '[': mCurr.type = TokenType::LBracket; break;
            case ']': mCurr.type = TokenType::RBracket; break;
            default: mCurr.type = TokenType::Error; break;
        }
    }
}

uint64_t ExpressionParser::parseExpr() {
    uint64_t val = parseTerm();
    while (mCurr.type == TokenType::Plus || mCurr.type == TokenType::Minus) {
        TokenType op = mCurr.type;
        nextToken();
        uint64_t rhs = parseTerm();
        if (op == TokenType::Plus) val += rhs;
        else val -= rhs;
    }
    return val;
}

uint64_t ExpressionParser::parseTerm() {
    uint64_t val = parseFactor();
    while (mCurr.type == TokenType::Multiply || mCurr.type == TokenType::Divide) {
        TokenType op = mCurr.type;
        nextToken();
        uint64_t rhs = parseFactor();
        if (op == TokenType::Multiply) val *= rhs;
        else if (rhs != 0) val /= rhs;
    }
    return val;
}

uint64_t ExpressionParser::parseFactor() {
    if (mCurr.type == TokenType::Number) {
        uint64_t val = mCurr.value;
        nextToken();
        return val;
    }
    if (mCurr.type == TokenType::Register) {
        uint64_t val = getRegisterValue(mCurr.text);
        nextToken();
        return val;
    }
    if (mCurr.type == TokenType::LParen) {
        nextToken();
        uint64_t val = parseExpr();
        if (mCurr.type == TokenType::RParen) nextToken();
        return val;
    }
    if (mCurr.type == TokenType::LBracket) {
        nextToken();
        uint64_t addr = parseExpr();
        if (mCurr.type == TokenType::RBracket) nextToken();
        return readMemory(addr);
    }
    if (mCurr.type == TokenType::Minus) {
        nextToken();
        return -parseFactor();
    }
    if (mCurr.type == TokenType::Plus) {
        nextToken();
        return parseFactor();
    }
    return 0;
}

uint64_t ExpressionParser::readMemory(uint64_t addr) {
    if (!mBus) return 0;
    MemAccess access;
    access.address = addr;
    access.size = 4;
    access.type = MemAccessType::Read;
    auto resp = mBus->read(access); 
    if (resp.success) return resp.data;
    return 0;
}

uint64_t ExpressionParser::getRegisterValue(const std::string& name) {
    if (!mCpu) return 0;
    
    if (name == "pc" || name == "PC") {
        return mCpu->getPc();
    }

    std::string numPart = name;
    if (numPart.size() > 1 && (numPart[0] == 'r' || numPart[0] == 'R')) {
        numPart = numPart.substr(1);
    }
    
    try {
        size_t idx = 0;
        uint32_t regId = std::stoul(numPart, &idx);
        return mCpu->getRegister(regId);
    } catch (...) {
        return 0;
    }
}
