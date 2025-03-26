#ifndef XHCI_H
#define XHCI_H
#include <drivers/pci_device_driver.h>
#include <drivers/usb/xhci/xhci_regs.h>

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

private:
    uintptr_t m_xhc_base;

    volatile xhci_capability_registers* m_cap_regs;
    volatile xhci_operational_registers* m_op_regs;

    // CAPLENGTH
    uint8_t m_capability_regs_length;
    
    // HCSPARAMS1
    uint8_t m_max_device_slots;
    uint8_t m_max_interrupters;
    uint8_t m_max_ports;

    // HCSPARAMS2
    uint8_t m_isochronous_scheduling_threshold;
    uint8_t m_erst_max;
    uint8_t m_max_scratchpad_buffers;

    // hccparams1
    bool m_64bit_addressing_capability;
    bool m_bandwidth_negotiation_capability;
    bool m_64byte_context_size;
    bool m_port_power_control;
    bool m_port_indicators;
    bool m_light_reset_capability;
    uint32_t m_extended_capabilities_offset;

private:
    void _parse_capability_registers();
    void _log_capability_registers();
    void _log_operational_registers();

    bool _reset_host_controller();
};
} // namespace drivers

#endif // XHCI_H
