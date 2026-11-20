#include "BossTutorial.h"

// チュートリアルは枠だけ用意。戦闘ロジックは未実装。
namespace BossTutorial {
void Reset(State& boss) { boss = {}; }

void Update(State& boss, Scene& currentScene, const char keys[], const char preKeys[], bool leftMousePressed, bool rightMousePressed) {
	(void)leftMousePressed;
	(void)rightMousePressed;
	++boss.frames;
	if (keys[DIK_RETURN] && !preKeys[DIK_RETURN]) {
		currentScene = SCENE_BOSS_SELECT;
	}
}

void Draw(const State& boss) {
	(void)boss;
	Novice::ScreenPrintf(60, 80, "BOSS TUTORIAL (WORK IN PROGRESS)");
	Novice::ScreenPrintf(60, 110, "Press ENTER to continue to boss select");
}
} // namespace BossTutorial
