#pragma once

#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "emulator/utils/singleton.h"

class BreakPointController : public Singleton<BreakPointController> {
public:
  void list(std::vector<std::string> &out) const;
  void add(const uint64_t &addr);
  void remove(const uint64_t &addr);
  bool contains(const uint64_t &addr) const;

private:
  std::set<uint64_t> mBreakPoints;

  friend class Singleton<BreakPointController>;
};