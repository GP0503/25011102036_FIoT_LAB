#include <reg52.h>


sbit ROW1 = P2^7;
sbit ROW2 = P2^6;
sbit ROW3 = P2^5;
sbit ROW4 = P2^4;

sbit COL1 = P2^3;
sbit COL2 = P2^2;
sbit COL3 = P2^1;
sbit COL4 = P2^0;

unsigned char mode = 0; 

void delay_ms(unsigned int ms) {
    unsigned int i, j;
    for(i = 0; i < ms; i++) {
        for(j = 0; j < 112; j++);
    }
}


bit is_key1_pressed() {
    ROW1 = 0; 
    ROW2 = 1; 
    ROW3 = 1;
    ROW4 = 1;

    COL1 = 1;

    
    if (COL1 == 0) {
        return 1; // Key 1 IS pressed
    }
    return 0; // Key 1 is NOT pressed
}

// Smart Delay: Keeps checking Key 1 while running mode animations
bit check_key_delay(unsigned int ms) {
    unsigned int elapsed = 0;
    while (elapsed < ms) {
        if (is_key1_pressed()) {       // 1. Check Key 1 status
            delay_ms(20);              // 2. Debounce delay
            if (is_key1_pressed()) {   // 3. Confirm press
                while (is_key1_pressed()); // 4. Wait for release
                return 1;              // Trigger mode change
            }
        }
        delay_ms(10);
        elapsed += 10;
    }
    return 0;
}

void main() {
    P1 = 0x00; // Turn off all Port 2 LEDs initially

    while (1) {
        
        // --- INITIAL STATE (Mode 0): Wait forsss 1st press ---
        if (mode == 0) {
            if (is_key1_pressed()) {
                delay_ms(20);
                if (is_key1_pressed()) {
                    while (is_key1_pressed());
                    mode = 1; // Jump to Mode 1
                }
            }
        }

        // --- MODE 1: Running LED ---
        else if (mode == 1) {
            unsigned char i;
            for (i = 0; i < 4; i++) {
                P1 = (1 << i); // 0x01 -> 0x02 -> 0x04 -> 0x08
                if (check_key_delay(300)) { mode = 2; break; }
            }
        }

        // --- MODE 2: Binary Counter ---
        else if (mode == 2) {
            unsigned char count;
            for (count = 0; count <= 15; count++) {
                P1 = count; // 0x00 to 0x0F
                if (check_key_delay(300)) { mode = 3; break; }
            }
        }

        // --- MODE 3: Alternating Pattern ---
        else if (mode == 3) {
            P1 = 0x0A; // 1010
            if (check_key_delay(300)) { mode = 4; continue; }
            
            P1 = 0x05; // 0101
            if (check_key_delay(300)) { mode = 4; continue; }
        }

        else if (mode == 4) {
            P1 = 0x0F; // 1111 (All ON)
            if (check_key_delay(300)) { mode = 1; continue; }
            
            P1 = 0x00; // 0000 (All OFF)
            if (check_key_delay(300)) { mode = 1; continue; }
        }
    }
}