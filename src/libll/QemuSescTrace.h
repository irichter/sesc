/* 
   SESC: Super ESCalar simulator
   Copyright (C) 2005 University California, Santa Cruz.

   Contributed by Saangetha
                  Keertika
		  Jose Renau

This file is part of SESC.

SESC is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation;
either version 2, or (at your option) any later version.

SESC is    distributed in the  hope that  it will  be  useful, but  WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should  have received a copy of  the GNU General  Public License along with
SESC; see the file COPYING.  If not, write to the  Free Software Foundation, 59
Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#ifndef QEMU_SESC_TRACE
#define QEMU_SESC_TRACE
#include <stdio.h>

typedef struct {
  unsigned int pc:32;    
  unsigned int npc:32;
  unsigned int src1:32;
  unsigned int src2:32;
  unsigned int dest:32;
  int opc;
  int type;
  int subtype;
} QemuSescTrace;

// defining the getter functions

void setpc(QemuSescTrace *qst, unsigned int pc);
void setopc(QemuSescTrace *qst, int opc);
void setsrc1(QemuSescTrace *qst, unsigned int src1);
void setsrc2(QemuSescTrace *qst, unsigned int src2);
void setdest(QemuSescTrace *qst, unsigned int dest);

unsigned int getpc(QemuSescTrace *qst);
int getopc(QemuSescTrace *qst);
unsigned int getsrc1(QemuSescTrace *qst);
unsigned int getsrc2(QemuSescTrace *qst);
unsigned int getdest(QemuSescTrace *qst);
unsigned int getNextPC(QemuSescTrace *qst,unsigned int);

//defining trace file functions

FILE *openFile(const char *logfilename );// getthe name of the file from the trace argument

void writeToFile(FILE* fd, QemuSescTrace *x);
QemuSescTrace* readFromFile();
void closeFile(FILE *fd);

#endif
