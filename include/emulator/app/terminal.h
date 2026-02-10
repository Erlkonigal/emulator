#pragma once

#include "emulator/app/vterm_manager.h"
#include <string>
#include <mutex>
#include <functional>
#include <atomic>
#include <queue>

enum class FocusPanel {
    VTERM,
    DEBUG
};

class Terminal {
public:
    Terminal();
    ~Terminal();

    void PrintLog(const char* level, const char* msg);
    void PrintChar(uint8_t ch);
    void UpdateStatus(const std::string& status);
    void UpdateLastCommandSuccess(bool success);

    using OnCommandCallback = std::function<void(const std::string&)>;
    void SetOnCommand(OnCommandCallback cb) { on_command_ = cb; }

    VTermManager& GetVTermManager() { return vterm_mgr_; }
    FocusPanel GetFocus() const { return focus_; }
    void SwitchFocus();

    void HandleMouse(int y, int x);
    void RunInputLoop();
    void Stop() { m_ShouldClose = true; }
    void SetOnInput(std::function<void(const std::string&)> callback) { on_input_ = callback; }

private:
    void RenderAll();
    void ProcessDebugInput(int ch);
    void SetupWindows();
    void HandleResize();

    std::mutex m_Mutex;

    WINDOW* status_win_ = nullptr;
    WINDOW* vterm_border_ = nullptr;
    WINDOW* debug_win_ = nullptr;

    VTermManager vterm_mgr_;
    FocusPanel focus_ = FocusPanel::VTERM;

    std::string m_CurrentStatus;
    bool m_LastCmdSuccess = true;

    std::string m_DebugInput;
    int m_DebugCursorPos = 0;
    int m_Height = 0;

    OnCommandCallback on_command_;
    std::atomic<bool> m_ShouldClose{false};
    
    std::function<void(const std::string&)> on_input_;
};
