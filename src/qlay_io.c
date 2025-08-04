/*
	QLAY - Sinclair QL emulator
	Copyright Jan Venema 1998
	QL input and output
*/

#include "emulator_logging.h"
#include <SDL3/SDL.h>

#include <SDL3/SDL_log.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unistd.h>

#define DEBUG 0

#include "emulator_debug.h"
#include "emulator_files.h"
#include "emulator_options.h"
#include "emulator_hardware.h"
#include "emulator_mainloop.h"
#include "emulator_memory.h"
#include "m68k.h"
#include "qlay_hooks.h"
#include "qlay_keyboard.h"
#include "qlay_sound.h"
#include "utarray.h"
#include "utstring.h"

#ifndef  O_BINARY
#define  O_BINARY 0
#endif

uint32_t qlay1msec = 7500 / 16;

/* xternal? */
uint8_t qliord_b(uint32_t a);
uint32_t qliord_l(uint32_t a);
void qliowr_b(uint32_t a, uint8_t d);

/* internal */
void wr8049(uint8_t data);
static void exec_IPCcmd(int cmd);
static void wrmdvdata(uint8_t x);
void wrmdvcntl(uint8_t x);
static void wrserdata(uint8_t x);
void wrZX8302(uint8_t x);
static int decode_key(int key);
static void init_mdvs(void);
static int dt_event(int dt_type);
static void mdv_select(int drive);
static void init_events(void);
static void eval_next_event(void);
static void ser_rcv_init(void);
static int ser_rcv_dequeue(int ch);
static int ser_rcv_size(int ch);
static void save_mdv_file(int mdvnum);

static int IPC020 = 0; /* current return value for 18020 */
static int IPCreturn; /* internal 8049 value */
static int IPCcnt; /* send bit counter */
static int IPCwfc = 1; /* wait for command */
uint8_t REG18021 = 0; /* interrupt control/status register 18021 */
static int ser12oc = 0; /* ser1,2 open: bit0: SER1, bit1: SER2 */
bool qlayIPCBeeping = false; /* BEEP is sounding */
Uint8 BEEPpars[16]; /* IPC beep parameters */

#define SERBUFLEN 23
//uint8_t	IPCserbuf[2][SERBUFLEN];/* serial receive buffers from 8049 */
static int IPCsercnt = 0; /* number of bytes left */
//static	int	IPCsersptr=0;		/* send to CPU pointer */
static int IPCchan = 0; /* 0: SER1, 1: SER2 */
#define SER_RCV_LEN 2048
static uint8_t ser_rcv_buf[2][SER_RCV_LEN];
static int ser_rcv_1st[2];
static int ser_rcv_fill[2];

uint32_t e50, emdv, emouse, esound, etx; /* events */

static int ZXmode; /* ZX8302 mode; sertx,net,mdv: bit4,5 of 18002 */
static int ZXbaud; /* ZX8302 sertx baudrate */
static int REG18020tx; /* ZX8302 sertx read register */
static uint32_t qlclkoff; /* QL hardware clock offset */

#define MDV_NOSECTS 255
#define MDV_NUMOFDRIVES 8
#define MDV_GAP_COUNT 80
#define MDV_PREAMBLE_COUNT 12
#define MDV_DATA_PREAMBLE_COUNT 8
#define MDV_INTERSECTOR_COUNT 120

#define MDV_PREAMBLE_SIZE (12)
#define MDV_HDR_CONTENT_SIZE (16)
#define MDV_HDR_SIZE (MDV_PREAMBLE_SIZE + MDV_HDR_CONTENT_SIZE)
#define MDV_DATA_PREAMBLE_SIZE (8)
#define MDV_DATA_HDR_SIZE (4)
#define MDV_DATA_CONTENT_SIZE (514)
#define MDV_DATA_SPARE_SIZE (120)
#define MDV_DATA_SIZE                                                     \
	(MDV_PREAMBLE_SIZE + MDV_DATA_HDR_SIZE + MDV_DATA_PREAMBLE_SIZE + \
	 MDV_DATA_CONTENT_SIZE + MDV_DATA_SPARE_SIZE)
#define MDV_SECTLEN (MDV_HDR_SIZE + MDV_DATA_SIZE)

