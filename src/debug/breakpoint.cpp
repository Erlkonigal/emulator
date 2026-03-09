#include "emulator/debug/breakpoint.h"

void BreakPointController::list(std::vector<std::string> &out) const {
  size_t index = 0;
  for (const auto &bp : mBreakPoints) {
    out.push_back(std::to_string(index++) + ":\t" + std::to_string(bp));
  }
}

void BreakPointController::add(const uint64_t &addr) {
  mBreakPoints.insert(addr);
}

void BreakPointController::remove(const uint64_t &addr) {
  mBreakPoints.erase(addr);
}

bool BreakPointController::contains(const uint64_t &addr) const {
  return mBreakPoints.find(addr) != mBreakPoints.end();
}

void BreakPointController::reset() { mBreakPoints.clear(); }