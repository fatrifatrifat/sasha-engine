#pragma once
#include <chrono>

class Timer {
public:
    Timer() {
        Reset();
    }

    void Reset() {
        mStartTime = Clock::now();
        mPrevTime = mStartTime;
        mTotalPausedDuration = Duration::zero();
        mStopped = false;
    }

    void Tick() {
        if (mStopped) {
            mDeltaTime = Duration::zero();
            return;
        }

        auto currentTime = Clock::now();
        mDeltaTime = currentTime - mPrevTime;
        mPrevTime = currentTime;
    }

    void Start() {
        if (!mStopped) return;
        auto now = Clock::now();
        mTotalPausedDuration += now - mStopTime;
        mPrevTime = now;
        mStopped = false;
    }

    void Stop() {
        if (mStopped) return;
        mStopTime = Clock::now();
        mStopped = true;
    }

    float DeltaTime() const {
        return std::chrono::duration<float>(mDeltaTime).count();
    }

    float TotalTime() const {
        if (mStopped) {
            return std::chrono::duration<float>(mStopTime - mStartTime - mTotalPausedDuration).count();
        }
        auto now = Clock::now();
        return std::chrono::duration<float>(now - mStartTime - mTotalPausedDuration).count();
    }

private:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Duration = std::chrono::duration<float>;

    TimePoint mStartTime;
    TimePoint mPrevTime;
    TimePoint mStopTime;
    Duration mDeltaTime = Duration::zero();
    Duration mTotalPausedDuration = Duration::zero();
    bool mStopped = false;
};
