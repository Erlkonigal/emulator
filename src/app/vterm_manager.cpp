#include "emulator/app/vterm_manager.h"

#include <cstring>
#include <algorithm>
#include <atomic>

static void output_callback(const char *s, size_t len, void *user) {
    VTermManager* self = static_cast<VTermManager*>(user);
    if (self->on_output_) {
        self->on_output_(s, len);
    }
}

static const VTermScreenCallbacks kScreenCallbacks = {
    .damage = [](VTermRect rect, void* user) -> int {
        VTermManager* self = static_cast<VTermManager*>(user);
        self->dirty_ = true;
        return 1;
    },
    .moverect = nullptr,
    .movecursor = [](VTermPos pos, VTermPos oldpos, int visible, void* user) {
        VTermManager* self = static_cast<VTermManager*>(user);
        self->cursor_row_ = pos.row;
        self->cursor_col_ = pos.col;
        if (!self->has_focus_) {
            self->cursor_visible_ = visible;
        }
        self->dirty_ = true;
        return 0;
    },
    .settermprop = [](VTermProp prop, VTermValue* val, void* user) -> int {
        VTermManager* self = static_cast<VTermManager*>(user);
        if (prop == VTERM_PROP_CURSORVISIBLE) {
            // 如果有焦点，忽略 vterm 的光标隐藏请求，始终保持光标可见
            if (!self->has_focus_) {
                self->cursor_visible_ = val->boolean;
            }
            self->dirty_ = true;
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
    Shutdown();
}

void VTermManager::Initialize(int rows, int cols) {
    if (vterm_) {
        vterm_free(vterm_);
    }

    rows_ = rows;
    cols_ = cols;

    vterm_ = vterm_new(rows, cols);
    vterm_set_utf8(vterm_, 1);

    vterm_output_set_callback(vterm_, output_callback, this);

    screen_ = vterm_obtain_screen(vterm_);
    vterm_screen_enable_altscreen(screen_, 1);

    dirty_ = true;

    vterm_screen_set_callbacks(screen_, &kScreenCallbacks, this);

    vterm_screen_reset(screen_, 1);
}

void VTermManager::Shutdown() {
    if (vterm_) {
        vterm_free(vterm_);
        vterm_ = nullptr;
        screen_ = nullptr;
        state_ = nullptr;
    }
}

void VTermManager::Resize(int rows, int cols) {
    if (vterm_) {
        vterm_set_size(vterm_, rows, cols);
        rows_ = rows;
        cols_ = cols;
        dirty_ = true;
    }
}

void VTermManager::PushOutput(const uint8_t* data, size_t len) {
    if (vterm_) {
        for (size_t i = 0; i < len; ++i) {
            if (data[i] == '\n') {
                char cr = '\r';
                vterm_input_write(vterm_, &cr, 1);
            }
            vterm_input_write(vterm_, reinterpret_cast<const char*>(&data[i]), 1);
        }
        
        dirty_ = true;
    }
}

void VTermManager::PushLog(const char* level, const char* msg) {
    if (vterm_) {
        char formatted[512];
        snprintf(formatted, sizeof(formatted), "%s\r\n", msg);
        
        vterm_input_write(vterm_, formatted, strlen(formatted));
        dirty_ = true;
    }
}

void VTermManager::ProcessInput(int ch) {
    if (!vterm_ || !has_focus_) return;

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
        case KEY_F(1) ... KEY_F(12):
            key = static_cast<VTermKey>(VTERM_KEY_FUNCTION_0 + (ch - KEY_F(1)));
            break;
        case 23:
            return;
        default:
            if (ch >= 32 && ch < 127) {
                vterm_keyboard_unichar(vterm_, static_cast<uint32_t>(ch), mod);
                dirty_ = true;
                return;
            }
            break;
    }

    if (key != VTERM_KEY_NONE) {
        vterm_keyboard_key(vterm_, key, mod);
        dirty_ = true;
    }
}

void VTermManager::Render(bool force_cursor) {
    if (!vterm_ || !vterm_win_) return;

    bool was_dirty = dirty_.exchange(false);

    if (was_dirty) {
        VTermScreenCell cell;
        werase(vterm_win_);

        for (int row = 0; row < rows_; ++row) {
            for (int col = 0; col < cols_; ++col) {
                VTermPos pos = {row, col};
                if (vterm_screen_get_cell(screen_, pos, &cell) == 1) {
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

                    if (attr) wattron(vterm_win_, attr);
                    if (cell.chars[0] > 0) {
                        mvwaddch(vterm_win_, row, col, cell.chars[0]);
                    } else {
                        mvwaddch(vterm_win_, row, col, ' ');
                    }
                    if (attr) wattroff(vterm_win_, attr);
                }
            }
        }
    }

    if (was_dirty || force_cursor) {
        if (cursor_visible_ && cursor_row_ >= 0 && cursor_row_ < rows_ &&
            cursor_col_ >= 0 && cursor_col_ < cols_) {
            wmove(vterm_win_, cursor_row_, cursor_col_);
            curs_set(1);
        } else {
            curs_set(0);
        }
        wrefresh(vterm_win_);
    }
}

void VTermManager::SetFocus(bool focus) {
    has_focus_ = focus;
    if (focus && vterm_) {
        vterm_state_focus_in(vterm_obtain_state(vterm_));
    } else if (vterm_) {
        vterm_state_focus_out(vterm_obtain_state(vterm_));
    }
}

void VTermManager::ShowCursor() {
    cursor_visible_ = true;
    curs_set(1);
}

void VTermManager::HideCursor() {
    cursor_visible_ = false;
    curs_set(0);
}

void VTermManager::DrawBorder(bool focused) {
    if (border_win_) {
        if (focused) {
            wborder(border_win_, '|', '|', '-', '-', '+', '+', '+', '+');
        } else {
            wborder(border_win_, ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ');
        }
        wrefresh(border_win_);
    }
}

void VTermManager::ForceRefresh() {
    if (vterm_win_) {
        if (cursor_visible_ && cursor_row_ >= 0 && cursor_row_ < rows_ &&
            cursor_col_ >= 0 && cursor_col_ < cols_) {
            wmove(vterm_win_, cursor_row_, cursor_col_);
        }
        wrefresh(vterm_win_);
    }
}
