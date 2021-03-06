#include "sd.h"
#include <string.h>
#include <avr/io.h>
#include <util/delay.h>

#if \
	defined(__AVR_ATmega8__) || \
	defined(__AVR_ATmega48__) || \
	defined(__AVR_ATmega48P__) || \
	defined(__AVR_ATmega88__) || \
	defined(__AVR_ATmega88P__) || \
	defined(__AVR_ATmega168__) || \
	defined(__AVR_ATmega168P__) || \
	defined(__AVR_ATmega328P__)

#define SPI_DIR                DDRB
#define SPI_OUT                PORTB
#define MOSI                   (1 << 3)
#define SCK                    (1 << 5)
#define CS                     (1 << 2)
#define MISO                   (1 << 4)

#elif \
	defined(__AVR_ATmega16__) || \
	defined(__AVR_ATmega32__)

#define SPI_DIR                DDRB
#define SPI_OUT                PORTB
#define MOSI                   (1 << 5)
#define SCK                    (1 << 7)
#define CS                     (1 << 4)
#define MISO                   (1 << 6)

#elif \
	defined(__AVR_ATmega64__) || \
	defined(__AVR_ATmega128__) || \
	defined(__AVR_ATmega169__)

#define SPI_DIR                DDRB
#define SPI_OUT                PORTB
#define MOSI                   (1 << 2)
#define SCK                    (1 << 1)
#define CS                     (1 << 0)
#define MISO                   (1 << 3)

#else
#error "Target AVR not supported: No pin mappings available."
#endif

#define CONF_SPI() { SPI_DIR |= MOSI | SCK | CS; SPI_DIR &= ~MISO; }
#define SELECT()               SPI_OUT &= ~CS
#define DESELECT()             SPI_OUT |= CS

#define CMD_GO_IDLE_STATE      0x00
#define CMD_SEND_OP_COND       0x01
#define CMD_SEND_IF_COND       0x08
#define CMD_SEND_CSD           0x09
#define CMD_SEND_CID           0x0A
#define CMD_SET_BLOCKLEN       0x10
#define CMD_READ_SINGLE_BLOCK  0x11
#define CMD_WRITE_SINGLE_BLOCK 0x18
#define CMD_SD_SEND_OP_COND    0x29
#define CMD_APP                0x37
#define CMD_READ_OCR           0x3A

#define IDLE_STATE             (1 << 0)
#define ILLEGAL_CMD            (1 << 2)

#define SD_1                   (1 << 0)
#define SD_2                   (1 << 1)
#define SD_HC                  (1 << 2)

static uint8_t _card_type;

static uint8_t _spi_xchg(uint8_t b)
{
	SPDR = b;
	while(!(SPSR & (1 << SPIF))) ;
	SPSR &= ~(1 << SPIF);
	return SPDR;
}

static uint8_t _command(uint8_t cmd, uint32_t arg)
{
	uint8_t i, response;
	_spi_xchg(0xFF);
	_spi_xchg(0x40 | cmd);
	_spi_xchg((arg >> 24) & 0xFF);
	_spi_xchg((arg >> 16) & 0xFF);
	_spi_xchg((arg >> 8) & 0xFF);
	_spi_xchg((arg >> 0) & 0xFF);
	switch(cmd)
	{
	case CMD_GO_IDLE_STATE:
		_spi_xchg(0x95);
		break;

	case CMD_SEND_IF_COND:
		_spi_xchg(0x87);
		break;

	default:
		_spi_xchg(0xFF);
		break;
	}

	for(i = 0; i < 10 && ((response = _spi_xchg(0xFF)) == 0xFF); ++i) ;
	return response;
}