#define QLAY_MDV_SIZE 174930
#define QLAY_MDV_NOSECTS 255
#define QLAY_MDV_PREAMBLE_SIZE (12)
#define QLAY_MDV_HDR_CONTENT_SIZE (16)
#define QLAY_MDV_HDR_SIZE (QLAY_MDV_PREAMBLE_SIZE + QLAY_MDV_HDR_CONTENT_SIZE)
#define QLAY_MDV_DATA_PREAMBLE_SIZE (8)
#define QLAY_MDV_DATA_HDR_SIZE (4)
#define QLAY_MDV_DATA_CONTENT_SIZE (514)
#define QLAY_MDV_DATA_SPARE_SIZE (120)
#define QLAY_MDV_DATA_SIZE                                          \
	(QLAY_MDV_PREAMBLE_SIZE + QLAY_MDV_DATA_HDR_SIZE +          \
	 QLAY_MDV_DATA_PREAMBLE_SIZE + QLAY_MDV_DATA_CONTENT_SIZE + \
	 QLAY_MDV_DATA_SPARE_SIZE)
#define QLAY_MDV_SECTLEN (QLAY_MDV_HDR_SIZE + QLAY_MDV_DATA_SIZE)

#define MDI_MDV_SIZE 136170
#define MDI_MDV_NOSECTS 255
#define MDI_MDV_HDR_CONTENT_SIZE 16
#define MDI_MDV_DATA_HDR_SIZE 4
#define MDI_MDV_DATA_CONTENT_SIZE 514
#define MDI_MDV_SECTLEN                                     \
	(MDI_MDV_HDR_CONTENT_SIZE + MDI_MDV_DATA_HDR_SIZE + \
	 MDI_MDV_DATA_CONTENT_SIZE)

typedef enum QLAY_ {
	MDV_GAP1,
	MDV_PREAMBLE1,
	MDV_HDR,
	MDV_GAP2,
	MDV_PREAMBLE2,
	MDV_DATA_HDR,
	MDV_DATA_PREAMBLE,
	MDV_DATA,
	MDV_INTERSECTOR
} mdvstate;

typedef enum {
	MDV_FORMAT_QLAY,
	MDV_FORMAT_MDUMP1,
	MDV_FORMAT_MDUMP2,
	MDV_FORMAT_MDI
} mdvtype;

struct mdvsector {
	uint8_t header[16];
	uint8_t blockheader[4];
	uint8_t block[514];
};

struct mdvt {
	const char *name; /* file name */
	mdvtype type; /* file type */
	bool present; /* is it in? */
	bool wrprot; /* write protected */
	bool mdvwritten; /* has been written to */
	int no_sectors; /* number of sectors in file */
	int sector; /* current sector */
	int idx; /* current index into file */
	mdvstate mdvstate; /* which phase are we in */
	int mdvgapcnt; /* where are we in gap */
	struct mdvsector *data; /* pointer to data */
};

struct mdvt mdrive[MDV_NUMOFDRIVES];

const uint8_t preamble[12] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			       0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF };
const uint8_t data_preamble[8] = { 0x00, 0x00, 0x00, 0x00,
				   0x00, 0x00, 0xFF, 0xFF };

const char *mdvname; /* mdv file name */
int mdvnum; /* current mdv */
int mdvpresent; /* mdv in drive? */
int mdvwp; /* mdv write protected */
int mdvixb; /* mdv byte index */
int mdvixs; /* mdv sector index */
static bool mdvwrite = false; /* mdv r/w */
static bool mdvmotor = false; /* mdv motor running */
int mdvghstate; /* mdv g/h/g/s progress indicator */
int mdvdoub2; /* 2 2 mdv command */
int mdvwra; /* mdv write actions */
int mdvgap = 1;
int mdvgapcnt = 70;
int mdvrd = 0;
int mdvcuridx;
int mdvsectidx;
static bool mdvtxfl = false;
static bool mdverase = false;
static uint8_t PC_TRAK = 0;
static uint8_t PC_TDATA = 0;

int fpr(const char *fmt, ...)
{
	int res;
	va_list l;

	va_start(l, fmt);
	res = vfprintf(stderr, fmt, l);
	va_end(l);
	return res;
}

