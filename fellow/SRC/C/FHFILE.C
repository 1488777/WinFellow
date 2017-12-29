/*=========================================================================*/
/* Fellow                                                                  */
/*                                                                         */
/* Hardfile device                                                         */
/*                                                                         */
/* Authors: Petter Schau                                                   */
/*          Torsten Enderling (carfesh@gmx.net)                            */
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

#include "defs.h"
#include "fellow.h"
#include "fhfile.h"
#include "fmem.h"
#include "draw.h"
#include "CpuModule.h"
#include "fswrap.h"

#ifdef RETRO_PLATFORM
#include "RetroPlatform.h"
#endif

#include <list>
#include <vector>

/*====================*/
/* fhfile.device      */
/*====================*/


/*===================================*/
/* Fixed locations in fhfile_rom:    */
/* ------------------------------    */
/* offset 4088 - max number of units */
/* offset 4092 - configdev pointer   */
/*===================================*/


/*===================================================================*/
/* The hardfile device is at least in spirit based on ideas found in */
/* the sources below:                                                */
/* - The device driver source found on Fish 39                       */
/*   and other early Fish-disks                                      */
/* - UAE was useful for test and compare when the device was written */
/*   and didn't work                                                 */
/*===================================================================*/


fhfile_dev fhfile_devs[FHFILE_MAX_DEVICES];
std::vector<RDBFilesystemHeader*> fhfile_rdb_filesystems;
ULO fhfile_romstart;
ULO fhfile_bootcode;
ULO fhfile_configdev;
UBY fhfile_rom[65536];

/*============================================================================*/
/* Configuration properties                                                   */
/*============================================================================*/

BOOLE fhfile_enabled;

static BOOLE fhfileHasZeroDevices(void) {
  ULO i;
  ULO dev_count = 0;

  for (i = 0; i < FHFILE_MAX_DEVICES; i++) if (fhfile_devs[i].F != NULL) dev_count++;
  return (dev_count == 0);
}

void fhfileSetPhysicalGeometryFromRigidDiskBlock(fhfile_dev *fhfile)
{
  fhfile->bytespersector = fhfile->rdb->BlockSize;
  fhfile->bytespersector_original = fhfile->rdb->BlockSize;
  fhfile->sectorspertrack = fhfile->rdb->SectorsPerTrack * fhfile->rdb->Heads;
  fhfile->surfaces = fhfile->rdb->Heads;
  fhfile->tracks = fhfile->rdb->Cylinders;
  fhfile->reservedblocks = 1;
  fhfile->reservedblocks_original = 1;
  fhfile->lowCylinder = fhfile->rdb->LowCylinder;
  fhfile->highCylinder = fhfile->rdb->HighCylinder;
}

static void fhfileInitializeHardfile(ULO index) {
  ULO size;
  fs_navig_point *fsnp;

  if (fhfile_devs[index].F != NULL)                     /* Close old hardfile */
  {
    fclose(fhfile_devs[index].F);
  }
  fhfile_devs[index].F = NULL;                           /* Set config values */
  fhfile_devs[index].status = FHFILE_NONE;
  if ((fsnp = fsWrapMakePoint(fhfile_devs[index].filename)) != NULL)
  {
    fhfile_devs[index].readonly |= (!fsnp->writeable);
    size = fsnp->size;
    fhfile_devs[index].F = fopen(fhfile_devs[index].filename, (fhfile_devs[index].readonly) ? "rb" : "r+b");

    if (fhfile_devs[index].F != NULL)                          /* Open file */
    {
      fhfile_devs[index].hasRigidDiskBlock = RDBHandler::HasRigidDiskBlock(fhfile_devs[index].F);

      if (fhfile_devs[index].hasRigidDiskBlock)
      {
        // RDB configured hardfile
        fhfile_devs[index].rdb = RDBHandler::GetDriveInformation(fhfile_devs[index].F);
        fhfileSetPhysicalGeometryFromRigidDiskBlock(&fhfile_devs[index]);
        fhfile_devs[index].size = size;
        fhfile_devs[index].status = FHFILE_HDF;
      }
      else
      {
        // Manually configured hardfile
        ULO track_size = (fhfile_devs[index].sectorspertrack * fhfile_devs[index].surfaces * fhfile_devs[index].bytespersector);
        if (size < track_size)
        {
          /* Error: File must be at least one track long */
          fclose(fhfile_devs[index].F);
          fhfile_devs[index].F = NULL;
          fhfile_devs[index].status = FHFILE_NONE;
        }
        else                                                    /* File is OK */
        {
          fhfile_devs[index].tracks = size / track_size;
          fhfile_devs[index].size = fhfile_devs[index].tracks * track_size;
          fhfile_devs[index].status = FHFILE_HDF;
        }
      }
    }
    free(fsnp);
  }
}

/* Returns TRUE if a hardfile was inserted */

BOOLE fhfileRemoveHardfile(ULO index) {
  BOOLE result = FALSE;
  if (index >= FHFILE_MAX_DEVICES) return result;
  if (fhfile_devs[index].F != NULL) {
    fflush(fhfile_devs[index].F);
    fclose(fhfile_devs[index].F);
    result = TRUE;
  }
  if (fhfile_devs[index].rdb != nullptr)
  {
    delete fhfile_devs[index].rdb;
    fhfile_devs[index].rdb = nullptr;
  }
  memset(&(fhfile_devs[index]), 0, sizeof(fhfile_dev));
  fhfile_devs[index].status = FHFILE_NONE;
  return result;
}


void fhfileSetEnabled(BOOLE enabled) {
  fhfile_enabled = enabled;
}


BOOLE fhfileGetEnabled(void) {
  return fhfile_enabled;
}


void fhfileSetHardfile(fhfile_dev hardfile, ULO index) {
  if (index >= FHFILE_MAX_DEVICES) return;
  fhfileRemoveHardfile(index);
  strncpy(fhfile_devs[index].filename, hardfile.filename, CFG_FILENAME_LENGTH);
  fhfile_devs[index].readonly = hardfile.readonly_original;
  fhfile_devs[index].readonly_original = hardfile.readonly_original;
  fhfile_devs[index].bytespersector = (hardfile.bytespersector_original & 0xfffffffc);
  fhfile_devs[index].bytespersector_original = hardfile.bytespersector_original;
  fhfile_devs[index].sectorspertrack = hardfile.sectorspertrack;
  fhfile_devs[index].surfaces = hardfile.surfaces;
  fhfile_devs[index].reservedblocks = hardfile.reservedblocks_original;
  fhfile_devs[index].reservedblocks_original = hardfile.reservedblocks_original;
  if (fhfile_devs[index].reservedblocks < 1)
    fhfile_devs[index].reservedblocks = 1;
  fhfileInitializeHardfile(index);

#ifdef RETRO_PLATFORM
  if(RP.GetHeadlessMode())
    RP.SendHardDriveContent(index, hardfile.filename, hardfile.readonly_original ? true : false);
#endif
}


BOOLE fhfileCompareHardfile(fhfile_dev hardfile, ULO index) {
  if (index >= FHFILE_MAX_DEVICES) return FALSE;
  return (fhfile_devs[index].readonly_original == hardfile.readonly_original) &&
    (fhfile_devs[index].bytespersector_original == hardfile.bytespersector_original) &&
    (fhfile_devs[index].sectorspertrack == hardfile.sectorspertrack) &&
    (fhfile_devs[index].surfaces == hardfile.surfaces) &&
    (fhfile_devs[index].reservedblocks_original == hardfile.reservedblocks_original) &&
    (strncmp(fhfile_devs[index].filename, hardfile.filename, CFG_FILENAME_LENGTH) == 0);
}


void fhfileClear(void) {
  ULO i;

  for (i = 0; i < FHFILE_MAX_DEVICES; i++) fhfileRemoveHardfile(i);
}


/*===================*/
/* Set HD led symbol */
/*===================*/

static void fhfileSetLed(bool state)
{
  drawSetLED(4, state);
}


/*==================*/
/* BeginIO Commands */
/*==================*/

static void fhfileIgnore(ULO index) {
  memoryWriteLong(0, cpuGetAReg(1) + 32);
  cpuSetDReg(0, 0);
}

