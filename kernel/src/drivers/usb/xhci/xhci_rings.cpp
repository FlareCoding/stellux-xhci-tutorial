#include <drivers/usb/xhci/xhci_rings.h>
#include <serial/serial.h>

xhci_command_ring::xhci_command_ring(size_t max_trbs) {
    m_max_trb_count = max_trbs;
    m_rcs_bit = XHCI_CRCR_RING_CYCLE_STATE;
    m_enqueue_ptr = 0;

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

void xhci_command_ring::enqueue(xhci_trb_t* trb) {
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
}

