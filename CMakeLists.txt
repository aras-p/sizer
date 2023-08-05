cmake_minimum_required (VERSION 3.21)
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW) # enable MSVC_RUNTIME_LIBRARY
endif()

project ("sizer")

add_executable (Sizer
	src/debuginfo.cpp
	src/debuginfo.hpp
	src/main.cpp
	src/mmapfile.cpp
	src/mmapfile.h
	src/parg.c
	src/parg.h
	src/pdb_typetable.cpp
	src/pdb_typetable.hpp
	src/pdbfile.cpp
	src/pdbfile.hpp
	src/pe_utils.cpp
	src/pe_utils.hpp
	src/types.hpp

	src/raw_pdb
	src/raw_pdb/Foundation
	src/raw_pdb/Foundation/PDB_ArrayView.h
	src/raw_pdb/Foundation/PDB_Assert.h
	src/raw_pdb/Foundation/PDB_BitOperators.h
	src/raw_pdb/Foundation/PDB_BitUtil.h
	src/raw_pdb/Foundation/PDB_DisableWarningsPop.h
	src/raw_pdb/Foundation/PDB_DisableWarningsPush.h
	src/raw_pdb/Foundation/PDB_Forward.h
	src/raw_pdb/Foundation/PDB_Log.h
	src/raw_pdb/Foundation/PDB_Macros.h
	src/raw_pdb/Foundation/PDB_Memory.h
	src/raw_pdb/Foundation/PDB_Move.h
	src/raw_pdb/Foundation/PDB_Platform.h
	src/raw_pdb/Foundation/PDB_PointerUtil.h
	src/raw_pdb/Foundation/PDB_Warnings.h
	src/raw_pdb/PDB.cpp
	src/raw_pdb/PDB.h
	src/raw_pdb/PDB_CoalescedMSFStream.cpp
	src/raw_pdb/PDB_CoalescedMSFStream.h
	src/raw_pdb/PDB_DBIStream.cpp
	src/raw_pdb/PDB_DBIStream.h
	src/raw_pdb/PDB_DBITypes.cpp
	src/raw_pdb/PDB_DBITypes.h
	src/raw_pdb/PDB_DirectMSFStream.cpp
	src/raw_pdb/PDB_DirectMSFStream.h
	src/raw_pdb/PDB_ErrorCodes.h
	src/raw_pdb/PDB_GlobalSymbolStream.cpp
	src/raw_pdb/PDB_GlobalSymbolStream.h
	src/raw_pdb/PDB_IPIStream.cpp
	src/raw_pdb/PDB_IPIStream.h
	src/raw_pdb/PDB_IPITypes.h
	src/raw_pdb/PDB_ImageSectionStream.cpp
	src/raw_pdb/PDB_ImageSectionStream.h
	src/raw_pdb/PDB_InfoStream.cpp
	src/raw_pdb/PDB_InfoStream.h
	src/raw_pdb/PDB_ModuleInfoStream.cpp
	src/raw_pdb/PDB_ModuleInfoStream.h
	src/raw_pdb/PDB_ModuleLineStream.cpp
	src/raw_pdb/PDB_ModuleLineStream.h
	src/raw_pdb/PDB_ModuleSymbolStream.cpp
	src/raw_pdb/PDB_ModuleSymbolStream.h
	src/raw_pdb/PDB_NamesStream.cpp
	src/raw_pdb/PDB_NamesStream.h
	src/raw_pdb/PDB_PCH.cpp
	src/raw_pdb/PDB_PCH.h
	src/raw_pdb/PDB_PublicSymbolStream.cpp
	src/raw_pdb/PDB_PublicSymbolStream.h
	src/raw_pdb/PDB_RawFile.cpp
	src/raw_pdb/PDB_RawFile.h
	src/raw_pdb/PDB_SectionContributionStream.cpp
	src/raw_pdb/PDB_SectionContributionStream.h
	src/raw_pdb/PDB_SourceFileStream.cpp
	src/raw_pdb/PDB_SourceFileStream.h
	src/raw_pdb/PDB_TPIStream.cpp
	src/raw_pdb/PDB_TPIStream.h
	src/raw_pdb/PDB_TPITypes.h
	src/raw_pdb/PDB_Types.cpp
	src/raw_pdb/PDB_Types.h
	src/raw_pdb/PDB_Util.h	

	CMakeLists.txt
	CMakePresets.json
)
set_property(TARGET Sizer PROPERTY CXX_STANDARD 14)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT CMAKE_CXX_SIMULATE_ID MATCHES "MSVC")
	target_compile_options(Sizer PRIVATE -fdeclspec -fms-extensions)
endif()

# link to static MSVC runtime
set_property(TARGET Sizer PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Enable debug symbols (RelWithDebInfo is not only that; it also turns on
# incremental linking, disables some inlining, etc. etc.)
if (WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(Sizer PRIVATE /Zi)
    target_link_options(Sizer PRIVATE /DEBUG /OPT:ICF /OPT:REF)
endif()


target_compile_definitions(Sizer PRIVATE
	_CRT_SECURE_NO_DEPRECATE
	_CRT_NONSTDC_NO_WARNINGS
	NOMINMAX
)
