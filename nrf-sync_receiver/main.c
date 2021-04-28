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

//GPIOTE stuff
#define OUTPUT_PIN_NUMBER 10UL //Output pin number
#define OUTPUT_PIN_PORT 1UL //Output pin port

#define GPIOTE_CH 0

//TIMER stuff
#define PULSE_DURATION 10 //Time in ms

//Radio stuff
static uint8_t packet; //Packet will be stored here 


/**
 * @brief Function for initializing output pin with GPIOTE. It will be set in Task mode with action on pin configured 
 * to toggle. Pin is set to begin low. 
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

    NRF_TIMER0->CC[0] = PULSE_DURATION * 1000;

    //Event when CC[0] will be connected via PPI to the GPIOTE task and shortcutted to clear timer 
    //task and to stop timer.


    NRF_TIMER0->SHORTS = (TIMER_SHORTS_COMPARE0_CLEAR_Enabled << TIMER_SHORTS_COMPARE0_CLEAR_Pos) |
                         (TIMER_SHORTS_COMPARE0_STOP_Enabled  << TIMER_SHORTS_COMPARE0_STOP_Pos);
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

    //Shortcuts
    //- READY and START 
    //- END and START (Radio must be always listening for the packet)
    NRF_RADIO->SHORTS = (RADIO_SHORTS_READY_START_Enabled << RADIO_SHORTS_READY_START_Pos) |
                        (RADIO_SHORTS_END_START_Enabled << RADIO_SHORTS_END_START_Pos);

    // CRC Config
    NRF_RADIO->CRCCNF = (RADIO_CRCCNF_LEN_Two << RADIO_CRCCNF_LEN_Pos); // Number of checksum bits
    NRF_RADIO->CRCINIT = 0xFFFFUL;   // Initial value
    NRF_RADIO->CRCPOLY = 0x11021UL;  // CRC poly: x^16 + x^12^x^5 + 1

    //Pointer to packet payload
    NRF_RADIO->PACKETPTR = (uint32_t)&packet;
}

/**
 * @brief Function for initializing PPI. 
 * Connections to be made: - Toggle pin high when Radio packet is received correctly: EVENTS_CRCOK from RADIO to TASKS_OUT[GPIOTE_CH] (will set pin high) -> PPI channel 0
 *                         - Start Timer 0 that manages pulse duration: EVENTS_CRCOK from RADIO with TASKS_START from TIMER0 -> PPI channel 0 FORK[0].TEP (same event triggers 2 tasks)
 *                         - Toggle pin low after pulse time: EVENTS_COMPARE[0] with TASKS_OUT[GPIOTE_CH] (will set pin low) -> PPI channel 1
 *                         - EVENTS_HFCLKSTARTED from CLOCK to TASKS_RXEN from RADIO -> PPI channel 2
 */
void ppi_setup() {
    //get endpoint addresses
    uint32_t gpiote_task_addr = (uint32_t)&NRF_GPIOTE->TASKS_OUT[GPIOTE_CH];
    uint32_t timer0_task_start_addr = (uint32_t)&NRF_TIMER0->TASKS_START;
    uint32_t radio_tasks_rxen_addr = (uint32_t)&NRF_RADIO->TASKS_RXEN;
    uint32_t timer0_events_compare_0_addr = (uint32_t)&NRF_TIMER0->EVENTS_COMPARE[0];
    uint32_t clock_events_hfclkstart_addr = (uint32_t)&NRF_CLOCK->EVENTS_HFCLKSTARTED;
    uint32_t radio_events_crcok_addr = (uint32_t)&NRF_RADIO->EVENTS_CRCOK;

    //set endpoints
    NRF_PPI->CH[0].EEP = radio_events_crcok_addr;
    NRF_PPI->CH[0].TEP = gpiote_task_addr;

    NRF_PPI->FORK[0].TEP = timer0_task_start_addr;

    NRF_PPI->CH[1].EEP = timer0_events_compare_0_addr;
    NRF_PPI->CH[1].TEP = gpiote_task_addr;

    NRF_PPI->CH[2].EEP = clock_events_hfclkstart_addr;
    NRF_PPI->CH[2].TEP = radio_tasks_rxen_addr;

    //enable channels
    NRF_PPI->CHENSET = (PPI_CHENSET_CH0_Enabled << PPI_CHENSET_CH0_Pos) | 
                       (PPI_CHENSET_CH1_Enabled << PPI_CHENSET_CH1_Pos) |
                       (PPI_CHENSET_CH2_Enabled << PPI_CHENSET_CH2_Pos);
}

/**
 * @brief Function for application main entry.
 * @return 0. int return type required by ANSI/ISO standard.
 */
int main(void) {
    //setup peripherals
    gpiote_setup();
    timer0_setup();
    radio_setup();
    ppi_setup();

    //Start
    //External HFCLK must be started and the Radio must be enabled as TX (now the radio thing will be done through PPI)
    NRF_CLOCK->TASKS_HFCLKSTART = CLOCK_TASKS_HFCLKSTART_TASKS_HFCLKSTART_Trigger;

    while (true) {
        __WFE();
    }
}

/**
 *@}
 **/