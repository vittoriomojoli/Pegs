/*
 * game.cpp
 *
 *  Created on: 31 Jan 2016
 *      Author: vittorio
 */
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdint.h>
using namespace std;

static const int NO_OF_HOLES = 33;
static const int FINAL_LEVEL = 32;
static const int MID_LEVEL = 16;

void trimMidHalfLevel(bool centreHoleIsFull);

const uint32_t bufSize = 10000;
uint32_t sbuf[bufSize];
uint32_t lobuf[bufSize];
uint32_t hibuf[bufSize];

const char * myFileName = "testFile.out";
const char * modeCreateWriteBinary = "wb";
const char * modeOpenReadBinary = "rb";
const char * modeOpenReadWriteBinary = "r+b";

/*
 * A move affects three positions, which must be adjacent and aligned vertically
 * or horizontally.
 * Before the move there is a peg in the 'from' and 'middle' positions only.
 * In the move the peg in 'from' jumps over 'middle' and lands in 'to'.
 * The peg in the 'middle' is removed from the board.
 *
 */
struct move {
  int from;
  int middle;
  int to;
};

/*
 * In a coded move each peg position is represented by a bit in a 32-bit word.
 * The bit position is the given by the peg number in the 'positions' array
 * The mask has a 1 in the 'from', 'middle' and 'to' positions of the move.
 * The match has a 1 in the 'from' and 'middle' position of the move.
 * The move is possible if of masking the board status with the mask the
 * result is the match.
 * Executing the move is equivalent to XOR-in the coded board status with the
 * mask.
 * The word that represents the board status has 32 bits, hence it can represent
 * only holes 0-31. Hole 32 is represented separately to avoid having to use
 * 64 bits to represent the board status.
 * TODO: would a 64-bit representation be faster?
 */
struct coded_move {
  uint32_t mask;
  uint32_t match;
};

/*
 * The array positions[][] represent the playing board,
 * where the holes that initially contain a peg are numbered as follows.
 * The numbers are chosen so that adding 8 modulo 32 produce a rotation by 90
 * degrees.
 * The hole that is initially empty and those that fall outside the board are
 * marked with -1.
 * The hole at the centre of the board is numbered 32.
 *
 *         00 01 02
 *         03 04 05
 *   26 29 31 06 07 11 08
 *   25 28 30    14 12 09
 *   24 27 23 22 15 13 10
 *         21 20 19
 *         18 17 16
 */
int positions[7][7] = {
{-1,  -1,   0,   1,   2,   -1,  -1},
{-1,  -1,   3,   4,   5,   -1,  -1},
{26, 29, 31,   6,   7,  11,  8},
{25, 28, 30, -1,  14, 12,  9},
{24, 27, 23, 22,  15, 13, 10},
{-1,  -1, 21, 20,  19,  -1, -1},
{-1,  -1, 18, 17,  16,  -1, -1}};

uint32_t posMasks[7][7];

/*
 * List of all the moves that do not affect hole 32.
 */
const move normal_moves[] = {
  {0, 3, 31}, {1, 4, 6}, {2, 5, 7},
  { 3, 31, 30}, {5, 7, 14},
  {26, 25, 24}, {29, 28, 27}, {31, 30, 23}, {7, 14, 15}, {11, 12, 13}, {8, 9, 10},
  {30, 23, 21}, {14, 15, 19},
  {23, 21, 18}, {22, 20, 17}, {15, 19, 16}
#if 1
,

  {24,27,23},{25,28,30},{26,29,31},
  {27,23,22},{29,31,6},
  {18,17,16},{21,20,19},{23,22,15},{31,6,7},{3,4,5},{0,1,2},
  {22,15,13},{6,7,11},
  {15,13,10},{14,12,9},{7,11,8},

  {16,19,15},{17,20,22},{18,21,23},
  {19,15,14},{21,23,30},
  {10,9,8},{13,12,11},{15,14,7},{23,30,31},{27,28,29},{24,25,26},
  {14,7,5},{30,31,3},
  {7,5,2},{6,4,1},{31,3,0},

  {8,11,7}, {9,12,14},{10,13,15},
  {11,7,6}, {13,15,22},
  {2,1,0}, {5,4,3}, {7,6,31}, {15,22,23}, {19,20,21}, {16,17,18},
  {6,31,29}, {22,23,27},
  {31,29,26}, {30,28,25}, {23, 27,24}
#endif
};
const int nNormal = sizeof(normal_moves)/sizeof(move);
coded_move moves_normal[nNormal];

