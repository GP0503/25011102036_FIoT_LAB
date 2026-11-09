#include <reg52.h>
#include <stdio.h>

unsigned char received;
unsigned char received_flag = 0;

/*void uart_init(){


	TMOD = 0x20;
		
	TH1 =0XFD;
	TL1 = 0XFD;

	SCON = 0X50;
	TR1 = 1;
	
	ES = 1;
	EA = 1;
	
}*/
void uart_init()
{
    TMOD = 0x20;
    TH1 = 0xFA;
    TL1 = 0xFA;

    SCON = 0x50;

    TR1 = 1;
		ES = 1;
		EA = 1;
}

void uart_tx(char c)
{
    SBUF = c;

    while (TI == 0);

    TI = 0;
}
void uart_ISR(void) interrupt 4
{
    if (RI == 1)
    {
        received = SBUF;   // Read received character
        RI = 0;         	 // Clear receive flag
        received_flag = 1; 
    }
}

void main(){

    uart_init();

    while (1)
    {
			if (received_flag == 1)
        {
            uart_tx(received);
            received_flag = 0;
        }
    }

}
