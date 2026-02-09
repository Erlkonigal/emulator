#include "emulator/app/terminal.h"

#include <curses.h>
#include <unistd.h>
#include <poll.h>
#include <cstring>

Terminal::Terminal() {
    // Initialize ncurses
    if (initscr() == nullptr) {
        // Fallback or error handling if needed, though initscr usually exits on failure
        return;
    }
    cbreak();               // Line buffering disabled
    noecho();               // Don't echo while we do getch
    keypad(stdscr, TRUE);   // Enable special keys (F1, arrows, etc.)
    scrollok(stdscr, TRUE); // Enable scrolling
    setscrreg(1, LINES - 2); // Set scrolling region to allow scrolling (excluding status bar and command bar)
    timeout(0);             // Non-blocking reads by default (we manage blocking via poll)
    
    m_Height = LINES;
    m_LogY = 1; // Start logging below status bar

    m_Running = true;
    m_DrawingThread = std::thread([this]() {
        while (m_Running) {
            DrawGui();
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    });
}

Terminal::~Terminal() {
    m_Running = false;
    if (m_DrawingThread.joinable()) {
        m_DrawingThread.join();
    }
    endwin();
}

void Terminal::SetInputMode(bool enable) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_InputMode = enable;
}

bool Terminal::IsInputMode() const {
    return m_InputMode;
}

void Terminal::DrawGui() {
    std::lock_guard<std::mutex> lock(m_Mutex);
    // 0. Erase all
    setscrreg(0, LINES - 1);
    clear();

    // 1. Draw Status Bar
    move(0, 0);
    attron(A_REVERSE);
    
    int width = COLS;
    std::string line = m_CurrentStatus;
    if (line.length() < static_cast<size_t>(width)) {
        line.append(width - line.length(), ' ');
    } else {
        line = line.substr(0, width);
    }
    
    printw("%s", line.c_str());
    attroff(A_REVERSE);

    // 2. Draw Log Area
    setscrreg(1, LINES - 2);
    move(1, 0);
    for (const auto& log : m_LogHistory) {
        printw("%s", log.c_str());
    }

    // 3. Draw Prompt or Input Mode Prompt
    setscrreg(0, LINES - 1);
    int promptY = LINES - 1;
    if (m_InputMode) {
        getyx(stdscr, m_LogY, m_LogX); // Save current cursor position in log area
        // In input mode, show a centered prompt in reverse video
        attron(A_REVERSE);
        int padding = (COLS - static_cast<int>(m_InputModePrompt.length()) - 2) / 2;
        if (padding < 0) padding = 0;
        move(promptY, padding);
        printw(" %s ", m_InputModePrompt.c_str());
        attroff(A_REVERSE);
        move(m_LogY, m_LogX); // Restore cursor to log area
    } else {
        move(promptY, 0);
        // Normal prompt mode
        printw("%s %s", m_DebugModePrompt.c_str(), m_CurrentInput.c_str());
        // 3. Restore Cursor
        move(promptY, 5 + m_CursorPos);
    }
    
    refresh();
}

void Terminal::UpdateStatus(const std::string& status) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_CurrentStatus = status;
}

bool Terminal::ReadChar(Key& key, char& out, int timeoutMs) {
    // Use poll to wait for input without holding the ncurses lock
    struct pollfd pfd;
    pfd.fd = STDIN_FILENO;
    pfd.events = POLLIN;

    int ret = poll(&pfd, 1, timeoutMs);

    // Handle input or signal interruption (resize)
    if ((ret > 0 && (pfd.revents & POLLIN)) || (ret < 0 && errno == EINTR)) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        
        int ch = getch();
        
        if (ch == ERR) {
            return false;
        }

        key = Key::None;
        out = 0;

        switch (ch) {
            case KEY_UP: key = Key::Up; break;
            case KEY_DOWN: key = Key::Down; break;
            case KEY_LEFT: key = Key::Left; break;
            case KEY_RIGHT: key = Key::Right; break;
            case KEY_HOME: key = Key::Home; break;
            case KEY_END: key = Key::End; break;
            case KEY_BACKSPACE: 
            case 127: 
            case '\b':
                key = Key::Backspace; 
                break;
            case KEY_DC:
                key = Key::Delete;
                break;
            case KEY_ENTER:
            case '\n': 
            case '\r':
                key = Key::Enter;
                out = '\n';
                break;
            case 4: // Ctrl+D (EOF)
                key = Key::Eof;
                break;
            case KEY_RESIZE:
                m_Height = LINES;
                key = Key::None; 
                break;
            default:
                if (ch >= 32 && ch < 127) {
                    key = Key::Char;
                    out = static_cast<char>(ch);
                }
                break;
        }
        return key != Key::None;
    }
    
    return false;
}

void Terminal::PrintLog(const char* msg) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    
    // Store in history
    m_LogHistory.push_back(msg);
    if (m_LogHistory.size() > kMaxLogHistory) {
        m_LogHistory.pop_front();
    }
}

void Terminal::PrintChar(char ch) {
    std::lock_guard<std::mutex> lock(m_Mutex);

    m_LogHistory.back().push_back(ch);
}

void Terminal::RefreshLine(const std::string& inputBuffer, size_t cursorPos) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    
    m_CurrentInput = inputBuffer;
    m_CursorPos = cursorPos;
    
    // We want to redraw everything including status bar if possible, 
    // but RefreshLine is called frequently on typing.
    // However, if we just draw prompt, we might miss pending status update.
}
