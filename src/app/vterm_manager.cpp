#include "emulator/app/vterm_manager.h"

#include <cstring>
#include <algorithm>
#include <atomic>

static void outputCallback(const char *s, size_t len, void* user) {
    VTermManager* self = static_cast<VTermManager*>(user);
    if (self->mOnOutput) {
        self->mOnOutput(s, len);
    }
}

static const VTermScreenCallbacks kScreenCallbacks = {
    .damage = [](VTermRect rect, void* user) -> int {
        (void)rect;
        VTermManager* self = static_cast<VTermManager*>(user);
        self->mDirty = true;
        return 1;
    },
    .moverect = nullptr,
    .movecursor = [](VTermPos pos, VTermPos oldpos, int visible, void* user) {
        (void)oldpos;
        VTermManager* self = static_cast<VTermManager*>(user);
        self->mCursorRow = pos.row;
        self->mCursorCol = pos.col;
        if (!self->mHasFocus) {
            self->mCursorVisible = visible;
        }
        self->mDirty = true;
        return 0;
    },
    .settermprop = [](VTermProp prop, VTermValue* val, void* user) -> int {
        VTermManager* self = static_cast<VTermManager*>(user);
        if (prop == VTERM_PROP_CURSORVISIBLE) {
            if (!self->mHasFocus) {
                self->mCursorVisible = val->boolean;
            }
            self->mDirty = true;
        }
        return 0;
    },
    .bell = nullptr,
    .resize = nullptr,
    .sb_pushline = nullptr,
    .sb_popline = nullptr,
};

VTermManager::VTermManager() = default;

VTermManager::~VTermManager() {
    shutdown();
}

void VTermManager::initialize(int rows, int cols) {
    if (mVterm) {
        vterm_free(mVterm);
    }

    mRows = rows;
    mCols = cols;

    mVterm = vterm_new(rows, cols);
    vterm_set_utf8(mVterm, 1);

    vterm_output_set_callback(mVterm, outputCallback, this);

    mScreen = vterm_obtain_screen(mVterm);
    vterm_screen_enable_altscreen(mScreen, 1);

    mDirty = true;

    vterm_screen_set_callbacks(mScreen, &kScreenCallbacks, this);

    vterm_screen_reset(mScreen, 1);
}

void VTermManager::shutdown() {
    if (mVterm) {
        vterm_free(mVterm);
        mVterm = nullptr;
        mScreen = nullptr;
        mState = nullptr;
    }
}

void VTermManager::resize(int rows, int cols) {
    if (mVterm) {
        vterm_set_size(mVterm, rows, cols);
        mRows = rows;
        mCols = cols;
        mDirty = true;
    }
}

void VTermManager::pushChar(const char ch) {
    if (mVterm) {
        vterm_input_write(mVterm, &ch, 1);
        mDirty = true;
    }
}

void VTermManager::pushLog(const char* msg) {
    if (mVterm) {
        char formatted[512];
        snprintf(formatted, sizeof(formatted), "%s\r", msg);
        vterm_input_write(mVterm, formatted, strlen(formatted));
        mDirty = true;
    }
}

