// Executable size report utility.
// Aras Pranckevicius, https://aras-p.info/projSizer.html
// Public domain.

#pragma once

#include <stddef.h>

struct MemoryMappedFile
{
#ifdef _WIN32
	void* file = (void*)~0;
	void* mapping = (void*)~0;
#else
    int file = 0;
#endif
	void* baseAddress = nullptr;
	size_t fileSize = 0;

	explicit MemoryMappedFile(const char* path);
	~MemoryMappedFile();
};
