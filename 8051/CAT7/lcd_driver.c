#include <reg52.h>

sbit RS = P2^7;
sbit EN = P2^4;
sbit D4 = P2^5;
sbit D5 = P2^6;
sbit D6 = P2^3;
sbit D7 = P2^2;

void delay(unsigned int t)
{
    unsigned int i, j;
    for(i = 0; i < t; i++)
        for(j = 0; j < 1275; j++);
}

void lcd_nibble(unsigned char n)
{
    D4 = n & 0x01;
    D5 = (n >> 1) & 0x01;
    D6 = (n >> 2) & 0x01;
    D7 = (n >> 3) & 0x01;

    EN = 1;
    delay(1);
    EN = 0;
}

void lcd_cmd(unsigned char cmd)
{
    RS = 0;
    lcd_nibble(cmd >> 4);
    lcd_nibble(cmd & 0x0F);
    delay(2);
}
void lcd_data(unsigned char d)
{
    RS = 1;
    lcd_nibble(d >> 4);
    lcd_nibble(d & 0x0F);
    delay(2);
}

void lcd_init(void)
{
    RS = 0;
    EN = 0;

    delay(20);

    lcd_nibble(0x03);
    delay(5);

    lcd_nibble(0x03);
    delay(1);

    lcd_nibble(0x03);
    delay(1);

    lcd_nibble(0x02);
    delay(1);

    lcd_cmd(0x28);
    lcd_cmd(0x0C);
    lcd_cmd(0x06);
    lcd_cmd(0x01);

    delay(2);
}

void lcd_puts(char *str)
{
    while(*str)
    {
        lcd_data(*str);
        str++;
    }
}

void main(void)
{
    lcd_init();

    lcd_cmd(0x80);
    lcd_puts("A");

    lcd_cmd(0xC0);
    lcd_puts("Hello");

    while(1);
}