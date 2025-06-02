#include <drivers/usb/xhci/xhci_regs.h>

xhci_doorbell_manager::xhci_doorbell_manager(uintptr_t base) {
    m_doorbell_registers = reinterpret_cast<xhci_doorbell_register*>(base);
}

void xhci_doorbell_manager::ring_doorbell(uint8_t doorbell, uint8_t target) {
    m_doorbell_registers[doorbell].raw = static_cast<uint32_t>(target);
}

void xhci_doorbell_manager::ring_command_doorbell() {
    ring_doorbell(0, XHCI_DOORBELL_TARGET_COMMAND_RING);
}

void xhci_doorbell_manager::ring_control_endpoint_doorbell(uint8_t doorbell) {
    ring_doorbell(doorbell, XHCI_DOORBELL_TARGET_CONTROL_EP_RING);
}

xhci_extended_capability::xhci_extended_capability(volatile uint32_t* capPtr) : m_base(capPtr) {
    m_entry.raw = *m_base;
    _read_next_ext_caps();
}

void xhci_extended_capability::_read_next_ext_caps() {
    if (m_entry.next) {
        auto next_cap_ptr = XHCI_NEXT_EXT_CAP_PTR(m_base, m_entry.next);
        m_next = kstl::shared_ptr<xhci_extended_capability>(
            new xhci_extended_capability(next_cap_ptr)
        );
    }
}

