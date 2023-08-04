// Executable size report utility.
// Aras Pranckevicius, https://aras-p.info/projSizer.html
// Public domain.

#include "mmapfile.h"
#include <windows.h>

MemoryMappedFile::MemoryMappedFile(const char* path)
{
	void* file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, nullptr);
	if (file == INVALID_HANDLE_VALUE)
	{
		return;
	}

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
}

MemoryMappedFile::~MemoryMappedFile()
{
	if (baseAddress != nullptr)
		UnmapViewOfFile(baseAddress);
	if (mapping != INVALID_HANDLE_VALUE)
		CloseHandle(mapping);
	if (file != INVALID_HANDLE_VALUE)
		CloseHandle(file);
}