static BYT fhfileRead(ULO index)
{
  ULO dest = memoryReadLong(cpuGetAReg(1) + 40);
  ULO offset = memoryReadLong(cpuGetAReg(1) + 44);
  ULO length = memoryReadLong(cpuGetAReg(1) + 36);

  if ((offset + length) > fhfile_devs[index].size)
  {
    return -3;
  }
  fhfileSetLed(true);
#ifdef RETRO_PLATFORM
  if(RP.GetHeadlessMode())
     RP.PostHardDriveLED(index, true, false);
#endif
  fseek(fhfile_devs[index].F, offset, SEEK_SET);
  fread(memoryAddressToPtr(dest), 1, length, fhfile_devs[index].F);
  memoryWriteLong(length, cpuGetAReg(1) + 32);
  fhfileSetLed(false);
#ifdef RETRO_PLATFORM
  if(RP.GetHeadlessMode())
     RP.PostHardDriveLED(index, false, false);
#endif
  return 0;
}

static BYT fhfileWrite(ULO index)
{
  ULO dest = memoryReadLong(cpuGetAReg(1) + 40);
  ULO offset = memoryReadLong(cpuGetAReg(1) + 44);
  ULO length = memoryReadLong(cpuGetAReg(1) + 36);

  if (fhfile_devs[index].readonly ||
    ((offset + length) > fhfile_devs[index].size))
  {
    return -3;
  }
  fhfileSetLed(true);
#ifdef RETRO_PLATFORM
  if(RP.GetHeadlessMode())
     RP.PostHardDriveLED(index, true, true);
#endif
  fseek(fhfile_devs[index].F, offset, SEEK_SET);
  fwrite(memoryAddressToPtr(dest),1, length, fhfile_devs[index].F);
  memoryWriteLong(length, cpuGetAReg(1) + 32);
  fhfileSetLed(false);
 #ifdef RETRO_PLATFORM
  if(RP.GetHeadlessMode())
     RP.PostHardDriveLED(index, false, true);
#endif
  return 0;
}

static void fhfileGetNumberOfTracks(ULO index) {
  memoryWriteLong(fhfile_devs[index].tracks, cpuGetAReg(1) + 32);
}

static void fhfileGetDriveType(ULO index) {
  memoryWriteLong(1, cpuGetAReg(1) + 32);
}

static void fhfileWriteProt(ULO index) {
  memoryWriteLong(fhfile_devs[index].readonly, cpuGetAReg(1) + 32);
}

/*======================================================*/
/* fhfileDiag native callback                           */
/*                                                      */
/* Pointer to our configdev struct is stored in $f40ffc */
/* For later use when filling out the bootnode          */
/*======================================================*/

void fhfileDoDiag()
{
  fhfile_configdev = cpuGetAReg(3);
  memoryDmemSetLongNoCounter(FHFILE_MAX_DEVICES, 4088);
  memoryDmemSetLongNoCounter(fhfile_configdev, 4092);
  cpuSetDReg(0, 1);
}

/*======================================*/
/* Native callbacks for device commands */
/*======================================*/

void fhfileDoOpen()
{
  if (cpuGetDReg(0) < FHFILE_MAX_DEVICES)
  {
    memoryWriteByte(7, cpuGetAReg(1) + 8);                     /* ln_type (NT_REPLYMSG) */
    memoryWriteByte(0, cpuGetAReg(1) + 31);                    /* io_error */
    memoryWriteLong(cpuGetDReg(0), cpuGetAReg(1) + 24);                 /* io_unit */
    memoryWriteLong(memoryReadLong(cpuGetAReg(6) + 32) + 1, cpuGetAReg(6) + 32);  /* LIB_OPENCNT */
    cpuSetDReg(0, 0);                              /* ? */
  }
  else
  {
    memoryWriteLong(-1, cpuGetAReg(1) + 20);            
    memoryWriteByte(-1, cpuGetAReg(1) + 31);                   /* io_error */
    cpuSetDReg(0, -1);                             /* ? */
  }
}

void fhfileDoClose()
{
  memoryWriteLong(memoryReadLong(cpuGetAReg(6) + 32) - 1, cpuGetAReg(6) + 32);    /* LIB_OPENCNT */
  cpuSetDReg(0, 0);                                /* ? */
}

void fhfileDoExpunge()
{
  cpuSetDReg(0, 0);                                /* ? */
}

void fhfileDoNULL()
{
}

void fhfileDoBeginIO()
{
  BYT error = 0;
  ULO unit = memoryReadLong(cpuGetAReg(1) + 24);

  UWO cmd = memoryReadWord(cpuGetAReg(1) + 28);
  switch (cmd)
  {
    case 2:
      error = fhfileRead(unit);
      break;
    case 3:
    case 11:
      error = fhfileWrite(unit);
      break;
    case 18:
      fhfileGetDriveType(unit);
      break;
    case 19:
      fhfileGetNumberOfTracks(unit);
      break;
    case 15:
      fhfileWriteProt(unit);
      break;
    case 4:
    case 5:
    case 9:
    case 10:
    case 12:
    case 13:
    case 14:
    case 20:
    case 21:
      fhfileIgnore(unit);
      break;
    default:
      error = -3;
      cpuSetDReg(0, 0);
      break;
  }
  memoryWriteByte(5, cpuGetAReg(1) + 8);      /* ln_type */
  memoryWriteByte(error, cpuGetAReg(1) + 31); /* ln_error */
}

void fhfileDoAbortIO()
{
  cpuSetDReg(0, -3);
}

// RDB support functions, native callbacks

// 1 - Returns the number of RDB filesystem headers in D0
void fhfileDoGetFileSystemCount()
{
  ULO filesystemCount = fhfile_rdb_filesystems.size();

  fellowAddLog("fhfileDoGetFilesystemCount() - Returns %d\n", filesystemCount);

  cpuSetDReg(0, filesystemCount);
}

void fhfileDoGetFileSystemHunkCount()
{
  ULO fsIndex = cpuGetDReg(1);
  ULO hunkCount = fhfile_rdb_filesystems[fsIndex]->FilesystemHandler.Hunks.size();
  cpuSetDReg(0, hunkCount);
}

void fhfileDoGetFileSystemHunkSize()
{
  ULO fsIndex = cpuGetDReg(1);
  ULO hunkIndex = cpuGetDReg(2);
  ULO hunkSize = fhfile_rdb_filesystems[fsIndex]->FilesystemHandler.Hunks[hunkIndex]->GetSizeInLongwords() * 4;
  cpuSetDReg(0, hunkSize);
}

void fhfileDoRelocateHunk()
{
  ULO destination = cpuGetDReg(0);
  ULO fsIndex = cpuGetDReg(1);
  ULO hunkIndex = cpuGetDReg(2);

  UBY *relocatedHunkData = fhfile_rdb_filesystems[fsIndex]->FilesystemHandler.Hunks[hunkIndex]->Relocate(destination + 4);
  ULO hunkSize = fhfile_rdb_filesystems[fsIndex]->FilesystemHandler.Hunks[hunkIndex]->GetSizeInLongwords() * 4;
  memoryWriteLong(hunkSize + 8, destination);
  memoryWriteLong(0, destination + 4);  // No next segment for now

  if (relocatedHunkData == nullptr)
  {
    fellowAddLog("fhfile: RDB Filesystem - fhfileDoRelocateHunk() called for hunk with no data.\n");
    return;
  }

  for (ULO i = 0; i < hunkSize; i++)
  {
    memoryWriteByte(relocatedHunkData[i], destination + i + 8);
  }
  delete relocatedHunkData;
}

