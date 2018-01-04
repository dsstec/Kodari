/* copyright (c) 2010 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <common.h>
#include <asm/io.h>
#include <asm/types.h>
#include <environment.h>
#include <command.h>
#include <fastboot.h>

#include <asm/errno.h>
#include <decompress_ext4.h>
#include <mmc.h>

#ifdef CONFIG_LCD)
#include<lcd.h>
#endif



void LCD_setwriteprogress(int percentage)
{
#ifdef CONFIG_KODARI_TYPE_MOBILE
		u32 width = panel_info.vl_col / 3;
#else
		u32 height = panel_info.vl_row / 3;
#endif
		u32 line = lcd_line_length;
		uchar *screen_base = (uchar *)lcd_base;
		u32 i, j;
		u32 length=0, length_h=0;
		if(percentage != 0){
#ifdef CONFIG_KODARI_TYPE_MOBILE
			length_h = (panel_info.vl_row * percentage) / 100;
#else
			length = (panel_info.vl_col * percentage) / 100;	
#endif
		}
	
		debug("%s - percentage:%i, length:%i(%i)\n", __func__, percentage, length, panel_info.vl_col);
#ifdef CONFIG_KODARI_TYPE_MOBILE
		if(length_h == 0) { 		
			for( i = (width * 1) +15; i < (width * 1) + 25; i++) {
				for (j = 0; j < panel_info.vl_row; j++) {
					memset(screen_base + j * line + i * 4 + 0, 0x00, 1);
					memset(screen_base + j * line + i * 4 + 1, 0x00, 1);
					memset(screen_base + j * line + i * 4 + 2, 0x00, 1);
					memset(screen_base + j * line + i * 4 + 3, 0x00, 1);
				}
			}
		} else {	
			for (i = (width * 1) +15; i < (width * 1) + 25; i++) {
				for (j = 0; j < length_h; j++) {	
					memset(screen_base + j * line + i * 4 + 0, 0x00, 1);
					memset(screen_base + j * line + i * 4 + 1, 0x00, 1);
					memset(screen_base + j * line + i * 4 + 2, 0xff, 1);
					memset(screen_base + j * line + i * 4 + 3, 0x00, 1);
				}
			}
		}
#else
		if(length == 0) {		
			for( i = (height * 2) +15; i < (height * 2) + 25; i++) {
				for (j = 0; j < panel_info.vl_col; j++) {
					memset(screen_base + i * line + j * 4 + 0, 0x00, 1);
					memset(screen_base + i * line + j * 4 + 1, 0x00, 1);
					memset(screen_base + i * line + j * 4 + 2, 0x00, 1);
					memset(screen_base + i * line + j * 4 + 3, 0x00, 1);
				}
			}
		} else {	
			for (i = (height * 2) +15; i < (height * 2) + 25; i++) {
				for (j = 0; j < length; j++) {
					memset(screen_base + i * line + j * 4 + 0, 0x00, 1);
					memset(screen_base + i * line + j * 4 + 1, 0x00, 1);
					memset(screen_base + i * line + j * 4 + 2, 0xff, 1);
					memset(screen_base + i * line + j * 4 + 3, 0x00, 1);
				}
			}
		}
#endif

}


#define SECTOR_BITS		9	/* 512B */

#define ext4_printf(args, ...)

static int write_raw_chunk(char* data, unsigned int sector, unsigned int sector_size);


int check_compress_ext4(char *img_base, unsigned long long parti_size) {
	ext4_file_header *file_header;

	file_header = (ext4_file_header*)img_base;

	if (file_header->magic != EXT4_FILE_HEADER_MAGIC) {
		return -1;
	}

	if (file_header->major != EXT4_FILE_HEADER_MAJOR) {
		printf("Invalid Version Info! 0x%2x\n", file_header->major);
		return -1;
	}

	if (file_header->file_header_size != EXT4_FILE_HEADER_SIZE) {
		printf("Invalid File Header Size! 0x%8x\n",
								file_header->file_header_size);
		return -1;
	}

	if (file_header->chunk_header_size != EXT4_CHUNK_HEADER_SIZE) {
		printf("Invalid Chunk Header Size! 0x%8x\n",
								file_header->chunk_header_size);
		return -1;
	}

	if (file_header->block_size != EXT4_FILE_BLOCK_SIZE) {
		printf("Invalid Block Size! 0x%8x\n", file_header->block_size);
		return -1;
	}

	if ((parti_size/file_header->block_size)  < file_header->total_blocks) {
		printf("Invalid Volume Size! Image is bigger than partition size!\n");
		printf("partion size %lld , image size %d \n",
			(parti_size/file_header->block_size), file_header->total_blocks);
		while(1);
	}

	/* image is compressed ext4 */
	return 0;
}

int write_raw_chunk(char* data, unsigned int sector, unsigned int sector_size) {
	char run_cmd[64];

	ext4_printf("write raw data in %d size %d \n", sector, sector_size);
	sprintf(run_cmd,"mmc write 0 0x%x 0x%x 0x%x", (int)data, sector, sector_size);
	run_command(run_cmd, 0);

	return 0;
}

int write_compressed_ext4(char* img_base, unsigned int sector_base) {
	unsigned int sector_size;
	int total_chunks;
	ext4_chunk_header *chunk_header;
	ext4_file_header *file_header;

	file_header = (ext4_file_header*)img_base;
	total_chunks = file_header->total_chunks;

	ext4_printf("total chunk = %d \n", total_chunks);

	img_base += EXT4_FILE_HEADER_SIZE;

	
	debug("%s - start\n", __func__);
	LCD_setwriteprogress(0);
	while(total_chunks) {		
		chunk_header = (ext4_chunk_header*)img_base;
		sector_size = (chunk_header->chunk_size * file_header->block_size) >> SECTOR_BITS;

		switch(chunk_header->type)
		{
		case EXT4_CHUNK_TYPE_RAW:
			ext4_printf("raw_chunk \n");
			write_raw_chunk(img_base + EXT4_CHUNK_HEADER_SIZE,
							sector_base, sector_size);
			sector_base += sector_size;
			break;

		case EXT4_CHUNK_TYPE_FILL:
			printf("*** fill_chunk ***\n");
			sector_base += sector_size;
			break;

		case EXT4_CHUNK_TYPE_NONE:
			ext4_printf("none chunk \n");
			sector_base += sector_size;
			break;

		default:
			printf("*** unknown chunk type ***\n");
			sector_base += sector_size;
			break;
		}
		total_chunks--;
		ext4_printf("remain chunks = %d \n", total_chunks);

		img_base += chunk_header->total_size;

		debug("%s - progress:%i\n", __func__, ((file_header->total_chunks - total_chunks) * 100)/file_header->total_chunks);

		LCD_setwriteprogress(((file_header->total_chunks - total_chunks) * 100)/file_header->total_chunks);
	};

	ext4_printf("write done \n");
	return 0;
}
