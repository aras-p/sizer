// Executable size report utility.
// Aras Pranckevicius, https://aras-p.info/projSizer.html
// Public domain.

#include "mmapfile.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

MemoryMappedFile::MemoryMappedFile(const char* path)
{
#ifdef _WIN32
	void* file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, nullptr);
	if (file == INVALID_HANDLE_VALUE)
		return;

	LARGE_INTEGER fileSize = {};
	GetFileSizeEx(file, &fileSize);

	void* fileMapping = CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
	if (fileMapping == nullptr)
	{
		CloseHandle(file);
		return;
	}

	void* baseAddress = MapViewOfFile(fileMapping, FILE_MAP_READ, 0, 0, 0);
	if (baseAddress == nullptr)
	{
		CloseHandle(fileMapping);
		CloseHandle(file);
		return;
	}

	this->file = file;
	this->mapping = fileMapping;
	this->baseAddress = baseAddress;
	this->fileSize = fileSize.QuadPart;
#else
    int file = open(path, O_RDONLY);
    if (file == -1)
        return;
    struct stat fileSt;
    if (fstat(file, &fileSt) == -1)
    {
        close(file);
        return;
    }
    void* baseAddress = mmap(nullptr, fileSt.st_size, PROT_READ, MAP_PRIVATE, file, 0);
    if (baseAddress == MAP_FAILED)
    {
        close(file);
        return;
    }
    this->file = file;
    this->baseAddress = baseAddress;
    this->fileSize = fileSt.st_size;
#endif
}

MemoryMappedFile::~MemoryMappedFile()
{
#ifdef _WIN32
	if (baseAddress != nullptr)
		UnmapViewOfFile(baseAddress);
	if (mapping != INVALID_HANDLE_VALUE)
		CloseHandle(mapping);
	if (file != INVALID_HANDLE_VALUE)
		CloseHandle(file);
#else
    munmap(baseAddress, fileSize);
    close(file);
#endif
}
