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
#include "raw_pdb/PDB_TPIStream.h"
#include "pdb_typetable.hpp"

#include <algorithm>
#include <set>
#include <unordered_map>

struct SectionContrib
{
    uint32_t Section;
    uint32_t Offset;
    uint32_t Length;
    uint32_t Compiland;
    int32_t Type;
    int32_t ObjFile;
};

struct PDBSymbol
{
    std::string name;
    uint32_t rva = 0;
    uint32_t length = 0;
    uint32_t section = 0;
    uint32_t offset = 0;
    uint32_t typeIndex = 0;
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

typedef std::unordered_map<uint32_t, PDBSymbol> RVAToSymbolMap;


static void AddSymbol(const SectionContrib* contribs, int contribsCount, uint32_t section, uint32_t offset, const std::string& name, uint32_t length, uint32_t rva, DebugInfo& to)
{
    const SectionContrib* contrib = ContribFromSectionOffset(contribs, contribsCount, section, offset);
    int32_t objFile = 0;
    int32_t sectionType = DIC_UNKNOWN;
    if (contrib)
    {
        objFile = contrib->ObjFile;
        sectionType = contrib->Type;
        if (length == 0)
            length = contrib->Length;
    }

    DISymbol outSym;

    to.Symbols.push_back(DISymbol());
    outSym.name = name.empty() ? "<noname>" : name;
    outSym.objFileNum = objFile;
    outSym.VA = rva;
    outSym.Size = length;
    outSym.Class = sectionType;
    outSym.NameSpNum = to.GetNameSpaceByName(name.c_str());

    to.Symbols.emplace_back(outSym);
}

static void ProcessSymbol(const PDB::ImageSectionStream& imageSectionStream, const PDB::CodeView::DBI::Record* record, RVAToSymbolMap& toMap)
{
    PDBSymbol symbol;
    if (record->header.kind == PDB::CodeView::DBI::SymbolRecordKind::S_PUB32)
    {
        if (PDB_AS_UNDERLYING(record->data.S_PUB32.flags) & PDB_AS_UNDERLYING(PDB::CodeView::DBI::PublicSymbolFlags::Function))
        {
            symbol.name = record->data.S_PUB32.name;
            symbol.section = record->data.S_PUB32.section;
            symbol.offset = record->data.S_PUB32.offset;
        }
    }
    else if (record->header.kind == PDB::CodeView::DBI::SymbolRecordKind::S_LPROC32)
    {
        symbol.name = record->data.S_LPROC32.name;
        symbol.section = record->data.S_LPROC32.section;
        symbol.offset = record->data.S_LPROC32.offset;
        symbol.length = record->data.S_LPROC32.codeSize;
    }
    else if (record->header.kind == PDB::CodeView::DBI::SymbolRecordKind::S_GPROC32)
    {
        symbol.name = record->data.S_GPROC32.name;
        symbol.section = record->data.S_GPROC32.section;
        symbol.offset = record->data.S_GPROC32.offset;
        symbol.length = record->data.S_GPROC32.codeSize;
    }
    else if (record->header.kind == PDB::CodeView::DBI::SymbolRecordKind::S_LPROC32_ID)
    {
        symbol.name = record->data.S_LPROC32_ID.name;
        symbol.section = record->data.S_LPROC32_ID.section;
        symbol.offset = record->data.S_LPROC32_ID.offset;
        symbol.length = record->data.S_LPROC32_ID.codeSize;
    }
    else if (record->header.kind == PDB::CodeView::DBI::SymbolRecordKind::S_GPROC32_ID)
    {
        symbol.name = record->data.S_GPROC32_ID.name;
        symbol.section = record->data.S_GPROC32_ID.section;
        symbol.offset = record->data.S_GPROC32_ID.offset;
        symbol.length = record->data.S_GPROC32_ID.codeSize;
    }
    else if (record->header.kind == PDB::CodeView::DBI::SymbolRecordKind::S_LDATA32)
    {
        symbol.name = record->data.S_LDATA32.name;
        // Often there are LDATA32 symbols without a name that are same size as function entries? Skip those.
        if (!symbol.name.empty())
        {
            symbol.section = record->data.S_LDATA32.section;
            symbol.offset = record->data.S_LDATA32.offset;
            symbol.typeIndex = record->data.S_LDATA32.typeIndex;
        }
    }
    else if (record->header.kind == PDB::CodeView::DBI::SymbolRecordKind::S_GDATA32)
    {
        symbol.name = record->data.S_GDATA32.name;
        symbol.section = record->data.S_GDATA32.section;
        symbol.offset = record->data.S_GDATA32.offset;
        symbol.typeIndex = record->data.S_GDATA32.typeIndex;
    }
    else if (record->header.kind == PDB::CodeView::DBI::SymbolRecordKind::S_LTHREAD32)
    {
        symbol.name = record->data.S_LTHREAD32.name;
        symbol.section = record->data.S_LTHREAD32.section;
        symbol.offset = record->data.S_LTHREAD32.offset;
        symbol.typeIndex = record->data.S_LTHREAD32.typeIndex;
    }
    else if (record->header.kind == PDB::CodeView::DBI::SymbolRecordKind::S_GTHREAD32)
    {
        symbol.name = record->data.S_GTHREAD32.name;
        symbol.section = record->data.S_GTHREAD32.section;
        symbol.offset = record->data.S_GTHREAD32.offset;
        symbol.typeIndex = record->data.S_GTHREAD32.typeIndex;
    }
    symbol.rva = imageSectionStream.ConvertSectionOffsetToRVA(symbol.section, symbol.offset);
    if (symbol.rva == 0u)
    {
        // certain symbols (e.g. control-flow guard symbols) don't have a valid RVA, ignore those
        return;
    }

    toMap.insert({ symbol.rva, symbol });
}


static void ReadEverything(const PDB::RawFile& rawPdbFile, const PDB::DBIStream& dbiStream, DebugInfo &to)
{
    fprintf(stderr, "[      ]");

    // create the PDB streams
    const PDB::ImageSectionStream imageSectionStream = dbiStream.CreateImageSectionStream(rawPdbFile);
    const PDB::ModuleInfoStream moduleInfoStream = dbiStream.CreateModuleInfoStream(rawPdbFile);
    const PDB::SectionContributionStream sectionContributionStream = dbiStream.CreateSectionContributionStream(rawPdbFile);
    const PDB::CoalescedMSFStream symbolRecordStream = dbiStream.CreateSymbolRecordStream(rawPdbFile);

    // get all section contributions
    const PDB::ArrayView<PDB::DBI::SectionContribution> sectionContributions = sectionContributionStream.GetContributions();
    std::vector<SectionContrib> contributions;
    size_t sectionContribsSize = sectionContributions.GetLength();
    contributions.reserve(sectionContribsSize);

    size_t processedContribsCount = 0;
    for (const PDB::DBI::SectionContribution& srcContrib : sectionContributions)
    {
        ++processedContribsCount;
        if ((processedContribsCount & 65535) == 0)
            fprintf(stderr, "\b\b\b\b\b\b\b\b[%5.1f%%]", processedContribsCount * 10.0 / sectionContribsSize);

        const uint32_t rva = imageSectionStream.ConvertSectionOffsetToRVA(srcContrib.section, srcContrib.offset);
        if (rva == 0u)
        {
            fprintf(stderr, "  Contribution (section %i, offset %i) has invalid RVA\n", srcContrib.section, srcContrib.offset);
            continue;
        }

        SectionContrib contrib = {};
        contrib.Offset = srcContrib.offset;
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
        contributions.emplace_back(contrib);
    }

    RVAToSymbolMap rvaToSymbol;
    rvaToSymbol.reserve(1024);

    // get symbols from the modules
    const PDB::ArrayView<PDB::ModuleInfoStream::Module> modules = moduleInfoStream.GetModules();
    size_t moduleCount = modules.GetLength();
    size_t processedModuleCount = 0;
    for (const PDB::ModuleInfoStream::Module& module : modules)
    {
        ++processedModuleCount;
        if ((processedModuleCount & 127) == 0)
            fprintf(stderr, "\b\b\b\b\b\b\b\b[%5.1f%%]", 10.0 + processedModuleCount * 40.0 / moduleCount);
        if (!module.HasSymbolStream())
            continue;

        const PDB::ModuleSymbolStream moduleSymbolStream = module.CreateSymbolStream(rawPdbFile);
        moduleSymbolStream.ForEachSymbol([&](const PDB::CodeView::DBI::Record* record)
        {
            ProcessSymbol(imageSectionStream, record, rvaToSymbol);
        });
    }

    // get global symbols
    {
        const PDB::GlobalSymbolStream globalSymbolStream = dbiStream.CreateGlobalSymbolStream(rawPdbFile);
        const PDB::ArrayView<PDB::HashRecord> hashRecords = globalSymbolStream.GetRecords();
        for (const PDB::HashRecord& hashRecord : hashRecords)
        {
            const PDB::CodeView::DBI::Record* record = globalSymbolStream.GetRecord(symbolRecordStream, hashRecord);
            ProcessSymbol(imageSectionStream, record, rvaToSymbol);
        }
    }
    // There can be public function symbols we haven't seen yet in any of the modules, especially for PDBs that don't provide module-specific information.
    {
        const PDB::PublicSymbolStream publicSymbolStream = dbiStream.CreatePublicSymbolStream(rawPdbFile);
        const PDB::ArrayView<PDB::HashRecord> hashRecords = publicSymbolStream.GetRecords();
        for (const PDB::HashRecord& hashRecord : hashRecords)
        {
            const PDB::CodeView::DBI::Record* record = publicSymbolStream.GetRecord(symbolRecordStream, hashRecord);
            ProcessSymbol(imageSectionStream, record, rvaToSymbol);
        }
    }

    // Gather all symbols, sort by RVA, figure out sizes of the ones that did not have a size
    std::vector<PDBSymbol> rvaSortedSymbols;
    const size_t symbolCount = rvaToSymbol.size();
    rvaSortedSymbols.reserve(symbolCount);
    for (const auto& sym : rvaToSymbol)
        rvaSortedSymbols.push_back(sym.second);
    rvaToSymbol.clear();

    std::sort(rvaSortedSymbols.begin(), rvaSortedSymbols.end(), [](const auto& a, const auto& b) { return a.rva < b.rva; });

    if (symbolCount != 0)
    {
        const PDB::TPIStream tpiStream = PDB::CreateTPIStream(rawPdbFile);
        //@TODO: need to check for valid TPI streams? check with stripped PDBs
        TypeTable typeTable(tpiStream);

        std::unordered_map<uint32_t, size_t> typeSizeCache;

        for (size_t i = 0; i < symbolCount; ++i)
        {
            PDBSymbol& curr = rvaSortedSymbols[i];
            if (curr.length != 0)
                continue;

            // Estimate symbol length by: type size, contribution size, difference between curr and next
            // symbol. Whichever is available and smaller.

            // Type size:
            if (curr.typeIndex != 0)
            {
                size_t typeSize = 0;
                auto it = typeSizeCache.find(curr.typeIndex);
                if (it != typeSizeCache.end())
                {
                    typeSize = it->second;
                }
                {
                    typeSize = PDBGetTypeSize(typeTable, curr.typeIndex);
                    typeSizeCache.insert({curr.typeIndex, typeSize});
                }
                if (typeSize != 0)
                    curr.length = typeSize;
            }

            // Contribution:
            const SectionContrib* contrib = ContribFromSectionOffset(contributions.data(), contributions.size(), curr.section, curr.offset);
            if (contrib && (contrib->Length < curr.length || curr.length == 0))
                curr.length = contrib->Length;

            // Difference between symbols:
            if (i != symbolCount - 1)
            {
                const PDBSymbol& next = rvaSortedSymbols[i + 1];
                uint32_t rvaSize = next.rva - curr.rva;
                if (rvaSize != 0 && (rvaSize < curr.length || curr.length == 0))
                    curr.length = rvaSize;
            }
        }
    }

    // Add symbols to the destination map
    size_t addedSymbolCount = 0;
    for (const PDBSymbol& sym : rvaSortedSymbols)
    {
        ++addedSymbolCount;
        if ((addedSymbolCount & 65535) == 0)
            fprintf(stderr, "\b\b\b\b\b\b\b\b[%5.1f%%]", 50.0 + addedSymbolCount * 50.0 / symbolCount);
        AddSymbol(contributions.data(), contributions.size(), sym.section, sym.offset, sym.name, sym.length, sym.rva, to);
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

    if (dbiStream.GetHeader().flags & 0x1)
    {
        printf("Warning: PDB file is created with incremental linking, some information might be misleading.\n");
    }
    if (dbiStream.GetHeader().flags & 0x2)
    {
        printf("Warning: PDB file is stripped, some information might be missing or misleading.\n");
    }

    ReadEverything(rawPdbFile, dbiStream, to);

    return true;
}
