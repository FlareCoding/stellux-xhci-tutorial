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

typedef struct xhci_command_completion_request_block {
    uint64_t command_trb_pointer;
    struct {
        uint32_t rsvd0           : 24;
        uint32_t completion_code : 8;
    };
    struct {
        uint32_t cycle_bit   : 1;
        uint32_t rsvd1       : 9;
        uint32_t trb_type    : 6;
        uint32_t vfid        : 8;
        uint32_t slot_id     : 8;
    };
} xhci_command_completion_trb_t;
static_assert(sizeof(xhci_command_completion_trb_t) == sizeof(uint32_t) * 4);

static inline const char* trb_completion_code_to_string(uint8_t completion_code) {
    switch (completion_code) {
    case XHCI_TRB_COMPLETION_CODE_INVALID:
        return "INVALID";
    case XHCI_TRB_COMPLETION_CODE_SUCCESS:
        return "SUCCESS";
    case XHCI_TRB_COMPLETION_CODE_DATA_BUFFER_ERROR:
        return "DATA_BUFFER_ERROR";
    case XHCI_TRB_COMPLETION_CODE_BABBLE_DETECTED_ERROR:
        return "BABBLE_DETECTED_ERROR";
    case XHCI_TRB_COMPLETION_CODE_USB_TRANSACTION_ERROR:
        return "USB_TRANSACTION_ERROR";
    case XHCI_TRB_COMPLETION_CODE_TRB_ERROR:
        return "TRB_ERROR";
    case XHCI_TRB_COMPLETION_CODE_STALL_ERROR:
        return "STALL_ERROR";
    case XHCI_TRB_COMPLETION_CODE_RESOURCE_ERROR:
        return "RESOURCE_ERROR";
    case XHCI_TRB_COMPLETION_CODE_BANDWIDTH_ERROR:
        return "BANDWIDTH_ERROR";
    case XHCI_TRB_COMPLETION_CODE_NO_SLOTS_AVAILABLE:
        return "NO_SLOTS_AVAILABLE";
    case XHCI_TRB_COMPLETION_CODE_INVALID_STREAM_TYPE:
        return "INVALID_STREAM_TYPE";
    case XHCI_TRB_COMPLETION_CODE_SLOT_NOT_ENABLED:
        return "SLOT_NOT_ENABLED";
    case XHCI_TRB_COMPLETION_CODE_ENDPOINT_NOT_ENABLED:
        return "ENDPOINT_NOT_ENABLED";
    case XHCI_TRB_COMPLETION_CODE_SHORT_PACKET:
        return "SHORT_PACKET";
    case XHCI_TRB_COMPLETION_CODE_RING_UNDERRUN:
        return "RING_UNDERRUN";
    case XHCI_TRB_COMPLETION_CODE_RING_OVERRUN:
        return "RING_OVERRUN";
    case XHCI_TRB_COMPLETION_CODE_VF_EVENT_RING_FULL:
        return "VF_EVENT_RING_FULL";
    case XHCI_TRB_COMPLETION_CODE_PARAMETER_ERROR:
        return "PARAMETER_ERROR";
    case XHCI_TRB_COMPLETION_CODE_BANDWIDTH_OVERRUN:
        return "BANDWIDTH_OVERRUN";
    case XHCI_TRB_COMPLETION_CODE_CONTEXT_STATE_ERROR:
        return "CONTEXT_STATE_ERROR";
    case XHCI_TRB_COMPLETION_CODE_NO_PING_RESPONSE:
        return "NO_PING_RESPONSE";
    case XHCI_TRB_COMPLETION_CODE_EVENT_RING_FULL:
        return "EVENT_RING_FULL";
    case XHCI_TRB_COMPLETION_CODE_INCOMPATIBLE_DEVICE:
        return "INCOMPATIBLE_DEVICE";
    case XHCI_TRB_COMPLETION_CODE_MISSED_SERVICE:
        return "MISSED_SERVICE";
    case XHCI_TRB_COMPLETION_CODE_COMMAND_RING_STOPPED:
        return "COMMAND_RING_STOPPED";
    case XHCI_TRB_COMPLETION_CODE_COMMAND_ABORTED:
        return "COMMAND_ABORTED";
    case XHCI_TRB_COMPLETION_CODE_STOPPED:
        return "STOPPED";
    case XHCI_TRB_COMPLETION_CODE_STOPPED_LENGTH_INVALID:
        return "STOPPED_LENGTH_INVALID";
    case XHCI_TRB_COMPLETION_CODE_STOPPED_SHORT_PACKET:
        return "STOPPED_SHORT_PACKET";
    case XHCI_TRB_COMPLETION_CODE_MAX_EXIT_LATENCY_ERROR:
        return "MAX_EXIT_LATENCY_ERROR";
    default:
        return "UNKNOWN_COMPLETION_CODE";
    }
}

