#include "fullScreen.h"

// フルスクリーンとウィンドウ表示を切り替える関数
void ToggleFullScreen(
    HWND gameWindow,
    RECT& windowRect,
    LONG_PTR& windowStyle,
    bool& isFullScreen
)
{
    // ウィンドウハンドルが取得できていなければ処理しない
    if (gameWindow == nullptr)
    {
        return;
    }

    // ウィンドウ表示からフルスクリーンに切り替える
    if (!isFullScreen)
    {
        // 元のウィンドウの位置とサイズを保存
        GetWindowRect(gameWindow, &windowRect);

        // 元のウィンドウのスタイルを保存
        windowStyle = GetWindowLongPtr(
            gameWindow,
            GWL_STYLE
        );

        // ウィンドウの枠をなくす
        SetWindowLongPtr(
            gameWindow,
            GWL_STYLE,
            WS_POPUP | WS_VISIBLE
        );

        // ウィンドウがあるモニターを取得
        HMONITOR monitor = MonitorFromWindow(
            gameWindow,
            MONITOR_DEFAULTTONEAREST
        );

        // モニター情報を取得
        MONITORINFO monitorInfo = {};
        monitorInfo.cbSize = sizeof(MONITORINFO);

        GetMonitorInfo(monitor, &monitorInfo);

        // モニター全体にウィンドウを広げる
        SetWindowPos(
            gameWindow,
            HWND_TOP,
            monitorInfo.rcMonitor.left,
            monitorInfo.rcMonitor.top,
            monitorInfo.rcMonitor.right -
            monitorInfo.rcMonitor.left,
            monitorInfo.rcMonitor.bottom -
            monitorInfo.rcMonitor.top,
            SWP_FRAMECHANGED | SWP_SHOWWINDOW
        );

        // フルスクリーン状態にする
        isFullScreen = true;
    } else
    {
        // 元のウィンドウスタイルに戻す
        SetWindowLongPtr(
            gameWindow,
            GWL_STYLE,
            windowStyle
        );

        // 元の位置とサイズに戻す
        SetWindowPos(
            gameWindow,
            HWND_NOTOPMOST,
            windowRect.left,
            windowRect.top,
            windowRect.right - windowRect.left,
            windowRect.bottom - windowRect.top,
            SWP_FRAMECHANGED | SWP_SHOWWINDOW
        );

        // ウィンドウ表示状態にする
        isFullScreen = false;
    }
}