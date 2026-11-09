#include <reg52.h>

sbit P1_0 = P1^0;     // LED on P1.0

unsigned int pwm_count = 0;
unsigned int duty = 0;

unsigned int fade_count = 0;
unsigned int direction = 1;

void main()
{
    TMOD = 0x01;       // Timer 0, Mode 1

    TH0 = 0xF8;
    TL0 = 0xCB;        // ~2 ms

    TR0 = 1;

    while(1)
    {
        if(TF0 == 1)
        {
            TR0 = 0;
            TF0 = 0;

            TH0 = 0xF8;
            TL0 = 0xCB;

            TR0 = 1;

            

            pwm_count++;

            if(pwm_count >= 10)
            {
                pwm_count = 0;
            }

            if(pwm_count < duty)
            {
                P1_0 = 1;
            }
            else
            {
                P1_0 = 0;
            }

            

            fade_count++;

            if(fade_count >= 25)
            {
                fade_count = 0;

                if(direction == 1)
                {
                    duty++;

                    if(duty >= 10)
                    {
                        duty = 10;
                        direction = 0;
                    }
                }
                else
                {
                    duty--;

                    if(duty <= 0)
                    {
                        duty = 0;
                        direction = 1;
                    }
                }
            }
        }
    }
}