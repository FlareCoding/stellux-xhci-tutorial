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

    // Parse the extended capabilities
    _parse_extended_capability_registers();

    // Reset the host controller
    if (!_reset_host_controller()) {
        return false;
    }

    // Setup operational registers
    _configure_operational_registers();
    _log_operational_registers();

    // Setup runtime registers
    _configure_runtime_registers();

    // Register the xhci host controller IRQ handler
    if (m_irq_vector != 0) {
        if (!register_irq_handler(m_irq_vector, reinterpret_cast<irq_handler_t>(_xhci_irq_handler), false, static_cast<void*>(this))) {
            serial::printf("Failed to register xhci handler at IRQ%i\n\n", m_irq_vector - IRQ0);
        } else {
            serial::printf("Registered xhci handler at IRQ%i\n\n", m_irq_vector - IRQ0);
        }
    }

    return true;
}

bool xhci_driver::start_device() {
    // At this point the controller is all setup so we can start it
    if (!_start_host_controller()) {
        serial::printf("Failed to start the host controller\n");
        return false;
    }

    serial::printf("Controller started!\n\n");

    for (uint8_t i = 0; i < m_max_ports; i++) {
        xhci_portsc_register portsc = _read_portsc_reg(i);

        if (portsc.csc && portsc.ccs) {
            bool reset_successful = _reset_port(i);

            if (reset_successful) {
                serial::printf("Device connected on port %i - %s\n", i, _usb_speed_to_string(portsc.port_speed));
                _setup_device(i);
            } else {
                serial::printf("Failed to reset port %i after connection detection\n", i);
            }
        }
    }

    return true;
}

bool xhci_driver::shutdown_device() {
    return true;
}

irqreturn_t xhci_driver::_xhci_irq_handler(void*, xhci_driver* driver) {
    driver->_process_events();
    driver->_acknowledge_irq(0);

    // Acknowledge the interrupt
    irq_send_eoi();

    // Return indicating that the interrupt was handled successfully
    return IRQ_HANDLED;
}

void xhci_driver::_process_events() {
    // Poll the event ring for the command completion event
    kstl::vector<xhci_trb_t*> events;
    if (m_event_ring->has_unprocessed_events()) {
        m_event_ring->dequeue_events(events);
    }

    uint8_t command_completion_status = 0;

    for (size_t i = 0; i < events.size(); i++) {
        xhci_trb_t* event = events[i];
        switch (event->trb_type) {
        case XHCI_TRB_TYPE_CMD_COMPLETION_EVENT: {
            command_completion_status = 1;
            m_command_completion_events.push_back((xhci_command_completion_trb_t*)event);
            break;
        }
        default: break;
        }
    }

    m_command_irq_completed = command_completion_status;
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

    // Construct a manager class instance for the doorbell register array
    m_doorbell_manager = kstl::shared_ptr<xhci_doorbell_manager>(
        new xhci_doorbell_manager(m_xhc_base + m_cap_regs->dboff)
    );
}

