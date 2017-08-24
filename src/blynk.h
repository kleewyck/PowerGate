#ifndef LVD_BLYNK_H_
#define LVD_BLYNK_H_


#define BLYNK_HEADER_SIZE 5
#define BLYNK_RESPONSE 0
#define BLYNK_LOGIN 2
#define BLYNK_PING 6
#define BLYNK_HARDWARE 20
//
// Virtual Pins
//
#define VIRT_CURRENT_PIN  0
#define VIRT_BATT_V_PIN   1
#define VIRT_PS_V_PIN     2
#define VIRT_OUT_V_PIN    3
#define VIRT_RELAY_PIN    4
//
// External Variables
//
extern int s_reconnect_interval_ms;

typedef void (*blynk_handler_t)(struct mg_connection *, const char *cmd,
                                int pin, int val, void *user_data);

void blynk_send(struct mg_connection *c, uint8_t type, uint16_t id,
                const void *data, uint16_t len);
void blynk_set_handler(blynk_handler_t func, void *user_data);
void default_blynk_handler(struct mg_connection *c, const char *cmd,
                              int pin, int val, void *user_data);
void handle_blynk_frame(struct mg_connection *c, void *user_data,
                        const uint8_t *data, uint16_t len);
void ev_handler(struct mg_connection *c, int ev, void *ev_data,
                        void *user_data);
void reconnect_timer_cb(void *arg);

#endif
