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
#include <sstream>

#ifdef RETRO_PLATFORM
#include "RetroPlatform.h"
#endif

HardfileHandler hardfileHandler;

bool HardfileFileSystemEntry::IsOlderOrSameFileSystemVersion(ULO DOSType, ULO version)
{
  return IsDOSType(DOSType) && IsOlderOrSameVersion(version);
}

bool HardfileFileSystemEntry::IsDOSType(ULO DOSType)
{
  return Header->DOSType == DOSType;
}

bool HardfileFileSystemEntry::IsOlderVersion(ULO version)
{
  return Header->Version < version;
}

bool HardfileFileSystemEntry::IsOlderOrSameVersion(ULO version)
{
  return Header->Version <= version;
}

ULO HardfileFileSystemEntry::GetDOSType()
{
  return Header->DOSType;
}

ULO HardfileFileSystemEntry::GetVersion()
{
  return Header->Version;
}

HardfileFileSystemEntry::HardfileFileSystemEntry(RDBFileSystemHeader *header, ULO segListAddress)
  : Header(header), SegListAddress(segListAddress)
{
}

/*====================*/
/* fhfile.device      */
/*====================*/

/*===================================*/
/* Fixed locations in fhfile_rom:    */
/* ------------------------------    */
/* offset 4088 - max number of units */
/* offset 4092 - configdev pointer   */
/*===================================*/

/*===========================================================================*/
/* Hardfile card init                                                        */
/*===========================================================================*/

void HardfileHandler_CardInit()
{
  hardfileHandler.CardInit();
}

void HardfileHandler_CardMap(ULO mapping)
{
  hardfileHandler.CardMap(mapping);
}

UBY HardfileHandler_ReadByte(ULO address)
{
  return hardfileHandler.ReadByte(address);
}

UWO HardfileHandler_ReadWord(ULO address)
{
  return hardfileHandler.ReadWord(address);
}

ULO HardfileHandler_ReadLong(ULO address)
{
  return hardfileHandler.ReadLong(address);
}

void HardfileHandler_WriteByte(UBY data, ULO address)
{
}

void HardfileHandler_WriteWord(UWO data, ULO address)
{
}

void HardfileHandler_WriteLong(ULO data, ULO address)
{
}

/*================================================================*/
/* fhfile_card_init                                               */
/* In order to obtain a configDev struct.                         */
/*================================================================*/

void HardfileHandler::CardInit()
{
  memoryEmemSet(0, 0xd1);
  memoryEmemSet(8, 0xc0);
  memoryEmemSet(4, 2);
  memoryEmemSet(0x10, 2011 >> 8);
  memoryEmemSet(0x14, 2011 & 0xf);
  memoryEmemSet(0x18, 0);
  memoryEmemSet(0x1c, 0);
  memoryEmemSet(0x20, 0);
  memoryEmemSet(0x24, 1);
  memoryEmemSet(0x28, 0x10);
  memoryEmemSet(0x2c, 0);
  memoryEmemSet(0x40, 0);
  memoryEmemMirror(0x1000, _rom + 0x1000, 0xa0);
}

/*====================================================*/
/* fhfile_card_map                                    */
/* The rom must be remapped to the location specified.*/
/*====================================================*/

void HardfileHandler::CardMap(ULO mapping)
{
  _romstart = (mapping << 8) & 0xff0000;
  ULO bank = _romstart >> 16;
  memoryBankSet(HardfileHandler_ReadByte,
    HardfileHandler_ReadWord,
    HardfileHandler_ReadLong,
    HardfileHandler_WriteByte,
    HardfileHandler_WriteWord,
    HardfileHandler_WriteLong,
    _rom,
    bank,
    bank,
    FALSE);
}

/*============================================================================*/
/* Functions to get and set data in the fhfile memory area                    */
/*============================================================================*/

UBY HardfileHandler::ReadByte(ULO address)
{
  return _rom[address & 0xffff];
}

UWO HardfileHandler::ReadWord(ULO address)
{
  UBY *p = _rom + (address & 0xffff);
  return (((UWO)p[0]) << 8) | (UWO)p[1];
}

ULO HardfileHandler::ReadLong(ULO address)
{
  UBY *p = _rom + (address & 0xffff);
  return (((ULO)p[0]) << 24) | (((ULO)p[1]) << 16) | (((ULO)p[2]) << 8) | (ULO)p[3];
}

bool HardfileHandler::HasZeroDevices()
{
  for (ULO i = 0; i < FHFILE_MAX_DEVICES; i++)
  {
    if (_devices[i].F != nullptr)
    {
      return false;
    }
  }
  return true;
}

bool HardfileHandler::PreferredNameExists(const string& preferredName)
{
  for (const HardfileMountListEntry& partition : _mountList)
  {
    if (preferredName == partition.Name)
    {
      return true;
    }    
  }
  return false;
}

string HardfileHandler::MakeDeviceName(int no)
{
  ostringstream o;
  o << "DH" << no;
  return o.str();
}

string HardfileHandler::MakeDeviceName(const string& preferredName, int no)
{
  if (!PreferredNameExists(preferredName))
  {
    return preferredName;
  }
  return MakeDeviceName(no);
}

void HardfileHandler::CreateMountList()
{
  int totalPartitionCount = 0;

  _mountList.clear();

  for (int deviceIndex = 0; deviceIndex < FHFILE_MAX_DEVICES; deviceIndex++)
  {
    if (_devices[deviceIndex].F != nullptr)
    {
      if (_devices[deviceIndex].hasRDB)
      {
        RDBHeader *rdbHeader = _devices[deviceIndex].rdb;

        for (size_t partitionIndex = 0; partitionIndex < rdbHeader->Partitions.size(); partitionIndex++)
        {
          RDBPartition *rdbPartition = rdbHeader->Partitions[partitionIndex];
          if (rdbPartition->IsAutomountable())
          {
            _mountList.push_back(HardfileMountListEntry(deviceIndex, partitionIndex, MakeDeviceName(rdbPartition->DriveName, totalPartitionCount++)));
          }
        }
      }
      else
      {
        _mountList.push_back(HardfileMountListEntry(deviceIndex, -1, MakeDeviceName(totalPartitionCount++)));
      }
    }
  }
}

int HardfileHandler::FindOlderOrSameFileSystemVersion(ULO DOSType, ULO version)
{
  int size = _fileSystems.size();
  for (int index = 0; index < size; index++)
  {
    if (_fileSystems[index].IsOlderOrSameFileSystemVersion(DOSType, version))
    {
      return index;
    }
  }
  return -1;
}

HardfileFileSystemEntry *HardfileHandler::GetFileSystemForDOSType(ULO DOSType)
{
  for (HardfileFileSystemEntry& fileSystemEntry : _fileSystems)
  {
    if (fileSystemEntry.IsDOSType(DOSType))
    {
      return &fileSystemEntry;
    }
  }
  return nullptr;
}

void HardfileHandler::AddFileSystemsFromRdb(HardfileDevice& device)
{
  if (device.F == nullptr || !device.hasRDB)
  {
    return;
  }

  for (RDBFileSystemHeader* header : device.rdb->FileSystemHeaders)
  {
    int olderVersionIndex = FindOlderOrSameFileSystemVersion(header->DOSType, header->Version);
    if (olderVersionIndex == -1)
    {
      _fileSystems.push_back(HardfileFileSystemEntry(header, 0));
    }
    else if (_fileSystems[olderVersionIndex].IsOlderVersion(header->Version))
    {
      // Replace older fs version with this one
      _fileSystems[olderVersionIndex] = HardfileFileSystemEntry(header, 0);
    }
    // Ignore if newer or same fs version already added
  }
}

void HardfileHandler::AddFileSystemsFromRdb()
{
  for (int i = 0; i < FHFILE_MAX_DEVICES; i++)
  {
    AddFileSystemsFromRdb(_devices[i]);
  }
}

void HardfileHandler::EraseOlderOrSameFileSystemVersion(ULO DOSType, ULO version)
{
  int olderOrSameVersionIndex = FindOlderOrSameFileSystemVersion(DOSType, version);
  if (olderOrSameVersionIndex != -1)
  {
    fellowAddLog("fhfile: Erased RDB filesystem entry (%.8X, %.8X), newer version (%.8X, %.8X) found in RDB or newer/same version supported by Kickstart.\n",
      _fileSystems[olderOrSameVersionIndex].GetDOSType(), _fileSystems[olderOrSameVersionIndex].GetVersion(), DOSType, version);

    _fileSystems.erase(_fileSystems.begin() + olderOrSameVersionIndex);
  }
}

