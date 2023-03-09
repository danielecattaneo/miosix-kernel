
#include <cstdio>
#include <thread>
#include <map>
#include "miosix.h"
#include "kernel/scheduler/scheduler.h"

using namespace std;
using namespace miosix;

void spin()
{
    delayMs(1000*20);
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
    delayMs(1000*5);
    for (;;) {
        sleep(1);
    }
}

