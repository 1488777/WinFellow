/*============================================================================*/
/* Fellow Amiga Emulator                                                      */
/* Joystick and Mouse                                                         */
/*                                                                            */
/* Author: Petter Schau (peschau@online.no)                                   */
/*                                                                            */
/* This file is under the GNU Public License (GPL)                            */
/*============================================================================*/

#include "portable.h"
#include "renaming.h"

#include "defs.h"
#include "fellow.h"
#include "draw.h"
#include "fmem.h"
#include "gameport.h"
#include "mousedrv.h"

/*===========================================================================*/
/* IO-Registers                                                              */
/*===========================================================================*/

ULO potgor, potdat[2];


/*===========================================================================*/
/* Gameport state                                                            */
/*===========================================================================*/

BOOLE gameport_fire0[2], gameport_fire1[2];
BOOLE gameport_left[2], gameport_right[2], gameport_up[2], gameport_down[2];
LON gameport_x[2], gameport_y[2];
LON gameport_x_last_read[2], gameport_y_last_read[2];
BOOLE gameport_mouse_first_time[2];


/*===========================================================================*/
/* Configuration information                                                 */
/*===========================================================================*/

gameport_inputs gameport_input[2];

void gameportSetInput(ULO index, gameport_inputs gameportinput) {
  gameport_input[index] = gameportinput;
}


/*===========================================================================*/
/* IO-Registers                                                              */
/*===========================================================================*/

/*===========================================================================*/
/* General joydat calculation                                                */
/* This routine will try to avoid overflow in the mouse counters             */
/*===========================================================================*/

static ULO rjoydat(ULO i) {
  if (gameport_input[i] == GP_MOUSE0) { /* Gameport input is mouse */
    LON diffx, diffy;

    diffx = gameport_x[i] - gameport_x_last_read[i];
    diffy = gameport_y[i] - gameport_y_last_read[i];
    if (diffx > 127)
      diffx = 127;  /* Find relative movement */
    else if (diffx < -127)
      diffx = -127;
    if (diffy > 127)
      diffy = 127;
    else if (diffy < -127)
      diffy = -127;
    gameport_x_last_read[i] += diffx;
    gameport_y_last_read[i] += diffy;
    
    return ((gameport_y_last_read[i] & 0xff)<<8) | (gameport_x_last_read[i] & 0xff);
  }
  else { /* Gameport input is of joystick type */
    ULO retval = 0;

    if (gameport_right[i])
      retval |= 3;

    if (gameport_left[i])
      retval |= 0x300;

    if (gameport_up[i])
      retval ^= 0x100;

    if (gameport_down[i])
      retval ^= 1;

    return retval;
  }
}

/* JOY0DATR - $A */

ULO rjoy0datC(ULO address) {
  return rjoydat(0);
}

/* JOY1DATR - $C */

ULO rjoy1datC(ULO address) {
  return rjoydat(1);
}

/* POT0DATR - $12 */

ULO rpot0datC(ULO address) {
  return potdat[0];
}

/* POT1DATR - $14 */

ULO rpot1datC(ULO address) {
  return potdat[1];
}

/* POTGOR - $16 */

ULO rpotgorC(ULO address) {
  ULO val = potgor & 0xbbff;

  if (!gameport_fire1[0])
    val |= 0x400;
  if (!gameport_fire1[1]) val |= 0x4000;
  return val;
}

/* JOYTEST $36 */

void wjoytestC(ULO data, ULO address) {
  ULO i;

  for (i = 0; i < 2; i++) {
    gameport_x[i] = gameport_x_last_read[i] = (BYT) (data & 0xff);
    gameport_y[i] = gameport_y_last_read[i] = (BYT) ((data>>8) & 0xff);
  }
}


/*===========================================================================*/
/* Mouse movement handler                                                    */
/* Called by mousedrv.c whenever a change occurs                             */
/* The input coordinates are used raw. There can be a granularity problem.   */
/*                                                                           */
/* Parameters:                                                               */
/* mouseno                   - mouse 0 or mouse 1                            */
/* x, y                      - New absolute position of mouse                */
/* button1, button2, button3 - State of the mouse buttons, button2 not used  */
/*===========================================================================*/

void gameportMouseHandler(gameport_inputs mousedev,
			  LON x,
			  LON y,
			  BOOLE button1,
			  BOOLE button2,
			  BOOLE button3) {
  ULO i;

  char szMsg[255];

  sprintf( szMsg, "mouse %d %d\n", x, y );
  fellowAddLog( szMsg );

  for (i = 0; i < 2; i++) {
    if (gameport_input[i] == mousedev) {
      if ((!gameport_fire1[i]) && button3)
  	potdat[i] = (potdat[i] + 0x100) & 0xffff; 
      gameport_fire0[i] = button1;
      gameport_fire1[i] = button3;
      gameport_x[i] += x;
      gameport_y[i] += y;
    }
  }
}

