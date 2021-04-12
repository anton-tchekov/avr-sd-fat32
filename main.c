/* AVR ATMega328p Audio Player
 * Channels: 1 (Mono), 2 (Stereo)
 * Bit resolutions: 8 bit
 * PWM Timer0 Frequency: 16 000 000 Hz (F_CPU) / 256 = 62 500 Hz (MAX)
 */

#include "sd.h"
#include "fat.h"
#include "util.h"
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>

#define PIN_LED               2
#define PIN_CHANNEL_RIGHT     5
#define PIN_CHANNEL_LEFT      6

#define BTN_DEBOUNCE_TICKS  200

#define ARRAY_LENGTH(a)        (sizeof(a) / sizeof(*a))

/* Timer 0: Noninverting Fast PWM on PD5 and PD6 (62.5 KHz) */
/* Timer 2: CTC Mode, Interrupt, Frequency is set separately */
#define TIMER_INIT() do { \
	TCCR2A = (1 << WGM21); \
	TIMSK2 = (1 << OCIE2A); \
	TCCR0A = (1 << COM0A1) | (1 << COM0B1) | \
		(1 << WGM01) | (1 << WGM00); \
	OCR0A = 0; \
	OCR0B = 0; \
} while(0);

/* Set Pins of the Led, the Left and the Right Channel to output */
/* Enable the internal pullup resistors for the buttons on PC0..3 */
#define IO_INIT() do { \
	DDRD = (1 << PIN_LED); \
	PORTC = 0x0F; \
} while(0);

/* Timer0 - No Prescaler, Timer2 - Prescaler F_CPU / 8 */
#define TIMER_START() do { \
	DDRD |= (1 << PIN_CHANNEL_RIGHT) | (1 << PIN_CHANNEL_LEFT); \
	TCCR2B = (1 << CS21); \
	TCCR0B = (1 << CS00); \
} while(0);

#define TIMER_STOP() do { \
	DDRD &= ~((1 << PIN_CHANNEL_RIGHT) | (1 << PIN_CHANNEL_LEFT)); \
	TCCR0B = 0; \
	TCCR2B = 0; \
} while(0);

struct
{
	uint32_t sample_rate;
	uint8_t compare_value;
}
/* OCR values for Timer 2 for different sample rates */
static sample_rates[] =
{
	{  8000, 250 },
	{ 11025, 181 },
	{ 16000, 125 },
	{ 22050,  91 },
	{ 24000,  83 },
	{ 32000,  63 },
	{ 44100,  45 },
	{ 48000,  42 }
};

struct
{
	uint8_t num_channels, compare_value;
	uint16_t offset;
	uint32_t data_len;
}
static volatile wi;

/* Double Buffering */
static uint8_t buf0[512];
static uint8_t buf1[512];

static uint16_t max[2];

static volatile uint8_t
	/* Set when the buffers are swapped */
	flag,
	/* The buffer from which currently is read */
	cbuf;

/* The currently output byte */
static uint16_t ibuf = 0;

static void error(void)
{
	TIMER_STOP();
	for(;;)
	{
		PORTD ^= (1 << PIN_LED);
		_delay_ms(500);
	}
}

static uint8_t wavinfo(void)
{
{
	uint16_t n;
	if(fat_fread(buf0, 512, &n) || n != 512)
	{
		return 1;
	}
}

	if(!mem_cmp(buf0, (uint8_t *)"RIFF", 4))
	{
		return 1;
	}

	if(!mem_cmp(buf0 + 8, (uint8_t *)"WAVEfmt ", 8))
	{
		return 1;
	}

	/* Subchunk1Size - PCM */
	if(ld_u32(buf0 + 16) != 16)
	{
		return 1;
	}

	/* AudioFormat - Linear Quantization */
	if(ld_u16(buf0 + 20) != 1)
	{
		return 1;
	}

	/* NumChannels */
	wi.num_channels = ld_u16(buf0 + 22);
	if(wi.num_channels != 1 && wi.num_channels != 2)
	{
		return 1;
	}

{
	/* SampleRate */
	uint8_t i;
	uint32_t sample_rate;
	sample_rate = ld_u32(buf0 + 24);
	for(i = 0; i < ARRAY_LENGTH(sample_rates); ++i)
	{
		if(sample_rate == sample_rates[i].sample_rate)
		{
			wi.compare_value = sample_rates[i].compare_value;
			break;
		}
	}

	if(i == ARRAY_LENGTH(sample_rates))
	{
		return 1;
	}
}

{
	const uint8_t *data_ptr;
	if(!(data_ptr = mem_mem(buf0 + 36, 512 - 36, (uint8_t *)"data", 4)))
	{
		return 1;
	}

	wi.offset = data_ptr - buf0 + 8;
	wi.data_len = ld_u32(data_ptr + 4);
}

	return 0;
}

int main(void)
{
	dir_t d;
	direntry_t di;
	uint8_t res, i, stopped = 0, btns[4];
	uint16_t n = 0;
	uint32_t nread = 0;

	IO_INIT();
	TIMER_INIT();
	TIMER_STOP();

	if(sd_init())
	{
		error();
	}

	if(fat_mount())
	{
		error();
	}

	if(fat_opendir(&d, "/"))
	{
		error();
	}

	for(;;)
	{
		while(!(res = fat_readdir(&d, &di)))
		{
			if(di.type)
			{
				continue;
			}

			if(fat_fopen(di.name))
			{
				continue;
			}

			if(wavinfo())
			{
				continue;
			}

			max[0] = 512;
			max[1] = 512;
			nread = 0;
			fat_fseek(0);
			ibuf = wi.offset;
			OCR2A = wi.compare_value;
			TIMER_START();
			sei();
			while(nread < wi.data_len)
			{
				if(flag)
				{
					if(fat_fread(cbuf ? buf0 : buf1, 512, &n))
					{
						error();
					}

					if(n < 512)
					{
						max[!cbuf] = n;
					}

					nread += n;
					flag = 0;
				}

				for(i = 0; i < 4; ++i)
				{
					if(!(PINC & (1 << i)))
					{
						if(btns[i] < BTN_DEBOUNCE_TICKS)
						{
							++btns[i];
						}
						else if(btns[i] == BTN_DEBOUNCE_TICKS)
						{
							btns[i] = BTN_DEBOUNCE_TICKS + 1;
							switch(i)
							{
							case 0:
								/* Play/Pause */
								if(stopped)
								{
									TIMER_START();
								}
								else
								{
									TIMER_STOP();
								}

								stopped = !stopped;
								break;
							}
						}
					}
					else
					{
						btns[i] = 0;
					}
				}
			}

			TIMER_STOP();
			cli();
		}

		fat_readdir(&d, 0);
	}

	return 0;
}

ISR(TIMER2_COMPA_vect)
{
	if(wi.num_channels == 1)
	{
		/* mono */
		uint8_t sample = cbuf ? buf1[ibuf] : buf0[ibuf];
		OCR0A = sample;
		OCR0B = sample;
		if(++ibuf == max[cbuf])
		{
			ibuf = 0;
			cbuf = !cbuf;
			flag = 1;
		}
	}
	else
	{
		/* stereo */
		OCR0A = cbuf ? buf1[ibuf] : buf0[ibuf];
		OCR0B = cbuf ? buf1[ibuf + 1] : buf0[ibuf + 1];
		ibuf += 2;
		if(ibuf == max[cbuf])
		{
			ibuf = 0;
			cbuf = !cbuf;
			flag = 1;
		}
	}
}