void fhfileDoInitializeFileSystemEntry()
{
  ULO index = cpuGetDReg(1);
  ULO fsEntry = cpuGetDReg(0);

  RDBFilesystemHeader *fsHeader = fhfile_rdb_filesystems[index];

  for (int i = 0; i < 4; i++)
  {
    memoryWriteByte(fsHeader->DosType[i], fsEntry + 14 + i);
  }
  memoryWriteLong(fsHeader->Version, fsEntry + 18);
  memoryWriteLong(fsHeader->PatchFlags, fsEntry + 22);
  memoryWriteLong(fsHeader->DnType, fsEntry + 26);
  memoryWriteLong(fsHeader->DnTask, fsEntry + 30);
  memoryWriteLong(fsHeader->DnLock, fsEntry + 34);
  memoryWriteLong(fsHeader->DnHandler, fsEntry + 38);
  memoryWriteLong(fsHeader->DnStackSize, fsEntry + 42);
  memoryWriteLong(fsHeader->DnPriority, fsEntry + 46);
  memoryWriteLong(fsHeader->DnStartup, fsEntry + 50);
  memoryWriteLong(fsHeader->DnSegListBlock, fsEntry + 54);
  memoryWriteLong(fsHeader->DnGlobalVec, fsEntry + 58);

  for (int i = 0; i < 23; i++)
  {
    memoryWriteLong(fsHeader->Reserved2[i], fsEntry + 62 + i*4);
  }
}

std::string fhfileLogGetStringFromMemory(ULO address)
{
  std::string name;
  if (address == 0)
  {
    return std::string();
  }
  char c = memoryReadByte(address++);
  while (c != 0)
  {
    name.push_back((c == '\n' ? '.' : c));
    c = memoryReadByte(address++);
  }
  return name;
}

// D0 - pointer to resource list
void fhfileDoLogAvailableResources()
{
  fellowAddLog("fhfileDoLogAvailableResources()\n");

  ULO execBase = memoryReadLong(4);
  ULO rsListHeader = memoryReadLong(execBase + 0x150);

  fellowAddLog("Resource list header (%.8X): Head %.8X Tail %.8X TailPred %.8X Type %d\n", rsListHeader, memoryReadLong(rsListHeader), memoryReadLong(rsListHeader + 4), memoryReadLong(rsListHeader + 8), memoryReadByte(rsListHeader + 9));
  if (rsListHeader == memoryReadLong(rsListHeader + 8))
  {
    fellowAddLog("fhfile: Resource list is empty.\n");
    return;
  }

  ULO fsNode = memoryReadLong(rsListHeader);
  while (fsNode != 0 && (fsNode != rsListHeader + 4))
  {
    fellowAddLog("fhfile: ResourceEntry Node (%.8X): Succ %.8X Pred %.8X Type %d Pri %d NodeName '%s'\n", fsNode, memoryReadLong(fsNode), memoryReadLong(fsNode + 4), memoryReadByte(fsNode + 8), memoryReadByte(fsNode + 9), fhfileLogGetStringFromMemory(memoryReadLong(fsNode + 10)).c_str());
    fsNode = memoryReadLong(fsNode);
  }
}

void fhfileDoLogAllocMemResult()
{
  fellowAddLog("fhfile: AllocMem() returned %.8X\n", cpuGetDReg(0));
}

void fhfileDoLogOpenResourceResult()
{
  fellowAddLog("fhfile: OpenResource() returned %.8X\n", cpuGetDReg(0));
}

// D0 - pointer to resource list
void fhfileDoLogAvailableFilesystems()
{
  fellowAddLog("fhfile: Available filesystems\n");

  ULO fsResource = cpuGetDReg(0);

  fellowAddLog("fhfile: FileSystem resource node (%.8X): Succ %.8X Pred %.8X Type %d Pri %d NodeName '%s' Creator '%s'\n", fsResource, memoryReadLong(fsResource), memoryReadLong(fsResource + 4), memoryReadByte(fsResource + 8), memoryReadByte(fsResource + 9), fhfileLogGetStringFromMemory(memoryReadLong(fsResource + 10)).c_str(), fhfileLogGetStringFromMemory(memoryReadLong(fsResource + 14)).c_str());

  ULO fsList = fsResource + 18;
  fellowAddLog("fhfile: FileSystemEntries list header (%.8X): Head %.8X Tail %.8X TailPred %.8X Type %d\n", fsList, memoryReadLong(fsList), memoryReadLong(fsList + 4), memoryReadLong(fsList + 8), memoryReadByte(fsList + 9));
  if (fsList == memoryReadLong(fsList + 8))
  {
    fellowAddLog("fhfile: FileSystemEntry list is empty.\n");
    return;
  }

  ULO fsNode = memoryReadLong(fsList);
  while (fsNode != 0 && (fsNode != fsList + 4))
  {
    fellowAddLog("fhfile: FileSystemEntry Node (%.8X): Succ %.8X Pred %.8X Type %d Pri %d NodeName '%s'\n", fsNode, memoryReadLong(fsNode), memoryReadLong(fsNode + 4), memoryReadByte(fsNode + 8), memoryReadByte(fsNode + 9), fhfileLogGetStringFromMemory(memoryReadLong(fsNode + 10)).c_str());

    ULO fsEntry = fsNode + 14;
    fellowAddLog("fhfile: FileSystemEntry DosType   : %.8X\n", memoryReadLong(fsEntry));
    fellowAddLog("fhfile: FileSystemEntry Version   : %.8X\n", memoryReadLong(fsEntry + 4));
    fellowAddLog("fhfile: FileSystemEntry PatchFlags: %.8X\n", memoryReadLong(fsEntry + 8));
    fellowAddLog("fhfile: FileSystemEntry Type      : %.8X\n", memoryReadLong(fsEntry + 12));
    fellowAddLog("fhfile: FileSystemEntry Task      : %.8X\n", memoryReadLong(fsEntry + 16));
    fellowAddLog("fhfile: FileSystemEntry Lock      : %.8X\n", memoryReadLong(fsEntry + 20));
    fellowAddLog("fhfile: FileSystemEntry Handler   : %.8X\n", memoryReadLong(fsEntry + 24));
    fellowAddLog("fhfile: FileSystemEntry StackSize : %.8X\n", memoryReadLong(fsEntry + 28));
    fellowAddLog("fhfile: FileSystemEntry Priority  : %.8X\n", memoryReadLong(fsEntry + 32));
    fellowAddLog("fhfile: FileSystemEntry Startup   : %.8X\n", memoryReadLong(fsEntry + 36));
    fellowAddLog("fhfile: FileSystemEntry SegList   : %.8X\n", memoryReadLong(fsEntry + 40));
    fellowAddLog("fhfile: FileSystemEntry GlobalVec : %.8X\n\n", memoryReadLong(fsEntry + 44));
    fsNode = memoryReadLong(fsNode);
  }
}

/*=================================================*/
/* fhfile_do                                       */
/* The M68000 stubs entered in the device tables   */
/* write a longword to $f40000, which is forwarded */
/* by the memory system to this procedure.         */
/* Hardfile commands are issued by 0x0001XXXX      */
/* RDB filesystem commands by 0x0002XXXX           */
/*=================================================*/

void fhfileDo(ULO data)
{
  ULO type = data >> 16;
  ULO operation = data & 0xffff;
  if (type == 1)
  {
    switch (operation)
    {
      case 1:
        fhfileDoDiag();
        break;
      case 2:
        fhfileDoOpen();
        break;
      case 3:
        fhfileDoClose();
        break;
      case 4:
        fhfileDoExpunge();
        break;
      case 5:
        fhfileDoNULL();
        break;
      case 6:
        fhfileDoBeginIO();
        break;
      case 7:
        fhfileDoAbortIO();
        break;
      default:
        break;
    }
  }
  else if (type == 2)
  {
    switch (operation)
    {
      case 1:
        fhfileDoGetFileSystemCount();
        break;
      case 2:
        fhfileDoGetFileSystemHunkCount();
        break;
      case 3:
        fhfileDoGetFileSystemHunkSize();
        break;
      case 4:
        fhfileDoRelocateHunk();
        break;
      case 5:
        fhfileDoInitializeFileSystemEntry();
        break;
      case 0xa0:
        fhfileDoLogAllocMemResult();
        break;
      case 0xa1:
        fhfileDoLogOpenResourceResult();
        break;
      case 0xa2:
        fhfileDoLogAvailableResources();
        break;
      case 0xa3:
        fhfileDoLogAvailableFilesystems();
        break;
    }
  }
}

/*===========================================================================*/
/* Hardfile card init                                                        */
/*===========================================================================*/

/*================================================================*/
/* fhfile_card_init                                               */
/* We want a configDev struct.  AmigaDOS won't give us one unless */
/* we pretend to be an expansion card.                            */
/*================================================================*/