static inline const char* trb_type_to_string(uint8_t trb_type) {
    switch (trb_type) {
    case XHCI_TRB_TYPE_RESERVED: return "XHCI_TRB_TYPE_RESERVED";
    case XHCI_TRB_TYPE_NORMAL: return "XHCI_TRB_TYPE_NORMAL";
    case XHCI_TRB_TYPE_SETUP_STAGE: return "XHCI_TRB_TYPE_SETUP_STAGE";
    case XHCI_TRB_TYPE_DATA_STAGE: return "XHCI_TRB_TYPE_DATA_STAGE";
    case XHCI_TRB_TYPE_STATUS_STAGE: return "XHCI_TRB_TYPE_STATUS_STAGE";
    case XHCI_TRB_TYPE_ISOCH: return "XHCI_TRB_TYPE_ISOCH";
    case XHCI_TRB_TYPE_LINK: return "XHCI_TRB_TYPE_LINK";
    case XHCI_TRB_TYPE_EVENT_DATA: return "XHCI_TRB_TYPE_EVENT_DATA";
    case XHCI_TRB_TYPE_NOOP: return "XHCI_TRB_TYPE_NOOP";
    case XHCI_TRB_TYPE_ENABLE_SLOT_CMD: return "XHCI_TRB_TYPE_ENABLE_SLOT_CMD";
    case XHCI_TRB_TYPE_DISABLE_SLOT_CMD: return "XHCI_TRB_TYPE_DISABLE_SLOT_CMD";
    case XHCI_TRB_TYPE_ADDRESS_DEVICE_CMD: return "XHCI_TRB_TYPE_ADDRESS_DEVICE_CMD";
    case XHCI_TRB_TYPE_CONFIGURE_ENDPOINT_CMD: return "XHCI_TRB_TYPE_CONFIGURE_ENDPOINT_CMD";
    case XHCI_TRB_TYPE_EVALUATE_CONTEXT_CMD: return "XHCI_TRB_TYPE_EVALUATE_CONTEXT_CMD";
    case XHCI_TRB_TYPE_RESET_ENDPOINT_CMD: return "XHCI_TRB_TYPE_RESET_ENDPOINT_CMD";
    case XHCI_TRB_TYPE_STOP_ENDPOINT_CMD: return "XHCI_TRB_TYPE_STOP_ENDPOINT_CMD";
    case XHCI_TRB_TYPE_SET_TR_DEQUEUE_PTR_CMD: return "XHCI_TRB_TYPE_SET_TR_DEQUEUE_PTR_CMD";
    case XHCI_TRB_TYPE_RESET_DEVICE_CMD: return "XHCI_TRB_TYPE_RESET_DEVICE_CMD";
    case XHCI_TRB_TYPE_FORCE_EVENT_CMD: return "XHCI_TRB_TYPE_FORCE_EVENT_CMD";
    case XHCI_TRB_TYPE_NEGOTIATE_BANDWIDTH_CMD: return "XHCI_TRB_TYPE_NEGOTIATE_BANDWIDTH_CMD";
    case XHCI_TRB_TYPE_SET_LATENCY_TOLERANCE_VALUE_CMD: return "XHCI_TRB_TYPE_SET_LATENCY_TOLERANCE_VALUE_CMD";
    case XHCI_TRB_TYPE_GET_PORT_BANDWIDTH_CMD: return "XHCI_TRB_TYPE_GET_PORT_BANDWIDTH_CMD";
    case XHCI_TRB_TYPE_FORCE_HEADER_CMD: return "XHCI_TRB_TYPE_FORCE_HEADER_CMD";
    case XHCI_TRB_TYPE_NOOP_CMD: return "XHCI_TRB_TYPE_NOOP_CMD";
    case XHCI_TRB_TYPE_GET_EXTENDED_PROPERTY_CMD: return "XHCI_TRB_TYPE_GET_EXTENDED_PROPERTY_CMD";
    case XHCI_TRB_TYPE_SET_EXTENDED_PROPERTY_CMD: return "XHCI_TRB_TYPE_SET_EXTENDED_PROPERTY_CMD";
    case XHCI_TRB_TYPE_TRANSFER_EVENT: return "XHCI_TRB_TYPE_TRANSFER_EVENT";
    case XHCI_TRB_TYPE_CMD_COMPLETION_EVENT: return "XHCI_TRB_TYPE_CMD_COMPLETION_EVENT";
    case XHCI_TRB_TYPE_PORT_STATUS_CHANGE_EVENT: return "XHCI_TRB_TYPE_PORT_STATUS_CHANGE_EVENT";
    case XHCI_TRB_TYPE_BANDWIDTH_REQUEST_EVENT: return "XHCI_TRB_TYPE_BANDWIDTH_REQUEST_EVENT";
    case XHCI_TRB_TYPE_DOORBELL_EVENT: return "XHCI_TRB_TYPE_DOORBELL_EVENT";
    case XHCI_TRB_TYPE_HOST_CONTROLLER_EVENT: return "XHCI_TRB_TYPE_HOST_CONTROLLER_EVENT";
    case XHCI_TRB_TYPE_DEVICE_NOTIFICATION_EVENT: return "XHCI_TRB_TYPE_DEVICE_NOTIFICATION_EVENT";
    default: return "UNKNOWN_TRB_TYPE";
    }
}

#endif // XHCI_TRB_H

