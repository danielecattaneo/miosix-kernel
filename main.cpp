
#include <cstdio>
#include "miosix.h"

using namespace std;
using namespace miosix;

void test_data_bus()
{
    iprintf("pattern? 0: 55/AA, 1: shifting 1\n");
    int pattern;
    scanf("%d", &pattern);
    volatile uint32_t *sdram_area = (uint32_t *)0xC0000000;
    uint32_t i = 0;
    uint32_t test;
    if (pattern == 0)
        test = 0x55555555;
    else
        test = 0x00000001;
    while (true) {
        sdram_area[i] = test;
        iprintf("addr. %p wrote %08lx read %08lx\n", &sdram_area[i], test, sdram_area[i]);
        i = (i + 0x4000401) & (0x1FFFFFFF / 4);
        if ((i & 1) == 0) {
            if (pattern == 0) {
                test = ~test;
            } else {
                test = test << 1;
                if (test == 0)
                    test = 0x00000001;
            }
        }
    }
}

int main()
{
    iprintf("marco-ram-board half-assed RAM test\n");
    volatile uint32_t *sdram_ptr = (uint32_t *)0xC0000000;
    uint32_t offset = 0;
    uint32_t end = (512*1024*1024) / 4;
    srand(1234);
    while (offset < end) {
        uint32_t test = rand();
        sdram_ptr[offset] = test;
        iprintf("addr. %p wrote %08lx\n", &sdram_ptr[offset], test);
        test = rand();
        if (offset == 0)
            offset = 1;
        else
            offset = offset << 1;
    }
    srand(1234);
    offset = 0;
    int fail = 0;
    while (offset < end) {
        uint32_t test = rand();
        uint32_t read = sdram_ptr[offset];
        int ok = test == read;
        iprintf("addr. %p read %08lx expected %08lx %s\n", &sdram_ptr[offset], read, test, ok ? "OK" : "NG");
        fail += !ok;
        test = rand();
        if (offset == 0)
            offset = 1;
        else
            offset = offset << 1;
    }
    iprintf("failures: %d\n", fail);
}

