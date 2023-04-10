
#include <cstdio>
#include <thread>
#include <map>
#include "miosix.h"
#include "kernel/scheduler/scheduler.h"

using namespace std;
using namespace miosix;

Semaphore producer(5);
Semaphore consumer(5);
int n;

void producerThread(int id)
{
    for (int i=0; i<4; i++) {
        consumer.wait();
    }
    iprintf("B consumed 4, counter=%d\n", consumer.getCount());
    bool res = consumer.tryWait();
    iprintf("B consumed 1, counter=%d, trywait=%d\n", consumer.getCount(), static_cast<int>(res));
    res = consumer.tryWait();
    iprintf("B consumed 0, counter=%d, trywait=%d\n", consumer.getCount(), static_cast<int>(res));
    iprintf("B consumed 5\n");
    for (int i=0; i<10; i++) {
        producer.signal();
        iprintf("produced\n");
        consumer.wait();
        iprintf("received consumption signal\n");
    }
    for (int i=0; i<5; i++) {
        delayMs(1000);
        producer.signal();
        iprintf("produced slow\n");
        consumer.wait();
        iprintf("received consumption signal\n");
    }
}

//void top()
//{
//    #ifdef WITH_CPU_TIME_COUNTER
//    CPUProfiler::thread(1000000000LL);
//    #endif
//}

int main()
{
    //std::thread topThd(top);
    std::thread thd(producerThread, 1);
    for (int i=0; i<5; i++) {
        producer.wait();
    }
    iprintf("A consumed 5, counter=%d\n", producer.getCount());
    for (int i=0; i<10; i++) {
        producer.wait();
        iprintf("received production signal\n");
        consumer.signal();
        iprintf("consumed\n");
    }
    iprintf("producer = %d\nconsumer = %d\n", producer.getCount(), consumer.getCount());
    auto res = producer.timedWait(getTime()+2000000000LL);
    if (res == TimedWaitResult::NoTimeout)
        iprintf("received production signal\n");
    else
        iprintf("error!");
    iprintf("producer = %d\nconsumer = %d\n", producer.getCount(), consumer.getCount());
    consumer.signal();
    iprintf("consumed\n");
    for (int i=0; i<4; i++) {
        auto res = producer.timedWait(getTime()+90000000LL);
        int j = 0;
        while (res == TimedWaitResult::Timeout) {
            j++;
            iprintf("timeout=%d\n", j);
            res = producer.timedWait(getTime()+90000000LL);
        }
        iprintf("received production signal\n");
        consumer.signal();
        iprintf("consumed\n");
    }
    iprintf("producer = %d\nconsumer = %d\n", producer.getCount(), consumer.getCount());
    iprintf("ok!\n");
    thd.join();
}

