#ifndef XHCI_DEVICE_H
#define XHCI_DEVICE_H

#include "xhci_device_ctx.h"
#include "xhci_rings.h"

class xhci_device {
public:
    // port is a 1-based port ID
    xhci_device(uint8_t port, uint8_t slot, uint8_t speed, bool use_64byte_ctx);
    ~xhci_device() = default;

    uint8_t get_port() const { return m_port; }
    uint8_t get_port_regset_idx() const { return m_port - 1; }
    uint8_t get_slot() const { return m_slot; }
    uint8_t get_speed() const { return m_speed; }

    uintptr_t get_input_ctx_dma() const { return m_input_ctx_dma_addr; }

    xhci_input_control_context32*   get_input_ctrl_ctx();
    xhci_slot_context32*            get_input_slot_ctx();
    xhci_endpoint_context32*        get_input_ctrl_ep_ctx();
    xhci_endpoint_context32*        get_input_ep_ctx(uint8_t endpoint_num);

    // Copies data from the output device context into the input context
    void sync_input_ctx(void* out_ctx);

private:
    const uint8_t m_port;           // 1-based port ID
    const uint8_t m_slot;           // slot index in the xhci DCBAA
    const uint8_t m_speed;          // port speed
    const bool    m_use64byte_ctx;

    void*         m_input_ctx;
    uintptr_t     m_input_ctx_dma_addr;

private:
    void _alloc_input_ctx();
};

#endif // XHCI_DEVICE_H
