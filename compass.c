// vim: ts=2 shiftwidth=2
#include <avr/pgmspace.h>
#include <stdint.h>	// int32_t, etc.
#include <stdbool.h>	// true, false.
#include <string.h>	// strchr
#include <stdlib.h>
#include "setup.h"
#include "compass.h"


#define	COMPASS_MAX_FIELDS	20

/*****************************************************************************/
static uint8_t			compass_field_index = 0;	///< 0 - not receiving, 1=type (--HDT), 2=x.x, 3=T, *CHECKSUM
static uint8_t			compass_buffer[64];	// this 
static uint8_t			compass_buffer_index = 0;
static bool			is_compass_sentence = false;
static bool			compass_is_checksum = false;
static bool			compass_has_course = false;
static uint16_t			compass_course_x100 = 0;
static uint8_t			compass_checksum = 0;

/*****************************************************************************/
static uint16_t
parse_n_decimals(	const uint8_t*	s,
			const uint8_t	n)
{
	uint8_t	i;
	uint16_t	r = 0;
	for (i=0; i<n; ++i) {
		r = r*10 + (s[i]-'0');
	}
	return r;
}

/*****************************************************************************/
static uint8_t
hexchar_of_int(		const uint8_t	ii)
{
	return ii<10 ? ii + '0' : ii + 'A' - 10;
}

/*****************************************************************************/
static bool
compass_parse_course(
	uint16_t*	course_x100,
	const char*	s,
	const uint8_t	size)
{
	const char*	dotptr = strchr(s, '.');
	const int8_t	dotpos = dotptr==0
				? -1
				: (dotptr - s);
	uint16_t	before_comma = atoi(s);
	
	if (*s) {
		if (dotpos > 0) {
			const int8_t	n = size - dotpos - 1;
			const uint16_t	after_comma = parse_n_decimals((const uint8_t*)(s + dotpos + 1), n);
			switch (n) {
			case 1:
				*course_x100 = before_comma*100 + after_comma*10;
				return true;
			case 2:
				*course_x100 = before_comma*100 + after_comma;
				return true;
			case 3:
				*course_x100 = before_comma*100 + after_comma/10;
				return true;
			}
		} else {
			*course_x100 = before_comma * 100;
			return true;
		}
	}
	// nothing to do.
	return false;
}

/*****************************************************************************/
bool
handle_compass_input(	const uint8_t		c,
			uint16_t*		course_x100)
{
	bool	r = false;

	switch (c) {
	case '$':
		// Start again.
		compass_field_index = 1;
		compass_buffer_index = 0;
		is_compass_sentence = false;
		compass_is_checksum = false;
		compass_has_course = false;
		compass_checksum = 0;
		compass_buffer[0] = 0;
		break;
	case '*':
		// Switch to checksum.
		compass_is_checksum = true;
		compass_buffer_index = 0;
		compass_buffer[0] = 0;
		break;
	case ',':
		compass_checksum = compass_checksum ^ c;
		// Field over.
		if (compass_field_index>=1 && compass_field_index + 1 < COMPASS_MAX_FIELDS)
		{
			if (compass_field_index==1) {
				if (compass_buffer_index==5) {
					if (memcmp_P(compass_buffer+2, PSTR("HDT"), 3)==0) {
						is_compass_sentence = true;
					}
				}
			} else if (is_compass_sentence && compass_field_index==2) {
				// course
				compass_has_course = compass_parse_course(&compass_course_x100, (const char*)compass_buffer, compass_buffer_index);
				break;
			}
		}
		compass_buffer_index = 0;
		compass_buffer[0] = 0;
		++compass_field_index;
		break;
	case 0x0D:
		// Check checksum.
		if (compass_buffer_index>=2
			&& hexchar_of_int(compass_checksum >> 4)==compass_buffer[0]
			&& hexchar_of_int(compass_checksum & 0x0F)==compass_buffer[1]
			&& is_compass_sentence) {
			r = true;
			*course_x100 = compass_course_x100;
		} else {
#if (GPS_DEBUG)
			setup_send_char(compass_has_time ? '!' : '#');
			setup_send_hex(compass_sentence);
			setup_send_char(':');
			setup_send_hex(compass_checksum);
			setup_send_char('=');
			setup_send_char(compass_buffer[0]);
			setup_send_char(compass_buffer[1]);
			setup_send_P(PSTR("\r\n"));
#endif
		}
		// Clear.
		compass_field_index = 0;
		compass_has_course = false;
		is_compass_sentence = false;
		break;
	case 0x0A:
		// pass.
		break;
	default:
		// Receiving?
		if (compass_field_index>0) {
			// !Overflow?
			if (compass_buffer_index+1 < sizeof(compass_buffer)/sizeof(compass_buffer[0])) {
				compass_buffer[compass_buffer_index] = c;
				++compass_buffer_index;
				compass_buffer[compass_buffer_index] = 0;	// prepare with zeros :)
				if (!compass_is_checksum) {
					compass_checksum ^= c;
				}
			} else {
				compass_field_index = 0; // restart on overflow.
			}
		}
	}

	return r;
}

