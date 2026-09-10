#ifndef __APP_PORT_MONITOR_H
#define __APP_PORT_MONITOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "APP_I2C.h"

#define APP_PORT_MONITOR_DEVICE_COUNT  4U
#define APP_PORT_MONITOR_PORT_COUNT    8U
#define APP_PORT_MONITOR_POLL_MS       500U

typedef enum
{
  APP_PORT_C1 = 0,
  APP_PORT_C2,
  APP_PORT_C3,
  APP_PORT_C4,
  APP_PORT_C5,
  APP_PORT_C6,
  APP_PORT_A1,
  APP_PORT_A2
} APP_PortMonitorPortId;

typedef enum
{
  APP_PORT_MONITOR_RESULT_NOT_RUN = 0,
  APP_PORT_MONITOR_RESULT_OK,
  APP_PORT_MONITOR_RESULT_I2C_ERROR,
  APP_PORT_MONITOR_RESULT_CHECKSUM_ERROR
} APP_PortMonitorResult;

typedef struct
{
  uint8_t port_connected[APP_PORT_MONITOR_PORT_COUNT];
  uint16_t port_voltage_mv[APP_PORT_MONITOR_PORT_COUNT];
  uint16_t port_current_ma[APP_PORT_MONITOR_PORT_COUNT];
  uint8_t port_power_w[APP_PORT_MONITOR_PORT_COUNT];
  uint8_t device_temperature_c[APP_PORT_MONITOR_DEVICE_COUNT];
  uint8_t max_temperature_c;
  uint8_t valid;
  uint32_t sequence;
  uint32_t last_update_tick;
} APP_PortMonitorData;

typedef struct
{
  uint32_t poll_attempt_count;
  uint32_t successful_cycle_count;
  uint32_t failed_cycle_count;
  uint32_t checksum_error_count;
  uint32_t i2c_error_count;
  uint8_t last_failed_address;
  uint8_t last_failed_command;
  HAL_StatusTypeDef last_i2c_status;
  APP_PortMonitorResult last_result;
} APP_PortMonitorDiagnostics;

extern APP_PortMonitorData g_app_port_monitor_data;
extern APP_PortMonitorDiagnostics g_app_port_monitor_diagnostics;

void APP_PortMonitor_Init(void);
uint8_t APP_PortMonitor_Poll(uint32_t now);
const APP_PortMonitorData *APP_PortMonitor_GetSnapshot(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_PORT_MONITOR_H */
