/*
	QLAY - Sinclair QL emulator
	Copyright Jan Venema 1998
	QL input and output
*/

#include <stdint.h>
#include <stdio.h>

#define DEBUG 1

#include "emulator_debug.h"
#include "emulator_files.h"
#include "emulator_options.h"
#include "emulator_hardware.h"
#include "emulator_mainloop.h"
#include "emulator_memory.h"
#include "m68k.h"
#include "qlay_hooks.h"
#include "qlay_keyboard.h"
#include "utarray.h"

uint32_t qlay1msec = 7500 / 16;

/* xternal? */
uint8_t qliord_b(uint32_t a);
uint32_t qliord_l(uint32_t a);
void qliowr_b(uint32_t a, uint8_t d);
void QL_exit(void);

/* internal */
static void do_mdv_motor(void);

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
static void do_tx(void);
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

uint32_t e50, emdv, emouse, esound, etx; /* events */

static int ZXmode; /* ZX8302 mode; sertx,net,mdv: bit4,5 of 18002 */
static int ZXbaud; /* ZX8302 sertx baudrate */
static int REG18020tx; /* ZX8302 sertx read register */
static uint32_t qlclkoff; /* QL hardware clock offset */

#define MDV_NOSECTS 255
#define MDV_NUMOFDRIVES 8
#define MDV_GAP_COUNT 70
#define MDV_PREAMBLE_COUNT 10
#define MDV_DATA_PREAMBLE_COUNT 8
#define MDV_PREAMBLE_SIZE (12)
#define MDV_HDR_CONTECT_SIZE (16)
#define MDV_HDR_SIZE (MDV_PREAMBLE_SIZE + MDV_HDR_CONTECT_SIZE)
#define MDV_DATA_PREAMBLE_SIZE (8)
#define MDV_DATA_HDR_SIZE (4)
#define MDV_DATA_CONTENT_SIZE (514)
#define MDV_DATA_SPARE_SIZE (120)
#define MDV_DATA_SIZE                                                     \
	(MDV_PREAMBLE_SIZE + MDV_DATA_HDR_SIZE + MDV_DATA_PREAMBLE_SIZE + \
	 MDV_DATA_CONTENT_SIZE + MDV_DATA_SPARE_SIZE)
#define MDV_SECTLEN (MDV_HDR_SIZE + MDV_DATA_SIZE)

typedef enum {
	MDV_GAP1,
	MDV_PREAMBLE1,
	MDV_HDR,
	MDV_GAP2,
	MDV_PREAMBLE2,
	MDV_DATA_HDR,
	MDV_DATA_PREAMBLE,
	MDV_DATA
} mdvstate;

struct mdvt {
	const char *name; /* file name */
	bool present; /* is it in? */
	bool wrprot; /* write protected */
	int sector; /* current sector */
	int idx;
	mdvstate mdvstate;
	int mdvgapcnt;
	uint8_t data[MDV_NOSECTS * MDV_SECTLEN]; /* mdv drive data */
};

struct mdvt mdrive[MDV_NUMOFDRIVES];

