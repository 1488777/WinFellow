#include "DEFS.H"
#include "FELLOW.H"
#include "RDBHandler.h"

ULO GetByteswappedLongAt(UBY *base, ULO address)
{
  ULO value = (base[address] << 24) | (base[address + 1] << 16) | (base[address + 2] << 8) | base[address + 3];
  return value;
}

void ParseHunks(RDBFilesystemHandler *fs)
{
  ULO address = 0;
  ULO hunk_type = GetByteswappedLongAt(fs->Data, address);
  address += 4;

  fellowAddLog("Hunk type: %X\n", hunk_type);

  ULO stringLength = GetByteswappedLongAt(fs->Data, address);
  address += 4;

  while (stringLength != 0)
  {
    fellowAddLog("String of length %u longs\n", stringLength);
    address += 4 * stringLength;

    stringLength = GetByteswappedLongAt(fs->Data, address);
    address += 4;
  }

  ULO tableSize = GetByteswappedLongAt(fs->Data, address);
  address += 4;

  fellowAddLog("Table size: %u\n", tableSize);

  ULO firstHunk = GetByteswappedLongAt(fs->Data, address);
  address += 4;

  fellowAddLog("First hunk: %u\n", firstHunk);

  ULO lastHunk = GetByteswappedLongAt(fs->Data, address);
  address += 4;

  fellowAddLog("Last hunk: %u\n", lastHunk);

  for (ULO i = firstHunk; i <= lastHunk; i++)
  {
    ULO hunkSize = GetByteswappedLongAt(fs->Data, address);
    address += 4;

    ULO hunkFlags = hunkSize >> 30;
    if (hunkFlags == 3)
    {
      ULO additionalFlag = GetByteswappedLongAt(fs->Data, address);
      address += 4;
    }
    fellowAddLog("Hunk %u size: %u %s\n", i, hunkSize & 0x3fffffff, (hunkFlags == 0) ? "Any memory" : ((hunkFlags == 1) ? "Chip memory" : ((hunkFlags == 2) ? "Fast memory" : "Additional memory flags")));
  }
}

RDBLSegBlock::RDBLSegBlock()
  : SizeInLongs(0),
  CheckSum(0),
  HostId(0),
  Next(-1),
  Data(nullptr)
{
  Id[0] = Id[1] = Id[2] = Id[3] = 0;
}

RDBLSegBlock::~RDBLSegBlock()
{
  if (Data != nullptr)
  {
    delete[] Data;
  }
}

LON RDBLSegBlock::GetDataSize()
{
  return 4 * (SizeInLongs - 5);
}

void RDBLSegBlock::ReadFromFile(FILE *F, ULO index)
{
  RDBHandler::ReadCharsFromFile(F, index + 0, Id, 4);
  SizeInLongs = RDBHandler::ReadLONFromFile(F, index + 4);
  CheckSum = RDBHandler::ReadLONFromFile(F, index + 8);
  HostId = RDBHandler::ReadLONFromFile(F, index + 12);
  Next = RDBHandler::ReadLONFromFile(F, index + 16);
  LON dataSize = GetDataSize();
  Data = new UBY[dataSize];
  fread(Data, 1, dataSize, F);
}

void RDBLSegBlock::Log()
{
  fellowAddLog("LSegBlock\n");
  fellowAddLog("-----------------------------------------\n");
  fellowAddLog("0   - id:                     %c%c%c%c\n", Id[0], Id[1], Id[2], Id[3]);
  fellowAddLog("4   - size in longs:          %d\n", SizeInLongs);
  fellowAddLog("8   - checksum:               %d\n", CheckSum);
  fellowAddLog("12  - host id:                %d\n", HostId);
  fellowAddLog("16  - next:                   %d\n\n", Next);
}

RDBFilesystemHandler::RDBFilesystemHandler()
  : Size(0),
  Data(nullptr)
{
}

RDBFilesystemHandler::~RDBFilesystemHandler()
{
  if (Data != nullptr)
  {
    delete[] Data;
  }
}


#include "fileops.h"
void RDBFilesystemHandler::ReadFromFile(FILE *F, ULO blockChainStart, ULO blockSize)
{
  std::list<RDBLSegBlock*> blocks;
  LON nextBlock = blockChainStart;

  fellowAddLog("Reading filesystem handler from block-chain at %d\n", blockChainStart);

  Size = 0;
  while (nextBlock != -1)
  {
    LON index = nextBlock*blockSize;
    RDBLSegBlock *block = new RDBLSegBlock();
    block->ReadFromFile(F, index);
    block->Log();
    blocks.push_back(block);
    Size += block->GetDataSize();
    nextBlock = block->Next;
  }

  fellowAddLog("%d LSegBlocks read\n", blocks.size());
  fellowAddLog("Total filesystem size was %d bytes\n", Size);

  Data = new UBY[Size];
  ULO nextCopyPosition = 0;
  for (auto block : blocks)
  {
    LON size = block->GetDataSize();
    memcpy(Data + nextCopyPosition, block->Data, size);
    nextCopyPosition += size;
  }

  blocks.clear();


  STR fname[256];
  fileopsGetGenericFileName(fname, "WinFellow", "filesystem.bin");
  FILE *W = fopen(fname, "wb");
  fwrite(Data, 1, Size, W);
  fclose(W);

}

