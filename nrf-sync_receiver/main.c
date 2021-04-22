/** @file
*
* @defgroup nrf-sync_receiver_main main.c
* @{
* @ingroup nrf-sync_receiver
* @brief Nrf-Sync Receiver Application main file.
*
* This file contains the source code for the Nrf-Sync application.
*
*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "radio_config.h"
#include "nrf_gpio.h"
#include "boards.h"
#include "bsp.h"
#include "app_timer.h"
#include "nordic_common.h"
#include "nrf_error.h"
#include "app_error.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"


/**
 * @brief Function for initializing output pin with GPIOTE. It will be set in Task mode with action on pin configured 
 * to toggle. Event is generated when the pin toggles. Pin is set to begin low. 
 */
void gpiote_setup() {

}

/**
 * @brief Function for initializing TIMER0. This Timer will be in charge of managing the pulse duration and frequency.
 * Default values: PRESCALER = 4, MODE = Timer
 */
void timer0_setup() {

}

/**
 * @brief Function for initializing PPI. 
 */
void ppi_setup() {

}

/**
 * @brief Function for application main entry.
 * @return 0. int return type required by ANSI/ISO standard.
 */
int main(void) {


}

/**
 *@}
 **/