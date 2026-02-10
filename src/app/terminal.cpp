#include "emulator/app/terminal.h"

#include <curses.h>
#include <unistd.h>
#include <cstring>
#include <csignal>

static volatile sig_atomic_t gResizeFlag = 0;

static void sigwinchHandler(int sig) {
    (void)sig;
    gResizeFlag = 1;
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

    signal(SIGWINCH, sigwinchHandler);

    mHeight = LINES;
    setupWindows();

    curs_set(1);
}

Terminal::~Terminal() {
    endwin();
}

void Terminal::setupWindows() {
    int totalRows = LINES;
    int totalCols = COLS;

    if (mStatusWin) delwin(mStatusWin);
    if (mVtermBorder) delwin(mVtermBorder);
    if (mDebugWin) delwin(mDebugWin);

    mStatusWin = newwin(1, totalCols, 0, 0);
    mVtermBorder = newwin(totalRows - 2, totalCols, 1, 0);
    mDebugWin = newwin(1, totalCols, totalRows - 1, 0);

    scrollok(mStatusWin, FALSE);
    scrollok(mVtermBorder, FALSE);
    scrollok(mDebugWin, FALSE);

    int vtermRows = totalRows - 4;
    int vtermCols = totalCols - 2;
    WINDOW* vtermWin = derwin(mVtermBorder, vtermRows, vtermCols, 1, 1);

    mVTermManager.initialize(vtermRows, vtermCols);
    mVTermManager.setWindow(vtermWin);
    mVTermManager.setFocus(true);

    mVTermManager.setOnOutput([this](const char* s, size_t len) {
        if (mOnInput) {
            mOnInput(std::string(s, len));
        }
    });

    keypad(mDebugWin, TRUE);
    keypad(vtermWin, TRUE);

    wtimeout(mDebugWin, 10);
    wtimeout(vtermWin, 10);
}

void Terminal::renderAll() {
    std::lock_guard<std::mutex> lock(mMutex);

    werase(mStatusWin);
    
    std::string status = mCurrentStatus;
    if (mFocus == FocusPanel::VTERM) {
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

    mvwprintw(mStatusWin, 0, 0, "%s", status.c_str());
    wrefresh(mStatusWin);

    if (mFocus == FocusPanel::VTERM) {
        wborder(mVtermBorder, '|', '|', '-', '-', '+', '+', '+', '+');
    } else {
        wborder(mVtermBorder, ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ');
    }
    wrefresh(mVtermBorder);

    werase(mDebugWin);
    mvwprintw(mDebugWin, 0, 0, "dbg> %s", mDebugInput.c_str());
    
    if (mFocus == FocusPanel::DEBUG) {
        mvwchgat(mDebugWin, 0, 5 + mDebugCursorPos, 1, A_REVERSE, 0, nullptr);
    }

    if (mFocus == FocusPanel::VTERM) {
        wrefresh(mDebugWin);
        mVTermManager.render(true);
    } else {
        mVTermManager.render(false);
        wmove(mDebugWin, 0, 5 + mDebugCursorPos);
        wrefresh(mDebugWin);
    }
}

void Terminal::updateStatus(const std::string& status) {
    std::lock_guard<std::mutex> lock(mMutex);
    mCurrentStatus = status;
}

void Terminal::printLog(const char* msg) {
    std::lock_guard<std::mutex> lock(mMutex);
    mVTermManager.pushLog(msg);
}

void Terminal::printChar(uint8_t ch) {
    std::lock_guard<std::mutex> lock(mMutex);
    mVTermManager.pushChar(ch);
}

void Terminal::switchFocus() {
    std::lock_guard<std::mutex> lock(mMutex);

    if (mFocus == FocusPanel::VTERM) {
        mFocus = FocusPanel::DEBUG;
        mVTermManager.hideCursor();
        mVTermManager.setFocus(false);
    } else {
        mFocus = FocusPanel::VTERM;
        mVTermManager.showCursor();
        mVTermManager.setFocus(true);
    }
}

void Terminal::handleMouse(int y, int x) {
    (void)x;
    std::lock_guard<std::mutex> lock(mMutex);

    if (y >= 1 && y <= LINES - 2) {
        if (mFocus != FocusPanel::VTERM) {
            mFocus = FocusPanel::VTERM;
            mVTermManager.showCursor();
            mVTermManager.setFocus(true);
        }
    } else if (y == LINES - 1) {
        if (mFocus != FocusPanel::DEBUG) {
            mFocus = FocusPanel::DEBUG;
            mVTermManager.hideCursor();
            mVTermManager.setFocus(false);
        }
    }
}

void Terminal::runInputLoop() {
    WINDOW* currentWin = mVTermManager.getWindow();

    while (!mShouldClose) {
        if (gResizeFlag) {
            gResizeFlag = 0;
            endwin();
            refresh();
            clear();
            mHeight = LINES;
            setupWindows();
            currentWin = mVTermManager.getWindow();
        }

        int ch;
        if (mFocus == FocusPanel::VTERM) {
            ch = wgetch(currentWin);
        } else {
            ch = wgetch(mDebugWin);
        }

        if (ch == ERR) {
        } else if (ch == KEY_MOUSE) {
            MEVENT event;
            if (getmouse(&event) == OK) {
                handleMouse(event.y, event.x);
            }
        } else if (ch == 23) {
            switchFocus();
        } else if (mFocus == FocusPanel::VTERM) {
            std::lock_guard<std::mutex> lock(mMutex);
            mVTermManager.processInput(ch);
        } else {
            processDebugInput(ch);
        }

        renderAll();
    }
}

void Terminal::processDebugInput(int ch) {
    switch (ch) {
        case '\n':
        case '\r':
            if (mOnCommand && !mDebugInput.empty()) {
                mOnCommand(mDebugInput);
            }
            mDebugInput.clear();
            mDebugCursorPos = 0;
            break;
        case KEY_BACKSPACE:
        case 127:
        case '\b':
            if (mDebugCursorPos > 0) {
                mDebugInput.erase(mDebugCursorPos - 1, 1);
                mDebugCursorPos--;
            }
            break;
        case KEY_DC:
            if (mDebugCursorPos < static_cast<int>(mDebugInput.length())) {
                mDebugInput.erase(mDebugCursorPos, 1);
            }
            break;
        case KEY_LEFT:
            if (mDebugCursorPos > 0) mDebugCursorPos--;
            break;
        case KEY_RIGHT:
            if (mDebugCursorPos < static_cast<int>(mDebugInput.length())) mDebugCursorPos++;
            break;
        case KEY_HOME:
            mDebugCursorPos = 0;
            break;
        case KEY_END:
            mDebugCursorPos = mDebugInput.length();
            break;
        default:
            if (ch >= 32 && ch < 127) {
                mDebugInput.insert(mDebugCursorPos, 1, static_cast<char>(ch));
                mDebugCursorPos++;
            }
            break;
    }
}

void Terminal::handleResize() {
    endwin();
    refresh();
    clear();

    mHeight = LINES;
    setupWindows();
}