void fhfileCardInit(void) {
  memoryEmemSet(0, 0xd1);
  memoryEmemSet(8, 0xc0);
  memoryEmemSet(4, 2);
  memoryEmemSet(0x10, 2011>>8);
  memoryEmemSet(0x14, 2011 & 0xf);
  memoryEmemSet(0x18, 0);
  memoryEmemSet(0x1c, 0);
  memoryEmemSet(0x20, 0);
  memoryEmemSet(0x24, 1);
  memoryEmemSet(0x28, 0x10);
  memoryEmemSet(0x2c, 0);
  memoryEmemSet(0x40, 0);
  memoryEmemMirror(0x1000, fhfile_rom + 0x1000, 0xa0);
}

/*============================================================================*/
/* Functions to get and set data in the fhfile memory area                    */
/*============================================================================*/

UBY fhfileReadByte(ULO address)
{
  return fhfile_rom[address & 0xffff];
}

UWO fhfileReadWord(ULO address)
{
  UBY *p = fhfile_rom + (address & 0xffff);
  return (((UWO)p[0]) << 8) | ((UWO)p[1]);
}

ULO fhfileReadLong(ULO address)
{
  UBY *p = fhfile_rom + (address & 0xffff);
  return (((ULO)p[0]) << 24) | (((ULO)p[1]) << 16) | (((ULO)p[2]) << 8) | ((ULO)p[3]);
}

void fhfileWriteByte(UBY data, ULO address)
{
  // NOP
}

void fhfileWriteWord(UWO data, ULO address)
{
  // NOP
}

void fhfileWriteLong(ULO data, ULO address)
{
  // NOP
}

/*====================================================*/
/* fhfile_card_map                                    */
/* Our rom must be remapped to the location specified */
/* by Amiga OS                                        */
/*====================================================*/

void fhfileCardMap(ULO mapping) {
  fhfile_romstart = (mapping<<8) & 0xff0000;
  ULO bank = fhfile_romstart>>16;
  memoryBankSet(fhfileReadByte,
    fhfileReadWord,
    fhfileReadLong,
    fhfileWriteByte,
    fhfileWriteWord,
    fhfileWriteLong,
    fhfile_rom,
    bank,
    bank,
    FALSE);
}


/*=================================================*/
/* Make a dosdevice packet about the device layout */
/*=================================================*/

static void fhfileMakeDOSDevPacket(ULO devno, ULO unitnameptr, ULO devnameptr)
{
  if (fhfile_devs[devno].F)
  {
    memoryDmemSetLong(devno);				     /* Flag to initcode */

    memoryDmemSetLong(unitnameptr);			     /*  0 Device driver name "FELLOWX" */
    memoryDmemSetLong(devnameptr);			     /*  4 Device name "fhfile.device" */
    memoryDmemSetLong(devno);				     /*  8 Unit # */
    memoryDmemSetLong(0);				     /* 12 OpenDevice flags */

    // Struct DosEnvec
    memoryDmemSetLong(16);                                   /* 16 Environment size in long words*/
    memoryDmemSetLong(fhfile_devs[devno].bytespersector>>2); /* 20 Longwords in a block */
    memoryDmemSetLong(0);				     /* 24 sector origin (unused) */
    memoryDmemSetLong(fhfile_devs[devno].surfaces);	     /* 28 Heads */
    memoryDmemSetLong(1);				     /* 32 Sectors per logical block (unused) */
    memoryDmemSetLong(fhfile_devs[devno].sectorspertrack);   /* 36 Sectors per track */
    memoryDmemSetLong(fhfile_devs[devno].reservedblocks);    /* 40 Reserved blocks, min. 1 */
    memoryDmemSetLong(0);				     /* 44 mdn_prefac - Unused */
    memoryDmemSetLong(0);				     /* 48 Interleave */
    if (fhfile_devs[devno].hasRigidDiskBlock)
    {
      memoryDmemSetLong(fhfile_devs[devno].lowCylinder);     /* 52 Lower cylinder */
      memoryDmemSetLong(fhfile_devs[devno].highCylinder);    /* 56 Upper cylinder */
    }
    else
    {
      memoryDmemSetLong(0);				     /* 52 Lower cylinder */
      memoryDmemSetLong(fhfile_devs[devno].tracks - 1);	     /* 56 Upper cylinder */
    }
    memoryDmemSetLong(0);				     /* 60 Number of buffers */
    memoryDmemSetLong(0);				     /* 64 Type of memory for buffers */
    memoryDmemSetLong(0x7fffffff);			     /* 68 Largest transfer */
    memoryDmemSetLong(~1U);				     /* 72 Add mask */
    memoryDmemSetLong(-1);				     /* 76 Boot priority */
    memoryDmemSetLong(0x444f5303);			     /* 80 DOS file handler name */
    memoryDmemSetLong(0);
  }
  if (devno == (FHFILE_MAX_DEVICES - 1))
    memoryDmemSetLong(-1);
}


/*===========================================================*/
/* fhfileHardReset                                           */
/* This will set up the device structures and stubs          */
/* Can be called at every reset, but really only needed once */
/*===========================================================*/

