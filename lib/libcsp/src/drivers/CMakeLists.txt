target_sources(csp PRIVATE usart/usart_kiss.c)

if(LIBSOCKETCAN_FOUND)
  add_library(driver_can OBJECT can/can_socketcan.c)
  target_include_directories(driver_can PRIVATE
    ${csp_inc}
    ${LIBSOCKETCAN_INCLUDE_DIRS})
  target_link_libraries(driver_can PRIVATE csp_common
    ${LIBSOCKETCAN_LIBRARIES})
  target_link_libraries(csp PRIVATE driver_can)
  if(BUILD_SHARED_LIBS)
    set_property(TARGET driver_can PROPERTY POSITION_INDEPENDENT_CODE ON)
  endif()
endif()

if(CSP_POSIX)
  target_sources(csp PRIVATE usart/usart_linux.c)
  target_sources(csp PRIVATE eth/eth_linux.c)
elseif(CSP_ZEPHYR)
  target_sources(csp PRIVATE usart/usart_zephyr.c)
endif()

if(HAVE_ZEPHYR_CAN)
  target_sources(csp PRIVATE can/can_zephyr.c)
endif()

# STM32 FreeRTOS Hardware Drivers Selection
if(CSP_SYSTEM_NAME STREQUAL "freertos")
  message(STATUS "Configuring libcsp hardware drivers for STM32 FreeRTOS (CM7)")
  
  # Link the STM32 specific CAN hardware driver
  target_sources(csp PRIVATE can/can_stm32.c)
  target_include_directories(csp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include/csp/drivers)
  
  # If you plan to add a custom UART driver later, you can uncomment the line below:
  # target_sources(csp PRIVATE usart/usart_stm32.c)
endif()