#pragma once
#include <Windows.h>
#include <bitset>

class Keyboard
{
public:
    void OnKeyDown(WPARAM key);
    void OnKeyUp(WPARAM key);

    bool IsKeyPressed(WPARAM key) const;
    bool WasKeyPressedThisFrame(WPARAM key) const;

    void EndFrame();

private:
    std::bitset<256> _keyStates;
    std::bitset<256> _keyPressedThisFrame;
};