uint8_t readQLHw(uint32_t addr)
{
	//printf("addr: %08x\n", addr);

	if (addr == 0x18020) {
		if (IPC020 != 0) {
			int t = IPC020;
			IPC020 >>= 8;
			if (IPC020 == 0xa5) {
				IPC020 = 0; /* clear end marker */
			}

			return (uint8_t)t & 0xff;
		}

		if ((ZXmode & 0x10) == 0) { /* TXSER mode */
			return REG18020tx;
		}

		if (!mdvmotor) { /* no motor running, return NOP */
			return 0x0;
		} else { /* return MDV responses */
			//if (!mdvpresent) {
			//	printf("No PRESENT\n");
			//	return 0x08;
			//}

			if (!mdvwrite) {
				if (mdvgap) {
					//printf("G\n");
					return PC__GAP;
				}

				if (mdvrd) {
					//printf("R\n");
					return PC__RXRD;
				}

				return 0x00;
			} else if (mdvtxfl) {
				return PC__TXFL;
			}
		}
	}

	if ((addr == PC_TRAK1) || (addr == PC_TRAK2)) {
		mdvrd = 0;
		return PC_TRAK;
	}

	return 0;
}

void writeMdvSer(uint8_t data)
{
	if ((ZXmode & 0x18) == 0x10) {
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
	mdvtxfl = true;
	mdrive[mdvnum].mdvwritten = true;

	PC_TDATA = x;

	// Unfortunately JS and Minerva use different timing
	// constants so a hack here to move state if we get
	// a write during gap2
	if (mdrive[mdvnum].mdvstate == MDV_GAP2) {
		mdrive[mdvnum].mdvstate = MDV_PREAMBLE2;
		mdrive[mdvnum].mdvgapcnt = MDV_PREAMBLE_COUNT;
		mdrive[mdvnum].idx = MDV_PREAMBLE_SIZE + MDV_HDR_SIZE;
	}
}

static uint8_t mdvselect = 0;
static bool mdvselbit = false;

void wrmdvcntl(uint8_t x)
{
	if (x & PC__WRITE) {
		SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "MDV Write On");
		mdvwrite = true;

		// Unfortunately JS and Minerva use different timing
		// constants so a hack here to move state if we get
		// a write during gap2
		if (mdvmotor && (mdrive[mdvnum].mdvstate == MDV_GAP2)) {
			//mdrive[mdvnum].mdvstate = MDV_PREAMBLE2;
			//mdrive[mdvnum].mdvgapcnt = MDV_PREAMBLE_COUNT;
			//mdrive[mdvnum].idx = MDV_PREAMBLE_SIZE + MDV_HDR_SIZE;
			mdrive[mdvnum].mdvgapcnt = 1;
		}
	} else {
		SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "MDV Write Off");
		mdvwrite = false;
	}

	if ((x & PC__ERASE) && !mdverase) {
		SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "MDV Erase On");
		mdverase = true;
	}

	if (!(x & PC__ERASE) && mdverase) {
		SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "MDV Erase Off");
		mdverase = false;
	}

	// clock bit on 1->0 transition
	if (mdvselbit && !(x & PC__SCLK)) {
		mdvselect <<= 1;

		if (x & PC__SEL) {
			mdvselect |= BIT(0);
		} else {
			mdvselect &= ~BIT(0);
		}

		if (!mdvselect) {
			mdv_select(0);
		} else {
			int mdv;

			for (mdv = 0; mdv < 8; mdv++) {
				if (BIT(mdv) & mdvselect) {
					break;
				}
			}
			mdv_select(mdv + 1);
		}
	}

	// store clock bit for next cycle
	if (x & PC__SCLK) {
		mdvselbit = true;
	} else {
		mdvselbit = false;
	}
}

static void set_gap_irq(void);

/* in: drive; 0: no drive, 1..8: select MDVdrive */
static void mdv_select(int drive)
{
	mdvixb = 0;
	mdvwrite = false;
	mdvghstate = 0;
	mdvdoub2 = 0;
	mdvwra = 0;

	if (drive == 0) {
		SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "MDV MOTOR OFF");
		for (int i = 0; i < 8; i++) {
			if (mdrive[i].mdvwritten) {
				// write MDV back to disk
				save_mdv_file(i);
				mdrive[i].mdvwritten = false;
			}
		}
		mdvnum = -1;
		mdvname = NULL;
		mdvpresent = 0;
		mdvixs = 0;
		mdvwp = 0; /* 1 ->just in case - 0 by Jimmy */
		mdvmotor = false;
		mdvtxfl = false;

		qlayStopMdvSound();
	} else {
		SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "MDV MOTOR ON %d",
			     drive);
		mdvnum = drive - 1;
		mdvname = mdrive[mdvnum].name;
		mdvpresent = mdrive[mdvnum].present;
		mdvixs = mdrive[mdvnum].sector;
		mdvwp = mdrive[mdvnum].wrprot;
		mdvtxfl = false;
		mdvmotor = true;
		mdrive[mdvnum].mdvstate = MDV_GAP1;
		mdrive[mdvnum].mdvgapcnt = MDV_GAP_COUNT;
		set_gap_irq();

		qlayStartMdvSound();
	}
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
	__attribute__((unused)) int ch;
	int p = 0;

	ch = 0;
	if (ZXmode == 0x8)
		ch = 1;
	//send_serial_char(ch, d);
	if (p)
		fpr("ST%02x(%c) ", d, d);
	if (p)
		fpr("ST%ld ", cycles());
	REG18020tx |= 0x02; /* set busy */
	etx = cycles() +
	      10000 * qlay1msec /
		      ZXbaud; /* clear busy after 10 bits transmitted */
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

