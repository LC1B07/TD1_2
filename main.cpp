#include <Novice.h>

// シーンの種類
enum Scene {
	SCENE_TITLE,
	SCENE_BOSS,
	SCENE_GAMEOVER,
	SCENE_CLEAR
};

// 元のガチャコードから背景の光パーティクルだけを移植
const int kWidth = 1280;
const int kHeight = 720;

void DrawAmbientBackground(int frame) {
	Novice::DrawBox(0, 0, kWidth, kHeight, 0.0f, 0x151826FF, kFillModeSolid);
	for (int i = 0; i < 14; ++i) {
		int x = (i * 191 + 83) % kWidth;
		int y = (i * 127 + 47 - frame / (2 + i % 3)) % kHeight;
		if (y < 0) y += kHeight;
		int radius = 2 + i % 4;
		Novice::DrawBox(x - radius * 2, y - radius * 2, radius * 4, radius * 4,
			0.0f, 0x7BBFFF30u, kFillModeSolid);
		Novice::DrawBox(x, y, radius, radius, 0.0f, 0xD9F3FF90, kFillModeSolid);
	}
}

const char kWindowTitle[] = "LC1D_03_イシダ_ノリヒコ_タイトル";

// Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

	// ライブラリの初期化
	Novice::Initialize(kWindowTitle, 1280, 720);

	// キー入力結果を受け取る箱
	char keys[256] = { 0 };
	char preKeys[256] = { 0 };

	Scene currentScene = SCENE_TITLE; // 最初はタイトル画面
	int effectFrame = 0; // 背景パーティクルのアニメーション用

	// ウィンドウの×ボタンが押されるまでループ
	while (Novice::ProcessMessage() == 0) {
		// フレームの開始
		Novice::BeginFrame();

		// キー入力を受け取る
		memcpy(preKeys, keys, 256);
		Novice::GetHitKeyStateAll(keys);

		///
		/// ↓更新処理ここから
		++effectFrame; // 毎フレーム背景を動かす
		///

		///
		// Enterキーを押した瞬間だけシーンを切り替える
		if (preKeys[DIK_RETURN] == 0 && keys[DIK_RETURN] != 0) {
			switch (currentScene) {
			case SCENE_TITLE:    currentScene = SCENE_BOSS;     break;
			case SCENE_BOSS:     currentScene = SCENE_GAMEOVER; break;
			case SCENE_GAMEOVER: currentScene = SCENE_CLEAR;    break;
			case SCENE_CLEAR:    currentScene = SCENE_TITLE;    break;
			}
		}

		/// ↑更新処理ここまで
		///

		///
		/// ↓描画処理ここから
		///

		///
		// すべてのシーンの背面に背景パーティクルを描画する
		DrawAmbientBackground(effectFrame);

		// 現在のシーンに応じた画面を描画する
		switch (currentScene) {
		case SCENE_TITLE:
			Novice::ScreenPrintf(540, 330, "TITLE");
			break;
		case SCENE_BOSS:
			Novice::ScreenPrintf(540, 330, "BOSS BATTLE");
			break;
		case SCENE_GAMEOVER:
			Novice::ScreenPrintf(540, 330, "GAME OVER");
			break;
		case SCENE_CLEAR:
			Novice::ScreenPrintf(540, 330, "GAME CLEAR");
			break;
		}
		Novice::ScreenPrintf(490, 390, "Press ENTER to change scene");

		/// ↑描画処理ここまで
		///

		// フレームの終了
		Novice::EndFrame();

		// ESCキーが押されたらループを抜ける
		if (preKeys[DIK_ESCAPE] == 0 && keys[DIK_ESCAPE] != 0) {
			break;
		}
	}

	// ライブラリの終了
	Novice::Finalize();
	return 0;
}
