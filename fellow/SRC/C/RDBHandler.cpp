#include "DEFS.H"
#include "FELLOW.H"
#include "RDBHandler.h"

string RDBFileReader::ReadString(off_t offset, size_t maxCount)
{
  int c;
  string s;
  fseek(_F, offset, SEEK_SET);
  while (maxCount-- != 0 && (c = fgetc(_F)) != -1)
  {
    s.push_back(c);
  }
  return s;
}

UBY RDBFileReader::ReadUBY(off_t offset)
{
  fseek(_F, offset, SEEK_SET);
  return static_cast<UBY>(fgetc(_F));
}

ULO RDBFileReader::ReadULO(off_t offset)
{
  UBY value[4];
  fseek(_F, offset, SEEK_SET);
  fread(&value, 1, 4, _F);
  return static_cast<ULO>(value[0]) << 24 | static_cast<ULO>(value[1]) << 16 | static_cast<ULO>(value[2]) << 8 | static_cast<ULO>(value[3]);
}

LON RDBFileReader::ReadLON(off_t offset)
{
  return static_cast<LON>(ReadULO(offset));
}

UBY *RDBFileReader::ReadData(off_t offset, size_t byteCount)
{
  UBY *data = new UBY[byteCount];
  fread(data, 1, byteCount, _F);
  return data;
}

RDBFileReader::RDBFileReader(FILE *F)
  : _F(F)
{
}

RDBLSegBlock::RDBLSegBlock() : 
  SizeInLongs(0),
  CheckSum(0),
  HostID(0),
  Next(-1),
  Data(nullptr)
{
}

RDBLSegBlock::~RDBLSegBlock()
{
  if (Data != nullptr)
  {
    delete Data;
  }
}

LON RDBLSegBlock::GetDataSize()
{
  return 4 * (SizeInLongs - 5);
}

void RDBLSegBlock::ReadFromFile(RDBFileReader& reader, ULO index)
{
  ID = reader.ReadString(index, 4);
  SizeInLongs = reader.ReadLON(index + 4);
  CheckSum = reader.ReadLON(index + 8);
  HostID = reader.ReadLON(index + 12);
  Next = reader.ReadLON(index + 16);
  Data = reader.ReadData(index + 20, GetDataSize());
}

void RDBLSegBlock::Log()
{
  fellowAddLog("LSegBlock\n");
  fellowAddLog("-----------------------------------------\n");
  fellowAddLog("0   - id:                     %.4s\n", ID.c_str());
  fellowAddLog("4   - size in longs:          %d\n", SizeInLongs);
  fellowAddLog("8   - checksum:               %.8X\n", CheckSum);
  fellowAddLog("12  - host id:                %d\n", HostID);
  fellowAddLog("16  - next:                   %d\n\n", Next);
}

RDBFileSystemHandler::RDBFileSystemHandler()
  : Size(0),
    RawData(nullptr)
{
}

RDBFileSystemHandler::~RDBFileSystemHandler()
{
  if (RawData != nullptr)
  {
    delete RawData;
  }
}

bool RDBFileSystemHandler::ReadFromFile(RDBFileReader& reader, ULO blockChainStart, ULO blockSize)
{
  vector<RDBLSegBlock*> blocks;
  LON nextBlock = blockChainStart;

  fellowAddLog("Reading filesystem handler from block-chain at %d\n", blockChainStart);

  Size = 0;
  while (nextBlock != -1)
  {
    LON index = nextBlock*blockSize;
    RDBLSegBlock *block = new RDBLSegBlock();
    block->ReadFromFile(reader, index);
    block->Log();
    blocks.push_back(block);
    Size += block->GetDataSize();
    nextBlock = block->Next;
  }

  fellowAddLog("%d LSegBlocks read\n", blocks.size());
  fellowAddLog("Total filesystem size was %d bytes\n", Size);

  RawData = new UBY[Size];
  ULO nextCopyPosition = 0;
  for (auto block : blocks)
  {
    LON size = block->GetDataSize();
    memcpy(RawData + nextCopyPosition, block->Data, size);
    nextCopyPosition += size;
  }

  blocks.clear();

  HunkParser hunkParser(RawData);
  return hunkParser.Parse(Hunks);
}

