//============================================================================
// Name        : pegSolitaire.cpp
// Author      : Vittorio Mojoli
// Version     :
// Copyright   :
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <iostream>
#include <stdlib.h>
#include <stdint.h>
using namespace std;

extern void prepareAllMoves ();
void findForwardReachablePositions(int finalLevel, bool show);
extern void retraceSteps(bool full, int level, uint32_t value);
extern void findForwardAndBackwardRichablePositions(int level);

static const int FINAL_LEVEL = 32;
static const int MID_LEVEL = 16;

#if 0
int main() {
	cout << "Welcome to Peg Solitaire" << endl;
	return 0;
}
#endif

int main (int argc, char ** args) {
  int level;
  if (argc < 2)
    level = FINAL_LEVEL;
  else
    level = atoi(args[1]);
  bool show;
  if (argc < 3)
    show = false;
  else
    show = (args[2][0] == 'v');
#if 0
  prepareAllMoves();
  if (argc < 3 || args[2][0] != 'e')
    showLongFile(getName(level, true), true);
  if (argc < 3 || args[2][0] != 'f')
    showLongFile(getName(level, false), false);#if 1
    bool show;
    if (argc < 3)
      show = false;
    else
      show = (args[2][0] == 'v');
    findForwardReachablePositions (level, show);
    // todo why is this called here?
    if (level == FINAL_LEVEL) {
  	  findForwardAndBackwardRichablePositions();
    }
#endif

#if 0
  retraceSteps(true,level,0);
#endif
#if 0
  findForwardReachablePositions (MID_LEVEL, false);
#endif
  findForwardAndBackwardRichablePositions(MID_LEVEL);
  return 0;
}

