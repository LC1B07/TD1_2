#include "../Controller/Controller.h"

namespace Controller {
    // Novice入力のみを使用するため、XInputによる振動は無効化。
    void SetVibration(unsigned short left, unsigned short right) {
        (void)left;
        (void)right;
    }

    void StopVibration() {
    }

    // コントローラー番号
    const int kJoystickNo = 0;

    // ボタン数
    const int kButtonCount = 16;

    // 接続状態
    bool isConnected = false;

    // ボタン状態
    bool currentButtons[kButtonCount] = {};
    bool previousButtons[kButtonCount] = {};

    // スティック入力
    int leftX = 0;
    int leftY = 0;

    int rightX = 0;
    int rightY = 0;

    // 初期化
    void Initialize() {

        Novice::SetJoystickDeadZone(kJoystickNo, 5000, 5000);
    }

    // 更新
    void Update() {

        // 前回のボタン状態を保存
        for (int i = 0; i < kButtonCount; i++) {
            previousButtons[i] = currentButtons[i];
            currentButtons[i] = false;
        }

        // 接続確認
        isConnected =
            Novice::GetNumberOfJoysticks() > kJoystickNo;

        // 接続されていなければ入力をリセット
        if (!isConnected) {
            leftX = 0;
            leftY = 0;
            rightX = 0;
            rightY = 0;
            return;
        }

        // 左スティック取得
        Novice::GetAnalogInputLeft(
            kJoystickNo, &leftX, &leftY
        );

        // 右スティック取得
        Novice::GetAnalogInputRight(
            kJoystickNo, &rightX, &rightY
        );

        // ボタン状態取得
        for (int i = 0; i < kButtonCount; i++) {

            Button button = static_cast<Button>(i);

            currentButtons[i] =
                Novice::IsPressButton(kJoystickNo, button) != 0;
        }
    }

    // 接続確認
    bool IsConnected() {
        return isConnected;
    }

    // 押し続けているか
    bool IsDown(Button button) {

        int index = static_cast<int>(button);

        if (index < 0 || index >= kButtonCount) {
            return false;
        }

        return currentButtons[index];
    }

    // 押した瞬間か
    bool IsPressed(Button button) {

        int index = static_cast<int>(button);

        if (index < 0 || index >= kButtonCount) {
            return false;
        }

        return currentButtons[index] &&
            !previousButtons[index];
    }

    // 離した瞬間か
    bool IsReleased(Button button) {

        int index = static_cast<int>(button);

        if (index < 0 || index >= kButtonCount) {
            return false;
        }

        return !currentButtons[index] &&
            previousButtons[index];
    }

    // 左スティックX
    float GetLeftStickX() {
        return (float)leftX / 32768.0f;
    }

    // 左スティックY
    float GetLeftStickY() {
        return (float)leftY / 32768.0f;
    }

    // 右スティックX
    float GetRightStickX() {
        return (float)rightX / 32768.0f;
    }

    // 右スティックY
    float GetRightStickY() {
        return (float)rightY / 32768.0f;
    }

}