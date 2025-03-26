#include <drivers/usb/xhci/xhci.h>
#include <serial/serial.h>

namespace drivers {

xhci_driver::xhci_driver() : pci_device_driver("xhci_driver") {}

bool xhci_driver::init_device() {
    serial::printf("xhci init device!\n");
    return true;
}

bool xhci_driver::start_device() {
    serial::printf("xhci start device!\n");
    return true;
}

bool xhci_driver::shutdown_device() {
    return true;
}
} // namespace drivers
