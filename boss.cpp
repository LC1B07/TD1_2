
#include "boss.h"

// ボス戦の初期化
void ResetBossBattle(
    Player& player,
    Boss& boss,
    Bullet bullets[]
) {

    // プレイヤーの初期位置
    player = {
        140.0f,
        kGroundY - 48.0f,
        1,
        0.0f,
        true
    };

    // ボスの初期位置
    boss = {
        1020.0f,
        300.0f,
        0
    };

    // 弾をすべて消す
    for (int i = 0; i < kMaxBullets; ++i) {
        bullets[i].active = false;
    }
}
