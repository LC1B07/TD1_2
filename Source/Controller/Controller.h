#pragma once

#include <Novice.h>
#include <xinput.h>

namespace Controller {

    void SetVibration(unsigned short left, unsigned short right);
    void StopVibration();

    // Novice側のPadButton型を利用
    using Button = ::PadButton;

    void Initialize();
    void Update();

    bool IsConnected();

    bool IsDown(Button button);
    bool IsPressed(Button button);
    bool IsReleased(Button button);

    float GetLeftStickX();
    float GetLeftStickY();

    float GetRightStickX();
    float GetRightStickY();

    bool IsLeftStickLeftPressed();
    bool IsLeftStickRightPressed();
    bool IsLeftStickUpPressed();
    bool IsLeftStickDownPressed();
}