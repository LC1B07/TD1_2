#pragma once
#include <Windows.h>

// フルスクリーンとウィンドウ表示を切り替える関数
void ToggleFullScreen(
    HWND gameWindow,
    RECT& windowRect,
    LONG_PTR& windowStyle,
    bool& isFullScreen
);
