
/** @file
*
* @defgroup nrf-sync_transmitter_main main.c
* @{
* @ingroup nrf-sync_transmitter
* @brief Nrf-Sync Transmitter Application main file.
*
* This file contains the source code for the Nrf-Sync application.
*
*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "radio_config.h"
#include "nrf_gpio.h"
#include "app_timer.h"
#include "boards.h"
#include "bsp.h"
#include "nordic_common.h"
#include "nrf_error.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

static uint32_t                   packet;                    /**< Packet to transmit. */

/**@brief Function for sending packet.
 */
void send_packet()
{
    // send the packet:
    NRF_RADIO->EVENTS_READY = 0U;
    NRF_RADIO->TASKS_TXEN   = 1;

    while (NRF_RADIO->EVENTS_READY == 0U)
    {
        // wait
    }
    NRF_RADIO->EVENTS_END  = 0U;
    NRF_RADIO->TASKS_START = 1U;

    while (NRF_RADIO->EVENTS_END == 0U)
    {
        // wait
    }

    uint32_t err_code = bsp_indication_set(BSP_INDICATE_SENT_OK);
    NRF_LOG_INFO("The packet was sent");
    APP_ERROR_CHECK(err_code);

    NRF_RADIO->EVENTS_DISABLED = 0U;
    // Disable radio
    NRF_RADIO->TASKS_DISABLE = 1U;

    while (NRF_RADIO->EVENTS_DISABLED == 0U)
    {
        // wait
    }
}


/**@brief Function for handling bsp events.
 */
void bsp_evt_handler(bsp_event_t evt)
{
    uint32_t prep_packet = 0;
    switch (evt)
    {
        case BSP_EVENT_KEY_0:
            /* Fall through. */
        case BSP_EVENT_KEY_1:
            /* Fall through. */
        case BSP_EVENT_KEY_2:
            /* Fall through. */
        case BSP_EVENT_KEY_3:
            /* Fall through. */
        case BSP_EVENT_KEY_4:
            /* Fall through. */
        case BSP_EVENT_KEY_5:
            /* Fall through. */
        case BSP_EVENT_KEY_6:
            /* Fall through. */
        case BSP_EVENT_KEY_7:
            /* Get actual button state. */
            for (int i = 0; i < BUTTONS_NUMBER; i++)
            {
                prep_packet |= (bsp_board_button_state_get(i) ? (1 << i) : 0);
            }
            break;
        default:
            /* No implementation needed. */
            break;
    }
    packet = prep_packet;
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


/**
 * @brief Function for application main entry.
 * @return 0. int return type required by ANSI/ISO standard.
 */
int main(void)
{
    uint32_t err_code = NRF_SUCCESS;

    clock_initialization();

    err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);

    err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();

    err_code = bsp_init(BSP_INIT_LEDS | BSP_INIT_BUTTONS, bsp_evt_handler);
    APP_ERROR_CHECK(err_code);

    // Set radio configuration parameters
    radio_configure();

    // Set payload pointer
    NRF_RADIO->PACKETPTR = (uint32_t)&packet;

    err_code = bsp_indication_set(BSP_INDICATE_USER_STATE_OFF);
    NRF_LOG_INFO("Radio transmitter example started.");
    NRF_LOG_INFO("Press Any Button");
    APP_ERROR_CHECK(err_code);

    while (true)
    {
        if (packet != 0)
        {
            send_packet();
            NRF_LOG_INFO("The contents of the package was %u", (unsigned int)packet);
            packet = 0;
        }
        NRF_LOG_FLUSH();
        __WFE();
    }
}


/**
 *@}
 **/