void wr8049(Uint8 data)
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
			SDL_LogDebug(QLAY_LOG_IPC, "ERRORIPC?:%x %x",
				     m68k_get_reg(NULL, M68K_REG_PC), data);
		}
		IPC020 = 0;
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
		SDL_LogDebug(QLAY_LOG_IPC, "BRC: %d", cmd);
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
		SDL_LogDebug(QLAY_LOG_IPC, "BR: %d", IPCbaud);
		IPCwfc = 1;
		cmd = 0x10;
	}

	if (IPCpcmd == 0x09) { /*keyrow*/
		int row;
		row = cmd;
		IPCreturn = qlayGetKeyrow(row);
		IPCcnt = 8;
		cmd = 0x10;
	}

	if (IPCpcmd == 0x0a) { /*sound*/
		static int params = 0;
		BEEPpars[params] = cmd;
		SDL_LogDebug(QLAY_LOG_IPC, "B %d:%x", params, cmd);
		params++;
		if (params > 15) {
			IPCpcmd = 0x10;
			params = 0;
			qlayIPCBeepSound(BEEPpars);
		}
		IPCwfc = 1;
		return;
	}

	/*082d ipc cmd C has 1 nibble parameter, no reply */
	if (IPCpcmd == 0x0c) {
		SDL_LogDebug(QLAY_LOG_IPC, "IPCc'C'par%d", cmd);
		IPCwfc = 1;
		IPCpcmd = 0x10;
		return;
	}

	if (IPCpcmd == 0x0f) { /*test*/
		static int params = 0;
		static int testval = 0;
		params++;
		SDL_LogDebug(QLAY_LOG_IPC, "TP%d:%x", params, cmd);
		testval = 16 * testval + cmd;
		if (params > 1) {
			SDL_LogDebug(QLAY_LOG_IPC, "RTV%02x", testval);
			IPCpcmd = 0x10;
			params = 0;
			IPCreturn = testval;
			testval = 0; /* for next time 'round */
			IPCcnt = 8;
			cmd = 0x10;
			IPCwfc = 1;
		} else {
			IPCwfc = 1;
			return;
		}
	}

	switch (cmd) {
	case 0: /* init */
		SDL_LogDebug(QLAY_LOG_IPC, "IPC%02x", cmd);
		break;
	case 1: /* get interrupt status */
		SDL_LogDebug(QLAY_LOG_IPC, "Interrupt Status");
		IPCreturn = 0;
		if (utarray_len(qlayKeyBuffer) || qlayKeysPressed) {
			IPCreturn |= 0x01;
		}
		if (qlayIPCBeeping)
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
			SDL_LogDebug(QLAY_LOG_IPC, "Open Ser 1, ZXb: %d",
				     ZXbaud);
			//open_serial(0, ZXbaud, 0);
		}
		if (cmd == 3) {
			ser12oc |= 2;
			SDL_LogDebug(QLAY_LOG_IPC, "Open Ser 2, ZXb: %d",
				     ZXbaud);
			//open_serial(1, ZXbaud, 0);
		}
		if (cmd == 4) {
			ser12oc &= 0xfe;
			SDL_LogDebug(QLAY_LOG_IPC, "Close Ser 1");
			//close_serial(0);
		}
		if (cmd == 5) {
			ser12oc &= 0xfd;
			SDL_LogDebug(QLAY_LOG_IPC, "Close Ser 2");
			//close_serial(1);
		}
		SDL_LogDebug(QLAY_LOG_IPC, "IPC %02x, SER12OC %02x", cmd,
			     ser12oc);
		IPCwfc = 1;
		break;
	case 6: /* SER1 rcv */
	case 7: /* SER2 rcv */
		/* only called when 8049 sent a SER RCV interrupt */
		SDL_LogDebug(QLAY_LOG_IPC, "IPCrcv%02x", cmd);
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
		SDL_LogDebug(QLAY_LOG_IPC, "C8");
		IPCreturn = 0;
		IPCcnt = 4;
		if (utarray_len(qlayKeyBuffer)) { /* just double check */
			int *key;
			key = (int *)utarray_front(qlayKeyBuffer);
			utarray_erase(qlayKeyBuffer, 0, 1);
			IPCreturn = decode_key(*key);
			IPCcnt = 16;
		} else {
			if (qlayKeysPressed) { /* still pressed: autorepeat */
				IPCreturn = 0x8; /* just the repeat bit */
				IPCcnt = 4;
			}
		}
		break;
	}
	case 0x9: /* keyrow */
		SDL_LogDebug(QLAY_LOG_IPC, "KEYR");
		IPCwfc = 1;
		break;
	case 0xa: /* set sound */
		SDL_LogDebug(QLAY_LOG_IPC, "BEEP");
		IPCwfc = 1;
		break;
	case 0xb: /* kill sound */
		SDL_LogDebug(QLAY_LOG_IPC, "KILLBEEP");
		qlayIPCKillSound();
		IPCwfc = 1;
		break;
	case 0xc: /* test interrupt 2?? */
		SDL_LogDebug(QLAY_LOG_IPC, "IPCcINT");
		IPCwfc = 1;
		break;
	case 0xd: /* set baud rate */
		SDL_LogDebug(QLAY_LOG_IPC, "BRATE");
		IPCwfc = 1;
		break;
	case 0xe: /* get random, return 16 bits */
		IPCreturn = SDL_rand(65535);
		IPCcnt = 16;
		SDL_LogDebug(QLAY_LOG_IPC, "IPCrnd %04x", IPCreturn);
		IPCwfc = 1;
		break;
	case 0xf: /* test, return received byte */
		SDL_LogDebug(QLAY_LOG_IPC, "IPCtest");
		IPCwfc = 1;
		break;
	case 0x10: /* fake: no more parameters */
		break;
	default:
		SDL_LogDebug(QLAY_LOG_IPC, "ERRORunexpIPCcmd:%x", cmd);
		IPCreturn = 0;
		IPCcnt = 4;
		break;
	}
	IPCpcmd = cmd;
}

