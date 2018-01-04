/*
 * (C) Copyright 2011 Samsung Electronics Co. Ltd
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <common.h>
#include <asm/arch/kodari_micom.h>
#include <malloc.h>

#define 	PAGESIZE	256

#define 	CHIP_ID		0x30
#define 	CHIP_ID0	0x48

enum	Bootloader_Cmds{
		CMD_NONE,
		CMD_RESET,
		CMD_APP_EXEC,
		CMD_ERASE_APP,
		CMD_PROGRAM_FLASH,
		CMD_PROGRAM_EEPROM,
		CMD_FLASH_CRC,
		CMD_EEROM_CRC
};

typedef enum {false, true} bool;


void MicomDelay(void)
{
	unsigned long i;
	for(i=0;i<MICOM_DELAY;i++);
}

void MicomDelay_L(void)
{
	unsigned long i;
	for(i=0;i<100;i++)
		MicomDelay();
}

void IIC7_SCLH_SDAH(void)
{
	IIC7_ESCL_Hi;
	IIC7_ESDA_Hi;
	MicomDelay();
}

void IIC7_SCLH_SDAL(void)
{
	IIC7_ESCL_Hi;
	IIC7_ESDA_Lo;
	MicomDelay();
}

void IIC7_SCLL_SDAH(void)
{
	IIC7_ESCL_Lo;
	IIC7_ESDA_Hi;
	MicomDelay();
}

void IIC7_SCLL_SDAL(void)
{
	IIC7_ESCL_Lo;
	IIC7_ESDA_Lo;
	MicomDelay();
}


void IIC7_ELow(void)
{
	IIC7_SCLL_SDAL();
	IIC7_SCLH_SDAL();
	IIC7_SCLH_SDAL();
	IIC7_SCLL_SDAL();
}

void IIC7_EHigh(void)
{
	IIC7_SCLL_SDAH();
	IIC7_SCLH_SDAH();
	IIC7_SCLH_SDAH();
	IIC7_SCLL_SDAH();
}

void IIC7_EStart(void)
{
	IIC7_SCLH_SDAH();
	IIC7_SCLH_SDAL();
	MicomDelay();
	IIC7_SCLL_SDAL();
}

void IIC7_Erestart(void)
{
	IIC7_SCLL_SDAH();
	IIC7_SCLH_SDAH();
	udelay(5);
	IIC7_SCLH_SDAL();
	MicomDelay();
	IIC7_SCLL_SDAL();
}


void IIC7_EEnd(void)
{
	IIC7_SCLL_SDAL();
	IIC7_SCLH_SDAL();
	MicomDelay();
	IIC7_SCLH_SDAH();
}

void IIC7_ESDA_IN(void)
{
	IIC7_ESDA_INP;			// Function <- Input
	MicomDelay();
}

void IIC7_ESDA_OUT(void)
{
	IIC7_ESDA_INP;			// Function <- Input
	MicomDelay();
}

void IIC7_ESCL_LO(void)
{
	IIC7_ESCL_Lo;			//
	MicomDelay();
}

void IIC7_ESCL_HI(void)
{
	IIC7_ESCL_Hi;			//
	MicomDelay();
}

unsigned long IIC7_GET_SDA(void)
{
	return GPD0DAT;
}

unsigned char IIC7_EAck_write(void)
{

	unsigned long ack=0,ack_sts=0,ack_err=1,ack_timeout=0;

	IIC7_ESDA_INP;			// Function <- Input
	IIC7_ESCL_INP;			// Function <- Input

	for(ack_timeout=0;ack_timeout<100;ack_timeout++){
		
		udelay(1);
		ack_sts = GPD0DAT;
		ack_sts = (ack_sts>>3)&0x1;
		if(ack_sts==1){
			break;
		}

		if(ack_timeout>98){

			ack_err=1;
			printf("wait err 2");
			goto IIC7_EACK_RET;
		}

	}
	
	IIC7_ESCL_Hi;
	IIC7_ESCL_OUTP;
	MicomDelay();
	ack = GPD0DAT;
	IIC7_ESCL_Hi;
	MicomDelay();
	IIC7_ESCL_Hi;
	MicomDelay();
	IIC7_ESCL_Hi;
	MicomDelay();
	
	ack = (ack>>2)&0x1;
	//	while(ack!=0);
	if(ack){
		
		printf("wait err 3");
		goto IIC7_EACK_RET;
	}
	ack_err=0;
	IIC7_ESCL_Lo;
	MicomDelay();
	IIC7_ESCL_Lo;
	MicomDelay();

IIC7_EACK_RET:
	IIC7_ESDA_OUTP; 		// Function <- Output (SDA)
	
	IIC7_SCLL_SDAL();


	return ack_err;

}

unsigned char IIC7_BAck_write(void)
{

	unsigned long ack=0,ack_sts=0,ack_err=1,ack_timeout=0;

	IIC7_ESDA_INP;			// Function <- Input
	IIC7_ESCL_INP;			// Function <- Input

	for(ack_timeout=0;ack_timeout<100;ack_timeout++){
		
		udelay(1);
		ack_sts = GPD0DAT;
		ack_sts = (ack_sts>>3)&0x1;
		if(ack_sts==1){
			break;
		}

		if(ack_timeout>98){

			ack_err=1;
			printf("wait err 2");
			goto IIC7_EACK_RET;
		}

	}
	
	IIC7_ESCL_Hi;
	IIC7_ESCL_OUTP;
	MicomDelay();
	ack = GPD0DAT;
	IIC7_ESCL_Hi;
	MicomDelay();
	IIC7_ESCL_Hi;
	MicomDelay();
	IIC7_ESCL_Hi;
	MicomDelay();
	
	ack = (ack>>2)&0x1;
	//	while(ack!=0);
	if(ack){
		
		printf("wait err 3");
		goto IIC7_EACK_RET;
	}
	ack_err=0;
	IIC7_ESCL_Lo;
	MicomDelay();
	IIC7_ESCL_Lo;
	MicomDelay();

	for(ack_timeout=0;ack_timeout<100;ack_timeout++){
		
		udelay(1);
		ack_sts = GPD0DAT;
		ack_sts = (ack_sts>>3)&0x1;
		if(ack_sts==0){
			break;
		}

		if(ack_timeout>98){

			ack_err=1;
			printf("wait err 4");
			goto IIC7_EACK_RET;
		}

	}

IIC7_EACK_RET:
	IIC7_ESDA_OUTP; 		// Function <- Output (SDA)
	
	IIC7_SCLL_SDAL();


	return ack_err;

}


void IIC7_EAck_read(void)
{
	IIC7_ESDA_OUTP;			// Function <- Output

	IIC7_ESDA_Lo;
	IIC7_ESDA_Lo;

	IIC7_ESCL_Hi;
	MicomDelay();
	IIC7_ESCL_Hi;
	MicomDelay();
	IIC7_ESCL_Hi;
	MicomDelay();

	IIC7_ESCL_Lo;
	MicomDelay();		
	IIC7_ESCL_Lo;
	MicomDelay();		

	IIC7_ESDA_INP;			// Function <- Input (SDA)

	IIC7_SCLL_SDAL();
}

void IIC7_ENAck_read(void)
{
	IIC7_ESDA_OUTP;			// Function <- Output

	IIC7_ESDA_Hi;
	MicomDelay();
	IIC7_ESDA_Hi;

	IIC7_ESCL_Hi;
	MicomDelay();
	IIC7_ESCL_Hi;
	MicomDelay();

	IIC7_ESCL_Lo;
	MicomDelay();		
	IIC7_ESCL_Lo;
	MicomDelay();		

	IIC7_ESDA_INP;			// Function <- Input (SDA)

	IIC7_SCLL_SDAL();

}



void IIC7_ESetport(void)
{
	GPD0PUD &= ~(0xf<<4);	// Pull Up/Down Disable	SCL, SDA

	IIC7_ESCL_Hi;
	IIC7_ESDA_Hi;

	IIC7_ESCL_OUTP;		// Function <- Output (SCL)
	IIC7_ESDA_OUTP;		// Function <- Output (SDA)

	MicomDelay();
}
void IIC7_BRead (unsigned char ChipId, unsigned int IicAddr, unsigned char *IicData, unsigned int length)
{
	unsigned long reg;
	unsigned char data = 0, ntemp=0;
	unsigned char IicAddrH,IicAddrL;
	int i;

	IicAddrH=(IicAddr>>8);
	IicAddrL=(IicAddr);

	IIC7_EStart();

////////////////// write chip id //////////////////
	for(i = 7; i>0; i--)
	{
		if((ChipId >> (i -1)) & 0x0001) {	
			IIC7_EHigh();
		} else {	
			IIC7_ELow();
		}
	}
	IIC7_ELow();	// read

	if(IIC7_BAck_write()){
		printf("Micom commucation error !(chip ID) \n");
		goto END_MCU_I2C;
	}
	udelay(10);
////////////////// write reg. addr. //////////////////
	for(i = 8; i>0; i--)
	{
	//	printf("REG(0x%x) %i - %i\n", IicAddr, i, ((IicAddr >> (i-1)) & 0x0001));
		if((IicAddrL >> (i-1)) & 0x0001)
			IIC7_EHigh();
		else
			IIC7_ELow();
	}

	if(IIC7_BAck_write()){
		printf("Micom commucation error !(address low byte) \n");
		goto END_MCU_I2C;

	}
 
	udelay(10);
	for(i = 8; i>0; i--)
	{
	//	printf("REG(0x%x) %i - %i\n", IicAddr, i, ((IicAddr >> (i-1)) & 0x0001));
		if((IicAddrH >> (i-1)) & 0x0001)
			IIC7_EHigh();
		else
			IIC7_ELow();
	}

	if(IIC7_EAck_write()){
		printf("Micom commucation error !(address high byte\n");
		goto END_MCU_I2C;

	}
 
	IIC7_Erestart();
	udelay(10);
////////////////// write chip id //////////////////
	for(i = 7; i>0; i--)
	{
//		printf("2. ADDR(0x%x) %i - %i\n", ChipId, i, ((ChipId >> (i-1)) & 0x0001));
		if((ChipId >> (i-1)) & 0x0001)
			IIC7_EHigh();
		else
			IIC7_ELow();
	}

	IIC7_EHigh();	// read
	
	if(IIC7_EAck_write()){
		printf("Micom commucation error !(second address) \n");
		goto END_MCU_I2C;
	}

	udelay(10);	
////////////////// read reg. data. //////////////////
	for(ntemp=0;ntemp<length;ntemp++){
		IIC7_ESDA_INP;

		for(i = 8; i>0; i--)
		{
			IIC7_ESCL_Lo;		
			MicomDelay();
			IIC7_ESCL_Lo;		
			MicomDelay();
			IIC7_ESCL_Hi;
			MicomDelay();
			reg = GPD0DAT;
			IIC7_ESCL_Hi;
			MicomDelay();
			IIC7_ESCL_Hi;
			MicomDelay();		
			reg = (reg >> 2) & 0x1;
			data |= reg << (i-1);


		}
		IIC7_ESCL_Lo;		
		MicomDelay();
		IIC7_ESCL_Lo;		
		MicomDelay();

		if(ntemp==(length-1))	// last data?
			IIC7_ENAck_read();	// Nack
		else
			IIC7_EAck_read();	// ACK

		udelay(10);
		*(IicData+ntemp)=data;
		data=0;


	}

	IIC7_ESDA_OUTP;

END_MCU_I2C:
	IIC7_EEnd();
	
}

void IIC7_BWrite(unsigned char ChipId, unsigned int IicAddr, unsigned char *IicData, unsigned int length)
{
	unsigned long reg;
	unsigned char data = 0, ntemp=0;
	u16	ntemp_i;
	unsigned char IicAddrH,IicAddrL;
	int i;

	IicAddrH=(IicAddr>>8);
	IicAddrL=(IicAddr);

	IIC7_EStart();
////////////////// write chip id //////////////////
	for(i = 7; i>0; i--)
	{
		if((ChipId >> (i -1)) & 0x0001) {	
			IIC7_EHigh();
		} else {	
			IIC7_ELow();
		}
	}
	IIC7_ELow();	// read

	if(IIC7_BAck_write()){
		printf("Micom commucation error !(chip ID) \n");
		goto END_MCU_I2C;
	}
	udelay(10);
////////////////// write reg. addr. //////////////////
	for(i = 8; i>0; i--)
	{
	//	printf("REG(0x%x) %i - %i\n", IicAddr, i, ((IicAddr >> (i-1)) & 0x0001));
		if((IicAddrL >> (i-1)) & 0x0001)
			IIC7_EHigh();
		else
			IIC7_ELow();
	}

	if(IIC7_BAck_write()){
		printf("Micom commucation error !(address low byte) \n");
		goto END_MCU_I2C;

	}
 
	udelay(10);
	for(i = 8; i>0; i--)
	{
	//	printf("REG(0x%x) %i - %i\n", IicAddr, i, ((IicAddr >> (i-1)) & 0x0001));
		if((IicAddrH >> (i-1)) & 0x0001)
			IIC7_EHigh();
		else
			IIC7_ELow();
	}

	if(IIC7_EAck_write()){
		printf("Micom commucation error !(address high byte\n");
		goto END_MCU_I2C;

	}
 
	IIC7_Erestart();
	udelay(10);
////////////////// write chip id //////////////////
	for(i = 7; i>0; i--)
	{
//		printf("2. ADDR(0x%x) %i - %i\n", ChipId, i, ((ChipId >> (i-1)) & 0x0001));
		if((ChipId >> (i-1)) & 0x0001)
			IIC7_EHigh();
		else
			IIC7_ELow();
	}

	IIC7_ELow();	// Write
	
	if(IIC7_EAck_write()){
		printf("Micom commucation error !(second address) \n");
		goto END_MCU_I2C;
	}

	udelay(10);	

	for(i = 8; i>0; i--)
	{
	//	printf("REG(0x%x) %i - %i\n", IicAddr, i, ((IicAddr >> (i-1)) & 0x0001));
		if((IicAddrL >> (i-1)) & 0x0001)
			IIC7_EHigh();
		else
			IIC7_ELow();
	}

	if(IIC7_BAck_write()){
		printf("Micom commucation error !(address low byte) \n");
		goto END_MCU_I2C;

	}
 
	udelay(10);
	for(i = 8; i>0; i--)
	{
	//	printf("REG(0x%x) %i - %i\n", IicAddr, i, ((IicAddr >> (i-1)) & 0x0001));
		if((IicAddrH >> (i-1)) & 0x0001)
			IIC7_EHigh();
		else
			IIC7_ELow();
	}

	if(IIC7_EAck_write()){
		printf("Micom commucation error !(address high byte\n");
		goto END_MCU_I2C;

	}
	udelay(10);
////////////////// read reg. data. //////////////////

	for(ntemp_i=0;ntemp_i<length;ntemp_i++){

		for(i = 8; i>0; i--)
		{
			if((*(IicData+ntemp_i) >> (i-1)) & 0x0001)
				IIC7_EHigh();
			else
				IIC7_ELow();
		}
		if(IIC7_BAck_write()){
			printf("Micom commucation error !(address low byte)  \d \n",ntemp_i);
			goto END_MCU_I2C;
		
		}
		udelay(10);
	}

	IIC7_ESDA_OUTP;

END_MCU_I2C:
	IIC7_EEnd();

}

void IIC7_EWrite (unsigned char ChipId, unsigned char IicAddr, unsigned char IicData)
{
	unsigned long i;

	IIC7_EStart();


////////////////// write chip id //////////////////
	for(i = 7; i>0; i--)
	{
		if((ChipId >> (i -1)) & 0x0001) {	
			IIC7_EHigh();
		} else {	
			IIC7_ELow();
		}
	}
	IIC7_ELow();	// write 'W'

	if(IIC7_EAck_write()){
		printf("Micom commucation error !(w address) \n");
		goto END_MCU_I2C_W;
	}
	udelay(10);

////////////////// write reg. addr. //////////////////
	for(i = 8; i>0; i--)
	{
		if((IicAddr >> (i-1)) & 0x0001)
			IIC7_EHigh();
		else
			IIC7_ELow();
	}

	if(IIC7_EAck_write()){
		printf("Micom commucation error !(w command) \n");
		goto END_MCU_I2C_W;
	}
	udelay(10);

	IIC7_Erestart();

	udelay(10);
////////////////// write chip id //////////////////
	for(i = 7; i>0; i--)
	{
//		printf("2. ADDR(0x%x) %i - %i\n", ChipId, i, ((ChipId >> (i-1)) & 0x0001));
		if((ChipId >> (i-1)) & 0x0001)
			IIC7_EHigh();
		else
			IIC7_ELow();
	}

//	IIC7_EHigh();	// read
	IIC7_ELow();	// write 'W'
	
	if(IIC7_EAck_write()){
		printf("Micom commucation error !(second address) \n");
		goto END_MCU_I2C_W;
	}

	udelay(10);
////////////////// write reg. data. //////////////////
	for(i = 8; i>0; i--)
	{
		if((IicData >> (i-1)) & 0x0001)
			IIC7_EHigh();
		else
			IIC7_ELow();
	}


	if(IIC7_EAck_write()){
		printf("Micom commucation error !(w data) \n");
		goto END_MCU_I2C_W;
	}
	udelay(10);

END_MCU_I2C_W:
	IIC7_EEnd();
}


void IIC7_ETest ()
{
	IIC7_EStart();
	IIC7_EEnd();
}


void IIC7_ERead (unsigned char ChipId, unsigned char IicAddr, unsigned char *IicData)
{
	unsigned long reg;
	unsigned char data = 0;
	int i;


	IIC7_EStart();

////////////////// write chip id //////////////////
	for(i = 7; i>0; i--)
	{
		if((ChipId >> (i -1)) & 0x0001) {	
			IIC7_EHigh();
		} else {	
			IIC7_ELow();
		}
	}
	IIC7_ELow();	// write

	if(IIC7_EAck_write()){
		printf("Micom commucation error !(address) \n");
		goto END_MCU_I2C;
	}
	udelay(10);
////////////////// write reg. addr. //////////////////
	for(i = 8; i>0; i--)
	{
	//	printf("REG(0x%x) %i - %i\n", IicAddr, i, ((IicAddr >> (i-1)) & 0x0001));
		if((IicAddr >> (i-1)) & 0x0001)
			IIC7_EHigh();
		else
			IIC7_ELow();
	}

	if(IIC7_EAck_write()){
		printf("Micom commucation error !(command) \n");
		goto END_MCU_I2C;

	}

//	IIC7_EStart();
	IIC7_Erestart();

	udelay(10);
////////////////// write chip id //////////////////
	for(i = 7; i>0; i--)
	{
//		printf("2. ADDR(0x%x) %i - %i\n", ChipId, i, ((ChipId >> (i-1)) & 0x0001));
		if((ChipId >> (i-1)) & 0x0001)
			IIC7_EHigh();
		else
			IIC7_ELow();
	}

	IIC7_EHigh();	// read
	
	if(IIC7_EAck_write()){
		printf("Micom commucation error !(second address) \n");
		goto END_MCU_I2C;
	}

	udelay(10);
////////////////// read reg. data. //////////////////
	IIC7_ESDA_INP;
	for(i = 8; i>0; i--)
	{
		IIC7_ESCL_Lo;		
		MicomDelay();
		IIC7_ESCL_Lo;		
		MicomDelay();
		IIC7_ESCL_Hi;
		MicomDelay();
		reg = GPD0DAT;
		IIC7_ESCL_Hi;
		MicomDelay();
		IIC7_ESCL_Hi;
		MicomDelay();		
		reg = (reg >> 2) & 0x1;
		data |= reg << (i-1);
	}

	IIC7_EAck_read();	// ACK
	IIC7_ESDA_OUTP;

END_MCU_I2C:
	IIC7_EEnd();

	*IicData = data;

}

unsigned int pagecnt=0,imagecnt=0;

unsigned char BYTE_Hex2bin(unsigned char first, unsigned char second)
{
	unsigned char decimal=0, decimal2=0;

	
	if(first>='0' && first<='9')
		decimal+=(first-'0');
	if(first>='A' && first<='F')
		decimal+=(first-55);
	if(first>='a' && first<='f')
		decimal+=(first-87);
	
	if(second>='0' && second<='9')
		decimal2+=(second-'0');
	if(second>='A' && second<='F')
		decimal2+=(second-55);
	if(second>='a' && second<='f')
		decimal2+=(second-87);

	return ((decimal<<4)|(decimal2));
}

void Converter2binTable(unsigned char *image,unsigned char *Bimage)
{
	unsigned char nLine,ntemp0,temp00;
	unsigned char *Old_img;
	Old_img=Bimage;
	pagecnt=1;
	imagecnt=0;

	do{
		image++;										// skip  ":"
		if(*image=='0' && *(image+1)=='0')	break;		// last line

		nLine=BYTE_Hex2bin(*image,*(image+1));

		image=image+8;										// skip size & address & record code

//		printf("line %x data  ", nLine);

		for(ntemp0=0;ntemp0<nLine;ntemp0++){
			*Bimage=BYTE_Hex2bin(*image,*(image+1));
//			printf("%x ",*Bimage);
			Bimage++;
			imagecnt++;	
			if((imagecnt%256) == 0)	pagecnt++;
			image=image+2;
		}
		image=image+4;							// skip check sum & enter(0x0d, 0x0a)
//		printf("\n");

	}while(true);

//	printf("total counter is %x ",imagecnt);
	Bimage=Old_img;

}

static u16 crc_ccitt_update(uint16_t crc, uint8_t data)
{
    data ^= crc & 0xFF;
    data ^= data << 4;

    return ((((uint16_t)data << 8) | ((crc & 0xFF00) >> 8)) ^ \
            (uint8_t)(data >> 4) ^ \
            ((uint16_t)data << 3));
}

u16 Calcurate_CRC(u8 *Bimage,u16 imagecnt)
{
	u16 crc = 0;
	u16 temp_dw = 0;
	
	while (temp_dw < imagecnt) {
		
			crc = crc_ccitt_update(crc, *(Bimage+temp_dw));
			++temp_dw;
	}

	return crc;
}

int MCU_flash_cmd(unsigned char *image)
{
	unsigned char *Bimage;
	unsigned int  nTemp0=0;
	unsigned char *I2ctemp,*I2ctemp_1;
	unsigned char temp00=0, temp01, temp02;
	unsigned int  itemp00,itemp01;
	unsigned char tempBuff[PAGESIZE];
	u16 	CalCRC=0;
	
	Bimage = (unsigned char *) malloc (0x20000);	// max 131,072 byte
	I2ctemp = (unsigned char *) malloc (sizeof(int));	// max 131,072 byte
	I2ctemp_1 = (unsigned char *) malloc (sizeof(int));	// max 131,072 byte

// check DC adapter
	IIC7_ERead(KODARI_MICOM_ADDR, KODARI_AC_ONLINE_0, &temp00);
	printf("DC status : %d\n",temp00);
	if(temp00==0){
		return 0xF;							// return fail. 	
	}
	IIC7_EWrite(CHIP_ID0,0x00,0x1);		// dummy write
	
	if(*image!=0x3A){		// Check Hex format
		return 1;			// Not hex file
	}

	for(itemp00=0;itemp00<0x20000;itemp00++)	*(Bimage+itemp00)=0xFF;

	Converter2binTable(image,Bimage);
	CalCRC=Calcurate_CRC(Bimage,pagecnt*256);

	printf("Start MCU Flash  \n");

	// 1. send setting booloader mode
	printf("1. send setting booloader mode \n");
	IIC7_EWrite(CHIP_ID0,0x0E,0xAB);
	udelay(500000);		// 500ms
	
	printf("1.1 check booloader mode \n");
	IIC7_BRead(CHIP_ID,0x0004,I2ctemp,2);
	if(*I2ctemp !=0xCD || *(I2ctemp+1) != 0xAB){
		printf("Fail! Entering booloader mode \n");
		return 2; 
	}

	// 2. send page size & number of page
	printf("2. send page size & number of page \n");
	itemp00=PAGESIZE;
	*I2ctemp  = itemp00;
	*I2ctemp_1= (itemp00>>8);
 	IIC7_BWrite(CHIP_ID,0x0000,I2ctemp,1);		// size of page
 	IIC7_BWrite(CHIP_ID,0x0001,I2ctemp_1,1);		// size of page

	itemp00=pagecnt;
	*I2ctemp  = itemp00;
	*I2ctemp_1= (itemp00>>8);
 	IIC7_BWrite(CHIP_ID,0x0002,I2ctemp,1);		// size of page
 	IIC7_BWrite(CHIP_ID,0x0003,I2ctemp_1,1);		// number of page

	// 3. erase application area
	printf("3. erase application area \n");
	*I2ctemp=CMD_ERASE_APP;
 	IIC7_BWrite(CHIP_ID,0x010A,I2ctemp,1);		// erase command
 	udelay(500000);		// 500ms
	

	// 4. Send data & Excute flash
	printf("4. Send data & Excute flash \n");
	for(itemp01=0;itemp01<pagecnt;itemp01++){
		itemp00=itemp01<<8;
		*I2ctemp  = itemp00;
		*I2ctemp_1= (itemp00>>8);
		
		IIC7_BWrite(CHIP_ID,0x0006,I2ctemp,1); 	// lower address
		IIC7_BWrite(CHIP_ID,0x0007,I2ctemp_1,1); 	// upper  address

		itemp00=PAGESIZE;
		*I2ctemp  = itemp00;
		*I2ctemp_1= (itemp00>>8);
		IIC7_BWrite(CHIP_ID,0x0008,I2ctemp,1); 	// length
		IIC7_BWrite(CHIP_ID,0x0009,I2ctemp_1,1);	// 

		tempBuff[0]=0;

		for(itemp00=0;itemp00<256;itemp00++) {
			tempBuff[itemp00]= *(Bimage+itemp00+(itemp01*PAGESIZE));
		}

		
		IIC7_BWrite(CHIP_ID,0x000A,tempBuff,PAGESIZE);	// send flash data

		*I2ctemp  = CMD_PROGRAM_FLASH;
		IIC7_BWrite(CHIP_ID,0x010A,I2ctemp,1); 		//  Send excute command

	}


	// 5. check CRC value
	printf("5. Send check CRC cmd \n");
	*I2ctemp=CMD_FLASH_CRC;
	IIC7_BWrite(CHIP_ID,0x010A,I2ctemp,1); 	//	Send CRC Cal command
	
 	udelay(500000);	// 500ms

	printf("6. check CRC value \n");
	IIC7_BRead(CHIP_ID,0x010B,I2ctemp,1);
	IIC7_BRead(CHIP_ID,0x010C,I2ctemp_1,1);

	itemp00=*I2ctemp;
	itemp00+=(unsigned int)*I2ctemp_1<<8;

	
	if(itemp00 != CalCRC){
		printf("CRC fail, so exit bootloader \n");
		printf("Image CRC : %X, MCU CRC : %X \n",CalCRC,itemp00);
		return 3;
	}
 
	 // 7. exit bootloader mode
	 printf("7. exit bootloader mode \n");
	 *I2ctemp=CMD_APP_EXEC;
	 IIC7_BWrite(CHIP_ID,0x010A,I2ctemp,1);	 //  Send excute application area

	 printf("Complited MCU flash ! \n");
	return 0;
}

