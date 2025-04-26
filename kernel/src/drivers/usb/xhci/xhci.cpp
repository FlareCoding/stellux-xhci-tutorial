#include <drivers/usb/xhci/xhci.h>
#include <serial/serial.h>
#include <time/time.h>

namespace drivers {

xhci_driver::xhci_driver() : pci_device_driver("xhci_driver") {}

bool xhci_driver::init_device() {
    serial::printf("xhci init!\n");

    pci::pci_bar bar = m_pci_dev->get_bars()[0];
    m_xhc_base = xhci_map_mmio(bar.address, bar.size);

    serial::printf("m_xhc_base virtual  : 0x%llx\n", m_xhc_base);
    serial::printf("m_xhc_base physical : 0x%llx\n", xhci_get_physical_addr((void*)m_xhc_base));

    // Read capability registers
    _parse_capability_registers();
    _log_capability_registers();

    // Reset the host controller
    if (!_reset_host_controller()) {
        return false;
    }

    // Setup operational registers
    _configure_operational_registers();
    _log_operational_registers();

    // Setup runtime registers
    _configure_runtime_registers();

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

    // Update the base pointer to the runtime register set
    m_runtime_regs = reinterpret_cast<volatile xhci_runtime_registers*>(m_xhc_base + m_cap_regs->rtsoff);
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

void xhci_driver::_log_operational_registers() {
    serial::printf("===== Xhci Operational Registers (0x%llx) =====\n", (uint64_t)m_op_regs);
    serial::printf("    usbcmd     : 0x%x\n", m_op_regs->usbcmd);
    serial::printf("    usbsts     : 0x%x\n", m_op_regs->usbsts);
    serial::printf("    pagesize   : 0x%x\n", m_op_regs->pagesize);
    serial::printf("    dnctrl     : 0x%x\n", m_op_regs->dnctrl);
    serial::printf("    crcr       : 0x%llx\n", m_op_regs->crcr);
    serial::printf("    dcbaap     : 0x%llx\n", m_op_regs->dcbaap);
    serial::printf("    config     : 0x%x\n", m_op_regs->config);
    serial::printf("\n");
}

bool xhci_driver::_reset_host_controller() {
    // Make sure we clear the Run/Stop bit
    uint32_t usbcmd = m_op_regs->usbcmd;
    usbcmd &= ~XHCI_USBCMD_RUN_STOP;
    m_op_regs->usbcmd = usbcmd;

    // Wait for the HCHalted bit to be set
    uint32_t timeout = 200;
    while (!(m_op_regs->usbsts & XHCI_USBSTS_HCH)) {
        if (--timeout == 0) {
            serial::printf("Host controller did not halt within %ums\n", timeout);
            return false;
        }

        msleep(1);
    }

    // Set the HC Reset bit
    usbcmd = m_op_regs->usbcmd;
    usbcmd |= XHCI_USBCMD_HCRESET;
    m_op_regs->usbcmd = usbcmd;

    // Wait for this bit and CNR bit to clear
    timeout = 1000;
    while (
        m_op_regs->usbcmd & XHCI_USBCMD_HCRESET ||
        m_op_regs->usbsts & XHCI_USBSTS_CNR
    ) {
        if (--timeout == 0) {
            serial::printf("Host controller did not reset within %ums\n", timeout);
            return false;
        }

        msleep(1);
    }

    msleep(50);

    // Check the defaults of the operational registers
    if (m_op_regs->usbcmd != 0)
        return false;

    if (m_op_regs->dnctrl != 0)
        return false;

    if (m_op_regs->crcr != 0)
        return false;

    if (m_op_regs->dcbaap != 0)
        return false;

    if (m_op_regs->config != 0)
        return false;

    return true;
}

void xhci_driver::_configure_operational_registers() {
    // Enable device notifications 
    m_op_regs->dnctrl = 0xffff;

    // Configure the usbconfig field
    m_op_regs->config = static_cast<uint32_t>(m_max_device_slots);

    // Setup device context base address array and scratchpad buffers
    _setup_dcbaa();

    // Setup the command ring and write CRCR
    m_command_ring = kstl::shared_ptr<xhci_command_ring>(
        new xhci_command_ring(XHCI_COMMAND_RING_TRB_COUNT)
    );
    m_op_regs->crcr = m_command_ring->get_physical_base() | m_command_ring->get_cycle_bit();
}

void xhci_driver::_setup_dcbaa() {
    size_t dcbaa_size = sizeof(uintptr_t) * (m_max_device_slots + 1);

    m_dcbaa = reinterpret_cast<uint64_t*>(
        alloc_xhci_memory(dcbaa_size, XHCI_DEVICE_CONTEXT_ALIGNMENT, XHCI_DEVICE_CONTEXT_BOUNDARY)
    );

    m_dcbaa_virtual_addresses = new uint64_t[m_max_device_slots + 1];

    /*
    // xHci Spec Section 6.1 (page 404)

    If the Max Scratchpad Buffers field of the HCSPARAMS2 register is > ‘0’, then
    the first entry (entry_0) in the DCBAA shall contain a pointer to the Scratchpad
    Buffer Array. If the Max Scratchpad Buffers field of the HCSPARAMS2 register is
    = ‘0’, then the first entry (entry_0) in the DCBAA is reserved and shall be
    cleared to ‘0’ by software.
    */

    // Initialize scratchpad buffer array if needed
    if (m_max_scratchpad_buffers > 0) {
        uint64_t* scratchpad_array = reinterpret_cast<uint64_t*>(
            alloc_xhci_memory(
                m_max_scratchpad_buffers * sizeof(uint64_t),
                XHCI_DEVICE_CONTEXT_ALIGNMENT,
                XHCI_DEVICE_CONTEXT_BOUNDARY
            )
        );
        
        // Create scratchpad pages
        for (uint8_t i = 0; i < m_max_scratchpad_buffers; i++) {
            void* scratchpad = alloc_xhci_memory(
                PAGE_SIZE,
                XHCI_SCRATCHPAD_BUFFERS_ALIGNMENT,
                XHCI_SCRATCHPAD_BUFFERS_BOUNDARY
            );
            
            uint64_t scratchpad_paddr = xhci_get_physical_addr(scratchpad);
            scratchpad_array[i] = scratchpad_paddr;
        }

        uint64_t scratchpad_array_physical_base = xhci_get_physical_addr(scratchpad_array);

        // Set the first slot in the DCBAA to point to the scratchpad array
        m_dcbaa[0] = scratchpad_array_physical_base;

        m_dcbaa_virtual_addresses[0] = reinterpret_cast<uint64_t>(scratchpad_array);
    }

    // Set DCBAA pointer in the operational registers
    m_op_regs->dcbaap = xhci_get_physical_addr(m_dcbaa);
}

void xhci_driver::_configure_runtime_registers() {
    // Get the primary interrupter registers
    volatile xhci_interrupter_registers* interrupter_regs = &m_runtime_regs->ir[0];

    // Enable interrupts
    uint32_t iman = interrupter_regs->iman;
    iman |= XHCI_IMAN_INTERRUPT_ENABLE;
    interrupter_regs->iman = iman;

    // Setup the event ring and write to interrupter
    // registers to set ERSTSZ, ERSDP, and ERSTBA.
    // TO-DO

    // Clear any pending interrupts for primary interrupter
    _acknowledge_irq(0);
}

void xhci_driver::_acknowledge_irq(uint8_t interrupter) {
    // Clear the EINT bit in USBSTS by writing '1' to it
    m_op_regs->usbsts = XHCI_USBSTS_EINT;

    // Get the interrupter registers
    volatile xhci_interrupter_registers* interrupter_regs = &m_runtime_regs->ir[interrupter];

    // Read the current value of IMAN
    uint32_t iman = interrupter_regs->iman;

    // Set the IP bit to '1' to clear it, preserve other bits including IE
    iman |= XHCI_IMAN_INTERRUPT_PENDING;

    // Write back to IMAN
    interrupter_regs->iman = iman;
}
} // namespace drivers
