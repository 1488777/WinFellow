#ifndef HUNKS_H
#define HUNKS_H

#include "DEFS.H"
#include <vector>
#include <string>
#include <memory>

//---------------------------------------------------------------------------

class RawReader
{
private:
  UBY *_rawData;
  ULO _address;
  ULO GetLong(ULO address);

public:
  ULO GetNextByteswappedLong();
  std::string GetNextString(ULO lengthInLongwords);
  UBY *GetNextBytes(ULO lengthInLongwords);

  RawReader(UBY *rawReader);
};

//---------------------------------------------------------------------------

struct HunkSize
{
  ULO Size;
  ULO AdditionalFlags;

  HunkSize(ULO size, ULO additionalFlags);
};

//---------------------------------------------------------------------------

class Hunk
{
public:
  virtual bool Read(RawReader& rawReader);
  virtual ULO GetSizeInLongwords();
  virtual UBY* Relocate(ULO baseAddress);

  Hunk();
  virtual ~Hunk();
};
typedef std::unique_ptr<Hunk> HunkPtr;

//---------------------------------------------------------------------------

class Header : public Hunk
{
public:
  std::vector<std::string> ResidentLibraries;
  std::vector<HunkSize> HunkSizes;

  bool Read(RawReader& rawReader) override;

  Header();
  virtual ~Header();
};

//---------------------------------------------------------------------------

class Code : public Hunk
{
private:
  ULO SizeInLongwords;
  UBY *RawData;

public:
  bool Read(RawReader& rawReader) override;
  ULO GetSizeInLongwords() override;
  UBY* Relocate(ULO baseAddress) override;

  Code();
  virtual ~Code();
};

//---------------------------------------------------------------------------

class Data : public Hunk
{
private:
  ULO SizeInLongwords;
  UBY *RawData;

public:
  bool Read(RawReader& rawReader) override;
  ULO GetSizeInLongwords() override;
  UBY* Relocate(ULO baseAddress) override;

  Data();
  virtual ~Data();
};

//---------------------------------------------------------------------------

class BSS : public Hunk
{
private:
  ULO SizeInLongwords;

public:
  bool Read(RawReader& rawReader) override;
  ULO GetSizeInLongwords() override;
  UBY* Relocate(ULO baseAddress) override;

  BSS();
  virtual ~BSS();
};

//---------------------------------------------------------------------------

class HunkFactory
{
public:
  static Hunk* Create(ULO type);
};

//---------------------------------------------------------------------------

class HunkParser
{
private:
  UBY *_rawData;

  Header* ReadHeader(RawReader& rawReader);

public:
  bool Parse(std::vector<HunkPtr>& hunks);

  HunkParser(UBY *rawData);
};

#endif
