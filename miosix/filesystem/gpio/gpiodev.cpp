#include "interfaces/bsp.h"
#include "gpiodev.h"

namespace miosix {

GPIODevice::GPIODevice() : Device(DeviceType::STREAM)
{
}

ssize_t GPIODevice::readBlock(void *buffer, size_t size, off_t where)
{
    ledOff();
    return 0;
}

ssize_t GPIODevice::writeBlock(const void *buffer, size_t size, off_t where)
{
    ledOn();
    return 0;
}

int GPIODevice::ioctl(int cmd, void *arg)
{
    return -EINVAL;
}

} // namespace miosix