void xhci_driver::_parse_extended_capability_registers() {
    volatile uint32_t* head_cap_ptr = reinterpret_cast<volatile uint32_t*>(
        m_xhc_base + m_extended_capabilities_offset
    );

    m_extended_capabilities_head = kstl::shared_ptr<xhci_extended_capability>(
        new xhci_extended_capability(head_cap_ptr)
    );

    auto node = m_extended_capabilities_head;
    while (node.get()) {
        if (node->id() == xhci_extended_capability_code::supported_protocol) {
            xhci_usb_supported_protocol_capability cap(node->base());

            // Make the ports zero-based
            uint8_t first_port = cap.compatible_port_offset - 1;
            uint8_t last_port = first_port + cap.compatible_port_count - 1;

            if (cap.major_revision_version == 3) {
                for (uint8_t port = first_port; port <= last_port; port++) {
                    m_usb3_ports.push_back(port);
                }
            }
        }

        // Advance to the next node
        node = node->next();
    }
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

void xhci_driver::_log_usbsts() {
    uint32_t status = m_op_regs->usbsts;
    serial::printf("===== USBSTS =====\n");
    if (status & XHCI_USBSTS_HCH)  serial::printf("    Host Controlled Halted\n");
    if (status & XHCI_USBSTS_HSE)  serial::printf("    Host System Error\n");
    if (status & XHCI_USBSTS_EINT) serial::printf("    Event Interrupt\n");
    if (status & XHCI_USBSTS_PCD)  serial::printf("    Port Change Detect\n");
    if (status & XHCI_USBSTS_SSS)  serial::printf("    Save State Status\n");
    if (status & XHCI_USBSTS_RSS)  serial::printf("    Restore State Status\n");
    if (status & XHCI_USBSTS_SRE)  serial::printf("    Save/Restore Error\n");
    if (status & XHCI_USBSTS_CNR)  serial::printf("    Controller Not Ready\n");
    if (status & XHCI_USBSTS_HCE)  serial::printf("    Host Controller Error\n");
    serial::printf("\n");
}

xhci_portsc_register xhci_driver::_read_portsc_reg(uint8_t port_num) {
    uint64_t reg_base = reinterpret_cast<uint64_t>(m_op_regs) + (0x400 + (0x10 * port_num));
    
    xhci_portsc_register reg;
    reg.raw = *reinterpret_cast<volatile uint32_t*>(reg_base);

    return reg;
}

void xhci_driver::_write_portsc_reg(xhci_portsc_register reg, uint8_t port_num) {
    uint64_t reg_base = reinterpret_cast<uint64_t>(m_op_regs) + (0x400 + (0x10 * port_num));
    *reinterpret_cast<volatile uint32_t*>(reg_base) = reg.raw;
}

bool xhci_driver::_is_usb3_port(uint8_t port_num) {
    for (size_t i = 0; i < m_usb3_ports.size(); ++i) {
        if (m_usb3_ports[i] == port_num) {
            return true;
        }
    }

    return false;
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

bool xhci_driver::_start_host_controller() {
    // Ensure USBCMD bits for RUN/STOP are properly set
    uint32_t usbcmd = m_op_regs->usbcmd;
    usbcmd |= XHCI_USBCMD_RUN_STOP;
    usbcmd |= XHCI_USBCMD_INTERRUPTER_ENABLE;
    usbcmd |= XHCI_USBCMD_HOSTSYS_ERROR_ENABLE;
    m_op_regs->usbcmd = usbcmd;

    // Ensure the controller transitions out of the halted state
    constexpr int max_retries = 1000;
    int retries = 0;

    while (m_op_regs->usbsts & XHCI_USBSTS_HCH) {
        if (retries++ >= max_retries) {
            // Timeout: Controller failed to start
            return false;
        }
        msleep(1); // Poll every 1 ms for responsiveness
    }

    // Verify CNR (Controller Not Ready) bit is clear
    if (m_op_regs->usbsts & XHCI_USBSTS_CNR) {
        return false; // Controller is not ready
    }

    // Controller started successfully
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
    m_event_ring = kstl::shared_ptr<xhci_event_ring>(
        new xhci_event_ring(XHCI_EVENT_RING_TRB_COUNT, interrupter_regs)
    );

    // Clear any pending interrupts for primary interrupter
    _acknowledge_irq(0);
}

void xhci_driver::_acknowledge_irq(uint8_t interrupter) {
    // Get the interrupter registers
    volatile xhci_interrupter_registers* interrupter_regs = &m_runtime_regs->ir[interrupter];

    // Read the current value of IMAN
    uint32_t iman = interrupter_regs->iman;

    // Set the IP bit to '1' to clear it, preserve other bits including IE
    iman |= XHCI_IMAN_INTERRUPT_PENDING;

    // Write back to IMAN
    interrupter_regs->iman = iman;

    // Clear the EINT bit in USBSTS by writing '1' to it
    m_op_regs->usbsts = XHCI_USBSTS_EINT;
}

xhci_command_completion_trb_t* xhci_driver::_send_command_trb(xhci_trb_t* cmd_trb, uint32_t timeout_ms) {
    // Enqueue the TRB
    if (!m_command_ring->enqueue(cmd_trb)) {
        serial::printf("Failed to enqueue command. Command ring is full.");
        return nullptr;
    };

    // Ring the command doorbell
    m_doorbell_manager->ring_command_doorbell();

    // Wait for the IRQ and let the host controller process the command
    uint64_t sleep_passed = 0;
    while (!m_command_irq_completed) {
        usleep(10);
        sleep_passed += 10;

        if (sleep_passed > timeout_ms * 1000) {
            break;
        }
    }

    // ** Important Assumption **
    //  - Only one command is being sent to the controller at a time
    xhci_command_completion_trb_t* completion_trb =
        m_command_completion_events.size() ? m_command_completion_events[0] : nullptr;

    // Reset the irq flag and clear out the command completion event queue
    m_command_completion_events.clear();
    m_command_irq_completed = 0;

    if (!completion_trb) {
        serial::printf("Failed to find completion TRB for command %i\n", cmd_trb->trb_type);
        return nullptr;
    }

    if (completion_trb->completion_code != XHCI_TRB_COMPLETION_CODE_SUCCESS) {
        serial::printf("Command TRB failed with error: %s\n", trb_completion_code_to_string(completion_trb->completion_code));
        return nullptr;
    }

    // Update the command ring dequeue pointer
    m_command_ring->process_event(completion_trb);

    return completion_trb;
}

bool xhci_driver::_reset_port(uint8_t port_num) {
    xhci_portsc_register portsc = _read_portsc_reg(port_num);

    bool is_usb3_port = _is_usb3_port(port_num);

    // Power on the port if necessary
    if (portsc.pp == 0) {
        portsc.pp = 1;
        _write_portsc_reg(portsc, port_num);
        msleep(20); // Wait for power stabilization
        portsc = _read_portsc_reg(port_num);

        if (portsc.pp == 0) {
            serial::printf("Port %i: Failed to power on port\n", port_num);
            return false;
        }
    }

    // Clear any lingering status change bits before initiating the reset
    portsc.csc = 1; // Clear connect status change
    portsc.pec = 1; // Clear port enable/disable change
    portsc.prc = 1; // Clear port reset change
    _write_portsc_reg(portsc, port_num);

    // Initiate the port reset
    if (is_usb3_port) {
        portsc.wpr = 1; // Warm reset for USB 3.0
    } else {
        portsc.pr = 1; // Standard port reset for USB 2.0
    }
    _write_portsc_reg(portsc, port_num);

    // Wait for the reset to complete
    int timeout = 100;
    while (timeout > 0) {
        portsc = _read_portsc_reg(port_num);

        if ((is_usb3_port && portsc.wrc) || (!is_usb3_port && portsc.prc)) {
            break; // Reset has completed
        }

        timeout--;
        msleep(1);
    }

    if (timeout == 0) {
        serial::printf("Port %i: Port reset timed out\n", port_num);
        return false;
    }

    msleep(3); // Give the hardware time to settle

    // Clear the reset completion and status change bits
    portsc.prc = 1; // Clear port reset change
    portsc.wrc = 1; // Clear warm reset change (USB 3.0)
    portsc.csc = 1; // Clear connect status change
    portsc.pec = 1; // Clear port enable/disable change
    portsc.ped = 0; // Don't clear the PED bit
    _write_portsc_reg(portsc, port_num);

    msleep(3);

    // Re-read the register to check if the port is enabled
    portsc = _read_portsc_reg(port_num);

    // This case could happen when the port has been reset after
    // a device disconnect event, and no device has connected since.
    if (portsc.ped == 0) {
        return false;
    }

    return true;
}

const char* xhci_driver::_usb_speed_to_string(uint8_t speed) {
    static const char* speed_string[7] = {
        "Invalid",
        "Full Speed (12 MB/s - USB2.0)",
        "Low Speed (1.5 Mb/s - USB 2.0)",
        "High Speed (480 Mb/s - USB 2.0)",
        "Super Speed (5 Gb/s - USB3.0)",
        "Super Speed Plus (10 Gb/s - USB 3.1)",
        "Undefined"
    };

    return speed_string[speed];
}

uint8_t xhci_driver::_get_port_speed(uint8_t port) {
    xhci_portsc_register portsc = _read_portsc_reg(port);
    return static_cast<uint8_t>(portsc.port_speed);
}

uint8_t xhci_driver::_enable_device_slot() {
    xhci_trb_t enable_slot_trb;
    zeromem(&enable_slot_trb, sizeof(xhci_trb_t));

    enable_slot_trb.trb_type = XHCI_TRB_TYPE_ENABLE_SLOT_CMD;

    auto completion_trb = _send_command_trb(&enable_slot_trb);
    if (!completion_trb) {
        return 0;
    }

    return completion_trb->slot_id;
}

bool xhci_driver::_create_device_context(uint8_t slot_id) {
    // Determine the size of the device context
    // based on the capability register parameters.
    uint64_t device_context_size = m_64byte_context_size ? sizeof(xhci_device_context64) : sizeof(xhci_device_context32);

    // Allocate a memory block for the device context
    void* ctx = alloc_xhci_memory(
        device_context_size,
        XHCI_DEVICE_CONTEXT_ALIGNMENT,
        XHCI_DEVICE_CONTEXT_BOUNDARY
    );

    if (!ctx) {
        serial::printf("Failed to allocate memory for a device context\n");
        return false;
    }

    // Insert the device context's physical address
    // into the Device Context Base Addres Array (DCBAA).
    m_dcbaa[slot_id] = xhci_get_physical_addr(ctx);

    // Store the virtual address as well
    m_dcbaa_virtual_addresses[slot_id] = reinterpret_cast<uint64_t>(ctx);

    return true;
}

void xhci_driver::_setup_device(uint8_t port) {
    uint8_t port_speed = _get_port_speed(port);
    uint8_t port_id = port + 1;

    // Allocate a device slot for the device
    uint8_t slot_id = _enable_device_slot();
    if (!slot_id) {
        serial::printf("Failed to enable device slot for port %i\n", port);
        return;
    }

    // Create a device context for the allocated slot
    if (!_create_device_context(slot_id)) {
        serial::printf("Failed to create device context for slot %i\n", slot_id);
        return;
    }

    xhci_device* device = new xhci_device(port_id, slot_id, port_speed, m_64byte_context_size);

    serial::printf("Allocated device:\n");
    serial::printf("  port  - %i\n", device->get_port());
    serial::printf("  slot  - %i\n", device->get_slot());
    serial::printf("  speed - %s\n", _usb_speed_to_string(device->get_speed()));
    serial::printf("  inctx - 0x%llx\n", device->get_input_ctx_dma());
    serial::printf("\n");
}

} // namespace drivers
