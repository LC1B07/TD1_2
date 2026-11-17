
#include "game.h"

// 背景描画
void DrawBackground() {

    Novice::DrawBox(
        0,
        0,
        kWidth,
        kHeight,
        0.0f,
        0x151826FF,
        kFillModeSolid
    );
}

// アルファブレンディング
unsigned int WithAlpha(unsigned int rgb, int alpha) {

    if (alpha < 0) {
        alpha = 0;
    }

    if (alpha > 255) {
        alpha = 255;
    }

    return (rgb & 0xFFFFFF00u) | (unsigned int)alpha;
}

// 四角形の当たり判定
bool IsHit(
    float ax, float ay, float aw, float ah,
    float bx, float by, float bw, float bh
) {

    return ax < bx + bw &&
        ax + aw > bx &&
        ay < by + bh &&
        ay + ah > by;
}
