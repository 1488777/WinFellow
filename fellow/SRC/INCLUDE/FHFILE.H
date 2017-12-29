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

typedef struct
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
  bool hasRigidDiskBlock;
  RDBHeader *rdb;
  ULO lowCylinder;
  ULO highCylinder;
} fhfile_dev;

class HardfileHandler
{
private:
  fhfile_dev _devs[FHFILE_MAX_DEVICES];
  vector<RDBFileSystemHeader*> _rdb_filesystems;
  ULO _romstart;
  ULO _bootcode;
  ULO _configdev;
  UBY _rom[65536];
  BOOLE _enabled;

  bool HasZeroDevices();


  int FindOlderOrSameFileSystemVersion(ULO dosType, ULO version);
  void AddFileSystemsFromRdb(fhfile_dev *device);
  void AddFileSystemsFromRdb();
  void EraseOlderOrSameFileSystemVersion(ULO dosType, ULO version);
  void SetPhysicalGeometryFromRigidDiskBlock(fhfile_dev *fhfile);
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

  string LogGetStringFromMemory(ULO address);
  void DoLogAvailableResources();
  void DoLogAllocMemResult();
  void DoLogOpenResourceResult();
  void DoRemoveRDBFileSystemsAlreadySupportedBySystem();

  void MakeDOSDevPacketForPlainHardfile(ULO devno, ULO unitnameptr, ULO devnameptr);

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
  bool CompareHardfile(fhfile_dev hardfile, ULO index);
  void SetHardfile(fhfile_dev hardfile, ULO index);
  BOOLE RemoveHardfile(ULO index);

  // UI helper function
  bool Create(fhfile_dev hfile);

  // Global events
  void HardReset();
  void Startup();
  void Shutdown();

  HardfileHandler();
  ~HardfileHandler();
};

extern HardfileHandler hardfileHandler;

#endif
