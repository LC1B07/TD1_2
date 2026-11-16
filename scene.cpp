
#include "scene.h"

// タイトルの選択ボタンを描画
void DrawMenuButton(
    int x,
    int y,
    const char* label,
    bool selected
) {

    unsigned int fill =
        selected ? 0x293D59FF : 0x1D2536FF;

    unsigned int edge =
        selected ? 0x8BCBFFFF : 0x53627CFF;

    // 影
    Novice::DrawBox(
        x + 5,
        y + 7,
        320,
        64,
        0.0f,
        WithAlpha(0x00000000u, 100),
        kFillModeSolid
    );

    // ボタン本体
    Novice::DrawBox(
        x,
        y,
        320,
        64,
        0.0f,
        fill,
        kFillModeSolid
    );

    // 選択時のハイライト
    if (selected) {

        Novice::DrawBox(
            x + 3,
            y + 3,
            314,
            9,
            0.0f,
            WithAlpha(0x8BCBFF00u, 75),
            kFillModeSolid
        );
    }

    // 枠線
    Novice::DrawBox(
        x,
        y,
        320,
        64,
        0.0f,
        edge,
        kFillModeWireFrame
    );

    // 文字
    Novice::ScreenPrintf(
        x + 95,
        y + 23,
        "%s",
        label
    );

    if (selected) {
        Novice::ScreenPrintf(x + 25, y + 23, ">");
    }
}
