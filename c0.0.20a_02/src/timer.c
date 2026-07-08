// timer.c: fixed timestep timer, ticks and partialTicks

#define _POSIX_C_SOURCE 200809L

#include "timer.h"

#if defined(_WIN32)
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
#elif defined(__APPLE__)
  #include <mach/mach_time.h>
  #include <time.h>
#else
  #include <time.h>
#endif

void Timer_init(Timer* timer, float ticksPerSecond) {
    timer->ticksPerSecond     = ticksPerSecond;
    timer->lastHRTime         = 0.0;
    timer->ticks              = 0;
    timer->partialTicks       = 0.0f;
    timer->timeScale          = 1.0f;
    timer->passedTime         = 0.0f;
    timer->lastSyncSysClock   = currentTimeMillis();
    timer->lastSyncHRClock    = getCurrentTimeInNanoseconds() / 1000000LL;
    timer->timeSyncAdjustment = 1.0;
}

long long getCurrentTimeInNanoseconds(void) {
#if defined(_WIN32)
    static LARGE_INTEGER freq = {0};
    LARGE_INTEGER now;
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);
    // split so the multiply can't overflow after a few hours of uptime
    long long whole     = now.QuadPart / freq.QuadPart;
    long long remainder = now.QuadPart % freq.QuadPart;
    return whole * 1000000000LL + (remainder * 1000000000LL) / freq.QuadPart;

#elif defined(__APPLE__)
    static mach_timebase_info_data_t tb = {0};
    if (tb.denom == 0) mach_timebase_info(&tb);
    uint64_t t = mach_absolute_time();
    return (long long)(t * (uint64_t)tb.numer / (uint64_t)tb.denom);

#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000000000LL + (long long)ts.tv_nsec;
#endif
}

long long currentTimeMillis(void) {
#if defined(_WIN32)
    static LARGE_INTEGER freq = {0};
    LARGE_INTEGER now;
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);
    return (long long)((now.QuadPart * 1000LL) / freq.QuadPart);

#elif defined(__APPLE__)
    static mach_timebase_info_data_t tb = {0};
    if (tb.denom == 0) mach_timebase_info(&tb);
    uint64_t t = mach_absolute_time();
    uint64_t ns = t * (uint64_t)tb.numer / (uint64_t)tb.denom;
    return (long long)(ns / 1000000ULL);

#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000LL + (long long)(ts.tv_nsec / 1000000L);
#endif
}

// System.nanoTime in milliseconds, the sync factor eases a fifth of the way each second
void Timer_advanceTime(Timer* timer) {
    long long now = currentTimeMillis();
    long long passed = now - timer->lastSyncSysClock;
    long long hr = getCurrentTimeInNanoseconds() / 1000000LL;
    if (passed > 1000LL) {
        long long hrPassed = hr - timer->lastSyncHRClock;
        double ratio = (double)passed / (double)hrPassed;
        timer->timeSyncAdjustment += (ratio - timer->timeSyncAdjustment) * (double)0.2f;
        timer->lastSyncSysClock = now;
        timer->lastSyncHRClock = hr;
    }
    if (passed < 0LL) {
        timer->lastSyncSysClock = now;
        timer->lastSyncHRClock = hr;
    }

    double hrTime = (double)hr / 1000.0;
    double delta = (hrTime - timer->lastHRTime) * timer->timeSyncAdjustment;
    timer->lastHRTime = hrTime;
    if (delta < 0.0) delta = 0.0;
    if (delta > 1.0) delta = 1.0;

    timer->passedTime = (float)((double)timer->passedTime + delta * (double)timer->timeScale * (double)timer->ticksPerSecond);
    timer->ticks = (int)timer->passedTime;
    if (timer->ticks > 100) timer->ticks = 100;
    timer->passedTime -= (float)timer->ticks;
    timer->partialTicks = timer->passedTime;
}