void RDBFilesystemHeader::ReadFromFile(FILE *F, ULO blockChainStart, ULO blockSize)
{
  ULO index = blockSize*blockChainStart;

  SizeInLongs = RDBHandler::ReadULOFromFile(F, index + 4);
  CheckSum = RDBHandler::ReadLONFromFile(F, index + 8);
  HostId = RDBHandler::ReadULOFromFile(F, index + 12);
  Next = RDBHandler::ReadULOFromFile(F, index + 16);
  Flags = RDBHandler::ReadULOFromFile(F, index + 20);
  RDBHandler::ReadCharsFromFile(F, index + 32, DosType, 4);
  Version = RDBHandler::ReadULOFromFile(F, index + 36);
  PatchFlags = RDBHandler::ReadULOFromFile(F, index + 40);

  // Device node
  DnType = RDBHandler::ReadULOFromFile(F, index + 44);
  DnTask = RDBHandler::ReadULOFromFile(F, index + 48);
  DnLock = RDBHandler::ReadULOFromFile(F, index + 52);
  DnHandler = RDBHandler::ReadULOFromFile(F, index + 56);
  DnStackSize = RDBHandler::ReadULOFromFile(F, index + 60);
  DnPriority = RDBHandler::ReadULOFromFile(F, index + 64);
  DnStartup = RDBHandler::ReadULOFromFile(F, index + 68);
  DnSegListBlock = RDBHandler::ReadULOFromFile(F, index + 72);
  DnGlobalVec = RDBHandler::ReadULOFromFile(F, index + 76);

  FilesystemHandler.ReadFromFile(F, DnSegListBlock, blockSize);
}

void RDBFilesystemHeader::Log()
{
  fellowAddLog("Filesystem header block\n");
  fellowAddLog("-----------------------------------------\n");
  fellowAddLog("0   - id:                     FSHD\n");
  fellowAddLog("4   - size in longs:          %u\n", SizeInLongs);
  fellowAddLog("8   - checksum:               %d\n", CheckSum);
  fellowAddLog("12  - host id:                %u\n", HostId);
  fellowAddLog("16  - next:                   %d\n", Next);
  fellowAddLog("20  - flags:                  %X\n", Flags);
  fellowAddLog("32  - dos type:               %.4s\n", DosType);
  fellowAddLog("36  - version:              0x%X ie %d.%d\n", Version, (Version & 0xffff0000) >> 16, Version & 0xffff);
  fellowAddLog("40  - patch flags:          0x%X\n", PatchFlags);
  fellowAddLog("Device node:-----------------------------\n");
  fellowAddLog("44  - type:                   %u\n", DnType);
  fellowAddLog("48  - task:                   %u\n", DnTask);
  fellowAddLog("52  - lock:                   %u\n", DnLock);
  fellowAddLog("56  - handler:                %u\n", DnHandler);
  fellowAddLog("60  - stack size:             %u\n", DnStackSize);
  fellowAddLog("64  - priority:               %u\n", DnPriority);
  fellowAddLog("68  - startup:                %u\n", DnStartup);
  fellowAddLog("72  - seg list block:         %u\n", DnSegListBlock);
  fellowAddLog("76  - global vec:             %u\n\n", DnGlobalVec);
}

void RDBHandler::ReadCharsFromFile(FILE *F, off_t offset, STR* destination, size_t count)
{
  fseek(F, offset, SEEK_SET);
  fread(destination, 1, count, F);
}

ULO RDBHandler::ReadULOFromFile(FILE *F, off_t offset)
{
  UBY value[4];
  fseek(F, offset, SEEK_SET);
  fread(&value, 1, 4, F);
  return static_cast<ULO>(value[0]) << 24 | static_cast<ULO>(value[1]) << 16 | static_cast<ULO>(value[2]) << 8 | static_cast<ULO>(value[3]);
}

LON RDBHandler::ReadLONFromFile(FILE *F, off_t offset)
{
  return static_cast<LON>(ReadULOFromFile(F, offset));
}

bool RDBHandler::HasRigidDiskBlock(FILE *F)
{
  STR header[5];
  ReadCharsFromFile(F, 0, header, 4);
  header[4] = '\0';
  return strcmp(header, "RDSK") == 0;
}

