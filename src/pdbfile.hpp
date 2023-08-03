// Executable size report utility.
// Aras Pranckevicius, https://aras-p.info/projSizer.html
// Based on code by Fabian "ryg" Giesen, http://farbrausch.com/~fg/
// Public domain.

#pragma once

class DebugInfo;

bool ReadDebugInfo(const char* fileName, DebugInfo& to);
