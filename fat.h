#ifndef __FAT_H__
#define __FAT_H__

#include "types.h"

typedef struct
{
	u16 index;
	u8 *fn;
	u32 sclust;
	u32 clust;
	u32 sect;
} dir_t;

typedef struct
{
	u32 size;
	u8 type;
	char name[13];
} direntry_t;

extern u32 fat_fsize;
extern u32 fat_ftell;
#define fat_fsize() fat_fsize
#define fat_ftell() fat_ftell

u8 fat_mount(void);
u8 fat_fopen(const char *path);
u8 fat_fread(void *buf, u16 btr, u16 *br);
u8 fat_fseek(u32 offset);

u8 fat_opendir(dir_t *dj, const char *path);
u8 fat_readdir(dir_t *dj, direntry_t *fno);

#endif /* __FAT_H__ */