/*
 * List of all the moves that change hole 32 from empty to full
 */

const move e2f_moves[] = {
 {4, 6, 32}
#if 1
, {28,30,32},{20,22,32},{12,14,32}
#endif
};
const int ne2f = sizeof(e2f_moves)/sizeof(move);
coded_move moves_e2f[ne2f];

/*
 * List of all the moves that change hole 32 from full to empty
 */

const move f2e_moves[] = {
 {6, 32, 22}, {32, 22, 20}
#if 1
,
 {30,32,14},{32,14,12},
 {22,32,6},{32,6,4},
 {14,32,30},{32,30,28}
#endif
};
const int nf2e = sizeof(f2e_moves)/sizeof(move);
coded_move moves_f2e[nf2e];

/*
 * Converts an array of moves from hole number representation
 * to bit coded representation.
 * Note that in the conversion hole 32 is discarded.
 */

void prepareMoves (const move * m, coded_move * cm, int n)
{
//  cout.setf(ios::hex, ios::basefield);
  for (int i = 0; i < n ; i ++)
  {
    uint32_t from = (m[i].from < 32) ? (1<<m[i].from) : 0;
    uint32_t middle = (m[i].middle < 32) ? (1<<m[i].middle) : 0;
    uint32_t to = (m[i].to < 32) ? (1<<m[i].to) : 0;
    cm[i].match = from | middle;
    cm[i].mask = from | middle | to;
//    cout << "mask= " << cm[i].mask << " match=" << cm[i].match << endl;
  }
//  cout.setf(ios::dec, ios::basefield);
}

/*
 * Convert all moves from from hole numbers to bit match/mask form
 */
void prepareAllMoves ()
{
  prepareMoves(normal_moves, moves_normal, nNormal);
  prepareMoves(e2f_moves, moves_e2f, ne2f);
  prepareMoves(f2e_moves, moves_f2e, nf2e);
  int i, j;
  for (i = 0; i<7; i++) {
    for (j  = 0; j<7; j++) {
      if (positions[i][j] < 0)
        posMasks[i][j] = 0;
      else {
        posMasks[i][j] = (1 << positions[i][j]);
//        cout << "posmask i=" << i << " j=" << j << " mask=" <<  posMasks[i][j] << endl;
      }
    }
  }
}

/*
 * Compare uint32_ts pointed to by void pointers
 */
int unsignedLongCompare (const void * elem1, const void * elem2 )
{
  if (*((uint32_t *)elem1) < *((uint32_t *)elem2)) return -1;
  if (*((uint32_t *)elem1) > *((uint32_t *)elem2)) return 1;
  return 0;
}

/*
 * Removed duplicates from an ordered file of uint32_ts.
 */
uint32_t longUniq(const char * fileName)
{
  char tempName[L_tmpnam];
  FILE * fr = fopen(fileName, modeOpenReadBinary);
  tmpnam(tempName);
  FILE * fw = fopen(tempName, modeCreateWriteBinary);
  // read and write the first unsigned
  uint32_t lv;
  uint32_t ucount = 0;
  if (fread (&lv, sizeof(uint32_t), 1, fr) == 1) {
      fwrite(&lv, sizeof(uint32_t), 1, fw);
      ucount++;
  }
  while (1) {
    // read a buffer worth
    int sbufc = fread (sbuf, sizeof(uint32_t), bufSize, fr);
    if (sbufc <= 0)
      break;
    // scan the buffer for unique values in the buffer
    int sbufr = 0;
    int sbufw = 0;
    while (sbufr < sbufc) {
      uint32_t v = sbuf[sbufr++];
      if (v < lv) {
        cout << "error: v=" << v << "; lv=" << lv << endl;  // out of sequence
        return ucount;
      }
      if (v > lv)
        lv = sbuf[sbufw++] = v;
    }
    // write out the unique values left in the buffer
    if (sbufw > 0) {
      fwrite(sbuf, sizeof(uint32_t), sbufw, fw);
      ucount += sbufw;
    }
  }
  fclose(fr);
  fclose(fw);
  remove (fileName);
  rename (tempName, fileName);
  return ucount;
}

