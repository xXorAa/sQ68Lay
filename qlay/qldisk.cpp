/*
	QLAY - Sinclair QL emulator
	Copyright Jan Venema 1998
	QL disk access functions
*/

#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

#include "emu_options.hpp"
#include "m68k.h"
#include "q68_emulator.hpp"
#include "qldisk.hpp"

namespace qldisk
{

std::vector<std::string> winfn(8);

/* internal */
static int wrnfafile(int drivenr, int filenum, int sectnum, int bytenum,
		     int bytecnt);
static int rdnfafile(int drivenr, int filenum, int sectnum, int bytenum,
		     int bytecnt);
static int gdnfa(int drivenr, int filenum, int sectnum, int bytenum,
		 int bytecnt);
static int truncnfa(int drivenr, int filenum, int sectnum, int bytenum,
		    int bytecnt);
static int rennfa(int drivenr, int filenum, int sectnum, int bytenum,
		  int bytecnt);
static int drvcfgnfa(int drivenr);
static int store_dir(int drivenr, int offset, int bytecnt);
static int get_dir(int drivenr, int offset, int bytecnt);
static int fnum2fname(int drivenr, int fnum, std::string& fname);
static int fnum2dirname(int drivenr, std::string& fname);

static void print_byte(uint8_t val);
static void print_open(void);
static void print_close(void);

#define END_CMD 0x00
#define RD_CMD 0x81
#define WR_CMD 0x82
#define DEL_CMD 0x84
#define GD_CMD 0x88
#define TRC_CMD 0x90
#define REN_CMD 0xa0
#define DCF_CMD 0xc0
#define RST_CMD 0xff

#define CNTLREG 0x18100
#define DRIVENR 0x18101
#define FILENUM 0x18102
#define SECTNUM 0x18104
#define BYTENUM 0x18106
#define BYTECNT 0x18108
#define SECTOR 0x18200

#define OFFSET sectnum * 512 + bytenum
#define CHOPHEAD()

#define MAXDRIVE 8
#define MAXDLEN (512 * 20)
#define MAXFNLEN (255 + 37) /* drive + directory + file */

static uint8_t dir[MAXDRIVE][MAXDLEN];
static uint8_t sector[512];
static FILE *nfa;
static std::string filename;

static int p = 0; /* print debug */

static FILE *prttmp;
static int prtopen;
static int seropen;

void init_qldisk(void)
{
	int i = 0;
	for (auto &winName : options::drives) {
		if ((winName.find("win") == 0) && (winName[3] > '0') && (winName[3] < '9') && winName[4] == '@') {

			int drvNum = winName[3] - '1';

			std::string driveDir = winName.substr(5);

			auto pos = driveDir.find("qlay.dir");
			if (pos != std::string::npos) {
				driveDir.erase(pos);
			}

			winfn[drvNum] = driveDir;
		}
	}

	prtopen = 0;
	seropen = 0;
}

void exit_qldisk(void)
{
	if (prtopen == 1)
		print_close();
	prtopen = 0;
}

/* store part of directory in 'sector' into 'dir' */
/* return QDOS error */
int store_dir(int drivenr, int offset, int bytecnt)
{
	int i;

	for (i = 0; i < bytecnt; i++) {
		if (offset + i >= MAXDLEN)
			return -11; /*DF*/
		dir[drivenr - 1][offset + i] = sector[i];
	}
	return 0; /*OK*/
}

/* get part of directory 'dir' into 'sector' */
/* return QDOS error */
int get_dir(int drivenr, int offset, int bytecnt)
{
	int i;

	for (i = 0; i < bytecnt; i++) {
		if (offset + i >= MAXDLEN)
			return -7; /*NF*/
		sector[i] = dir[drivenr - 1][offset + i];
	}
	return 0; /*OK*/
}

int fnum2fname(int drivenr, int fnum, std::string& fname)
{
	if (fnum == 0) {
		//strcpy(fname, winfn[drivenr - 1]);
		//strncat(fname, "qlay.dir", 36);
		fname = winfn[drivenr - 1];
		fname.append("qlay.dir");
		return 0;
	}
	/* first file (1) stored at pos '0' */
	fnum--;
	if (fnum * 64 < MAXDLEN) {
		//strcpy(fname, winfn[drivenr - 1]);
		//strncat(fname, &dir[drivenr - 1][fnum * 64 + 16], 36);
		fname = winfn[drivenr - 1];
		fname.append((char *)&dir[drivenr -1][fnum * 64 + 16]);
		return 0;
	}
	return -1;
}

/* get directory prefix, for RENAME */
int fnum2dirname(int drivenr, std::string& fname)
{
	//strcpy(fname, winfn[drivenr - 1]);
	fname = winfn[drivenr - 1];

	return 0;
}

static int win_drives = 0;

int win_avail()
{
	return win_drives;
}

void wrnfa(uint32_t addr, uint8_t data)
{
	int drivenr, filenum, sectnum, bytenum, bytecnt,
		r; // (uint8_t),r -> No just a cast at end - Unless it should result not working NFA.

	if (addr == 0x18100) {
		//draw_LED(First_Led_X - 16, First_Led_Y, LED_GREEN,
		//	 1); /* full yellow */
		drivenr = emulator::q68MemorySpace[0x18101];
		filenum = emulator::q68MemorySpace[0x18102] * 256 + emulator::q68MemorySpace[0x18103];
		sectnum = emulator::q68MemorySpace[0x18104] * 256 + emulator::q68MemorySpace[0x18105];
		bytenum = emulator::q68MemorySpace[0x18106] * 256 + emulator::q68MemorySpace[0x18107];
		bytecnt = emulator::q68MemorySpace[0x18108] * 256 + emulator::q68MemorySpace[0x18109];

		win_drives |= (1 << (drivenr - 1));

		switch (data) {
		case END_CMD:
			r = 0;
			break;
		case RD_CMD:
			r = rdnfafile(drivenr, filenum, sectnum, bytenum,
				      bytecnt);
			break;
		case WR_CMD:
			r = wrnfafile(drivenr, filenum, sectnum, bytenum,
				      bytecnt);
			break;
		case DEL_CMD:
			fnum2fname(drivenr, filenum, filename);
			/* more checks needed... */
			r = remove(filename.c_str());
			break;
		case GD_CMD:
			filenum = 0;
			bytenum = 0;
			bytecnt = 512;
			r = gdnfa(drivenr, filenum, sectnum, bytenum, bytecnt);
			break;
		case TRC_CMD:
			r = truncnfa(drivenr, filenum, sectnum, bytenum,
				     bytecnt);
			break;
		case REN_CMD:
			r = rennfa(drivenr, filenum, sectnum, bytenum, bytecnt);
			break;
		case DCF_CMD:
			r = drvcfgnfa(drivenr);
			break;
		case RST_CMD:
			r = 0;
			break;
		default:
			r = -19; /*NIyet*/
		}
		// Type cast add by Jimmy (uint32_t) & (uint8_t)
		m68k_write_memory_8((uint32_t)0x1810a, (uint8_t)((r >> 8) & 0xff));
		m68k_write_memory_8((uint32_t)0x1810b, (uint8_t)(r & 0xff));
		//draw_LED(First_Led_X - 16, First_Led_Y, LED_GREEN,
		//	 0); /* full black */
	}
}

uint8_t rdnfa(uint32_t addr)
{
	uint8_t data;

	data = emulator::q68MemorySpace[addr];
	return data;
}

int drvcfgnfa(int drivenr)
{
	if (drivenr == 1)
		return 1; /* win1_ always there */
	if (winfn[drivenr - 1].size())
		return 1;
	else
		return 0;
	//	if (winpresent[drivenr-1]) return 1; else return 0;
}

int wrnfafile(int drivenr, int filenum, int sectnum, int bytenum, int bytecnt)
{
	long offset;
	int pos, i;

	offset = OFFSET - 64;
	if (offset < 0) {
		if (bytecnt <= 64) {
			return bytecnt;
		} else {
			bytecnt = bytecnt + offset; /* sic! */
			offset = 0;
		}
	}
	fnum2fname(drivenr, filenum, filename);
	nfa = fopen(filename.c_str(), "rb+");
	if (nfa == NULL) {
		nfa = fopen(filename.c_str(), "wb+");
		if (nfa == NULL) {
			return -7; /*NF*/
		}
	}
	fseek(nfa, 0, 0);
	pos = fseek(nfa, offset, 1);
	if (pos != 0) { /*seek failed*/
		if (filenum == 0) {
			fseek(nfa, 0, 0);
			for (i = 0; i < offset; i++)
				putc(0, nfa);
		} else {
			fclose(nfa);
			return -10; /*EOF*/
		}
	}
	for (i = bytenum; i < bytenum + bytecnt; i++)
		sector[i - bytenum] = emulator::q68MemorySpace[i + 0x18200];
	/* eqv:	for(i=0;i<bytecnt;i++) sector[i]=get_byte(i+bytenum+0x18200);*/

	i = fwrite(sector, 1, bytecnt, nfa);
	if (i != bytecnt) {
		fclose(nfa);
		return -7; /*NF*/
	}
	fclose(nfa);
	if (filenum == 0) {
		if (store_dir(drivenr, offset, bytecnt) < 0)
			return -11; /* DF */
	}
	return 0; /*OK*/
}

int rdnfafile(int drivenr, int filenum, int sectnum, int bytenum, int bytecnt)
{
	long offset;
	int pos, i;

	offset = OFFSET - 64;
	if (offset < 0) {
		if (bytecnt <= 64) {
			return bytecnt;
		} else {
			bytecnt = bytecnt + offset; /* sic! */
			offset = 0;
		}
	}

	if (0)
		if (filenum == 0) {
			if (get_dir(drivenr, offset, bytecnt) < 0)
				return -7; /*NF*/
			for (i = 0; i < bytecnt; i++)
				m68k_write_memory_8(i + 0x18200 + bytenum, sector[i]);
			return 0;
		}

	fnum2fname(drivenr, filenum, filename);
	nfa = fopen(filename.c_str(), "rb");
	if (nfa == NULL) {
		return -7; /*NF*/
	}
	fseek(nfa, 0, 0);
	pos = fseek(nfa, offset, 0);
	if (pos != 0) { /*seekfailed*/
		fclose(nfa);
		return -10; /*EOF*/
	}
	i = fread(sector, 1, bytecnt, nfa);
	if (i != bytecnt) {
		/*		fclose(nfa);*/
		/*		return -7 */ /*NF*/;
		bytecnt = i;
	}
	for (i = bytenum; i < bytenum + bytecnt; i++)
		m68k_write_memory_8(i + 0x18200, sector[i - bytenum]);
	fclose(nfa);
	return 0; /*OK*/
}

int gdnfa(int drivenr, int filenum, int sectnum, int bytenum, int bytecnt)
{
	long offset;
	int pos, i, rv;
	int s0offset;

	offset = OFFSET - 64;
	s0offset = 0;
	if (offset < 0) {
		if (bytecnt <= 64) {
			return bytecnt;
		} else {
			s0offset = -offset; /* compensate for return */
			bytecnt = bytecnt - s0offset;
			offset = 0;
		}
	}

	fnum2fname(drivenr, filenum, filename);
	nfa = fopen(filename.c_str(), "rb");
	if (nfa == NULL) {
		return -7; /*NF*/
	}
	/*	fseek(nfa,0L,0);*/
	pos = fseek(nfa, offset, 0);
	if (ftell(nfa) != offset) { /*seekfailed*/
		fclose(nfa);
		return 0; /* this occurs when dir file len%512=0 */
	}
	rv = fread(sector, 1, bytecnt, nfa);

	for (i = bytenum; i < bytenum + rv; i++)
		m68k_write_memory_8(i + 0x18200, sector[i - bytenum]);
	fclose(nfa);
	if (store_dir(drivenr, offset, rv) < 0)
		return -11; /* DF */
	return rv + s0offset; /*OK*/
}

int truncnfa(int drivenr, int filenum, int sectnum, int bytenum, int bytecnt)
{
	long offset;
	int pos; //Jimmy - 093 remove : int i,rv;
	int s0offset;

	offset = OFFSET - 64;
	s0offset = 0;
	if (offset < 0) {
		if (bytecnt <= 64) {
			return bytecnt;
		} else {
			s0offset = -offset; /* compsensate for return */
			bytecnt = bytecnt - s0offset;
			offset = 0;
		}
	}

	fnum2fname(drivenr, filenum, filename);
	nfa = fopen(filename.c_str(), "rb");
	if (nfa == NULL) {
		return -7; /*NF*/
	}
	/*	fseek(nfa,0L,0);*/
	pos = fseek(nfa, offset, 0);
	pos = ftell(nfa);
	if (pos < offset) { /* seekfailed? how do we get this? */
		/* should the file be extended till offset? */
		fclose(nfa);
		return 0; /* this occurs when dir file len%512=0 */
	}
	fseek(nfa, 0L, 2);
	pos = ftell(nfa);
	if (pos > offset) {
		ftruncate(fileno(nfa), offset); /* not tested yet */
	}
	fclose(nfa);
	return 0; /*OK*/
}

/* rename file. filename in sector, return QDOS error code */
int rennfa(int drivenr, int filenum, int sectnum, int bytenum, int bytecnt)
{
	int i, rv, fnlen;
	std::string newname;

	rv = 0;

	fnum2fname(drivenr, filenum, filename);
	nfa = fopen(filename.c_str(), "rb");
	if (nfa == NULL) {
		return -7; /*NF*/
	}
	fclose(nfa);
	fnlen = bytecnt;
	if (fnlen > 36)
		fnlen = 36; /* silently adjust */
	for (i = 0; i < fnlen; i++)
		sector[i] = emulator::q68MemorySpace[i + 0x18200];
	sector[fnlen] = '\0';
	fnum2dirname(drivenr, newname);
	newname.append((char *)sector);
	/* check if already exist */
	nfa = fopen(newname.c_str(), "rb");
	if (nfa != NULL) {
		fclose(nfa);
		return -8; /*AE*/
	}
	rv = rename(filename.c_str(), newname.c_str());
	if (rv != 0)
		rv = -1; /*NC*/
	return rv;
}

uint8_t rdserpar(uint32_t a)
{
	return 0;
}

void wrserpar(uint32_t a, uint8_t d)
{
	if (a == 0x180c0)
		print_byte(d);
}

static void print_open(void)
{
	// prttmp = fopen(prtname, "wb");
	// if (prttmp == NULL) {
	// 	prtopen = -1;
	// } else {
	// 	prtopen = 1;
	//}
}

static void print_close(void)
{
	fclose(prttmp);
	prtopen = 0;
}

static void print_byte(uint8_t val)
{
	if (prtopen == 0)
		print_open();
	if (prtopen == 1) {
		fputc(val, prttmp);
		fflush(prttmp);
		if (val == 0x1A) { /* eof ^Z */
			print_close();
		}
	}
}

} // namespace qldisk