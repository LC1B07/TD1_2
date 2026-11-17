
#include "Source/Boss/boss.h"
#include <cmath>

// ボスの攻撃
void UpdateBossAttack(
    Boss& boss,
    const Player& player,
    Bullet bullets[]
) {

    ++boss.shotTimer;

    if (boss.shotTimer >= kShotInterval) {

        boss.shotTimer = 0;

        BossShotBullet(boss, player, bullets);
    }
}

// ボス更新
void UpdateBoss(
    Boss& boss,
    const Player& player,
    Bullet bullets[]
) {

    UpdateBossAttack(boss, player, bullets);
}

// 弾の発射
void BossShotBullet(
    Boss& boss,
    const Player& player,
    Bullet bullets[]
) {

    for (int i = 0; i < kMaxBullets; ++i) {

        if (!bullets[i].active) {

            float startX = boss.x;
            float startY = boss.y + 48.0f;

            float dx = player.x + 18.0f - startX;
            float dy = player.y + 24.0f - startY;

            float length = sqrtf(dx * dx + dy * dy);

            if (length < 0.001f) {
                length = 1.0f;
            }

            bullets[i] = {
                startX,
                startY,
                dx / length * 7.0f,
                dy / length * 7.0f,
                true,
                false
            };

            break;
        }
    }
}

// ボス戦の初期化
void ResetBossBattle(
    Player& player,
    Boss& boss,
    Bullet bullets[]
) {

    // プレイヤー初期位置
    player = {
        140.0f,
        kGroundY - 48.0f,
        1,
        0.0f,
        true
    };

    // ボス初期位置
    boss = {
        1020.0f,
        300.0f,
        0
    };

    // 弾を消す
    for (int i = 0; i < kMaxBullets; ++i) {
        bullets[i] = {};
    }
}