/*
 * Quick sort an area of a file of uint32_t integers, starting at the element of index 'lo'
 * and ending at the element of index 'hi'
 */
void quickFileSort(FILE * f, uint32_t lo, uint32_t hi) {
  if (lo>=hi) return; /* nothing left to sort */
  uint32_t n = hi - lo + 1; /* size of area to sort */
  if (n <= bufSize) {
	 /* the area is small enough to be sorted in memory */
     fseek ( f, lo * sizeof(uint32_t), SEEK_SET );
     fread (sbuf, sizeof(uint32_t), n, f);
     qsort (sbuf, n, sizeof(uint32_t), unsignedLongCompare );
     fseek ( f, lo * sizeof(uint32_t), SEEK_SET );
     fwrite(sbuf, sizeof(uint32_t), n, f);
     return;
  }
  int sbufc, lobufc, hibufc;
  uint32_t c, lofr, lofw, hifr, hifw;
  uint32_t pv;
  // read the first element as pivot value
  fseek( f, lo * sizeof(uint32_t), SEEK_SET);
  fread(&pv, sizeof(uint32_t), 1, f);
  lofw = lo;
  lofr = lo + 1;  // skip after pivot, which has already been read
  hifr = hifw = hi + 1;  // point after last unprocessed in file
  lobufc = 0; // lobuf empty
  hibufc = 0; // hibuf empty
  sbufc = 0; // sbuf empty
  while (1) {
    // here the source buffer is always empty, and the high and low buffer between them contain at most one buffer worth of data
    // make space to copy hibuf back to the file by reading from the high end into the source buffer
    if (hifw - hifr < hibufc) {
      c = hibufc;
      if (c > hifr - lofr)
         c = hifr - lofr;
      if (c > 0) {
        fseek(f, (hifr-c) * sizeof(uint32_t), SEEK_SET);
        fread(sbuf, sizeof(uint32_t), c, f);
        sbufc = c;
        hifr -= c;
      }
    }
    // copy the high buffer to the high end
    if (hibufc > 0) {
      fseek( f, (hifw - hibufc) * sizeof(uint32_t), SEEK_SET);
      fwrite(hibuf, sizeof(uint32_t), hibufc, f);
      hifw -= hibufc;
      hibufc = 0;
    }
    // free the low end by filling in the rest of the source buffer
    if (sbufc < bufSize) {
      c = bufSize - sbufc;
      if (c > hifr - lofr)
        c = hifr - lofr;
      if (c > 0) {
        fseek( f, lofr * sizeof(uint32_t), SEEK_SET);
        fread(sbuf + sbufc, sizeof(uint32_t), c, f);
        sbufc += c;
        lofr += c;
      }
    }
    // copy the low buffer to the low end
    if (lobufc > 0) {
      fseek( f, lofw * sizeof(uint32_t), SEEK_SET);
      fwrite(lobuf, sizeof(uint32_t), lobufc, f);
      lofw += lobufc;
      lobufc = 0;
    }
    // if the source buffer is empty, we have finished
    if (sbufc == 0)
      break;
    // sort the content of the source buffer into the low or high buffers
    while (sbufc) {
      uint32_t v = sbuf[--sbufc];
      if (v > pv)
        hibuf[hibufc++] = v;
      else
        lobuf[lobufc++] = v;
    }
  }
  // put the pivot back where it belongs
  fseek( f, lofw * sizeof(uint32_t), SEEK_SET);
  fwrite(&pv, sizeof(uint32_t), 1, f);
  quickFileSort(f, lo, lofw-1);
  quickFileSort(f, lofw+1, hi);
}

char * getName(int level, bool centreHoleFull, bool isTrimmed) {
  static char buf[20];
  buf[0] = centreHoleFull ? 'F' : 'E';
  buf[1] = '0' + (char)(level /10);
  buf[2] = '0' + (char)(level %10);
  strcpy(buf+3, isTrimmed ? "T.gam" : ".gam");
  return buf;
}