void HardfileHandler::SetPhysicalGeometryFromRDB(HardfileDevice *fhfile)
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

void HardfileHandler::InitializeHardfile(ULO index)
{
  fs_navig_point *fsnp;

  if (_devices[index].F != nullptr)                     /* Close old hardfile */
  {
    fclose(_devices[index].F);
  }
  _devices[index].F = nullptr;                           /* Set config values */
  _devices[index].status = FHFILE_NONE;
  if ((fsnp = fsWrapMakePoint(_devices[index].filename)) != nullptr)
  {
    _devices[index].readonly |= (!fsnp->writeable);
    ULO size = fsnp->size;
    _devices[index].F = fopen(_devices[index].filename, _devices[index].readonly ? "rb" : "r+b");

    if (_devices[index].F != nullptr)                          /* Open file */
    {
      RDBFileReader reader(_devices[index].F);
      _devices[index].hasRDB = RDBHandler::HasRigidDiskBlock(reader);

      if (_devices[index].hasRDB)
      {
        // RDB configured hardfile
        _devices[index].rdb = RDBHandler::GetDriveInformation(reader);
        SetPhysicalGeometryFromRDB(&_devices[index]);
        _devices[index].size = size;
        _devices[index].status = FHFILE_HDF;
      }
      else
      {
        // Manually configured hardfile
        ULO track_size = (_devices[index].sectorspertrack * _devices[index].surfaces * _devices[index].bytespersector);
        if (size < track_size)
        {
          /* Error: File must be at least one track long */
          fclose(_devices[index].F);
          _devices[index].F = nullptr;
          _devices[index].status = FHFILE_NONE;
        }
        else                                                    /* File is OK */
        {
          _devices[index].tracks = size / track_size;
          _devices[index].size = _devices[index].tracks * track_size;
          _devices[index].status = FHFILE_HDF;
        }
      }
    }
    free(fsnp);
  }
}

/* Returns TRUE if a hardfile was inserted */

BOOLE HardfileHandler::RemoveHardfile(ULO index)
{
  BOOLE result = FALSE;
  if (index >= FHFILE_MAX_DEVICES)
  {
    return result;
  }
  if (_devices[index].F != nullptr)
  {
    fflush(_devices[index].F);
    fclose(_devices[index].F);
    result = TRUE;
  }
  if (_devices[index].hasRDB)
  {
    delete _devices[index].rdb;
    _devices[index].rdb = nullptr;
    _devices[index].hasRDB = false;
  }
  memset(&(_devices[index]), 0, sizeof(HardfileDevice));
  _devices[index].status = FHFILE_NONE;
  return result;
}

void HardfileHandler::SetEnabled(BOOLE enabled)
{
  _enabled = enabled;
}

BOOLE HardfileHandler::GetEnabled()
{
  return _enabled;
}

void HardfileHandler::SetHardfile(HardfileDevice hardfile, ULO index)
{
  if (index >= FHFILE_MAX_DEVICES)
  {
    return;
  }
  RemoveHardfile(index);
  strncpy(_devices[index].filename, hardfile.filename, CFG_FILENAME_LENGTH);
  _devices[index].readonly = hardfile.readonly_original;
  _devices[index].readonly_original = hardfile.readonly_original;
  _devices[index].bytespersector = (hardfile.bytespersector_original & 0xfffffffc);
  _devices[index].bytespersector_original = hardfile.bytespersector_original;
  _devices[index].sectorspertrack = hardfile.sectorspertrack;
  _devices[index].surfaces = hardfile.surfaces;
  _devices[index].reservedblocks = hardfile.reservedblocks_original;
  _devices[index].reservedblocks_original = hardfile.reservedblocks_original;
  if (_devices[index].reservedblocks < 1)
  {
    _devices[index].reservedblocks = 1;
  }
  InitializeHardfile(index);

#ifdef RETRO_PLATFORM
  if(RP.GetHeadlessMode())
    RP.SendHardDriveContent(index, hardfile.filename, hardfile.readonly_original ? true : false);
#endif
}

bool HardfileHandler::CompareHardfile(HardfileDevice hardfile, ULO index)
{
  if (index >= FHFILE_MAX_DEVICES)
  {
    return false;
  }
  return (_devices[index].readonly_original == hardfile.readonly_original) &&
    (_devices[index].bytespersector_original == hardfile.bytespersector_original) &&
    (_devices[index].sectorspertrack == hardfile.sectorspertrack) &&
    (_devices[index].surfaces == hardfile.surfaces) &&
    (_devices[index].reservedblocks_original == hardfile.reservedblocks_original) &&
    (strncmp(_devices[index].filename, hardfile.filename, CFG_FILENAME_LENGTH) == 0);
}

void HardfileHandler::Clear()
{
  for (ULO i = 0; i < FHFILE_MAX_DEVICES; i++)
  {
    RemoveHardfile(i);
  }
  _fileSystems.clear();
  _mountList.clear();
}

/*===================*/
/* Set HD led symbol */
/*===================*/

void HardfileHandler::SetLed(bool state)
{
  drawSetLED(4, state);
}

/*==================*/
/* BeginIO Commands */
/*==================*/

void HardfileHandler::Ignore(ULO index)
{
  memoryWriteLong(0, cpuGetAReg(1) + 32);
  cpuSetDReg(0, 0);
}

BYT HardfileHandler::Read(ULO index)
{
  ULO dest = memoryReadLong(cpuGetAReg(1) + 40);
  ULO offset = memoryReadLong(cpuGetAReg(1) + 44);
  ULO length = memoryReadLong(cpuGetAReg(1) + 36);

  if ((offset + length) > _devices[index].size)
  {
    return -3;
  }
  
  SetLed(true); // NOTE: This has no effect in standalone since nothing updates the screen in-between

#ifdef RETRO_PLATFORM
  if(RP.GetHeadlessMode())
     RP.PostHardDriveLED(index, true, false);
#endif

  fseek(_devices[index].F, offset, SEEK_SET);
  fread(memoryAddressToPtr(dest), 1, length, _devices[index].F);
  memoryWriteLong(length, cpuGetAReg(1) + 32);

  SetLed(false); // NOTE: This has no effect in standalone since nothing updates the screen in-between

#ifdef RETRO_PLATFORM
  if(RP.GetHeadlessMode())
     RP.PostHardDriveLED(index, false, false);
#endif

  return 0;
}

BYT HardfileHandler::Write(ULO index)
{
  ULO dest = memoryReadLong(cpuGetAReg(1) + 40);
  ULO offset = memoryReadLong(cpuGetAReg(1) + 44);
  ULO length = memoryReadLong(cpuGetAReg(1) + 36);

  if (_devices[index].readonly || (offset + length) > _devices[index].size)
  {
    return -3;
  }

  SetLed(true); // NOTE: This has no effect in standalone since nothing updates the screen in-between

  #ifdef RETRO_PLATFORM
  if(RP.GetHeadlessMode())
     RP.PostHardDriveLED(index, true, true);
#endif

  fseek(_devices[index].F, offset, SEEK_SET);
  fwrite(memoryAddressToPtr(dest), 1, length, _devices[index].F);
  memoryWriteLong(length, cpuGetAReg(1) + 32);

  SetLed(false); // NOTE: This has no effect in standalone since nothing updates the screen in-between

 #ifdef RETRO_PLATFORM
  if(RP.GetHeadlessMode())
     RP.PostHardDriveLED(index, false, true);
#endif

  return 0;
}

void HardfileHandler::GetNumberOfTracks(ULO index)
{
  memoryWriteLong(_devices[index].tracks, cpuGetAReg(1) + 32);
}

void HardfileHandler::GetDriveType(ULO index)
{
  memoryWriteLong(1, cpuGetAReg(1) + 32);
}

void HardfileHandler::WriteProt(ULO index)
{
  memoryWriteLong(_devices[index].readonly, cpuGetAReg(1) + 32);
}

/*======================================================*/
/* fhfileDiag native callback                           */
/*                                                      */
/* Pointer to our configdev struct is stored in $f40ffc */
/* For later use when filling out the bootnode          */
/*======================================================*/

