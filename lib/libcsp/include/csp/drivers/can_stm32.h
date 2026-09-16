#ifndef CAN_STM32_H
#define CAN_STM32_H

#include <csp/csp.h>
#include <csp/interfaces/csp_if_can.h>
#include <stdbool.h>

int csp_can_stm32_open_and_add_interface(uint16_t address,
                                         uint32_t bitrate,
                                         bool promisc,
                                         csp_iface_t ** return_iface);

int csp_can_stm32_stop(csp_iface_t * iface);

/* Call this from the router task */
void csp_can_stm32_process_rx(void);

#endif
