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

//GPIOTE stuff
#define OUTPUT_PIN_NUMBER 13UL //Output pin number
#define OUTPUT_PIN_PORT 0UL //Output pin port
#define BUTTON_PIN_NUMBER 11 //Button 1
#define BUTTON_PIN_PORT 0UL

#define GPIOTE_CH_PULSE 0
#define GPIOTE_CH_BUTTON 1

//TIMER stuff
#define PULSE_DURATION 10 //Time in ms
#define PULSE_PERIOD 1000 //Time in ms -> 1 pulse per second
#define TIMER_OFFSET 10 //Time in ms

//Radio stuff
#define MAGIC_NUMBER 42

static uint8_t packet = MAGIC_NUMBER; 


/**
 * @brief Function for initializing output pin with GPIOTE. It will be set in Task mode with action on pin configured 
 * to toggle. Pin is set to begin low. 
 */
void gpiote_setup() {
    NRF_GPIOTE->CONFIG[GPIOTE_CH_PULSE] = (GPIOTE_CONFIG_MODE_Task       << GPIOTE_CONFIG_MODE_Pos) |
                                          (OUTPUT_PIN_NUMBER             << GPIOTE_CONFIG_PSEL_Pos) |
                                          (OUTPUT_PIN_PORT               << GPIOTE_CONFIG_PORT_Pos) |
                                          (GPIOTE_CONFIG_POLARITY_Toggle << GPIOTE_CONFIG_POLARITY_Pos) |
                                          (GPIOTE_CONFIG_OUTINIT_Low     << GPIOTE_CONFIG_OUTINIT_Pos);

    NRF_GPIOTE->CONFIG[GPIOTE_CH_BUTTON] = (GPIOTE_CONFIG_MODE_Event      << GPIOTE_CONFIG_MODE_Pos) |
                                           (BUTTON_PIN_NUMBER             << GPIOTE_CONFIG_PSEL_Pos) |
                                           (BUTTON_PIN_PORT               << GPIOTE_CONFIG_PORT_Pos) |
                                           (GPIOTE_CONFIG_POLARITY_HiToLo << GPIOTE_CONFIG_POLARITY_Pos); //Buttons are active low
}

/**
 * @brief Function for initializing TIMER0. This Timer will be in charge of managing the pulse duration and frequency.
 * Default values: PRESCALER = 4, MODE = Timer
 */
