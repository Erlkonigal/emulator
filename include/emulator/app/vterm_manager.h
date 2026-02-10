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

    void initialize(int rows, int cols);
    void shutdown();
    void resize(int rows, int cols);

    void pushChar(const char ch);
    void pushLog(const char* msg);
    void processInput(int ch);
    void render(bool forceCursor = false);
    bool isDirty() const { return mDirty.load(); }
    void clearDirty() { mDirty.store(false); }

    void setFocus(bool focus);
    bool hasFocus() const { return mHasFocus; }

    void showCursor();
    void hideCursor();
    void drawBorder(bool focused);

    void setWindow(WINDOW* win) { mVtermWin = win; }
    WINDOW* getWindow() { return mVtermWin; }

    void forceRefresh();
    int getCursorRow() const { return mCursorRow; }
    int getCursorCol() const { return mCursorCol; }
    bool isCursorVisible() const { return mCursorVisible; }

    void setOnOutput(std::function<void(const char*, size_t)> callback) { mOnOutput = callback; }

private:
    VTerm* mVterm = nullptr;
    VTermScreen* mScreen = nullptr;
    VTermState* mState = nullptr;
    WINDOW* mVtermWin = nullptr;
    WINDOW* mBorderWin = nullptr;
    
public:
    bool mHasFocus = false;
    bool mCursorVisible = true;
    int mCursorRow = 0;
    int mCursorCol = 0;
    std::atomic<bool> mDirty{false};
    
    std::function<void(const char*, size_t)> mOnOutput;
    
private:
    int mRows = 0;
    int mCols = 0;
};
