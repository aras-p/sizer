// Executable size report utility.
// Aras Pranckevicius, https://aras-p.info/projSizer.html
// Public domain.

#pragma once

struct MemoryMappedFile
{
	void* file = (void*)~0;
	void* mapping = (void*)~0;
	void* baseAddress = nullptr;
	size_t fileSize = 0;

	explicit MemoryMappedFile(const char* path);
	~MemoryMappedFile();
};