void RDBFileSystemHeader::ReadFromFile(RDBFileReader& reader, ULO blockChainStart, ULO blockSize)
{
  ULO index = blockSize*blockChainStart;

  SizeInLongs = reader.ReadULO(index + 4);
  CheckSum = reader.ReadLON(index + 8);
  HostID = reader.ReadULO(index + 12);
  Next = reader.ReadULO(index + 16);
  Flags = reader.ReadULO(index + 20);
  DOSType = reader.ReadULO(index + 32);
  Version = reader.ReadULO(index + 36);
  PatchFlags = reader.ReadULO(index + 40);

  // Device node
  DnType = reader.ReadULO(index + 44);
  DnTask = reader.ReadULO(index + 48);
  DnLock = reader.ReadULO(index + 52);
  DnHandler = reader.ReadULO(index + 56);
  DnStackSize = reader.ReadULO(index + 60);
  DnPriority = reader.ReadULO(index + 64);
  DnStartup = reader.ReadULO(index + 68);
  DnSegListBlock = reader.ReadULO(index + 72);
  DnGlobalVec = reader.ReadULO(index + 76);

  // Reserved for additional patchflags
  for (int i = 0; i < 23; i++)
  {
    Reserved2[i] = reader.ReadULO(index + i + 80);
  }

  FileSystemHandler.ReadFromFile(reader, DnSegListBlock, blockSize);
}

void RDBFileSystemHeader::Log()
{
  fellowAddLog("Filesystem header block\n");
  fellowAddLog("-----------------------------------------\n");
  fellowAddLog("0  - id:                     FSHD\n");
  fellowAddLog("4  - size in longs:          %u\n", SizeInLongs);
  fellowAddLog("8  - checksum:               %d\n", CheckSum);
  fellowAddLog("12 - host id:                %u\n", HostID);
  fellowAddLog("16 - next:                   %d\n", Next);
  fellowAddLog("20 - flags:                  %X\n", Flags);
  fellowAddLog("32 - dos type:               %.8X\n", DOSType);
  fellowAddLog("36 - version:                %.8X ie %d.%d\n", Version, (Version & 0xffff0000) >> 16, Version & 0xffff);
  fellowAddLog("40 - patch flags:            %.8X\n", PatchFlags);
  fellowAddLog("Device node:-----------------------------\n");
  fellowAddLog("44 - type:                   %u\n", DnType);
  fellowAddLog("48 - task:                   %u\n", DnTask);
  fellowAddLog("48 - task:                   %u\n", DnTask);
  fellowAddLog("52 - lock:                   %u\n", DnLock);
  fellowAddLog("56 - handler:                %u\n", DnHandler);
  fellowAddLog("60 - stack size:             %u\n", DnStackSize);
  fellowAddLog("64 - priority:               %u\n", DnPriority);
  fellowAddLog("68 - startup:                %u\n", DnStartup);
  fellowAddLog("72 - seg list block:         %u\n", DnSegListBlock);
  fellowAddLog("76 - global vec:             %d\n\n", DnGlobalVec);
}

bool RDBFileSystemHeader::IsOlderOrSameFileSystemVersion(ULO dosType, ULO version)
{
  return DOSType == dosType && Version <= version;
}

RDBFileSystemHeader::RDBFileSystemHeader() :
  SizeInLongs(0),
  CheckSum(0),
  HostID(0),
  Next(0),
  Flags(0),
  DOSType(0),
  Version(0),
  PatchFlags(0),
  DnType(0),
  DnTask(0),
  DnLock(0),
  DnHandler(0),
  DnStackSize(0),
  DnPriority(0),
  DnStartup(0),
  DnSegListBlock(0),
  DnGlobalVec(0)
{
}

RDBFileSystemHeader::~RDBFileSystemHeader()
{  
}

void RDBPartition::ReadFromFile(RDBFileReader& reader, ULO blockChainStart, ULO blockSize)
{  
  ULO index = blockSize*blockChainStart;

  ID = reader.ReadString(index, 4);
  SizeInLongs = reader.ReadULO(index + 4);
  CheckSum = reader.ReadLON(index + 8);
  HostID = reader.ReadULO(index + 12);
  Next = reader.ReadULO(index + 16);
  Flags = reader.ReadULO(index + 20);
  DevFlags = reader.ReadULO(index + 32);
  DriveNameLength = reader.ReadUBY(index + 36);
  DriveName = reader.ReadString(index + 37, DriveNameLength);

  // DOS Environment vector
  SizeOfVector = reader.ReadULO(index + 128);
  SizeBlock = reader.ReadULO(index + 132);
  SecOrg = reader.ReadULO(index + 136);
  Surfaces = reader.ReadULO(index + 140);
  SectorsPerBlock = reader.ReadULO(index + 144);
  BlocksPerTrack = reader.ReadULO(index + 148);
  Reserved = reader.ReadULO(index + 152);
  PreAlloc = reader.ReadULO(index + 156);
  Interleave = reader.ReadULO(index + 160);
  LowCylinder = reader.ReadULO(index + 164);
  HighCylinder = reader.ReadULO(index + 168);
  NumBuffer = reader.ReadULO(index + 172);
  BufMemType = reader.ReadULO(index + 176);
  MaxTransfer = reader.ReadULO(index + 180);
  Mask = reader.ReadULO(index + 184);
  BootPri = reader.ReadULO(index + 188);
  DOSType = reader.ReadULO(index + 192);
  Baud = reader.ReadULO(index + 196);
  Control = reader.ReadULO(index + 200);
  Bootblocks = reader.ReadULO(index + 204);
}

