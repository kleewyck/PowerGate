#include "lvd.h"
#include "blynk.h"

static struct mg_connection *s_blynk_conn = NULL;
static blynk_handler_t s_blynk_handler = NULL;
static void *s_user_data = NULL;
static int s_ping_interval_sec = 2;

int s_reconnect_interval_ms = 3000;
/***
 * Setup Hander pointers
 ***/
void blynk_set_handler(blynk_handler_t func, void *user_data) {
  s_blynk_handler = func;
  s_user_data = user_data;
}
/***
 * Routing to map Network Int to Internal Integer
 ***/

static uint16_t getuint16(const uint8_t *buf) {
  return buf[0] << 8 | buf[1];
}
/***
 * Sent Packet to Blink server
 ***/
void blynk_send(struct mg_connection *c, uint8_t type, uint16_t id,
                const void *data, uint16_t len) {
  static uint16_t cnt;
  uint8_t header[BLYNK_HEADER_SIZE];

  if (id == 0) id = ++cnt;

  LOG(LL_DEBUG, ("BLYNK SEND type %hhu, id %hu, len %hu", type, id, len));
  header[0] = type;
  header[1] = (id >> 8) & 0xff;
  header[2] = id & 0xff;
  header[3] = (len >> 8) & 0xff;
  header[4] = len & 0xff;

  mg_send(c, header, sizeof(header));
  mg_send(c, data, len);
}
/***
 * Format String for Blynk print
 ***/
static void blynk_printf(struct mg_connection *c, uint8_t type, uint16_t id,
                         const char *fmt, ...) {
  char buf[100];
  int len;
  va_list ap;
  va_start(ap, fmt);
  len = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  blynk_send(c, type, id, buf, len);
}

void default_blynk_handler(struct mg_connection *c, const char *cmd,
                                  int pin, int val, void *user_data) {
  dpt_system_t *systemValues = user_data;
  LOG(LL_INFO, ("**Blynk Handler: cmd %s, Pin %d",cmd, pin));
  if (strcmp(cmd, "vr") == 0) {
    if (pin == VIRT_CURRENT_PIN) {
      blynk_printf(c, BLYNK_HARDWARE, 0, "vw%c%d%c%.2f",0, VIRT_CURRENT_PIN, 0,systemValues->out_current/100.0);
      blynk_printf(c, BLYNK_HARDWARE, 0, "vw%c%d%c%.2f",0, VIRT_BATT_V_PIN, 0,systemValues->batt_voltage/100.0);
      blynk_printf(c, BLYNK_HARDWARE, 0, "vw%c%d%c%.2f",0, VIRT_PS_V_PIN, 0,systemValues->ps_voltage/100.0);
      blynk_printf(c, BLYNK_HARDWARE, 0, "vw%c%d%c%.2f",0, VIRT_OUT_V_PIN, 0,systemValues->out_voltage/100.0);
      blynk_printf(c, BLYNK_HARDWARE, 0, "vw%c%d%c%d",0, VIRT_RELAY_PIN, 0,systemValues->relayStatus);

    }
  } else if (strcmp(cmd, "vw") == 0) {
    if (pin == VIRT_RELAY_PIN) {
      if (systemValues->buttonStatus) {
    		relayOff(systemValues);
    		systemValues->buttonStatus = FALSE;
    	} else {
    		relayOn(systemValues);
    		systemValues->buttonStatus = TRUE;
    	}
      (void)val;
    }
  }
}


