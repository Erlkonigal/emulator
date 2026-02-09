#ifndef DEBUGGER_EXPRESSION_PARSER_H
#define DEBUGGER_EXPRESSION_PARSER_H

#include <string>
#include <cstdint>

class ICpuExecutor;
class MemoryBus;

class ExpressionParser {
public:
    ExpressionParser(ICpuExecutor* cpu, MemoryBus* bus, const std::string& expr);
    uint64_t Parse();

private:
    ICpuExecutor* cpu_;
    MemoryBus* bus_;
    std::string expr_;
    size_t pos_;
    
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
        uint64_t value; // For numbers
        std::string text; // For registers
    };

    Token curr_;

    void NextToken();
    uint64_t ParseExpr();
    uint64_t ParseTerm();
    uint64_t ParseFactor();
    uint64_t ReadMemory(uint64_t addr);
    uint64_t GetRegisterValue(const std::string& name);
};

#endif
