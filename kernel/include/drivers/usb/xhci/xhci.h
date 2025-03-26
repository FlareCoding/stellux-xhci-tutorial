#ifndef XHCI_H
#define XHCI_H
#include <drivers/pci_device_driver.h>

namespace drivers {

class xhci_driver : public pci_device_driver {
public:
    xhci_driver();
    ~xhci_driver() = default;

    // ------------------------------------------------------------------------
    // Lifecycle Hooks (overrides from module_base)
    // ------------------------------------------------------------------------
    bool init_device() override;
    bool start_device() override;
    bool shutdown_device() override;
};
} // namespace drivers

#endif // XHCI_H
