#ifndef XHCI_REGS_H
#define XHCI_REGS_H
#include "xhci_mem.h"

struct xhci_capability_registers {
    const uint8_t caplength;    // Capability Register Length
    const uint8_t reserved0;
    const uint16_t hciversion;  // Interface Version Number
    const uint32_t hcsparams1;  // Structural Parameters 1
    const uint32_t hcsparams2;  // Structural Parameters 2
    const uint32_t hcsparams3;  // Structural Parameters 3
    const uint32_t hccparams1;  // Capability Parameters 1
    const uint32_t dboff;       // Doorbell Offset
    const uint32_t rtsoff;      // Runtime Register Space Offset
    const uint32_t hccparams2;  // Capability Parameters 2
};
static_assert(sizeof(xhci_capability_registers) == 32);

struct xhci_operational_registers {
    uint32_t usbcmd;        // USB Command
    uint32_t usbsts;        // USB Status
    uint32_t pagesize;      // Page Size
    uint32_t reserved0[2];
    uint32_t dnctrl;        // Device Notification Control
    uint64_t crcr;          // Command Ring Control
    uint32_t reserved1[4];
    uint64_t dcbaap;        // Device Context Base Address Array Pointer
    uint32_t config;        // Configure
    uint32_t reserved2[49];
    // Port Register Set offset has to be calculated dynamically based on MAXPORTS
};
static_assert(sizeof(xhci_operational_registers) == 256);

/*
// xHci Spec Section 5.5.2 (page 389)

Note: All registers of the Primary Interrupter shall be initialized before
setting the Run/Stop (RS) flag in the USBCMD register to ‘1’. Secondary
Interrupters may be initialized after RS = ‘1’, however all Secondary
Interrupter registers shall be initialized before an event that targets them is
generated. Not following these rules, shall result in undefined xHC behavior.
*/
struct xhci_interrupter_registers {
    uint32_t iman;         // Interrupter Management
    uint32_t imod;         // Interrupter Moderation
    uint32_t erstsz;       // Event Ring Segment Table Size
    uint32_t rsvd;         // Reserved
    uint64_t erstba;       // Event Ring Segment Table Base Address
    union {
        struct {
            // This index is used to accelerate the checking of
            // an Event Ring Full condition. This field can be 0.
            uint64_t dequeue_erst_segment_index : 3;

            // This bit is set by the controller when it sets the
            // Interrupt Pending bit. Then once your handler is finished
            // handling the event ring, you clear it by writing a '1' to it.
            uint64_t event_handler_busy         : 1;

            // Physical address of the _next_ item in the event ring
            uint64_t event_ring_dequeue_pointer : 60;
        };
        uint64_t erdp;     // Event Ring Dequeue Pointer (offset 18h)
    };
};

/*
// xHci Spec Section 5.5 Table 5-35: Host Controller Runtime Registers (page 388)

This section defines the xHCI Runtime Register space. The base address of this
register space is referred to as Runtime Base. The Runtime Base shall be 32-
byte aligned and is calculated by adding the value Runtime Register Space
Offset register (refer to Section 5.3.8) to the Capability Base address. All
Runtime registers are multiples of 32 bits in length.
Unless otherwise stated, all registers should be accessed with Dword references
on reads, with an appropriate software mask if needed. A software
read/modify/write mechanism should be invoked for partial writes.
Software should write registers containing a Qword address field using only
Qword references. If a system is incapable of issuing Qword references, then 
388 Document Number: 625472, Revision: 1.2b Intel Confidential
writes to the Qword address fields shall be performed using 2 Dword
references; low Dword-first, high-Dword second.
*/
struct xhci_runtime_registers {
    uint32_t mf_index;                      // Microframe Index (offset 0000h)
    uint32_t rsvdz[7];                      // Reserved (offset 001Fh:0004h)
    xhci_interrupter_registers ir[1024];    // Interrupter Register Sets (offset 0020h to 8000h)
};

#endif // XHCI_REGS_H
