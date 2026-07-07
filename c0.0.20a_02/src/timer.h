// timer.h: fixed timestep timer, ticks and partialTicks

#ifndef TIMER_H
#define TIMER_H

typedef struct {
    float      ticksPerSecond;
    // the clock in seconds at the last frame, 0 at the start so the first frame runs a whole second of ticks
    double     lastHRTime;
    int        ticks;
    float      partialTicks;
    float      timeScale;
    float      passedTime;
    // the system clock and the game clock at the last sync, every second
    long long  lastSyncSysClock;
    long long  lastSyncHRClock;
    // how much faster the system clock runs than the game clock
    double     timeSyncAdjustment;
} Timer;

void       Timer_init(Timer* timer, float ticksPerSecond);
void       Timer_advanceTime(Timer* timer);
// System.currentTimeMillis
long long  currentTimeMillis(void);
long long  getCurrentTimeInNanoseconds(void);
// sleeps until 1/fps after the last call
void       Display_sync(int fps);

#endif /* TIMER_H */