void RDBHeader::ReadFromFile(FILE *F)
{
  RDBHandler::ReadCharsFromFile(F, 0, Id, 4);
  SizeInLongs = RDBHandler::ReadULOFromFile(F, 4);
  CheckSum = RDBHandler::ReadLONFromFile(F, 8);
  HostId = RDBHandler::ReadULOFromFile(F, 12);
  BlockSize = RDBHandler::ReadULOFromFile(F, 16);
  Flags = RDBHandler::ReadULOFromFile(F, 20);
  BadBlockList = RDBHandler::ReadULOFromFile(F, 24);
  PartitionList = RDBHandler::ReadULOFromFile(F, 28);
  FilesystemHeaderList = RDBHandler::ReadULOFromFile(F, 32);
  DriveInitCode = RDBHandler::ReadULOFromFile(F, 36);

  // Physical drive characteristics
  Cylinders = RDBHandler::ReadULOFromFile(F, 64);
  SectorsPerTrack = RDBHandler::ReadULOFromFile(F, 68);
  Heads = RDBHandler::ReadULOFromFile(F, 72);
  Interleave = RDBHandler::ReadULOFromFile(F, 76);
  ParkingZone = RDBHandler::ReadULOFromFile(F, 80);
  WritePreComp = RDBHandler::ReadULOFromFile(F, 96);
  ReducedWrite = RDBHandler::ReadULOFromFile(F, 100);
  StepRate = RDBHandler::ReadULOFromFile(F, 104);

  // Logical drive characteristics
  RdbBlockLow = RDBHandler::ReadULOFromFile(F, 128);
  RdbBlockHigh = RDBHandler::ReadULOFromFile(F, 132);
  LowCylinder = RDBHandler::ReadULOFromFile(F, 136);
  HighCylinder = RDBHandler::ReadULOFromFile(F, 140);
  CylinderBlocks = RDBHandler::ReadULOFromFile(F, 144);
  AutoParkSeconds = RDBHandler::ReadULOFromFile(F, 148);
  HighRDSKBlock = RDBHandler::ReadULOFromFile(F, 152);

  RDBHandler::ReadCharsFromFile(F, 160, DiskVendor, 8);
  RDBHandler::ReadCharsFromFile(F, 168, DiskProduct, 16);
  RDBHandler::ReadCharsFromFile(F, 184, DiskRevision, 4);
  RDBHandler::ReadCharsFromFile(F, 188, ControllerVendor, 8);
  RDBHandler::ReadCharsFromFile(F, 196, ControllerProduct, 16);
  RDBHandler::ReadCharsFromFile(F, 212, ControllerRevision, 4);

  ULO nextFilesystemHeader = FilesystemHeaderList;
  while (nextFilesystemHeader != -1)
  {
    RDBFilesystemHeader *filesystemHeader = new RDBFilesystemHeader();
    filesystemHeader->ReadFromFile(F, nextFilesystemHeader, BlockSize);
    filesystemHeader->Log();
    FilesystemHeaders.push_back(filesystemHeader);
    nextFilesystemHeader = filesystemHeader->Next;
  }
}

void RDBHeader::Log()
{
  fellowAddLog("RDB Hardfile\n");
  fellowAddLog("-----------------------------------------\n");
  fellowAddLog("0   - id:                     %.4s\n", Id);
  fellowAddLog("4   - size in longs:          %u\n", SizeInLongs);
  fellowAddLog("8   - checksum:               %d\n", CheckSum);
  fellowAddLog("12  - host id:                %u\n", HostId);
  fellowAddLog("16  - block size:             %u\n", BlockSize);
  fellowAddLog("20  - flags:                  %X\n", Flags);
  fellowAddLog("24  - bad block list:         %X\n", BadBlockList);
  fellowAddLog("28  - partition list:         %X\n", PartitionList);
  fellowAddLog("32  - filesystem header list: %X\n", FilesystemHeaderList);
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
  fellowAddLog("128 - RDB block low:          %u\n", RdbBlockLow);
  fellowAddLog("132 - RDB block high:         %u\n", RdbBlockHigh);
  fellowAddLog("136 - low cylinder:           %u\n", LowCylinder);
  fellowAddLog("140 - high cylinder:          %u\n", HighCylinder);
  fellowAddLog("144 - cylinder blocks:        %u\n", CylinderBlocks);
  fellowAddLog("148 - auto park seconds:      %u\n", AutoParkSeconds);
  fellowAddLog("152 - high RDSK block:        %u\n", HighRDSKBlock);
  fellowAddLog("Drive identification:-------------------\n");
  fellowAddLog("160 - disk vendor:            %.8s\n", DiskVendor);
  fellowAddLog("168 - disk product:           %.16s\n", DiskProduct);
  fellowAddLog("184 - disk revision:          %.4s\n", DiskRevision);
  fellowAddLog("188 - controller vendor:      %.8s\n", ControllerVendor);
  fellowAddLog("196 - controller product:     %.16s\n", ControllerProduct);
  fellowAddLog("212 - controller revision:    %.4s\n", ControllerRevision);
  fellowAddLog("-----------------------------------------\n\n");
}

RDBHeader* RDBHandler::GetDriveInformation(FILE *F)
{
  RDBHeader* rdb = new RDBHeader();
  rdb->ReadFromFile(F);
  rdb->Log();

  ParseHunks(&rdb->FilesystemHeaders.front()->FilesystemHandler);

  return rdb;
}


