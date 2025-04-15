#ifndef XHCI_RINGS_H
#define XHCI_RINGS_H

#include "xhci_mem.h"
#include "xhci_trb.h"

class xhci_command_ring {
public:
    xhci_command_ring(size_t max_trbs);

    inline xhci_trb_t* get_virtual_base() const { return m_trbs; }
    inline uintptr_t get_physical_base() const { return m_physical_base; }
    inline uint8_t  get_cycle_bit() const { return m_rcs_bit; }

    void enqueue(xhci_trb_t* trb);

private:
    size_t              m_max_trb_count;     // Number of valid TRBs in the ring including the LINK_TRB
    size_t              m_enqueue_ptr;       // Index in the ring where to enqueue next TRB
    xhci_trb_t*         m_trbs;              // Base address of the ring buffer
    uintptr_t           m_physical_base;     // Physical base of the ring
    uint8_t             m_rcs_bit;           // Ring cycle state
};

#endif // XHCI_RINGS_H
