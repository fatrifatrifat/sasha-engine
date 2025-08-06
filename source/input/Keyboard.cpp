#include "../../include/sasha/input/Keyboard.h"

void Keyboard::OnKeyDown(WPARAM key)
{
    if (!_keyStates[key])
        _keyPressedThisFrame[key] = true;
    _keyStates[key] = true;
}

void Keyboard::OnKeyUp(WPARAM key)
{
    _keyStates[key] = false;
}

bool Keyboard::IsKeyPressed(WPARAM key) const
{
    return _keyStates[key];
}

bool Keyboard::WasKeyPressedThisFrame(WPARAM key) const
{
    return _keyPressedThisFrame[key];
}

void Keyboard::EndFrame()
{
    _keyPressedThisFrame.reset();
}