void VTermManager::processInput(int ch) {
    if (!mVterm || !mHasFocus) return;

    VTermKey key = VTERM_KEY_NONE;
    VTermModifier mod = VTERM_MOD_NONE;

    switch (ch) {
        case '\n':
        case '\r':
            key = VTERM_KEY_ENTER;
            break;
        case '\t':
            key = VTERM_KEY_TAB;
            break;
        case 127:
        case '\b':
            key = VTERM_KEY_BACKSPACE;
            break;
        case 27:
            key = VTERM_KEY_ESCAPE;
            break;
        case KEY_UP:
            key = VTERM_KEY_UP;
            break;
        case KEY_DOWN:
            key = VTERM_KEY_DOWN;
            break;
        case KEY_LEFT:
            key = VTERM_KEY_LEFT;
            break;
        case KEY_RIGHT:
            key = VTERM_KEY_RIGHT;
            break;
        case KEY_HOME:
            key = VTERM_KEY_HOME;
            break;
        case KEY_END:
            key = VTERM_KEY_END;
            break;
        case KEY_PPAGE:
            key = VTERM_KEY_PAGEUP;
            break;
        case KEY_NPAGE:
            key = VTERM_KEY_PAGEDOWN;
            break;
        case KEY_DC:
            key = VTERM_KEY_DEL;
            break;
        case KEY_IC:
            key = VTERM_KEY_INS;
            break;
        case KEY_F(1):
        case KEY_F(2):
        case KEY_F(3):
        case KEY_F(4):
        case KEY_F(5):
        case KEY_F(6):
        case KEY_F(7):
        case KEY_F(8):
        case KEY_F(9):
        case KEY_F(10):
        case KEY_F(11):
        case KEY_F(12):
            key = static_cast<VTermKey>(VTERM_KEY_FUNCTION_0 + (ch - KEY_F(1)));
            break;
        case 23:
            return;
        default:
            if (ch >= 32 && ch < 127) {
                vterm_keyboard_unichar(mVterm, static_cast<uint32_t>(ch), mod);
                mDirty = true;
                return;
            }
            break;
    }

    if (key != VTERM_KEY_NONE) {
        vterm_keyboard_key(mVterm, key, mod);
        mDirty = true;
    }
}

void VTermManager::render(bool forceCursor) {
    if (!mVterm || !mVtermWin) return;

    bool wasDirty = mDirty.exchange(false);

    if (wasDirty) {
        VTermScreenCell cell;
        werase(mVtermWin);

        for (int row = 0; row < mRows; ++row) {
            for (int col = 0; col < mCols; ++col) {
                VTermPos pos = {row, col};
                if (vterm_screen_get_cell(mScreen, pos, &cell) == 1) {
                    int attr = 0;
                    if (cell.attrs.bold) attr |= A_BOLD;
                    if (cell.attrs.italic) attr |= A_ITALIC;
                    if (cell.attrs.underline == VTERM_UNDERLINE_SINGLE ||
                        cell.attrs.underline == VTERM_UNDERLINE_DOUBLE ||
                        cell.attrs.underline == VTERM_UNDERLINE_CURLY) {
                        attr |= A_UNDERLINE;
                    }
                    if (cell.attrs.reverse) attr |= A_REVERSE;
                    if (cell.attrs.strike) attr |= A_STANDOUT;

                    if (attr) wattron(mVtermWin, attr);
                    if (cell.chars[0] > 0) {
                        mvwaddch(mVtermWin, row, col, cell.chars[0]);
                    } else {
                        mvwaddch(mVtermWin, row, col, ' ');
                    }
                    if (attr) wattroff(mVtermWin, attr);
                }
            }
        }
    }

    if (wasDirty || forceCursor) {
        if (mCursorVisible && mCursorRow >= 0 && mCursorRow < mRows &&
            mCursorCol >= 0 && mCursorCol < mCols) {
            wmove(mVtermWin, mCursorRow, mCursorCol);
            curs_set(1);
        } else {
            curs_set(0);
        }
        wrefresh(mVtermWin);
    }
}

void VTermManager::setFocus(bool focus) {
    mHasFocus = focus;
    if (focus && mVterm) {
        vterm_state_focus_in(vterm_obtain_state(mVterm));
    } else if (mVterm) {
        vterm_state_focus_out(vterm_obtain_state(mVterm));
    }
}

void VTermManager::showCursor() {
    mCursorVisible = true;
    curs_set(1);
}

void VTermManager::hideCursor() {
    mCursorVisible = false;
    curs_set(0);
}

void VTermManager::drawBorder(bool focused) {
    if (mBorderWin) {
        if (focused) {
            wborder(mBorderWin, '|', '|', '-', '-', '+', '+', '+', '+');
        } else {
            wborder(mBorderWin, ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ');
        }
        wrefresh(mBorderWin);
    }
}

void VTermManager::forceRefresh() {
    if (mVtermWin) {
        if (mCursorVisible && mCursorRow >= 0 && mCursorRow < mRows &&
            mCursorCol >= 0 && mCursorCol < mCols) {
            wmove(mVtermWin, mCursorRow, mCursorCol);
        }
        wrefresh(mVtermWin);
    }
}
