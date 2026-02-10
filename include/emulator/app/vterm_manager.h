#pragma once

#include <vterm.h>
#include <curses.h>
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <atomic>
#include <functional>

class VTermManager {
public:
    VTermManager();
    ~VTermManager();

    void Initialize(int rows, int cols);
    void Shutdown();
    void Resize(int rows, int cols);

    void PushOutput(const uint8_t* data, size_t len);
    void PushLog(const char* level, const char* msg);
    void ProcessInput(int ch);
    void Render(bool force_cursor = false);
    bool IsDirty() const { return dirty_.load(); }
    void ClearDirty() { dirty_.store(false); }

    void SetFocus(bool focus);
    bool HasFocus() const { return has_focus_; }

    void ShowCursor();
    void HideCursor();
    void DrawBorder(bool focused);

    void SetWindow(WINDOW* win) { vterm_win_ = win; }
    WINDOW* GetWindow() { return vterm_win_; }

    void ForceRefresh();
    int GetCursorRow() const { return cursor_row_; }
    int GetCursorCol() const { return cursor_col_; }
    bool IsCursorVisible() const { return cursor_visible_; }

    void SetOnOutput(std::function<void(const char*, size_t)> callback) { on_output_ = callback; }

private:
    VTerm* vterm_ = nullptr;
    VTermScreen* screen_ = nullptr;
    VTermState* state_ = nullptr;
    WINDOW* vterm_win_ = nullptr;
    WINDOW* border_win_ = nullptr;
    
public:
    bool has_focus_ = false;
    bool cursor_visible_ = true;
    int cursor_row_ = 0;
    int cursor_col_ = 0;
    std::atomic<bool> dirty_{false};
    
    std::function<void(const char*, size_t)> on_output_;
    
private:
    int rows_ = 0;
    int cols_ = 0;
};
