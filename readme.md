# Sizer - Win32/64 executable size report utility

See [**project page**](https://aras-p.info/projSizer.html) for a description.

Changelog [here](changelog.md).

Originally based on code by [Fabian "ryg" Giesen](http://farbrausch.com/~fg/).

Other similar tools:
* [SizeBench](https://devblogs.microsoft.com/performance-diagnostics/sizebench-a-new-tool-for-analyzing-windows-binary-size/) from Microsoft.
* [Bloaty McBloatface](https://github.com/google/bloaty) for ELF (Linux) / Mach-O (Mac) binaries.
* [SymbolSort](https://github.com/adrianstone55/SymbolSort) for Win32/PDB binaries.

License of the tool itself is public domain. Source contains third party code:
- BSD-licensed [MolecularMatters/raw_pdb](https://github.com/MolecularMatters/raw_pdb) PDB library (rev `74b2b97`, 2023 Jul 28).
- CC0-licensed [jibsen/parg](https://github.com/jibsen/parg) command line parser.
