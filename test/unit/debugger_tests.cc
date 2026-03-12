#include "framework/test_framework.h"

#include <cstdint>
#include <string>

#include "emulator/commit/shadow_arch.h"
#include "emulator/debug/breakpoint.h"
#include "emulator/debug/debugger.h"
#include "emulator/generated/hardware_config.h"

using emulator::BreakPointController;
using emulator::Debugger;
using emulator::ShadowArch;

namespace {

bool gPauseCalled = false;
bool gRunCalled = false;
uint32_t gStepCount = 0;

void ResetTestFlags() {
    gPauseCalled = false;
    gRunCalled = false;
    gStepCount = 0;
}

}

TEST(debugger_cmd_help_shows_commands) {
    Debugger& dbg = Debugger::getInstance();
    dbg.reset();
    
    dbg.processCommand("help");
}

TEST(debugger_cmd_regs_shows_all_registers) {
    Debugger& dbg = Debugger::getInstance();
    dbg.reset();
    
    dbg.processCommand("regs");
}

TEST(debugger_cmd_mem_valid_args) {
    Debugger& dbg = Debugger::getInstance();
    dbg.reset();
    
    dbg.processCommand("mem 0x80000000 16");
}

TEST(debugger_cmd_mem_missing_args) {
    Debugger& dbg = Debugger::getInstance();
    dbg.reset();
    
    dbg.processCommand("mem");
}

TEST(debugger_cmd_mem_missing_len) {
    Debugger& dbg = Debugger::getInstance();
    dbg.reset();
    
    dbg.processCommand("mem 0x80000000");
}

TEST(debugger_cmd_mem_invalid_address) {
    Debugger& dbg = Debugger::getInstance();
    dbg.reset();
    
    dbg.processCommand("mem invalid 10");
}

TEST(debugger_cmd_bp_add) {
    Debugger& dbg = Debugger::getInstance();
    auto& bpCtrl = BreakPointController::getInstance();
    dbg.reset();
    bpCtrl.reset();
    
    dbg.processCommand("bp add 0x80000000");
}

TEST(debugger_cmd_bp_del) {
    Debugger& dbg = Debugger::getInstance();
    auto& bpCtrl = BreakPointController::getInstance();
    dbg.reset();
    bpCtrl.reset();
    
    dbg.processCommand("bp add 0x80000100");
    dbg.processCommand("bp del 0x80000100");
}

TEST(debugger_cmd_bp_list) {
    Debugger& dbg = Debugger::getInstance();
    auto& bpCtrl = BreakPointController::getInstance();
    dbg.reset();
    bpCtrl.reset();
    
    dbg.processCommand("bp add 0x80000000");
    dbg.processCommand("bp add 0x80000004");
    dbg.processCommand("bp list");
}

TEST(debugger_cmd_bp_list_empty) {
    Debugger& dbg = Debugger::getInstance();
    auto& bpCtrl = BreakPointController::getInstance();
    dbg.reset();
    bpCtrl.reset();
    
    dbg.processCommand("bp list");
}

TEST(debugger_cmd_bp_invalid_action) {
    Debugger& dbg = Debugger::getInstance();
    dbg.reset();
    
    dbg.processCommand("bp invalid");
}

TEST(debugger_cmd_bp_add_invalid_addr) {
    Debugger& dbg = Debugger::getInstance();
    dbg.reset();
    
    dbg.processCommand("bp add invalid");
}

TEST(debugger_cmd_log_trace) {
    Debugger& dbg = Debugger::getInstance();
    dbg.reset();
    
    dbg.processCommand("log trace");
}

TEST(debugger_cmd_log_debug) {
    Debugger& dbg = Debugger::getInstance();
    dbg.reset();
    
    dbg.processCommand("log debug");
}

TEST(debugger_cmd_log_info) {
    Debugger& dbg = Debugger::getInstance();
    dbg.reset();
    
    dbg.processCommand("log info");
}

TEST(debugger_cmd_log_warn) {
    Debugger& dbg = Debugger::getInstance();
    dbg.reset();
    
    dbg.processCommand("log warn");
}

TEST(debugger_cmd_log_error) {
    Debugger& dbg = Debugger::getInstance();
    dbg.reset();
    
    dbg.processCommand("log error");
}

TEST(debugger_cmd_log_invalid) {
    Debugger& dbg = Debugger::getInstance();
    dbg.reset();
    
    dbg.processCommand("log invalid");
}

TEST(debugger_cmd_eval_hex) {
    Debugger& dbg = Debugger::getInstance();
    dbg.reset();
    
    dbg.processCommand("eval 0x1234");
}

TEST(debugger_cmd_eval_decimal) {
    Debugger& dbg = Debugger::getInstance();
    dbg.reset();
    
    dbg.processCommand("eval 12345");
}