uint8_t sd_init(void)
{
	uint8_t response;
	uint16_t i;
	uint32_t arg;

	CONF_SPI();
	DESELECT();
	SPCR = (0 << SPIE) | (1 << SPE)  | (0 << DORD) | (1 << MSTR) |
		(0 << CPOL) | (0 << CPHA) | (1 << SPR1) | (1 << SPR0);
	SPSR &= ~(1 << SPI2X);
	_card_type = 0;
	for(i = 0; i < 10; ++i)
	{
		_spi_xchg(0xFF);
	}

	SELECT();
	for(i = 0; ; ++i)
	{
		if(_command(CMD_GO_IDLE_STATE, 0) == IDLE_STATE)
		{
			break;
		}

		if(i == 0x1ff)
		{
			DESELECT();
			return 1;
		}
	}

	if((_command(CMD_SEND_IF_COND, 0x1AA) & ILLEGAL_CMD) == 0)
	{
		_spi_xchg(0xFF);
		_spi_xchg(0xFF);
		if(((_spi_xchg(0xFF) & 0x01) == 0) ||
			(_spi_xchg(0xFF) != 0xAA))
		{
			return 1;
		}

		_card_type |= SD_2;
	}
	else
	{
		_command(CMD_APP, 0);
		if((_command(CMD_SD_SEND_OP_COND, 0) & ILLEGAL_CMD) == 0)
		{
			_card_type |= SD_1;
		}
	}

	for(i = 0; ; ++i)
	{
		if(_card_type & (SD_1 | SD_2))
		{
			arg = 0;
			if(_card_type & SD_2)
			{
				arg = 0x40000000;
			}

			_command(CMD_APP, 0);
			response = _command(CMD_SD_SEND_OP_COND, arg);
		}
		else
		{
			response = _command(CMD_SEND_OP_COND, 0);
		}

		if((response & IDLE_STATE) == 0)
		{
			break;
		}

		if(i == 0x7FFF)
		{
			DESELECT();
			return 1;
		}
	}

	if(_card_type & SD_2)
	{
		if(_command(CMD_READ_OCR, 0))
		{
			DESELECT();
			return 1;
		}

		if(_spi_xchg(0xFF) & 0x40)
		{
			_card_type |= SD_HC;
		}

		_spi_xchg(0xFF);
		_spi_xchg(0xFF);
		_spi_xchg(0xFF);
	}

	if(_command(CMD_SET_BLOCKLEN, 512))
	{
		DESELECT();
		return 1;
	}

	DESELECT();
	SPCR &= ~((1 << SPR1) | (1 << SPR0));
	SPSR |= (1 << SPI2X);
	_delay_ms(20);
	return 0;
}

