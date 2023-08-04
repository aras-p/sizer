// Executable size report utility.
// Aras Pranckevicius, https://aras-p.info/projSizer.html
// Public domain.

#pragma once

#include <string>

std::string PEGetPDBPath(const void* data, size_t size);
