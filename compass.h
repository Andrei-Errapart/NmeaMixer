// vim: ts=2 shiftwidth=2
#ifndef compass_h_
#define compass_h_


#include <stdint.h>	// int32_t, etc.
#include <stdbool.h>	// true, false.


/** Handle gps input. */
/*
$--HDT,x.x,T*hh

x.x = heading degrees, true.
T = true
checksum
*/

extern bool
handle_compass_input(	const uint8_t		c,
			uint16_t*		course_x100);

#endif /* compass_h_ */

