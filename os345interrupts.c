// os345interrupts.c - pollInterrupts	08/08/2013
// ***********************************************************************
// **   DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER   **
// **                                                                   **
// ** The code given here is the basis for the BYU CS345 projects.      **
// ** It comes "as is" and "unwarranted."  As such, when you use part   **
// ** or all of the code, it becomes "yours" and you are responsible to **
// ** understand any algorithm or method presented.  Likewise, any      **
// ** errors or problems become your responsibility to fix.             **
// **                                                                   **
// ** NOTES:                                                            **
// ** -Comments beginning with "// ??" may require some implementation. **
// ** -Tab stops are set at every 3 spaces.                             **
// ** -The function API's in "OS345.h" should not be altered.           **
// **                                                                   **
// **   DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER ** DISCLAMER   **
// ***********************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <time.h>
#include <assert.h>

#include "os345.h"
#include "os345config.h"
#include "os345signals.h"

// **********************************************************************
//	local prototypes
//
void pollInterrupts(void);
static void keyboard_isr(void);
static void timer_isr(void);

int arrowKeyBegan;

// **********************************************************************
// **********************************************************************
// global semaphores

//extern Semaphore* keyboard;				// keyboard semaphore
extern Semaphore* charReady;				// character has been entered
extern Semaphore* inBufferReady;			// input buffer ready semaphore

extern Semaphore* tics10sec;				// 10 second semaphore
extern Semaphore* tics1sec;					// 1 second semaphore
extern Semaphore* tics10thsec;				// 1/10 second semaphore

extern char** lineHistory;
extern int nextLine;
extern int currentLine;

extern char inChar;				// last entered character
extern int charFlag;				// 0 => buffered input
extern int inBufIndx;				// input pointer into input buffer
extern char inBuffer[INBUF_SIZE + 1];	// character input buffer

extern time_t oldTime10;				// old 10sec time
extern time_t oldTime1;					// old 1sec time
extern clock_t myClkTime;
extern clock_t myOldClkTime;

extern int pollClock;				// current clock()
extern int lastPollClock;			// last pollClock

extern int superMode;						// system mode

// **********************************************************************
// **********************************************************************
// simulate asynchronous interrupts by polling events during idle loop
//
void pollInterrupts(void) {
	// check for task monopoly
	pollClock = clock();
	assert("Timeout" && ((pollClock - lastPollClock) < MAX_CYCLES));
	lastPollClock = pollClock;

	// check for keyboard interrupt
	if ((inChar = GET_CHAR) > 0) {
		keyboard_isr();
	}

	// timer interrupt
	timer_isr();

	return;
} // end pollInterrupts

// **********************************************************************
// keyboard interrupt service routine
//
static void keyboard_isr() {
	// assert system mode
	assert("keyboard_isr Error" && superMode);

	semSignal(charReady);					// SIGNAL(charReady) (No Swap)
	if (charFlag == 0) {
		switch (inChar) {
		case '\r':
		case '\n': {
			printf("%c", inChar);
			inBufIndx = 0;				// EOL, signal line ready
			semSignal(inBufferReady);	// SIGNAL(inBufferReady)
			break;
		}
		case 0x12:						// ^r
		{
			clearSignal(-1, mySIGTSTP);
			clearSignal(-1, mySIGSTOP);
			sigSignal(-1, mySIGCONT);	// resume all tasks
			break;
		}
		case 0x17:						// ^w
		{
			sigSignal(-1, mySIGTSTP);	// pause all tasks
			break;
		}
		case 0x18:						// ^x
		{
			inBufIndx = 0;
			inBuffer[0] = 0;
			sigSignal(0, mySIGINT);		// halt all tasks
			semSignal(inBufferReady);	// SEM_SIGNAL(inBufferReady)
			break;
		}
		case 0x7f:						// backspacke
		{
			if (inBufIndx > 0) {
				printf("\b \b");
				inBuffer[inBufIndx--] = '\0';
			}
			break;
		}
		case 0x1b:						// Arrow key first char
		{
			arrowKeyBegan = 1;
			break;
		}
		case 0x5b:						// Arrow key second char
		{
			if (arrowKeyBegan == 1)
				arrowKeyBegan = 2;
			break;
		}
		case 'A':						// up
		{
			if (arrowKeyBegan == 2) {
				if (currentLine > 0) {
					strcpy(inBuffer, lineHistory[--currentLine]);
					inBufIndx = strlen(inBuffer);
					for (int i=0; i<50; i++)
						printf(" ");
					for (int i=0; i<100; i++)
						printf("\b \b");
					printf("%s", inBuffer);
				}
				arrowKeyBegan = 0;
				break;
			}
		}
		case 'B':						// down
		{
			if (arrowKeyBegan == 2) {
				if (currentLine < nextLine-1) {
					strcpy(inBuffer, lineHistory[++currentLine]);
					inBufIndx = strlen(inBuffer);
					for (int i=0; i<50; i++)
						printf(" ");
					for (int i=0; i<100; i++)
						printf("\b \b");
					printf("%s", inBuffer);
				} else if (currentLine < nextLine) {
					strcpy(inBuffer, "\n");
					inBufIndx = 0;
					for (int i=0; i<50; i++)
						printf(" ");
					for (int i=0; i<100; i++)
						printf("\b \b");
					printf("%s", inBuffer);
				}
				arrowKeyBegan = 0;
				break;
			}
		}
		default: {
			arrowKeyBegan = 0;
			if (inBufIndx < INBUF_SIZE - 1) {
				//printf("\n  hex(%x)   char(%c)", inChar, inChar);
				printf("%c", inChar);
				inBuffer[inBufIndx++] = inChar;
				inBuffer[inBufIndx] = 0;
			} else {
				printf("\a");
			}
		}
		}
	} else {
		// single character mode
		inBufIndx = 0;
		inBuffer[inBufIndx] = 0;
	}
	return;
} // end keyboard_isr

// **********************************************************************
// timer interrupt service routine
//
static void timer_isr() {
	time_t currentTime;						// current time

	// assert system mode
	assert("timer_isr Error" && superMode);

	// capture current time
	time(&currentTime);

	// one second timer
	if ((currentTime - oldTime1) >= ONE_SECOND) {
		// signal 1 second
		semSignal(tics1sec);
		oldTime1 += 1;
	}

	// ten second timer
	if ((currentTime - oldTime10) >= TEN_SECONDS) {
		// signal 10 second
		semSignal(tics10sec);
		oldTime10 += 10;
	}

	// sample fine clock
	myClkTime = clock();
	if ((myClkTime - myOldClkTime) >= ONE_TENTH_SEC) {
		myOldClkTime = myOldClkTime + ONE_TENTH_SEC;   // update old
		semSignal(tics10thsec);
	}

	return;
} // end timer_isr