uint8_t *mdv; /* mdv drive data */
const char *mdvname; /* mdv file name */
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
int mdvgap = 1;
int mdvgapcnt = 70;
int mdvrd = 0;
int mdvcuridx;
int mdvsectidx;
bool mdvtxfl = false;
int PC_TRAK = 0;

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

		if (mdvmotor == 0) { /* no motor running, return NOP */
			return 0x0;
		} else { /* return MDV responses */
			//if (!mdvpresent) {
			//	printf("No PRESENT\n");
			//	return 0x08;
			//}

			if (mdvwrite == 0) {
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
	debug_print("%x\n", x);
	mdvtxfl = true;

	if (!mdvpresent)
		return; /* is this necessary ? */
	if (mdvwp)
		return;
	/*	if(mdvixb<0x40)fpr("M%02x%02x ",mdvixs,mdvixb);*/
}

static uint8_t mdvselect = 0;
static bool mdvselbit = false;

int uint8_log2(uint64_t n)
{
#define S(k)                          \
	if (n >= (UINT8_C(1) << k)) { \
		i += k;               \
		n >>= k;              \
	}

	int i = -(n == 0);
	S(4);
	S(2);
	S(1);
	return i;

#undef S
}

void wrmdvcntl(uint8_t x)
{
	debug_print("%02x %d\n", x, mdvselect);

	if (x & PC__WRITE) {
		mdvwrite = 1;
	} else {
		mdvwrite = 0;
	}

	// clock bit on 1->0 transition
	if (mdvselbit && !(x & PC__SCLK)) {
		mdvselect <<= 1;

		if (x & PC__SEL) {
			mdvselect |= BIT(0);
		} else {
			mdvselect &= ~BIT(0);
		}
	}

	// store clock bit for next cycle
	if (x & PC__SCLK) {
		mdvselbit = true;
	} else {
		mdvselbit = false;
	}

	if (!mdvselect) {
		mdv_select(0);
	} else {
		int mdv = uint8_log2(mdvselect);
		debug_print("MDV Select %d\n", mdv + 1);
		mdv_select(mdv + 1);
	}
}

static void set_gap_irq(void);

/* in: drive; 0: no drive, 1..8: select MDVdrive */
static void mdv_select(int drive)
{
	mdvnum = drive - 1;
	mdvixb = 0;
	mdvwrite = 0;
	mdvghstate = 0;
	mdvdoub2 = 0;
	mdvwra = 0;

	if (drive == 0) {
		mdvname = NULL;
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
		set_gap_irq();
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

void wr8049(uint8_t data)
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
			fpr("ERRORIPC?:%x %x ", m68k_get_reg(NULL, M68K_REG_PC),
			    data);
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
		IPCreturn = qlayGetKeyrow(row);
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
		if (utarray_len(qlayKeyBuffer) || qlayKeysPressed) {
			IPCreturn |= 0x01;
		}
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
		/*fpr("C8 ");*/
		IPCreturn = 0;
		IPCcnt = 4;
		if (utarray_len(qlayKeyBuffer)) { /* just double check */
			int *key;
			key = (int *)utarray_front(qlayKeyBuffer);
			utarray_erase(qlayKeyBuffer, 0, 1);
			/*fpr("Dec %x ",key);*/
			IPCreturn = decode_key(*key);
			IPCcnt = 16;
		} else {
			if (qlayKeysPressed) { /* still pressed: autorepeat */
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

void QL_exit(void)
{
}
/** Externalis�e par Jimmy **/
void init_mdvs(void)
{
	int i = 0;
	int noDrives = emulatorOptionDevCount("drive");
	bool res;

	memset(mdrive, 0, sizeof(mdrive));

	for (i = 0; i < noDrives; i++) {
		const char *drive = emulatorOptionDev("drive", i);
		const char *mdvName;

		int mdvNum;

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

		//mdrive[i].name[0] = "";
		mdrive[mdvNum].present = 0;
		mdrive[mdvNum].sector = 0;
		mdrive[mdvNum].wrprot = 0;
		mdrive[mdvNum].idx = 0;
		mdrive[mdvNum].mdvstate = MDV_GAP1;
		mdrive[mdvNum].mdvgapcnt = MDV_GAP_COUNT;

		// TODO: handle read only
		/*
		if(mdvName.find("R:") == 0) {
			if (mdvName.size() == 2) {
				continue;
			}
			mdrive[i].wrprot = 1;
			mdvName.erase(0, 2);
		}
		*/

		res = emulatorLoadFile(mdvName, mdrive[mdvNum].data,
				       MDV_NOSECTS * MDV_SECTLEN);
		if (res) {
			mdrive[mdvNum].name = mdvName;
			mdrive[mdvNum].present = 1;
			mdrive[mdvNum].sector = 0;
			mdv = mdrive[mdvNum].data;
			mdvpresent = 1;
			mdvname = mdvName;
			printf("MDV%01d is %s\n", mdvNum + 1, mdvname);
		}

		i++;
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

/* a simple brute force implementation that works just fine ... */
void do_next_event(void)
{
	int e;

	uint8_t intmask = (m68k_get_reg(NULL, M68K_REG_SR) >> 8) & 7;
	e = 0;
	if (cycles() >= e50) { /* 50Hz interrupt, 20 msec */
		// do_50Hz();
		//m68k_set_irq(2);
		//doIrq = true;
		//EMU_PC_INTR |= PC_INTRF;
		e50 = cycles() + (20 * qlay1msec);
		e++;
		if (0)
			fpr("I5 ");
	}
	if (cycles() >= emdv) { /* MDV gap interrupt, 31 msec */
		if (intmask < 2) {
			do_mdv_motor();
		}
		emdv = cycles() +
		       (5 *
			qlay1msec); // Jimmy 093 - To increase MDV Speed ! - 5 -> 31
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
	if (cycles() == etx) {
		do_tx();
		etx = cycles() - 1; /* nothing more */
		e++;
		if (0)
			fpr("ITX ");
	}

	/* decide who's next */
	eval_next_event();

	if (!e) {
		fpr("\nError NoE c: %d ", cycles());
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

int irq_level(void)
{
	if (REG18021 & 0x1f)
		return 2;
	else
		return -1;
}

static void do_mdv_motor(void)
{
	if (mdvmotor) {
		EMU_PC_INTR |= 0x01;
		doIrq = true;
		dt_event(1); /* set time stamp */
	}
	return;
}

static void do_tx(void)
{
	REG18020tx &= ~0x02; /* clear tx busy bit: byte is transmitted */
}

static void set_gap_irq(void)
{
	EMU_PC_INTR |= PC_INTRG;

	/* dont actually trigger irq if masked */
	if (!(EMU_PC_INTR_MASK & PC_MASKG)) {
		return;
	}

	doIrq = 1;
}

void do_mdv_tick(void)
{
	if (mdvmotor) {
		int fullidx;

		if (mdrive[mdvnum].present == 0) {
			set_gap_irq();
			mdvgap = 1;
			return;
		}

		if (mdvwrite) {
			mdvtxfl = false;
		}

		switch (mdrive[mdvnum].mdvstate) {
		case MDV_GAP1:
			debug_print("%s\n", "GAP1");
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
			debug_print("%s\n", "PREAMBLE1");
			mdvgap = 0;
			mdvrd = 0;
			mdrive[mdvnum].mdvgapcnt--;

			if (mdrive[mdvnum].mdvgapcnt == 0) {
				mdrive[mdvnum].mdvstate = MDV_HDR;
				mdrive[mdvnum].idx = 12;
			}
			break;
		case MDV_HDR:
			debug_print("%s\n", "HDR");
			mdvgap = 0;
			mdvrd = 1;

			fullidx = (mdrive[mdvnum].sector * MDV_SECTLEN) +
				  mdrive[mdvnum].idx;

			PC_TRAK = mdrive[mdvnum].data[fullidx];

			mdrive[mdvnum].idx++;

			if (mdrive[mdvnum].idx == MDV_HDR_SIZE) {
				mdrive[mdvnum].mdvstate = MDV_GAP2;
				mdrive[mdvnum].mdvgapcnt = MDV_GAP_COUNT;
			}
			break;
		case MDV_GAP2:
			debug_print("%s\n", "GAP2");
			mdvgap = 1;
			mdvrd = 0;
			//set_gap_irq();
			mdrive[mdvnum].mdvgapcnt--;

			if (mdrive[mdvnum].mdvgapcnt == 0) {
				mdrive[mdvnum].mdvstate = MDV_PREAMBLE2;
				mdrive[mdvnum].mdvgapcnt = MDV_PREAMBLE_COUNT;
			}
			break;
		case MDV_PREAMBLE2:
			debug_print("%s\n", "PREAMBLE2");
			mdvgap = 0;
			mdvrd = 0;
			mdrive[mdvnum].mdvgapcnt--;

			if (mdrive[mdvnum].mdvgapcnt == 0) {
				mdrive[mdvnum].mdvstate = MDV_DATA_HDR;
				mdrive[mdvnum].idx =
					MDV_HDR_SIZE + MDV_PREAMBLE_SIZE;
			}
			break;
		case MDV_DATA_HDR:
			debug_print("%s\n", "DATA_HDR");
			mdvgap = 0;
			mdvrd = 1;

			fullidx = (mdrive[mdvnum].sector * MDV_SECTLEN) +
				  mdrive[mdvnum].idx;

			PC_TRAK = mdrive[mdvnum].data[fullidx];

			mdrive[mdvnum].idx++;

			if (mdrive[mdvnum].idx ==
			    (MDV_HDR_SIZE + MDV_PREAMBLE_SIZE +
			     MDV_DATA_HDR_SIZE)) {
				mdrive[mdvnum].mdvstate = MDV_DATA_PREAMBLE;
				mdrive[mdvnum].mdvgapcnt =
					MDV_DATA_PREAMBLE_COUNT;
			}
			break;
		case MDV_DATA_PREAMBLE:
			debug_print("%s\n", "DATA_PREAMBLE");
			mdvgap = 0;
			mdvrd = 0;
			mdrive[mdvnum].mdvgapcnt--;

			if (mdrive[mdvnum].mdvgapcnt == 0) {
				mdrive[mdvnum].mdvstate = MDV_DATA;
				mdrive[mdvnum].idx = MDV_HDR_SIZE +
						     MDV_PREAMBLE_SIZE +
						     MDV_DATA_HDR_SIZE +
						     MDV_DATA_PREAMBLE_SIZE;
			}
			break;
		case MDV_DATA:
			debug_print("%s\n", "DATA");
			mdvgap = 0;
			mdvrd = 1;

			fullidx = (mdrive[mdvnum].sector * MDV_SECTLEN) +
				  mdrive[mdvnum].idx;

			PC_TRAK = mdrive[mdvnum].data[fullidx];

			mdrive[mdvnum].idx++;

			if (mdrive[mdvnum].idx == MDV_SECTLEN) {
				mdrive[mdvnum].mdvstate = MDV_GAP1;
				mdrive[mdvnum].mdvgapcnt = MDV_GAP_COUNT;
				mdrive[mdvnum].idx = 0;
				mdrive[mdvnum].sector++;
				mdrive[mdvnum].sector %= MDV_NOSECTS;
			}
			break;
		default:
			fprintf(stderr, "MDV in unknown state\n");
			break;
		}
	} else {
		mdvgap = 0;
	}
}
