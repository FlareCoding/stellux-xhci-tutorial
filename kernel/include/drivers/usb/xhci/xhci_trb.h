#ifndef XHCI_TRB_H
#define XHCI_TRB_H

#include "xhci_common.h"

/*
// xHci Spec Section 4.11 Figure 4-13: TRB Template (page 188)

This section discusses the properties and uses of TRBs that are outside of the
scope of the general data structure descriptions that are provided in section
6.4.
*/
typedef struct xhci_transfer_request_block {
    uint64_t parameter; // TRB-specific parameter
    uint32_t status;    // Status information
    union {
        struct {
            uint32_t cycle_bit               : 1;
            uint32_t eval_next_trb           : 1;
            uint32_t interrupt_on_short_pkt  : 1;
            uint32_t no_snoop                : 1;
            uint32_t chain_bit               : 1;
            uint32_t interrupt_on_completion : 1;
            uint32_t immediate_data          : 1;
            uint32_t rsvd0                   : 2;
            uint32_t block_event_interrupt   : 1;
            uint32_t trb_type                : 6;
            uint32_t rsvd1                   : 16;
        };
        uint32_t control; // Control bits, including the TRB type
    };
} xhci_trb_t;
static_assert(sizeof(xhci_trb_t) == sizeof(uint32_t) * 4);

#endif // XHCI_TRB_H

