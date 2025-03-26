#include <drivers/usb/xhci/xhci.h>
#include <serial/serial.h>

namespace drivers {

xhci_driver::xhci_driver() : pci_device_driver("xhci_driver") {}

bool xhci_driver::init_device() {
    serial::printf("xhci init!\n");

    pci::pci_bar bar = m_pci_dev->get_bars()[0];
    m_xhc_base = xhci_map_mmio(bar.address, bar.size);

    serial::printf("m_xhc_base virtual  : 0x%llx\n", m_xhc_base);
    serial::printf("m_xhc_base physical : 0x%llx\n", xhci_get_physical_addr((void*)m_xhc_base));

    _parse_capability_registers();
    _log_capability_registers();

    return true;
}

bool xhci_driver::start_device() {
    serial::printf("xhci start device!\n");
    return true;
}

bool xhci_driver::shutdown_device() {
    return true;
}

void xhci_driver::_parse_capability_registers() {
    m_cap_regs = reinterpret_cast<volatile xhci_capability_registers*>(m_xhc_base);

    m_capability_regs_length = m_cap_regs->caplength;

    m_max_device_slots = XHCI_MAX_DEVICE_SLOTS(m_cap_regs);
    m_max_interrupters = XHCI_MAX_INTERRUPTERS(m_cap_regs);
    m_max_ports = XHCI_MAX_PORTS(m_cap_regs);

    m_isochronous_scheduling_threshold = XHCI_IST(m_cap_regs);
    m_erst_max = XHCI_ERST_MAX(m_cap_regs);
    m_max_scratchpad_buffers = XHCI_MAX_SCRATCHPAD_BUFFERS(m_cap_regs);

    m_64bit_addressing_capability = XHCI_AC64(m_cap_regs);
    m_bandwidth_negotiation_capability = XHCI_BNC(m_cap_regs);
    m_64byte_context_size = XHCI_CSZ(m_cap_regs);
    m_port_power_control = XHCI_PPC(m_cap_regs);
    m_port_indicators = XHCI_PIND(m_cap_regs);
    m_light_reset_capability = XHCI_LHRC(m_cap_regs);
    m_extended_capabilities_offset = XHCI_XECP(m_cap_regs) * sizeof(uint32_t);

    // Update the base pointer to operational register set
    m_op_regs = reinterpret_cast<volatile xhci_operational_registers*>(m_xhc_base + m_capability_regs_length);
}

void xhci_driver::_log_capability_registers() {
    serial::printf("===== Xhci Capability Registers (0x%llx) =====\n", (uint64_t)m_cap_regs);
    serial::printf("    Length                : %i\n", m_capability_regs_length);
    serial::printf("    Max Device Slots      : %i\n", m_max_device_slots);
    serial::printf("    Max Interrupters      : %i\n", m_max_interrupters);
    serial::printf("    Max Ports             : %i\n", m_max_ports);
    serial::printf("    IST                   : %i\n", m_isochronous_scheduling_threshold);
    serial::printf("    ERST Max Size         : %i\n", m_erst_max);
    serial::printf("    Scratchpad Buffers    : %i\n", m_max_scratchpad_buffers);
    serial::printf("    64-bit Addressing     : %s\n", m_64bit_addressing_capability ? "yes" : "no");
    serial::printf("    Bandwidth Negotiation : %i\n", m_bandwidth_negotiation_capability);
    serial::printf("    64-byte Context Size  : %s\n", m_64byte_context_size ? "yes" : "no");
    serial::printf("    Port Power Control    : %i\n", m_port_power_control);
    serial::printf("    Port Indicators       : %i\n", m_port_indicators);
    serial::printf("    Light Reset Available : %i\n", m_light_reset_capability);
    serial::printf("\n");
}
} // namespace drivers
