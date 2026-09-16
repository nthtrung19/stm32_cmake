/*
 * can_stm32.c - STM32 FDCAN driver for libcsp (External Loopback test)
 */

#include "can_stm32.h"

#include "main.h"
#include <string.h>
#include <stdio.h>

#include <csp/csp.h>
#include <csp/csp_id.h>
#include <csp/interfaces/csp_if_can.h>

#include "FreeRTOS.h"
#include "queue.h"

extern FDCAN_HandleTypeDef hfdcan1;

/* ---------- Driver context ---------- */
typedef struct {
    char name[CSP_IFLIST_NAME_MAX + 1];
    csp_iface_t iface;
    csp_can_interface_data_t ifdata;
    FDCAN_HandleTypeDef * hfdcan;
} can_context_t;

static can_context_t ctx;

/* ---------- RX queue (ISR-safe) ---------- */
typedef struct {
    uint32_t id;
    uint8_t  data[8];
    uint8_t  dlc;
} can_rx_frame_t;

#define CAN_RX_QUEUE_LEN  32
static QueueHandle_t can_rx_queue = NULL;

/* ============================================================
 * TX function – must match csp_can_driver_tx_t of this libcsp 1.4
 * Signature: int (*)(void *, uint32_t, const uint8_t *, uint8_t)
 * ============================================================ */
static int csp_can_tx_frame(void * driver_data, uint32_t id,
                            const uint8_t * data, uint8_t dlc)
{
    can_context_t * c = (can_context_t *)driver_data;

    if (dlc > 8) {
        return CSP_ERR_INVAL;
    }

    uint32_t timeout = HAL_GetTick() + 50;
    while (HAL_FDCAN_GetTxFifoFreeLevel(c->hfdcan) == 0) {
        if (HAL_GetTick() >= timeout) {
            return CSP_ERR_TX;
        }
    }

    FDCAN_TxHeaderTypeDef tx_header = {0};
    tx_header.Identifier          = id;
    tx_header.IdType              = FDCAN_EXTENDED_ID;
    tx_header.TxFrameType         = FDCAN_DATA_FRAME;
    tx_header.DataLength          = dlc;
    tx_header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    tx_header.BitRateSwitch       = FDCAN_BRS_OFF;
    tx_header.FDFormat            = FDCAN_CLASSIC_CAN;
    tx_header.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    tx_header.MessageMarker       = 0;

    if (HAL_FDCAN_AddMessageToTxFifoQ(c->hfdcan, &tx_header, (uint8_t *)data) != HAL_OK) {
        return CSP_ERR_TX;
    }

    return CSP_ERR_NONE;
}

/* ============================================================
 * RX Callback – only enqueue (ISR safe)
 * ============================================================ */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef * hfdcan, uint32_t RxFifo0ITs)
{
    if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) == 0) {
        return;
    }

    FDCAN_RxHeaderTypeDef rx_header;
    uint8_t data[8];
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    while (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &rx_header, data) == HAL_OK) {

        if (rx_header.IdType != FDCAN_EXTENDED_ID) {
            continue;
        }
        if (rx_header.RxFrameType == FDCAN_REMOTE_FRAME) {
            continue;
        }

        can_rx_frame_t frame;
        frame.id  = rx_header.Identifier;
        frame.dlc = (rx_header.DataLength > 8) ? 8 : (uint8_t)rx_header.DataLength;
        memcpy(frame.data, data, frame.dlc);

        if (can_rx_queue != NULL) {
            xQueueSendFromISR(can_rx_queue, &frame, &xHigherPriorityTaskWoken);
        }
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/* ============================================================
 * Process RX queue – call from router task / RX task
 * ============================================================ */
void csp_can_stm32_process_rx(void)
{
    can_rx_frame_t frame;

    while (can_rx_queue != NULL &&
           xQueueReceive(can_rx_queue, &frame, 0) == pdTRUE) {

        /* 5-argument form matching your libcsp version */
        csp_can_rx(&ctx.iface, frame.id, frame.data, frame.dlc, NULL);
    }
}

/* ============================================================
 * Open + register interface
 * ============================================================ */
int csp_can_stm32_open_and_add_interface(uint16_t address,
                                         uint32_t bitrate,
                                         bool promisc,
                                         csp_iface_t ** return_iface)
{
    (void)bitrate;
    (void)promisc;

    memset(&ctx, 0, sizeof(ctx));

    strncpy(ctx.name, CSP_IF_CAN_DEFAULT_NAME, sizeof(ctx.name) - 1);
    ctx.iface.name           = ctx.name;
    ctx.iface.addr           = address;
    ctx.iface.interface_data = &ctx.ifdata;
    ctx.iface.driver_data    = &ctx;
    ctx.ifdata.tx_func       = csp_can_tx_frame;   /* now correct type */
    ctx.ifdata.pbufs         = NULL;
    ctx.hfdcan               = &hfdcan1;

    if (can_rx_queue == NULL) {
        can_rx_queue = xQueueCreate(CAN_RX_QUEUE_LEN, sizeof(can_rx_frame_t));
        if (can_rx_queue == NULL) {
            return CSP_ERR_NOMEM;
        }
    }

    /* Optional: you can keep your FDCAN re-init here if needed.
       If MX_FDCAN1_Init() already configured the peripheral,
       you may skip re-init and only start + enable notification. */

    if (HAL_FDCAN_ActivateNotification(&hfdcan1, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK) {
        return CSP_ERR_DRIVER;
    }

    if (HAL_FDCAN_Start(&hfdcan1) != HAL_OK) {
        return CSP_ERR_DRIVER;
    }

    int res = csp_can_add_interface(&ctx.iface);
    if (res != CSP_ERR_NONE) {
        return res;
    }

    if (return_iface) {
        *return_iface = &ctx.iface;
    }

    return CSP_ERR_NONE;
}

int csp_can_stm32_stop(csp_iface_t * iface)
{
    if (iface == NULL) {
        return CSP_ERR_INVAL;
    }

    HAL_FDCAN_Stop(&hfdcan1);
    csp_can_remove_interface(iface);

    return CSP_ERR_NONE;
}