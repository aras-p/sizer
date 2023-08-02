// Executable size report utility.
// Aras Pranckevicius, http://aras-p.info/projSizer.html
// Based on code by Fabian "ryg" Giesen, http://farbrausch.com/~fg/
// Public domain.

#ifndef __DEBUGINFO_HPP__
#define __DEBUGINFO_HPP__

#include "types.hpp"
#include <map>

using std::string;

/****************************************************************************/

#define DIC_END     0
#define DIC_CODE    1
#define DIC_DATA    2
#define DIC_BSS     3 // uninitialized data
#define DIC_UNKNOWN 4

struct DISymFile // File
{
    int32_t  fileName;
    uint32_t  codeSize;
    uint32_t  dataSize;
};

struct DISymNameSp // Namespace
{
    int32_t  name;
    uint32_t  codeSize;
    uint32_t  dataSize;
};

struct DISymbol
{
    int32_t name;
    int32_t mangledName;
    int32_t NameSpNum;
    int32_t objFileNum;
    uint32_t VA;
    uint32_t Size;
    int32_t Class;
};

struct TemplateSymbol
{
    std::string name;
    std::string mangledName;
    uint32_t size;
    uint32_t count;
};

struct DebugFilters
{
    DebugFilters() : minFunction(512), minData(1024), minClass(2048), minFile(2048), minTemplate(512), minTemplateCount(3) { }
    void SetMinSize(int m)
    {
        minFunction = minData = minClass = minFile = minTemplate = m;
    }
    std::string name;
    int minFunction;
    int minData;
    int minClass;
    int minFile;
    int minTemplate;
    int minTemplateCount;
};

class DebugInfo
{
    typedef std::vector<string>   StringByIndexVector;
    typedef std::map<string, int32_t> IndexByStringMap;

    StringByIndexVector m_StringByIndex;
    IndexByStringMap  m_IndexByString;

    uint32_t CountSizeInClass(int32_t type) const;

public:
    std::vector<DISymbol>  Symbols;
    std::vector<TemplateSymbol>  Templates;
    std::vector<DISymFile> m_Files;
    std::vector<DISymNameSp> NameSps;

    // only use those before reading is finished!!
    int32_t MakeString(char *s);
    const char* GetStringPrep(int32_t index) const { return m_StringByIndex[index].c_str(); }

    void FinishedReading();

    int32_t GetFile(int32_t fileName);
    int32_t GetFileByName(char *objName);

    int32_t GetNameSpace(int32_t name);
    int32_t GetNameSpaceByName(char *name);

    void StartAnalyze();
    void FinishAnalyze();
    bool FindSymbol(uint32_t VA, DISymbol **sym);

    std::string WriteReport(const DebugFilters& filters);
};

class DebugInfoReader
{
public:
    virtual bool ReadDebugInfo(const char *fileName, DebugInfo &to) = 0;
};


#endif
