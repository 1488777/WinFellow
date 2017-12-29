#include "DEFS.H"
#include "FELLOW.H"
#include "Hunks.h"

//---------------------------------------------------------------------------

HunkSize::HunkSize(ULO size, ULO additionalFlags)
  : Size(size), AdditionalFlags(additionalFlags)
{
}

//---------------------------------------------------------------------------

bool Hunk::Read(RawReader& rawReader)
{
  return false;
}

ULO Hunk::GetSizeInLongwords()
{
  return 0;
}

UBY* Hunk::Relocate(ULO baseAddress)
{
  return nullptr;
}

Hunk::Hunk()
{  
}

Hunk::~Hunk()
{ 
}

//---------------------------------------------------------------------------

bool Header::Read(RawReader& rawReader)
{
  fellowAddLog("fhfile: RDB filesystem header hunk\n");

  ULO stringLength = rawReader.GetNextByteswappedLong();
  while (stringLength != 0)
  {
    ResidentLibraries.push_back(rawReader.GetNextString(stringLength));
    fellowAddLog("fhfile: RDB filesystem resident library name, '%s'\n", ResidentLibraries.back().c_str());

    stringLength = rawReader.GetNextByteswappedLong();
  }

  ULO tableSize = rawReader.GetNextByteswappedLong();

  fellowAddLog("fhfile: RDB filesystem hunk table size: %u\n", tableSize);

  ULO firstHunk = rawReader.GetNextByteswappedLong();
  ULO lastHunk = rawReader.GetNextByteswappedLong();

  for (ULO i = firstHunk; i <= lastHunk; i++)
  {
    ULO hunkSize = rawReader.GetNextByteswappedLong();
    ULO additionalFlags = 0;
    ULO hunkFlags = hunkSize >> 30;
    if (hunkFlags == 3)
    {
      additionalFlags = rawReader.GetNextByteswappedLong();
    }
    HunkSizes.push_back(HunkSize(hunkSize, additionalFlags));
    fellowAddLog("fhfile: RDB filesystem hunk %u size: %u %s\n", i, (hunkSize & 0x3fffffff) * 4, (hunkFlags == 0) ? "Any memory" : ((hunkFlags == 1) ? "Chip memory" : ((hunkFlags == 2) ? "Fast memory" : "With additional memory flags")));
  }

  return true;
}

Header::Header()
  : Hunk()
{
}

Header::~Header()
{
}

//---------------------------------------------------------------------------

bool Code::Read(RawReader& rawReader)
{
  SizeInLongwords = rawReader.GetNextByteswappedLong();
  RawData = rawReader.GetNextBytes(SizeInLongwords);

  fellowAddLog("fhfile: RDB Filesystem read code hunk, length %u\n", SizeInLongwords * 4);
  return true;
}

ULO Code::GetSizeInLongwords()
{
  return SizeInLongwords & 0x3fffffff;
}

UBY* Code::Relocate(ULO baseAddress)
{
  if (GetSizeInLongwords() == 0)
  {
    return nullptr;
  }

  UBY *relocatedData = new UBY[GetSizeInLongwords() * 4];
  memcpy(relocatedData, RawData, GetSizeInLongwords() * 4);
  return relocatedData;
}

Code::Code()
  : Hunk(), SizeInLongwords(0), RawData(nullptr)
{
}

Code::~Code()
{
  if (RawData != nullptr)
  {
    delete RawData;
  }
}

//---------------------------------------------------------------------------

bool Data::Read(RawReader& rawReader)
{
  SizeInLongwords = rawReader.GetNextByteswappedLong();
  RawData = rawReader.GetNextBytes(SizeInLongwords);

  fellowAddLog("fhfile: RDB filesystem read data hunk, length %u\n", SizeInLongwords * 4);
  return true;
}

ULO Data::GetSizeInLongwords()
{
  return SizeInLongwords & 0x3fffffff;
}

UBY* Data::Relocate(ULO baseAddress)
{
  if (GetSizeInLongwords() == 0)
  {
    return nullptr;
  }

  UBY *relocatedData = new UBY[GetSizeInLongwords() * 4];
  memcpy(relocatedData, RawData, GetSizeInLongwords() * 4);
  return relocatedData;
}

