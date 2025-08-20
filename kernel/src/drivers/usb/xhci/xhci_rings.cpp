#include <drivers/usb/xhci/xhci_rings.h>
#include <serial/serial.h>

xhci_command_ring::xhci_command_ring(size_t max_trbs) {
    m_max_trb_count = max_trbs;
    m_rcs_bit = XHCI_CRCR_RING_CYCLE_STATE;
    m_enqueue_ptr = 0;
    m_dequeue_ptr = 0;
    m_consumer_cycle_state = true;

    const uint64_t ring_size = max_trbs * sizeof(xhci_trb_t);

    // Create the command ring memory block
    m_trbs = (xhci_trb_t*)alloc_xhci_memory(
        ring_size,
        XHCI_COMMAND_RING_SEGMENTS_ALIGNMENT,
        XHCI_COMMAND_RING_SEGMENTS_BOUNDARY
    );

    m_physical_base = xhci_get_physical_addr(m_trbs);

    // Set the last TRB as a link TRB to point back to the first TRB
    m_trbs[m_max_trb_count - 1].parameter = m_physical_base;
    m_trbs[m_max_trb_count - 1].control =
        (XHCI_TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT) | XHCI_LINK_TRB_TC_BIT | m_rcs_bit;
}

bool xhci_command_ring::enqueue(xhci_trb_t* trb) {
    auto can_enqueue = m_consumer_cycle_state == m_rcs_bit 
        ? m_enqueue_ptr >= m_dequeue_ptr 
        : m_enqueue_ptr < m_dequeue_ptr;
    if (!can_enqueue) {
        return false;
    }
        
    // Adjust the TRB's cycle bit to the current RCS
    trb->cycle_bit = m_rcs_bit;

    // Insert the TRB into the ring
    m_trbs[m_enqueue_ptr] = *trb;

    // Advance and possibly wrap the enqueue pointer if needed.
    // maxTrbCount - 1 accounts for the LINK_TRB.
    if (++m_enqueue_ptr == m_max_trb_count - 1) {
        // Update the Link TRB to reflect the current,
        // cycle state including the TC flag.
        m_trbs[m_max_trb_count - 1].control =
            (XHCI_TRB_TYPE_LINK << XHCI_TRB_TYPE_SHIFT) | XHCI_LINK_TRB_TC_BIT | m_rcs_bit;

        m_enqueue_ptr = 0;
        m_rcs_bit = !m_rcs_bit;
    }

    return true;
}

void xhci_command_ring::process_event(xhci_command_completion_trb_t* event) {
    // xHCI 4.9.3 Command Ring Management
    // > The location of the Command Ring Dequeue Pointer is reported on the Event Ring in Command Completion Events.
    // xHCI 3.3 Command Interface
    // > Commands are executed by the xHC in the order that they are placed on the Command Ring.
    auto command_index = (event->command_trb_pointer - m_physical_base) / sizeof(xhci_trb_t);
    // This could result in the dequeue pointer pointing to a Link TRB, which should be pretty instantly processed.
    // But we can't assume that the xHC processed the Link TRB and we shouldn't overwrite it until we're sure.
    // Since commands are executed in order, we don't need to worry about the dequeue pointer getting moved back because of out-of-order events.
    auto new_dequeue_ptr = command_index + 1;
    // If the consumer (xHC) looped around, it must have toggled its consumer cycle state
    if (new_dequeue_ptr < m_dequeue_ptr) {
        m_consumer_cycle_state = !m_consumer_cycle_state;
    }
    m_dequeue_ptr = new_dequeue_ptr;
}

xhci_event_ring::xhci_event_ring(
    size_t max_trbs,
    volatile xhci_interrupter_registers* interrupter
) {
    m_interrupter_regs = interrupter;
    m_segment_trb_count = max_trbs;
    m_rcs_bit = XHCI_CRCR_RING_CYCLE_STATE;
    m_dequeue_ptr = 0;

    // Event ring will only use one segment
    const uint64_t segment_count = 1;

    const uint64_t segment_size = max_trbs * sizeof(xhci_trb_t);
    const uint64_t segment_table_size = segment_count * sizeof(xhci_erst_entry);

    // Create the event ring segment memory block
    m_trbs = (xhci_trb_t*)alloc_xhci_memory(
        segment_size,
        XHCI_EVENT_RING_SEGMENTS_ALIGNMENT,
        XHCI_EVENT_RING_SEGMENTS_BOUNDARY
    );

    // Store the physical DMA base
    m_physical_base = xhci_get_physical_addr(m_trbs);

    // Create the event ring segment table
    m_segment_table = (xhci_erst_entry*)alloc_xhci_memory(
        segment_table_size,
        XHCI_EVENT_RING_SEGMENT_TABLE_ALIGNMENT,
        XHCI_EVENT_RING_SEGMENT_TABLE_BOUNDARY
    );

    // Construct the segment table entry
    xhci_erst_entry entry;
    entry.ring_segment_base_address = m_physical_base;
    entry.ring_segment_size = m_segment_trb_count;
    entry.rsvd = 0;

    // Insert the constructed segment into the table
    m_segment_table[0] = entry;

    // Configure the Event Ring Segment Table Size (ERSTSZ) register
    m_interrupter_regs->erstsz = 1;

    // Initialize and set ERDP
    _update_erdp();

    // Write to ERSTBA register
    m_interrupter_regs->erstba = xhci_get_physical_addr(m_segment_table);
}

bool xhci_event_ring::has_unprocessed_events() {
    return (m_trbs[m_dequeue_ptr].cycle_bit == m_rcs_bit);
}

void xhci_event_ring::dequeue_events(kstl::vector<xhci_trb_t*>& trbs) {
    // Process each event TRB
    while (has_unprocessed_events()) {
        xhci_trb_t* trb = _dequeue_trb();
        if (!trb) {
            break;
        }

        trbs.push_back(trb);
    }

    // Update the ERDP register
    _update_erdp();

    // Clear the EHB (Event Handler Busy) bit
    uint64_t erdp = m_interrupter_regs->erdp;
    erdp |= XHCI_ERDP_EHB;
    m_interrupter_regs->erdp = erdp;
}

void xhci_event_ring::flush_unprocessed_events() {
    kstl::vector<xhci_trb_t*> events;
    dequeue_events(events);
    events.clear();
}

void xhci_event_ring::_update_erdp() {
    uint64_t dequeue_address = m_physical_base + (m_dequeue_ptr * sizeof(xhci_trb_t));
    m_interrupter_regs->erdp = dequeue_address;
}

xhci_trb_t* xhci_event_ring::_dequeue_trb() {
    if (m_trbs[m_dequeue_ptr].cycle_bit != m_rcs_bit) {
        serial::printf("Event Ring attempted to dequeue an invalid TRB, returning nullptr!\n");
        return nullptr;
    }

    // Get the resulting TRB
    xhci_trb_t* ret = &m_trbs[m_dequeue_ptr];

    // Advance and possibly wrap the dequeue pointer if needed
    if (++m_dequeue_ptr == m_segment_trb_count) {
        m_dequeue_ptr = 0;
        m_rcs_bit = !m_rcs_bit;
    }

    return ret;
}