void RDBPartition::Log()
{
  fellowAddLog("RDB Partition\n");
  fellowAddLog("----------------------------------------------------\n");
  fellowAddLog("0   - id:                       %s (Should be PART)\n", ID.c_str());
  fellowAddLog("4   - size in longs:            %u (Should be 64)\n", SizeInLongs);
  fellowAddLog("8   - checksum:                 %.8X\n", CheckSum);
  fellowAddLog("12  - host id:                  %u\n", HostID);
  fellowAddLog("16  - next block:               %d\n", Next);
  fellowAddLog("20  - flags:                    %X (%s, %s)\n", Flags, Flags & 1 ? "Bootable" : "Not bootable", Flags & 2 ? "No automount" : "Automount");
  fellowAddLog("32  - DevFlags:                 %X\n", DevFlags);
  fellowAddLog("36  - DriveNameLength:          %d\n", DriveNameLength);
  fellowAddLog("37  - DriveName:                %s\n", DriveName.c_str());
  fellowAddLog("Partition DOS Environment vector:-------------------\n");
  fellowAddLog("128 - size of vector (in longs):%u (=%d bytes)\n", SizeOfVector, SizeOfVector*4);
  fellowAddLog("132 - SizeBlock (in longs):     %u (=%d bytes)\n", SizeBlock, SizeBlock*4);
  fellowAddLog("136 - SecOrg:                   %u (Should be 0)\n", SecOrg);
  fellowAddLog("140 - Surfaces:                 %u\n", Surfaces);
  fellowAddLog("144 - Sectors per block:        %u\n", SectorsPerBlock);
  fellowAddLog("148 - Blocks per track:         %u\n", BlocksPerTrack);
  fellowAddLog("152 - Reserved (blocks):        %u\n", Reserved);
  fellowAddLog("156 - Pre Alloc:                %u\n", PreAlloc);
  fellowAddLog("160 - Interleave:               %u\n", Interleave);
  fellowAddLog("164 - low cylinder:             %u\n", LowCylinder);
  fellowAddLog("168 - high cylinder:            %u\n", HighCylinder);
  fellowAddLog("172 - num buffer:               %u\n", NumBuffer);
  fellowAddLog("176 - BufMemType:               %u\n", BufMemType);
  fellowAddLog("180 - MaxTransfer:              %u\n", MaxTransfer);
  fellowAddLog("184 - Mask:                     %X\n", Mask);
  fellowAddLog("188 - BootPri:                  %u\n", BootPri);
  fellowAddLog("192 - DosType:                  %u\n", DOSType);
  fellowAddLog("196 - Baud:                     %u\n", Baud);
  fellowAddLog("200 - Control:                  %u\n", Control);
  fellowAddLog("204 - Bootblocks:               %u\n", Bootblocks);
}

bool RDBHandler::HasRigidDiskBlock(RDBFileReader& reader)
{
  string headerID = reader.ReadString(0, 4);
  return headerID == "RDSK";
}

