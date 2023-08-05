// Executable size report utility.
// Aras Pranckevicius, https://aras-p.info/projSizer.html
// Based on code by Fabian "ryg" Giesen, http://farbrausch.com/~fg/
// Public domain.

#pragma once

#include <map>
#include <string>
#include <vector>

enum class SectionType
{
    Unknown,
    Code,
    Data,
    BSS,
};

struct DISymFile // File
{
    int32_t  fileName;
    uint32_t  codeSize;
    uint32_t  dataSize;
};

struct DISymNameSp // Namespace
{
    int32_t  name = 0;
    uint32_t  codeSize = 0;
    uint32_t  dataSize = 0;
};

struct DISymbol
{
    std::string name;
    int32_t NameSpNum = 0;
    int32_t objFileNum = 0;
    uint32_t VA = 0;
    uint32_t Size = 0;
    SectionType sectionType = SectionType::Unknown;
};

struct TemplateSymbol
{
    std::string name;
    uint32_t size = 0;
    uint32_t count = 0;
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
    typedef std::vector<std::string>   StringByIndexVector;
    typedef std::map<std::string, int32_t> IndexByStringMap;
    typedef std::map<int32_t, int32_t> NameIndexToArrayIndexMap;

    StringByIndexVector m_StringByIndex;
    IndexByStringMap  m_IndexByString;
    NameIndexToArrayIndexMap m_NameSpaceIndexByName;

    uint32_t CountSizeInSection(SectionType type) const;

public:
    std::vector<DISymbol>  Symbols;
    std::vector<TemplateSymbol>  Templates;
    std::vector<DISymFile> m_Files;
    std::vector<DISymNameSp> NameSps;

    int32_t MakeStringPtr(const char *s);
    int32_t MakeStringStd(const std::string& s);
    const char* GetStringPrep(int32_t index) const { return m_StringByIndex[index].c_str(); }

    void FinishedReading();

    int32_t GetFile(int32_t fileName);
    int32_t GetFileByName(const char *objName);

    int32_t GetNameSpace(int32_t name);
    int32_t GetNameSpaceByName(const char *name);

    void StartAnalyze();
    void FinishAnalyze();

    std::string WriteReport(const DebugFilters& filters);
};
