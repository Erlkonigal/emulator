#pragma once

#include <cstdint>
#include <string>

#include "emulator/utils/singleton.h"

class ExpressionParser : public Singleton<ExpressionParser> {
public:
  ExpressionParser();
  ~ExpressionParser() = default;
  uint64_t parse(const std::string &expr);

private:
  std::string mExpr;
  size_t mPos;

  enum class TokenType {
    End,
    Colon,
    Width,
    Number,
    Register,
    Plus,
    Minus,
    Multiply,
    Divide,
    LParen,
    RParen,
    LBracket,
    RBracket,
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
  uint64_t readMemory(const uint64_t &addr, const uint32_t &width);
  uint64_t getRegisterValue(const std::string &name);

  friend class Singleton<ExpressionParser>;
};
