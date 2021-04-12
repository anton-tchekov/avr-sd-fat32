#ifndef __SD_H__
#define __SD_H__

#include <stdint.h>

#define SD_EN_READ  1
#define SD_EN_WRITE 0
#define SD_EN_INFO  0

#if defined(SD_EN_INFO) && SD_EN_INFO
typedef struct
{
	uint8_t manufacturer;
	uint8_t oem[3];
	uint8_t product[6];
	uint8_t revision;
	uint32_t serial;
	uint8_t manufacturing_year;
	uint8_t manufacturing_month;
	uint32_t capacity;
	uint8_t flag_copy;
	uint8_t flag_write_protect;
	uint8_t flag_write_protect_temp;
	uint8_t format;
} sd_info;
#endif

uint8_t sd_init(void);

#if defined(SD_EN_INFO) && SD_EN_INFO
uint8_t sd_get_info(sd_info *info);
#endif

#if defined(SD_EN_WRITE) && SD_EN_WRITE
uint8_t sd_write(const uint8_t *buffer, uint32_t block);
#endif

#if defined(SD_EN_READ) && SD_EN_READ
uint8_t sd_read
	(uint8_t *buffer, uint32_t block, uint16_t offset, uint16_t count);
#endif

#endif /* __SD_H__ */