void HardfileHandler::DoDiag()
{
  _configdev = cpuGetAReg(3);
  memoryDmemSetLongNoCounter(FHFILE_MAX_DEVICES, 4088);
  memoryDmemSetLongNoCounter(_configdev, 4092);
  cpuSetDReg(0, 1);
}

/*======================================*/
/* Native callbacks for device commands */
/*======================================*/

void HardfileHandler::DoOpen()
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

void HardfileHandler::DoClose()
{
  memoryWriteLong(memoryReadLong(cpuGetAReg(6) + 32) - 1, cpuGetAReg(6) + 32);    /* LIB_OPENCNT */
  cpuSetDReg(0, 0);                                /* ? */
}

void HardfileHandler::DoExpunge()
{
  cpuSetDReg(0, 0);                                /* ? */
}

void HardfileHandler::DoNULL()
{
}

void HardfileHandler::DoBeginIO()
{
  BYT error = 0;
  ULO unit = memoryReadLong(cpuGetAReg(1) + 24);

  UWO cmd = memoryReadWord(cpuGetAReg(1) + 28);
  switch (cmd)
  {
    case 2:
      error = Read(unit);
      break;
    case 3:
    case 11:
      error = Write(unit);
      break;
    case 18:
      GetDriveType(unit);
      break;
    case 19:
      GetNumberOfTracks(unit);
      break;
    case 15:
      WriteProt(unit);
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
      Ignore(unit);
      break;
    default:
      error = -3;
      cpuSetDReg(0, 0);
      break;
  }
  memoryWriteByte(5, cpuGetAReg(1) + 8);      /* ln_type */
  memoryWriteByte(error, cpuGetAReg(1) + 31); /* ln_error */
}

void HardfileHandler::DoAbortIO()
{
  cpuSetDReg(0, -3);
}

// RDB support functions, native callbacks

// Returns the number of RDB filesystem headers in D0
void HardfileHandler::DoGetRDBFileSystemCount()
{
  ULO count = _fileSystems.size();

  fellowAddLog("DoGetRDBFilesystemCount() - Returns %d\n", count);

  cpuSetDReg(0, count);
}

void HardfileHandler::DoGetRDBFileSystemHunkCount()
{
  ULO fsIndex = cpuGetDReg(1);
  ULO hunkCount = _fileSystems[fsIndex].Header->FileSystemHandler.Hunks.size();

  fellowAddLog("ffhfile: DoGetRDBFileSystemHunkCount() fsIndex %d hunkCount %d\n", fsIndex, hunkCount);

  cpuSetDReg(0, hunkCount);
}

void HardfileHandler::DoGetRDBFileSystemHunkSize()
{
  ULO fsIndex = cpuGetDReg(1);
  ULO hunkIndex = cpuGetDReg(2);
  ULO hunkSize = _fileSystems[fsIndex].Header->FileSystemHandler.Hunks[hunkIndex]->GetSizeInLongwords() * 4;

  fellowAddLog("ffhfile: DoGetRDBFileSystemHunkSize() fsIndex %d hunkIndex %d hunkSize %d\n", fsIndex, hunkIndex, hunkSize);

  cpuSetDReg(0, hunkSize);
}

void HardfileHandler::DoRelocateHunk()
{
  ULO destination = cpuGetDReg(0);
  ULO fsIndex = cpuGetDReg(1);
  ULO hunkIndex = cpuGetDReg(2);

  fellowAddLog("ffhfile: DoRelocateHunk() destination %.8X fsIndex %d hunkIndex %d\n", destination, fsIndex, hunkIndex);

  HardfileFileSystemEntry& fileSystem = _fileSystems[fsIndex];

  UBY *relocatedHunkData = fileSystem.Header->FileSystemHandler.Hunks[hunkIndex]->Relocate(destination + 8);
  if (relocatedHunkData == nullptr)
  {
    fellowAddLog("fhfile: RDB Filesystem - DoRelocateHunk() called for hunk with no data.\n");
    return;
  }

  if (fileSystem.SegListAddress == 0)
  {
    fileSystem.SegListAddress = destination + 4;
  }

  ULO hunkSize = fileSystem.Header->FileSystemHandler.Hunks[hunkIndex]->GetSizeInLongwords() * 4;
  memoryWriteLong(hunkSize + 8, destination);
  memoryWriteLong(0, destination + 4);  // No next segment for now

  for (ULO i = 0; i < hunkSize; i++)
  {
    memoryWriteByte(relocatedHunkData[i], destination + i + 8);
  }
  delete relocatedHunkData;
}

void HardfileHandler::DoInitializeRDBFileSystemEntry()
{
  ULO fsEntry = cpuGetDReg(0);
  ULO fsIndex = cpuGetDReg(1);

  fellowAddLog("ffhfile: DoInitializeRDBFileSystemEntry() fsEntry %.8X fsIndex %d\n", fsEntry, fsIndex);

  RDBFileSystemHeader *fsHeader = _fileSystems[fsIndex].Header;

  memoryWriteLong(_fsname, fsEntry + 10);
  memoryWriteLong(fsHeader->DOSType, fsEntry + 14);
  memoryWriteLong(fsHeader->Version, fsEntry + 18);
  memoryWriteLong(fsHeader->PatchFlags, fsEntry + 22);
  memoryWriteLong(fsHeader->DnType, fsEntry + 26);
  memoryWriteLong(fsHeader->DnTask, fsEntry + 30);
  memoryWriteLong(fsHeader->DnLock, fsEntry + 34);
  memoryWriteLong(fsHeader->DnHandler, fsEntry + 38);
  memoryWriteLong(fsHeader->DnStackSize, fsEntry + 42);
  memoryWriteLong(fsHeader->DnPriority, fsEntry + 46);
  memoryWriteLong(fsHeader->DnStartup, fsEntry + 50);
  memoryWriteLong(_fileSystems[fsIndex].SegListAddress>>2, fsEntry + 54);
//  memoryWriteLong(fsHeader->DnSegListBlock, fsEntry + 54);
  memoryWriteLong(fsHeader->DnGlobalVec, fsEntry + 58);

  for (int i = 0; i < 23; i++)
  {
    memoryWriteLong(fsHeader->Reserved2[i], fsEntry + 62 + i*4);
  }
}

