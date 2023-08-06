### 0.6.0, 2023 Aug 6

- When multiple object files have the same filename, disambiguate them (output their folder name in that case too).
- Object file sizes are counted better, in the case where some of their contributions are without symbols.
- Sizer output now also lists object files by data size.
- Slightly improve performance.

### 0.5.1, 2023 Aug 5

- Sizer now builds on Windows, Mac and Linux. Note that it still only analyzes the *Windows* executables/PDBs, but can do that while running on Mac/Linux.
- The build system was changed to CMake.
- Github Actions was added, which also builds the Sizer executable across all of Windows/Mac/Linux platforms.

### 0.5.0, 2023 Aug 4

Large speedup and removed dependency on `msdia*.dll`, by switching to [MolecularMatters/raw_pdb](https://github.com/MolecularMatters/raw_pdb)
library for PDB file parsing. Sizer no longer uses MS DIA SDK, so maybe someday it will be able to run on Mac or Linux too!

The speedup example of analyzing current Blender build: time goes from 9.6sec down to 2.1sec (only some of that is raw_pdb library
being faster, other speedups are within Sizer itself).

Overall sensible things that were reported before still seem to be sensible, but some "strange" things have changed. For example, previous versions
used to report a massive `.text$mn` function attributed to one object file; that no longer happens. Previous versions used to *not* report
some functions coming from linked static libraries, depending on how these were built.

- Sizer no longer uses MS DIA SDK, and thus no longer needs `msdia_*.dll` being present anywhere.
- Generally much faster now (4x-20x faster in my tests).
- Command line accepts either executable/dll (as before), or directly path to PDB file.
- Change: some "strange" functions (e.g. massive `.text$mn`) are no longer reported.
- Fix: previously some symbols from linked static libraries were not included, depending on how the libraries are built and how their debug symbols are being generated.
- Emits warning when analyzing a partially stripped (`/PDBSTRIPPED`) debug information.

### 0.4.0, 2023 Aug 2

- Sizer itself is now 64 bit executable (aras)
- Fixed template report in some cases stopping too early (sagamusix)
- Convert project files to VS2022 (aras)

### 0.3.0, 2017 Nov 8

Previously, minimum symbol sizes were hardcoded (only symbols larger than 512 bytes printed, etc.).
Now they are configurable via command line options! See `--help` for full list of arguments.

- Control report symbol sizes, e.g. `--all`, `--min=0.1` etc.
- Optionally filter report to only include symbols with given substring in name/file: `--name=str`
- Tweaked output to be more readable in general (column widths, etc.)	

### 0.2.0, 2017 06 27

- Massively speed up execution time for large & 64 bit binaries, by changing the symbol processing loop (Lionel Fuentes)
- Improved progress reporting; percentage printed now (aras)

### 0.1.7, 2017 02 13

- Add support for VS2013 and VS2015 DIA SDKs (Brian Smith)
- Undecorate C++ names in reports (db4)

### 0.1.6, 2014 03 15

- Add support for VS2010 and VS2012 DIA SDKs (ryg)

### 0.1.5, 2013 04 23

- Converted project files to VS2010 (aras)

### 0.1.4, 2010 10 14

- Convert project files to vs2008 (lucas)
- Add support for reading vs2008 generated pdb's (lucas)

### 0.1.3, 2008 01 17

- Fixed a crash on some executables; `IDiaSymbol2::get_type` may return `S_ERROR` (aras; reported by Ivan-Assen Ivanov)
- Print a dot for each 1000 symbols read. Some executables spend ages inside DIA dlls
	
### 0.1.2, 2008 01 14

- Added support for VC8.0 DIA DLL (ryg)
- Added support for loading DIA DLLs that are not registered; drop `msdia*.dll` into app folder (ryg)
- Split up "data" report into data and BSS (uninitialized data) sections (ryg)
- Fixed some size computations (ryg)
- Strip whitespace from symbol names; often happens with templates (aras)

### 0.1.1, 2008 01 13

- Improved error messages (aras)
- Removed unused source files (aras)

### 0.1, 2008 01 13

- Initial release