void timer0_setup() {
    NRF_TIMER0->BITMODE = TIMER_BITMODE_BITMODE_32Bit;

    NRF_TIMER0->CC[1] = PULSE_DURATION * 1000;
    NRF_TIMER0->CC[2] = (PULSE_PERIOD - TIMER_OFFSET) * 1000; //End of Timer (minus Timer offset to avoid counting twice the offset)

    //Event when CC[1] will be connected via PPI to the GPIOTE task. Event when CC[2] is shortcutted to clear timer 
    //task and to stop timer. Also, event when CC[2] will start Timer 1 and will start Radio so packet is sent through PPI.


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
 * Radio in this case should be set up as Tx. TASKS_TXEN must be triggered outside and at the beginning. 
 */
void radio_setup() {
    NRF_RADIO->TXPOWER = (RADIO_TXPOWER_TXPOWER_0dBm << RADIO_TXPOWER_TXPOWER_Pos);
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

    NRF_RADIO->TXADDRESS = 0UL;  // Set device address 0 to use when transmitting

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
 * Connections to be made: - Toggle pin high after offset time: EVENTS_COMPARE[0] from TIMER1 with TASKS_OUT[GPIOTE_CH_PULSE] (will set pin high) -> PPI channel 0
 *                         - Start Timer 0 that manages pulse duration: EVENTS_COMPARE[0] from TIMER1 with TASKS_START from TIMER0 -> PPI channel 0 FORK[0].TEP (same event triggers 2 tasks)
 *                         - Toggle pin low after pulse time: EVENTS_COMPARE[1] with TASKS_OUT[GPIOTE_CH_PULSE] (will set pin low) -> PPI channel 1
 *                         - Start Timer 1 that manages the offset after Timer 0 ends: EVENTS_COMPARE[2] from TIMER0 with TASKS_START from TIMER1 -> PPI channel 2
 *                         - Send another packet after Timer 0 ends: EVENTS_COMPARE[2] with TASKS_START from RADIO -> PPI channel 2 FORK[2].TEP (at the same time that the offset timer starts)
 *                              *the offset will include the time difference between the Radio starting and when the packet is actually sent
 *                         - EVENTS_HFCLKSTARTED from CLOCK to TASKS_TXEN from RADIO -> PPI channel 3
 *                         - EVENTS_IN[GPIOTE_CH_BUTTON] from GPIOTE to TASKS_START from TIMER1 (just to begin the program) -> PPI channel 4
 */
void ppi_setup() {
    //get endpoint addresses
    uint32_t gpiote_task_addr = (uint32_t)&NRF_GPIOTE->TASKS_OUT[GPIOTE_CH_PULSE];
    uint32_t timer0_task_start_addr = (uint32_t)&NRF_TIMER0->TASKS_START;
    uint32_t timer1_task_start_addr = (uint32_t)&NRF_TIMER1->TASKS_START;
    uint32_t radio_tasks_txen_addr = (uint32_t)&NRF_RADIO->TASKS_TXEN;
    uint32_t radio_tasks_start_addr = (uint32_t)&NRF_RADIO->TASKS_START;
    uint32_t timer1_events_compare_0_addr = (uint32_t)&NRF_TIMER1->EVENTS_COMPARE[0];
    uint32_t timer0_events_compare_1_addr = (uint32_t)&NRF_TIMER0->EVENTS_COMPARE[1];
    uint32_t timer0_events_compare_2_addr = (uint32_t)&NRF_TIMER0->EVENTS_COMPARE[2];
    uint32_t clock_events_hfclkstart_addr = (uint32_t)&NRF_CLOCK->EVENTS_HFCLKSTARTED;
    uint32_t gpiote_events_addr = (uint32_t)&NRF_GPIOTE->EVENTS_IN[GPIOTE_CH_BUTTON];

    //set endpoints
    NRF_PPI->CH[0].EEP = timer1_events_compare_0_addr;
    NRF_PPI->CH[0].TEP = gpiote_task_addr;

    NRF_PPI->FORK[0].TEP = timer0_task_start_addr;

    NRF_PPI->CH[1].EEP = timer0_events_compare_1_addr;
    NRF_PPI->CH[1].TEP = gpiote_task_addr;

    NRF_PPI->CH[2].EEP = timer0_events_compare_2_addr;
    NRF_PPI->CH[2].TEP = timer1_task_start_addr;

    NRF_PPI->FORK[2].TEP = radio_tasks_start_addr;

    NRF_PPI->CH[3].EEP = clock_events_hfclkstart_addr;
    NRF_PPI->CH[3].TEP = radio_tasks_txen_addr;

    NRF_PPI->CH[4].EEP = gpiote_events_addr;
    NRF_PPI->CH[4].TEP = timer1_task_start_addr;

    //enable channels
    NRF_PPI->CHENSET = (PPI_CHENSET_CH0_Enabled << PPI_CHENSET_CH0_Pos) | 
                       (PPI_CHENSET_CH1_Enabled << PPI_CHENSET_CH1_Pos) |
                       (PPI_CHENSET_CH2_Enabled << PPI_CHENSET_CH2_Pos) |
                       (PPI_CHENSET_CH3_Enabled << PPI_CHENSET_CH3_Pos) |
                       (PPI_CHENSET_CH4_Enabled << PPI_CHENSET_CH4_Pos);
}

/**
 * @brief Function for application main entry.
 * @return 0. int return type required by ANSI/ISO standard.
 */
int main(void) {
    //TEST Final: Start transmitting when button pressed.

    //setup peripherals
    gpiote_setup();
    timer0_setup();
    timer1_setup();
    radio_setup();
    ppi_setup();

    //Start
    //External HFCLK must be started and the Radio must be enabled as TX (the radio thing will be done through PPI)
    NRF_CLOCK->TASKS_HFCLKSTART = CLOCK_TASKS_HFCLKSTART_TASKS_HFCLKSTART_Trigger;

    while (true) {
        __WFE();
    }
}

/**
 *@}
 **/