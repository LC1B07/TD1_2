
#include "player.h"

void DrawSword(
    const Player& player,
    int swingTimer,
    int shakeX,
    int shakeY
) {

    const float pi = 3.14159265f;

    const float cx =
        player.x + 18.0f + player.direction * 12.0f + shakeX;

    const float cy =
        player.y + 24.0f + shakeY;

    const bool swinging = swingTimer > 0;

    const float progress = swinging ?
        (float)(kSwordSwingFrames - swingTimer) /
        (float)kSwordSwingFrames : 0.0f;

    const float baseAngle = swinging ?
        (-pi * 0.75f + progress * pi) :
        (-pi * 0.22f);

    const float angle =
        player.direction == 1 ? baseAngle : pi - baseAngle;

    // 剣の残像
    if (swinging) {

        for (int i = 1; i <= 4; ++i) {

            float oldProgress = progress - 0.055f * i;

            if (oldProgress < 0.0f) {
                continue;
            }

            float oldBase =
                -pi * 0.75f + oldProgress * pi;

            float oldAngle =
                player.direction == 1 ? oldBase : pi - oldBase;

            float ex =
                cx + cosf(oldAngle) * kSwordLength;

            float ey =
                cy + sinf(oldAngle) * kSwordLength;

            Novice::DrawLine(
                (int)cx,
                (int)cy,
                (int)ex,
                (int)ey,
                WithAlpha(0x63CFFFFFu, 100 - i * 19)
            );
        }
    }

    const float dx = cosf(angle);
    const float dy = sinf(angle);

    const float tipX = cx + dx * kSwordLength;
    const float tipY = cy + dy * kSwordLength;

    // 剣の刃
    Novice::DrawLine(
        (int)cx,
        (int)cy,
        (int)tipX,
        (int)tipY,
        0x52BFFFFFu
    );

    Novice::DrawLine(
        (int)(cx - dy * 2.0f),
        (int)(cy + dx * 2.0f),
        (int)(tipX - dy * 2.0f),
        (int)(tipY + dx * 2.0f),
        0xE5F9FFFFu
    );

    // 柄
    Novice::DrawLine(
        (int)(cx - dx * 12.0f),
        (int)(cy - dy * 12.0f),
        (int)cx,
        (int)cy,
        0x326E9BFFu
    );

    // 鍔
    Novice::DrawLine(
        (int)(cx - dy * 7.0f),
        (int)(cy + dx * 7.0f),
        (int)(cx + dy * 7.0f),
        (int)(cy - dx * 7.0f),
        0xFFFFFFFFu
    );
}
