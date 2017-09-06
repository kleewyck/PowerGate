#include "lvd.h"
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

float out_cur_readings[20];
uint8_t ocr_idx = 0;
char output_buffer[256];
/**
 * Output readings
 */
void output_readings(dpt_system_t *systemValues) {
  unsigned int watts = systemValues->out_voltage/100*systemValues->out_current;
  sprintf(output_buffer, "B:%.4lu:P:%.4lu:O:%.4lu:A:%.4lu:R:%.1i:W:%.4u:%lf",
							 systemValues->batt_voltage,
							 systemValues->ps_voltage,
							 systemValues->out_voltage,
							 systemValues->out_current,
							 systemValues->relayStatus,
							 watts,
							 systemValues->upTime/1000);
  LOG(LL_INFO,(output_buffer));
}

void relayOn(dpt_system_t *systemValues){
	LOG(LL_INFO,("Relay on"));
	mgos_gpio_write(GPIO_RELAY_SET, 1);
	mgos_usleep(25000);
	mgos_gpio_write(GPIO_RELAY_SET, 0);
	systemValues->relayStatus = eRELAY_ON;
}

void relayOff(dpt_system_t *systemValues){
	LOG(LL_INFO,("Relay off"));
	mgos_gpio_write(GPIO_RELAY_RST, 1);
	mgos_usleep(25000);
	mgos_gpio_write(GPIO_RELAY_RST, 0);
	systemValues->relayStatus = eRELAY_OFF;
}

/**
 *
 * Set up GPIO and other global Elements
 *
 **/
void lvdInit(struct sys_config *device_cfg,dpt_system_t *systemValues ) {
	 LOG(LL_DEBUG, ("Initialize GPIO ports \n"));
	 mgos_gpio_set_mode(GPIO_LED, MGOS_GPIO_MODE_OUTPUT);
	 mgos_gpio_set_pull(GPIO_LED, MGOS_GPIO_PULL_NONE);
	 mgos_gpio_set_mode(GPIO_MUX0, MGOS_GPIO_MODE_OUTPUT);
	 mgos_gpio_set_pull(GPIO_MUX0, MGOS_GPIO_PULL_DOWN);
	 mgos_gpio_set_mode(GPIO_MUX1, MGOS_GPIO_MODE_OUTPUT);
	 mgos_gpio_set_pull(GPIO_MUX1, MGOS_GPIO_PULL_DOWN);
	 mgos_gpio_set_mode(GPIO_RELAY_SET, MGOS_GPIO_MODE_OUTPUT);
	 mgos_gpio_set_pull(GPIO_RELAY_SET, MGOS_GPIO_PULL_NONE);
	 mgos_gpio_set_mode(GPIO_RELAY_RST, MGOS_GPIO_MODE_OUTPUT);
	 mgos_gpio_set_pull(GPIO_RELAY_RST, MGOS_GPIO_PULL_NONE);

   // Set up ADC
	 mgos_adc_enable(0);
	 // Get configuration parameters
	 systemValues->low_voltage_disconnect = device_cfg->lvd_app.low_voltage_disconnect;
	 systemValues->low_voltage_reconnect = device_cfg->lvd_app.low_voltage_reconnect;
	 systemValues->high_voltage_disconnect = device_cfg->lvd_app.high_voltage_disconnect;
	 systemValues->high_voltage_reconnect = device_cfg->lvd_app.high_voltage_reconnect;
	 systemValues->high_current_disconnect = device_cfg->lvd_app.high_current_disconnect;
	 systemValues->voltage_delay = device_cfg->lvd_app.voltage_delay;
	 systemValues->current_delay = device_cfg->lvd_app.current_delay;
	 systemValues->buttonStatus = FALSE;
	 systemValues->relayStatus = eRELAY_OFF;
	 relayOff(systemValues);

}

