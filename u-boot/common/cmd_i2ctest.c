/*
 * (C) Copyright 2001
 * Gerald Van Baren, Custom IDEAS, vanbaren@cideas.com.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/*
 * I2C Functions similar to the standard memory functions.
 *
 * There are several parameters in many of the commands that bear further
 * explanations:
 *
 * {i2c_chip} is the I2C chip address (the first byte sent on the bus).
 *   Each I2C chip on the bus has a unique address.  On the I2C data bus,
 *   the address is the upper seven bits and the LSB is the "read/write"
 *   bit.  Note that the {i2c_chip} address specified on the command
 *   line is not shifted up: e.g. a typical EEPROM memory chip may have
 *   an I2C address of 0x50, but the data put on the bus will be 0xA0
 *   for write and 0xA1 for read.  This "non shifted" address notation
 *   matches at least half of the data sheets :-/.
 *
 * {addr} is the address (or offset) within the chip.  Small memory
 *   chips have 8 bit addresses.  Large memory chips have 16 bit
 *   addresses.  Other memory chips have 9, 10, or 11 bit addresses.
 *   Many non-memory chips have multiple registers and {addr} is used
 *   as the register index.  Some non-memory chips have only one register
 *   and therefore don't need any {addr} parameter.
 *
 *   The default {addr} parameter is one byte (.1) which works well for
 *   memories and registers with 8 bits of address space.
 *
 *   You can specify the length of the {addr} field with the optional .0,
 *   .1, or .2 modifier (similar to the .b, .w, .l modifier).  If you are
 *   manipulating a single register device which doesn't use an address
 *   field, use "0.0" for the address and the ".0" length field will
 *   suppress the address in the I2C data stream.  This also works for
 *   successive reads using the I2C auto-incrementing memory pointer.
 *
 *   If you are manipulating a large memory with 2-byte addresses, use
 *   the .2 address modifier, e.g. 210.2 addresses location 528 (decimal).
 *
 *   Then there are the unfortunate memory chips that spill the most
 *   significant 1, 2, or 3 bits of address into the chip address byte.
 *   This effectively makes one chip (logically) look like 2, 4, or
 *   8 chips.  This is handled (awkwardly) by #defining
 *   CONFIG_SYS_I2C_EEPROM_ADDR_OVERFLOW and using the .1 modifier on the
 *   {addr} field (since .1 is the default, it doesn't actually have to
 *   be specified).  Examples: given a memory chip at I2C chip address
 *   0x50, the following would happen...
 *     i2c md 50 0 10   display 16 bytes starting at 0x000
 *                      On the bus: <S> A0 00 <E> <S> A1 <rd> ... <rd>
 *     i2c md 50 100 10 display 16 bytes starting at 0x100
 *                      On the bus: <S> A2 00 <E> <S> A3 <rd> ... <rd>
 *     i2c md 50 210 10 display 16 bytes starting at 0x210
 *                      On the bus: <S> A4 10 <E> <S> A5 <rd> ... <rd>
 *   This is awfully ugly.  It would be nice if someone would think up
 *   a better way of handling this.
 *
 * Adapted from cmd_mem.c which is copyright Wolfgang Denk (wd@denx.de).
 */

#include <common.h>
#include <command.h>
#include <environment.h>

#include <asm/arch/kodari_micom.h>




int do_i2c_test(cmd_tbl_t *cmdtp, int flag,
		int argc, char * const argv[])
{
	u8 read_id = 0;
	unsigned char ChipId;
	int i=0;
	ChipId = 0x48;

	//read micom type
//	KODARI_MICOM_ADDR
	IIC7_ERead(KODARI_MICOM_ADDR, KODARI_BOARD_VER, &read_id);
	printf("micom type :%x\n", read_id);
//	IIC7_ERead(KODARI_MICOM_ADDR, KODARI_MICOM_MAJOR_VER, &read_id);
//	printf("micom major :%x\n", read_id);
	
//	IIC7_ERead(KODARI_MICOM_ADDR, KODARI_MICOM_MINOR_VER, &read_id);
//	printf("micom minor :%x\n", read_id);

	return 0;
}

U_BOOT_CMD(
	i2ctest,	1,	0,	do_i2c_test,
	"i2c test for kodari",
	"i2ctest\n"
);


