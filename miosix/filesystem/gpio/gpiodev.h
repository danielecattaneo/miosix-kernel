#pragma once

#include "kernel/sync.h"
#include "filesystem/devfs/devfs.h"
#include "filesystem/ioctl.h"
#include "filesystem/console/console_device.h"
#include "kernel/sync.h"
#include "kernel/queue.h"
#include "interfaces/gpio.h"

namespace miosix {

/**
 * GPIO
 */
class GPIODevice : public Device
{
public:
    GPIODevice();

    virtual ssize_t readBlock(void *buffer, size_t size, off_t where);
    
    virtual ssize_t writeBlock(const void *buffer, size_t size, off_t where);
    
    virtual int ioctl(int cmd, void *arg);
private:
};

} //namespace miosix
