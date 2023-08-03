// Executable size report utility.
// Aras Pranckevicius, https://aras-p.info/projSizer.html
// Based on code by Fabian "ryg" Giesen, http://farbrausch.com/~fg/
// Public domain.

#include "types.hpp"
#include "debuginfo.hpp"
#include "pdbfile.hpp"

#include "mmapfile.h"
#include "raw_pdb/PDB.h"
#include "raw_pdb/PDB_InfoStream.h"
#include "raw_pdb/PDB_RawFile.h"
#include "raw_pdb/PDB_DBIStream.h"


struct SectionContrib
{
    uint32_t Section;
    uint32_t Offset;
    uint32_t Length;
    uint32_t Compiland;
    int32_t Type;
    int32_t ObjFile;
};


static const SectionContrib* ContribFromSectionOffset(const SectionContrib* contribs, int contribsCount, uint32_t sec, uint32_t offs)
{
    int32_t l, r, x;

    l = 0;
    r = contribsCount;

    while (l < r)
    {
        x = (l + r) / 2;
        const SectionContrib &cur = contribs[x];

        if (sec < cur.Section || sec == cur.Section && offs < cur.Offset)
            r = x;
        else if (sec > cur.Section || sec == cur.Section && offs >= cur.Offset + cur.Length)
            l = x + 1;
        else if (sec == cur.Section && offs >= cur.Offset && offs < cur.Offset + cur.Length) // we got a winner
            return &cur;
        else
            break; // there's nothing here!
    }

    // normally, this shouldn't happen!
    return 0;
}

static void ProcessSymbol(const SectionContrib* contribs, int contribsCount, const PDB::ImageSectionStream& imageSectionStream, const PDB::CodeView::DBI::Record* record, DebugInfo &to)
{
    const char* name = nullptr;
    uint32_t section = 0u;
    uint32_t offset = 0u;
    uint32_t length = 0u;
    if (record->header.kind == PDB::CodeView::DBI::SymbolRecordKind::S_LPROC32)
    {
        name = record->data.S_LPROC32.name;
        section = record->data.S_LPROC32.section;
        offset = record->data.S_LPROC32.offset;
        length = record->data.S_LPROC32.codeSize;
    }
    else if (record->header.kind == PDB::CodeView::DBI::SymbolRecordKind::S_GPROC32)
    {
        name = record->data.S_GPROC32.name;
        section = record->data.S_GPROC32.section;
        offset = record->data.S_GPROC32.offset;
        length = record->data.S_GPROC32.codeSize;
    }
    else if (record->header.kind == PDB::CodeView::DBI::SymbolRecordKind::S_LPROC32_ID)
    {
        name = record->data.S_LPROC32_ID.name;
        section = record->data.S_LPROC32_ID.section;
        offset = record->data.S_LPROC32_ID.offset;
        length = record->data.S_LPROC32_ID.codeSize;
    }
    else if (record->header.kind == PDB::CodeView::DBI::SymbolRecordKind::S_GPROC32_ID)
    {
        name = record->data.S_GPROC32_ID.name;
        section = record->data.S_GPROC32_ID.section;
        offset = record->data.S_GPROC32_ID.offset;
        length = record->data.S_GPROC32_ID.codeSize;
    }
    else if (record->header.kind == PDB::CodeView::DBI::SymbolRecordKind::S_LDATA32)
    {
        name = record->data.S_LDATA32.name;
        section = record->data.S_LDATA32.section;
        offset = record->data.S_LDATA32.offset;
    }
	else if (record->header.kind == PDB::CodeView::DBI::SymbolRecordKind::S_LTHREAD32)
    {
        name = record->data.S_LTHREAD32.name;
        section = record->data.S_LTHREAD32.section;
        offset = record->data.S_LTHREAD32.offset;
    }
    uint32_t rva = imageSectionStream.ConvertSectionOffsetToRVA(section, offset);
    if (rva == 0u)
    {
        // certain symbols (e.g. control-flow guard symbols) don't have a valid RVA, ignore those
        return;
    }


    const SectionContrib *contrib = ContribFromSectionOffset(contribs, contribsCount, section, offset);
    int32_t objFile = 0;
    int32_t sectionType = DIC_UNKNOWN;
    if (contrib)
    {
        objFile = contrib->ObjFile;
        sectionType = contrib->Type;
		if (length == 0)
			length = contrib->Length;
    }

	if (name == nullptr || name[0] == 0)
		name = "<noname>";

    DISymbol outSym;

    to.Symbols.push_back(DISymbol());
    outSym.mangledName = to.MakeString(name);
    outSym.name = to.MakeString(name); //@TODO: undecorated name?
    outSym.objFileNum = objFile;
    outSym.VA = rva;
    outSym.Size = length;
    outSym.Class = sectionType;
    outSym.NameSpNum = to.GetNameSpaceByName(name);

    to.Symbols.emplace_back(outSym);
}