#if defined(SD_EN_INFO) && SD_EN_INFO
uint8_t sd_get_info(sd_info *info)
{
	uint8_t i, b, csd_read_bl_len, csd_c_size_mult, csd_structure;
	uint16_t csd_c_size;
	memset(info, 0, sizeof(*info));
	SELECT();

	/* Read CID register */
	if(_command(CMD_SEND_CID, 0))
	{
		DESELECT();
		return 0;
	}

	while(_spi_xchg(0xFF) != 0xFE) ;

	for(i = 0; i < 18; ++i)
	{
		b = _spi_xchg(0xFF);
		switch(i)
		{
		case 0:
		{
			info->manufacturer = b;
			break;
		}

		case 1:
		case 2:
		{
			info->oem[i - 1] = b;
			break;
		}

		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
		{
			info->product[i - 3] = b;
			break;
		}

		case 8:
		{
			info->revision = b;
			break;
		}

		case 9:
		case 10:
		case 11:
		case 12:
		{
			info->serial |= (uint32_t) b << ((12 - i) * 8);
			break;
		}

		case 13:
		{
			info->manufacturing_year = b << 4;
			break;
		}

		case 14:
		{
			info->manufacturing_year |= b >> 4;
			info->manufacturing_month = b & 0x0f;
			break;
		}
		}
	}

	/* Read CSD register */
	csd_read_bl_len = 0;
	csd_c_size_mult = 0;
	csd_structure = 0;
	csd_c_size = 0;

	if(_command(CMD_SEND_CSD, 0))
	{
		DESELECT();
		return 0;
	}

	while(_spi_xchg(0xFF) != 0xFE) ;

	for(i = 0; i < 18; ++i)
	{
		b = _spi_xchg(0xFF);
		if(i == 0)
		{
			csd_structure = b >> 6;
		}
		else if(i == 14)
		{
			if(b & 0x40)
			{
				info->flag_copy = 1;
			}

			if(b & 0x20)
			{
				info->flag_write_protect = 1;
			}

			if(b & 0x10)
			{
				info->flag_write_protect_temp = 1;
			}

			info->format = (b & 0x0C) >> 2;
		}
		else
		{
			if(csd_structure == 0x01)
			{
				switch(i)
				{
				case 7:
				{
					b &= 0x3f;
				}

				case 8:
				case 9:
				{
					csd_c_size <<= 8;
					csd_c_size |= b;
					++csd_c_size;
					info->capacity = (uint32_t)csd_c_size << 10;
				}
				}
			}
			else if(csd_structure == 0x00)
			{
				switch (i)
				{
				case 5:
				{
					csd_read_bl_len = b & 0x0F;
					break;
				}

				case 6:
				{
					csd_c_size = b & 0x03;
					csd_c_size <<= 8;
					break;
				}

				case 7:
				{
					csd_c_size |= b;
					csd_c_size <<= 2;
					break;
				}

				case 8:
				{
					csd_c_size |= b >> 6;
					++csd_c_size;
					break;
				}

				case 9:
				{
					csd_c_size_mult = b & 0x03;
					csd_c_size_mult <<= 1;
					break;
				}

				case 10:
				{
					csd_c_size_mult |= b >> 7;
					info->capacity = ((uint32_t)csd_c_size <<
						(csd_c_size_mult + csd_read_bl_len + 2)) >> 9;
					break;
				}
				}
			}
		}
	}

	DESELECT();
	return 1;
}
#endif /* SD_EN_INFO */

#if defined(SD_EN_READ) && SD_EN_READ
uint8_t sd_read
	(uint8_t *buffer, uint32_t block, uint16_t offset, uint16_t count)
{
	uint16_t i;
	SELECT();
	if(_command(CMD_READ_SINGLE_BLOCK,
		_card_type & SD_HC ? block : (block >> 9)))
	{
		DESELECT();
		return 1;
	}

	for(i = 0; ; ++i)
	{
		if(_spi_xchg(0xFF) == 0xFE)
		{
			break;
		}

		if(i == 0xFFFF)
		{
			DESELECT();
			return 1;
		}
	}

	for(i = 0; i < offset; ++i)
	{
		_spi_xchg(0xFF);
	}

	for(; i < offset + count; ++i)
	{
		*buffer++ = _spi_xchg(0xFF);
	}

	for(; i < 512; ++i)
	{
		_spi_xchg(0xFF);
	}

	_spi_xchg(0xFF);
	_spi_xchg(0xFF);
	DESELECT();
	_spi_xchg(0xFF);
	return 0;
}
#endif /* SD_EN_READ */

#if defined(SD_EN_WRITE) && SD_EN_WRITE
uint8_t sd_write(const uint8_t *buffer, uint32_t block)
{
	uint16_t i;

	SELECT();
	if(_command(CMD_WRITE_SINGLE_BLOCK,
		(_card_type & SD_HC ? block : (block >> 9))))
	{
		DESELECT();
		return 0;
	}

	_spi_xchg(0xFE);
	for(i = 0; i < 512; ++i)
	{
		_spi_xchg(*buffer++);
	}

	_spi_xchg(0xFF);
	_spi_xchg(0xFF);
	while(_spi_xchg(0xFF) != 0xFF) ;
	_spi_xchg(0xFF);
	DESELECT();
	return 1;
}
#endif /* SD_EN_WRITE */