void RDBHeader::ReadFromFile(RDBFileReader& reader)
{
  ID = reader.ReadString(0, 4);
  SizeInLongs = reader.ReadULO(4);
  CheckSum = reader.ReadLON(8);
  HostID = reader.ReadULO(12);
  BlockSize = reader.ReadULO(16);
  Flags = reader.ReadULO(20);
  BadBlockList = reader.ReadULO(24);
  PartitionList = reader.ReadULO(28);
  FilesystemHeaderList = reader.ReadULO(32);
  DriveInitCode = reader.ReadULO(36);

  // Physical drive characteristics
  Cylinders = reader.ReadULO(64);
  SectorsPerTrack = reader.ReadULO(68);
  Heads = reader.ReadULO(72);
  Interleave = reader.ReadULO(76);
  ParkingZone = reader.ReadULO(80);
  WritePreComp = reader.ReadULO(96);
  ReducedWrite = reader.ReadULO(100);
  StepRate = reader.ReadULO(104);

  // Logical drive characteristics
  RDBBlockLow = reader.ReadULO(128);
  RDBBlockHigh = reader.ReadULO(132);
  LowCylinder = reader.ReadULO(136);
  HighCylinder = reader.ReadULO(140);
  CylinderBlocks = reader.ReadULO(144);
  AutoParkSeconds = reader.ReadULO(148);
  HighRDSKBlock = reader.ReadULO(152);

  DiskVendor = reader.ReadString(160, 8);
  DiskProduct = reader.ReadString(168, 16);
  DiskRevision = reader.ReadString(184, 4);
  ControllerVendor = reader.ReadString(188, 8);
  ControllerProduct = reader.ReadString(196, 16);
  ControllerRevision = reader.ReadString(212, 4);

  ULO nextPartition = PartitionList;
  while (nextPartition != -1)
  {
    RDBPartition *partition = new RDBPartition();
    partition->ReadFromFile(reader, nextPartition, BlockSize);
    partition->Log();
    Partitions.push_back(partition);
    nextPartition = partition->Next;
  }

  ULO nextFilesystemHeader = FilesystemHeaderList;
  while (nextFilesystemHeader != -1)
  {
    RDBFileSystemHeader *fileSystemHeader = new RDBFileSystemHeader();
    fileSystemHeader->ReadFromFile(reader, nextFilesystemHeader, BlockSize);
    fileSystemHeader->Log();
    FileSystemHeaders.push_back(fileSystemHeader);
    nextFilesystemHeader = fileSystemHeader->Next;
  }
}

void RDBHeader::Log()
{
  fellowAddLog("RDB Hardfile\n");
  fellowAddLog("-----------------------------------------\n");
  fellowAddLog("0   - id:                     %s\n", ID.c_str());
  fellowAddLog("4   - size in longs:          %u\n", SizeInLongs);
  fellowAddLog("8   - checksum:               %.8X\n", CheckSum);
  fellowAddLog("12  - host id:                %u\n", HostID);
  fellowAddLog("16  - block size:             %u\n", BlockSize);
  fellowAddLog("20  - flags:                  %X\n", Flags);
  fellowAddLog("24  - bad block list:         %d\n", BadBlockList);
  fellowAddLog("28  - partition list:         %d\n", PartitionList);
  fellowAddLog("32  - filesystem header list: %d\n", FilesystemHeaderList);
  fellowAddLog("36  - drive init code:        %X\n", DriveInitCode);
  fellowAddLog("Physical drive characteristics:---------\n");
  fellowAddLog("64  - cylinders:              %u\n", Cylinders);
  fellowAddLog("68  - sectors per track:      %u\n", SectorsPerTrack);
  fellowAddLog("72  - heads:                  %u\n", Heads);
  fellowAddLog("76  - interleave:             %u\n", Interleave);
  fellowAddLog("80  - parking zone:           %u\n", ParkingZone);
  fellowAddLog("96  - write pre-compensation: %u\n", WritePreComp);
  fellowAddLog("100 - reduced write:          %u\n", ReducedWrite);
  fellowAddLog("104 - step rate:              %u\n", StepRate);
  fellowAddLog("Logical drive characteristics:----------\n");
  fellowAddLog("128 - RDB block low:          %u\n", RDBBlockLow);
  fellowAddLog("132 - RDB block high:         %u\n", RDBBlockHigh);
  fellowAddLog("136 - low cylinder:           %u\n", LowCylinder);
  fellowAddLog("140 - high cylinder:          %u\n", HighCylinder);
  fellowAddLog("144 - cylinder blocks:        %u\n", CylinderBlocks);
  fellowAddLog("148 - auto park seconds:      %u\n", AutoParkSeconds);
  fellowAddLog("152 - high RDSK block:        %u\n", HighRDSKBlock);
  fellowAddLog("Drive identification:-------------------\n");
  fellowAddLog("160 - disk vendor:            %.8s\n", DiskVendor.c_str());
  fellowAddLog("168 - disk product:           %.16s\n", DiskProduct.c_str());
  fellowAddLog("184 - disk revision:          %.4s\n", DiskRevision.c_str());
  fellowAddLog("188 - controller vendor:      %.8s\n", ControllerVendor.c_str());
  fellowAddLog("196 - controller product:     %.16s\n", ControllerProduct.c_str());
  fellowAddLog("212 - controller revision:    %.4s\n", ControllerRevision.c_str());
  fellowAddLog("-----------------------------------------\n\n");
}

RDBHeader* RDBHandler::GetDriveInformation(RDBFileReader& reader)
{
  RDBHeader* rdb = new RDBHeader();
  rdb->ReadFromFile(reader);
  rdb->Log();
  return rdb;
}
