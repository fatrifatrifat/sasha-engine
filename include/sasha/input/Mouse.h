#pragma once
#include <Windows.h>

class Mouse
{
public:
    void OnMouseMove(int x, int y);
    void OnButtonDown(WPARAM button);
    void OnButtonUp(WPARAM button);
    void OnWheelDelta(short delta);

    int GetDeltaX() const;
    int GetDeltaY() const;
    short GetWheelDelta() const;

    bool OnRButtonDown() const;

    void EndFrame();

private:
    int _x = 0, _y = 0;
    int _lastX = 0, _lastY = 0;
    short _wheelDelta = 0;
    bool _buttonStates[3] = {};
};
