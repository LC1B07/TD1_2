#pragma once

#include "../../Game/game.h"

// BossA: 鬼。AI・当たり判定・予備動作・画面描画はこのモジュールだけで編集する。
namespace BossA {
void Reset(GameState& state);
void Update(GameState& state, Scene& currentScene, const char keys[], const char preKeys[], bool leftMousePressed, bool rightMousePressed);
void Draw(const GameState& state);
} // namespace BossA
