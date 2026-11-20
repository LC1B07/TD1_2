#pragma once
#include "../../Game/game.h"

// 竜ボス担当: Stateと以下の関数の中身は、このフォルダだけで拡張できます。
namespace BossB {
struct State {
	int frames = 0; // 仮データ。竜のHP・位置・行動状態はここへ追加。
};
void Reset(State& boss);
void Update(State& boss, Scene& currentScene, const char keys[], const char preKeys[], bool leftMousePressed, bool rightMousePressed);
void Draw(const State& boss);
} // namespace BossB
