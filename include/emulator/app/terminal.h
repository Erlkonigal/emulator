#pragma once

#include <string>
#include <mutex>
#include <vector>
#include <deque>
#include <chrono>
#include <atomic>
#include <thread>

enum class Key {
    None,
    Char,
    Backspace,
    Delete,
    Enter,
    Up,
    Down,
    Left,
    Right,
    Home,
    End,
    Eof
};

class Terminal {
public:
    Terminal();
    ~Terminal();

    // Input
    // Returns true if character read, false if timeout/no input
    // Uses ncurses getch() internally
    bool ReadChar(Key& key, char& ch, int timeoutMs = 50);

    // Output
    // Prints a log message atomically while preserving the prompt line
    // Thread-safe
    void PrintLog(const char* msg);

    // Prints a single character atomically while preserving the prompt line
    // Does NOT add newline - useful for streaming output
    // Thread-safe
    void PrintChar(char ch);

    // Updates the current prompt line (user typing)
    // Refreshes the "dbg> [buffer]" line
    // cursorPos: 0-based index in inputBuffer where cursor should be placed
    void RefreshLine(const std::string& inputBuffer, size_t cursorPos);

    // Updates the status bar at the top of the screen
    void UpdateStatus(const std::string& status);

    // Sets input mode for UART input
    // When enabled, shows a input prompt instead of "dbg>"
    void SetInputMode(bool enable);
    bool IsInputMode() const;

private:
    std::mutex m_Mutex;
    std::atomic<bool> m_Running;
    std::thread m_DrawingThread;
    // Cache the current user input so PrintLog can redraw it
    std::string m_CurrentInput;
    size_t m_CursorPos = 0;
    
    // Cache the current status text
    std::string m_CurrentStatus;

    // Input mode state and prompt
    bool m_InputMode = false;
    std::string m_InputModePrompt = "Press Ctrl+D to exit input mode";
    std::string m_DebugModePrompt = "dbg>";

    // Track cursor for logging area
    int m_LogY = 0;
    int m_LogX = 0;
    
    // Previous terminal height for resize handling
    int m_Height = 0;
    
    // Log history for repainting on resize
    std::deque<std::string> m_LogHistory;
    const size_t kMaxLogHistory = 1000;

    // Helper to draw status bar and prompt
    void DrawGui();
};