long getVoltage(voltage_sources_t selVoltage) {
	float x;
	float adcValue = 0.0;
	float normalizedVoltage = 0;
  long returnValue = 0;
	/*
	* Select Mux port to read
	*/
	int MUX0_VALUE = selVoltage & 0x01;
	int MUX1_VALUE = (selVoltage >> 1) & 0x01;
  mgos_gpio_write(GPIO_MUX0, MUX0_VALUE);
	mgos_gpio_write(GPIO_MUX1, MUX1_VALUE);
	mgos_usleep(1000);
	adcValue = (float)mgos_adc_read(0);
		/*
	 * Normalize the values
	 */
	 switch (selVoltage) {
	    case eBATT_VOLTAGE:
	    case ePS_VOLTAGE:
	    case eOUT_VOLTAGE:
			    x = adcValue/1023.0;        // 1024 step ADC
          x = x * 1220000.0/220000.0; // First Voltage devider 1M and 220K
			    normalizedVoltage = x*12.0/2.0; // LVD Voltage Divider 10k and 2K
          printf("Normalized Voltage at Divider %f\n\r",normalizedVoltage);
          returnValue = (long)(normalizedVoltage *100.0);
	        break;
	    case eOUT_CURRENT:
			    //out_cur_readings[ocr_idx] = (5.0*(float)analogRead(AMP_PIN)/1024.0-2.5)*100/0.04;
					//out_cur_readings[ocr_idx] = (1.0*adcValue/1023.0-0.454)*100.0/0.04;
			    //out_cur_readings[ocr_idx] = ((adcValue/1023.0)-0.440)*100.0/0.008;
					// See http://forum.arduino.cc/index.php?topic=133144.0 for help
					out_cur_readings[ocr_idx] = (adcValue-461.0) * 1/(0.008 *1024.0);
          printf("ADC Value: %f, Current: %f\r\n",adcValue, out_cur_readings[ocr_idx]);
					ocr_idx = ((ocr_idx+1) % 20);
					float sum = 0.0;
					for (int i=0; i < 20; i++) {
						sum += out_cur_readings[i];
					}
					normalizedVoltage = sum/ 20.0;
          normalizedVoltage = normalizedVoltage *1000;
          returnValue = (long)normalizedVoltage;
					break;
	}
	printf("Return Value : %li\n\r", returnValue);
	return returnValue;
}


/**
 * Update the relays:
 *   - If output is on
 *       - Turn it off if voltage < disconnect voltage
 *       - Turn it off if voltage > high voltage disconnect
 *       - Raise alarm on serial port + LED (fast blink ?)
 *   - If output is off
 *       - Turn it on if either PS or Battery > low voltage reconnect
 *              ... AND either PS or Battery < high voltage reconnect
 *       - Notify
 */
void update_relay(dpt_system_t *systemValues) {
	systemValues->upTime = mgos_uptime();
  double now = systemValues->upTime;
  if (systemValues->relayStatus == eRELAY_ON) {
    if ( (systemValues->out_voltage < systemValues->low_voltage_disconnect) || (systemValues->out_voltage > systemValues->high_voltage_disconnect) ) {
      if (systemValues->volt_alarm_start == 0) {
        LOG(LL_DEBUG, ("d:Voltage alarm conditions detected"));
        output_readings(systemValues);
        // We just went over the alarm threshold, get the timestamp and wait for next time
        systemValues->volt_alarm_start = now;
        return;
      }
      if ( (now - systemValues->volt_alarm_start) > systemValues->voltage_delay) {
        relayOff(systemValues);
        LOG(LL_DEBUG, ("d:Voltage alarm triggered after delay"));
        systemValues->volt_alarm_start = 0;
      }
    } else if ((systemValues->out_voltage > systemValues->low_voltage_disconnect) && (systemValues->out_voltage < systemValues->high_voltage_disconnect)) {
      // We are back within our operating parameters
      if (systemValues->volt_alarm_start != 0) {
        systemValues->volt_alarm_start = 0;
        output_readings(systemValues);
        LOG(LL_DEBUG, ("d:Voltage alarm resolved"));
      }
    }
  } else {
      long vmin = MIN(systemValues->batt_voltage, systemValues->ps_voltage);
      long vmax = MAX(systemValues->batt_voltage, systemValues->ps_voltage);
      if ( ( vmax > systemValues->low_voltage_reconnect) && ( vmin < systemValues->high_voltage_reconnect) ) {
          LOG(LL_DEBUG, ("B:%.4lu:P:%.4lu:O:%.4lu:A:%.4lu:R:%.1i:%lf",
					             systemValues->batt_voltage,
											 systemValues->ps_voltage,
											 systemValues->out_voltage,
											 systemValues->out_current,
											 systemValues->relayStatus,
											 systemValues->upTime/1000));
          relayOn(systemValues);
      }
  }
}