#if 0
void debugMove(uint32_t source, uint32_t mask, uint32_t match, uint32_t dest, bool matched, char type) {
  cout.setf(ios::hex, ios::basefield);
  cout << source << ' ' << mask << ' ' << match << ' ' << dest << ' ' << matched << ' ' << type << endl;
  cout.setf(ios::dec, ios::basefield);
}
#endif

#if 0
void debugNewMove (uint32_t m1, uint32_t m2, uint32_t m3, uint32_t m4) {
  cout.setf(ios::hex, ios::basefield);
  cout << "New Position: " << m1 << ' ' << m2 << ' ' << m3 << ' ' << m4 << endl;
  cout.setf(ios::dec, ios::basefield);
}
#endif

/*
 * For each positions contained in the source buffer find all the reachable positions
 * at the next level and store them in the destination file (if the centre hole remains the same)
 * or in the complement destination file (if the centre hole has changed from full to empty or
 * empty to full.
 */
void expandBuffer(bool full, uint32_t * sbuf, int sc, FILE* fdest, FILE* fdestComplement) {
  const int dl = 10000;
  const int dcl = 1000;
  uint32_t dbuf[dl], dcbuf[dcl];
  int dc = 0;
  int dcc = 0;
  while (sc > 0) {
    uint32_t s = sbuf[--sc];
    // make all the moves that leave the centre hole unchanged
    for (int i = 0; i < nNormal; i++) {
      uint32_t d = moves_normal[i].mask;
//      debugMove(s, moves_normal[i].mask, moves_normal[i].match, d, false, 'n');
      if ((s & d) == moves_normal[i].match) {
        d = s ^ d;
        if ( dc < dl - 4) {
//          cout << "New move found - normal = " << d << endl;
          dbuf[dc++] = d;
#if 0
          dbuf[dc++] = ((d<<8) & 0xFFFFFF00) | ((d>>24) & 0x000000FF);
          dbuf[dc++] = ((d<<16) & 0xFFFF0000) | ((d>>16) & 0x0000FFFF);
          dbuf[dc++] = ((d<<24) & 0xFF000000) | ((d>>8) & 0x00FFFFFF);
#endif
//          debugNewMove (dbuf[dc-4], dbuf[dc-3], dbuf[dc-2], dbuf[dc-1]);
        } else {
          cout << "No space in dbuf" << endl;
        }
//        debugMove(s, moves_normal[i].mask, moves_normal[i].match, d, true, 'N');
      } else {
//        debugMove(s, moves_normal[i].mask, moves_normal[i].match, d, false, 'N');
      }
    }
    if (full) {
    	// make all the moves that change the centre hole from full to empty
      for (int i = 0; i < nf2e; i++) {
        uint32_t d = moves_f2e[i].mask;
        if ((s & d) == moves_f2e[i].match) {
          d = s ^ d;
          if ( dcc < dcl - 4) {
//            cout << "New move found - f2e = " << d << endl;
            dcbuf[dcc++] = d;
#if 0
            dcbuf[dcc++] = ((d<<8) & 0xFFFFFF00) | ((d>>24) & 0x000000FF);
            dcbuf[dcc++] = ((d<<16) & 0xFFFF0000) | ((d>>16) & 0x0000FFFF);
            dcbuf[dcc++] = ((d<<24) & 0xFF000000) | ((d>>8) & 0x00FFFFFF);
#endif
//            debugNewMove (dcbuf[dcc-4], dcbuf[dcc-3], dcbuf[dcc-2], dcbuf[dcc-1]);
          } else {
            cout << "No space in dcbuf" << endl;
          }
//          debugMove(s, moves_f2e[i].mask, moves_f2e[i].match, d, true, 'F');
        } else {
//          debugMove(s, moves_f2e[i].mask, moves_f2e[i].match, d, false, 'F');
        }
      }
    } else {
    	// make all the moves that change the centre hole from empty to full
      for (int i = 0; i < ne2f; i++) {
        uint32_t d = moves_e2f[i].mask;
        if ((s & d) == moves_e2f[i].match) {
          d = s ^ d;
          if ( dcc < dcl - 4) {
//            cout << "New move found - e2f = " << d << endl;
            dcbuf[dcc++] = d;
#if 0
            dcbuf[dcc++] = ((d<<8) & 0xFFFFFF00) | ((d>>24) & 0x000000FF);
            dcbuf[dcc++] = ((d<<16) & 0xFFFF0000) | ((d>>16) & 0x0000FFFF);
            dcbuf[dcc++] = ((d<<24) & 0xFF000000) | ((d>>8) & 0x00FFFFFF);
#endif
//            debugMove(s, moves_e2f[i].mask, moves_e2f[i].match, d, true, 'E');
          } else {
            cout << "No space in dcbuf" << endl;
          }
        } else {
//          debugMove(s, moves_e2f[i].mask, moves_e2f[i].match, d, false, 'E');
        }
      }
    }
  }
  if (dc > 0) {
    fwrite(dbuf, sizeof(uint32_t), dc, fdest);
  }
  if (dcc > 0) {
    fwrite(dcbuf, sizeof(uint32_t), dcc, fdestComplement);
  }
}

