#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include "tlcdtest.h"

#define TRUE      1
#define FALSE      0

#define SUCCESS      0
#define FAIL      1

static int  tlcd_fd;

#define DRIVER_NAME      "/dev/cntlcd"
/******************************************************************************
*
*      TEXT LCD FUNCTION
*
******************************************************************************/
#define CLEAR_DISPLAY      0x0001
#define CURSOR_AT_HOME      0x0002

// Entry Mode set 
#define MODE_SET_DEF      0x0004
#define MODE_SET_DIR_RIGHT   0x0002
#define MODE_SET_SHIFT      0x0001

// Display on off
#define DIS_DEF            0x0008
#define DIS_LCD            0x0004
#define DIS_CURSOR         0x0002
#define DIS_CUR_BLINK      0x0001

// shift
#define CUR_DIS_DEF         0x0010
#define CUR_DIS_SHIFT      0x0008
#define CUR_DIS_DIR         0x0004

// set DDRAM  address 
#define SET_DDRAM_ADD_DEF   0x0080

// read bit
#define BUSY_BIT         0x0080
#define DDRAM_ADD_MASK      0x007F


#define DDRAM_ADDR_LINE_1   0x0000
#define DDRAM_ADDR_LINE_2   0x0040


#define SIG_BIT_E         0x0400
#define SIG_BIT_RW         0x0200
#define SIG_BIT_RS         0x0100

/***************************************************
read /write  sequence
write cycle
RS,(R/W) => E (rise) => Data => E (fall)

***************************************************/
int IsBusy(void)
{
	unsigned short wdata, rdata;

	wdata = SIG_BIT_RW;
	write(tlcd_fd, &wdata, 2);

	wdata = SIG_BIT_RW | SIG_BIT_E;
	write(tlcd_fd, &wdata, 2);

	read(tlcd_fd, &rdata, 2);

	wdata = SIG_BIT_RW;
	write(tlcd_fd, &wdata, 2);

	if (rdata &  BUSY_BIT)
		return TRUE;

	return FALSE;
}
int writecmd(unsigned short cmd)
{
	unsigned short wdata;

	if (IsBusy())
		return FALSE;

	wdata = cmd;
	write(tlcd_fd, &wdata, 2);

	wdata = cmd | SIG_BIT_E;
	write(tlcd_fd, &wdata, 2);

	wdata = cmd;
	write(tlcd_fd, &wdata, 2);

	return TRUE;
}

int setDDRAMAddr(int x, int y)
{
	unsigned short cmd = 0;
	
	if (IsBusy())
	{
		perror("setDDRAMAddr busy error.\n");
		return FALSE;

	}

	if (y == 1)
	{
		cmd = DDRAM_ADDR_LINE_1 + x;
	}
	else if (y == 2)
	{
		cmd = DDRAM_ADDR_LINE_2 + x;
	}
	else
		return FALSE;

	if (cmd >= 0x80)
		return FALSE;

	if (!writecmd(cmd | SET_DDRAM_ADD_DEF))
	{
		perror("setDDRAMAddr error\n");
		return FALSE;
	}
	
	usleep(1000);
	return TRUE;
}

int displayMode(int bCursor, int bCursorblink, int blcd)
{
	unsigned short cmd = 0;

	if (bCursor)
	{
		cmd = DIS_CURSOR;
	}

	if (bCursorblink)
	{
		cmd |= DIS_CUR_BLINK;
	}

	if (blcd)
	{
		cmd |= DIS_LCD;
	}

	if (!writecmd(cmd | DIS_DEF))
		return FALSE;

	return TRUE;
}

int writeCh(unsigned short ch)
{
	unsigned short wdata = 0;

	if (IsBusy())
		return FALSE;

	wdata = SIG_BIT_RS | ch;
	write(tlcd_fd, &wdata, 2);

	wdata = SIG_BIT_RS | ch | SIG_BIT_E;
	write(tlcd_fd, &wdata, 2);

	wdata = SIG_BIT_RS | ch;
	write(tlcd_fd, &wdata, 2);
	usleep(1000);
	return TRUE;

}


int setCursorMode(int bMove, int bRightDir)
{
	unsigned short cmd = MODE_SET_DEF;

	if (bMove)
		cmd |= MODE_SET_SHIFT;

	if (bRightDir)
		cmd |= MODE_SET_DIR_RIGHT;

	if (!writecmd(cmd))
		return FALSE;
	return TRUE;
}