// it is not used by anyone, it should be called to emulate an analog joystick
// but we have decided to not support them anymore
// so it will be commented out

/*
void gameportJoyHandler(LON dx, LON dy, BOOLE button1, BOOLE button2, BOOLE button3) {
  ULO i;

  for (i = 0; i < 2; i++) {
	switch(gameport_input[i])
	{
	case GP_JOYKEY0:
	case GP_JOYKEY1:
	case GP_ANALOG0:
	case GP_ANALOG1:
      if ((!gameport_fire1[i]) && button2)
  	    potdat[i] = (potdat[i] + 0x100) & 0xffff; 
      gameport_fire0[i] = button1;
      gameport_fire1[i] = button2;
      gameport_x[i] += dx;
      gameport_y[i] += dy;
	  break;
    }
  }
}
*/

/*===========================================================================*/
/* Joystick movement handler                                                 */
/* Called by joydrv.c or kbddrv.c whenever a change occurs                   */
/*                                                                           */
/* Parameters:                                                               */
/* left, up, right, down     - New state of the joystick                     */
/* button1, button2, button3 - State of the joystick buttons                 */
/*===========================================================================*/

void gameportJoystickHandler(gameport_inputs joydev,
			     BOOLE left,
			     BOOLE up,
			     BOOLE right,
			     BOOLE down,
			     BOOLE button1,
			     BOOLE button2) {
	ULO i;
	
	for (i = 0; i < 2; i++)
		if (gameport_input[i] == joydev)
		{
			if ((!gameport_fire1[i]) && button2)
				potdat[i] = (potdat[i] + 0x100) & 0xffff; 
			
			gameport_fire0[i] = button1;
			gameport_fire1[i] = button2;
			gameport_left[i] = left;
			gameport_up[i] = up;
			gameport_right[i] = right;
			gameport_down[i] = down;
		}
}


/*===========================================================================*/
/* Initialize the register stubs for the gameports                           */
/*===========================================================================*/

void gameportIOHandlersInstall(void) {
  memorySetIOReadStub(0xa, rjoy0dat);
  memorySetIOReadStub(0xc, rjoy1dat);
  memorySetIOReadStub(0x12, rpot0dat);
  memorySetIOReadStub(0x14, rpot1dat);
  memorySetIOReadStub(0x16, rpotgor);
  memorySetIOWriteStub(0x36, wjoytest);
}


/*===========================================================================*/
/* Clear all gameport state                                                  */
/*===========================================================================*/

void gameportIORegistersClear(BOOLE clear_pot) {
  ULO i;

  if (clear_pot) potgor = 0xffff;
  for (i = 0; i < 2; i++) {
    if (clear_pot) potdat[i] = 0;
    gameport_fire0[i] = FALSE;
    gameport_fire1[i] = FALSE;
    gameport_left[i] = FALSE;
    gameport_right[i] = FALSE;
    gameport_up[i] = FALSE;
    gameport_down[i] = FALSE;
    gameport_x[i] = 0;
    gameport_y[i] = 0;
    gameport_y_last_read[i] = 0;
    gameport_y_last_read[i] = 0;
    gameport_mouse_first_time[i] = FALSE;
  }
}


/*===========================================================================*/
/* Standard Fellow module functions                                          */
/*===========================================================================*/

void gameportHardReset(void) {
  gameportIORegistersClear(TRUE);
  mouseDrvHardReset();
  joyDrvHardReset();
}

void gameportEmulationStart(void) {
  gameportIOHandlersInstall();
  gameport_input[0] = GP_MOUSE0;
  fellowAddLog("gameportEmulationStart()\n");
  mouseDrvEmulationStart();
  joyDrvEmulationStart();
  gameportIORegistersClear(FALSE);
}

void gameportEmulationStop(void) {
  joyDrvEmulationStop();
  mouseDrvEmulationStop();
}

void gameportStartup(void) {
  gameportIORegistersClear(TRUE);
  mouseDrvStartup();
  joyDrvStartup();

  // -- nova --
  // this is only an initial settings, they will be overrided
  // by the Game port configuration section of cfgManagerConfigurationActivate

  gameportSetInput(0, GP_MOUSE0);
  gameportSetInput(1, GP_NONE);
}

void gameportShutdown(void) {
  joyDrvShutdown();
  mouseDrvShutdown();
}