void fhfileHardReset()
{
  ULO unitnames[FHFILE_MAX_DEVICES];
  STR tmpunitname[32];

  fhfile_rdb_filesystems.clear();

  if (!fhfileHasZeroDevices() && fhfileGetEnabled() && memoryGetKickImageVersion() >= 34)
  {
      memoryDmemSetCounter(0);

      /* Device-name and ID string */

      ULO devicename = memoryDmemGetCounter();
      memoryDmemSetString("fhfile.device");
      ULO idstr = memoryDmemGetCounter();
      memoryDmemSetString("Fellow Hardfile device V4");

      /* Device name as seen in Amiga DOS */

      for (int i = 0; i < FHFILE_MAX_DEVICES; i++)
      {
	unitnames[i] = memoryDmemGetCounter();
	sprintf(tmpunitname, "FELLOW%d", i);
	memoryDmemSetString(tmpunitname);
      }

      /* dos.library name */

      ULO doslibname = memoryDmemGetCounter();
      memoryDmemSetString("dos.library");

      /* fhfile.open */

      ULO fhfile_t_open = memoryDmemGetCounter();
      memoryDmemSetWord(0x23fc);
      memoryDmemSetLong(0x00010002); memoryDmemSetLong(0xf40000); /* move.l #$00010002,$f40000 */
      memoryDmemSetWord(0x4e75);					/* rts */

      /* fhfile.close */

      ULO fhfile_t_close = memoryDmemGetCounter();
      memoryDmemSetWord(0x23fc);
      memoryDmemSetLong(0x00010003); memoryDmemSetLong(0xf40000); /* move.l #$00010003,$f40000 */
      memoryDmemSetWord(0x4e75);					/* rts */

      /* fhfile.expunge */

      ULO fhfile_t_expunge = memoryDmemGetCounter();
      memoryDmemSetWord(0x23fc);
      memoryDmemSetLong(0x00010004); memoryDmemSetLong(0xf40000); /* move.l #$00010004,$f40000 */
      memoryDmemSetWord(0x4e75);					/* rts */

      /* fhfile.null */

      ULO fhfile_t_null = memoryDmemGetCounter();
      memoryDmemSetWord(0x23fc);
      memoryDmemSetLong(0x00010005); memoryDmemSetLong(0xf40000); /* move.l #$00010005,$f40000 */
      memoryDmemSetWord(0x4e75);                                  /* rts */

      /* fhfile.beginio */

      ULO fhfile_t_beginio = memoryDmemGetCounter();
      memoryDmemSetWord(0x23fc);
      memoryDmemSetLong(0x00010006); memoryDmemSetLong(0xf40000); /* move.l #$00010006,$f40000 */
      memoryDmemSetLong(0x48e78002);				/* movem.l d0/a6,-(a7) */
      memoryDmemSetLong(0x08290000); memoryDmemSetWord(0x001e);   /* btst   #$0,30(a1)   */
      memoryDmemSetWord(0x6608);					/* bne    (to rts)     */
      memoryDmemSetLong(0x2c780004);				/* move.l $4.w,a6      */
      memoryDmemSetLong(0x4eaefe86);				/* jsr    -378(a6)     */
      memoryDmemSetLong(0x4cdf4001);				/* movem.l (a7)+,d0/a6 */
      memoryDmemSetWord(0x4e75);					/* rts */

      /* fhfile.abortio */

      ULO fhfile_t_abortio = memoryDmemGetCounter();
      memoryDmemSetWord(0x23fc);
      memoryDmemSetLong(0x00010007); memoryDmemSetLong(0xf40000); /* move.l #$00010007,$f40000 */
      memoryDmemSetWord(0x4e75);					/* rts */

      /* Func-table */

      ULO functable = memoryDmemGetCounter();
      memoryDmemSetLong(fhfile_t_open);
      memoryDmemSetLong(fhfile_t_close);
      memoryDmemSetLong(fhfile_t_expunge);
      memoryDmemSetLong(fhfile_t_null);
      memoryDmemSetLong(fhfile_t_beginio);
      memoryDmemSetLong(fhfile_t_abortio);
      memoryDmemSetLong(0xffffffff);

      /* Data-table */

      ULO datatable = memoryDmemGetCounter();
      memoryDmemSetWord(0xE000);          /* INITBYTE */
      memoryDmemSetWord(0x0008);          /* LN_TYPE */
      memoryDmemSetWord(0x0300);          /* NT_DEVICE */
      memoryDmemSetWord(0xC000);          /* INITLONG */
      memoryDmemSetWord(0x000A);          /* LN_NAME */
      memoryDmemSetLong(devicename);
      memoryDmemSetWord(0xE000);          /* INITBYTE */
      memoryDmemSetWord(0x000E);          /* LIB_FLAGS */
      memoryDmemSetWord(0x0600);          /* LIBF_SUMUSED+LIBF_CHANGED */
      memoryDmemSetWord(0xD000);          /* INITWORD */
      memoryDmemSetWord(0x0014);          /* LIB_VERSION */
      memoryDmemSetWord(0x0002);
      memoryDmemSetWord(0xD000);          /* INITWORD */
      memoryDmemSetWord(0x0016);          /* LIB_REVISION */
      memoryDmemSetWord(0x0000);
      memoryDmemSetWord(0xC000);          /* INITLONG */
      memoryDmemSetWord(0x0018);          /* LIB_IDSTRING */
      memoryDmemSetLong(idstr);
      memoryDmemSetLong(0);               /* END */

      /* bootcode */

      fhfile_bootcode = memoryDmemGetCounter();
      memoryDmemSetWord(0x227c); memoryDmemSetLong(doslibname); /* move.l #doslibname,a1 */
      memoryDmemSetLong(0x4eaeffa0);			      /* jsr    -96(a6) ; FindResident() */
      memoryDmemSetWord(0x2040);				      /* move.l d0,a0 */
      memoryDmemSetLong(0x20280016);			      /* move.l 22(a0),d0 */
      memoryDmemSetWord(0x2040);				      /* move.l d0,a0 */
      memoryDmemSetWord(0x4e90);				      /* jsr    (a0) */
      memoryDmemSetWord(0x4e75);				      /* rts */

      /* fhfile.init */

      ULO fhfile_t_init = memoryDmemGetCounter();

      memoryDmemSetByte(0x48); memoryDmemSetByte(0xE7); memoryDmemSetByte(0xFF); memoryDmemSetByte(0xFE);
      memoryDmemSetByte(0x61); memoryDmemSetByte(0x00); memoryDmemSetByte(0x01); memoryDmemSetByte(0x1E);
      memoryDmemSetByte(0x4A); memoryDmemSetByte(0x80); memoryDmemSetByte(0x67); memoryDmemSetByte(0x20);
      memoryDmemSetByte(0x61); memoryDmemSetByte(0x00); memoryDmemSetByte(0x00); memoryDmemSetByte(0xFE);
      memoryDmemSetByte(0x61); memoryDmemSetByte(0x00); memoryDmemSetByte(0x01); memoryDmemSetByte(0xA8);
      memoryDmemSetByte(0x4A); memoryDmemSetByte(0x80); memoryDmemSetByte(0x66); memoryDmemSetByte(0x0C);
      memoryDmemSetByte(0x61); memoryDmemSetByte(0x00); memoryDmemSetByte(0x01); memoryDmemSetByte(0x62);
      memoryDmemSetByte(0x61); memoryDmemSetByte(0x00); memoryDmemSetByte(0x01); memoryDmemSetByte(0x9C);
      memoryDmemSetByte(0x4A); memoryDmemSetByte(0x80); memoryDmemSetByte(0x67); memoryDmemSetByte(0x08);
      memoryDmemSetByte(0x61); memoryDmemSetByte(0x00); memoryDmemSetByte(0x00); memoryDmemSetByte(0xE6);
      memoryDmemSetByte(0x61); memoryDmemSetByte(0x00); memoryDmemSetByte(0x02); memoryDmemSetByte(0x10);
      memoryDmemSetByte(0x2F); memoryDmemSetByte(0x00); memoryDmemSetByte(0x2C); memoryDmemSetByte(0x78);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x04); memoryDmemSetByte(0x43); memoryDmemSetByte(0xFA);
      memoryDmemSetByte(0x02); memoryDmemSetByte(0x24); memoryDmemSetByte(0x4E); memoryDmemSetByte(0xAE);
      memoryDmemSetByte(0xFE); memoryDmemSetByte(0x68); memoryDmemSetByte(0x28); memoryDmemSetByte(0x40);
      memoryDmemSetByte(0x41); memoryDmemSetByte(0xFA); memoryDmemSetByte(0x02); memoryDmemSetByte(0x6E);
      memoryDmemSetByte(0x2E); memoryDmemSetByte(0x08); memoryDmemSetByte(0x20); memoryDmemSetByte(0x47);
      memoryDmemSetByte(0x4A); memoryDmemSetByte(0x90); memoryDmemSetByte(0x6B); memoryDmemSetByte(0x00);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x9C); memoryDmemSetByte(0x58); memoryDmemSetByte(0x87);
      memoryDmemSetByte(0x20); memoryDmemSetByte(0x3C); memoryDmemSetByte(0x00); memoryDmemSetByte(0x00);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x58); memoryDmemSetByte(0x61); memoryDmemSetByte(0x00);
      memoryDmemSetByte(0x01); memoryDmemSetByte(0x0C); memoryDmemSetByte(0x2A); memoryDmemSetByte(0x40);
      memoryDmemSetByte(0x20); memoryDmemSetByte(0x47); memoryDmemSetByte(0x70); memoryDmemSetByte(0x54);
      memoryDmemSetByte(0x2B); memoryDmemSetByte(0xB0); memoryDmemSetByte(0x08); memoryDmemSetByte(0x00);
      memoryDmemSetByte(0x08); memoryDmemSetByte(0x00); memoryDmemSetByte(0x59); memoryDmemSetByte(0x80);
      memoryDmemSetByte(0x64); memoryDmemSetByte(0xF6); memoryDmemSetByte(0xCD); memoryDmemSetByte(0x4C);
      memoryDmemSetByte(0x20); memoryDmemSetByte(0x4D); memoryDmemSetByte(0x4E); memoryDmemSetByte(0xAE);
      memoryDmemSetByte(0xFF); memoryDmemSetByte(0x70); memoryDmemSetByte(0xCD); memoryDmemSetByte(0x4C);
      memoryDmemSetByte(0x26); memoryDmemSetByte(0x40); memoryDmemSetByte(0x70); memoryDmemSetByte(0x00);
      memoryDmemSetByte(0x27); memoryDmemSetByte(0x40); memoryDmemSetByte(0x00); memoryDmemSetByte(0x08);
      memoryDmemSetByte(0x27); memoryDmemSetByte(0x40); memoryDmemSetByte(0x00); memoryDmemSetByte(0x10);
      memoryDmemSetByte(0x27); memoryDmemSetByte(0x40); memoryDmemSetByte(0x00); memoryDmemSetByte(0x20);
      memoryDmemSetByte(0x24); memoryDmemSetByte(0x5F); memoryDmemSetByte(0xB5); memoryDmemSetByte(0xFC);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x00); memoryDmemSetByte(0x00); memoryDmemSetByte(0x00);
      memoryDmemSetByte(0x67); memoryDmemSetByte(0x18); memoryDmemSetByte(0x20); memoryDmemSetByte(0x2A);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x2A); memoryDmemSetByte(0x27); memoryDmemSetByte(0x40);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x14); memoryDmemSetByte(0x20); memoryDmemSetByte(0x2A);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x36); memoryDmemSetByte(0x27); memoryDmemSetByte(0x40);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x20); memoryDmemSetByte(0x20); memoryDmemSetByte(0x2A);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x3A); memoryDmemSetByte(0x27); memoryDmemSetByte(0x40);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x24); memoryDmemSetByte(0x70); memoryDmemSetByte(0x14);
      memoryDmemSetByte(0x61); memoryDmemSetByte(0x00); memoryDmemSetByte(0x00); memoryDmemSetByte(0xBA);
      memoryDmemSetByte(0x22); memoryDmemSetByte(0x47); memoryDmemSetByte(0x2C); memoryDmemSetByte(0x29);
      memoryDmemSetByte(0xFF); memoryDmemSetByte(0xFC); memoryDmemSetByte(0x22); memoryDmemSetByte(0x40);
      memoryDmemSetByte(0x70); memoryDmemSetByte(0x00); memoryDmemSetByte(0x22); memoryDmemSetByte(0x80);
      memoryDmemSetByte(0x23); memoryDmemSetByte(0x40); memoryDmemSetByte(0x00); memoryDmemSetByte(0x04);
      memoryDmemSetByte(0x33); memoryDmemSetByte(0x40); memoryDmemSetByte(0x00); memoryDmemSetByte(0x0E);
      memoryDmemSetByte(0x33); memoryDmemSetByte(0x7C); memoryDmemSetByte(0x10); memoryDmemSetByte(0xFF);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x08); memoryDmemSetByte(0x9D); memoryDmemSetByte(0x69);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x08); memoryDmemSetByte(0x23); memoryDmemSetByte(0x79);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0xF4); memoryDmemSetByte(0x0F); memoryDmemSetByte(0xFC);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x0A); memoryDmemSetByte(0x23); memoryDmemSetByte(0x4B);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x10); memoryDmemSetByte(0x41); memoryDmemSetByte(0xEC);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x4A); memoryDmemSetByte(0x4E); memoryDmemSetByte(0xAE);
      memoryDmemSetByte(0xFE); memoryDmemSetByte(0xF2); memoryDmemSetByte(0x06); memoryDmemSetByte(0x87);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x00); memoryDmemSetByte(0x00); memoryDmemSetByte(0x58);
      memoryDmemSetByte(0x60); memoryDmemSetByte(0x00); memoryDmemSetByte(0xFF); memoryDmemSetByte(0x60);
      memoryDmemSetByte(0x2C); memoryDmemSetByte(0x78); memoryDmemSetByte(0x00); memoryDmemSetByte(0x04);
      memoryDmemSetByte(0x22); memoryDmemSetByte(0x4C); memoryDmemSetByte(0x4E); memoryDmemSetByte(0xAE);
      memoryDmemSetByte(0xFE); memoryDmemSetByte(0x62); memoryDmemSetByte(0x4C); memoryDmemSetByte(0xDF);
      memoryDmemSetByte(0x7F); memoryDmemSetByte(0xFF); memoryDmemSetByte(0x4E); memoryDmemSetByte(0x75);
      memoryDmemSetByte(0x23); memoryDmemSetByte(0xFC); memoryDmemSetByte(0x00); memoryDmemSetByte(0x02);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0xA0); memoryDmemSetByte(0x00); memoryDmemSetByte(0xF4);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x00); memoryDmemSetByte(0x4E); memoryDmemSetByte(0x75);
      memoryDmemSetByte(0x23); memoryDmemSetByte(0xFC); memoryDmemSetByte(0x00); memoryDmemSetByte(0x02);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0xA1); memoryDmemSetByte(0x00); memoryDmemSetByte(0xF4);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x00); memoryDmemSetByte(0x4E); memoryDmemSetByte(0x75);
      memoryDmemSetByte(0x23); memoryDmemSetByte(0xFC); memoryDmemSetByte(0x00); memoryDmemSetByte(0x02);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0xA2); memoryDmemSetByte(0x00); memoryDmemSetByte(0xF4);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x00); memoryDmemSetByte(0x4E); memoryDmemSetByte(0x75);
      memoryDmemSetByte(0x23); memoryDmemSetByte(0xFC); memoryDmemSetByte(0x00); memoryDmemSetByte(0x02);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0xA3); memoryDmemSetByte(0x00); memoryDmemSetByte(0xF4);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x00); memoryDmemSetByte(0x4E); memoryDmemSetByte(0x75);
      memoryDmemSetByte(0x23); memoryDmemSetByte(0xFC); memoryDmemSetByte(0x00); memoryDmemSetByte(0x02);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x01); memoryDmemSetByte(0x00); memoryDmemSetByte(0xF4);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x00); memoryDmemSetByte(0x4E); memoryDmemSetByte(0x75);
      memoryDmemSetByte(0x23); memoryDmemSetByte(0xFC); memoryDmemSetByte(0x00); memoryDmemSetByte(0x02);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x02); memoryDmemSetByte(0x00); memoryDmemSetByte(0xF4);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x00); memoryDmemSetByte(0x4E); memoryDmemSetByte(0x75);
      memoryDmemSetByte(0x23); memoryDmemSetByte(0xFC); memoryDmemSetByte(0x00); memoryDmemSetByte(0x02);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x03); memoryDmemSetByte(0x00); memoryDmemSetByte(0xF4);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x00); memoryDmemSetByte(0x4E); memoryDmemSetByte(0x75);
      memoryDmemSetByte(0x23); memoryDmemSetByte(0xFC); memoryDmemSetByte(0x00); memoryDmemSetByte(0x02);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x04); memoryDmemSetByte(0x00); memoryDmemSetByte(0xF4);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x00); memoryDmemSetByte(0x4E); memoryDmemSetByte(0x75);
      memoryDmemSetByte(0x23); memoryDmemSetByte(0xFC); memoryDmemSetByte(0x00); memoryDmemSetByte(0x02);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x05); memoryDmemSetByte(0x00); memoryDmemSetByte(0xF4);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x00); memoryDmemSetByte(0x4E); memoryDmemSetByte(0x75);
      memoryDmemSetByte(0x48); memoryDmemSetByte(0xE7); memoryDmemSetByte(0x78); memoryDmemSetByte(0x00);
      memoryDmemSetByte(0x22); memoryDmemSetByte(0x3C); memoryDmemSetByte(0x00); memoryDmemSetByte(0x01);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x01); memoryDmemSetByte(0x2C); memoryDmemSetByte(0x78);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x04); memoryDmemSetByte(0x4E); memoryDmemSetByte(0xAE);
      memoryDmemSetByte(0xFF); memoryDmemSetByte(0x3A); memoryDmemSetByte(0x61); memoryDmemSetByte(0x00);
      memoryDmemSetByte(0xFF); memoryDmemSetByte(0x80); memoryDmemSetByte(0x4C); memoryDmemSetByte(0xDF);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x1E); memoryDmemSetByte(0x4E); memoryDmemSetByte(0x75);
      memoryDmemSetByte(0x20); memoryDmemSetByte(0x3C); memoryDmemSetByte(0x00); memoryDmemSetByte(0x00);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x20); memoryDmemSetByte(0x61); memoryDmemSetByte(0x00);
      memoryDmemSetByte(0xFF); memoryDmemSetByte(0xDC); memoryDmemSetByte(0x2A); memoryDmemSetByte(0x40);
      memoryDmemSetByte(0x1B); memoryDmemSetByte(0x7C); memoryDmemSetByte(0x00); memoryDmemSetByte(0x08);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x08); memoryDmemSetByte(0x41); memoryDmemSetByte(0xFA);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0xDA); memoryDmemSetByte(0x2B); memoryDmemSetByte(0x48);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x0A); memoryDmemSetByte(0x41); memoryDmemSetByte(0xFA);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0xE6); memoryDmemSetByte(0x2B); memoryDmemSetByte(0x48);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x0E); memoryDmemSetByte(0x49); memoryDmemSetByte(0xED);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x12); memoryDmemSetByte(0x28); memoryDmemSetByte(0x8C);
      memoryDmemSetByte(0x06); memoryDmemSetByte(0x94); memoryDmemSetByte(0x00); memoryDmemSetByte(0x00);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x04); memoryDmemSetByte(0x42); memoryDmemSetByte(0xAC);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x04); memoryDmemSetByte(0x29); memoryDmemSetByte(0x4C);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x08); memoryDmemSetByte(0x22); memoryDmemSetByte(0x4D);
      memoryDmemSetByte(0x4E); memoryDmemSetByte(0xAE); memoryDmemSetByte(0xFE); memoryDmemSetByte(0x1A);
      memoryDmemSetByte(0x4E); memoryDmemSetByte(0x75); memoryDmemSetByte(0x2C); memoryDmemSetByte(0x78);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x04); memoryDmemSetByte(0x70); memoryDmemSetByte(0x00);
      memoryDmemSetByte(0x43); memoryDmemSetByte(0xFA); memoryDmemSetByte(0x00); memoryDmemSetByte(0xA8);
      memoryDmemSetByte(0x4E); memoryDmemSetByte(0xAE); memoryDmemSetByte(0xFE); memoryDmemSetByte(0x0E);
      memoryDmemSetByte(0x61); memoryDmemSetByte(0x00); memoryDmemSetByte(0xFF); memoryDmemSetByte(0x36);
      memoryDmemSetByte(0x4E); memoryDmemSetByte(0x75); memoryDmemSetByte(0x61); memoryDmemSetByte(0x00);
      memoryDmemSetByte(0xFF); memoryDmemSetByte(0x60); memoryDmemSetByte(0x28); memoryDmemSetByte(0x00);
      memoryDmemSetByte(0x4A); memoryDmemSetByte(0x84); memoryDmemSetByte(0x67); memoryDmemSetByte(0x00);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x20); memoryDmemSetByte(0x74); memoryDmemSetByte(0x00);
      memoryDmemSetByte(0x61); memoryDmemSetByte(0x00); memoryDmemSetByte(0xFF); memoryDmemSetByte(0x5E);
      memoryDmemSetByte(0x4A); memoryDmemSetByte(0x80); memoryDmemSetByte(0x67); memoryDmemSetByte(0x00);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x0C); memoryDmemSetByte(0x50); memoryDmemSetByte(0x80);
      memoryDmemSetByte(0x61); memoryDmemSetByte(0x00); memoryDmemSetByte(0xFF); memoryDmemSetByte(0x76);
      memoryDmemSetByte(0x61); memoryDmemSetByte(0x00); memoryDmemSetByte(0xFF); memoryDmemSetByte(0x5A);
      memoryDmemSetByte(0x52); memoryDmemSetByte(0x82); memoryDmemSetByte(0xB8); memoryDmemSetByte(0x82);
      memoryDmemSetByte(0x66); memoryDmemSetByte(0x00); memoryDmemSetByte(0xFF); memoryDmemSetByte(0xE6);
      memoryDmemSetByte(0x4E); memoryDmemSetByte(0x75); memoryDmemSetByte(0x2F); memoryDmemSetByte(0x08);
      memoryDmemSetByte(0x61); memoryDmemSetByte(0x00); memoryDmemSetByte(0xFF); memoryDmemSetByte(0xD0);
      memoryDmemSetByte(0x2F); memoryDmemSetByte(0x00); memoryDmemSetByte(0x20); memoryDmemSetByte(0x3C);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x00); memoryDmemSetByte(0x00); memoryDmemSetByte(0xBE);
      memoryDmemSetByte(0x61); memoryDmemSetByte(0x00); memoryDmemSetByte(0xFF); memoryDmemSetByte(0x56);
      memoryDmemSetByte(0x61); memoryDmemSetByte(0x00); memoryDmemSetByte(0xFF); memoryDmemSetByte(0x46);
      memoryDmemSetByte(0x2E); memoryDmemSetByte(0x1F); memoryDmemSetByte(0x22); memoryDmemSetByte(0x40);
      memoryDmemSetByte(0x58); memoryDmemSetByte(0x87); memoryDmemSetByte(0xE4); memoryDmemSetByte(0x8F);
      memoryDmemSetByte(0x23); memoryDmemSetByte(0x47); memoryDmemSetByte(0x00); memoryDmemSetByte(0x36);
      memoryDmemSetByte(0x4B); memoryDmemSetByte(0xFA); memoryDmemSetByte(0x00); memoryDmemSetByte(0x77);
      memoryDmemSetByte(0x29); memoryDmemSetByte(0x4D); memoryDmemSetByte(0x00); memoryDmemSetByte(0x0A);
      memoryDmemSetByte(0x20); memoryDmemSetByte(0x5F); memoryDmemSetByte(0x2F); memoryDmemSetByte(0x09);
      memoryDmemSetByte(0x2C); memoryDmemSetByte(0x79); memoryDmemSetByte(0x00); memoryDmemSetByte(0x00);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x04); memoryDmemSetByte(0x41); memoryDmemSetByte(0xE8);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x12); memoryDmemSetByte(0x4E); memoryDmemSetByte(0xAE);
      memoryDmemSetByte(0xFF); memoryDmemSetByte(0x10); memoryDmemSetByte(0x20); memoryDmemSetByte(0x1F);
      memoryDmemSetByte(0x4E); memoryDmemSetByte(0x75); memoryDmemSetByte(0x20); memoryDmemSetByte(0x40);
      memoryDmemSetByte(0x61); memoryDmemSetByte(0x00); memoryDmemSetByte(0xFE); memoryDmemSetByte(0xE6);
      memoryDmemSetByte(0x26); memoryDmemSetByte(0x00); memoryDmemSetByte(0x4A); memoryDmemSetByte(0x83);
      memoryDmemSetByte(0x67); memoryDmemSetByte(0x00); memoryDmemSetByte(0x00); memoryDmemSetByte(0x10);
      memoryDmemSetByte(0x72); memoryDmemSetByte(0x00); memoryDmemSetByte(0x61); memoryDmemSetByte(0x00);
      memoryDmemSetByte(0xFF); memoryDmemSetByte(0xAE); memoryDmemSetByte(0x52); memoryDmemSetByte(0x81);
      memoryDmemSetByte(0xB6); memoryDmemSetByte(0x81); memoryDmemSetByte(0x66); memoryDmemSetByte(0x00);
      memoryDmemSetByte(0xFF); memoryDmemSetByte(0xF6); memoryDmemSetByte(0x4E); memoryDmemSetByte(0x75);
      memoryDmemSetByte(0x65); memoryDmemSetByte(0x78); memoryDmemSetByte(0x70); memoryDmemSetByte(0x61);
      memoryDmemSetByte(0x6E); memoryDmemSetByte(0x73); memoryDmemSetByte(0x69); memoryDmemSetByte(0x6F);
      memoryDmemSetByte(0x6E); memoryDmemSetByte(0x2E); memoryDmemSetByte(0x6C); memoryDmemSetByte(0x69);
      memoryDmemSetByte(0x62); memoryDmemSetByte(0x72); memoryDmemSetByte(0x61); memoryDmemSetByte(0x72);
      memoryDmemSetByte(0x79); memoryDmemSetByte(0x00); memoryDmemSetByte(0x46); memoryDmemSetByte(0x69);
      memoryDmemSetByte(0x6C); memoryDmemSetByte(0x65); memoryDmemSetByte(0x53); memoryDmemSetByte(0x79);
      memoryDmemSetByte(0x73); memoryDmemSetByte(0x74); memoryDmemSetByte(0x65); memoryDmemSetByte(0x6D);
      memoryDmemSetByte(0x2E); memoryDmemSetByte(0x72); memoryDmemSetByte(0x65); memoryDmemSetByte(0x73);
      memoryDmemSetByte(0x6F); memoryDmemSetByte(0x75); memoryDmemSetByte(0x72); memoryDmemSetByte(0x63);
      memoryDmemSetByte(0x65); memoryDmemSetByte(0x00); memoryDmemSetByte(0x46); memoryDmemSetByte(0x65);
      memoryDmemSetByte(0x6C); memoryDmemSetByte(0x6C); memoryDmemSetByte(0x6F); memoryDmemSetByte(0x77);
      memoryDmemSetByte(0x20); memoryDmemSetByte(0x68); memoryDmemSetByte(0x61); memoryDmemSetByte(0x72);
      memoryDmemSetByte(0x64); memoryDmemSetByte(0x66); memoryDmemSetByte(0x69); memoryDmemSetByte(0x6C);
      memoryDmemSetByte(0x65); memoryDmemSetByte(0x20); memoryDmemSetByte(0x64); memoryDmemSetByte(0x65);
      memoryDmemSetByte(0x76); memoryDmemSetByte(0x69); memoryDmemSetByte(0x63); memoryDmemSetByte(0x65);
      memoryDmemSetByte(0x00); memoryDmemSetByte(0x46); memoryDmemSetByte(0x65); memoryDmemSetByte(0x6C);
      memoryDmemSetByte(0x6C); memoryDmemSetByte(0x6F); memoryDmemSetByte(0x77); memoryDmemSetByte(0x20);
      memoryDmemSetByte(0x68); memoryDmemSetByte(0x61); memoryDmemSetByte(0x72); memoryDmemSetByte(0x64);
      memoryDmemSetByte(0x66); memoryDmemSetByte(0x69); memoryDmemSetByte(0x6C); memoryDmemSetByte(0x65);
      memoryDmemSetByte(0x20); memoryDmemSetByte(0x52); memoryDmemSetByte(0x44); memoryDmemSetByte(0x42);
      memoryDmemSetByte(0x20); memoryDmemSetByte(0x66); memoryDmemSetByte(0x73); memoryDmemSetByte(0x00);

      /* The mkdosdev packets */

      for (int i = 0; i < FHFILE_MAX_DEVICES; i++)
      {
        fhfileMakeDOSDevPacket(i, unitnames[i], devicename);
      }

      /* Init-struct */

      ULO initstruct = memoryDmemGetCounter();
      memoryDmemSetLong(0x100);                   /* Data-space size, min LIB_SIZE */
      memoryDmemSetLong(functable);               /* Function-table */
      memoryDmemSetLong(datatable);               /* Data-table */
      memoryDmemSetLong(fhfile_t_init);           /* Init-routine */

      /* RomTag structure */

      ULO romtagstart = memoryDmemGetCounter();
      memoryDmemSetWord(0x4afc);                  /* Start of structure */
      memoryDmemSetLong(romtagstart);             /* Pointer to start of structure */
      memoryDmemSetLong(romtagstart+26);          /* Pointer to end of code */
      memoryDmemSetByte(0x81);                    /* Flags, AUTOINIT+COLDSTART */
      memoryDmemSetByte(0x1);                     /* Version */
      memoryDmemSetByte(3);                       /* DEVICE */
      memoryDmemSetByte(0);                       /* Priority */
      memoryDmemSetLong(devicename);              /* Pointer to name (used in opendev)*/
      memoryDmemSetLong(idstr);                   /* ID string */
      memoryDmemSetLong(initstruct);              /* Init_struct */

      /* Clear hardfile rom */

      memset(fhfile_rom, 0, 65536);

      /* Struct DiagArea */

      fhfile_rom[0x1000] = 0x90; /* da_Config */
      fhfile_rom[0x1001] = 0;    /* da_Flags */
      fhfile_rom[0x1002] = 0;    /* da_Size */
      fhfile_rom[0x1003] = 0x96;
      fhfile_rom[0x1004] = 0;    /* da_DiagPoint */
      fhfile_rom[0x1005] = 0x80;
      fhfile_rom[0x1006] = 0;    /* da_BootPoint */
      fhfile_rom[0x1007] = 0x90;
      fhfile_rom[0x1008] = 0;    /* da_Name */
      fhfile_rom[0x1009] = 0;
      fhfile_rom[0x100a] = 0;    /* da_Reserved01 */
      fhfile_rom[0x100b] = 0;
      fhfile_rom[0x100c] = 0;    /* da_Reserved02 */
      fhfile_rom[0x100d] = 0;

      fhfile_rom[0x1080] = 0x23; /* DiagPoint */
      fhfile_rom[0x1081] = 0xfc; /* move.l #$00010001,$f40000 */
      fhfile_rom[0x1082] = 0x00;
      fhfile_rom[0x1083] = 0x01;
      fhfile_rom[0x1084] = 0x00;
      fhfile_rom[0x1085] = 0x01;
      fhfile_rom[0x1086] = 0x00;
      fhfile_rom[0x1087] = 0xf4;
      fhfile_rom[0x1088] = 0x00;
      fhfile_rom[0x1089] = 0x00;
      fhfile_rom[0x108a] = 0x4e; /* rts */
      fhfile_rom[0x108b] = 0x75;

      fhfile_rom[0x1090] = 0x4e; /* BootPoint */
      fhfile_rom[0x1091] = 0xf9; /* JMP fhfile_bootcode */
      fhfile_rom[0x1092] = (UBY) (fhfile_bootcode>>24);
      fhfile_rom[0x1093] = (UBY) (fhfile_bootcode>>16);
      fhfile_rom[0x1094] = (UBY) (fhfile_bootcode>>8);
      fhfile_rom[0x1095] = (UBY) fhfile_bootcode;

      /* NULLIFY pointer to configdev */

      memoryDmemSetLongNoCounter(0, 4092);
      memoryEmemCardAdd(fhfileCardInit, fhfileCardMap);

      for (int i = 0; i < FHFILE_MAX_DEVICES; i++)
      {
        if (fhfile_devs[i].F != nullptr && fhfile_devs[i].rdb != nullptr)
        {
          for (auto fileSystemHeader : fhfile_devs[i].rdb->FilesystemHeaders)
          {
            fhfile_rdb_filesystems.push_back(fileSystemHeader);
          }
        }
      }
  }
  else
  {
    memoryDmemClear();
  }
}


