#pragma once

#include <iostream>
#include <chrono>
#include <ctime>

const auto InitTime = std::chrono::high_resolution_clock::now();

static int64_t getTime()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now() - InitTime).count();
}

class Timer
{
public:
    Timer() : mStartTime(getTime()) {}

    double get() const
    {
        return getTime() - mStartTime;
    }

    void reset()
    {
        mStartTime = getTime();
    }

private:
    int64_t mStartTime;
};