/* return how many cycles past previous dt_event() call */
/* dt_type=0 will read only since last update; 1 will update time */
static int dt_event(int dt_type)
{
	static uint32_t prevevent = 0x80000000;
	uint32_t rv;
	/* this is wrong?? see QABS 970801 */
	if (cycles() < prevevent)
		rv = prevevent - cycles();
	else
		rv = cycles() - prevevent;
	/*fpr("DT%d ",rv);*/
	if (dt_type)
		prevevent = cycles();
	return rv;
}

void qlayInitIPC(void)
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
	cycleNextEvent = 0;
}

/* load a qlay formatted MDV
 */
static bool load_qlay_mdv_file(int fd, int mdvnum)
{
	mdrive[mdvnum].no_sectors = QLAY_MDV_NOSECTS;
	mdrive[mdvnum].data =
		malloc(sizeof(struct mdvsector) * QLAY_MDV_NOSECTS);
	mdrive[mdvnum].type = MDV_FORMAT_QLAY;

	/* malloc failed for some reason */
	if (mdrive[mdvnum].data == NULL) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
			     "malloc failed %s %d", __FILE__, __LINE__);
		return false;
	}

	struct mdvsector *cursect = mdrive[mdvnum].data;

	for (int i = 0; i < QLAY_MDV_NOSECTS; i++) {
		lseek(fd, i * QLAY_MDV_SECTLEN, SEEK_SET);

		/* skip pre-amble */
		lseek(fd, QLAY_MDV_PREAMBLE_SIZE, SEEK_CUR);
		read(fd, cursect->header, QLAY_MDV_HDR_CONTENT_SIZE);

		/* skip the next pre-amble */
		lseek(fd, QLAY_MDV_PREAMBLE_SIZE, SEEK_CUR);
		read(fd, cursect->blockheader, QLAY_MDV_DATA_HDR_SIZE);

		/* skip the next pre-amble */
		lseek(fd, QLAY_MDV_DATA_PREAMBLE_SIZE, SEEK_CUR);
		read(fd, cursect->block, QLAY_MDV_DATA_CONTENT_SIZE);

		cursect++;
	}

	return true;
}

/* load a MDI formatted MDV
 */