/*
 *
 */
void expandHalfLevel(bool full, FILE* fsource, FILE* fdest, FILE* fdestComplement) {
  const int sl = 20;
  uint32_t sbuf[sl];
  while (1) {
    int sc = fread(sbuf, sizeof(uint32_t), sl, fsource);
    if (sc <= 0)
      break;
    expandBuffer(full,  sbuf, sc , fdest, fdestComplement);
  }
}

clock_t ticks;

/*
 * Note start time
 */
void startTime () {
  ticks = clock();
}

/*
 * Show time elapsed since noted start time.
 */
void showTime() {
  cout << "At time " << (clock() - ticks)/CLK_TCK << " sec: ";
}

/*
 * Output a picture of the board state, where 'X' marks a present peg
 * and '.' marks an absent peg.
 */
void showPosition (uint32_t pos, bool full) {
#if 0
  cout.setf(ios::hex, ios::basefield);
  cout << "position code is: " << pos << " full:" << full << endl;
  cout.setf(ios::dec, ios::basefield);
#endif
  cout << endl;
  for (int i = 0; i<7; i++) {
    for (int j = 0; j<7; j++) {
      if (i == 3 && j == 3)
        cout << (full ? 'X' : '.');
      else {
        if (posMasks[i][j] == 0) {
          cout << ' ';
        } else  {
          uint32_t masked = pos & posMasks[i][j];
          cout << (masked != 0 ? 'X' : '.');
        }
      }
    }
    cout << endl;
  }
}

/*
 * Show all board positions in a file of longs
 */
void showLongFile(char * fname, bool full) {
  cout << "Showing file " << fname << endl;
  FILE * f = fopen(fname, modeOpenReadBinary);
  uint32_t ul;
  if (f != 0) {
    while (fread(&ul, sizeof(uint32_t), 1, f) > 0) {
      showPosition(ul, full);
    }
    fclose(f);
  } else {
    cout << "cannot open file " << fname << endl;
  }
}

/*
 * Sort and remove duplicates and optionally show a file
 * at a given level and central peg state.
 */
void sortCompressShow (bool full, int level, bool show) {
  FILE * f = fopen(getName(level, full, false), modeOpenReadWriteBinary);
  fseek ( f, 0, SEEK_END );
  uint32_t len = ftell(f)/sizeof(uint32_t);
  quickFileSort(f, 0, len - 1);
  showTime();
  cout << "Level " << level << (full ? " full" : " empty") << " sorted. Length = " << len << endl;
  fclose(f);
  uint32_t lu = longUniq(getName(level, full, false));
  showTime();
  cout << "Level " << level << (full ? " full" : " empty") << " uniq-ed. Length = " << lu << endl;
  if (show) {
    showLongFile(getName(level, full, false), full);
  }
}


/*
 * Expand a level into the next level
 */
void expandLevel(int level, bool show) {
  FILE * fer = fopen(getName(level, false, false), modeOpenReadBinary);
  FILE * ffr = fopen(getName(level, true, false), modeOpenReadBinary);
  FILE * few = fopen(getName(level+1, false, false), modeCreateWriteBinary);
  FILE * ffw = fopen(getName(level+1, true, false), modeCreateWriteBinary);
  expandHalfLevel(false, fer, few, ffw);
  showTime();
  cout << "Level " << level << " empty expanded" << endl;
  expandHalfLevel(true, ffr, ffw, few);
  showTime();
  cout << "Level " << level << " full expanded" << endl;
  fclose(fer);
  fclose(ffr);
  fclose(few);
  fclose(ffw);
#if 0
  showLongFile(getName(level+1, false), false);
  showLongFile(getName(level+1,true), true);
#endif
  sortCompressShow (false, level+1, show);
  sortCompressShow (true, level+1, show);
}

