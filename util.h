#ifndef __UTIL_H__
#define __UTIL_H__

#include <stdint.h>

uint32_t ld_u32(const uint8_t *p);
uint16_t ld_u16(const uint8_t *p);
void mem_set(uint8_t *dst, uint8_t val, uint16_t cnt);
uint8_t mem_cmp(const uint8_t *dst, const uint8_t *src, uint16_t cnt);
const uint8_t *mem_mem(
	const uint8_t *haystack, uint16_t haystack_len,
	const uint8_t *needle, uint16_t needle_len);

#endif /* __UTIL_H__ */