static bool load_mdi_mdv_file(int fd, int mdvnum)
{
	mdrive[mdvnum].no_sectors = MDI_MDV_NOSECTS;
	mdrive[mdvnum].data =
		malloc(sizeof(struct mdvsector) * MDI_MDV_NOSECTS);
	mdrive[mdvnum].type = MDV_FORMAT_MDI;

	/* malloc failed for some reason */
	if (mdrive[mdvnum].data == NULL) {
		SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
			     "malloc failed %s %d", __FILE__, __LINE__);
		return false;
	}

	struct mdvsector *cursect = mdrive[mdvnum].data;

	for (int i = 0; i < MDI_MDV_NOSECTS; i++) {
		lseek(fd, i * MDI_MDV_SECTLEN, SEEK_SET);

		read(fd, cursect->header, MDI_MDV_HDR_CONTENT_SIZE);

		read(fd, cursect->blockheader, MDI_MDV_DATA_HDR_SIZE);

		read(fd, cursect->block, MDI_MDV_DATA_CONTENT_SIZE);

		cursect++;
	}

	return true;
}

static bool load_mdv_file(const char *filename, int mdvnum)
{
	struct stat stat;
	int fd;
	bool res = false;

	fd = open(filename, O_RDONLY | O_BINARY);

	fstat(fd, &stat);

	if (stat.st_size == QLAY_MDV_SIZE) {
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%s is a QLAY file",
			    filename);
		res = load_qlay_mdv_file(fd, mdvnum);
	} else if (stat.st_size == MDI_MDV_SIZE) {
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%s is a MDI file",
			    filename);
		res = load_mdi_mdv_file(fd, mdvnum);
	}

	close(fd);

	if (res == false) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
			     "Failed to load file %s", filename);
		return false;
	}

	return true;
}

static bool save_qlay_mdv_file(int fd, int mdvnum)
{
	struct mdvsector *cursect = mdrive[mdvnum].data;

	for (int i = 0; i < QLAY_MDV_NOSECTS; i++) {
		lseek(fd, i * QLAY_MDV_SECTLEN, SEEK_SET);

		/* skip pre-amble */
		lseek(fd, QLAY_MDV_PREAMBLE_SIZE, SEEK_CUR);
		write(fd, cursect->header, QLAY_MDV_HDR_CONTENT_SIZE);

		/* skip the next pre-amble */
		lseek(fd, QLAY_MDV_PREAMBLE_SIZE, SEEK_CUR);
		write(fd, cursect->blockheader, QLAY_MDV_DATA_HDR_SIZE);

		/* skip the next pre-amble */
		lseek(fd, QLAY_MDV_DATA_PREAMBLE_SIZE, SEEK_CUR);
		write(fd, cursect->block, QLAY_MDV_DATA_CONTENT_SIZE);

		cursect++;
	}

	return true;
}

static bool save_mdi_mdv_file(int fd, int mdvnum)
{
	struct mdvsector *cursect = mdrive[mdvnum].data;

	for (int i = 0; i < MDI_MDV_NOSECTS; i++) {
		lseek(fd, i * MDI_MDV_SECTLEN, SEEK_SET);

		write(fd, cursect->header, MDI_MDV_HDR_CONTENT_SIZE);

		write(fd, cursect->blockheader, MDI_MDV_DATA_HDR_SIZE);

		write(fd, cursect->block, MDI_MDV_DATA_CONTENT_SIZE);

		cursect++;
	}

	return true;
}

static void save_mdv_file(int mdvnum)
{
	int fd;
	bool res = false;

	SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Saving: %s",
		    mdrive[mdvnum].name);

	if (mdrive[mdvnum].name == NULL) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "MDV%d name (NULL)",
			     mdvnum + 1);
		return;
	}

	if (mdrive[mdvnum].wrprot) {
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
			    "MDV%d write protected NOT saving", mdvnum + 1);
		return;
	}

	SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "MDV Saving %s %d",
		     mdrive[mdvnum].name, mdvnum);

	fd = open(mdrive[mdvnum].name, O_WRONLY | O_CREAT | O_BINARY);
	if (fd < 0) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
			     "opening MDV for write %s", strerror(errno));
		return;
	}

	switch (mdrive[mdvnum].type) {
	case MDV_FORMAT_QLAY:
		res = save_qlay_mdv_file(fd, mdvnum);
		break;
	case MDV_FORMAT_MDI:
		res = save_mdi_mdv_file(fd, mdvnum);
		break;
	default:
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
			     "Unknown mdv format MDV%d\n", mdvnum + 1);
	}

	close(fd);

	if (res == false) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
			     "Failed to save file %s", mdrive[mdvnum].name);
	}
}

