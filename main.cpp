
#include <cstdio>
#include <thread>
#include <map>
#include "miosix.h"
#include "kernel/scheduler/scheduler.h"

using namespace std;
using namespace miosix;

void spin()
{
    #ifdef WITH_CPU_TIME_COUNTER
    long long a = CPUTimeCounter::getActiveThreadTime();
    iprintf("spin thread time 0 = %lld\n", a);
    #endif
    delayMs(1000*20);
    #ifdef WITH_CPU_TIME_COUNTER
    long long b = CPUTimeCounter::getActiveThreadTime();
    iprintf("spin thread time 1 = %lld\n", b);
    iprintf("spin thread dt = %lld\n", b - a);
    #endif
}

void top()
{
    #ifdef WITH_CPU_TIME_COUNTER
    CPUProfiler::thread(1000000000LL);
    #endif
}

int main()
{
    std::thread topThd(top);
    std::thread spinThd(spin);
    #ifdef WITH_CPU_TIME_COUNTER
    long long a = CPUTimeCounter::getActiveThreadTime();
    iprintf("main thread time 0 = %lld\n", a);
    #endif
    delayMs(1000*5);
    #ifdef WITH_CPU_TIME_COUNTER
    long long b = CPUTimeCounter::getActiveThreadTime();
    iprintf("main thread time 1 = %lld\n", b);
    iprintf("main thread dt = %lld\n", b - a);
    #endif
    spinThd.join();
    sleep(3);
    std::thread spinThd2(spin);
    for (;;) {
        sleep(1);
    }
}