void handle_blynk_frame(struct mg_connection *c, void *user_data,
                               const uint8_t *data, uint16_t len) {
  LOG(LL_DEBUG, ("BLYNK STATUS: type %hhu, len %hu rlen %hhu", data[0], len,
                 getuint16(data + 3)));
  switch (data[0]) {
    case BLYNK_RESPONSE:
      if (getuint16(data + 3) == 200) {
        LOG(LL_DEBUG, ("BLYNK LOGIN SUCCESS, setting ping timer"));
        mg_set_timer(c, mg_time() + s_ping_interval_sec);
      } else {
        LOG(LL_ERROR, ("BLYNK LOGIN FAILED"));
        c->flags |= MG_F_CLOSE_IMMEDIATELY;
      }
      break;
    case BLYNK_HARDWARE:
      if (len >= 4 && memcmp(data + BLYNK_HEADER_SIZE, "vr", 3) == 0) {
        int i, pin = 0;
        for (i = BLYNK_HEADER_SIZE + 3; i < len; i++) {
          pin *= 10;
          pin += data[i] - '0';
        }
        LOG(LL_DEBUG, ("BLYNK HW: vr %d", pin));
        s_blynk_handler(c, "vr", pin, 0, s_user_data);
      } else if (len >= 4 && memcmp(data + BLYNK_HEADER_SIZE, "vw", 3) == 0) {
        int i, pin = 0, val = 0;
        for (i = BLYNK_HEADER_SIZE + 3; i < len && data[i]; i++) {
          pin *= 10;
          pin += data[i] - '0';
        }
        for (i++; i < len && data[i]; i++) {
          val *= 10;
          val += data[i] - '0';
        }
        LOG(LL_DEBUG, ("BLYNK HW: vw %d %d", pin, val));
        s_blynk_handler(c, "vw", pin, val, s_user_data);
      }
      break;
  }
  (void) user_data;
}
void ev_handler(struct mg_connection *c, int ev, void *ev_data,
                       void *user_data) {
  dpt_system_t *systemValues = user_data;
  switch (ev) {
    case MG_EV_CONNECT:
    LOG(LL_DEBUG, ("ev_handler 1EV : %d",ev));
      LOG(LL_DEBUG, ("BLYNK CONNECT"));
      blynk_send(c, BLYNK_LOGIN, 1, get_cfg()->blynk.auth,
                 strlen(get_cfg()->blynk.auth));
      break;
    case MG_EV_RECV:
    LOG(LL_DEBUG, ("ev_handler 2EV : %d",ev));
      while (c->recv_mbuf.len >= BLYNK_HEADER_SIZE) {
        const uint8_t *buf = (const uint8_t *) c->recv_mbuf.buf;
        uint8_t type = buf[0];
        uint16_t id = getuint16(buf + 1), len = getuint16(buf + 3);
        if (id == 0) {
          c->flags |= MG_F_CLOSE_IMMEDIATELY;
          break;
        }
        if (type == BLYNK_RESPONSE) len = 0;
        if (c->recv_mbuf.len < (size_t) BLYNK_HEADER_SIZE + len) break;
        handle_blynk_frame(c, user_data, buf, BLYNK_HEADER_SIZE + len);
        mbuf_remove(&c->recv_mbuf, BLYNK_HEADER_SIZE + len);
      }
      break;
    case MG_EV_TIMER:
      LOG(LL_DEBUG, ("ev_handler 3EV : %d",ev));
      blynk_printf(c, BLYNK_HARDWARE, 0, "vw%c%d%c%.2f",0, VIRT_CURRENT_PIN, 0,systemValues->out_current/1000.0);
      blynk_printf(c, BLYNK_HARDWARE, 0, "vw%c%d%c%.2f",0, VIRT_BATT_V_PIN, 0,systemValues->batt_voltage/100.0);
      blynk_printf(c, BLYNK_HARDWARE, 0, "vw%c%d%c%.2f",0, VIRT_PS_V_PIN, 0,systemValues->ps_voltage/100.0);
      blynk_printf(c, BLYNK_HARDWARE, 0, "vw%c%d%c%.2f",0, VIRT_OUT_V_PIN, 0,systemValues->out_voltage/100.0);
      blynk_printf(c, BLYNK_HARDWARE, 0, "vw%c%d%c%d",0, VIRT_RELAY_PIN, 0,systemValues->relayStatus);

      blynk_send(c, BLYNK_PING, 0, NULL, 0);

      break;
    case MG_EV_CLOSE:
    LOG(LL_DEBUG, ("ev_handler 4EV : %d",ev));
      LOG(LL_DEBUG, ("BLYNK DISCONNECT"));
      s_blynk_conn = NULL;
      break;
  }
  (void) ev_data;
}

void reconnect_timer_cb(void *arg) {
  if (!get_cfg()->blynk.enable || get_cfg()->blynk.server == NULL ||
      get_cfg()->blynk.auth == NULL || s_blynk_conn != NULL)
    return;
  s_blynk_conn = mgos_connect(get_cfg()->blynk.server, ev_handler, arg);
}
