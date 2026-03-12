#include "emulator/debug/breakpoint.h"

#include <format>

namespace emulator {

void BreakPointController::list(std::vector<std::string> &out) const {
  size_t index = 0;
  for (const auto &bp : mBreakPoints) {
    out.push_back(std::format("{}:\t0x{:x}", index++, bp));
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

} // namespace emulator