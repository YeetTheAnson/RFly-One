// Written with Gemini. Firmware time not counted.
#include <Arduino.h>
#include "hardware/pio.h"
#include "hardware/clocks.h"

// --- Pin Definitions ---
#define PIN_PPM_IN    16
#define PIN_CLOCK_OUT 17

#define UART_TX_PIN   4
#define UART_RX_PIN   5
#define BAUD_RATE     115200

// --- PIO Setup Function ---
void setup_pio_clock_extraction() {
    // Select the first PIO block and claim a free state machine
    PIO pio = pio0;
    int sm = pio_claim_unused_sm(pio, true);
    if (sm == -1) {
        pio = pio1; // Fallback to PIO1 if PIO0 is full
        sm = pio_claim_unused_sm(pio, true);
    }
    if (sm == -1) return; // Fail safe if no state machines are free

    /* 
     * PIO PROGRAM EXPLANATION:
     * ADS-B operates at 1 Mbps. One Manchester bit is 1 µs long.
     * We run the PIO State Machine at 8 MHz (1 cycle = 0.125 µs).
     * 1 bit period = 8 cycles.
     * 
     * Instruction 0: Wait for a rising edge on PPM Input.
     * Instruction 1: Set Clock OUT High, wait 3 extra cycles (Total 4 cycles = 0.5 µs).
     * Instruction 2: Set Clock OUT Low, wait 2 extra cycles (Total 3 cycles = 0.375 µs).
     * 
     * The state machine then wraps back to Instruction 0. 
     * Because the SM is "busy" executing delays for 0.875 µs after triggering, 
     * it naturally masks out mid-symbol transitions and only synchronizes 
     * on the 1 µs bit boundaries, effectively recovering the clock.
     */
    uint16_t pio_instructions[] = {
        0x20a0, // wait 1 pin 0      (Wait for PPM to go high)
        0xe301, // set pins, 1 [3]   (Set High, delay 3)
        0xe200  // set pins, 0 [2]   (Set Low, delay 2)
    };
    
    struct pio_program pio_prog = {
        .instructions = pio_instructions,
        .length = 3,
        .origin = -1 // Auto-allocate space in instruction memory
    };
    
    uint offset = pio_add_program(pio, &pio_prog);
    pio_sm_config c = pio_get_default_sm_config();
    
    // Wrap from end (offset + 2) back to start (offset)
    sm_config_set_wrap(&c, offset, offset + 2);
    
    // Configure input mapped pin (for WAIT) and output mapped pin (for SET)
    sm_config_set_in_pins(&c, PIN_PPM_IN);
    sm_config_set_set_pins(&c, PIN_CLOCK_OUT, 1);
    
    // Initialize GPIO for PIO use
    pio_gpio_init(pio, PIN_CLOCK_OUT);
    pio_sm_set_consecutive_pindirs(pio, sm, PIN_CLOCK_OUT, 1, true);
    
    // Set PIO clock to 8 MHz to get exactly 0.125 us per cycle
    float div = (float)clock_get_hz(clk_sys) / 8000000.0;
    sm_config_set_clkdiv(&c, div);

    // Initialize and enable the state machine
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}

void setup() {
    // 1. Setup UART Relay on Serial1
    Serial1.setRX(UART_RX_PIN);
    Serial1.setTX(UART_TX_PIN);
    Serial1.begin(BAUD_RATE);

    // 2. Setup PIO for ADS-B clock extraction
    setup_pio_clock_extraction();
}

void loop() {
    // Hardware UART loopback: relay bytes received on RX out through TX
    if (Serial1.available()) {
        Serial1.write(Serial1.read());
    }
}