
#pragma once

#include "../Game/game.h"

// ボス戦の初期化
void ResetBossBattle(
    Player& player,
    Boss& boss,
    Bullet bullets[]
);

// ボス更新
void UpdateBoss(
    Boss& boss,
    const Player& player,
    Bullet bullets[]
);

// ボスの攻撃
void UpdateBossAttack(
    Boss& boss,
    const Player& player,
    Bullet bullets[]
);

// 弾の発射
void BossShotBullet(
    Boss& boss,
    const Player& player,
    Bullet bullets[]
);
