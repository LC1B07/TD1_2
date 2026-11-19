
#include "../Scene/scene.h"

#include "../Boss/BossA/bossA.h"

// メニューボタン描画
void DrawMenuButton(int x, int y, const char* label, bool selected) {

	unsigned int fill = selected ? 0x293D59FF : 0x1D2536FF;

	unsigned int edge = selected ? 0x8BCBFFFF : 0x53627CFF;

	// 影
	Novice::DrawBox(x + 5, y + 7, 320, 64, 0.0f, WithAlpha(0x00000000u, 100), kFillModeSolid);

	// ボタン本体
	Novice::DrawBox(x, y, 320, 64, 0.0f, fill, kFillModeSolid);

	// 選択時のハイライト
	if (selected) {

		Novice::DrawBox(x + 3, y + 3, 314, 9, 0.0f, WithAlpha(0x8BCBFF00u, 75), kFillModeSolid);
	}

	// 枠線
	Novice::DrawBox(x, y, 320, 64, 0.0f, edge, kFillModeWireFrame);

	// 文字
	Novice::ScreenPrintf(x + 95, y + 23, "%s", label);

	if (selected) {
		Novice::ScreenPrintf(x + 25, y + 23, ">");
	}
}

// タイトル更新
void UpdateTitleScene(GameState& state, Scene& currentScene, int& titleSelection, const char keys[], const char preKeys[]) {

	bool up = false;
	if (preKeys[DIK_W] == 0 && keys[DIK_W] != 0) {
		up = true;
	}

	bool down = false;
	if (preKeys[DIK_S] == 0 && keys[DIK_S] != 0) {
		down = true;
	}

	bool enter = false;
	if (preKeys[DIK_RETURN] == 0 && keys[DIK_RETURN] != 0) {
		enter = true;
	}

	if (up || down) {
		titleSelection = 1 - titleSelection;
	}

	if (enter) {

		if (titleSelection == 0) {

			BossA::Reset(state);

			currentScene = SCENE_BOSS_A;
		} else {

			currentScene = SCENE_CREDITS;
		}
	}
}

// その他のシーン更新
void UpdateOtherScene(const char keys[], const char preKeys[]) {

	bool enter = false;

	if (preKeys[DIK_RETURN] == 0 && keys[DIK_RETURN] != 0) {
		enter = true;
	}

	if (enter == false) {
		return;
	}
}

// タイトル描画
void DrawTitleScene(int titleSelection) {

	Novice::ScreenPrintf(608, 190, "TITLE");

	DrawMenuButton(480, 290, "ゲームを始める", titleSelection == 0);

	DrawMenuButton(480, 380, "CREDITS", titleSelection == 1);

	Novice::ScreenPrintf(490, 500, "W / S : 選択    ENTER : 決定");
}

// ゲームオーバー描画
void DrawGameOverScene() {
	Novice::ScreenPrintf(540, 330, "GAME OVER");
	Novice::ScreenPrintf(490, 390, "Press ENTER to return to title");
}

// クリア描画
void DrawClearScene() {
	Novice::ScreenPrintf(540, 330, "GAME CLEAR");
	Novice::ScreenPrintf(490, 390, "Press ENTER to return to title");
}

// クレジット描画
void DrawCreditsScene() {

	Novice::ScreenPrintf(590, 220, "CREDITS");

	Novice::ScreenPrintf(490, 350, "Created by: (your name)");

	Novice::ScreenPrintf(480, 470, "ENTER を 押して タイトル に 戻る");
}
