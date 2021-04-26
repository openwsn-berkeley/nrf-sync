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
#define OUTPUT_PIN_NUMBER 8UL //Output pin number
#define OUTPUT_PIN_PORT 1UL //Output pin port

#define GPIOTE_CH 0

//TIMER info
#define PULSE_DURATION 10 //Time in ms
#define PULSE_PERIOD 1000 //Time in ms -> 1 pulse per second
#define TIMER_OFFSET 10 //Time in ms


/**
 * @brief Function for initializing output pin with GPIOTE. It will be set in Task mode with action on pin configured 
 * to toggle. Event is generated when the pin toggles. Pin is set to begin low. 
 */
void gpiote_setup() {
    NRF_GPIOTE->CONFIG[GPIOTE_CH] = (GPIOTE_CONFIG_MODE_Task       << GPIOTE_CONFIG_MODE_Pos) |
                                    (OUTPUT_PIN_NUMBER             << GPIOTE_CONFIG_PSEL_Pos) |
                                    (OUTPUT_PIN_PORT               << GPIOTE_CONFIG_PORT_Pos) |
                                    (GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos) |
                                    (GPIOTE_CONFIG_OUTINIT_Low    << GPIOTE_CONFIG_OUTINIT_Pos);
}

/**
 * @brief Function for initializing TIMER0. This Timer will be in charge of managing the pulse duration and frequency.
 * Default values: PRESCALER = 4, MODE = Timer
 */
void timer0_setup() {
    NRF_TIMER0->BITMODE = TIMER_BITMODE_BITMODE_32Bit;

    NRF_TIMER0->CC[1] = PULSE_DURATION * 1000;
    NRF_TIMER0->CC[2] = (PULSE_PERIOD * 1000); //End of Timer

    //Event when CC[1] will be connected via PPI to the GPIOTE task. Event when CC[2] is shortcutted to clear timer 
    //task and to stop timer. Also, event when CC[2] will start Timer 1 through PPI. This will later be modified when
    //Radio is included.


    NRF_TIMER0->SHORTS = (TIMER_SHORTS_COMPARE2_CLEAR_Enabled << TIMER_SHORTS_COMPARE2_CLEAR_Pos) |
                         (TIMER_SHORTS_COMPARE2_STOP_Enabled  << TIMER_SHORTS_COMPARE2_STOP_Pos);
}

/**
 * @brief Function for initializing TIMER1. This Timer will be in charge of managing the offset.
 * Default values: PRESCALER = 4, MODE = Timer
 */
 void timer1_setup() {
    NRF_TIMER1->BITMODE = TIMER_BITMODE_BITMODE_32Bit;

    NRF_TIMER1->CC[0] = TIMER_OFFSET * 1000; 

     //Once this timer reaches the offset time, it clears, stops and through PPI starts Timer 0 and toggles the GPIOTE.

    NRF_TIMER1->SHORTS = (TIMER_SHORTS_COMPARE0_CLEAR_Enabled << TIMER_SHORTS_COMPARE0_CLEAR_Pos) | 
                         (TIMER_SHORTS_COMPARE0_STOP_Enabled  << TIMER_SHORTS_COMPARE0_STOP_Pos);
 }

/**
 * @brief Function for initializing RADIO. Radio will be in charge of sending a determined packet that the receiver will
 * identify so that both boards remain in sync. 
 * Radio in this case should be set up as Tx
 */
void radio_setup() {
    
}

/**
 * @brief Function for initializing PPI. 
 * Connections to be made: - EVENTS_COMPARE[0] from TIMER1 with TASK_OUT[GPIOTE_CH] (will set pin high) -> PPI channel 0
 *                         - EVENTS_COMPARE[0] from TIMER1 with TASK_START from TIMER0 -> PPI channel 0 FORK[0].TEP (same event triggers 2 tasks)
 *                         - EVENTS_COMPARE[1] with TASK_OUT[GPIOTE_CH] (will set pin low)  -> PPI channel 1
 *                         - EVENTS_COMPARE[2] from TIMER0 with TASK_START from TIMER1 -> PPI channel 2
 *                         - EVENTS_HFCLKSTARTED from CLOCK to TASK_START from TIMER1 (just to begin the program) -> PPI channel 3
 */
void ppi_setup() {
    //get endpoint addresses
    uint32_t gpiote_task_addr = &NRF_GPIOTE->TASKS_OUT[GPIOTE_CH];
    uint32_t timer0_task_start_addr = &NRF_TIMER0->TASKS_START;
    uint32_t timer1_task_start_addr = &NRF_TIMER1->TASKS_START;
    uint32_t timer1_events_compare_0_addr = &NRF_TIMER1->EVENTS_COMPARE[0];
    uint32_t timer0_events_compare_1_addr = &NRF_TIMER0->EVENTS_COMPARE[1];
    uint32_t timer0_events_compare_2_addr = &NRF_TIMER0->EVENTS_COMPARE[2];
    uint32_t clock_events_hfclkstart_addr = &NRF_CLOCK->EVENTS_HFCLKSTARTED;

    //set endpoints
    NRF_PPI->CH[0].EEP = timer1_events_compare_0_addr;
    NRF_PPI->CH[0].TEP = gpiote_task_addr;

    NRF_PPI->FORK[0].TEP = timer0_task_start_addr;

    NRF_PPI->CH[1].EEP = timer0_events_compare_1_addr;
    NRF_PPI->CH[1].TEP = gpiote_task_addr;

    NRF_PPI->CH[2].EEP = timer0_events_compare_2_addr;
    NRF_PPI->CH[2].TEP = timer1_task_start_addr;

    NRF_PPI->CH[3].EEP = clock_events_hfclkstart_addr;
    NRF_PPI->CH[3].TEP = timer1_task_start_addr;

    //enable channels
    NRF_PPI->CHENSET = (PPI_CHENSET_CH0_Enabled << PPI_CHENSET_CH0_Pos) | 
                       (PPI_CHENSET_CH1_Enabled << PPI_CHENSET_CH1_Pos) |
                       (PPI_CHENSET_CH2_Enabled << PPI_CHENSET_CH2_Pos) |
                       (PPI_CHENSET_CH3_Enabled << PPI_CHENSET_CH3_Pos);
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
    timer1_setup();
    ppi_setup();

    //Start
    NRF_CLOCK->TASKS_HFCLKSTART = CLOCK_TASKS_HFCLKSTART_TASKS_HFCLKSTART_Trigger;

    while (true) {
        __WFE();
    }
}

/**
 *@}
 **/