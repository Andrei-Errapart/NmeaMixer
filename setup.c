#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <avr/io.h>	// PORTC
#include <util/crc16.h>
#include <stdio.h>	// sprintf_P
#include <stdlib.h>	// strtol
#include <errno.h>	// errno
#include <limits.h>	// LONG_MIN
#include <ctype.h>	// isalnum
#include <string.h>	// strlen
#include "setup.h"
#include "usart.h"

#define	EEPROM_START_ADDRESS	((uint8_t*)(8))


/*****************************************************************************/
void
setup_send(	const char*	s)
{
	for (; *s != 0; ++s)
	{
		uart3_PutChar(*s);
	}
}

/*****************************************************************************/
void
setup_send_P(			PGM_P	s)
{
	PORTC = PORTC ^ 0x40;
	for (;;)
	{
		const char c = pgm_read_byte(s++);
		if (c==0) {
			break;
		}
		setup_send_char(c);
	}
}

/*****************************************************************************/
static uint8_t
hexchar_of_int(		const uint8_t	ii)
{
	return ii<10 ? ii + '0' : ii + 'A' - 10;
}

/*****************************************************************************/
void
setup_send_hex(			const uint8_t	x)
{
	setup_send_char(hexchar_of_int(x>>4));
	setup_send_char(hexchar_of_int(x&0x0F));
}

/*****************************************************************************/
void
setup_send_newline()
{
	setup_send_P(PSTR("\r\n"));
}

/*****************************************************************************/
void
setup_send_integer(
	PGM_P		name,
	const int32_t	i,
	PGM_P		unit)
{
	char	xbuf[14];
	setup_send_P(name);
	setup_send_P(PSTR(" = "));
	sprintf_P(xbuf, PSTR("%ld"), i);
	setup_send(xbuf);
	setup_send_P(unit);
	setup_send_newline();
}

/*****************************************************************************/
void
setup_send_integer_x10(
	PGM_P		name,
	const int32_t	i,
	PGM_P		unit)
{
	const int32_t	i_mod = i % 10;
	char	xbuf[20];
	setup_send_P(name);
	setup_send_P(PSTR(" = "));
	sprintf_P(xbuf, PSTR("%ld.%ld"), i/10, (i_mod<0 ? -i_mod : i_mod));
	setup_send(xbuf);
	setup_send_P(unit);
	setup_send_newline();
}

/*****************************************************************************/
void
setup_send_boolean(
	PGM_P		name,
	const bool 	b)
{
	setup_send_P(name);
	setup_send_P(PSTR(" = "));
	if (b) {
		setup_send_P(PSTR("true"));
	} else {
		setup_send_P(PSTR("false"));
	}
	setup_send_newline();
}

/*****************************************************************************/
void
setup_send_string(
	PGM_P		name,
	const char*	s)
{
	setup_send_P(name);
	setup_send_P(PSTR(" = "));
	setup_send(s);
	setup_send_newline();
}

/*****************************************************************************/
uint8_t
setup_crc(const SETUP* setup)
{
	uint8_t		r = 0;
	uint8_t		i;
	for (i=0; i<sizeof(*setup); ++i) {
		r = _crc_ibutton_update(r, ((const uint8_t*)setup)[i]);
	}
	return r;
}

/*****************************************************************************/
static void
setup_print(const SETUP* setup)
{
	setup_send_P(      PSTR("N  NAME             VALUE\r\n"));
	setup_send_integer_x10(PSTR("0: Compass offset"), setup->compass_offset_x10, PSTR(" deg."));
	setup_send_P(PSTR("Set new values as follows: N VALUE\r\n"));
	setup_send_P(PSTR("For example, set offset 3.3deg with the following command:\r\n"));
	setup_send_P(PSTR("0 33"));
}

/*****************************************************************************/
bool
setup_load_from_nvram(SETUP* setup)
{
	// 1. Read from eeprom.
	const uint8_t		crc2 = eeprom_read_byte((uint8_t*)(EEPROM_START_ADDRESS));
	eeprom_read_block(
		(uint8_t*)(setup),
		(void*)(EEPROM_START_ADDRESS+1), sizeof(*setup));

	// 2. Check CRC.
	if (setup_crc(setup) == crc2) {
		setup_print(setup);
		return true;
	} else {
		setup->compass_offset_x10 = 0;
		setup_print(setup);
		return false;
	}
}

/*****************************************************************************/
void
setup_store_to_nvram(const SETUP* setup)
{
	// 2. Write to the eeprom.
	eeprom_write_byte(((uint8_t*)(EEPROM_START_ADDRESS)), setup_crc(setup));
	eeprom_write_block(
		(const uint8_t*)(setup),
		(void*)(EEPROM_START_ADDRESS+1), sizeof(*setup));
}

/*****************************************************************************/
static int32_t
parse_integer_in_range(
	char*	s,
	const int32_t	min_value,
	const int32_t	max_value,
	const int32_t	old_value,
	PGM_P		name,
	PGM_P		unit)
{
	char		xbuf[128];
	int32_t		new_value;
	char*	endptr = s;

	errno = 0;
	new_value = strtol(s, &endptr, 10);

	if (errno==0 && endptr!=s) {
		if (new_value >= min_value && new_value<=max_value) {
			setup_send_P(name);

			sprintf_P(xbuf, PSTR(" is now %ld"), new_value);
			setup_send(xbuf);

			setup_send_char(' ');
			setup_send_P(unit);
			setup_send_P(PSTR(".\r\n"));
			return new_value;
		} else {
			setup_send_P(name);

			sprintf_P(xbuf, PSTR(" %ld"), new_value);
			setup_send(xbuf);

			sprintf_P(xbuf, PSTR(" is out of the range %ld .. %ld \r\n"), min_value, max_value);
			setup_send(xbuf);

			return old_value;
		}
	} else {
		setup_send_P(PSTR("Invalid integer specified.\r\n"));
		return old_value;
	}
}

/*****************************************************************************/
static unsigned int	input_length = 0;
static char		input_buffer[64];

void
setup_handle_input(
	const char 	c,
	SETUP*		setup)
{
	if (c == '\r') {
		setup_send_char('\n');
		input_buffer[input_length] = 0;
		// Look what we've got.
		if (input_length>0) {
			const char cmd = input_buffer[0];
			if (cmd == '?') {
				setup_print(setup);
			} else if (input_length>2) {
				switch (cmd) {
					case '0':
						setup->compass_offset_x10 = parse_integer_in_range(
							input_buffer + 2,
							-3600, 3600, setup->compass_offset_x10,
							PSTR("Compass offset"), PSTR("x 0.1deg"));
						setup_store_to_nvram(setup);
						break;
				}
			}
		}
		// Print prompt.
		setup_send_P(PSTR("\r\n>"));
		input_length = 0;
	} else if (c == 0x08) {
		// it is nice to handle backspace.
		if (input_length>0) {
			--input_length;
		}
	} else if (c!='\r' && c!='\n') {
		if (input_length+1<sizeof(input_buffer)) {
			input_buffer[input_length] = c;
			++input_length;
		} else {
			input_length = 0;
		}
	}
}

