#ifndef RDBHANDLER_H
#define RDBHANDLER_H

#include "DEFS.H"
#include "Hunks.h"
#include <vector>

struct RDBLSegBlock
{
  STR Id[4];
  LON SizeInLongs;
  LON CheckSum;
  LON HostId;
  LON Next;
  UBY *Data;

  RDBLSegBlock();
  ~RDBLSegBlock();

  LON GetDataSize();
  void ReadFromFile(FILE *F, ULO index);
  void Log();
};

struct RDBFilesystemHandler
{
  ULO Size;
  UBY *RawData;
  std::vector<HunkPtr> Hunks;

  RDBFilesystemHandler();
  ~RDBFilesystemHandler();

  void ReadFromFile(FILE *F, ULO blockChainStart, ULO blockSize);
};

struct RDBFilesystemHeader
{
  ULO SizeInLongs;
  LON CheckSum;
  ULO HostId;
  ULO Next;
  ULO Flags;
  STR DosType[4];
  ULO Version;
  ULO PatchFlags;

  // Device node
  ULO DnType;
  ULO DnTask;
  ULO DnLock;
  ULO DnHandler;
  ULO DnStackSize;
  ULO DnPriority;
  ULO DnStartup;
  ULO DnSegListBlock;
  ULO DnGlobalVec;
  ULO Reserved2[23];

  RDBFilesystemHandler FilesystemHandler;

  RDBFilesystemHeader();
  ~RDBFilesystemHeader();

  void ReadFromFile(FILE *F, ULO blockChainStart, ULO blockSize);
  void Log();
};

struct RDBHeader
{
  STR Id[4];
  ULO SizeInLongs;
  LON CheckSum;
  ULO HostId;
  ULO BlockSize;
  ULO Flags;
  ULO BadBlockList;
  ULO PartitionList;
  ULO FilesystemHeaderList;
  ULO DriveInitCode;

  ULO Cylinders;
  ULO SectorsPerTrack;
  ULO Heads;
  ULO Interleave;
  ULO ParkingZone;
  ULO WritePreComp;
  ULO ReducedWrite;
  ULO StepRate;

  ULO RdbBlockLow;
  ULO RdbBlockHigh;
  ULO LowCylinder;
  ULO HighCylinder;
  ULO CylinderBlocks;
  ULO AutoParkSeconds;
  ULO HighRDSKBlock;

  STR DiskVendor[8];
  STR DiskProduct[16];
  STR DiskRevision[4];
  STR ControllerVendor[8];
  STR ControllerProduct[16];
  STR ControllerRevision[4];

  std::vector<RDBFilesystemHeader*> FilesystemHeaders;

  void ReadFromFile(FILE *F);
  void Log();
};

class RDBHandler
{

public:
  static void ReadCharsFromFile(FILE *F, off_t offset, STR* destination, size_t count);
  static ULO ReadULOFromFile(FILE *F, off_t offset);
  static LON ReadLONFromFile(FILE *F, off_t offset);

  static bool HasRigidDiskBlock(FILE *F);
  static RDBHeader* GetDriveInformation(FILE *F);
};

#endif