int functionSet(void)
{
	unsigned short cmd = 0x0038; // 5*8 dot charater , 8bit interface , 2 line

	if (!writecmd(cmd))
		return FALSE;
	return TRUE;
}

int writeStr(char* str)
{
	unsigned char wdata;
	int i;
	for (i = 0; i < strlen(str); i++)
	{
		if (str[i] == '_')
			wdata = (unsigned char)' ';
		else
			wdata = str[i];
		writeCh(wdata);
	}
	return TRUE;

}

#define LINE_NUM         2
#define COLUMN_NUM         16         
int clearScreen(int nline)
{
	int i;
	if (nline == 0)
	{
		if (IsBusy())
		{
			perror("clearScreen error\n");
			return FALSE;
		}
		if (!writecmd(CLEAR_DISPLAY))
			return FALSE;
		return TRUE;
	}
	else if (nline == 1)
	{
		setDDRAMAddr(0, 1);
		for (i = 0; i <= COLUMN_NUM; i++)
		{
			writeCh((unsigned char)' ');
		}
		setDDRAMAddr(0, 1);

	}
	else if (nline == 2)
	{
		setDDRAMAddr(0, 2);
		for (i = 0; i <= COLUMN_NUM; i++)
		{
			writeCh((unsigned char)' ');
		}
		setDDRAMAddr(0, 2);
	}
	return TRUE;
}

#define CMD_TXT_WRITE      0
#define CMD_CURSOR_POS      1
#define CMD_CEAR_SCREEN      2

int opentlcd()
{
	// open driver
	tlcd_fd = open(DRIVER_NAME, O_RDWR);
	if (tlcd_fd < 0)
	{
		perror("driver open error.\n");
		return 1;
	}

	return tlcd_fd;

}




int tlcdtest(char tlcdmode, int line , int num)   // c , 1, 1    && w , 1 , 1
{
	char str[17] = {'_', };
	int nCmdMode;
	int bCursorOn, bBlink, nline, nColumn;
	char strWtext[COLUMN_NUM + 1]={0,};
	

	functionSet();
	
	if (tlcdmode == 'c')
	{   //setting
		nCmdMode = CMD_CURSOR_POS;
		bCursorOn = 1;
		bBlink = 1;
		nline = line;
		nColumn = num;
		clearScreen(1);
		clearScreen(2);
	}
	

	
	usleep(2000);
	
	if (tlcdmode == 'w')
	{    
		nCmdMode = CMD_TXT_WRITE;
		nline=line;
		nColumn=num;
		printf("nline:%d ,nColumn:%d\n", nline, nColumn);
		int i, year, month, day, hour, min;
		char y[5], m[3], d[3], h[2], mi[2];
		char system[] = "System Running!";		

		struct tm *t;
		time_t timer;
		timer = time(NULL);
		t = gmtime(&timer);
		

		year = t->tm_year + 1900;
		month = t->tm_mon + 1;
		
		day = t->tm_mday;
		hour = t->tm_hour;
		min = t->tm_min;
		
		
		sprintf(y, "%d", year);
		sprintf(m, "%d", month);
		sprintf(d, "%d", day); // <-successful!!! -by Chae ha
		sprintf(h, "%d", hour);
		sprintf(mi, "%d", min);
		strcat(str, y);
		strcat(str, "_");
		strcat(str, m);
		strcat(str, "_");
		strcat(str, d);
		strcat(str, "_");
		strcat(str, h);
		strcat(str, ":");
		strcat(str, mi);
		
		strcpy(strWtext, str);
		if(nline == 2)
		{
			strcpy(strWtext, system);
		}

	}
	
	
	switch (nCmdMode)
	{

	case CMD_CURSOR_POS:   
		displayMode(1, bBlink, TRUE);
		setDDRAMAddr(nline - 1, nColumn);
		break;
	case CMD_TXT_WRITE:
		setDDRAMAddr(0, nline); 
		usleep(2000);
		writeStr(strWtext); 
		break;
	case CMD_CEAR_SCREEN:
		clearScreen(nline);
		break;
	 default: 
		printf("error\n");
	}

	return 0;
}
