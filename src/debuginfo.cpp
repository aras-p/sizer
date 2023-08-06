// Executable size report utility.
// Aras Pranckevicius, https://aras-p.info/projSizer.html
// Based on code by Fabian "ryg" Giesen, http://farbrausch.com/~fg/
// Public domain.

#include "debuginfo.hpp"
#include <stdarg.h>
#include <algorithm>
#include <map>
#include <string.h>

uint32_t DebugInfo::CountSizeInSection(SectionType type) const
{
    uint32_t size = 0;
    for (const auto& sym : m_Symbols)
    {
        if (sym.sectionType == type)
            size += sym.size;
    }
    return size;
}

static bool StripTemplateParams(std::string& str)
{
    bool isTemplate = false;
    size_t start = str.find('<', 0);
    while (start != std::string::npos)
    {
        isTemplate = true;
        // scan to matching closing '>'
        size_t i = start + 1;
        int depth = 1;
        while (i < str.size())
        {
            char ch = str[i];
            if (ch == '<')
                ++depth;
            if (ch == '>')
            {
                --depth;
                if (depth == 0)
                    break;
            }
            ++i;
        }
        if (depth != 0)
            return isTemplate; // no matching '>', just return

        str = str.erase(start, i - start + 1);

        start = str.find('<', start);
    }

    return isTemplate;
}

void DebugInfo::ComputeDerivedData()
{
    std::map<std::string, int> templateToIndex;

    for (const auto& sym : m_Symbols)
    {
        // aggregate templates
        std::string templateName = sym.name;
        bool isTemplate = StripTemplateParams(templateName);
        if (isTemplate)
        {
            auto it = templateToIndex.find(templateName);
            int index;
            if (it != templateToIndex.end())
            {
                index = it->second;
                m_Templates[index].size += sym.size;
                m_Templates[index].count++;
            }
            else
            {
                index = int(m_Templates.size());
                templateToIndex.insert(std::make_pair(templateName, index));
                TemplateInfo info;
                info.name = templateName;
                info.count = 1;
                info.size = sym.size;
                m_Templates.emplace_back(info);
            }
        }

        // aggregate object file / namespace sizes
        if (sym.sectionType == SectionType::Code)
        {
            m_ObjectFiles[sym.objectFileIndex].codeSize += sym.size;
            m_Namespaces[sym.namespaceIndex].codeSize += sym.size;
        }
        else if (sym.sectionType == SectionType::Data)
        {
            m_ObjectFiles[sym.objectFileIndex].dataSize += sym.size;
            m_Namespaces[sym.namespaceIndex].dataSize += sym.size;
        }
    }

    for (const auto& ctr : m_Contribs)
    {
        // aggregate object file / namespace sizes
        if (ctr.sectionType == SectionType::Code)
        {
            m_ObjectFiles[ctr.objectFileIndex].contribCodeSize += ctr.size;
        }
        else if (ctr.sectionType == SectionType::Data)
        {
            m_ObjectFiles[ctr.objectFileIndex].contribDataSize += ctr.size;
        }
    }
}

static void splitPath(const std::string& path, std::string& outDir, std::string& outFile)
{
    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos)
    {
        outDir = path.substr(0, pos);
        outFile = path.substr(pos + 1);
    }
    else
    {
        outDir = "";
        outFile = path;
    }
}

int32_t DebugInfo::GetObjectFileIndex(const char* pathStr)
{
    std::string path = pathStr;
    auto it = m_ObjectPathToIndex.find(path);
    if (it != m_ObjectPathToIndex.end())
        return it->second;

    std::string dir, file;
    splitPath(path, dir, file);

    ObjectFileInfo info;
    info.fileDir = dir;
    info.fileName = file;

    int32_t index = int32_t(m_ObjectPathToIndex.size());
    info.index = index;

    m_ObjectFiles.emplace_back(info);
    m_ObjectPathToIndex.insert(it, {path, index});
    m_ObjectNameToFolders[file].insert(dir);
    return index;
}

std::string DebugInfo::GetObjectFileDesc(int index) const
{
    const ObjectFileInfo& info = m_ObjectFiles[index];
    const auto it = m_ObjectNameToFolders.find(info.fileName);
    if (it == m_ObjectNameToFolders.end() || it->second.size() < 2)
        return info.fileName;
    return info.fileName + " (" + info.fileDir + ")";
}

int32_t DebugInfo::GetNameSpaceIndex(const std::string& symName)
{
    std::string space;
    size_t pos = symName.rfind("::");
    if (pos == std::string::npos || pos == 0)
        space = "<global>";
    else
        space = symName.substr(0, pos);

    const auto it = m_NamespaceToIndex.find(space);
    if (it != m_NamespaceToIndex.end())
        return it->second;

    NamespaceInfo info;
    info.name = space;

    int32_t index = int32_t(m_NamespaceToIndex.size());
    info.index = index;

    m_Namespaces.emplace_back(info);
    m_NamespaceToIndex.insert(it, { space, index });
    return index;
}