/*
 * Remove from level the positions that are not in the complement of complementLevel.
 */
void intersectWithComplement(int level, int complementLevel) {
	// TODO HERE;
}


/*
 * Play starting from level 1 until the final level is reached.
 */
void findForwardReachablePositions(int finalLevel, bool show)
{
  prepareAllMoves();
  // seed level 1 files
  FILE * f = fopen(getName(1, false, false), modeCreateWriteBinary);
  uint32_t startPosition = 0xFFFFFFFF; // one position with the centre empty
  fwrite(&startPosition, sizeof(uint32_t), 1, f);
  fclose(f);
  f = fopen(getName(1, true, false), modeCreateWriteBinary);
  fclose(f); // no position with the centre full
  startTime();
  for (int i = 1; i < finalLevel; i++) {
    expandLevel(i, show);
    if (i >= (NO_OF_HOLES - i)) {
    	intersectWithComplement(i, NO_OF_HOLES-i);
    	intersectWithComplement(NO_OF_HOLES-1, i);
    }
  }
}

/*
 * Recursive function to find a uint32_t value in a file ordered in ascending values.
 */

bool valueFoundRecurse(FILE* f, uint32_t lo, uint32_t hi, uint32_t value) {
  uint32_t mid = (lo+1)/2 + hi/2;
  fseek( f, mid * sizeof(uint32_t), SEEK_SET);
  uint32_t fv;
  fread(&fv, sizeof(uint32_t), 1, f);
  if (value == fv) {
    return true;
  } else if (value < fv) {
    if (mid > lo)
      return valueFoundRecurse(f, lo, mid-1, value);
    else
      return false;
  } else {
    if (mid < hi)
      return valueFoundRecurse(f, mid+1, hi, value);
    else
      return false;
  }
}

/*
 * Check if a value is found in a level file
 */
bool valueFound(int level, bool full, uint32_t value) {
  FILE * f = fopen(getName(level, full, false), modeOpenReadBinary);
  if (f == NULL) {
    cout << "cannot open move file";
    return false;
  }
//  cout.setf(ios::hex, ios::basefield);
//  cout << "searching " << getName(level, full) << " for " << value << endl;
//  cout.setf(ios::dec, ios::basefield);
  uint32_t lo = 0;
  fseek ( f, 0, SEEK_END );
  uint32_t hi = ftell(f)/sizeof(uint32_t);
  bool found = valueFoundRecurse(f, lo, hi, value);
  fclose(f);
  return found;
}

/*
 * TODO
 */
void findPrecedingNormal(bool full, int level, uint32_t value, uint32_t * values, bool * fullflags, int * count) {
//  cout.setf(ios::hex, ios::basefield);
  for (int i = 0; i < nNormal; i++) {
    uint32_t s;
//    cout << "trying " << level << ' ' << value << ' ' << moves_normal[i].mask << ' ' << moves_normal[i].match << endl;
    if (((value & moves_normal[i].mask) != 0) && ((value & moves_normal[i].match) == 0)) {
      s = value ^  moves_normal[i].mask;
      if (valueFound(level - 1, full, s)) {
//        cout << "found";
        values[*count] = s;
        fullflags[*count] = full;
        (*count)++;
      }
    }
  }
//  cout.setf(ios::dec, ios::basefield);
//  cout << "end trying findPrecedingnormal" << endl;
}

/*
 * TODO
 */
