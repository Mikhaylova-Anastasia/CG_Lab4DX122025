#pragma once
#include <string>

class InputDevice {
public:
    InputDevice() = default;

    void AddChar(wchar_t ch);
    void Backspace();
    const std::wstring& GetText() const { return inputBuffer; }
    void Clear() { inputBuffer.clear(); }

private:
    std::wstring inputBuffer;
};