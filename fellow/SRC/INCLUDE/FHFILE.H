#ifndef FHFILE_H
#define FHFILE_H

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

extern void fhfileDo(ULO data);

extern void fhfileSetEnabled(BOOLE enabled);
extern BOOLE fhfileGetEnabled();
extern void fhfileSetHardfile(fhfile_dev hardfile, ULO index);
extern bool fhfileCompareHardfile(fhfile_dev hardfile, ULO index);
extern BOOLE fhfileRemoveHardfile(ULO index);
extern bool fhfileCreate(fhfile_dev hfile);
extern void fhfileClear();
extern void fhfileHardReset();
extern void fhfileStartup();
extern void fhfileShutdown();

#endif