TEST(debugger_cmd_eval_invalid) {
    Debugger& dbg = Debugger::getInstance();
    dbg.reset();
    
    dbg.processCommand("eval notanumber");
}

TEST(debugger_cmd_eval_empty) {
    Debugger& dbg = Debugger::getInstance();
    dbg.reset();
    
    dbg.processCommand("eval");
}

TEST(debugger_unknown_command) {
    Debugger& dbg = Debugger::getInstance();
    dbg.reset();
    
    dbg.processCommand("unknowncommand");
}

TEST(debugger_empty_command) {
    Debugger& dbg = Debugger::getInstance();
    dbg.reset();
    
    dbg.processCommand("");
}

TEST(debugger_whitespace_command) {
    Debugger& dbg = Debugger::getInstance();
    dbg.reset();
    
    dbg.processCommand("   ");
}

TEST(debugger_cmd_quit) {
    Debugger& dbg = Debugger::getInstance();
    dbg.reset();
    
    dbg.processCommand("quit");
}

TEST(debugger_cmd_exit) {
    Debugger& dbg = Debugger::getInstance();
    dbg.reset();
    
    dbg.processCommand("exit");
}

TEST(debugger_cmd_pause) {
    Debugger& dbg = Debugger::getInstance();
    dbg.reset();
    ResetTestFlags();
    
    dbg.setControlCallbacks(
        nullptr,
        nullptr,
        []() { gPauseCalled = true; return true; }
    );
    
    dbg.processCommand("pause");
    EXPECT_TRUE(gPauseCalled);
}

TEST(debugger_cmd_step_default) {
    Debugger& dbg = Debugger::getInstance();
    dbg.reset();
    ResetTestFlags();
    
    dbg.setControlCallbacks(
        nullptr,
        [](uint32_t count) { gStepCount = count; return true; },
        nullptr
    );
    
    dbg.processCommand("step");
    EXPECT_EQ(gStepCount, 1u);
}

TEST(debugger_cmd_step_with_count) {
    Debugger& dbg = Debugger::getInstance();
    dbg.reset();
    ResetTestFlags();
    
    dbg.setControlCallbacks(
        nullptr,
        [](uint32_t count) { gStepCount = count; return true; },
        nullptr
    );
    
    dbg.processCommand("step 10");
    EXPECT_EQ(gStepCount, 10u);
}

TEST(debugger_cmd_step_large_count) {
    Debugger& dbg = Debugger::getInstance();
    dbg.reset();
    ResetTestFlags();
    
    dbg.setControlCallbacks(
        nullptr,
        [](uint32_t count) { gStepCount = count; return true; },
        nullptr
    );
    
    dbg.processCommand("step 1000");
    EXPECT_EQ(gStepCount, 1000u);
}

TEST(debugger_cmd_run_with_callback) {
    Debugger& dbg = Debugger::getInstance();
    dbg.reset();
    ResetTestFlags();
    
    dbg.setControlCallbacks(
        []() { gRunCalled = true; return true; },
        nullptr,
        nullptr
    );
    
    dbg.processCommand("run");
    EXPECT_TRUE(gRunCalled);
}

TEST(debugger_multiple_bp_operations) {
    Debugger& dbg = Debugger::getInstance();
    auto& bpCtrl = BreakPointController::getInstance();
    dbg.reset();
    bpCtrl.reset();
    
    dbg.processCommand("bp add 0x80000000");
    dbg.processCommand("bp add 0x80000004");
    dbg.processCommand("bp add 0x80000008");
    dbg.processCommand("bp list");
    dbg.processCommand("bp del 0x80000004");
    dbg.processCommand("bp list");
    dbg.processCommand("bp del 0x80000000");
    dbg.processCommand("bp del 0x80000008");
}

TEST(debugger_mem_decimal_address) {
    Debugger& dbg = Debugger::getInstance();
    dbg.reset();
    
    dbg.processCommand("mem 2147483648 4");
}

TEST(debugger_mem_various_lengths) {
    Debugger& dbg = Debugger::getInstance();
    dbg.reset();
    
    dbg.processCommand("mem 0x80000000 1");
    dbg.processCommand("mem 0x80000000 16");
    dbg.processCommand("mem 0x80000000 32");
    dbg.processCommand("mem 0x80000000 64");
}

TEST(debugger_bp_functionality) {
    Debugger& dbg = Debugger::getInstance();
    auto& bpCtrl = BreakPointController::getInstance();
    dbg.reset();
    bpCtrl.reset();
    
    dbg.processCommand("bp add 0x80000000");
    EXPECT_TRUE(bpCtrl.contains(0x80000000));
    
    dbg.processCommand("bp del 0x80000000");
    EXPECT_TRUE(!bpCtrl.contains(0x80000000));
}