static void ReadEverything(const PDB::RawFile& rawPdbFile, const PDB::DBIStream& dbiStream, DebugInfo &to)
{
    //@TODO: could load the streams in parallel
    // prepare the image section stream first. it is needed for converting section + offset into an RVA
    const PDB::ImageSectionStream imageSectionStream = dbiStream.CreateImageSectionStream(rawPdbFile);
    // prepare the module info stream for matching contributions against files
    const PDB::ModuleInfoStream moduleInfoStream = dbiStream.CreateModuleInfoStream(rawPdbFile);
    // read contribution stream
    const PDB::SectionContributionStream sectionContributionStream = dbiStream.CreateSectionContributionStream(rawPdbFile);

    // get all section contributions
    const PDB::ArrayView<PDB::DBI::SectionContribution> sectionContributions = sectionContributionStream.GetContributions();
    std::vector<SectionContrib> contributions;
    contributions.reserve(sectionContributions.GetLength());

    for (const PDB::DBI::SectionContribution& srcContrib : sectionContributions)
    {
        const uint32_t rva = imageSectionStream.ConvertSectionOffsetToRVA(srcContrib.section, srcContrib.offset);
        if (rva == 0u)
        {
            fprintf(stderr, "  Contribution (section %i, offset %i) has invalid RVA\n", srcContrib.section, srcContrib.offset);
            continue;
        }

        SectionContrib contrib = {};
        contrib.Offset = srcContrib.offset; //@TODO: rva instead?
        contrib.Section = srcContrib.section;
        contrib.Length = srcContrib.size;
        contrib.Compiland = srcContrib.moduleIndex;

        // from IMAGE_SECTION_HEADER
        const uint32_t IMAGE_SCN_CNT_CODE = 0x20;
        const uint32_t IMAGE_SCN_CNT_INITIALIZED_DATA = 0x40;
        const uint32_t IMAGE_SCN_CNT_UNINITIALIZED_DATA = 0x80;
        bool hasCode = srcContrib.characteristics & IMAGE_SCN_CNT_CODE;
        bool hasInitData = srcContrib.characteristics & IMAGE_SCN_CNT_INITIALIZED_DATA;
        bool hasUninitData = srcContrib.characteristics & IMAGE_SCN_CNT_UNINITIALIZED_DATA;
        if (hasCode && !hasInitData && !hasUninitData)
            contrib.Type = DIC_CODE;
        else if (!hasCode && hasInitData && !hasUninitData)
            contrib.Type = DIC_DATA;
        else if (!hasCode && !hasInitData && hasUninitData)
            contrib.Type = DIC_BSS;
        else
            contrib.Type = DIC_UNKNOWN;

        const PDB::ModuleInfoStream::Module& module = moduleInfoStream.GetModule(srcContrib.moduleIndex); //@TODO: "<noobjfile>" when none?
        contrib.ObjFile = to.GetFileByName(module.GetName().Decay());
        if (contrib.Type == DIC_DATA || contrib.Type == DIC_BSS)
        {
            //printf("Data: type %i length %i obj %s\n", contrib.Type, contrib.Length, module.GetName().Decay());
        }
        contributions.emplace_back(contrib);
    }

    /*
    // Note: this was the original code; that was however extremely slow especially on larger or 64 bit binaries.
    // New code that replaces it (however it does not produce 100% identical results) is below.

    // enumerate symbols by (virtual) address
    IDiaEnumSymbolsByAddr *enumByAddr;
    if(SUCCEEDED(Session->getSymbolsByAddr(&enumByAddr)))
    {
      IDiaSymbol *symbol;
      // get first symbol to get first RVA (argh)
      if(SUCCEEDED(enumByAddr->symbolByAddr(1,0,&symbol)))
      {
        uint32_t rva;
        if(symbol->get_relativeVirtualAddress(&rva) == S_OK)
        {
          symbol->Release();

          // now, enumerate by rva.
          if(SUCCEEDED(enumByAddr->symbolByRVA(rva,&symbol)))
          {
            do
            {
              ProcessSymbol(symbol,to);
              symbol->Release();

              if(FAILED(enumByAddr->Next(1,&symbol,&celt)))
                break;
            }
            while(celt == 1);
          }
        }
        else
          symbol->Release();
      }

      enumByAddr->Release();
    }
    */

    // This is new code that replaces commented out code above. On one not-too-big executable this gets Sizer execution time from
    // 448 seconds down to 1.5 seconds. However it does not list some symbols that are "weird" and are likely due to linker padding
    // or somesuch; I did not dig in. On that particular executable, e.g. 128 kb that is coming from "* Linker *" file is gone.

    const PDB::ArrayView<PDB::ModuleInfoStream::Module> modules = moduleInfoStream.GetModules();
    size_t moduleCount = modules.GetLength();
    size_t processedModuleCount = 0;
    fprintf(stderr, "[      ]");
    for (const PDB::ModuleInfoStream::Module& module : modules)
    {
        ++processedModuleCount;
        fprintf(stderr, "\b\b\b\b\b\b\b\b[%5.1f%%]", processedModuleCount * 100.0 / moduleCount);
        if (!module.HasSymbolStream())
            continue;

        const PDB::ModuleSymbolStream moduleSymbolStream = module.CreateSymbolStream(rawPdbFile);

        moduleSymbolStream.ForEachSymbol([&](const PDB::CodeView::DBI::Record* record)
        {
            ProcessSymbol(contributions.data(), contributions.size(), imageSectionStream, record, to);
        });
    }
}

