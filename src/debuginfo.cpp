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
    for (const auto& sym : Symbols)
    {
        if (sym.sectionType == type)
            size += sym.Size;
    }
    return size;
}

int32_t DebugInfo::MakeStringPtr(const char *s)
{
    return MakeStringStd(std::string(s));
}

int32_t DebugInfo::MakeStringStd(const std::string& str)
{
    IndexByStringMap::iterator it = m_IndexByString.find(str);
    if (it != m_IndexByString.end())
        return it->second;

    int32_t index = int32_t(m_IndexByString.size());
    m_IndexByString.insert(std::make_pair(str, index));
    m_StringByIndex.push_back(str);
    return index;
}

bool virtAddressComp(const DISymbol &a, const DISymbol &b)
{
    return a.VA < b.VA;
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

void DebugInfo::FinishedReading()
{
    // fix strings and aggregate templates
    typedef std::map<std::string, int> StringIntMap;
    StringIntMap templateToIndex;

    for (int32_t i = 0; i < Symbols.size(); i++)
    {
        DISymbol *sym = &Symbols[i];

        std::string templateName = sym->name;
        bool isTemplate = StripTemplateParams(templateName);
        if (isTemplate)
        {
            StringIntMap::iterator it = templateToIndex.find(templateName);
            int index;
            if (it != templateToIndex.end())
            {
                index = it->second;
                Templates[index].size += sym->Size;
                Templates[index].count++;
            }
            else
            {
                index = int(Templates.size());
                templateToIndex.insert(std::make_pair(templateName, index));
                TemplateSymbol tsym;
                tsym.name = templateName;
                tsym.count = 1;
                tsym.size = sym->Size;
                Templates.push_back(tsym);
            }
        }
    }

    // sort symbols by virtual address
    std::sort(Symbols.begin(), Symbols.end(), virtAddressComp);
}

int32_t DebugInfo::GetFile(int32_t fileName)
{
    for (int32_t i = 0; i < m_Files.size(); i++)
        if (m_Files[i].fileName == fileName)
            return i;

    m_Files.push_back(DISymFile());
    DISymFile *file = &m_Files.back();
    file->fileName = fileName;
    file->codeSize = file->dataSize = 0;

    return int32_t(m_Files.size() - 1);
}

int32_t DebugInfo::GetFileByName(const char *objName)
{
    char *p;

    // skip path seperators
    while ((p = (char*)strchr(objName, '\\')))
        objName = p + 1;

    while ((p = (char*)strchr(objName, '/')))
        objName = p + 1;

    return GetFile(MakeStringPtr(objName));
}

int32_t DebugInfo::GetNameSpace(int32_t name)
{
    const auto it = m_NameSpaceIndexByName.find(name);
    if (it != m_NameSpaceIndexByName.end())
        return it->second;

    DISymNameSp namesp;
    namesp.name = name;
    namesp.codeSize = namesp.dataSize = 0;
    NameSps.push_back(namesp);

    int32_t index = int32_t(NameSps.size() - 1);
    m_NameSpaceIndexByName.insert({name, index});
    return index;
}

int32_t DebugInfo::GetNameSpaceByName(const char *name)
{
    const char *pp = name - 2;
    char *p;
    int32_t cname;

    while ((p = (char*)strstr(pp + 2, "::")))
        pp = p;

    while ((p = (char*)strchr(pp + 1, '.')))
        pp = p;

    if (pp != name - 2)
    {
        char buffer[2048];
        strncpy(buffer, name, sizeof(buffer)-1);

        if (pp - name < 2048)
            buffer[pp - name] = 0;

        cname = MakeStringPtr(buffer);
    }
    else
        cname = MakeStringPtr("<global>");

    return GetNameSpace(cname);
}

void DebugInfo::StartAnalyze()
{
    int32_t i;

    for (i = 0; i < m_Files.size(); i++)
    {
        m_Files[i].codeSize = m_Files[i].dataSize = 0;
    }

    for (i = 0; i < NameSps.size(); i++)
    {
        NameSps[i].codeSize = NameSps[i].dataSize = 0;
    }
}

void DebugInfo::FinishAnalyze()
{
    for (const auto& sym : Symbols)
    {
        if (sym.sectionType == SectionType::Code)
        {
            m_Files[sym.objFileNum].codeSize += sym.Size;
            NameSps[sym.NameSpNum].codeSize += sym.Size;
        }
        else if (sym.sectionType == SectionType::Data)
        {
            m_Files[sym.objFileNum].dataSize += sym.Size;
            NameSps[sym.NameSpNum].dataSize += sym.Size;
        }
    }
}

static bool symSizeComp(const DISymbol &a, const DISymbol &b)
{
    if (a.Size != b.Size)
        return a.Size > b.Size;
    return a.VA < b.VA;
}

static bool templateSizeComp(const TemplateSymbol& a, const TemplateSymbol& b)
{
    if (a.size != b.size)
        return a.size > b.size;
    if (a.count != b.count)
        return a.count > b.count;
    return a.name.length() < b.name.length();
}

static bool nameCodeSizeComp(const DISymNameSp &a, const DISymNameSp &b)
{
    if (a.codeSize != b.codeSize)
        return a.codeSize > b.codeSize;
    return a.dataSize > b.dataSize;
}

static bool fileCodeSizeComp(const DISymFile &a, const DISymFile &b)
{
    if (a.codeSize != b.codeSize)
        return a.codeSize > b.codeSize;
    return a.dataSize > b.dataSize;
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
    int32_t i; //,j;
    uint32_t size;
    const char* filterName = filters.name.empty() ? NULL : filters.name.c_str();

    Report.reserve(16384); // start out with 16k space
    if (filterName)
    {
        sAppendPrintF(Report, "Only including things with '%s' in their name/file\n\n", filterName);
    }

    // symbols
    sAppendPrintF(Report, "Functions by size (kilobytes, min %.2f):\n", filters.minFunction/1024.0);
    std::sort(Symbols.begin(), Symbols.end(), symSizeComp);

    for (const auto& sym : Symbols)
    {
        if (sym.Size < filters.minFunction)
            break;
        if (sym.sectionType == SectionType::Code)
        {
            const char* name1 = sym.name.c_str();
            const char* name2 = GetStringPrep(m_Files[sym.objFileNum].fileName);
            if (filterName && !strstr(name1, filterName) && !strstr(name2, filterName))
                continue;
            sAppendPrintF(Report, "%5d.%02d: %-80s %s\n",
                sym.Size / 1024, (sym.Size % 1024) * 100 / 1024,
                name1, name2);
        }
    }

    // templates
    sAppendPrintF(Report, "\nAggregated templates by size (kilobytes, min %.2f / %i):\n", filters.minTemplate/1024.0, filters.minTemplateCount);

    std::sort(Templates.begin(), Templates.end(), templateSizeComp);

    for (i = 0; i < Templates.size(); i++)
    {
        if (Templates[i].size < filters.minTemplate)
            break;
        if (Templates[i].count < filters.minTemplateCount)
            continue;
        const char* name1 = Templates[i].name.c_str();
        if (filterName && !strstr(name1, filterName))
            continue;
        sAppendPrintF(Report, "%5d.%02d #%5d: %s\n",
            Templates[i].size / 1024, (Templates[i].size % 1024) * 100 / 1024,
            Templates[i].count,
            name1);
    }

    sAppendPrintF(Report, "\nData by size (kilobytes, min %.2f):\n", filters.minData/1024.0);
    for (const auto& sym : Symbols)
    {
        if (sym.Size < filters.minData)
            break;
        if (sym.sectionType == SectionType::Data)
        {
            const char* name1 = sym.name.c_str();
            const char* name2 = GetStringPrep(m_Files[sym.objFileNum].fileName);
            if (filterName && !strstr(name1, filterName) && !strstr(name2, filterName))
                continue;
            sAppendPrintF(Report, "%5d.%02d: %-50s %s\n",
                sym.Size / 1024, (sym.Size % 1024) * 100 / 1024,
                name1, name2);
        }
    }

    sAppendPrintF(Report, "\nBSS by size (kilobytes, min %.2f):\n", filters.minData/1024.0);
    for (const auto& sym : Symbols)
    {
        if (sym.Size < filters.minData)
            break;
        if (sym.sectionType == SectionType::BSS)
        {
            const char* name1 = sym.name.c_str();
            const char* name2 = GetStringPrep(m_Files[sym.objFileNum].fileName);
            if (filterName && !strstr(name1, filterName) && !strstr(name2, filterName))
                continue;
            sAppendPrintF(Report, "%5d.%02d: %-50s %s\n",
                sym.Size / 1024, (sym.Size % 1024) * 100 / 1024,
                name1, name2);
        }
    }

    /*
    _snprintf(Report,512,"\nFunctions by object file and size:\n");
    Report += strlen(Report);

    for(i=1;i<Symbols.size();i++)
      for(j=i;j>0;j--)
      {
        int32_t f1 = Symbols[j].FileNum;
        int32_t f2 = Symbols[j-1].FileNum;

        if(f1 == -1 || f2 != -1 && stricmp(Files[f1].Name.String,Files[f2].Name.String) < 0)
          std::swap(Symbols[j],Symbols[j-1]);
      }

    for(i=0;i<Symbols.size();i++)
    {
      if(Symbols[i].Class == DIC_CODE)
      {
        _snprintf(Report,512,"%5d.%02d: %-50s %s\n",
          Symbols[i].Size/1024,(Symbols[i].Size%1024)*100/1024,
          Symbols[i].Name,Files[Symbols[i].FileNum].Name);

        Report += strlen(Report);
      }
    }
    */

    sAppendPrintF(Report, "\nClasses/Namespaces by code size (kilobytes, min %.2f):\n", filters.minClass/1024.0);
    std::sort(NameSps.begin(), NameSps.end(), nameCodeSizeComp);

    for (i = 0; i < NameSps.size(); i++)
    {
        if (NameSps[i].codeSize < filters.minClass)
            break;
        const char* name1 = GetStringPrep(NameSps[i].name);
        if (filterName && !strstr(name1, filterName))
            continue;
        sAppendPrintF(Report, "%5d.%02d: %s\n",
            NameSps[i].codeSize / 1024, (NameSps[i].codeSize % 1024) * 100 / 1024, name1);
    }

    sAppendPrintF(Report, "\nObject files by code size (kilobytes, min %.2f):\n", filters.minFile/1024.0);
    std::sort(m_Files.begin(), m_Files.end(), fileCodeSizeComp);

    for (i = 0; i < m_Files.size(); i++)
    {
        if (m_Files[i].codeSize < filters.minFile)
            break;
        const char* name1 = GetStringPrep(m_Files[i].fileName);
        if (filterName && !strstr(name1, filterName))
            continue;
        sAppendPrintF(Report, "%5d.%02d: %s\n", m_Files[i].codeSize / 1024,
            (m_Files[i].codeSize % 1024) * 100 / 1024, name1);
    }

    size = CountSizeInSection(SectionType::Code);
    sAppendPrintF(Report, "\nOverall code:  %5d.%02d kb\n", size / 1024,
        (size % 1024) * 100 / 1024);

    size = CountSizeInSection(SectionType::Data);
    sAppendPrintF(Report, "Overall data:  %5d.%02d kb\n", size / 1024,
        (size % 1024) * 100 / 1024);

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
