// Executable size report utility.
// Aras Pranckevicius, https://aras-p.info/projSizer.html
// Public domain.

#include "pe_utils.hpp"
#include <stdint.h>


#define IMAGE_DOS_SIGNATURE 0x5A4D // MZ
#define IMAGE_NT_SIGNATURE 0x00004550 // PE00
#define IMAGE_SIZEOF_SHORT_NAME 8
#define PE_IMAGE_NUMBEROF_DIRECTORY_ENTRIES 16

#define IMAGE_DIRECTORY_ENTRY_DEBUG 6

#define FIELD_OFFSET_IMPL(type, fld) ((size_t)&(((type*)0)->fld))
#define IMAGE_FIRST_SECTION_IMPL(h) ((PE_IMAGE_SECTION_HEADER*) ((size_t)h+FIELD_OFFSET_IMPL(PE_IMAGE_NT_HEADERS,OptionalHeader)+((PE_IMAGE_NT_HEADERS*)(h))->FileHeader.sizeOfOptionalHeader))

struct PE_IMAGE_DOS_HEADER
{
    uint16_t magic;
    uint16_t lastPageBytes;
    uint16_t pageCount;
    uint16_t relocations;
    uint16_t headerParaSize;
    uint16_t minAlloc;
    uint16_t maxAlloc;
    uint16_t initialSS;
    uint16_t initialSP;
    uint16_t checksum;
    uint16_t initialIP;
    uint16_t initialCS;
    uint16_t relocAddr;
    uint16_t overlay;
    uint16_t reserved[4];
    uint16_t oemID;
    uint16_t oemInfo;
    uint16_t reserved2[10];
    int32_t ntHeader;
};

struct PE_IMAGE_FILE_HEADER
{
    uint16_t  machine;
    uint16_t  numberOfSections;
    uint32_t timeDateStamp;
    uint32_t pointerToSymbolTable;
    uint32_t numberOfSymbols;
    uint16_t  sizeOfOptionalHeader;
    uint16_t  characteristics;
};

struct PE_IMAGE_DATA_DIRECTORY
{
    uint32_t va;
    uint32_t size;
};

struct PE_IMAGE_OPTIONAL_HEADER
{
    uint16_t   Magic;
    uint8_t    MajorLinkerVersion;
    uint8_t    MinorLinkerVersion;
    uint32_t   SizeOfCode;
    uint32_t   SizeOfInitializedData;
    uint32_t   SizeOfUninitializedData;
    uint32_t   AddressOfEntryPoint;
    uint32_t   BaseOfCode;
    size_t     ImageBase;

    uint32_t   SectionAlignment;
    uint32_t   FileAlignment;
    uint16_t   MajorOperatingSystemVersion;
    uint16_t   MinorOperatingSystemVersion;
    uint16_t   MajorImageVersion;
    uint16_t   MinorImageVersion;
    uint16_t   MajorSubsystemVersion;
    uint16_t   MinorSubsystemVersion;
    uint32_t   Win32VersionValue;
    uint32_t   SizeOfImage;
    uint32_t   SizeOfHeaders;
    uint32_t   CheckSum;
    uint16_t   Subsystem;
    uint16_t   DllCharacteristics;
    size_t     SizeOfStackReserve;
    size_t     SizeOfStackCommit;
    size_t     SizeOfHeapReserve;
    size_t     SizeOfHeapCommit;
    uint32_t   LoaderFlags;
    uint32_t   NumberOfRvaAndSizes;
    PE_IMAGE_DATA_DIRECTORY DataDirectory[PE_IMAGE_NUMBEROF_DIRECTORY_ENTRIES];
};

struct PE_IMAGE_NT_HEADERS
{
    uint32_t Signature;
    PE_IMAGE_FILE_HEADER FileHeader;
    PE_IMAGE_OPTIONAL_HEADER OptionalHeader;
};

struct PE_IMAGE_SECTION_HEADER
{
    uint8_t  Name[IMAGE_SIZEOF_SHORT_NAME];
    union
    {
        uint32_t PhysicalAddress;
        uint32_t VirtualSize;
    } Misc;
    uint32_t VirtualAddress;
    uint32_t SizeOfRawData;
    uint32_t PointerToRawData;
    uint32_t PointerToRelocations;
    uint32_t PointerToLinenumbers;
    uint16_t NumberOfRelocations;
    uint16_t NumberOfLinenumbers;
    uint32_t Characteristics;
};

#define PE_IMAGE_DEBUG_TYPE_CODEVIEW 2

struct PE_IMAGE_DEBUG_DIRECTORY
{
    uint32_t   Characteristics;
    uint32_t   TimeDateStamp;
    uint16_t   MajorVersion;
    uint16_t   MinorVersion;
    uint32_t   Type;
    uint32_t   SizeOfData;
    uint32_t   AddressOfRawData;
    uint32_t   PointerToRawData;
};

struct PE_DEBUGTYPE_CODEVIEW
{
    uint32_t  Signature;
    uint8_t   Guid[16];
    uint32_t  Age;
    char      PdbFileName[1];
};


static bool PEIsValidFile(const void* data, size_t size)
{
    if (size < sizeof(PE_IMAGE_DOS_HEADER))
        return false;
    const PE_IMAGE_DOS_HEADER* dosHeader = (const PE_IMAGE_DOS_HEADER*)data;
    if (dosHeader->magic != IMAGE_DOS_SIGNATURE)
        return false;
    const PE_IMAGE_NT_HEADERS* ntHeader = (PE_IMAGE_NT_HEADERS*)((const uint8_t*)data + dosHeader->ntHeader);
    if (ntHeader->Signature != IMAGE_NT_SIGNATURE)
        return false;
    return true;
}


std::string PEGetPDBPath(const void* data, size_t size)
{
    if (!PEIsValidFile(data, size))
        return "";

    const PE_IMAGE_DOS_HEADER* dosHeader = (const PE_IMAGE_DOS_HEADER*)data;
    const PE_IMAGE_NT_HEADERS* ntHeader = (PE_IMAGE_NT_HEADERS*)((const uint8_t*)data + dosHeader->ntHeader);

    const PE_IMAGE_DATA_DIRECTORY& debugDir = ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
    if (debugDir.size == 0)
        return "";

    // find which section that is in
    const PE_IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION_IMPL(ntHeader);
    for (int i = 0; i < ntHeader->FileHeader.numberOfSections; ++i, ++section)
    {
        if ((debugDir.va < section->VirtualAddress) || (debugDir.va + debugDir.size > section->VirtualAddress + section->SizeOfRawData))
            continue; // not in this section

        const char* sectionBasePtr = (const char*)data + section->PointerToRawData - section->VirtualAddress;
        const PE_IMAGE_DEBUG_DIRECTORY* entries = (PE_IMAGE_DEBUG_DIRECTORY*)(sectionBasePtr + debugDir.va);
        size_t entryCount = debugDir.size / sizeof(PE_IMAGE_DEBUG_DIRECTORY);
        for (int ei = 0; ei < entryCount; ++ei)
        {
            if (entries[ei].Type == PE_IMAGE_DEBUG_TYPE_CODEVIEW)
            {
                const PE_DEBUGTYPE_CODEVIEW* info = (const PE_DEBUGTYPE_CODEVIEW*)(sectionBasePtr + entries[ei].AddressOfRawData);
                if (memcmp(&info->Signature, "RSDS", 4) == 0)
                {
                    return info->PdbFileName;
                }
            }
        }
    }

    return "";
}
