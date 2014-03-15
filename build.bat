@echo off
cl /O2 /Zi /nologo /FeSizer.exe src\debuginfo.cpp src\main.cpp src\pdbfile.cpp ole32.lib oleaut32.lib /EHsc 
del *.obj