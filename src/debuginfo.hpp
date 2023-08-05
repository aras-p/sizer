// Executable size report utility.
// Aras Pranckevicius, https://aras-p.info/projSizer.html
// Based on code by Fabian "ryg" Giesen, http://farbrausch.com/~fg/
// Public domain.

#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

enum class SectionType
{
    Unknown,
    Code,
    Data,
    BSS,
};

struct SymbolInfo
{
    std::string name;
    int32_t namespaceIndex = 0;
    int32_t objectFileIndex = 0;
    uint32_t size = 0;
    SectionType sectionType = SectionType::Unknown;
};

struct ObjectFileInfo
{
    std::string fileDir;
    std::string fileName;
    int32_t index = 0;
    uint32_t codeSize = 0;
    uint32_t dataSize = 0;
};

struct NamespaceInfo
{
    std::string name;
    int32_t index = 0;
    uint32_t  codeSize = 0;
    uint32_t  dataSize = 0;
};

struct TemplateInfo
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
public:
    std::vector<SymbolInfo>  m_Symbols;

    int32_t GetObjectFileIndex(const char* pathStr);
    int32_t GetNameSpaceIndex(const std::string& symName);

    void ComputeDerivedData();

    std::string WriteReport(const DebugFilters& filters);

private:
    uint32_t CountSizeInSection(SectionType type) const;
    std::string GetObjectFileDesc(int index) const;

private:
    std::vector<NamespaceInfo> m_Namespaces;
    std::map<std::string, int32_t> m_NamespaceToIndex;

    std::vector<ObjectFileInfo> m_ObjectFiles;
    std::map<std::string, int32_t> m_ObjectPathToIndex;
    std::map<std::string, std::set<std::string>> m_ObjectNameToFolders;

    std::vector<TemplateInfo> m_Templates;
};
