/* @(#) $Id: COPPER.C,v 1.6 2009-07-26 22:56:07 peschau Exp $ */
/*=========================================================================*/
/* Fellow                                                                  */
/* Copper Emulation Initialization                                         */
/*                                                                         */
/* Author: Petter Schau                                                    */
/*                                                                         */
/*                                                                         */
/* Copyright (C) 1991, 1992, 1996 Free Software Foundation, Inc.           */
/*                                                                         */
/* This program is free software; you can redistribute it and/or modify    */
/* it under the terms of the GNU General Public License as published by    */
/* the Free Software Foundation; either version 2, or (at your option)     */
/* any later version.                                                      */
/*                                                                         */
/* This program is distributed in the hope that it will be useful,         */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of          */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           */
/* GNU General Public License for more details.                            */
/*                                                                         */
/* You should have received a copy of the GNU General Public License       */
/* along with this program; if not, write to the Free Software Foundation, */
/* Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.          */
/*=========================================================================*/

#ifdef _FELLOW_DEBUG_CRT_MALLOC
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#ifndef DBG_NEW
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#define new DBG_NEW
#endif
#endif

#include "DEFS.H"
#include "DRAW.H"
#include "CopperRegisters.h"
#include "LineExactCopper.h"
#include "CycleExactCopper.h"

Copper *copper = nullptr;

void copperInitializeFromEmulationMode()
{
  if (copper != nullptr)
  {
    delete copper;
    copper = nullptr;
  }
  if (drawGetGraphicsEmulationMode() == GRAPHICSEMULATIONMODE_CYCLEEXACT)
  {
    copper = new CycleExactCopper();
  }
  else
  {
    copper = new LineExactCopper();
  }
}

void copperEventHandler()
{
  copper->EventHandler();
}

void copperSaveState(FILE *F)
{
  copper_registers.SaveState(F);
}

void copperLoadState(FILE *F)
{
  copper_registers.LoadState(F);
}

void copperEndOfFrame()
{
  copper->EndOfFrame();
}

void copperEmulationStart()
{
  copper_registers.InstallIOHandlers();
  copper->EmulationStart();
}

void copperEmulationStop()
{
  copper->EmulationStop();
}

void copperHardReset()
{
  copper_registers.ClearState();
  copper->HardReset();
}

void copperStartup()
{
  copper_registers.ClearState();
  copperInitializeFromEmulationMode();
}

void copperShutdown()
{
  if (copper != nullptr)
  {
    delete copper;
    copper = nullptr;
  }
}
