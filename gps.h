// vim: ts=2 shiftwidth=2
#ifndef gps_h_
#define gps_h_


#include <stdint.h>	// int32_t, etc.
#include <stdbool.h>	// true, false.


typedef enum {
	SENTENCE_NONE = 0,
	SENTENCE_GGA = 1,
	SENTENCE_VTG = 2,
	SENTENCE_ZDA = 3,
} SENTENCE;

/** Handle gps input. */
extern SENTENCE
handle_gps_input(	const uint8_t		c,
			uint16_t*		course_x100,
			uint16_t*		speed_knots_x100,
			uint16_t*		speed_kmph_x100);

/** Construct $GPVTG sentence, including checksum and newline.
Buffer should be at least 64 chars.
*/
extern void
string_of_vtg(		uint8_t*		buffer,
			const uint16_t		course_x100,
			const uint16_t		speed_knots_x100,
			const uint16_t		speed_kmph_x100);

#endif /* gps_h_ */