static void sAppendPrintF(std::string &str, const char *format, ...)
{
    static const int bufferSize = 512; // cut off after this
    char buffer[bufferSize];
    va_list arg;

    va_start(arg, format);
    vsnprintf(buffer, bufferSize - 1, format, arg);
    va_end(arg);

    strcpy(&buffer[bufferSize - 5], "...\n");
    str += buffer;
}

std::string DebugInfo::WriteReport(const DebugFilters& filters)
{
    std::string Report;
    const char* filterName = filters.name.empty() ? NULL : filters.name.c_str();

    Report.reserve(64 * 1024);
    if (filterName)
    {
        sAppendPrintF(Report, "Only including things with '%s' in their name/file\n\n", filterName);
    }

    // symbols
    sAppendPrintF(Report, "Functions by size (kilobytes, min %.2f):\n", filters.minFunction/1024.0);
    std::sort(m_Symbols.begin(), m_Symbols.end(), [](const auto& a, const auto& b) {
        if (a.size != b.size)
            return a.size > b.size;
        if (a.objectFileIndex != b.objectFileIndex)
            return a.objectFileIndex < b.objectFileIndex;
        return a.name < b.name;
    });

    for (const auto& sym : m_Symbols)
    {
        if (sym.size < filters.minFunction)
            break;
        if (sym.sectionType == SectionType::Code)
        {
            const char* name1 = sym.name.c_str();
            std::string objFile = GetObjectFileDesc(sym.objectFileIndex);
            if (filterName && !strstr(name1, filterName) && !strstr(objFile.c_str(), filterName))
                continue;
            sAppendPrintF(Report, "%5d.%02d: %-80s %s\n",
                sym.size / 1024, (sym.size % 1024) * 100 / 1024,
                name1, objFile.c_str());
        }
    }

    // templates
    sAppendPrintF(Report, "\nAggregated templates by size (kilobytes, min %.2f / %i):\n", filters.minTemplate/1024.0, filters.minTemplateCount);

    std::sort(m_Templates.begin(), m_Templates.end(), [](const auto& a, const auto& b) {
        if (a.size != b.size)
            return a.size > b.size;
        if (a.count != b.count)
            return a.count > b.count;
        return a.name < b.name;
    });

    for (const auto& tpl : m_Templates)
    {
        if (tpl.size < filters.minTemplate)
            break;
        if (tpl.count < filters.minTemplateCount)
            continue;
        const char* name1 = tpl.name.c_str();
        if (filterName && !strstr(name1, filterName))
            continue;
        sAppendPrintF(Report, "%5d.%02d #%5d: %s\n",
            tpl.size / 1024, (tpl.size % 1024) * 100 / 1024,
            tpl.count,
            name1);
    }

    sAppendPrintF(Report, "\nData by size (kilobytes, min %.2f):\n", filters.minData/1024.0);
    for (const auto& sym : m_Symbols)
    {
        if (sym.size < filters.minData)
            break;
        if (sym.sectionType == SectionType::Data)
        {
            const char* name1 = sym.name.c_str();
            std::string objFile = GetObjectFileDesc(sym.objectFileIndex);
            if (filterName && !strstr(name1, filterName) && !strstr(objFile.c_str(), filterName))
                continue;
            sAppendPrintF(Report, "%5d.%02d: %-50s %s\n",
                sym.size / 1024, (sym.size % 1024) * 100 / 1024,
                name1, objFile.c_str());
        }
    }

    sAppendPrintF(Report, "\nBSS by size (kilobytes, min %.2f):\n", filters.minData/1024.0);
    for (const auto& sym : m_Symbols)
    {
        if (sym.size < filters.minData)
            break;
        if (sym.sectionType == SectionType::BSS)
        {
            const char* name1 = sym.name.c_str();
            std::string objFile = GetObjectFileDesc(sym.objectFileIndex);
            if (filterName && !strstr(name1, filterName) && !strstr(objFile.c_str(), filterName))
                continue;
            sAppendPrintF(Report, "%5d.%02d: %-50s %s\n",
                sym.size / 1024, (sym.size % 1024) * 100 / 1024,
                name1, objFile.c_str());
        }
    }

    sAppendPrintF(Report, "\nClasses/Namespaces by code size (kilobytes, min %.2f):\n", filters.minClass/1024.0);
    std::vector<NamespaceInfo> nameSpaces;
    for (const auto& n : m_Namespaces)
    {
        if (n.codeSize >= filters.minClass)
            nameSpaces.push_back(n);
    }
    std::sort(nameSpaces.begin(), nameSpaces.end(), [](const auto& a, const auto& b)
    {
        if (a.codeSize != b.codeSize)
            return a.codeSize > b.codeSize;
        if (a.dataSize != b.dataSize)
            return a.dataSize > b.dataSize;
        return a.name < b.name;
    });
    for (const auto& n : nameSpaces)
    {
        const std::string& name = n.name;
        if (filterName && !strstr(name.c_str(), filterName))
            continue;
        sAppendPrintF(Report, "%5d.%02d: %s\n",
            n.codeSize / 1024, (n.codeSize % 1024) * 100 / 1024, name.c_str());
    }

    sAppendPrintF(Report, "\nObject files by code size (kilobytes, min %.2f):\n", filters.minFile/1024.0);
    std::vector<ObjectFileInfo> objectFiles;
    for (const auto& f : m_ObjectFiles)
    {
        if (f.codeSize >= filters.minFile || f.contribCodeSize >= filters.minFile)
            objectFiles.push_back(f);
    }
    std::sort(objectFiles.begin(), objectFiles.end(), [](const ObjectFileInfo& a, const ObjectFileInfo& b) {
        if (a.contribCodeSize != b.contribCodeSize)
            return a.contribCodeSize > b.contribCodeSize;
        if (a.codeSize != b.codeSize)
            return a.codeSize > b.codeSize;
        return a.index < b.index;
    });
    for (const auto& f : objectFiles)
    {
        std::string objFile = GetObjectFileDesc(f.index);
        if (filterName && !strstr(objFile.c_str(), filterName))
            continue;

        if (f.codeSize * 1.2f >= f.contribCodeSize)
        {
            sAppendPrintF(Report, "%5d.%02d: %s\n",
                f.contribCodeSize / 1024, (f.contribCodeSize % 1024) * 100 / 1024,
                objFile.c_str());
        }
        else
        {
            sAppendPrintF(Report, "%5d.%02d: %s [%d.%02d with symbols]\n",
                f.contribCodeSize / 1024, (f.contribCodeSize % 1024) * 100 / 1024,
                objFile.c_str(),
                f.codeSize / 1024, (f.codeSize % 1024) * 100 / 1024);
        }
    }

    sAppendPrintF(Report, "\nObject files by data size (kilobytes, min %.2f):\n", filters.minFile / 1024.0);
    objectFiles.clear();
    for (const auto& f : m_ObjectFiles)
    {
        if (f.dataSize >= filters.minFile || f.contribDataSize >= filters.minFile)
            objectFiles.push_back(f);
    }
    std::sort(objectFiles.begin(), objectFiles.end(), [](const ObjectFileInfo& a, const ObjectFileInfo& b) {
        if (a.contribDataSize != b.contribDataSize)
            return a.contribDataSize > b.contribDataSize;
        if (a.dataSize != b.dataSize)
            return a.dataSize > b.dataSize;
        return a.index < b.index;
        });
    for (const auto& f : objectFiles)
    {
        std::string objFile = GetObjectFileDesc(f.index);
        if (filterName && !strstr(objFile.c_str(), filterName))
            continue;

        if (f.dataSize * 1.2f >= f.contribDataSize)
        {
            sAppendPrintF(Report, "%5d.%02d: %s\n",
                f.contribDataSize / 1024, (f.contribDataSize % 1024) * 100 / 1024,
                objFile.c_str());
        }
        else
        {
            sAppendPrintF(Report, "%5d.%02d: %s [%d.%02d with symbols]\n",
                f.contribDataSize / 1024, (f.contribDataSize % 1024) * 100 / 1024,
                objFile.c_str(),
                f.dataSize / 1024, (f.dataSize % 1024) * 100 / 1024);
        }
    }


    uint32_t contribCodeSize = 0, contribDataSize = 0;
    for (const auto& cnt : m_Contribs)
    {
        if (cnt.sectionType == SectionType::Code)
            contribCodeSize += cnt.size;
        if (cnt.sectionType == SectionType::Data)
            contribDataSize += cnt.size;
    }

    uint32_t size;
    size = CountSizeInSection(SectionType::Code);
    sAppendPrintF(Report, "\nOverall code:  %5d.%02d kb (%d.%02d with symbols)\n", contribCodeSize / 1024, (contribCodeSize % 1024) * 100 / 1024, size / 1024, (size % 1024) * 100 / 1024);

    size = CountSizeInSection(SectionType::Data);
    sAppendPrintF(Report, "Overall data:  %5d.%02d kb (%d.%02d with symbols)\n", contribDataSize / 1024, (contribDataSize % 1024) * 100 / 1024, size / 1024, (size % 1024) * 100 / 1024);

    size = CountSizeInSection(SectionType::BSS);
    sAppendPrintF(Report, "Overall BSS:   %5d.%02d kb\n", size / 1024,
        (size % 1024) * 100 / 1024);

    size = CountSizeInSection(SectionType::Unknown);
    if (size > 0)
    {
        sAppendPrintF(Report, "Overall other: %5d.%02d kb\n", size / 1024,
            (size % 1024) * 100 / 1024);
    }

    return Report;
}
