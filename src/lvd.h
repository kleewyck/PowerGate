/*
 *
 *   LVD.h
 *
 */
#ifndef LVD_H_
#define LVD_H_
#include "common/cs_dbg.h"
#include "common/mbuf.h"
#include "common/json_utils.h"
#include "common/platform.h"
#include "frozen/frozen.h"
#include "fw/src/mgos_app.h"
#include "fw/src/mgos_gpio.h"
#include "fw/src/mgos_mongoose.h"
#include "fw/src/mgos_sys_config.h"
#include "fw/src/mgos_hal.h"
#include "fw/src/mgos_timers.h"
#include "fw/src/mgos_app.h"
#include "mgos_wifi.h"
#include "fw/src/mgos_adc.h"

// Define JSON formats for different data outputs
#define STATUS_FMT "{cpu: %d, ssid: %Q, ip: %Q}"
#define JSON_STATUS_FMT "{cpu: %d, ssid: \"%s\", ip: \"%s\", bvolt: %.2f, psvolt: %.2f, outvolt: %.2f, current: %.2f,relaysta: %d, uptime: %lf}"


// Definitions
#define GPIO_LED           16
#define GPIO_MUX0          14
#define GPIO_MUX1          12
#define GPIO_RELAY_SET     13
#define GPIO_RELAY_RST     15
#define GPIO_BUTTON       2
#define SSID_SIZE          32       // Maximum SSID size
#define PASSWORD_SIZE      64       // Maximum password size

#define CALLBACK_PERIOD   2000

struct device_settings {
  char ssid[SSID_SIZE];
  char pswd[PASSWORD_SIZE];
};

typedef enum  {
    ePS_VOLTAGE = 0,
    eBATT_VOLTAGE = 1,
    eOUT_VOLTAGE = 2,
    eOUT_CURRENT = 3,
  } voltage_sources_t;

typedef enum
{
        eRELAY_OFF = 0,
        eRELAY_ON = 1,
        eRELAY_FAIL = 2
} relay_status_t;

typedef enum
{
        ePOWERED_ON = 0,
        ePOWERED_OFF = 1
} output_status_t;

typedef enum
{
        eSYS_NORMAL = 0,
        eOVER_VOLTAGE_FAULT = 1,
        eLOW_VOLTATE_FAULT = 2
} system_state_t;

// System Data Structure
typedef struct
{
    uint8_t     relayCommand;
    float       batt_voltage;
    float       ps_voltage;
    float       out_voltage;
    float       out_current;
    double      upTime;
    uint8_t     relayStatus;
    bool        buttonStatus;
    uint8_t     systemState;
    uint8_t     swVersion[2];
    uint8_t     hwVersion[2];
    int         low_voltage_disconnect;
    int         low_voltage_reconnect;
    int         high_voltage_disconnect;
    int         high_voltage_reconnect;
    int         high_current_disconnect;
    int         voltage_delay;
    int         current_delay;
    int         cpu_usage;
    char        jsonString[512];

} dpt_system_t;
//
// Procedure Calls
//
void relayOn(dpt_system_t *systemValues);
void relayOff(dpt_system_t *systemValues);
void lvdInit(struct sys_config *device_cfg,dpt_system_t *systemValues );
float getVoltage(voltage_sources_t selVoltage);

#endif
