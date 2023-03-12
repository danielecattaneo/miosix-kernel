
#include <cstdio>
#include "miosix.h"

using namespace std;
using namespace miosix;

int main()
{
    for (;;) {
        char c;
        do {
            iprintf("hello, type s\n");
            scanf("%c\n", &c);
        } while (c != 's');
        iprintf("core clock is %ld\n", SystemCoreClock);
        for (int i=0; i<10000; i+=100) {
            iprintf("testing %d us\n", i);
            long long delta;
            {
                FastInterruptDisableLock dLock;
                auto start=IRQgetTime();
                delayUs(i);
                delta=IRQgetTime()-start;
            }
            iprintf("IRQgetTime=%lld delayUs=%d\n", delta, i);
        }
        for (int i=0; i<1000; i+=50) {
            iprintf("testing %d ms\n", i);
            long long delta;
            {
                FastInterruptDisableLock dLock;
                auto start=IRQgetTime();
                delayMs(i);
                delta=IRQgetTime()-start;
            }
            iprintf("IRQgetTime=%lld delayMs=%d\n", delta, i);
        }
        do {
            iprintf("type s and start timing 5 seconds\n");
            scanf("%c\n", &c);
        } while (c != 's');
        {
            FastInterruptDisableLock dLock;
            delayUs(5000000);
        }
        iprintf("End of the 5 seconds\n");
    }
}