/*=========================*/
/* Startup hardfile device */
/*=========================*/

void fhfileStartup(void) {
  /* Clear first to ensure that F is NULL */
  memset(fhfile_devs, 0, sizeof(fhfile_dev)*FHFILE_MAX_DEVICES);
  fhfileClear();
}


/*==========================*/
/* Shutdown hardfile device */
/*==========================*/

void fhfileShutdown(void) {
  fhfileClear();
}

/*==========================*/
/* Create hardfile          */
/*==========================*/

BOOLE fhfileCreate(fhfile_dev hfile)
{
  BOOLE result = FALSE;

#ifdef WIN32
  HANDLE hf;

  if(*hfile.filename && hfile.size) 
  {   
    if((hf = CreateFile(hfile.filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL)) != INVALID_HANDLE_VALUE)
    {
      if( SetFilePointer(hf, hfile.size, NULL, FILE_BEGIN) == hfile.size )
	result = SetEndOfFile(hf);
      else
	fellowAddLog("SetFilePointer() failure.\n");
      CloseHandle(hf);
    }
    else
      fellowAddLog("CreateFile() failed.\n");
  }
  return result;
#else	/* os independent implementation */
#define BUFSIZE 32768
  ULO tobewritten;
  char buffer[BUFSIZE];
  FILE *hf;

  tobewritten = hfile.size;
  errno = 0;

  if(*hfile.filename && hfile.size) 
  {   
    if(hf = fopen(hfile.filename, "wb"))
    {
      memset(buffer, 0, sizeof(buffer));

      while(tobewritten >= BUFSIZE)
      {
	fwrite(buffer, sizeof(char), BUFSIZE, hf);
	if (errno != 0)
	{
	  fellowAddLog("Creating hardfile failed. Check the available space.\n");
	  fclose(hf);
	  return result;
	}
	tobewritten -= BUFSIZE;
      }
      fwrite(buffer, sizeof(char), tobewritten, hf);
      if (errno != 0)
      {
	fellowAddLog("Creating hardfile failed. Check the available space.\n");
	fclose(hf);
	return result;
      }
      fclose(hf);
      result = TRUE;
    }
    else
      fellowAddLog("fhfileCreate is unable to open output file.\n");
  }
  return result;
#endif
}
