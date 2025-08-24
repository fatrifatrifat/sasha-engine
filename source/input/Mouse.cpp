#include "../../include/sasha/input/Mouse.h"

void Mouse::OnMouseMove(int x, int y)
{
    _lastX = _x;
    _lastY = _y;
    _x = x;
    _y = y;
}

void Mouse::OnButtonDown(WPARAM button)
{
    switch (button)
    {
    case VK_LBUTTON:
        _buttonStates[0] = true;
        break;
    case VK_RBUTTON:
        _buttonStates[1] = true;
        break;
    case VK_MBUTTON:
        _buttonStates[2] = true;
        break;
    }
}

void Mouse::OnButtonUp(WPARAM button)
{
    switch (button)
    {
    case VK_LBUTTON:
        _buttonStates[0] = false;
        break;
    case VK_RBUTTON:
        _buttonStates[1] = false;
        break;
    case VK_MBUTTON:
        _buttonStates[2] = false;
        break;
    }
}

void Mouse::OnWheelDelta(short delta)
{
    _wheelDelta = delta;
}

int Mouse::GetDeltaX() const
{
    return _x - _lastX;
}

int Mouse::GetDeltaY() const
{
    return _y - _lastY;
}

short Mouse::GetWheelDelta() const
{
    return _wheelDelta;
}

bool Mouse::OnRButtonDown() const
{
    return _buttonStates[1];
}

void Mouse::EndFrame()
{
    _wheelDelta = 0;
    _lastX = _x;
    _lastY = _y;
}