void init_mdvs(void)
{
	int i = 0;
	int noDrives = emulatorOptionDevCount("drive");
	bool res;

	qlayInitMdvSound();

	memset(mdrive, 0, sizeof(mdrive));

	for (i = 0; i < noDrives; i++) {
		const char *drive = emulatorOptionDev("drive", i);
		const char *mdvName;
		bool wrprot = false;

		int mdvNum;
		if (strncmp(drive, "R:", 2) == 0) {
			wrprot = true;
			drive += 2;
		}

		if (!(strncmp(drive, "mdv", 3) == 0) || !isdigit(drive[3]) ||
		    (drive[4] != '@')) {
			continue;
		}

		mdvNum = drive[3] - '1';
		mdvName = drive + 5;

		if (mdvNum >= MDV_NUMOFDRIVES) {
			fprintf(stderr, "Invalid MDV num %d\n", mdvNum);
			continue;
		}

		mdrive[mdvNum].present = 0;
		mdrive[mdvNum].sector = 0;
		mdrive[mdvNum].wrprot = wrprot;
		mdrive[mdvNum].idx = 0;
		mdrive[mdvNum].mdvstate = MDV_GAP1;
		mdrive[mdvNum].mdvgapcnt = MDV_GAP_COUNT;

		res = load_mdv_file(mdvName, mdvNum);
		if (res) {
			mdrive[mdvNum].name = mdvName;
			mdrive[mdvNum].present = 1;
			mdrive[mdvNum].sector = 0;
			mdvpresent = 1;
			mdvname = mdvName;
			SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
				    "MDV%01d is %s", mdvNum + 1, mdvname);
		} else {
			SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
				    "MDV%01d is ejected", mdvNum + 1);
		}
	}
}

void init_events(void)
{
	e50 = emdv = emouse = esound = etx = 0;
}

void eval_next_event(void)
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
	dmin = e50 - cycles();
	dc = emdv - cycles();
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
	//	dc = etx - cycles();
	//	if (dc < dmin)
	//		dmin = dc;
	cycleNextEvent = cycles() + dmin;
}

static void set_gap_irq(void)
{
	EMU_PC_INTR |= PC_INTRG;

	/* dont actually trigger irq if masked */
	if (!(EMU_PC_INTR_MASK & PC_MASKG)) {
		return;
	}

	m68k_set_irq(2); /* set MDV interrupt */
}

