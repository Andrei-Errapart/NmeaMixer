// vim: ts=2 shiftwidth=2
#include <avr/pgmspace.h>
#include <stdint.h>	// int32_t, etc.
#include <stdbool.h>	// true, false.
#include <string.h>	// strchr
#include <stdlib.h>
#include <stdio.h>	// sprintf
#include "setup.h"
#include "gps.h"


#ifndef GPS_DEBUG
#define	GPS_DEBUG	0
#endif

#ifndef GPS_IGNORE_FIX
#define	GPS_IGNORE_FIX	0
#endif

#ifndef GPS_USE_GPZDA
#define	GPS_USE_GPZDA	1
#endif

#define	GPS_MAX_FIELDS	20

/*****************************************************************************/
typedef struct {
	uint8_t	hour;			///< 0 .. 23
	uint8_t	minute;		///< 0 .. 59
	uint8_t	second;		///< 0 .. 59
	uint16_t	tick;			///< Internal, set to zero only. 0 .. 7199
} TIME;

/*****************************************************************************/
static uint8_t			gps_field_index = 0;	///< 0 - not receiving, 1=type (GPGGA), 2=time (HHMMSS.s/ss/sss), 3=lat, 4='N'/'S', 5=lon, 6='E'/'W', 7=quality (0=no fix, 1=fix, 2=diff. fix), 8=number of satellites per view, 9=PDOP, 10=altitude, 11=alt. unit, ..., *CHECKSUM
static uint8_t			gps_buffer[64];	// this 
static uint8_t			gps_buffer_index = 0;
static SENTENCE			gps_sentence = SENTENCE_NONE;
static bool			gps_is_checksum = false;
static bool			gps_has_fix = false;
static bool			gps_has_speed_knots = false;
static bool			gps_has_speed_kmph = false;
static bool			gps_has_course = false;
static uint16_t			gps_course_x100 = 0; // VTG only.
static uint16_t			gps_speed_knots_x100 = 0; // VTG only.
static uint16_t			gps_speed_kmph_x100 = 0; // VTG only.
static uint8_t			gps_checksum = 0;

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
gps_parse_course(
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

#define	gps_parse_speed	gps_parse_course

/*****************************************************************************/
SENTENCE
handle_gps_input(	const uint8_t		c,
			uint16_t*		course_x100,
			uint16_t*		speed_knots_x100,
			uint16_t*		speed_kmph_x100)
{
	SENTENCE	r = SENTENCE_NONE;

	switch (c) {
	case '$':
		// Start again.
		gps_field_index = 1;
		gps_buffer_index = 0;
		gps_sentence = SENTENCE_NONE;
		gps_is_checksum = false;
		gps_has_fix = false;
		gps_has_course = false;
		gps_has_speed_knots = false;
		gps_has_speed_kmph = false;
		gps_checksum = 0;
		gps_buffer[0] = 0;
		break;
	case '*':
		// Switch to checksum.
		gps_is_checksum = true;
		gps_buffer_index = 0;
		gps_buffer[0] = 0;
		break;
	case ',':
		gps_checksum = gps_checksum ^ c;
		// Field over.
		if (gps_field_index>=1 && gps_field_index + 1 < GPS_MAX_FIELDS) {
			if (gps_field_index==1) {
				if (gps_buffer_index==5) {
					if (memcmp_P(gps_buffer, PSTR("GPGGA"), 5)==0) {
						gps_sentence = SENTENCE_GGA;
					} else if (memcmp_P(gps_buffer, PSTR("GPZDA"), 5)==0) {
						gps_sentence = SENTENCE_ZDA;
					} else if (memcmp_P(gps_buffer, PSTR("GPVTG"), 5)==0) {
						gps_sentence = SENTENCE_VTG;
					}
				}
			} else if (gps_sentence == SENTENCE_GGA) {
#if (!GPS_USE_GPZDA)
				switch (gps_field_index) {
					case 7:
						// fix
						gps_has_fix = gps_buffer_index>0 && gps_buffer[0]!='0';
#if (GPS_DEBUG)
						if (gps_has_fix) {
							setup_send_P(PSTR("GOT FIX\r\n"));
						} else {
							setup_send_char('$');
							setup_send_hex(gps_buffer_index);
							setup_send_char('_');
							setup_send_hex(gps_buffer[0]);
							setup_send_P(PSTR("NO FIX\r\n"));
						}
						break;
#endif
#if (GPS_IGNORE_FIX)
						gps_has_fix = true;
#endif
				}
#endif
			} else if (gps_sentence == SENTENCE_ZDA) {
#if (GPS_USE_GPZDA)
				if (gps_field_index == 2) {
					gps_has_fix = true;
				}
#endif
			} else if (gps_sentence == SENTENCE_VTG) {
				switch (gps_field_index) {
					case 2:
						gps_has_course = gps_parse_course(&gps_course_x100, (const char*)gps_buffer, gps_buffer_index);
#if (0)
						setup_send_P(PSTR("\r\nbuffer="));
						setup_send(gps_buffer);
						setup_send_P(PSTR("\r\n"));
						setup_send_integer(PSTR("gps_course_x100"), gps_course_x100, PSTR(""));
#endif
						break;
					case 6:
						gps_has_speed_knots = gps_parse_speed(&gps_speed_knots_x100, (const char*)gps_buffer, gps_buffer_index);
						break;
					case 8:
						gps_has_speed_kmph = gps_parse_speed(&gps_speed_kmph_x100, (const char*)gps_buffer, gps_buffer_index);
						break;
				}
			}
		}
		gps_buffer_index = 0;
		gps_buffer[0] = 0;
		++gps_field_index;
		break;
	case 0x0D:
		// Check checksum.
		if (gps_buffer_index>=2 && hexchar_of_int(gps_checksum >> 4)==gps_buffer[0] && hexchar_of_int(gps_checksum & 0x0F)==gps_buffer[1]) {
			switch (gps_sentence) {
				case SENTENCE_GGA: /* fallthrough */
				case SENTENCE_ZDA:
					if (gps_has_fix) {
						// YES!
						r = gps_sentence;
#if (GPS_DEBUG)
						setup_send_P(PSTR("TIME OK\r\n"));
#endif
					} else {
#if (GPS_DEBUG)
						setup_send_P(PSTR("CK OK\r\n"));
#endif
					}
					break;
				case SENTENCE_VTG:
					if (gps_has_course && gps_has_speed_knots && gps_has_speed_kmph) {
						*course_x100 = gps_course_x100;
						*speed_knots_x100 = gps_speed_knots_x100;
						*speed_kmph_x100 = gps_speed_kmph_x100;
						r = gps_sentence;
					}
					break;
				default:
					break; // pass
			}
		} else {
#if (GPS_DEBUG)
			setup_send_hex(gps_sentence);
			setup_send_char(':');
			setup_send_hex(gps_checksum);
			setup_send_char('=');
			setup_send_char(gps_buffer[0]);
			setup_send_char(gps_buffer[1]);
			setup_send_P(PSTR("\r\n"));
#endif
		}
		// Clear.
		gps_field_index = 0;
		gps_has_fix = false;
		gps_has_course = false;
		gps_sentence = SENTENCE_NONE;
		break;
	case 0x0A:
		// pass.
		break;
	default:
		// Receiving?
		if (gps_field_index>0) {
			// !Overflow?
			if (gps_buffer_index+1 < sizeof(gps_buffer)/sizeof(gps_buffer[0])) {
				gps_buffer[gps_buffer_index] = c;
				++gps_buffer_index;
				gps_buffer[gps_buffer_index] = 0;	// prepare with zeros :)
				if (!gps_is_checksum) {
					gps_checksum ^= c;
				}
			} else {
				gps_field_index = 0; // restart on overflow.
			}
		}
	}

	return r;
}

/*****************************************************************************/
static void
append_checksum_crlf(	uint8_t*	buffer)
{
	uint8_t		checksum = 0;
	uint8_t*	ptr = buffer;

	for (; *ptr!=0; ++ptr)
	{
		const uint8_t	c = *ptr;
		if (c=='$')
		{
			continue;
		}
		else if (c=='*')
		{
			++ptr;
			break;
		}
		checksum = checksum ^ c;
	}

	sprintf((char*)ptr, "*%02X\r\n", (int)checksum);
}

/*****************************************************************************/
void
string_of_vtg(	uint8_t*		buffer,
		const uint16_t		course_x100,
		const uint16_t		speed_knots_x100,
		const uint16_t		speed_kmph_x100)
{
	/* $GPVTG:
	* 1) Track, degrees
	* 2) T = True
	* 3) Track, degrees
	* 4) M = Magnetic
	* 5) Speed, knots,
	* 6) N = Knots
	* 7) Speed, kilometers per hour
	* 8) K = Kilometers per hour
	* 9) Checksum
	*/
	sprintf((char*)buffer, "$GPVTG,%d.%02d,T,,M,%d.%02d,N,%d.%02d,K,D",
		course_x100/100,	course_x100 % 100,
		speed_knots_x100/100,	speed_knots_x100 % 100,
		speed_kmph_x100/100,	speed_kmph_x100 % 100
		);

	append_checksum_crlf(buffer);
}