void findPrecedinge2f(int level, uint32_t value, uint32_t * values, bool * fullflags, int * count) {
  // find an empty preceding a full
//  cout.setf(ios::hex, ios::basefield);
  for (int i = 0; i < ne2f; i++) {
//    cout << "trying " << level << ' ' << value << ' ' << moves_e2f[i].mask << ' ' << moves_e2f[i].match << endl;
    uint32_t s;
    if ((value & moves_e2f[i].match) == 0) {
      s = value ^  moves_e2f[i].mask;
//      cout << "found move match" << endl;
      if (valueFound(level - 1, false, s)) {
//        cout << "match found in move file";
        values[*count] = s;
        fullflags[*count] = false;
        (*count)++;
      }
    }
  }
//  cout.setf(ios::dec, ios::basefield);
//  cout << "end trying findPrecedinge2f" << endl;
}

/*
 * TODO
 */
void findPrecedingf2e(int level, uint32_t value, uint32_t * values, bool * fullflags, int * count) {
  // find a full preceding an empty
//  cout.setf(ios::hex, ios::basefield);
  for (int i = 0; i < nf2e; i++) {
    uint32_t s;
//    cout << "trying " << level << ' ' << value << ' ' << moves_f2e[i].mask << ' ' << moves_f2e[i].match << endl;
    if (((value & moves_f2e[i].mask) != 0) && ((value & moves_f2e[i].match) == 0)) {
      s = value ^  moves_f2e[i].mask;
      if (valueFound(level - 1, true, s)) {
//        cout << "found";
        values[*count] = s;
        fullflags[*count] = true;
        (*count)++;
      }
    }
  }
//  cout << "end trying findPrecedingf2e" << endl;
//  cout.setf(ios::dec, ios::basefield);
}

/*
 * TODO
 */
void retraceSteps(bool full, int level, uint32_t value)
{
  showPosition(value, full);
  uint32_t values[100];
  bool fullflags[100];
  int count = 0;
  // find all preceding positions
  findPrecedingNormal(full, level, value, values, fullflags, &count);
  if (full)
    findPrecedinge2f(level, value, values, fullflags, &count);
  else
    findPrecedingf2e(level, value, values, fullflags, &count);
  if (count == 0 || level <= 1) {
//    cout << "the end at level " << level << endl;
    return;
  }
#if 1
  // find the lowest value move
  int i = 0;
  for (int j=1; j<count; j++) {
    if (values[j] < values[i])
      i = j;
  }
#else
  int i = rand() % count;
#endif
//  cout << "recursing retrace steps";
  retraceSteps(fullflags[i], level-1, values[i]);
}

/*
 * A position at level n has n empty holes and 33-n full holes.
 * Two positions are complementary if the holes that are full in one are empty in the other.
 * The start position (at level 1) is the complement of the end position (at level 32).
 * The complement of a position at level n (n empty holes) is at level 33-n (n full holes).
 * If a move can be played on a position, the same move can be played backward on its complement.
 * The positions at level n that can be reached from the start playing forward are complementary
 * to the positions at level (32-n) that can be reached from the end playing backward,
 * which are the positions at level (32-n) that can reach the end playing forward.
 * The complement of the positions at level 16 reachable from the start are the positions at
 * level 17 that can reach the end.
 * The valid positions at level 17 are those that can be reached from 16 and can reach the end.
 * If a position x at level 16 has a successor in a complement of position y at level 16, there is a path:
 * start .. x  ~y .. end. So x is a confirmed member of level 16.
 * The same sequence of moves played backwards and in reverse order would lead the sequence of positions:
 * start .. y ~x .. end. So y is also a confirmed member of level 16.
 * If for a given x there is no y that is part of the sequence
 * start .. x ~y .. end, then there is no y that is part of the sequence
 * start .. y ~x .. end, therefore neither x nor ~x are part of the solution, and x can be removed.
 * When all the level n members that are not part of the solution have been purged, the level n-1 members
 * that cannot reach any level n member can also be purged, and so on until levels 1 to 16 included are
 * purged. Then level 17 to 32 are obtained as the complements of the levels 16 to 1.
 * If there is a move from p to q, then there is a move from ~p2 to ~p1.
 * Therefore since the start and end position are complementary, p(1) = ~p(32), then if there is a
 * solution p1, p2, .., p31, p32, then ~p32, ~p31, .. , ~p2, ~p1 is also a solution.
 * So all the solutions at level n are complements of the solutions at level (33-n).
 * If a position is not a solution at level n, its complement is not a solution at level (33-n).
 * So when levels 16 and 17 are calculated going forward, all the solutions in 17 that are not in the
 * complement of 16 can be removed. Equally all the solutions in 16 that are not a complement of 17 can be
 * removed. So level 16 and 17 now contain only and all good solutions. From the good solutions at level 17
 * I can get to all good and some bad solutions at level 18. Intersecting level 18 with the complement of
 * level 15 produces a good level 18, and its complement is a good level 15.
 * So the algorithm from level 17 onward is:
 * - produce level n
 * - remove the elements that are not in the complement of level (33-n)
 * - remove from level (33-n) the elements that are not in the complement of level n
 */