string HardfileHandler::LogGetStringFromMemory(ULO address)
{
  string name;
  if (address == 0)
  {
    return string();
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
void HardfileHandler::DoLogAvailableResources()
{
  fellowAddLog("fhfileDoLogAvailableResources()\n");

  ULO execBase = memoryReadLong(4);
  ULO rsListHeader = memoryReadLong(execBase + 0x150);

  fellowAddLog("Resource list header (%.8X): Head %.8X Tail %.8X TailPred %.8X Type %d\n", rsListHeader, memoryReadLong(rsListHeader), memoryReadLong(rsListHeader + 4),
    memoryReadLong(rsListHeader + 8), memoryReadByte(rsListHeader + 9));

  if (rsListHeader == memoryReadLong(rsListHeader + 8))
  {
    fellowAddLog("fhfile: Resource list is empty.\n");
    return;
  }

  ULO fsNode = memoryReadLong(rsListHeader);
  while (fsNode != 0 && (fsNode != rsListHeader + 4))
  {
    fellowAddLog("fhfile: ResourceEntry Node (%.8X): Succ %.8X Pred %.8X Type %d Pri %d NodeName '%s'\n", fsNode, memoryReadLong(fsNode), memoryReadLong(fsNode + 4),
      memoryReadByte(fsNode + 8), memoryReadByte(fsNode + 9), LogGetStringFromMemory(memoryReadLong(fsNode + 10)).c_str());

    fsNode = memoryReadLong(fsNode);
  }
}

void HardfileHandler::DoLogAllocMemResult()
{
  fellowAddLog("fhfile: AllocMem() returned %.8X\n", cpuGetDReg(0));
}

void HardfileHandler::DoLogOpenResourceResult()
{
  fellowAddLog("fhfile: OpenResource() returned %.8X\n", cpuGetDReg(0));
}

// D0 - pointer to FileSystem.resource
void HardfileHandler::DoRemoveRDBFileSystemsAlreadySupportedBySystem()
{
  fellowAddLog("fhfile: fhfileDoRemoveRDBFileSystemsAlreadySupportedBySystem()\n");

  ULO fsResource = cpuGetDReg(0);
  fellowAddLog("fhfile: FileSystem.resource list node (%.8X): Succ %.8X Pred %.8X Type %d Pri %d NodeName '%s' Creator '%s'\n", fsResource, memoryReadLong(fsResource),
    memoryReadLong(fsResource + 4), memoryReadByte(fsResource + 8), memoryReadByte(fsResource + 9), LogGetStringFromMemory(memoryReadLong(fsResource + 10)).c_str(),
    LogGetStringFromMemory(memoryReadLong(fsResource + 14)).c_str());

  ULO fsList = fsResource + 18;
  fellowAddLog("fhfile: FileSystemEntries list header (%.8X): Head %.8X Tail %.8X TailPred %.8X Type %d\n", fsList, memoryReadLong(fsList), memoryReadLong(fsList + 4),
    memoryReadLong(fsList + 8), memoryReadByte(fsList + 9));

  if (fsList == memoryReadLong(fsList + 8))
  {
    fellowAddLog("fhfile: FileSystemEntry list is empty.\n");
    return;
  }

  ULO fsNode = memoryReadLong(fsList);
  while (fsNode != 0 && (fsNode != fsList + 4))
  {
    fellowAddLog("fhfile: FileSystemEntry Node (%.8X): Succ %.8X Pred %.8X Type %d Pri %d NodeName '%s'\n", fsNode, memoryReadLong(fsNode), memoryReadLong(fsNode + 4),
      memoryReadByte(fsNode + 8), memoryReadByte(fsNode + 9), LogGetStringFromMemory(memoryReadLong(fsNode + 10)).c_str());

    ULO fsEntry = fsNode + 14;

    ULO dosType = memoryReadLong(fsEntry);
    ULO version = memoryReadLong(fsEntry + 4);

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

    EraseOlderOrSameFileSystemVersion(dosType, version);    
  }
}

// D0 - pointer to FileSystem.resource
void HardfileHandler::DoLogAvailableFileSystems()
{
  fellowAddLog("fhfile: DoLogAvailableFileSystems()\n");

  ULO fsResource = cpuGetAReg(0);
  fellowAddLog("fhfile: FileSystem.resource list node (%.8X): Succ %.8X Pred %.8X Type %d Pri %d NodeName '%s' Creator '%s'\n", fsResource, memoryReadLong(fsResource),
    memoryReadLong(fsResource + 4), memoryReadByte(fsResource + 8), memoryReadByte(fsResource + 9), LogGetStringFromMemory(memoryReadLong(fsResource + 10)).c_str(),
    LogGetStringFromMemory(memoryReadLong(fsResource + 14)).c_str());

  ULO fsList = fsResource + 18;
  fellowAddLog("fhfile: FileSystemEntries list header (%.8X): Head %.8X Tail %.8X TailPred %.8X Type %d\n", fsList, memoryReadLong(fsList), memoryReadLong(fsList + 4),
    memoryReadLong(fsList + 8), memoryReadByte(fsList + 9));

  if (fsList == memoryReadLong(fsList + 8))
  {
    fellowAddLog("fhfile: FileSystemEntry list is empty.\n");
    return;
  }

  ULO fsNode = memoryReadLong(fsList);
  while (fsNode != 0 && (fsNode != fsList + 4))
  {
    fellowAddLog("fhfile: FileSystemEntry Node (%.8X): Succ %.8X Pred %.8X Type %d Pri %d NodeName '%s'\n", fsNode, memoryReadLong(fsNode), memoryReadLong(fsNode + 4),
      memoryReadByte(fsNode + 8), memoryReadByte(fsNode + 9), LogGetStringFromMemory(memoryReadLong(fsNode + 10)).c_str());

    ULO fsEntry = fsNode + 14;

    ULO dosType = memoryReadLong(fsEntry);
    ULO version = memoryReadLong(fsEntry + 4);

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

void HardfileHandler::DoPatchDOSDeviceNode()
{
  ULO node = cpuGetDReg(0);
  ULO packet = cpuGetAReg(5);

  fellowAddLog("fhfile: DoPatchDOSDeviceNode node %.8X packet %.8X\n", node, packet);

  memoryWriteLong(0, node + 8); // dn_Task = 0
  memoryWriteLong(0, node + 16); // dn_Handler = 0
  memoryWriteLong(-1, node + 36); // dn_GlobalVec = -1

  HardfileFileSystemEntry *fs = GetFileSystemForDOSType(memoryReadLong(packet + 80));
  if (fs != nullptr)
  {
    if (fs->Header->PatchFlags & 0x10)
    {
      memoryWriteLong(fs->Header->DnStackSize, node + 20);
    }
    if (fs->Header->PatchFlags & 0x80)
    {
      memoryWriteLong(fs->SegListAddress>>2, node + 32);
    }
    if (fs->Header->PatchFlags & 0x100)
    {
      memoryWriteLong(fs->Header->DnGlobalVec, node + 36);
    }
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

void HardfileHandler::Do(ULO data)
{
  ULO type = data >> 16;
  ULO operation = data & 0xffff;
  if (type == 1)
  {
    switch (operation)
    {
      case 1:
        DoDiag();
        break;
      case 2:
        DoOpen();
        break;
      case 3:
        DoClose();
        break;
      case 4:
        DoExpunge();
        break;
      case 5:
        DoNULL();
        break;
      case 6:
        DoBeginIO();
        break;
      case 7:
        DoAbortIO();
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
        DoGetRDBFileSystemCount();
        break;
      case 2:
        DoGetRDBFileSystemHunkCount();
        break;
      case 3:
        DoGetRDBFileSystemHunkSize();
        break;
      case 4:
        DoRelocateHunk();
        break;
      case 5:
        DoInitializeRDBFileSystemEntry();
        break;
      case 6:
        DoRemoveRDBFileSystemsAlreadySupportedBySystem();
        break;
      case 7:
        DoPatchDOSDeviceNode();
        break;
      case 0xa0:
        DoLogAllocMemResult();
        break;
      case 0xa1:
        DoLogOpenResourceResult();
        break;
      case 0xa2:
        DoLogAvailableResources();
        break;
      case 0xa3:
        DoLogAvailableFileSystems();
        break;
    }
  }
}

/*=================================================*/
/* Make a dosdevice packet about the device layout */
/*=================================================*/

void HardfileHandler::MakeDOSDevPacketForPlainHardfile(const HardfileMountListEntry& mountListEntry, ULO deviceNameAddress)
{
  const HardfileDevice& device = _devices[mountListEntry.DeviceIndex];
  if (device.F != nullptr)
  {
    memoryDmemSetLong(mountListEntry.DeviceIndex);            /* Flag to initcode */

    memoryDmemSetLong(mountListEntry.NameAddress);            /*  0 Unit name "FELLOWX" or similar */
    memoryDmemSetLong(deviceNameAddress);                     /*  4 Device name "fhfile.device" */
    // Might need to be partition index?
    memoryDmemSetLong(mountListEntry.DeviceIndex);            /*  8 Unit # */
    memoryDmemSetLong(0);                                     /* 12 OpenDevice flags */

    // Struct DosEnvec
    memoryDmemSetLong(16);                                    /* 16 Environment size in long words*/
    memoryDmemSetLong(device.bytespersector >> 2);            /* 20 Longwords in a block */
    memoryDmemSetLong(0);                                     /* 24 sector origin (unused) */
    memoryDmemSetLong(device.surfaces);                       /* 28 Heads */
    memoryDmemSetLong(1);                                     /* 32 Sectors per logical block (unused) */
    memoryDmemSetLong(device.sectorspertrack);                /* 36 Sectors per track */
    memoryDmemSetLong(device.reservedblocks);                 /* 40 Reserved blocks, min. 1 */
    memoryDmemSetLong(0);                                     /* 44 mdn_prefac - Unused */
    memoryDmemSetLong(0);                                     /* 48 Interleave */
    memoryDmemSetLong(0);                                     /* 52 Lower cylinder */
    memoryDmemSetLong(device.tracks - 1);                     /* 56 Upper cylinder */
    memoryDmemSetLong(0);                                     /* 60 Number of buffers */
    memoryDmemSetLong(0);                                     /* 64 Type of memory for buffers */
    memoryDmemSetLong(0x7fffffff);                            /* 68 Largest transfer */
    memoryDmemSetLong(~1U);                                   /* 72 Add mask */
    memoryDmemSetLong(-1);                                    /* 76 Boot priority */
    memoryDmemSetLong(0x444f5300);                            /* 80 DOS file handler name */
    memoryDmemSetLong(0);
  }
}

void HardfileHandler::MakeDOSDevPacketForRDBPartition(const HardfileMountListEntry& mountListEntry, ULO deviceNameAddress)
{
  const HardfileDevice& device = _devices[mountListEntry.DeviceIndex];
  const RDBPartition* partition = device.rdb->Partitions[mountListEntry.PartitionIndex];
  if (device.F != nullptr)
  {
    memoryDmemSetLong(mountListEntry.DeviceIndex);            /* Flag to initcode */

    memoryDmemSetLong(mountListEntry.NameAddress);       /*  0 Unit name "FELLOWX" or similar */
    memoryDmemSetLong(deviceNameAddress);                     /*  4 Device name "fhfile.device" */
    memoryDmemSetLong(mountListEntry.DeviceIndex);            /*  8 Unit # */
    memoryDmemSetLong(0);                                     /* 12 OpenDevice flags */

    // Struct DosEnvec
    memoryDmemSetLong(16);                                    /* 16 Environment size in long words*/
    memoryDmemSetLong(partition->SizeBlock);                  /* 20 Longwords in a block */
    memoryDmemSetLong(partition->SecOrg);                     /* 24 sector origin (unused) */
    memoryDmemSetLong(partition->Surfaces);                   /* 28 Heads */
    memoryDmemSetLong(partition->SectorsPerBlock);            /* 32 Sectors per logical block (unused) */
    memoryDmemSetLong(partition->BlocksPerTrack);             /* 36 Sectors per track */
    memoryDmemSetLong(partition->Reserved);                   /* 40 Reserved blocks, min. 1 */
    memoryDmemSetLong(partition->PreAlloc);                   /* 44 mdn_prefac - Unused */
    memoryDmemSetLong(partition->Interleave);                 /* 48 Interleave */
    memoryDmemSetLong(partition->LowCylinder);                /* 52 Lower cylinder */
    memoryDmemSetLong(partition->HighCylinder);               /* 56 Upper cylinder */
    memoryDmemSetLong(partition->NumBuffer);                  /* 60 Number of buffers */
    memoryDmemSetLong(partition->BufMemType);                 /* 64 Type of memory for buffers */
    memoryDmemSetLong(partition->MaxTransfer);                /* 68 Largest transfer */
    memoryDmemSetLong(partition->Mask);                       /* 72 Add mask */
    memoryDmemSetLong(partition->BootPri);                    /* 76 Boot priority */
    memoryDmemSetLong(partition->DOSType);                    /* 80 DOS file handler name */
    memoryDmemSetLong(0);
  }
}

/*===========================================================*/
/* fhfileHardReset                                           */
/* This will set up the device structures and stubs          */
/* Can be called at every reset, but really only needed once */
/*===========================================================*/

void HardfileHandler::HardReset()
{
  _fileSystems.clear();
  CreateMountList();

  if (!HasZeroDevices() && GetEnabled() && memoryGetKickImageVersion() >= 34)
  {
    memoryDmemSetCounter(0);

    /* Device-name and ID string */

    ULO devicename = memoryDmemGetCounter();
    memoryDmemSetString("fhfile.device");
    ULO idstr = memoryDmemGetCounter();
    memoryDmemSetString("Fellow Hardfile device V5");
    ULO doslibname = memoryDmemGetCounter();
    memoryDmemSetString("dos.library");
    _fsname = memoryDmemGetCounter();
    memoryDmemSetString("Fellow hardfile RDB fs");

    /* Device name as seen in Amiga DOS */

    for (HardfileMountListEntry& mountListEntry : _mountList)
    {
      mountListEntry.NameAddress = memoryDmemGetCounter();
      memoryDmemSetString(mountListEntry.Name.c_str());
    }

    /* fhfile.open */

    ULO fhfile_t_open = memoryDmemGetCounter();
    memoryDmemSetWord(0x23fc);
    memoryDmemSetLong(0x00010002); memoryDmemSetLong(0xf40000); /* move.l #$00010002,$f40000 */
    memoryDmemSetWord(0x4e75);                                  /* rts */

    /* fhfile.close */

    ULO fhfile_t_close = memoryDmemGetCounter();
    memoryDmemSetWord(0x23fc);
    memoryDmemSetLong(0x00010003); memoryDmemSetLong(0xf40000); /* move.l #$00010003,$f40000 */
    memoryDmemSetWord(0x4e75);                                  /* rts */

    /* fhfile.expunge */

    ULO fhfile_t_expunge = memoryDmemGetCounter();
    memoryDmemSetWord(0x23fc);
    memoryDmemSetLong(0x00010004); memoryDmemSetLong(0xf40000); /* move.l #$00010004,$f40000 */
    memoryDmemSetWord(0x4e75);                                  /* rts */

    /* fhfile.null */

    ULO fhfile_t_null = memoryDmemGetCounter();
    memoryDmemSetWord(0x23fc);
    memoryDmemSetLong(0x00010005); memoryDmemSetLong(0xf40000); /* move.l #$00010005,$f40000 */
    memoryDmemSetWord(0x4e75);                                  /* rts */

    /* fhfile.beginio */

    ULO fhfile_t_beginio = memoryDmemGetCounter();
    memoryDmemSetWord(0x23fc);
    memoryDmemSetLong(0x00010006); memoryDmemSetLong(0xf40000); /* move.l #$00010006,$f40000 */
    memoryDmemSetLong(0x48e78002);                              /* movem.l d0/a6,-(a7) */
    memoryDmemSetLong(0x08290000); memoryDmemSetWord(0x001e);   /* btst   #$0,30(a1)   */
    memoryDmemSetWord(0x6608);                                  /* bne    (to rts)     */
    memoryDmemSetLong(0x2c780004);                              /* move.l $4.w,a6      */
    memoryDmemSetLong(0x4eaefe86);                              /* jsr    -378(a6)     */
    memoryDmemSetLong(0x4cdf4001);                              /* movem.l (a7)+,d0/a6 */
    memoryDmemSetWord(0x4e75);                                  /* rts */

    /* fhfile.abortio */

    ULO fhfile_t_abortio = memoryDmemGetCounter();
    memoryDmemSetWord(0x23fc);
    memoryDmemSetLong(0x00010007); memoryDmemSetLong(0xf40000); /* move.l #$00010007,$f40000 */
    memoryDmemSetWord(0x4e75);                                  /* rts */

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

    _bootcode = memoryDmemGetCounter();
    memoryDmemSetWord(0x227c); memoryDmemSetLong(doslibname); /* move.l #doslibname,a1 */
    memoryDmemSetLong(0x4eaeffa0);                            /* jsr    -96(a6) ; FindResident() */
    memoryDmemSetWord(0x2040);                                /* move.l d0,a0 */
    memoryDmemSetLong(0x20280016);                            /* move.l 22(a0),d0 */
    memoryDmemSetWord(0x2040);                                /* move.l d0,a0 */
    memoryDmemSetWord(0x4e90);                                /* jsr    (a0) */
    memoryDmemSetWord(0x4e75);                                /* rts */

    /* fhfile.init */

    ULO fhfile_t_init = memoryDmemGetCounter();

//#include "c:\\temp\out.c"

    memoryDmemSetByte(0x48); memoryDmemSetByte(0xE7); memoryDmemSetByte(0xFF); memoryDmemSetByte(0xFE);
    memoryDmemSetByte(0x61); memoryDmemSetByte(0x00); memoryDmemSetByte(0x00); memoryDmemSetByte(0xF4);
    memoryDmemSetByte(0x4A); memoryDmemSetByte(0x80); memoryDmemSetByte(0x67); memoryDmemSetByte(0x24);
    memoryDmemSetByte(0x61); memoryDmemSetByte(0x00); memoryDmemSetByte(0x00); memoryDmemSetByte(0xD4);
    memoryDmemSetByte(0x61); memoryDmemSetByte(0x00); memoryDmemSetByte(0x01); memoryDmemSetByte(0x96);
    memoryDmemSetByte(0x4A); memoryDmemSetByte(0x80); memoryDmemSetByte(0x66); memoryDmemSetByte(0x0C);
    memoryDmemSetByte(0x61); memoryDmemSetByte(0x00); memoryDmemSetByte(0x01); memoryDmemSetByte(0x50);
    memoryDmemSetByte(0x61); memoryDmemSetByte(0x00); memoryDmemSetByte(0x01); memoryDmemSetByte(0x8A);
    memoryDmemSetByte(0x4A); memoryDmemSetByte(0x80); memoryDmemSetByte(0x67); memoryDmemSetByte(0x0C);
    memoryDmemSetByte(0x61); memoryDmemSetByte(0x00); memoryDmemSetByte(0x01); memoryDmemSetByte(0x10);
    memoryDmemSetByte(0x61); memoryDmemSetByte(0x00); memoryDmemSetByte(0x01); memoryDmemSetByte(0xEC);
    memoryDmemSetByte(0x61); memoryDmemSetByte(0x00); memoryDmemSetByte(0x00); memoryDmemSetByte(0xC0);
    memoryDmemSetByte(0x2C); memoryDmemSetByte(0x78); memoryDmemSetByte(0x00); memoryDmemSetByte(0x04);
    memoryDmemSetByte(0x43); memoryDmemSetByte(0xFA); memoryDmemSetByte(0x01); memoryDmemSetByte(0xFE);
    memoryDmemSetByte(0x4E); memoryDmemSetByte(0xAE); memoryDmemSetByte(0xFE); memoryDmemSetByte(0x68);
    memoryDmemSetByte(0x28); memoryDmemSetByte(0x40); memoryDmemSetByte(0x41); memoryDmemSetByte(0xFA);
    memoryDmemSetByte(0x02); memoryDmemSetByte(0x32); memoryDmemSetByte(0x2E); memoryDmemSetByte(0x08);
    memoryDmemSetByte(0x20); memoryDmemSetByte(0x47); memoryDmemSetByte(0x4A); memoryDmemSetByte(0x90);
    memoryDmemSetByte(0x6B); memoryDmemSetByte(0x00); memoryDmemSetByte(0x00); memoryDmemSetByte(0x70);
    memoryDmemSetByte(0x58); memoryDmemSetByte(0x87); memoryDmemSetByte(0x20); memoryDmemSetByte(0x3C);
    memoryDmemSetByte(0x00); memoryDmemSetByte(0x00); memoryDmemSetByte(0x00); memoryDmemSetByte(0x58);
    memoryDmemSetByte(0x61); memoryDmemSetByte(0x00); memoryDmemSetByte(0x00); memoryDmemSetByte(0xF8);
    memoryDmemSetByte(0x2A); memoryDmemSetByte(0x40); memoryDmemSetByte(0x20); memoryDmemSetByte(0x47);
    memoryDmemSetByte(0x70); memoryDmemSetByte(0x54); memoryDmemSetByte(0x2B); memoryDmemSetByte(0xB0);
    memoryDmemSetByte(0x08); memoryDmemSetByte(0x00); memoryDmemSetByte(0x08); memoryDmemSetByte(0x00);
    memoryDmemSetByte(0x59); memoryDmemSetByte(0x80); memoryDmemSetByte(0x64); memoryDmemSetByte(0xF6);
    memoryDmemSetByte(0xCD); memoryDmemSetByte(0x4C); memoryDmemSetByte(0x20); memoryDmemSetByte(0x4D);
    memoryDmemSetByte(0x4E); memoryDmemSetByte(0xAE); memoryDmemSetByte(0xFF); memoryDmemSetByte(0x70);
    memoryDmemSetByte(0xCD); memoryDmemSetByte(0x4C); memoryDmemSetByte(0x61); memoryDmemSetByte(0x00);
    memoryDmemSetByte(0x00); memoryDmemSetByte(0xCE); memoryDmemSetByte(0x26); memoryDmemSetByte(0x40);
    memoryDmemSetByte(0x70); memoryDmemSetByte(0x14); memoryDmemSetByte(0x61); memoryDmemSetByte(0x00);
    memoryDmemSetByte(0x00); memoryDmemSetByte(0xD2); memoryDmemSetByte(0x22); memoryDmemSetByte(0x47);
    memoryDmemSetByte(0x2C); memoryDmemSetByte(0x29); memoryDmemSetByte(0xFF); memoryDmemSetByte(0xFC);
    memoryDmemSetByte(0x22); memoryDmemSetByte(0x40); memoryDmemSetByte(0x70); memoryDmemSetByte(0x00);
    memoryDmemSetByte(0x22); memoryDmemSetByte(0x80); memoryDmemSetByte(0x23); memoryDmemSetByte(0x40);
    memoryDmemSetByte(0x00); memoryDmemSetByte(0x04); memoryDmemSetByte(0x33); memoryDmemSetByte(0x40);
    memoryDmemSetByte(0x00); memoryDmemSetByte(0x0E); memoryDmemSetByte(0x33); memoryDmemSetByte(0x7C);
    memoryDmemSetByte(0x10); memoryDmemSetByte(0xFF); memoryDmemSetByte(0x00); memoryDmemSetByte(0x08);
    memoryDmemSetByte(0x9D); memoryDmemSetByte(0x69); memoryDmemSetByte(0x00); memoryDmemSetByte(0x08);
    memoryDmemSetByte(0x23); memoryDmemSetByte(0x79); memoryDmemSetByte(0x00); memoryDmemSetByte(0xF4);
    memoryDmemSetByte(0x0F); memoryDmemSetByte(0xFC); memoryDmemSetByte(0x00); memoryDmemSetByte(0x0A);
    memoryDmemSetByte(0x23); memoryDmemSetByte(0x4B); memoryDmemSetByte(0x00); memoryDmemSetByte(0x10);
    memoryDmemSetByte(0x41); memoryDmemSetByte(0xEC); memoryDmemSetByte(0x00); memoryDmemSetByte(0x4A);
    memoryDmemSetByte(0x4E); memoryDmemSetByte(0xAE); memoryDmemSetByte(0xFE); memoryDmemSetByte(0xF2);
    memoryDmemSetByte(0x06); memoryDmemSetByte(0x87); memoryDmemSetByte(0x00); memoryDmemSetByte(0x00);
    memoryDmemSetByte(0x00); memoryDmemSetByte(0x58); memoryDmemSetByte(0x60); memoryDmemSetByte(0x00);
    memoryDmemSetByte(0xFF); memoryDmemSetByte(0x8C); memoryDmemSetByte(0x2C); memoryDmemSetByte(0x78);
    memoryDmemSetByte(0x00); memoryDmemSetByte(0x04); memoryDmemSetByte(0x22); memoryDmemSetByte(0x4C);
    memoryDmemSetByte(0x4E); memoryDmemSetByte(0xAE); memoryDmemSetByte(0xFE); memoryDmemSetByte(0x62);
    memoryDmemSetByte(0x4C); memoryDmemSetByte(0xDF); memoryDmemSetByte(0x7F); memoryDmemSetByte(0xFF);
    memoryDmemSetByte(0x4E); memoryDmemSetByte(0x75); memoryDmemSetByte(0x23); memoryDmemSetByte(0xFC);
    memoryDmemSetByte(0x00); memoryDmemSetByte(0x02); memoryDmemSetByte(0x00); memoryDmemSetByte(0xA0);
    memoryDmemSetByte(0x00); memoryDmemSetByte(0xF4); memoryDmemSetByte(0x00); memoryDmemSetByte(0x00);
    memoryDmemSetByte(0x4E); memoryDmemSetByte(0x75); memoryDmemSetByte(0x23); memoryDmemSetByte(0xFC);
    memoryDmemSetByte(0x00); memoryDmemSetByte(0x02); memoryDmemSetByte(0x00); memoryDmemSetByte(0xA1);
    memoryDmemSetByte(0x00); memoryDmemSetByte(0xF4); memoryDmemSetByte(0x00); memoryDmemSetByte(0x00);
    memoryDmemSetByte(0x4E); memoryDmemSetByte(0x75); memoryDmemSetByte(0x23); memoryDmemSetByte(0xFC);
    memoryDmemSetByte(0x00); memoryDmemSetByte(0x02); memoryDmemSetByte(0x00); memoryDmemSetByte(0xA2);
    memoryDmemSetByte(0x00); memoryDmemSetByte(0xF4); memoryDmemSetByte(0x00); memoryDmemSetByte(0x00);
    memoryDmemSetByte(0x4E); memoryDmemSetByte(0x75); memoryDmemSetByte(0x23); memoryDmemSetByte(0xFC);
    memoryDmemSetByte(0x00); memoryDmemSetByte(0x02); memoryDmemSetByte(0x00); memoryDmemSetByte(0xA3);
    memoryDmemSetByte(0x00); memoryDmemSetByte(0xF4); memoryDmemSetByte(0x00); memoryDmemSetByte(0x00);
    memoryDmemSetByte(0x4E); memoryDmemSetByte(0x75); memoryDmemSetByte(0x23); memoryDmemSetByte(0xFC);
    memoryDmemSetByte(0x00); memoryDmemSetByte(0x02); memoryDmemSetByte(0x00); memoryDmemSetByte(0x01);
    memoryDmemSetByte(0x00); memoryDmemSetByte(0xF4); memoryDmemSetByte(0x00); memoryDmemSetByte(0x00);
    memoryDmemSetByte(0x4E); memoryDmemSetByte(0x75); memoryDmemSetByte(0x23); memoryDmemSetByte(0xFC);
    memoryDmemSetByte(0x00); memoryDmemSetByte(0x02); memoryDmemSetByte(0x00); memoryDmemSetByte(0x02);
    memoryDmemSetByte(0x00); memoryDmemSetByte(0xF4); memoryDmemSetByte(0x00); memoryDmemSetByte(0x00);
    memoryDmemSetByte(0x4E); memoryDmemSetByte(0x75); memoryDmemSetByte(0x23); memoryDmemSetByte(0xFC);
    memoryDmemSetByte(0x00); memoryDmemSetByte(0x02); memoryDmemSetByte(0x00); memoryDmemSetByte(0x03);
    memoryDmemSetByte(0x00); memoryDmemSetByte(0xF4); memoryDmemSetByte(0x00); memoryDmemSetByte(0x00);
    memoryDmemSetByte(0x4E); memoryDmemSetByte(0x75); memoryDmemSetByte(0x23); memoryDmemSetByte(0xFC);
    memoryDmemSetByte(0x00); memoryDmemSetByte(0x02); memoryDmemSetByte(0x00); memoryDmemSetByte(0x04);
    memoryDmemSetByte(0x00); memoryDmemSetByte(0xF4); memoryDmemSetByte(0x00); memoryDmemSetByte(0x00);
    memoryDmemSetByte(0x4E); memoryDmemSetByte(0x75); memoryDmemSetByte(0x23); memoryDmemSetByte(0xFC);
    memoryDmemSetByte(0x00); memoryDmemSetByte(0x02); memoryDmemSetByte(0x00); memoryDmemSetByte(0x05);
    memoryDmemSetByte(0x00); memoryDmemSetByte(0xF4); memoryDmemSetByte(0x00); memoryDmemSetByte(0x00);
    memoryDmemSetByte(0x4E); memoryDmemSetByte(0x75); memoryDmemSetByte(0x23); memoryDmemSetByte(0xFC);
    memoryDmemSetByte(0x00); memoryDmemSetByte(0x02); memoryDmemSetByte(0x00); memoryDmemSetByte(0x06);
    memoryDmemSetByte(0x00); memoryDmemSetByte(0xF4); memoryDmemSetByte(0x00); memoryDmemSetByte(0x00);
    memoryDmemSetByte(0x4E); memoryDmemSetByte(0x75); memoryDmemSetByte(0x23); memoryDmemSetByte(0xFC);
    memoryDmemSetByte(0x00); memoryDmemSetByte(0x02); memoryDmemSetByte(0x00); memoryDmemSetByte(0x07);
    memoryDmemSetByte(0x00); memoryDmemSetByte(0xF4); memoryDmemSetByte(0x00); memoryDmemSetByte(0x00);
    memoryDmemSetByte(0x4E); memoryDmemSetByte(0x75); memoryDmemSetByte(0x48); memoryDmemSetByte(0xE7);
    memoryDmemSetByte(0x78); memoryDmemSetByte(0x00); memoryDmemSetByte(0x22); memoryDmemSetByte(0x3C);
    memoryDmemSetByte(0x00); memoryDmemSetByte(0x01); memoryDmemSetByte(0x00); memoryDmemSetByte(0x01);
    memoryDmemSetByte(0x2C); memoryDmemSetByte(0x78); memoryDmemSetByte(0x00); memoryDmemSetByte(0x04);
    memoryDmemSetByte(0x4E); memoryDmemSetByte(0xAE); memoryDmemSetByte(0xFF); memoryDmemSetByte(0x3A);
    memoryDmemSetByte(0x61); memoryDmemSetByte(0x00); memoryDmemSetByte(0xFF); memoryDmemSetByte(0x68);
    memoryDmemSetByte(0x4C); memoryDmemSetByte(0xDF); memoryDmemSetByte(0x00); memoryDmemSetByte(0x1E);
    memoryDmemSetByte(0x4E); memoryDmemSetByte(0x75); memoryDmemSetByte(0x20); memoryDmemSetByte(0x3C);
    memoryDmemSetByte(0x00); memoryDmemSetByte(0x00); memoryDmemSetByte(0x00); memoryDmemSetByte(0x20);
    memoryDmemSetByte(0x61); memoryDmemSetByte(0x00); memoryDmemSetByte(0xFF); memoryDmemSetByte(0xDC);
    memoryDmemSetByte(0x2A); memoryDmemSetByte(0x40); memoryDmemSetByte(0x1B); memoryDmemSetByte(0x7C);
    memoryDmemSetByte(0x00); memoryDmemSetByte(0x08); memoryDmemSetByte(0x00); memoryDmemSetByte(0x08);
    memoryDmemSetByte(0x41); memoryDmemSetByte(0xFA); memoryDmemSetByte(0x00); memoryDmemSetByte(0xC8);
    memoryDmemSetByte(0x2B); memoryDmemSetByte(0x48); memoryDmemSetByte(0x00); memoryDmemSetByte(0x0A);
    memoryDmemSetByte(0x41); memoryDmemSetByte(0xFA); memoryDmemSetByte(0x00); memoryDmemSetByte(0xD4);
    memoryDmemSetByte(0x2B); memoryDmemSetByte(0x48); memoryDmemSetByte(0x00); memoryDmemSetByte(0x0E);
    memoryDmemSetByte(0x49); memoryDmemSetByte(0xED); memoryDmemSetByte(0x00); memoryDmemSetByte(0x12);
    memoryDmemSetByte(0x28); memoryDmemSetByte(0x8C); memoryDmemSetByte(0x06); memoryDmemSetByte(0x94);
    memoryDmemSetByte(0x00); memoryDmemSetByte(0x00); memoryDmemSetByte(0x00); memoryDmemSetByte(0x04);
    memoryDmemSetByte(0x42); memoryDmemSetByte(0xAC); memoryDmemSetByte(0x00); memoryDmemSetByte(0x04);
    memoryDmemSetByte(0x29); memoryDmemSetByte(0x4C); memoryDmemSetByte(0x00); memoryDmemSetByte(0x08);
    memoryDmemSetByte(0x22); memoryDmemSetByte(0x4D); memoryDmemSetByte(0x4E); memoryDmemSetByte(0xAE);
    memoryDmemSetByte(0xFE); memoryDmemSetByte(0x1A); memoryDmemSetByte(0x4E); memoryDmemSetByte(0x75);
    memoryDmemSetByte(0x2C); memoryDmemSetByte(0x78); memoryDmemSetByte(0x00); memoryDmemSetByte(0x04);
    memoryDmemSetByte(0x70); memoryDmemSetByte(0x00); memoryDmemSetByte(0x43); memoryDmemSetByte(0xFA);
    memoryDmemSetByte(0x00); memoryDmemSetByte(0x96); memoryDmemSetByte(0x4E); memoryDmemSetByte(0xAE);
    memoryDmemSetByte(0xFE); memoryDmemSetByte(0x0E); memoryDmemSetByte(0x61); memoryDmemSetByte(0x00);
    memoryDmemSetByte(0xFF); memoryDmemSetByte(0x1E); memoryDmemSetByte(0x4E); memoryDmemSetByte(0x75);
    memoryDmemSetByte(0x61); memoryDmemSetByte(0x00); memoryDmemSetByte(0xFF); memoryDmemSetByte(0x48);
    memoryDmemSetByte(0x28); memoryDmemSetByte(0x00); memoryDmemSetByte(0x4A); memoryDmemSetByte(0x84);
    memoryDmemSetByte(0x67); memoryDmemSetByte(0x00); memoryDmemSetByte(0x00); memoryDmemSetByte(0x20);
    memoryDmemSetByte(0x74); memoryDmemSetByte(0x00); memoryDmemSetByte(0x61); memoryDmemSetByte(0x00);
    memoryDmemSetByte(0xFF); memoryDmemSetByte(0x46); memoryDmemSetByte(0x4A); memoryDmemSetByte(0x80);
    memoryDmemSetByte(0x67); memoryDmemSetByte(0x00); memoryDmemSetByte(0x00); memoryDmemSetByte(0x0C);
    memoryDmemSetByte(0x50); memoryDmemSetByte(0x80); memoryDmemSetByte(0x61); memoryDmemSetByte(0x00);
    memoryDmemSetByte(0xFF); memoryDmemSetByte(0x76); memoryDmemSetByte(0x61); memoryDmemSetByte(0x00);
    memoryDmemSetByte(0xFF); memoryDmemSetByte(0x42); memoryDmemSetByte(0x52); memoryDmemSetByte(0x82);
    memoryDmemSetByte(0xB8); memoryDmemSetByte(0x82); memoryDmemSetByte(0x66); memoryDmemSetByte(0x00);
    memoryDmemSetByte(0xFF); memoryDmemSetByte(0xE6); memoryDmemSetByte(0x4E); memoryDmemSetByte(0x75);
    memoryDmemSetByte(0x2F); memoryDmemSetByte(0x08); memoryDmemSetByte(0x2F); memoryDmemSetByte(0x01);
    memoryDmemSetByte(0x61); memoryDmemSetByte(0x00); memoryDmemSetByte(0xFF); memoryDmemSetByte(0xCE);
    memoryDmemSetByte(0x20); memoryDmemSetByte(0x3C); memoryDmemSetByte(0x00); memoryDmemSetByte(0x00);
    memoryDmemSetByte(0x00); memoryDmemSetByte(0xBE); memoryDmemSetByte(0x61); memoryDmemSetByte(0x00);
    memoryDmemSetByte(0xFF); memoryDmemSetByte(0x56); memoryDmemSetByte(0x22); memoryDmemSetByte(0x1F);
    memoryDmemSetByte(0x61); memoryDmemSetByte(0x00); memoryDmemSetByte(0xFF); memoryDmemSetByte(0x2C);
    memoryDmemSetByte(0x2C); memoryDmemSetByte(0x79); memoryDmemSetByte(0x00); memoryDmemSetByte(0x00);
    memoryDmemSetByte(0x00); memoryDmemSetByte(0x04); memoryDmemSetByte(0x20); memoryDmemSetByte(0x57);
    memoryDmemSetByte(0x41); memoryDmemSetByte(0xE8); memoryDmemSetByte(0x00); memoryDmemSetByte(0x12);
    memoryDmemSetByte(0x22); memoryDmemSetByte(0x40); memoryDmemSetByte(0x4E); memoryDmemSetByte(0xAE);
    memoryDmemSetByte(0xFF); memoryDmemSetByte(0x10); memoryDmemSetByte(0x20); memoryDmemSetByte(0x5F);
    memoryDmemSetByte(0x4E); memoryDmemSetByte(0x75); memoryDmemSetByte(0x20); memoryDmemSetByte(0x40);
    memoryDmemSetByte(0x61); memoryDmemSetByte(0x00); memoryDmemSetByte(0xFE); memoryDmemSetByte(0xE0);
    memoryDmemSetByte(0x26); memoryDmemSetByte(0x00); memoryDmemSetByte(0x4A); memoryDmemSetByte(0x83);
    memoryDmemSetByte(0x67); memoryDmemSetByte(0x00); memoryDmemSetByte(0x00); memoryDmemSetByte(0x10);
    memoryDmemSetByte(0x72); memoryDmemSetByte(0x00); memoryDmemSetByte(0x61); memoryDmemSetByte(0x00);
    memoryDmemSetByte(0xFF); memoryDmemSetByte(0xC0); memoryDmemSetByte(0x52); memoryDmemSetByte(0x81);
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
    memoryDmemSetByte(0x00); memoryDmemSetByte(0x00);
    /* The mkdosdev packets */

    for (const HardfileMountListEntry& mountListEntry : _mountList)
    {
      if (mountListEntry.PartitionIndex == -1)
      {
        MakeDOSDevPacketForPlainHardfile(mountListEntry, devicename);
      }
      else
      {
        MakeDOSDevPacketForRDBPartition(mountListEntry, devicename);
      }
    }
    memoryDmemSetLong(-1);  // Terminate list

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

    memset(_rom, 0, 65536);

    /* Struct DiagArea */

    _rom[0x1000] = 0x90; /* da_Config */
    _rom[0x1001] = 0;    /* da_Flags */
    _rom[0x1002] = 0;    /* da_Size */
    _rom[0x1003] = 0x96;
    _rom[0x1004] = 0;    /* da_DiagPoint */
    _rom[0x1005] = 0x80;
    _rom[0x1006] = 0;    /* da_BootPoint */
    _rom[0x1007] = 0x90;
    _rom[0x1008] = 0;    /* da_Name */
    _rom[0x1009] = 0;
    _rom[0x100a] = 0;    /* da_Reserved01 */
    _rom[0x100b] = 0;
    _rom[0x100c] = 0;    /* da_Reserved02 */
    _rom[0x100d] = 0;

    _rom[0x1080] = 0x23; /* DiagPoint */
    _rom[0x1081] = 0xfc; /* move.l #$00010001,$f40000 */
    _rom[0x1082] = 0x00;
    _rom[0x1083] = 0x01;
    _rom[0x1084] = 0x00;
    _rom[0x1085] = 0x01;
    _rom[0x1086] = 0x00;
    _rom[0x1087] = 0xf4;
    _rom[0x1088] = 0x00;
    _rom[0x1089] = 0x00;
    _rom[0x108a] = 0x4e; /* rts */
    _rom[0x108b] = 0x75;

    _rom[0x1090] = 0x4e; /* BootPoint */
    _rom[0x1091] = 0xf9; /* JMP fhfile_bootcode */
    _rom[0x1092] = static_cast<UBY>(_bootcode>>24);
    _rom[0x1093] = static_cast<UBY>(_bootcode >> 16);
    _rom[0x1094] = static_cast<UBY>(_bootcode >> 8);
    _rom[0x1095] = static_cast<UBY>(_bootcode);

    /* NULLIFY pointer to configdev */

    memoryDmemSetLongNoCounter(0, 4092);
    memoryEmemCardAdd(HardfileHandler_CardInit, HardfileHandler_CardMap);

    AddFileSystemsFromRdb();
  }
  else
  {
    memoryDmemClear();
  }
}

/*=========================*/
/* Startup hardfile device */
/*=========================*/

void HardfileHandler::Startup()
{
  /* Clear first to ensure that F is NULL */
  memset(_devices, 0, sizeof(HardfileDevice)*FHFILE_MAX_DEVICES);
  Clear();
}

/*==========================*/
/* Shutdown hardfile device */
/*==========================*/

void HardfileHandler::Shutdown()
{
  Clear();
}

/*==========================*/
/* Create hardfile          */
/*==========================*/

bool HardfileHandler::Create(HardfileDevice hfile)
{
  bool result = false;

#ifdef WIN32
  HANDLE hf;

  if(*hfile.filename && hfile.size) 
  {   
    if((hf = CreateFile(hfile.filename, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL)) != INVALID_HANDLE_VALUE)
    {
      if( SetFilePointer(hf, hfile.size, NULL, FILE_BEGIN) == hfile.size )
	result = SetEndOfFile(hf) == TRUE;
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
      result = true;
    }
    else
      fellowAddLog("fhfileCreate is unable to open output file.\n");
  }
  return result;
#endif
}

HardfileHandler::HardfileHandler()
  : _romstart(0), _bootcode(0), _configdev(0), _enabled(0)
{
}

HardfileHandler::~HardfileHandler()
{
}
