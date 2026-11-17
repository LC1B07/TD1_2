
#pragma once

#include "../Game/game.h"

// 演出タイマー更新
void UpdateEffectTimers(GameState& state);

// カウンターエフェクト描画
void DrawCounterEffect(
    const CounterEffect& e,
    int shakeX,
    int shakeY
);
