#include "emulator/app/terminal.h"

#include <curses.h>
#include <unistd.h>
#include <cstring>
#include <csignal>

static volatile sig_atomic_t g_resize_flag = 0;

static void sigwinch_handler(int sig) {
    (void)sig;
    g_resize_flag = 1;
}

Terminal::Terminal() {
    if (initscr() == nullptr) {
        return;
    }
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, nullptr);
    mouseinterval(10);
    set_escdelay(25);

    signal(SIGWINCH, sigwinch_handler);

    m_Height = LINES;
    SetupWindows();

    curs_set(1);
}

Terminal::~Terminal() {
    endwin();
}

void Terminal::SetupWindows() {
    int total_rows = LINES;
    int total_cols = COLS;

    if (status_win_) delwin(status_win_);
    if (vterm_border_) delwin(vterm_border_);
    if (debug_win_) delwin(debug_win_);

    status_win_ = newwin(1, total_cols, 0, 0);
    vterm_border_ = newwin(total_rows - 2, total_cols, 1, 0);
    debug_win_ = newwin(1, total_cols, total_rows - 1, 0);

    scrollok(status_win_, FALSE);
    scrollok(vterm_border_, FALSE);
    scrollok(debug_win_, FALSE);

    int vterm_rows = total_rows - 4;
    int vterm_cols = total_cols - 2;
    WINDOW* vterm_win = derwin(vterm_border_, vterm_rows, vterm_cols, 1, 1);

    vterm_mgr_.Initialize(vterm_rows, vterm_cols);
    vterm_mgr_.SetWindow(vterm_win);
    vterm_mgr_.SetFocus(true);

    vterm_mgr_.SetOnOutput([this](const char* s, size_t len) {
        if (on_input_) {
            on_input_(std::string(s, len));
        }
    });

    keypad(debug_win_, TRUE);
    keypad(vterm_win, TRUE);

    wtimeout(debug_win_, 10);
    wtimeout(vterm_win, 10);
}

void Terminal::RenderAll() {
    std::lock_guard<std::mutex> lock(m_Mutex);

    werase(status_win_);
    
    std::string status = m_CurrentStatus;
    if (focus_ == FocusPanel::VTERM) {
        status += " | [VTERM]";
    } else {
        status += " | [DEBUG] ";
    }

    int width = COLS;
    if (static_cast<int>(status.length()) < width) {
        status.append(width - status.length(), ' ');
    } else {
        status = status.substr(0, width);
    }

    mvwprintw(status_win_, 0, 0, "%s", status.c_str());
    wrefresh(status_win_);

    if (focus_ == FocusPanel::VTERM) {
        wborder(vterm_border_, '|', '|', '-', '-', '+', '+', '+', '+');
    } else {
        wborder(vterm_border_, ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ');
    }
    wrefresh(vterm_border_);

    werase(debug_win_);
    mvwprintw(debug_win_, 0, 0, "dbg> %s", m_DebugInput.c_str());
    
    if (focus_ == FocusPanel::DEBUG) {
        mvwchgat(debug_win_, 0, 5 + m_DebugCursorPos, 1, A_REVERSE, 0, nullptr);
    }

    if (focus_ == FocusPanel::VTERM) {
        wrefresh(debug_win_);
        vterm_mgr_.Render(true);
    } else {
        vterm_mgr_.Render(false);
        wmove(debug_win_, 0, 5 + m_DebugCursorPos);
        wrefresh(debug_win_);
    }
}

void Terminal::UpdateStatus(const std::string& status) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_CurrentStatus = status;
}

void Terminal::UpdateLastCommandSuccess(bool success) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_LastCmdSuccess = success;
}

void Terminal::PrintLog(const char* level, const char* msg) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    vterm_mgr_.PushLog(level, msg);
}

void Terminal::PrintChar(uint8_t ch) {
    std::lock_guard<std::mutex> lock(m_Mutex);
    vterm_mgr_.PushOutput(&ch, 1);
}

void Terminal::SwitchFocus() {
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (focus_ == FocusPanel::VTERM) {
        focus_ = FocusPanel::DEBUG;
        vterm_mgr_.HideCursor();
        vterm_mgr_.SetFocus(false);
    } else {
        focus_ = FocusPanel::VTERM;
        vterm_mgr_.ShowCursor();
        vterm_mgr_.SetFocus(true);
    }
}

void Terminal::HandleMouse(int y, int x) {
    (void)x;
    std::lock_guard<std::mutex> lock(m_Mutex);

    if (y >= 1 && y <= LINES - 2) {
        if (focus_ != FocusPanel::VTERM) {
            focus_ = FocusPanel::VTERM;
            vterm_mgr_.ShowCursor();
            vterm_mgr_.SetFocus(true);
        }
    } else if (y == LINES - 1) {
        if (focus_ != FocusPanel::DEBUG) {
            focus_ = FocusPanel::DEBUG;
            vterm_mgr_.HideCursor();
            vterm_mgr_.SetFocus(false);
        }
    }
}

void Terminal::RunInputLoop() {
    WINDOW* current_win = vterm_mgr_.GetWindow();

    while (!m_ShouldClose) {
        if (g_resize_flag) {
            g_resize_flag = 0;
            endwin();
            refresh();
            clear();
            m_Height = LINES;
            SetupWindows();
            current_win = vterm_mgr_.GetWindow();
        }

        int ch;
        if (focus_ == FocusPanel::VTERM) {
            ch = wgetch(current_win);
        } else {
            ch = wgetch(debug_win_);
        }

        if (ch == ERR) {
        } else if (ch == KEY_MOUSE) {
            MEVENT event;
            if (getmouse(&event) == OK) {
                HandleMouse(event.y, event.x);
            }
        } else if (ch == 23) {
            SwitchFocus();
        } else if (focus_ == FocusPanel::VTERM) {
            std::lock_guard<std::mutex> lock(m_Mutex);
            vterm_mgr_.ProcessInput(ch);
        } else {
            ProcessDebugInput(ch);
        }

        RenderAll();
    }
}

void Terminal::ProcessDebugInput(int ch) {
    switch (ch) {
        case '\n':
        case '\r':
            if (on_command_ && !m_DebugInput.empty()) {
                on_command_(m_DebugInput);
            }
            m_DebugInput.clear();
            m_DebugCursorPos = 0;
            break;
        case KEY_BACKSPACE:
        case 127:
        case '\b':
            if (m_DebugCursorPos > 0) {
                m_DebugInput.erase(m_DebugCursorPos - 1, 1);
                m_DebugCursorPos--;
            }
            break;
        case KEY_DC:
            if (m_DebugCursorPos < static_cast<int>(m_DebugInput.length())) {
                m_DebugInput.erase(m_DebugCursorPos, 1);
            }
            break;
        case KEY_LEFT:
            if (m_DebugCursorPos > 0) m_DebugCursorPos--;
            break;
        case KEY_RIGHT:
            if (m_DebugCursorPos < static_cast<int>(m_DebugInput.length())) m_DebugCursorPos++;
            break;
        case KEY_HOME:
            m_DebugCursorPos = 0;
            break;
        case KEY_END:
            m_DebugCursorPos = m_DebugInput.length();
            break;
        default:
            if (ch >= 32 && ch < 127) {
                m_DebugInput.insert(m_DebugCursorPos, 1, static_cast<char>(ch));
                m_DebugCursorPos++;
            }
            break;
    }
}

void Terminal::HandleResize() {
    endwin();
    refresh();
    clear();

    m_Height = LINES;
    SetupWindows();
}
