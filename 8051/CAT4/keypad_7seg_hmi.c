#include <reg52.h>

sbit ROW1 = P2^7;
sbit ROW2 = P2^6;
sbit ROW3 = P2^5;
sbit ROW4 = P2^4;

sbit COL1 = P2^3;
sbit COL2 = P2^2;
sbit COL3 = P2^1;
sbit COL4 = P2^0;

char code KEY_PAD[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

void delay(unsigned int ms) {
    unsigned int i, j;
    for (i = 0; i < ms; i++) {
        for (j = 0; j < 112; j++);
    }
}

char scan_keypad() {
    unsigned char r;
    for (r = 0; r < 4; r++) {
        ROW1 = 1; ROW2 = 1; ROW3 = 1; ROW4 = 1;

        if (r == 0) ROW1 = 0;
        if (r == 1) ROW2 = 0;
        if (r == 2) ROW3 = 0;
        if (r == 3) ROW4 = 0;

        COL1 = 1; COL2 = 1; COL3 = 1; COL4 = 1;

        if (COL1 == 0) return KEY_PAD[r][0];
        if (COL2 == 0) return KEY_PAD[r][1];
        if (COL3 == 0) return KEY_PAD[r][2];
        if (COL4 == 0) return KEY_PAD[r][3];
    }
    return 0;
}

bit check_key_delay(unsigned int ms) {
    unsigned int elapsed = 0;
    char k = 0;
    while (elapsed < ms) {
        k = scan_keypad();
        if (k != 0) {
            delay(20);
            if (scan_keypad() == k) {
                return 1;
            }
        }
        delay(10);
        elapsed += 10;
    }
    return 0;
}

void display_digit(unsigned char key) {
    switch(key) {
        case '0': P0 = 0x3F; break;
        case '1': P0 = 0x06; break;
        case '2': P0 = 0x5B; break;
        case '3': P0 = 0x4F; break;
        case '4': P0 = 0x66; break;
        case '5': P0 = 0x6D; break;
        case '6': P0 = 0x7D; break;
        case '7': P0 = 0x07; break;
        case '8': P0 = 0x7F; break;
        case '9': P0 = 0x6F; break;
        default:  P0 = 0x00; break;
    }
}

void main() {
    char key = 0;
    char mode = '0';
    unsigned char i, count;

    P1 = 0x00;
    P0 = 0x3F;

    while (1) {
        key = scan_keypad();
        if (key != 0) {
            delay(20);
            if (scan_keypad() == key) {
                mode = key;
                display_digit(mode);
                while (scan_keypad() != 0);
            }
        }

        if (mode == '1') {
            P1 = 0x01;
        }
        else if (mode == '2') {
            P1 = 0x02;
        }
        else if (mode == '3') {
            P1 = 0x04;
        }
        else if (mode == '4') {
            P1 = 0x08;
        }
        else if (mode == '5') {
            for (i = 0; i < 4; i++) {
                P1 = (1 << i);
                if (check_key_delay(300)) break;
            }
        }
        else if (mode == '6') {
            P1 = 0x0A;
            if (check_key_delay(300)) continue;
            P1 = 0x05;
            if (check_key_delay(300)) continue;
        }
        else if (mode == '7') {
            for (count = 0; count <= 15; count++) {
                P1 = count;
                if (check_key_delay(300)) break;
            }
        }
        else if (mode == '8') {
            P1 = 0x0F;
            if (check_key_delay(250)) continue;
            P1 = 0x00;
            if (check_key_delay(250)) continue;
        }
        else if (mode == '9' || mode == '0') {
            P1 = 0x00;
        }
    }
}