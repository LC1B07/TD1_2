
#pragma once

#include "../Game/game.h"

// タイトル更新
void UpdateTitleScene(GameState& state, Scene& currentScene, int& titleSelection, const char keys[], const char preKeys[]);

// その他のシーン更新
void UpdateOtherScene(const char keys[], const char preKeys[]);

// タイトル描画
void DrawTitleScene(int titleSelection);

// ゲームオーバー描画
void DrawGameOverScene();

// クリア描画
void DrawClearScene();

// クレジット描画
void DrawCreditsScene();

// メニューボタン描画
void DrawMenuButton(int x, int y, const char* label, bool selected);

// BossA/Bの選択を管理（タイトルのPLAY、チュートリアルから遷移）。
void UpdateBossSelectScene(Scene& currentScene, int& bossSelection, const char keys[], const char preKeys[]);
void DrawBossSelectScene(int bossSelection);
void DrawBgmScene();