// check whether the DBI stream offers all sub-streams we need
static bool HasValidDBIStreams(const PDB::RawFile& rawPdbFile, const PDB::DBIStream& dbiStream)
{
    if (dbiStream.HasValidImageSectionStream(rawPdbFile) != PDB::ErrorCode::Success)
        return false;
    if (dbiStream.HasValidPublicSymbolStream(rawPdbFile) != PDB::ErrorCode::Success)
        return false;
    if (dbiStream.HasValidGlobalSymbolStream(rawPdbFile) != PDB::ErrorCode::Success)
        return false;
    if (dbiStream.HasValidSectionContributionStream(rawPdbFile) != PDB::ErrorCode::Success)
        return false;
    return true;
}

bool ReadDebugInfo(const char *fileName, DebugInfo &to)
{
    //@TODO: if given an exe file, try to find where PDB of it is

    // open the PDB file
    MemoryMappedFile pdbFile(fileName);
    if (pdbFile.baseAddress == nullptr)
    {
        fprintf(stderr, "  failed to memory-map PDB file '%s'\n", fileName);
        return false;
    }
    PDB::ErrorCode errorCode = PDB::ValidateFile(pdbFile.baseAddress);
    if (errorCode != PDB::ErrorCode::Success)
    {
        fprintf(stderr, "  failed to validate PDB file '%s': error code %i\n", fileName, (int)errorCode);
        return false;
    }
    const PDB::RawFile rawPdbFile = PDB::CreateRawFile(pdbFile.baseAddress);
    errorCode = PDB::HasValidDBIStream(rawPdbFile);
    if (errorCode != PDB::ErrorCode::Success)
    {
        fprintf(stderr, "  PDB file '%s' does not have valid DBI stream, error code %i\n", fileName, (int)errorCode);
        return false;
    }
    const PDB::InfoStream infoStream(rawPdbFile);
    if (infoStream.UsesDebugFastLink())
    {
        fprintf(stderr, "  PDB file '%s' uses unsupported option /DEBUG:FASTLINK\n", fileName);
        return false;
    }
    const PDB::DBIStream dbiStream = PDB::CreateDBIStream(rawPdbFile);
    if (!HasValidDBIStreams(rawPdbFile, dbiStream))
    {
        fprintf(stderr, "  PDB file '%s' does not have required DBI sections\n", fileName);
        return false;
    }

    ReadEverything(rawPdbFile, dbiStream, to);

    return true;
}
