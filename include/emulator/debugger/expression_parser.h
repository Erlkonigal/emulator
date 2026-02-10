#ifndef DEBUGGER_EXPRESSION_PARSER_H
#define DEBUGGER_EXPRESSION_PARSER_H

#include <string>
#include <cstdint>

class ICpuExecutor;
class MemoryBus;

class ExpressionParser {
public:
    ExpressionParser(ICpuExecutor* cpu, MemoryBus* bus, const std::string& expr);
    uint64_t parse();

private:
    ICpuExecutor* mCpu;
    MemoryBus* mBus;
    std::string mExpr;
    size_t mPos;
    
    enum class TokenType {
        End,
        Number,
        Register,
        Plus, Minus, Multiply, Divide,
        LParen, RParen,
        LBracket, RBracket,
        Error
    };

    struct Token {
        TokenType type;
        uint64_t value;
        std::string text;
    };

    Token mCurr;

    void nextToken();
    uint64_t parseExpr();
    uint64_t parseTerm();
    uint64_t parseFactor();
    uint64_t readMemory(uint64_t addr);
    uint64_t getRegisterValue(const std::string& name);
};

#endif
