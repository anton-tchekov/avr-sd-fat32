#ifndef __UART_DEBUG_H__
#define __UART_DEBUG_H__

#define UART_BAUD  9600

void uart_init(void);
void uart_tx(char c);
void uart_tx_str(const char *s);
void uart_tx_str_P(const char *s);

#endif /* __UART_DEBUG_H__ */
