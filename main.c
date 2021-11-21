// vim: ts=4 shiftwidth=4
#include <stdint.h>	// uint8_t, etc.
#include <stdlib.h>
#include <stdio.h>
#include <string.h>	// strlen
#include <math.h>	// sin,cos.
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include "usart.h"
#include "gps.h"
#include "compass.h"
#include "setup.h"	// setup channel.

#define	DEGREES_360_X100	(360l * 100l)

static SETUP		setup = { 0 };

/*****************************************************/
void io_Init(void)
{
	PORTA = 0;
	DDRA = 0;

	PORTB = 0;
	DDRB = 0;

	PORTC = 0;
	DDRC = 0xff;

	PORTD = 0;
	DDRD = 0;

	PORTE = 0;
	DDRE = 0;

	PORTF = 0;
	DDRF = 0;

	PORTG = 0;
	DDRG = 0;

	PORTH = 0;
	DDRH = 0;

	PORTJ = 0;
	DDRJ = 0;

	PORTK = 0;
	DDRK = 0;

	PORTL = 0;
	DDRL = 0;
}

char		gps_input[200];
uint8_t		vtg_output[128];

/*****************************************************************************/
/*****************************************************************************/
int
main(void)
{
	uint8_t		gps_input_size = 0;
	// VTG data.
	uint16_t	vtg_course_x100 = 0;
	uint16_t	vtg_speed_knots_x100 = 0;
	uint16_t	vtg_speed_kmph_x100 = 0;

	// HDT data.
	uint16_t	hdt_course_x100 = 0;

	// Speed and course to be used for reconstructing VTG sentence.
	uint16_t	course_x100 = 0;
	uint16_t	speed_knots_x100 = 0;
	uint16_t	speed_kmph_x100 = 0;


	uint8_t 	ch;
	uint8_t 	i;

	io_Init();
	uart_Init();
	
	sei();

	uart0_PutString_P(PSTR("GPS INPUT (UART0)\r\n"));
	uart1_PutString_P(PSTR("COMPASS INPUT (UART1)\r\n"));
	uart2_PutString_P(PSTR("GPS OUTPUT (UART2)\r\n"));

	setup_send_P(PSTR("\r\nWelcome to NMEA MIXER v1.0!\r\n"));
	setup_load_from_nvram(&setup);
	setup_send_P(PSTR("\r\n>"));

	for (;;)
	{
		// UART0: Data From GPS
		// Intercept GPVTG messages, construct new one with the course taken from compass.
		if (!uart0_IsRxEmpty())
		{
			ch = uart0_GetChar();

			// Toggle LED-s if necessary.
			if (ch == '$')
			{
				PORTC = PORTC ^ 0x02;
			}

			// Is it possible to update input buffer?
			if (gps_input_size+1 < sizeof(gps_input))
			{
				gps_input[gps_input_size] = ch;
				++gps_input_size;
			}
			else
			{
				setup_send_P(PSTR("Overflow!?!!\r\n"));
				gps_input_size = 0;
				memset(gps_input, 0, sizeof(gps_input));
			}

			// Any VTG input?
			if (handle_gps_input(ch, &vtg_course_x100, &vtg_speed_knots_x100, &vtg_speed_kmph_x100) == SENTENCE_VTG)
			{
				speed_knots_x100 = vtg_speed_knots_x100;
				speed_kmph_x100 = vtg_speed_kmph_x100;
			}

			// Shall we output something?
			if (ch == 0x0A)
			{
				PORTC = PORTC ^ 0x10;

				// Was the last sentence $GPVTG?
				if (memcmp_P(gps_input, PSTR("$GPVTG"), 6) == 0)
				{
					int32_t		t = course_x100;
					t += setup.compass_offset_x10 * 10;
					while (t < 0)
					{
						t += DEGREES_360_X100;
					}
					while (t >= DEGREES_360_X100)
					{
						t -= DEGREES_360_X100;
					}
					string_of_vtg(vtg_output, t, speed_knots_x100, speed_kmph_x100);
					for (i=0; vtg_output[i]!=0; ++i)
					{
						uart2_PutChar(vtg_output[i]);
					}
				}
				else
				{
					for (i=0; i<gps_input_size; ++i)
					{
						uart2_PutChar(gps_input[i]);
					}
				}

				// We have to be prepared for next round!
				gps_input_size = 0;
				memset(gps_input, 0, sizeof(gps_input));
			}
		}    

		// UART1: Data from compass.
		if (!uart1_IsRxEmpty())
		{
			ch = uart1_GetChar();

			if (ch == '$')
			{
				PORTC = PORTC ^ 0x08;
			}

			if (handle_compass_input(ch, &hdt_course_x100))
			{
				course_x100 = hdt_course_x100;
			}
		}

		// UART2: Out.

		// UART3: Setup channel.
		if (!uart3_IsRxEmpty())
		{
			ch = uart3_GetChar();
			uart3_PutChar(ch);

			PORTC = PORTC ^ 0x80;
			setup_handle_input(ch, &setup);
		}

#if (0)
		// Empty channel - not working?
		if (!uart3_IsRxEmpty())
		{
			ch = uart3_GetChar();
			uart3_PutChar(ch);
		}
#endif
	}
}