void do_mdv_tick(void)
{
	int sector, idx;
	bool wrprot;

	if (mdvmotor) {
		if (mdrive[mdvnum].present == 0) {
			set_gap_irq();
			mdvgap = 1;
			return;
		}

		// A number of states need this so do here
		if (mdvwrite) {
			mdvtxfl = false;
		}

		sector = mdrive[mdvnum].sector;
		idx = mdrive[mdvnum].idx;
		wrprot = mdrive[mdvnum].wrprot;

		switch (mdrive[mdvnum].mdvstate) {
		case MDV_GAP1:
			if (mdrive[mdvnum].mdvgapcnt == MDV_GAP_COUNT) {
				SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
					     "GAP1");
			}
			mdvgap = 1;
			mdvrd = 0;
			set_gap_irq();
			mdrive[mdvnum].mdvgapcnt--;

			if (mdrive[mdvnum].mdvgapcnt == 0) {
				mdrive[mdvnum].mdvstate = MDV_PREAMBLE1;
				mdrive[mdvnum].mdvgapcnt = MDV_PREAMBLE_COUNT;
			}
			break;
		case MDV_PREAMBLE1:
			if (mdrive[mdvnum].mdvgapcnt == MDV_PREAMBLE_COUNT) {
				SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
					     "PREAMBLE1");
			}
			mdvgap = 0;
			mdvrd = 0;
			mdrive[mdvnum].mdvgapcnt--;

			if (mdrive[mdvnum].mdvgapcnt == 0) {
				mdrive[mdvnum].mdvstate = MDV_HDR;
				mdrive[mdvnum].idx = 0;
			}
			break;
		case MDV_HDR:
			if (mdrive[mdvnum].idx == 0) {
				SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
					     "HDR");
			}
			mdvgap = 0;
			mdvrd = 1;

			PC_TRAK = mdrive[mdvnum].data[sector].header[idx];

			mdrive[mdvnum].idx++;

			if (mdrive[mdvnum].idx == MDV_HDR_CONTENT_SIZE) {
				mdrive[mdvnum].mdvstate = MDV_GAP2;
				mdrive[mdvnum].mdvgapcnt = MDV_GAP_COUNT;
			}
			break;
		case MDV_GAP2:
			if (mdrive[mdvnum].mdvgapcnt == MDV_GAP_COUNT) {
				SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
					     "GAP2");
			}
			mdvgap = 1;
			mdvrd = 0;

			// If erase is on, then GAP2 lasts until
			// write is also on
			if (!mdverase) {
				mdrive[mdvnum].mdvgapcnt--;
			}

			if (mdrive[mdvnum].mdvgapcnt == 0) {
				mdrive[mdvnum].mdvstate = MDV_PREAMBLE2;
				mdrive[mdvnum].mdvgapcnt = MDV_PREAMBLE_COUNT;
				mdrive[mdvnum].idx =
					MDV_PREAMBLE_SIZE + MDV_HDR_SIZE;
			}
			break;
		case MDV_PREAMBLE2:
			if (mdrive[mdvnum].mdvgapcnt == MDV_PREAMBLE_COUNT) {
				SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
					     "PREAMBLE2");
			}
			mdvgap = 0;
			mdvrd = 0;
			mdrive[mdvnum].mdvgapcnt--;

			if (mdrive[mdvnum].mdvgapcnt == 0) {
				mdrive[mdvnum].mdvstate = MDV_DATA_HDR;
				mdrive[mdvnum].idx = 0;
			}
			break;
		case MDV_DATA_HDR:
			if (mdrive[mdvnum].idx == 0) {
				SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
					     "DATA_HDR");
			}
			mdvgap = 0;

			if (mdvwrite && !wrprot) {
				mdrive[mdvnum].data[sector].blockheader[idx] =
					PC_TDATA;
			} else {
				mdvrd = 1;
				PC_TRAK = mdrive[mdvnum]
						  .data[sector]
						  .blockheader[idx];
			}

			mdrive[mdvnum].idx++;

			if (mdrive[mdvnum].idx == MDV_DATA_HDR_SIZE) {
				mdrive[mdvnum].mdvstate = MDV_DATA_PREAMBLE;
				mdrive[mdvnum].mdvgapcnt =
					MDV_DATA_PREAMBLE_COUNT;
			}
			break;
		case MDV_DATA_PREAMBLE:
			if (mdrive[mdvnum].mdvgapcnt ==
			    MDV_DATA_PREAMBLE_COUNT) {
				SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
					     "DATA_PREAMBLE");
			}
			mdvgap = 0;
			mdvrd = 0;
			mdrive[mdvnum].mdvgapcnt--;

			if (mdrive[mdvnum].mdvgapcnt == 0) {
				mdrive[mdvnum].mdvstate = MDV_DATA;
				mdrive[mdvnum].idx = 0;
			}
			break;
		case MDV_DATA:
			if (mdrive[mdvnum].idx == 0) {
				SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
					     "DATA");
			}
			mdvgap = 0;

			if (mdvwrite && !wrprot) {
				mdrive[mdvnum].data[sector].block[idx] =
					PC_TDATA;
			} else {
				mdvrd = 1;
				PC_TRAK =
					mdrive[mdvnum].data[sector].block[idx];
			}

			mdrive[mdvnum].idx++;

			if (mdrive[mdvnum].idx == MDV_DATA_CONTENT_SIZE) {
				mdrive[mdvnum].mdvstate = MDV_INTERSECTOR;
				mdrive[mdvnum].mdvgapcnt =
					MDV_INTERSECTOR_COUNT;
			}
			break;
		case MDV_INTERSECTOR:
			if (mdrive[mdvnum].idx == 0) {
				SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
					     "INTERSECTOR");
			}

			if (!mdvwrite) {
				mdvrd = 1;
				PC_TRAK = 0x5A;
			}

			mdrive[mdvnum].mdvgapcnt--;

			if (mdrive[mdvnum].mdvgapcnt == 0) {
				mdrive[mdvnum].mdvstate = MDV_GAP1;
				mdrive[mdvnum].mdvgapcnt = MDV_GAP_COUNT;
				mdrive[mdvnum].idx = 0;
				mdrive[mdvnum].sector++;
				mdrive[mdvnum].sector %=
					mdrive[mdvnum].no_sectors;
			}
			break;
		default:
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
				     "MDV in unknown state");
			break;
		}
	} else {
		mdvgap = 0;
	}
}
