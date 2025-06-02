#ifndef XHCI_H
#define XHCI_H
#include <drivers/pci_device_driver.h>
#include <drivers/usb/xhci/xhci_regs.h>
#include <drivers/usb/xhci/xhci_ext_cap.h>
#include <drivers/usb/xhci/xhci_rings.h>

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

    volatile xhci_capability_registers*  m_cap_regs;
    volatile xhci_operational_registers* m_op_regs;
    volatile xhci_runtime_registers*     m_runtime_regs;

    // Linked list of extended capabilities
    kstl::shared_ptr<xhci_extended_capability> m_extended_capabilities_head;

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

    // Device context base address array's virtual address
    uint64_t* m_dcbaa;

    // Since DCBAA stores physical addresses, we want to keep
    // track of the virtual pointers to the output device contexts.
    uint64_t* m_dcbaa_virtual_addresses;

    // Main command ring
    kstl::shared_ptr<xhci_command_ring> m_command_ring;

    // Main event ring
    kstl::shared_ptr<xhci_event_ring> m_event_ring;

    // Doorbell register array manager
    kstl::shared_ptr<xhci_doorbell_manager> m_doorbell_manager;

    // Command completion events
    kstl::vector<xhci_command_completion_trb_t*> m_command_completion_events;

    // Flag indicating we have a command completion event
    volatile uint8_t m_command_irq_completed = 0;

    // USB3.x-specific ports (0-based)
    kstl::vector<uint8_t> m_usb3_ports;

private:
    static irqreturn_t _xhci_irq_handler(void*, xhci_driver* driver);
    void _process_events();

    void _parse_capability_registers();
    void _parse_extended_capability_registers();

    void _log_capability_registers();
    void _log_operational_registers();

    void _log_usbsts();

    // Check if 0-based port_num is part of the USB3 port register set
    bool _is_usb3_port(uint8_t port_num);

    bool _reset_host_controller();
    bool _start_host_controller();

    void _configure_operational_registers();
    void _setup_dcbaa();

    void _configure_runtime_registers();
    void _acknowledge_irq(uint8_t interrupter);

    xhci_command_completion_trb_t* _send_command_trb(xhci_trb_t* cmd_trb, uint32_t timeout_ms = 200);
};
} // namespace drivers

#endif // XHCI_H
