
#include "Source/Player/player.h"

// プレイヤー更新
void UpdatePlayer(
    GameState& state,
    const char keys[]
) {

    Player& player = state.player;

    // 剣のタイマー
    if (state.swordSwingTimer > 0) {
        --state.swordSwingTimer;
    }

    if (state.swordSwingTimer == 0) {
        state.normalSwordActive = false;
    }

    // 剣の攻撃開始
    if (state.swordRequest) {

        state.swordSwingTimer = kSwordSwingFrames;

        state.normalSwordActive = state.normalSwordRequest;

        state.swordHitBoss = false;
    }

    state.swordRequest = false;
    state.normalSwordRequest = false;

    // 移動入力
    float moveX = 0.0f;

    if (keys[DIK_A]) {
        moveX -= 1.0f;
    }

    if (keys[DIK_D]) {
        moveX += 1.0f;
    }

    // 向き
    if (moveX < 0.0f) {
        player.direction = -1;
    } else if (moveX > 0.0f) {
        player.direction = 1;
    }

    // ジャンプ
    if (state.jumpRequest && player.onGround) {

        player.velocityY = kJumpPower;

        player.onGround = false;
    }

    state.jumpRequest = false;

    // ダッシュ開始
    if (
        state.dashRequest &&
        state.dashCooldown == 0 &&
        state.dashTimer == 0
        ) {

        float inputDashX = moveX;
        float inputDashY = 0.0f;

        // 空中ダッシュ
        if (!player.onGround) {

            if (keys[DIK_W]) {
                inputDashY -= 1.0f;
            }

            if (keys[DIK_S]) {
                inputDashY += 1.0f;
            }
        }

        // 入力がない場合は向いている方向
        if (inputDashX == 0.0f && inputDashY == 0.0f) {
            inputDashX = (float)player.direction;
        }

        float dashLength = sqrtf(
            inputDashX * inputDashX +
            inputDashY * inputDashY
        );

        state.dashDirX = inputDashX / dashLength;
        state.dashDirY = inputDashY / dashLength;

        state.dashTimer = kDashDuration;
        state.dashCooldown = kDashCooldown;

        state.dashWave = {
            player.x + 18.0f,
            player.y + 24.0f,
            kDashWaveFrames
        };

        if (!player.onGround) {
            player.velocityY = 0.0f;
        }
    }

    state.dashRequest = false;

    // ダッシュ移動
    if (state.dashTimer > 0) {

        player.x += state.dashDirX * kDashSpeed;
        player.y += state.dashDirY * kDashSpeed;

        --state.dashTimer;

        player.velocityY = 0.0f;

        state.dashImages[state.dashImageIndex] = {
            player.x,
            player.y,
            14
        };

        state.dashImageIndex =
            (state.dashImageIndex + 1) % kDashAfterimageCount;
    } else {

        // 通常移動
        player.x += moveX * kMoveSpeed;

        player.velocityY += kGravity;

        player.y += player.velocityY;
    }

    // 着地
    if (player.y >= kGroundY - 48.0f) {

        player.y = kGroundY - 48.0f;

        player.velocityY = 0.0f;

        player.onGround = true;

        if (state.dashDirY > 0.0f) {
            state.dashTimer = 0;
        }
    } else {

        player.onGround = false;
    }

    // 画面外制限
    if (player.x < 0.0f) {
        player.x = 0.0f;
    }

    if (player.x > kWidth - 36.0f) {
        player.x = kWidth - 36.0f;
    }

    if (player.y < 0.0f) {

        player.y = 0.0f;

        if (player.velocityY < 0.0f) {
            player.velocityY = 0.0f;
        }
    }

    // 剣の攻撃判定
    CheckSwordHitBoss(state);
}

// 剣の攻撃判定
void CheckSwordHitBoss(GameState& state) {

    if (state.swordHitBoss) {
        return;
    }

    const bool swordHitActive =
        state.normalSwordActive &&
        state.swordSwingTimer <= kSwordHitStart &&
        state.swordSwingTimer >= kSwordHitEnd;

    if (!swordHitActive) {
        return;
    }

    float hitX = state.player.direction == 1
        ? state.player.x + 18.0f
        : state.player.x + 18.0f - kSwordHitWidth;

    float hitY =
        state.player.y + 24.0f - kSwordHitHeight * 0.5f;

    if (IsHit(
        hitX, hitY,
        kSwordHitWidth, kSwordHitHeight,
        state.boss.x, state.boss.y,
        100.0f, 120.0f
    )) {

        state.swordHitBoss = true;

        state.swordHitFlashTimer = 12;
        state.bossFlash = 12;
    }
}

// 剣の描画
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

    const float progress = swinging
        ? (float)(kSwordSwingFrames - swingTimer) /
        (float)kSwordSwingFrames
        : 0.0f;

    const float baseAngle = swinging
        ? (-pi * 0.75f + progress * pi)
        : (-pi * 0.22f);

    const float angle = player.direction == 1
        ? baseAngle
        : pi - baseAngle;

    // 剣の残像
    if (swinging) {

        for (int i = 1; i <= 4; ++i) {

            float oldProgress = progress - 0.055f * i;

            if (oldProgress < 0.0f) {
                continue;
            }

            float oldBase = -pi * 0.75f + oldProgress * pi;

            float oldAngle = player.direction == 1
                ? oldBase
                : pi - oldBase;

            float ex = cx + cosf(oldAngle) * kSwordLength;
            float ey = cy + sinf(oldAngle) * kSwordLength;

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
