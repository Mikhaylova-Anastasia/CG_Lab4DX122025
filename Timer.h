#pragma once

class Timer {
public:
    Timer();

    float TotalTime() const;  // в секундах
    float DeltaTime() const;  // в секундах

    void Reset();     // Вызывать перед началом сообщений
    void Start();     // Вызывать при разблокировке игры
    void Stop();      // Вызывать при блокировке игры
    void Tick();      // Вызывать каждый кадр

private:
    double m_SecondsPerCount;
    double m_DeltaTime;

    __int64 m_BaseTime;
    __int64 m_PausedTime;
    __int64 m_StopTime;
    __int64 m_PrevTime;
    __int64 m_CurrTime;

    bool m_Stopped;
};
