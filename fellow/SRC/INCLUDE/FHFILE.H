#ifndef HARDFILEHANDLER_H
#define HARDFILEHANDLER_H

#include <string>
#include <vector>

#include "RDBHandler.h"

#define FHFILE_MAX_DEVICES 20

typedef enum
{
  FHFILE_NONE = 0,
  FHFILE_HDF = 1
} fhfile_status;

struct HardfileDevice
{
  STR filename[256]; /* Config settings from here and down */
  BOOLE readonly_original;
  ULO bytespersector_original;
  ULO sectorspertrack;
  ULO reservedblocks_original;
  ULO surfaces;
  ULO tracks;            /* From here and down, used by the driver */
  BOOLE readonly;
  ULO bytespersector;
  ULO reservedblocks;
  fhfile_status status;
  FILE *F;
  ULO size;
  bool hasRDB;
  RDBHeader *rdb;
  ULO lowCylinder;
  ULO highCylinder;
};

struct HardfileMountListEntry
{
  int DeviceIndex;
  int PartitionIndex;
  string Name;
  ULO NameAddress;

  HardfileMountListEntry(int deviceIndex, int partitionIndex, const string& name)
    : DeviceIndex(deviceIndex), PartitionIndex(partitionIndex), Name(name), NameAddress(0)
  {
  }
};

struct HardfileFileSystemEntry
{
  RDBFileSystemHeader *Header;
  ULO SegListAddress;

  bool IsOlderOrSameFileSystemVersion(ULO DOSType, ULO version);

  bool IsDOSType(ULO DOSType);
  bool IsOlderVersion(ULO version);
  bool IsOlderOrSameVersion(ULO version);

  ULO GetDOSType();
  ULO GetVersion();

  HardfileFileSystemEntry(RDBFileSystemHeader *header, ULO segListAddress);
};

class HardfileHandler
{
private:
  HardfileDevice _devices[FHFILE_MAX_DEVICES];
  vector<HardfileFileSystemEntry> _fileSystems;
  vector<HardfileMountListEntry> _mountList;
  ULO _romstart;
  ULO _bootcode;
  ULO _configdev;
  ULO _fsname;
  UBY _rom[65536];
  BOOLE _enabled;

  bool HasZeroDevices();

  void CreateMountList();
  string MakeDeviceName(int no);
  string MakeDeviceName(const string& preferredName, int no);
  bool PreferredNameExists(const string& preferredName);
  int FindOlderOrSameFileSystemVersion(ULO dosType, ULO version);
  HardfileFileSystemEntry *GetFileSystemForDOSType(ULO dosType);
  void AddFileSystemsFromRdb(HardfileDevice& device);
  void AddFileSystemsFromRdb();
  void EraseOlderOrSameFileSystemVersion(ULO dosType, ULO version);
  void SetPhysicalGeometryFromRDB(HardfileDevice *fhfile);
  void InitializeHardfile(ULO index);

  void SetLed(bool state);

  // BeginIO commands
  void Ignore(ULO index);
  BYT Read(ULO index);
  BYT Write(ULO index);
  void GetNumberOfTracks(ULO index);
  void GetDriveType(ULO index);
  void WriteProt(ULO index);
  void DoDiag();
  void DoOpen();
  void DoClose();
  void DoExpunge();
  void DoNULL();
  void DoBeginIO();
  void DoAbortIO();
  void DoGetRDBFileSystemCount();
  void DoGetRDBFileSystemHunkCount();
  void DoGetRDBFileSystemHunkSize();
  void DoRelocateHunk();
  void DoInitializeRDBFileSystemEntry();
  void DoPatchDOSDeviceNode();
  void DoLogAvailableFileSystems();

  string LogGetStringFromMemory(ULO address);
  void DoLogAvailableResources();
  void DoLogAllocMemResult();
  void DoLogOpenResourceResult();
  void DoRemoveRDBFileSystemsAlreadySupportedBySystem();

  void MakeDOSDevPacketForPlainHardfile(const HardfileMountListEntry& partition, ULO deviceNameAddress);
  void MakeDOSDevPacketForRDBPartition(const HardfileMountListEntry& partition, ULO deviceNameAddress);

public:
  // Autoconfig and ROM memory
  void CardInit();
  void CardMap(ULO mapping);
  UBY ReadByte(ULO address);
  UWO ReadWord(ULO address);
  ULO ReadLong(ULO address);

  // Native callback
  void Do(ULO data);

  // Configuration
  void SetEnabled(BOOLE enabled);
  BOOLE GetEnabled();
  void Clear();
  bool CompareHardfile(HardfileDevice hardfile, ULO index);
  void SetHardfile(HardfileDevice hardfile, ULO index);
  BOOLE RemoveHardfile(ULO index);

  // UI helper function
  bool Create(HardfileDevice hfile);

  // Global events
  void HardReset();
  void Startup();
  void Shutdown();

  HardfileHandler();
  ~HardfileHandler();
};

extern HardfileHandler hardfileHandler;

#endif