Data::Data()
  : Hunk(), SizeInLongwords(0), RawData(nullptr)
{
}

Data::~Data()
{  
  if (RawData != nullptr)
  {
    delete RawData;
  }
}

//---------------------------------------------------------------------------

bool BSS::Read(RawReader& rawReader)
{
  SizeInLongwords = rawReader.GetNextByteswappedLong();

  fellowAddLog("fhfile: RDB filesystem read bss hunk, length %u\n", SizeInLongwords * 4);
  return true;
}

ULO BSS::GetSizeInLongwords()
{
  return SizeInLongwords & 0x3fffffff;
}

UBY* BSS::Relocate(ULO baseAddress)
{
  if (GetSizeInLongwords() == 0)
  {
    return nullptr;
  }

  UBY *relocatedData = new UBY[GetSizeInLongwords() * 4];
  memset(relocatedData, 0, GetSizeInLongwords() * 4);
  return relocatedData;
}

BSS::BSS()
  : Hunk(), SizeInLongwords(0)
{
}

BSS::~BSS()
{
}

//---------------------------------------------------------------------------

Hunk* HunkFactory::Create(ULO type)
{
  switch (type)
  {
    case 0x3f3: return new Header();
    case 0x3e9: return new Code();
    case 0x3ea: return new Data();
    case 0x3eb: return new BSS();
  }
  return nullptr;
}

//---------------------------------------------------------------------------

ULO RawReader::GetLong(ULO address)
{
  return static_cast<ULO>(_rawData[address]);
}

ULO RawReader::GetNextByteswappedLong()
{
  ULO value = (GetLong(_address) << 24) | (GetLong(_address + 1) << 16) | (GetLong(_address + 2) << 8) | GetLong(_address + 3);
  _address += 4;
  return value;
}

std::string RawReader::GetNextString(ULO lengthInLongwords)
{
  std::string::size_type offset = _address;
  std::string::size_type length = lengthInLongwords * 4;
  _address += length;
  return std::string(reinterpret_cast<char*>(_rawData), offset, length);
}

UBY *RawReader::GetNextBytes(ULO lengthInLongwords)
{
  ULO length = lengthInLongwords * 4;
  UBY *bytes = new UBY[length];
  memcpy(bytes, _rawData + _address, length);
  _address += length;
  return bytes;
}

RawReader::RawReader(UBY *rawData)
  : _rawData(rawData), _address(0)
{
}

//---------------------------------------------------------------------------

Header* HunkParser::ReadHeader(RawReader& rawReader)
{
  ULO type = rawReader.GetNextByteswappedLong();
  if (type != 0x3f3)
  {
    fellowAddLog("fhfile: Header hunk in RDB Filesystem handler is not 0x3f3 - Found type %.X\n", type);
    return nullptr;
  }

  Header* header = new Header();
  bool valid = header->Read(rawReader);
  if (!valid)
  {
    fellowAddLog("fhfile: Error reading contents of header hunk in RDB Filesystem handler\n");
    delete header;
    return nullptr;
  }
  return header;  
}

bool HunkParser::Parse(std::vector<HunkPtr>& hunks)
{
  RawReader rawReader(_rawData);

  Header *header = ReadHeader(rawReader);
  if (header == nullptr)
  {
    hunks.clear();
    return false;
  }
  hunks.push_back(HunkPtr(header));

  int hunkCount = header->HunkSizes.size();
  for (int i = 0; i < hunkCount; i++)
  {
    ULO type = rawReader.GetNextByteswappedLong();
    Hunk *hunk = HunkFactory::Create(type);
    if (hunk == nullptr)
    {
      fellowAddLog("fhfile: Unknown hunk type in RDB Filesystem handler - Type %.X\n", type);
      hunks.clear();
      return false;
    }

    bool valid = hunk->Read(rawReader);
    if (!valid)
    {
      fellowAddLog("fhfile: Error reading contents of hunk in RDB Filesystem handler - Type %.X\n", type);
      hunks.clear();
      return false;
    }
    hunks.push_back(HunkPtr(hunk));
  }

  return true;
}

HunkParser::HunkParser(UBY *rawData)
  : _rawData(rawData)
{  
}