/*
 * Trim level 16 by keeping only the positions from which a complementary position in level 16
 * can be reached
 */


void removeHalfPositionsThatCannotReachOwnComplement(int level, bool centreFull) {
	// TODO HERE
	cout << "removing positions of mid level " << level << " " << centreFull <<  endl;
    FILE * sourceFile = fopen(getName(level, centreFull, false), modeOpenReadBinary);
    FILE * complementSourceFile = fopen(getName(level, !centreFull, false), modeOpenReadBinary);
    FILE * destinationFile = fopen(getName(level, centreFull, true), modeCreateWriteBinary);
    uint32_t sourceSize = 0;
    uint32_t destinationSize = 0;
    while (1) {
    	uint32_t position;
    	if (fread (&position, sizeof(uint32_t), 1, sourceFile) != 1)
    		break;
    	sourceSize++;
		// for all moves
	    // if move is possible
			// calculate the resulting position
			// complement the resulting position
	        // if resulting position is found in complementary file
	               // add the position to the destination file
	              // break
    }
    fclose(sourceFile);
    fclose(complementSourceFile);
    fclose(destinationFile);
	cout << "level " << level << (centreFull ? " full" : " empty") << " reduced from " << sourceSize << " to " << destinationSize << endl;
#if 0
				  char tempName[L_tmpnam];
				  FILE * fr = fopen(fileName, modeOpenReadBinary);
				  tmpnam(tempName);
				  FILE * fw = fopen(tempName, modeCreateWriteBinary);
				  // read and write the first unsigned
				  uint32_t lv;
				  uint32_t ucount = 0;
				  if (fread (&lv, sizeof(uint32_t), 1, fr) == 1) {
				      fwrite(&lv, sizeof(uint32_t), 1, fw);
				      ucount++;
				  }
				  while (1) {
				    // read a buffer worth
				    int sbufc = fread (sbuf, sizeof(uint32_t), bufSize, fr);
				    if (sbufc <= 0)
				      break;
				    // scan the buffer for unique values in the buffer
				    int sbufr = 0;
				    int sbufw = 0;
				    while (sbufr < sbufc) {
				      uint32_t v = sbuf[sbufr++];
				      if (v < lv) {
				        cout << "error: v=" << v << "; lv=" << lv << endl;  // out of sequence
				        return ucount;
				      }
				      if (v > lv)
				        lv = sbuf[sbufw++] = v;
				    }
				    // write out the unique values left in the buffer
				    if (sbufw > 0) {
				      fwrite(sbuf, sizeof(uint32_t), sbufw, fw);
				      ucount += sbufw;
				    }
				  }
				  fclose(fr);
				  fclose(fw);
				  remove (fileName);
				  rename (tempName, fileName);
				  return ucount;
#endif
}


void removePositionsThatCannotReachOwnComplement(int level) {
	removeHalfPositionsThatCannotReachOwnComplement(level, true);
	removeHalfPositionsThatCannotReachOwnComplement(level, false);
}

void removeHalfPositionsThatCannotReachNextLevel(int level, bool centreFull) {
	// TODO HERE
	cout << "removing positions of level " << level << " " << centreFull <<  endl;
}

void removePositionsThatCannotReachNextLevel(int level) {
	removeHalfPositionsThatCannotReachNextLevel(level, true);
	removeHalfPositionsThatCannotReachNextLevel(level, false);
}

void findForwardAndBackwardRichablePositions(int middleLevel) {
	for (int level=middleLevel; level>=1; level--) {
		if (level == middleLevel) {
			removePositionsThatCannotReachOwnComplement(level);
		} else {
			removePositionsThatCannotReachNextLevel(level);
		}
	}
}
