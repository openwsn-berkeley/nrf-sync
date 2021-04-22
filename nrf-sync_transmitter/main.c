/** @file
*
* @defgroup nrf-sync_transmitter_main main.c
* @{
* @ingroup nrf-sync_transmitter
* @brief Nrf-Sync Transmitter Application main file.
*
* This file contains the source code for the Nrf-Sync application transmitter.
*
*/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "nrf52840_bitfields.h"
#include "nrf52840.h"
#include "nrf52840_peripherals.h"

//GPIOTE info
#define OUTPUT_PIN_NUMBER 13UL //Output pin number
#define OUTPUT_PIN_PORT 0UL //Output pin port

#define GPIOTE_CH 0

//TIMER info
#define PULSE_DURATION 10 //Time in ms
#define PULSE_PERIOD 1000 //Time in ms -> 1 pulse per second
#define TIMER_OFFSET 0.001 //Time in ms


/**
 * @brief Function for initializing output pin with GPIOTE. It will be set in Task mode with action on pin configured 
 * to toggle. Event is generated when the pin toggles. Pin is set to begin low. 
 */
void gpiote_setup() {
    NRF_GPIOTE->CONFIG[GPIOTE_CH] = (GPIOTE_CONFIG_MODE_Task       << GPIOTE_CONFIG_MODE_Pos) |
                                    (OUTPUT_PIN_NUMBER             << GPIOTE_CONFIG_PSEL_Pos) |
                                    (OUTPUT_PIN_PORT               << GPIOTE_CONFIG_PORT_Pos) |
                                    (GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos) |
                                    (GPIOTE_CONFIG_OUTINIT_High    << GPIOTE_CONFIG_OUTINIT_Pos);
}

/**
 * @brief Function for initializing TIMER0. This Timer will be in charge of managing the pulse duration and frequency.
 * Default values: PRESCALER = 4, MODE = Timer
 */
void timer0_setup() {
    NRF_TIMER0->BITMODE = TIMER_BITMODE_BITMODE_32Bit;

    NRF_TIMER0->CC[0] = TIMER_OFFSET * 1000; //Timer offset trigger event to set gpio pin high
    NRF_TIMER0->CC[1] = (TIMER_OFFSET + PULSE_DURATION) * 1000; //Timer trigger event to set gpio pin low, end of pulse
    NRF_TIMER0->CC[2] = (PULSE_PERIOD * 1000); //End of Timer

    //Event when CC[0] and CC[1] will be connected via PPI to the GPIOTE task. Event when CC[2] is shortcutted to clear timer 
    //task. Then timer automatically starts again. This will be changed when the Radio is the one that manages this. 

    NRF_TIMER0->SHORTS = TIMER_SHORTS_COMPARE2_CLEAR_Enabled << TIMER_SHORTS_COMPARE2_CLEAR_Pos;
}

/**
 * @brief Function for initializing PPI. 
 * Connections to be made: - EVENTS_COMPARE[0] with TASK_OUT[GPIOTE_CH] (will set pin high) -> PPI channel 0
 *                         - EVENTS_COMPARE[1] with TASK_OUT[GPIOTE_CH] (will set pin low)  -> PPI channel 1
 */
void ppi_setup() {
    //get endpoint addresses
    uint32_t gpiote_task_addr = &NRF_GPIOTE->TASKS_OUT[GPIOTE_CH];
    uint32_t timer0_events_compare_0_addr = &NRF_TIMER0->EVENTS_COMPARE[0];
    uint32_t timer0_events_compare_1_addr = &NRF_TIMER0->EVENTS_COMPARE[1];

    //set endpoints
    NRF_PPI->CH[0].EEP = timer0_events_compare_0_addr;
    NRF_PPI->CH[0].TEP = gpiote_task_addr;
    NRF_PPI->CHENSET = (PPI_CHENSET_CH0_Enabled << PPI_CHENSET_CH0_Pos);

    NRF_PPI->CH[1].EEP = timer0_events_compare_1_addr;
    NRF_PPI->CH[1].TEP = gpiote_task_addr;

    //enable channels
    NRF_PPI->CHENSET |= (PPI_CHENSET_CH1_Enabled << PPI_CHENSET_CH1_Pos);
}

/**
 * @brief Function for application main entry.
 * @return 0. int return type required by ANSI/ISO standard.
 */
int main(void) {
    //TEST 1: Generate square wave with 10ms duty cycle, offset 0.

    //setup peripherals
    gpiote_setup();
    timer0_setup();
    ppi_setup();

    //Start timer0
    NRF_TIMER0->TASKS_START = TIMER_TASKS_START_TASKS_START_Trigger;

    while (true) {
        __WFE();
    }
}

/**
 *@}
 **/