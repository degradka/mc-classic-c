// timer.h: fixed timestep timer, ticks and partialTicks

#ifndef TIMER_H
#define TIMER_H

typedef struct {
    float      ticksPerSecond;
    // nanoTime in seconds at the last frame, 0 at the start so the first frame runs a whole second of ticks
    double     lastHRTime;
    int        ticks;
    float      partialTicks;
    float      timeScale;
    float      passedTime;
    // the system clock and nanoTime at the last sync, every second
    long long  lastSyncSysClock;
    long long  lastSyncHRClock;
    // how much faster the system clock runs than nanoTime
    double     timeSyncAdjustment;
} Timer;

void       Timer_init(Timer* timer, float ticksPerSecond);
void       Timer_advanceTime(Timer* timer);
// System.currentTimeMillis
long long  currentTimeMillis(void);
long long  getCurrentTimeInNanoseconds(void);

#endif /* TIMER_H */
