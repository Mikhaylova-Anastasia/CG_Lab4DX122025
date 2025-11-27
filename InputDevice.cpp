#include "InputDevice.h"

void InputDevice::AddChar(wchar_t ch) {
    inputBuffer.push_back(ch);
}

void InputDevice::Backspace() {
    if (!inputBuffer.empty())
        inputBuffer.pop_back();
}