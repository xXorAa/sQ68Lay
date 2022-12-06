/*
	QLAY - Sinclair QL emulator
	Copyright Jan Venema 1998
	QL input and output
*/

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>

#include "emu_options.hpp"
#include "m68k.h"
#include "q68_emulator.hpp"
#include "q68_hardware.hpp"
#include "qlay_keyboard.hpp"

namespace ipc {

uint32_t qlay1msec = 7500;

/* xternal? */
uint8_t qliord_b(uint32_t a);
uint32_t qliord_l(uint32_t a);
void qliowr_b(uint32_t a, uint8_t d);
void QL_exit(void);

/* internal */
static void do_1Hz(void);
static void do_mdv_motor(void);

/* internal */
void wr8049(uint32_t addr, uint8_t data);
static void exec_IPCcmd(int cmd);
static void wrmdvdata(uint8_t x);
void wrmdvcntl(uint8_t x);
static void wrserdata(uint8_t x);
void wrZX8302(uint8_t x);
static int decode_key(int key);
static void QLwrdisk(void);
static void QLrddisk(void);
static void mdv_next_sect(void);
static void init_mdvs(void);
static int dt_event(int dt_type);
static void mdv_select(int drive);
static uint8_t rdmouse(uint32_t a);
static void wrmouse(uint32_t a, uint8_t d);
static void init_events(void);
static void eval_next_event(void);
static int sound_process(int);
static void do_tx(void);
static int get_next_ser(void);
static void sclck(int, int);
static void ser_rcv_init(void);
static int ser_rcv_dequeue(int ch);
static int ser_rcv_size(int ch);

static int IPC020 = 0; /* current return value for 18020 */
static int IPCreturn; /* internal 8049 value */
static int IPCcnt; /* send bit counter */
static int IPCwfc = 1; /* wait for command */
uint8_t REG18021 = 0; /* interrupt control/status register 18021 */
static int ser12oc = 0; /* ser1,2 open: bit0: SER1, bit1: SER2 */
static int IPCbeeping = 0; /* BEEP is sounding */
int BEEPpars[16]; /* IPC beep parameters */

#define SERBUFLEN 23
//uint8_t	IPCserbuf[2][SERBUFLEN];/* serial receive buffers from 8049 */
static int IPCsercnt = 0; /* number of bytes left */
//static	int	IPCsersptr=0;		/* send to CPU pointer */
static int IPCchan = 0; /* 0: SER1, 1: SER2 */
#define SER_RCV_LEN 2048
static uint8_t ser_rcv_buf[2][SER_RCV_LEN];
static int ser_rcv_1st[2];
static int ser_rcv_fill[2];

static uint8_t Mdirreg = 0; /* mouse direction */
static uint8_t Mbutreg = 0; /* mouse buttons */
static int Mcount = 0; /* mouse interrupt repeat counter */
static int Mavail = 0; /* is the mouse detected, required and available? */
/* signal when mouse ints should occur */

uint32_t e50, emdv, emouse, esound, etx; /* events */

static int ZXmode; /* ZX8302 mode; sertx,net,mdv: bit4,5 of 18002 */
static int ZXbaud; /* ZX8302 sertx baudrate */
static int REG18020tx; /* ZX8302 sertx read register */
static uint32_t qlclkoff; /* QL hardware clock offset */

constexpr int NOSECTS = 255;
constexpr int SECTLEN = (14 + 14 + 512 + 26 + 120);
constexpr int NUMOFDRIVES = 8;

typedef struct {
	std::string name; /* file name */
	bool present; /* is it in? */
	bool wrprot; /* write protected */
	int sector; /* current sector */
	uint8_t data[NOSECTS * SECTLEN]; /* mdv drive data */
} mdvt;

std::vector<mdvt> mdrive(NUMOFDRIVES);

uint8_t *mdv; /* mdv drive data */
std::string mdvname; /* mdv file name */
int mdvnum; /* current mdv */
int mdvpresent; /* mdv in drive? */
int mdvwp; /* mdv write protected */
int mdvixb; /* mdv byte index */
int mdvixs; /* mdv sector index */
int mdvwrite; /* mdv r/w */
int mdvmotor; /* mdv motor running */
int mdvghstate; /* mdv g/h/g/s progress indicator */
int mdvdoub2; /* 2 2 mdv command */
int mdvwra; /* mdv write actions */

int fpr(const char* fmt, ...)
{
int     res;
va_list l;

        va_start(l, fmt);
        res = vfprintf(stderr, fmt, l);
        va_end(l);
        return res;
}

static int pulse=0;
static int bcnt=0;

uint8_t readQLHw(uint32_t addr)
{
	if (addr == 0x18020) {
		if (IPC020!=0) {
			int t = IPC020;
			IPC020 >>= 8;
			if(IPC020 == 0xa5) {
				IPC020=0; /* clear end marker */
			}

			return (uint8_t)t & 0xff;
		}

		{
			int t;
			t = dt_event(0);
			if (t < 42) {
				return 0;
			}
		}

		if ((ZXmode&0x10)==0) { /* TXSER mode */
			return REG18020tx;
		}

		if(mdvmotor==0) { /* no motor running, return NOP */
			return 0x0;
		} else { /* return MDV responses */
			if (!mdvpresent) {
				return(0);
			}
			if (mdvwrite == 0) {
			  	if ((mdvghstate==0)||(mdvghstate==2)) {
						/* find GAP?? */
						if(pulse==0) {
							if (mdvghstate==0) mdv_next_sect();
							pulse++; return(0x8); /*gap*/
						}
						if(pulse<0x18) {pulse++; return(0x0); }
						pulse=0;
						mdvghstate++;
						if (mdvghstate==1) { /* skip preamble */
							mdvixb=12;
						} else mdvixb=40;
						bcnt=0;
						return(0x00);
				}
				if(mdvghstate==1) { /* read header */
					bcnt++;
					if (bcnt<0x10) return(0x04); /* read buf read */
					mdvghstate=2;
					bcnt=0;
					return(0x04); /* read buf ready */
				}
				if(mdvghstate==3) { /* read sector */
					bcnt++;
					if (bcnt<0x264) return(0x04);
					mdvghstate=0;
					bcnt=0;
					return(0x04);
				}
			}
		}
	}

	if ((addr == emulator::pc_trak1) || (addr == emulator::pc_trak2)) {
		if ((mdvixb == 0x2c) && (mdvdoub2 == 1)&&(dt_event(1) < 750)) {/*970730*/
			mdvixb += 8; /* skip preamb */
			mdvdoub2 = 0;
		}
		mdvixb = mdvixb % SECTLEN; /* justincase*/
		mdvixb++;
		return mdv[mdvixs * SECTLEN + mdvixb - 1];
	}

	return 0;
}

void writeMdvSer(uint8_t data) {
	if ((ZXmode&0x18)==0x10) {
		wrmdvdata(data);
	} else {
		wrserdata(data);
	}
}

#if 0
/* process QL io read */
uint8_t qliord_b(uint32_t a)
{
int t;
static int pulse=0;
static int bcnt=0;
int p=0,q=0; /*debug*/

/* 0.82c: 18400-18800 is not for nfa! */
	if ( (a>=0x18100) && (a<0x18400) ) return rdnfa(a);
	if ( (a>=0x180c0) && (a<0x18100) ) return rdserpar(a);
	if ((a&0xffffff00)==0x1bf00) return rdmouse(a);
	if (a==0x18021) {
		if(q)fpr("R021:%x:%02x ",m68k_getpc(),REG18021);
		return REG18021;
	}
	if (a==0x18020) {
		if (IPC020!=0) {
			t=IPC020;
			IPC020>>=8;
			if(IPC020==0xa5)IPC020=0; /* clear end marker */
			return (uint8_t)t&0xff;
		}
		{ int t;t=dt_event(0); if (t<70) return(0); }/*970730*/
/*082g*/
		if ((ZXmode&0x10)==0) { /* TXSER mode */
if(0)			fpr("R020tx%d ",REG18020tx);
			return REG18020tx;
		}
		if(mdvmotor==0) { /* no motor running, return NOP */
			return(0x0);
		} else { /* return MDV responses */
			if (!mdvpresent) return(0);
			if (mdvwrite==0) {
			  	if ((mdvghstate==0)||(mdvghstate==2)) {
						/* find GAP?? */
if(p)						fpr("FG%d:%x ",mdvghstate,m68k_getpc());
if(p)						fpr("P%x ",pulse);
						if(pulse==0) {
							if (mdvghstate==0) mdv_next_sect();
/*							fpr("HR%02x ",mdvixs);*/
							pulse++; return(0x8); /*gap*/
						}
						if(pulse<0x18) {pulse++; return(0x0); }
						pulse=0;
						mdvghstate++;
						if (mdvghstate==1) { /* skip preamble */
							mdvixb=12;
						} else mdvixb=40;
						bcnt=0;
						return(0x00);
				}
				if(mdvghstate==1) { /* read header */
					bcnt++;
if(p)					fpr("GB%d:%x ",mdvghstate,m68k_getpc());
if(p)					fpr("BC%03x ",bcnt);
					if (bcnt<0x10) return(0x04); /* read buf read */
					mdvghstate=2;
					bcnt=0;
					return(0x04); /* read buf ready */
				}
				if(mdvghstate==3) { /* read sector */
					bcnt++;
if(p)					fpr("GB%d:%x ",mdvghstate,m68k_getpc());
if(p)					fpr("BC%03x ",bcnt);
					if (bcnt<0x264) return(0x04);
					mdvghstate=0;
					bcnt=0;
					return(0x04);
				}
			}
		}
	}

	if ((a==pc_trak1)||(a==pc_trak2)) {
if(p)fpr("*D*%d %02x %03x ",mdvghstate,mdvixs,mdvixb);
		if ((mdvixb==0x2c)&&(mdvdoub2==1)&&(dt_event(1)<125)) {/*970730*/
if(0)fpr("< ");
			mdvixb+=8; /* skip preamb */
			mdvdoub2=0;
		}
		mdvixb=mdvixb%SECTLEN; /* justincase*/
/*		fpr("B%03x %02x ",mdvixb,mdv[mdvixs*SECTLEN+mdvixb]);*/
		mdvixb++;
		return mdv[mdvixs*SECTLEN+mdvixb-1];
	}
	return 0; /* jodo-bu breaks????? */
	/* 0.82c are you still there? */
	return ( qliord_l(0x18000) >> (8*(3-(a&3))) ) & 0xff;
}

/* process QL io write */
void qliowr_b(uint32_t a, uint8_t d)
{
int p=0;/*debug*/
	if (a==0x18000) {if(p)fpr("W18000:%02x ",d);sclck(1,d);}
	if (a==0x18001) {if(p)fpr("W18001:%02x ",d);sclck(0,d);}
	if (a==0x18002) wrZX8302(d);
	if (a==0x18003) wr8049(a,d);
	if (a==0x18020) {if(p)fpr("W020:%x:%02x \n",m68k_getpc(),d);wrmdvcntl(d);}
	if (a==0x18021) {
		if(p)fpr("W021:%x:%02x ",m68k_getpc(),d);
		REG18021&=~d;				/* clear interrupts */
return;
		REG18021=(d&0xe0) | (REG18021&0x1f);	/* copy upper 3 bits */
		if ((d&0x20)==0) REG18021&=0xfe;	/* clear interrupt */
	}
	if (a==0x18022) {
		if(p)fpr("W022:%02x ",d);
		if ((ZXmode&0x18)==0x10) wrmdvdata(d);
		else wrserdata(d);
	}
	if (a==0x18023) {if(p)fpr("W023:%02x ",d);}
	// Jimmy (0094)- Correction of warning C4098: 'qliowr_b' : 'void' function returning a value
	// return wrmouse(a,d);  BECOMES:
	if ((a&0xffffff00)==0x1bf00) {wrmouse(a,d);return;}

	/* 98432 */
	if (a==0x18080) {
		if (d==0x55) dump_memory();
	}
	if ( (a>=0x180c0) && (a<0x18100) ) wrserpar(a,d);
	if (a>=0x18100) wrnfa(a,d);
}
#endif

static void wrmdvdata(uint8_t x)
{
	if (!mdvpresent)
		return; /* is this necessary ? */
	if (mdvwp)
		return;
	/*	if(mdvixb<0x40)fpr("M%02x%02x ",mdvixs,mdvixb);*/
	mdv[mdvixs * SECTLEN + mdvixb] = x;
	mdvwra++;
	mdvixb = (mdvixb + 1) % SECTLEN;
}

/*
DV control
at reset; and after data transfer of any mdv
	(02 00) 8 times			stop all motors
start an mdvN:
	(03 01) (02 00) N-1 times	start motorN
format (erase):
	0a
	(0e 0e) 0a per header and per sector
format (verify):
	02
	(02 02) after finding pulses per header/sector
format (write directory):
	0a (0e 0e) 02 write sector
format (find sector 0):
	(02 02) after pulses
format (write directory):
	0a (0e 0e) 02 write sector
	(02 00) 8 times 		stop motor

read sector:
	(02 02) after pulses
	get 4 bytes
	(02 02) indication to skip PLL sequence: 6*00,2*ff
*/

void wrmdvcntl(uint8_t x)
{
	static int mdvcnt = 0;
	static int mdvcntls = 0;
	static int currdrive = 0;
	static int mcnt = 0;
	static int pcntl = -1;
	int p = 0; /*debug*/
	int tmp;

	/*process mdv state*/
	if (p)
		fpr("*M* ");
	tmp = dt_event(1);

	if (tmp > 150) { /*970730*/
		//	if (tmp>120) { /*zkul980620*/
		if (p)
			fpr("p%d x%d; %d>25 %ld\n", pcntl, x, tmp, emulator::cycles());
		mdvcnt = 0;
		mcnt = 0;
		mdvdoub2 = 0;
		pcntl = -1;
	}

	if ((pcntl == 2) && (x == 2)) {
		mdvdoub2 = 1;
	}
	if ((pcntl == 2) && (x == 0)) {
		/*zkul980620*/
		//	if ((pcntl==2)&&((x==0)||(x==1))) {
		if (mdvwra) { /* stopping motor: first write to OS disk now */
			mdvwra = 0;
			//QLwrdisk();
		}
		if (mcnt) {
			currdrive++;
			mdv_select(currdrive);
		}
		mdvcnt++;
		/*		fpr("T%d ",emulator::cycle);*/
	}
	if ((pcntl == 3) && (x == 1)) {
		mcnt = 1;
		currdrive = 1;
		mdv_select(currdrive);
	}

	if (mdvcnt == 8) { /* stopped mdv motor */
		currdrive = 0;
		mdv_select(currdrive);
	}
	pcntl = x;
	/*end process state*/

	switch (x) {
	case 0x00: /* motor control */
		mdvghstate = 0;
		break;
	case 0x01: /* motor ?? */
		break;
	case 0x02: /* motor control */
		mdvwrite = 0;
		break;
	case 0x03: /* motor on */

		break;
	case 0x0a: /* erase off? */
		if (mdvixb > 0x40) { /* somewhere after the sector header */
			mdv_next_sect();
			mdvixb = 0;
		}
		/*		fpr("H/S%02x ",mdvixs);*/
		break;
	case 0x0e: /* erase on? */
		mdvwrite = 1;
		break;
	}
}

/* in: drive; 0: no drive, 1..8: select MDVdrive */
static void mdv_select(int drive)
{
	mdvnum = drive;
	mdvixb = 0;
	mdvwrite = 0;
	mdvghstate = 0;
	mdvdoub2 = 0;
	mdvwra = 0;

	if (drive == 0) {
		mdvname[0] = '\0';
		mdvpresent = 0;
		mdvixs = 0;
		mdvwp = 0; /* 1 ->just in case - 0 by Jimmy */
		mdvmotor = 0;
	} else {
		drive--; /* structure is one less */
		mdvname = mdrive[drive].name;
		mdvpresent = mdrive[drive].present;
		mdvixs = mdrive[drive].sector;
		mdvwp = mdrive[drive].wrprot;
		mdv = mdrive[drive].data;
		mdvmotor = 1;
	}
	//update_LED();
	/*{int i; for(i=0;i<8;i++) fpr("NAME %s ",mdrive[i].name);}*/
}

#if 0
void update_LED()
{
	int i; // Jimmy 093 - Remove : int s;

	draw_LED(Power_Led_X, Power_Led_Y, LED_YELLOW,
		 1); /* power on: yellow */
	for (i = 0; i < 8; i++) {
		if ((i + 1) == mdvnum)
			draw_LED(First_Led_X + i * 16, First_Led_Y, LED_RED,
				 1); /* full red */
		else {
			draw_LED(First_Led_X + i * 16, First_Led_Y, LED_BLACK,
				 1); /* full black */
			if (mdrive[i].present)
				draw_LED(First_Led_X + i * 16, First_Led_Y,
					 LED_RED, 0); /* red bar */
		}
	}
}
#endif

static void mdv_next_sect()
{
	mdvixs = (mdvixs + 1) % NOSECTS;
}

/*18002*/
void wrZX8302(uint8_t d)
{
	if (0)
		fpr("WZX2:%02x:%x ", d, m68k_get_reg(NULL, M68K_REG_PC));

	ZXmode = d & 0x18;
	switch (d & 7) {
	case 7:
		ZXbaud = 75;
		break;
	case 6:
		ZXbaud = 300;
		break;
	case 5:
		ZXbaud = 600;
		break;
	case 4:
		ZXbaud = 1200;
		break;
	case 3:
		ZXbaud = 2400;
		break;
	case 2:
		ZXbaud = 4800;
		break;
	case 1:
		ZXbaud = 9600;
		break;
	case 0:
		ZXbaud = 19200;
		break;
	}
}

static void wrserdata(uint8_t d)
{
	int ch;
	int p = 0;

	ch = 0;
	if (ZXmode == 0x8)
		ch = 1;
	//send_serial_char(ch, d);
	if (p)
		fpr("ST%02x(%c) ", d, d);
	if (p)
		fpr("ST%ld ", emulator::cycles());
	REG18020tx |= 0x02; /* set busy */
	etx = emulator::cycles() + 10000*qlay1msec / ZXbaud; /* clear busy after 10 bits transmitted */
	eval_next_event();
	if (p)
		fpr("ETX%ld ", etx);
}

/*
process 8049 commands (p92)
WR 18003: 8049  commands
0:
1: get interrupt status
2: open ser1
3: open ser2
4: close ser1/ser2
5:
6: serial1 receive
7: serial2 receive
8: read keyboard
9: keyrow
a: set sound
b: kill sound
c:
d: set serial baudrate
e: get response
f: test

write nibble to 8049:
'dbca' in 4 writes to 18003: 000011d0, 000011c0, 000011b0, 000011a0.
each write is acknowledged by 8049 with a '0' in bit 6 of 18020.

response from 8049:
repeat for any number of bits to be received (MSB first)
970718
{ write 00001101 to 18003; wait for '0' in bit6 18020; read bit7 of 18020 }

*/

void wr8049(uint32_t addr, uint8_t data)
{
	static int IPCrcvd = 1; /* bit marker */
	int IPCcmd;

	dt_event(1); /* mark this here, check delay when reading 18020 */
	if (IPCwfc) {
		if ((data & 0x0c) == 0x0c) {
			IPCrcvd <<= 1;
			IPCrcvd |= (data == 0x0c) ? 0 : 1;
			if ((IPCrcvd & 0x10) == 0x10) {
				IPCcmd = IPCrcvd & 0x0f;
				IPCrcvd = 1;
				IPCwfc = 0;
				/*printf("C%x ",IPCcmd);*/
				exec_IPCcmd(IPCcmd);
			}
		}
	} else {

		/* expect 0x0e */
		if (data != 0x0e) {
			fpr("ERRORIPC?:%x %x ", m68k_get_reg(NULL, M68K_REG_PC), data);
		}
		/*970718*/ IPC020 = 0;
		IPCcnt--;
		if (IPCreturn & (1 << IPCcnt))
			IPC020 |= 0x80;
		IPC020 <<=
			8; /* will be read as byte twice, sender will shift back */
		/* lower 16 bit have real value now, put in an end indicator */
		IPC020 |= 0xa50000;

		if (IPCcnt == 0) { /* this command session done */
			if (IPCsercnt) {
				/* serial receive buffer transfer in progress */
				/* prepare next byte */
				IPCreturn = ser_rcv_dequeue(IPCchan);
				//				fpr("R: %03x\n",IPCreturn);
				IPCsercnt--;
				IPCcnt = 8;
			} else {
				IPCwfc = 1; /* wait for next command */
			}
		}
	}
}

static void ser_rcv_init(void)
{
	int i;
	for (i = 0; i < 2; i++) {
		ser_rcv_1st[i] = 0;
		ser_rcv_fill[i] = 0;
	}
}

static int ser_rcv_dequeue(int ch)
{
	int next;
	//fpr("deq-0 f %d, 1 %d\n",ser_rcv_fill[ch],ser_rcv_1st[ch]);

	if (ser_rcv_fill[ch] == 0) {
		fpr("RCV buffer empty\n");
		return '.';
	}
	next = ser_rcv_1st[ch] - ser_rcv_fill[ch];
	if (next < 0)
		next = next + SER_RCV_LEN;
	ser_rcv_fill[ch]--;
	//fpr("deq-1 f %d, 1 %d\n",ser_rcv_fill[ch],ser_rcv_1st[ch]);
	return ser_rcv_buf[ch][next];
}

// called from ser rcv thread
void ser_rcv_enqueue(int ch, int len, uint8_t *b)
{
	int next, i;

	if (len > SER_RCV_LEN - ser_rcv_fill[ch]) {
		// fpr is NOT fully reentrant
		fpr("RCV buffer overflow; dropping chars\n");
		len = SER_RCV_LEN - ser_rcv_fill[ch];
	}
	ser_rcv_fill[ch] += len;

	next = ser_rcv_1st[ch];
	for (i = 0; i < len; i++) {
		ser_rcv_buf[ch][next] = b[i];
		next++;
		if (next >= SER_RCV_LEN)
			next = 0;
	}
	ser_rcv_1st[ch] = next;
}

static int ser_rcv_size(int ch)
{
	return ser_rcv_fill[ch];
}

const char *KEYS =
	"xv/n,826qe0tu9wi	r-yol3m1apdj[[ksf-g;]z.cb#m`\n00e0\\ 000500047";
static int decode_key(int key)
{
	int rk;
	/*  char *sp;
    fpr("%04x ",key);*/
	/*	sp=strchr(KEYS,key);
	if(sp!=NULL) rk=(int)sp-(int)&KEYS+3; else rk=key;
*/
	rk = key;
	rk |= 0x1000; /* 1 key */
	return rk;
}

static void exec_IPCcmd(int cmd)
{
	static int IPCpcmd = 0x10; /* previous */
	static int IPCbaud = 0;

	if (IPCpcmd == 0x0d) { /*baudr*/
		if (0)
			fpr("BRC:%d ", cmd);
		switch (cmd) {
		case 7:
			IPCbaud = 75;
			break;
		case 6:
			IPCbaud = 300;
			break;
		case 5:
			IPCbaud = 600;
			break;
		case 4:
			IPCbaud = 1200;
			break;
		case 3:
			IPCbaud = 2400;
			break;
		case 2:
			IPCbaud = 4800;
			break;
		case 1:
			IPCbaud = 9600;
			break;
		case 0:
			IPCbaud = 19200;
			break;
		}
		if (0)
			fpr("BR:%d ", IPCbaud);
		IPCwfc = 1;
		cmd = 0x10;
	}

	if (IPCpcmd == 0x09) { /*keyrow*/
		int row;
		row = cmd;
		//IPCreturn = get_keyrow(row);
		IPCcnt = 8;
		cmd = 0x10;
	}

	if (IPCpcmd == 0x0a) { /*sound*/
		static int params = 0;
		BEEPpars[params] = cmd;
		if (0)
			fpr("B%d:%x ", params, cmd);
		params++;
		if (params > 15) {
			IPCbeeping = 1;
			IPCpcmd = 0x10;
			params = 0;
			//sound_process(1);
		}
		IPCwfc = 1;
		return;
	}

	/*082d ipc cmd C has 1 nibble parameter, no reply */
	if (IPCpcmd == 0x0c) {
		fpr("IPCc'C'par%d ", cmd);
		IPCwfc = 1;
		IPCpcmd = 0x10;
		return;
	}

	if (IPCpcmd == 0x0f) { /*test*/
		static int params = 0;
		static int testval = 0;
		params++;
		fpr("TP%d:%x ", params, cmd);
		testval = 16 * testval + cmd;
		if (params > 1) {
			fpr("RTV%02x ", testval);
			IPCpcmd = 0x10;
			params = 0;
			IPCreturn = testval;
			testval = 0; /* for next time 'round */
			IPCcnt = 8;
			cmd = 0x10;
		} else {
			IPCwfc = 1;
			return;
		}
	}

	switch (cmd) {
	case 0: /* init */
		fpr("IPC%02x ", cmd);
		break;
	case 1: /* get interrupt status */
		/*fpr("GI ");*/
		IPCreturn = 0;
		SDL_AtomicLock(&qlaykbd::qlay_kbd_lock);
		if (qlaykbd::keyBuffer.size() || qlaykbd::keysPressed) {
			IPCreturn |= 0x01;
		}
		SDL_AtomicUnlock(&qlaykbd::qlay_kbd_lock);
		//if (use_debugger || fakeF1)
		//	IPCreturn |= 0x01; /* fake kbd interrupt */
		if (IPCbeeping)
			IPCreturn |= 0x02;
		if (ser12oc & 0x01) { /* SER1 */
			if (ser_rcv_size(0) > 0) {
				IPCreturn |= 0x10;
			}
		}
		if (ser12oc & 0x02) { /* SER2 */
			if (ser_rcv_size(1) > 0) {
				IPCreturn |= 0x20;
			}
		}
		IPCcnt = 8;
		break;
	case 2: /* SER1 open */
	case 3: /* SER2 open */
	case 4: /* SER1 close */
	case 5: /* SER2 close */
		if (cmd == 2) {
			ser12oc |= 1;
			fpr("Open Ser 1, ZXb: %d\n", ZXbaud);
			//open_serial(0, ZXbaud, 0);
		}
		if (cmd == 3) {
			ser12oc |= 2;
			fpr("Open Ser 2, ZXb: %d\n", ZXbaud);
			//open_serial(1, ZXbaud, 0);
		}
		if (cmd == 4) {
			ser12oc &= 0xfe;
			fpr("Close Ser 1\n");
			//close_serial(0);
		}
		if (cmd == 5) {
			ser12oc &= 0xfd;
			fpr("Close Ser 2\n");
			//close_serial(1);
		}
		fpr("IPC %02x, SER12OC %02x ", cmd, ser12oc);
		IPCwfc = 1;
		break;
	case 6: /* SER1 rcv */
	case 7: /* SER2 rcv */
		/* only called when 8049 sent a SER RCV interrupt */
		if (0)
			fpr("IPCrcv%02x ", cmd);
		IPCchan = cmd & 0x01;
		//			IPCsersptr=0;
		IPCsercnt = ser_rcv_size(IPCchan);
		if (IPCsercnt > 20)
			IPCsercnt = 20;
		IPCreturn = IPCsercnt;
		IPCcnt = 8;
		break;
	case 8: /* read keyboard */
	{
		int key;
		static int lastkey = 0;
		/*fpr("C8 ");*/
		IPCreturn = 0;
		IPCcnt = 4;
		SDL_AtomicLock(&qlaykbd::qlay_kbd_lock);
		if (qlaykbd::keyBuffer.size()) { /* just double check */
			key = qlaykbd::keyBuffer.front();
			qlaykbd::keyBuffer.pop();
			lastkey = key;
			/*fpr("Dec %x ",key);*/
			IPCreturn = decode_key(key);
			IPCcnt = 16;
		} else {
			if (qlaykbd::keysPressed) { /* still pressed: autorepeat */
				/*fpr("Dec KP ");*/
				IPCreturn = 0x8; /* just the repeat bit */
				IPCcnt = 4;
				/*970705: not good!*/
				/*					IPCreturn=decode_key(lastkey);
					IPCreturn|=0x8000;
					IPCcnt=16;
*/
			}
		}
		SDL_AtomicUnlock(&qlaykbd::qlay_kbd_lock);
#if 0
		if (use_debugger || fakeF1) {
			IPCreturn =
				0x1039; /* 1 char, no spec.key, F1=39, F2=3b */
			IPCcnt = 16;
			fakeF1--;
		}
#endif
		break;
	}
	case 0x9: /* keyrow */
		/*			fpr("KEYR ");*/
		IPCwfc = 1;
		break;
	case 0xa: /* set sound */
		if (0)
			fpr("BEEP ");
		IPCwfc = 1;
		break;
	case 0xb: /* kill sound */
		if (0)
			fpr("KILLBEEP ");
		IPCbeeping = 0;
		IPCwfc = 1;
		break;
	case 0xc: /* test interrupt 2?? */
		fpr("IPCcINT ");
		IPCwfc = 1;
		break;
	case 0xd: /* set baud rate */
		/*			fpr("BRATE ");*/
		IPCwfc = 1;
		break;
	case 0xe: /* get random, return 16 bits */
		IPCreturn = 0xabcd; /* can do better.. */
		IPCcnt = 16;
		fpr("IPCrnd %04x ", IPCreturn);
		IPCwfc = 1;
		break;
	case 0xf: /* test, return received byte */
		fpr("IPCtest ");
		IPCwfc = 1;
		break;
	case 0x10: /* fake: no more parameters */
		break;
	default:
		fpr("ERRORunexpIPCcmd:%x ", cmd);
		IPCreturn = 0;
		IPCcnt = 4;
		break;
	}
	IPCpcmd = cmd;
}

/* return how many emulator::cycles past previous dt_event() call */
/* dt_type=0 will read only since last update; 1 will update time */
static int dt_event(int dt_type)
{
	static uint32_t prevevent = 0x80000000;
	uint32_t rv;
	/* this is wrong?? see QABS 970801 */
	if (emulator::cycles() < prevevent)
		rv = prevevent - emulator::cycles();
	else
		rv = emulator::cycles() - prevevent;
	/*fpr("DT%d ",rv);*/
	if (dt_type)
		prevevent = emulator::cycles();
	return rv;
}

void initIPC()
{
	init_mdvs();
	mdv_select(0);
	init_events();
	ZXmode = 0;
	ZXbaud = 9600;
	REG18020tx = 0;
	ser_rcv_init();
	//start_speaker();
	qlclkoff = 0;
	emulator::cycleNextEvent = 0;
}

void QL_exit()
{
}
/** Externalis�e par Jimmy **/
void init_mdvs()
{
	char *p;
	int i = 0;

	for (auto &mdvName : options::mdvFiles) {
		if (i > NUMOFDRIVES) {
			break;
		}

		//mdrive[i].name[0] = "";
		mdrive[i].present = 0;
		mdrive[i].sector = 0;
		mdrive[i].wrprot = 0;

		if(mdvName.find("R:") == 0) {
			if (mdvName.size() == 2) {
				continue;
			}
			mdrive[i].wrprot = 1;
			mdvName.erase(0, 2);
		}

		mdrive[i].name = mdvName;
		mdv = mdrive[i].data;
		mdvpresent = 1;
		mdvname = mdvName;
		QLrddisk();
		mdrive[i].present = mdvpresent;

		i++;
	}

	if (i == 0) {
		fpr("No micro drives found\n");
	}
}

/* write diskimage in mdv[] to a file */
static void QLwrdisk()
{
#if 0
	FILE *qldisk;
	int addr;

	//fpr("Try write %s ",mdvname);
	if (mdvwp)
		return; /* don't write to OS */
	qldisk = fopen(mdvname, "wb");
	for (addr = 0; addr < NOSECTS * SECTLEN; addr++) {
		putc(mdv[addr], qldisk);
	}
	fclose(qldisk);
	//fpr("Wrote %s ",mdvname);
#endif
}

/* read diskimage from file to mdv[] */
static void QLrddisk()
{
	try {
		emulator::q68LoadFile(mdvname, mdv, NOSECTS * SECTLEN);
	} catch (std::exception &e) {
		std::cerr << "mdv load error: " << e.what() << std::endl;
		mdvpresent = 0;
	}
}

void init_events()
{
	e50 = emdv = emouse = esound = etx = 0;
}

void eval_next_event()
{
	/*
uint32_t	d50,dmdv,dmouse,dsound,dtx;
#define QMIN(a,b) ( (a)<(b) ? (a) : (b) )
#define QABSD(a,b) ( (a)-(b) )

	d50=QABSD(e50,emulator::cycle);
	dmdv=QABSD(emdv,emulator::cycle);
	dmouse=QABSD(emouse,emulator::cycle);
	dsound=QABSD(esound,emulator::cycle);
	dtx=QABSD(etx,emulator::cycle);
	emulator::cycle_next_event=emulator::cycle+QMIN(QMIN(QMIN(dsound,dtx),d50),QMIN(dmouse,dmdv));
*/
	uint32_t dc, dmin;
	dmin = e50 - emulator::cycles();
	dc = emdv - emulator::cycles();
	if (dc < dmin)
		dmin = dc;
#if 0
	dc = emouse - emulator::cycle;
	if (dc < dmin)
		dmin = dc;
	dc = esound - emulator::cycle;
	if (dc < dmin)
		dmin = dc;
#endif
	dc = etx - emulator::cycles();
	if (dc < dmin)
		dmin = dc;
	emulator::cycleNextEvent = emulator::cycles() + dmin;
}

/* a simple brute force implementation that works just fine ... */
void do_next_event()
{
	int e;

	uint8_t intmask = (m68k_get_reg(NULL, M68K_REG_SR) >> 8) & 7;
	e = 0;
	if (emulator::cycles()>=e50) { /* 50Hz interrupt, 20 msec */
        // do_50Hz();
		//m68k_set_irq(2);
		emulator::doIrq = true;
		emulator::q68_pc_intr |= emulator::pc_intrf;
        e50=emulator::cycles() + (20 * qlay1msec);
        e++;
        if(0)fpr("I5 ");
    }
	if (emulator::cycles() >= emdv) { /* MDV gap interrupt, 31 msec */
		if (intmask < 2) {
			do_mdv_motor();
		}
		emdv = emulator::cycles() + (5 * qlay1msec); // Jimmy 093 - To increase MDV Speed ! - 5 -> 31
		e++;
		if (0)
			fpr("IM ");
	}
#if 0
	if (emulator::cycle == emouse) { /* mouse interrupt, x msec */
		do_mouse_xm();
		emouse = emulator::cycle + 2 * qlay1msec;
		e++;
		if (0)
			fpr("IMo ");
	}
#endif
#if 0
	if (emulator::cycle == esound) {
		esound = emulator::cycle + sound_process(0);
		e++;
		if (0)
			fpr("IS ");
	}
#endif
	if (emulator::cycles() == etx) {
		do_tx();
		etx = emulator::cycles() - 1; /* nothing more */
		e++;
		if (0)
			fpr("ITX ");
	}

	/* decide who's next */
	eval_next_event();

	if (!e) {
		fpr("\nError NoE c: %d ", emulator::cycles());
		fpr("e: %d %d %d %d %d\n", e50, emdv, emouse, esound, etx);
	}
}

#if 0
/* if 'initbeep' first init BEEP parameters */
/* return number of ticks till next process; -1 for stop process */
static int sound_process(int initbeep)
{
	/* these are the SuperBasic equivalent values */
	static int dur, pitch1, pitch2, gradx, grady, wrap, random, fuzzy;
	static int dur_decr, cpitch, cpstep, cpdelay, us72;
	int rv;

	if (!IPCbeeping)
		return -1;
	if (initbeep) {
		pitch1 = BEEPpars[0] * 16 + BEEPpars[1];
		if (pitch1 == 0)
			pitch1 = 256;
		pitch2 = BEEPpars[2] * 16 + BEEPpars[3];
		if (pitch2 == 0)
			pitch2 = 256;
		gradx = ((BEEPpars[6] * 16 + BEEPpars[7]) * 16 + BEEPpars[4]) *
				16 +
			BEEPpars[5];
		dur = ((BEEPpars[10] * 16 + BEEPpars[11]) * 16 + BEEPpars[8]) *
			      16 +
		      BEEPpars[9];
		if (dur == 0)
			dur_decr = 0;
		else
			dur_decr = 1;
		grady = BEEPpars[12];
		if (grady > 7)
			grady = 16 - grady;
		wrap = BEEPpars[13];
		random = BEEPpars[14];
		fuzzy = BEEPpars[15];
		if (0)
			fpr("\nBP: D:%d P1:%d P2:%d GX:%d GY:%d W:%d R:%d F:%d \n",
			    dur, pitch1 - 1, pitch2 - 1, gradx, grady, wrap,
			    random, fuzzy);
		/* prepare ourselves */
		us72 = qlay1msec / 13; /* 72 microseconds */
		cpdelay = gradx;
		cpstep = grady;
		if (cpstep > 0)
			if (pitch1 > pitch2) {
				int a = pitch2;
				pitch2 = pitch1;
				pitch1 = a;
			}
		if (cpstep < 0)
			if (pitch2 > pitch1) {
				int a = pitch2;
				pitch2 = pitch1;
				pitch1 = a;
			}
		/* assert pitch steps from pitch1 towards pitch2 */
		cpitch = pitch1;
		/*		cpitch=pitch2;
		if (cpstep<0) cpitch=pitch1;
		if (pitch1>pitch2) if (cpstep<0) cpstep-=cpstep;
		if (pitch1<pitch2) if (cpstep>0) cpstep-=cpstep;
*/
		/* get me started */
		esound = emulator::cycle + 1; /* next emulator::cycle we will start */
		eval_next_event();
		return 1; /* return value is actually don't care now */
	}
	do_speaker(); /* blip */
	if (dur_decr) {
		if (dur <= 0) {
			IPCbeeping = 0;
			stop_speaker();
			return -1;
		}
	}
	rv = cpitch * us72;
	dur -= cpitch;
	cpdelay -= cpitch;
	if (cpdelay <= 0) {
		cpdelay = gradx;
		cpitch += cpstep;
	}
	if (cpstep > 0) {
		if (cpitch > pitch2) {
			cpitch = pitch2;
			cpstep = -cpstep;
			{
				int a = pitch2;
				pitch2 = pitch1;
				pitch1 = a;
			}
		}
	} else {
		if (cpstep < 0) {
			if (cpitch < pitch2) {
				cpitch = pitch2;
				cpstep = -cpstep;
				{
					int a = pitch2;
					pitch2 = pitch1;
					pitch1 = a;
				}
			}
		}
	}
	if (rv == 0) {
		fpr("ErrorSND ");
		rv = us72; /* we'll be back soon... */
	}
	return rv;
}
#endif

static uint8_t rdmouse(uint32_t a)
{
	/*fpr("RM%x ",a);*/
	if (a == 0x1bfbc) {
		return Mdirreg;
	}
	if (a == 0x1bf9c) {
		if (Mbutreg == 1)
			return 0x10;
		if (Mbutreg == 2)
			return 0x20;
		if (Mbutreg == 3)
			return 0x30; /*untested*/
	}
	if (a == 0x1bf9d) {
#if 0
		if (opt_use_mouse) {
			Mavail = 1; /* add other criteria...*/
			return 1; /* mouse detection */
		} else {
			return 0;
		}
#endif
	}
	if (a == 0x1bfbe) {
		/* ext int ackn 292 */
		return 0;
	}
	return 0;
}

static void wrmouse(uint32_t a, uint8_t d)
{
	fpr("WM%x %x ", a, d);
}

int irq_level()
{
	if (REG18021 & 0x1f)
		return 2;
	else
		return -1;
}

static void do_1Hz(void)
{
	static uint32_t qtime = 0, pcycle = 0;
	static int first = 1;
	uint32_t pctime;

	pctime = (uint32_t)time(0);
	if (qtime != pctime) {
		/* one real second has passed since last call */
		qtime = pctime;
		if (first) {
			first = 0;
		} else {
			fpr("%d ", emulator::cycles() - pcycle);
		}
		pcycle = emulator::cycles();
	}
}

static void do_mdv_motor()
{
	if (mdvmotor) {
		emulator::q68_pc_intr |= 0x01;
		emulator::doIrq = true;
		dt_event(1); /* set time stamp */
	}
	return;
}

static void do_tx()
{
	REG18020tx &= ~0x02; /* clear tx busy bit: byte is transmitted */
}

} // namespace ipc
