#include "uart_debug.h"
#include <avr/io.h>
#include <avr/pgmspace.h>

#define _BAUD (((F_CPU / (UART_BAUD * 16UL))) - 1)

void uart_init(void)
{
	UCSR0B = (1 << RXEN0) | (1 << TXEN0);
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
	UBRR0L = (uint8_t)(_BAUD & 0xFF);
	UBRR0H = (uint8_t)((_BAUD >> 8) & 0xFF);
}

void uart_tx(char c)
{
	while(!(UCSR0A & (1 << UDRE0))) ;
	UDR0 = c;
}

void uart_tx_str(const char *s)
{
	register char c;
	while((c = *s++)) { uart_tx(c); }
}

void uart_tx_str_P(const char *s)
{
	register char c;
	while((c = pgm_read_byte(s++))) { uart_tx(c); }
}
