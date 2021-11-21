#ifndef setup_h_
#define setup_h_

#include <stdint.h>	// uint8_t, etc.
#include <stdbool.h>	// bool, true, false
#include "usart.h"	// uart2_PutChar

/** Setup channel. */

typedef struct {
	/** Compass offset, in units of 0.1 degrees. */
	int32_t	compass_offset_x10;
} SETUP;

/** CRC calculation. */
uint8_t
setup_crc(const SETUP* setup);

/** Load values from EEPROM. Set them to default when not found. */
bool
setup_load_from_nvram(SETUP* setup);

/** Store values to EEPROM. */
void
setup_store_to_nvram(const SETUP* setup);

extern void
setup_send(		const char*	s);

#define	setup_send_char(c)	uart3_PutChar((c))

extern void
setup_send_P(		PGM_P				s);

extern void
setup_send_hex(		const uint8_t	x);

void
setup_send_integer(
	PGM_P		name,
	const int32_t	i,
	PGM_P		unit);

/** Handle input. */
extern void
setup_handle_input(
	const char 	c,
	SETUP*		setup);

#endif /* setup_h_ */

