/** @file
*
* @defgroup nrf-sync_receiver_main main.c
* @{
* @ingroup nrf-sync_receiver
* @brief Nrf-Sync Receiver Application main file.
*
* This file contains the source code for the Nrf-Sync application receiver.
*
*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "nrf52840_bitfields.h"
#include "nrf52840.h"
#include "nrf52840_peripherals.h"

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

//Radio stuff
static uint8_t packet; //Packet will be stored here 


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

void radio_setup() {
    NRF_RADIO->FREQUENCY = 7UL; // Frequency bin 7, 2407MHz
    NRF_RADIO->MODE = (RADIO_MODE_MODE_Nrf_1Mbit << RADIO_MODE_MODE_Pos);

    //Address configuration (just random numbers I chose)
    NRF_RADIO->PREFIX0 = (0xF3UL << RADIO_PREFIX0_AP3_Pos) | // Prefix byte of address 3
                         (0xF2UL << RADIO_PREFIX0_AP2_Pos) | // Prefix byte of address 2
                         (0xF1UL << RADIO_PREFIX0_AP1_Pos)  | // Prefix byte of address 1
                         (0xF0UL << RADIO_PREFIX0_AP0_Pos);   // Prefix byte of address 0

    NRF_RADIO->PREFIX1 = (0xF7UL << RADIO_PREFIX1_AP7_Pos) | // Prefix byte of address 7
                         (0xF6UL << RADIO_PREFIX1_AP6_Pos) | // Prefix byte of address 6
                         (0xF5UL << RADIO_PREFIX1_AP5_Pos)  | // Prefix byte of address 5
                         (0xF4UL << RADIO_PREFIX1_AP4_Pos);   // Prefix byte of address 4

    NRF_RADIO->BASE0 = 0x14071997UL; // Base address for prefix 0

    NRF_RADIO->BASE1 = 0x16081931UL; // Base address for prefix 1-7

    NRF_RADIO->RXADDRESSES = (RADIO_RXADDRESSES_ADDR0_Enabled << RADIO_RXADDRESSES_ADDR0_Pos);  // Receive from address 0

    // Packet configuration
    NRF_RADIO->PCNF0 = 0UL; //not really interested in these

    NRF_RADIO->PCNF1 = (1UL << RADIO_PCNF1_MAXLEN_Pos) | //Only sending a 1 byte number
                       (1UL << RADIO_PCNF1_STATLEN_Pos) | // Since the LENGHT field is not set, this specifies the lenght of the payload
                       (4UL << RADIO_PCNF1_BALEN_Pos) |
                       (RADIO_PCNF1_ENDIAN_Little << RADIO_PCNF1_ENDIAN_Pos) | //I personally prefer little endian, for no particular reason
                       (RADIO_PCNF1_WHITEEN_Disabled << RADIO_PCNF1_WHITEEN_Pos);

    // CRC Config
    NRF_RADIO->CRCCNF = (RADIO_CRCCNF_LEN_Two << RADIO_CRCCNF_LEN_Pos); // Number of checksum bits
    NRF_RADIO->CRCINIT = 0xFFFFUL;   // Initial value
    NRF_RADIO->CRCPOLY = 0x11021UL;  // CRC poly: x^16 + x^12^x^5 + 1

    //Pointer to packet payload
    NRF_RADIO->PACKETPTR = (uint32_t)&packet;
}

/**
 * @brief Function for initializing PPI. 
 */
void ppi_setup() {

}

/**@brief Function for initialization oscillators.
 */
void clock_initialization()
{
    /* Start 16 MHz crystal oscillator */
    NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
    NRF_CLOCK->TASKS_HFCLKSTART    = 1;

    /* Wait for the external oscillator to start up */
    while (NRF_CLOCK->EVENTS_HFCLKSTARTED == 0)
    {
        // Do nothing.
    }

    /* Start low frequency crystal oscillator for app_timer(used by bsp)*/
    NRF_CLOCK->LFCLKSRC            = (CLOCK_LFCLKSRC_SRC_Xtal << CLOCK_LFCLKSRC_SRC_Pos);
    NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
    NRF_CLOCK->TASKS_LFCLKSTART    = 1;

    while (NRF_CLOCK->EVENTS_LFCLKSTARTED == 0)
    {
        // Do nothing.
    }
}

uint8_t read_packet()
{
    uint8_t result = 0;

    NRF_RADIO->EVENTS_READY = 0U;
    // Enable radio and wait for ready
    NRF_RADIO->TASKS_RXEN = 1U;

    while (NRF_RADIO->EVENTS_READY == 0U)
    {
        // wait
    }
    NRF_RADIO->EVENTS_END = 0U;
    // Start listening and wait for address received event
    NRF_RADIO->TASKS_START = 1U;

    // Wait for end of packet or buttons state changed
    while (NRF_RADIO->EVENTS_END == 0U)
    {
        // wait
    }

    if (NRF_RADIO->CRCSTATUS == 1U)
    {
        result = packet;
    }
    NRF_RADIO->EVENTS_DISABLED = 0U;
    // Disable radio
    NRF_RADIO->TASKS_DISABLE = 1U;

    while (NRF_RADIO->EVENTS_DISABLED == 0U)
    {
        // wait
    }
    return result;
}

/**
 * @brief Function for application main entry.
 * @return 0. int return type required by ANSI/ISO standard.
 */
int main(void) {
    uint32_t err_code = NRF_SUCCESS;

    clock_initialization();

    err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);

    err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);
    NRF_LOG_DEFAULT_BACKENDS_INIT();

    err_code = bsp_init(BSP_INIT_LEDS, NULL);
    APP_ERROR_CHECK(err_code);

    // Set radio configuration parameters
    radio_setup();

    err_code = bsp_indication_set(BSP_INDICATE_USER_STATE_OFF);
    NRF_LOG_INFO("Radio receiver example started.");
    NRF_LOG_INFO("Wait for first packet");
    APP_ERROR_CHECK(err_code);
    NRF_LOG_FLUSH();

    while (true)
    {
        uint32_t received = read_packet();

        err_code = bsp_indication_set(BSP_INDICATE_RCV_OK);
        NRF_LOG_INFO("Packet was received");
        APP_ERROR_CHECK(err_code);

        NRF_LOG_INFO("The contents of the package is %u", (unsigned int)received);
        NRF_LOG_FLUSH();
    }
}

/**
 *@}
 **/