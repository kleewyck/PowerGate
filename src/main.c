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
char *msg = "Blank is a message";
dpt_system_t systemValues;
// Forward Decliations for functions
static void buttonCallBackHandler(int pin,void *arg);
static struct device_settings s_settings = {"ssid", "password"};
/*
 *
 * handle_get_lvd_data.
 * This module invoked by the HTTP Get request. Data is formatted up into
 * a json packet and returned to the caller to be formatted by the main.js
 *
 */
static void handle_get_lvd_data(struct mg_connection *nc, dpt_system_t *systemValues) {

        struct mbuf fb;
        struct json_out fout = JSON_OUT_MBUF(&fb);
        mbuf_init(&fb, 512);
        LOG(LL_INFO,("Entering Get LVD Data routine"));
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

        /*
         * Use chunked encoding in order to avoid calculating Content-Length
         * Send responce back to remote Browser
         *
         */
        mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n");
        //      mg_printf_http_chunk(nc, f.p);
        mg_send_http_chunk(nc, f.p, f.len);
        mg_send_http_chunk(nc, "", 0);
        nc->flags |= MG_F_SEND_AND_CLOSE;
        mbuf_free(&fb);
        LOG(LL_INFO,("Leavin Get LVD Data routine"));
}
/*
 * SSI handler for SSI events...
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
 * Main event Dispatcher This will call the correct handler routing
 */
static void http_get_ev_handler(struct mg_connection *nc, int ev, void *ev_data, void *user_data) {
  struct http_message *hm = (struct http_message *) ev_data;
  switch (ev) {
    /*
     * Dispatch to the correct uri
     */
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
 * Event Handler for http post events "/updaterelay"
 * When the post comes in update the LVD Board
 */
static void http_post_ev_handler(struct mg_connection *nc, int ev, void *ev_data, void *user_data) {
  struct http_message *hm = (struct http_message *) ev_data;
  LOG(LL_INFO,("****Post Handler Entered"));
  switch (ev) {
    case MG_EV_HTTP_REQUEST:
      if (mg_vcmp(&hm->uri, "/updaterelay") == 0) {
        buttonCallBackHandler(GPIO_BUTTON,user_data);
        /*
         * Send a responce to repaint screen
         */
        //mg_http_send_redirect(nc, 302, mg_mk_str("/"), mg_mk_str(NULL));
      }
      break;
    case MG_EV_SSI_CALL:
      handle_ssi_call(nc, ev_data);
      break;
    default:
      break;
  }
  (void) user_data;
  LOG(LL_INFO,("*****Post Handler exit"));
}
/*
 * Update all datavalues every timer interval
 */
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
/*
 * Timer Call back handler
 * Routine called every Timer interval
 */
static void periodicCallBackHandler(void *arg) {
	dpt_system_t *systemValues = arg;
	printf("Burp %i\r\n",count++);
	mgos_gpio_toggle(GPIO_LED);      /* Blink Status Light*/
	updateStatus(systemValues);      /* Call the Main Data collection routine */
  //update_relay(systemValues);      /* Update relay based on current status  */
}
/*
 * Call back handeler for when the button pressed on the Board
 */
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
/*
 * Mongoos Main Routine This is where we Enter
 */
enum mgos_app_init_result mgos_app_init(void) {
	 /**
	  *
		* Get configuration from Global configuration.
    * These values come from the mos.yml file which get stored in
    * nvar when the ESP8266 gets flashed.
		*
		**/
   device_cfg = get_cfg();
   printf ("mgos_app_init          Device id is: %s \r\n", device_cfg->device.id);
   printf ("mgos_app_init          LVD_APP: %s \r\n", device_cfg->lvd_app.app_name);
   /*
    * Initilize our Low voltage disconnnect Board
    */
   lvdInit(device_cfg,&systemValues);
   /**
	  * Do we have WiFi if we do Setup our SSID and password.
		***/
		if (mgos_wifi_validate_sta_cfg(&device_cfg->wifi.sta, &msg) == false) {
						LOG(LL_DEBUG, ("%s\n", msg));
		}
		if (mgos_wifi_validate_ap_cfg(&device_cfg->wifi.ap, &msg) == false) {
						LOG(LL_DEBUG, ("%s\n", msg));
		}
    /*
     * Register the uri entry points for the get and set routines. These
     * called in form http://1.1.1.1/uri
     */
		LOG(LL_DEBUG, ("registering end points..."));
		mgos_register_http_endpoint("/updaterelay", http_post_ev_handler, &systemValues);
		mgos_register_http_endpoint("/get_lvd_data", http_get_ev_handler, &systemValues);
    /*
		 * Enable Button interupt. Not This specific function handles
     * Contact bounce.
		 */
		 mgos_gpio_set_button_handler(GPIO_BUTTON, MGOS_GPIO_PULL_UP,
                                  MGOS_GPIO_INT_EDGE_NEG,
                                  50, buttonCallBackHandler,
                                  &systemValues);

		/*
		 * Set Timer call back handler. This function set the timer interrupt that
     * calls the handerler every callback period.
		 */
    mgos_set_timer(CALLBACK_PERIOD, true, periodicCallBackHandler, &systemValues);

		/*
		 *  Set up Blynk access, attempt to transfer data to blynk server
     *  Once every s_reconnect_interval_ms
		 */
		 blynk_set_handler(default_blynk_handler, &systemValues);
     mgos_set_timer(s_reconnect_interval_ms, true, reconnect_timer_cb, &systemValues);

  return MGOS_APP_INIT_SUCCESS;
}
