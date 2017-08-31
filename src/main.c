#include <stdio.h>
#include "lvd.h"
#include "blynk.h"
#include "mgos_http_server.h"
/*
 *
 *   BAD Global Variables
 *
 */
struct sys_config *device_cfg;
int count = 0;
char *msg = "This is a message";
dpt_system_t systemValues;
// Forward Decliations for functions
static void buttonCallBackHandler(int pin,void *arg);

static struct device_settings s_settings = {"ssid", "password"};
//
// RElay Control
//


/*
 *handle_get_lvd_data. This reports CPU usage, and also provides connection status
 *
 */
static void handle_get_lvd_data(struct mg_connection *nc, dpt_system_t *systemValues) {

        struct mbuf fb;
        struct json_out fout = JSON_OUT_MBUF(&fb);
        mbuf_init(&fb, 512);

        printf("Entering get_cpu_usage\n\r");
        //json_printf(&fout, STATUS_FMT, systemValues->cpu_usage, mgos_wifi_get_connected_ssid(), mgos_wifi_get_status_str());
				json_printf(&fout,JSON_STATUS_FMT,systemValues->cpu_usage,
					                            mgos_wifi_get_connected_ssid(),
																			mgos_wifi_get_status_str(),
					                            systemValues->batt_voltage,
				                              systemValues->ps_voltage,
																		  systemValues->out_voltage,
																	    systemValues->out_current,
																			systemValues->relayStatus,
																		  systemValues->upTime );
				mbuf_trim(&fb);

        struct mg_str f = mg_mk_str_n(fb.buf, fb.len);  /* convert to string    */

        // Use chunked encoding in order to avoid calculating Content-Length
        mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
        //      mg_printf_http_chunk(nc, f.p);
        mg_send_http_chunk(nc, f.p, f.len);
        mg_send_http_chunk(nc, "", 0);
        nc->flags |= MG_F_SEND_AND_CLOSE;

        // LOG(LL_INFO, ("%s\n", f.p));

        mbuf_free(&fb);
        printf("Leaving get_cpu_usage\n\r");
}
/*
 * SSI handler for SSI events...
 * Don't need this now... May delete
 *
 */
static void handle_ssi_call(struct mg_connection *nc, const char *param) {
  if (strcmp(param, "ssid") == 0) {
    mg_printf_html_escape(nc, "%s", s_settings.ssid);
  }
  else if (strcmp(param, "password") == 0) {
    mg_printf_html_escape(nc, "%s", s_settings.pswd);
  }
}

/*
 * Event Handler for http get events
 *
 */
static void http_get_ev_handler(struct mg_connection *nc, int ev, void *ev_data, void *user_data) {
  struct http_message *hm = (struct http_message *) ev_data;

  // printf("Event: %d, uri: %s\n", ev, hm->uri.p);

  switch (ev) {
    case MG_EV_HTTP_REQUEST:
      if (mg_vcmp(&hm->uri, "/get_lvd_data") == 0) {
        handle_get_lvd_data(nc,user_data);
      }
      else {
        mg_http_send_redirect(nc, 302, mg_mk_str("/"), mg_mk_str(NULL));
      }
      break;
    case MG_EV_SSI_CALL:
      handle_ssi_call(nc, ev_data);
      break;
    default:
      break;
  }
  (void) user_data;
}
/*
 * Event Handler for http post events "/save"
 *
 */

static void http_post_ev_handler(struct mg_connection *nc, int ev, void *ev_data, void *user_data) {
  struct http_message *hm = (struct http_message *) ev_data;

  switch (ev) {
    case MG_EV_HTTP_REQUEST:
      if (mg_vcmp(&hm->uri, "/save") == 0) {
        buttonCallBackHandler(GPIO_BUTTON,user_data);
      }
      break;
    case MG_EV_SSI_CALL:
      handle_ssi_call(nc, ev_data);
      break;
    default:
      break;
  }
  (void) user_data;
}



static void updateStatus(dpt_system_t *systemValues) {
	systemValues->ps_voltage = getVoltage(ePS_VOLTAGE);
	systemValues->batt_voltage = getVoltage(eBATT_VOLTAGE);
	systemValues->out_voltage = getVoltage(eOUT_VOLTAGE);
	systemValues->out_current = getVoltage(eOUT_CURRENT);

  systemValues->upTime = mgos_uptime();
	systemValues->cpu_usage = ((double)mgos_get_free_heap_size()/(double)mgos_get_heap_size()) * 100.0;

	sprintf(systemValues->jsonString,JSON_STATUS_FMT,systemValues->cpu_usage,
		                            mgos_wifi_get_connected_ssid(),
																mgos_wifi_get_status_str(),
		                            systemValues->batt_voltage,
	                              systemValues->ps_voltage,
															  systemValues->out_voltage,
														    systemValues->out_current,
																systemValues->relayStatus,
															  systemValues->upTime);
	LOG(LL_INFO,("%s",systemValues->jsonString));
}


static void periodicCallBackHandler(void *arg) {
	dpt_system_t *systemValues = arg;
	printf("Burp %i\r\n",count++);
	mgos_gpio_toggle(GPIO_LED);
	updateStatus(systemValues);
  update_relay(systemValues);
}

static void buttonCallBackHandler(int pin,void *arg) {
	dpt_system_t *systemValues = arg;
	LOG(LL_INFO,("Button Handler Called for Pin: %i Button state: %i",pin,systemValues->buttonStatus));
	if (systemValues->buttonStatus) {
		relayOff(systemValues);
		systemValues->buttonStatus = FALSE;
	} else {
		relayOn(systemValues);
		systemValues->buttonStatus = TRUE;
	}

}

enum mgos_app_init_result mgos_app_init(void) {
	 /**
	  *
		*Get configuration from Global configuration
		*
		**/
   device_cfg = get_cfg();
   printf ("mgos_app_init          Device id is: %s \r\n", device_cfg->device.id);
   printf ("mgos_app_init          LVD_APP: %s \r\n", device_cfg->lvd_app.app_name);
   lvdInit(device_cfg,&systemValues);
   /**
	  * Do we have WiFi Setup
		***/
		if (mgos_wifi_validate_sta_cfg(&device_cfg->wifi.sta, &msg) == false) {
						LOG(LL_DEBUG, ("%s\n", msg));
		}
		if (mgos_wifi_validate_ap_cfg(&device_cfg->wifi.ap, &msg) == false) {
						LOG(LL_DEBUG, ("%s\n", msg));
		}
		LOG(LL_DEBUG, ("registering end points..."));
		mgos_register_http_endpoint("/save", http_post_ev_handler, &systemValues);
		mgos_register_http_endpoint("/get_lvd_data", http_get_ev_handler, &systemValues);
    /*
		 * Enable Button interupt
		 */
		 mgos_gpio_set_button_handler(GPIO_BUTTON, MGOS_GPIO_PULL_NONE,
                                  MGOS_GPIO_INT_EDGE_POS,
                                  50, buttonCallBackHandler,
                                  &systemValues);

		/*
		 * Set Timer call back handler
		 */
    mgos_set_timer(CALLBACK_PERIOD, true, periodicCallBackHandler, &systemValues);

		/*
		 *  Set up Blynk access
		 */
		 blynk_set_handler(default_blynk_handler, &systemValues);
     mgos_set_timer(s_reconnect_interval_ms, true, reconnect_timer_cb, &systemValues);

  return MGOS_APP_INIT_SUCCESS;
}
