#ifndef RDBHANDLER_H
#define RDBHANDLER_H

#include "DEFS.H"
#include "Hunks.h"
#include <string>
#include <vector>

using namespace std;

class RDBFileReader
{
private:
  FILE *_F;

public:
  string ReadString(off_t offset, size_t maxCount);
  UBY ReadUBY(off_t offset);
  ULO ReadULO(off_t offset);
  LON ReadLON(off_t offset);
  UBY *ReadData(off_t offset, size_t byteCount);

  RDBFileReader(FILE *F);
};

struct RDBLSegBlock
{
  string ID;
  LON SizeInLongs;
  LON CheckSum;
  LON HostID;
  LON Next;
  UBY *Data;

  RDBLSegBlock();
  ~RDBLSegBlock();

  LON GetDataSize();
  void ReadFromFile(RDBFileReader& reader, ULO index);
  void Log();
};

struct RDBFileSystemHandler
{
  ULO Size;
  UBY *RawData;
  vector<HunkPtr> Hunks;

  RDBFileSystemHandler();
  ~RDBFileSystemHandler();

  bool ReadFromFile(RDBFileReader& reader, ULO blockChainStart, ULO blockSize);
};

struct RDBFileSystemHeader
{
  ULO SizeInLongs;
  LON CheckSum;
  ULO HostID;
  ULO Next;
  ULO Flags;
  ULO DOSType;
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

  RDBFileSystemHandler FileSystemHandler;

  void ReadFromFile(RDBFileReader& reader, ULO blockChainStart, ULO blockSize);
  void Log();

  RDBFileSystemHeader();
  ~RDBFileSystemHeader();
};

struct RDBPartition
{
  string ID;
  ULO SizeInLongs;
  ULO CheckSum;
  ULO HostID;
  ULO Next;
  ULO Flags;
  ULO BadBlockList;
  ULO DevFlags;
  char DriveNameLength;
  string DriveName;

  // DOS Environment Vector
  ULO SizeOfVector;
  ULO SizeBlock;
  ULO SecOrg;
  ULO Surfaces;
  ULO SectorsPerBlock;
  ULO BlocksPerTrack;
  ULO Reserved;
  ULO PreAlloc;
  ULO Interleave;

  ULO LowCylinder;
  ULO HighCylinder;
  ULO NumBuffer;
  ULO BufMemType;
  ULO MaxTransfer;
  ULO Mask;
  ULO BootPri;
  ULO DOSType;
  ULO Baud;
  ULO Control;
  ULO Bootblocks;

  bool IsAutomountable();
  bool IsBootable();

  void ReadFromFile(RDBFileReader& reader, ULO blockChainStart, ULO blockSize);
  void Log();
};

struct RDBHeader
{
  string ID;
  ULO SizeInLongs;
  LON CheckSum;
  ULO HostID;
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

  ULO RDBBlockLow;
  ULO RDBBlockHigh;
  ULO LowCylinder;
  ULO HighCylinder;
  ULO CylinderBlocks;
  ULO AutoParkSeconds;
  ULO HighRDSKBlock;

  string DiskVendor;
  string DiskProduct;
  string DiskRevision;
  string ControllerVendor;
  string ControllerProduct;
  string ControllerRevision;

  vector<RDBPartition *> Partitions;
  vector<RDBFileSystemHeader*> FileSystemHeaders;

  void ReadFromFile(RDBFileReader& reader);
  void Log();
};

class RDBHandler
{
public:
  static bool HasRigidDiskBlock(RDBFileReader& reader);
  static RDBHeader* GetDriveInformation(RDBFileReader& reader);
};

#endif
