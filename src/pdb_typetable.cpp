// Executable size report utility.
// Aras Pranckevicius, https://aras-p.info/projSizer.html
// Public domain.

#include "pdb_typetable.hpp"

TypeTable::TypeTable(const PDB::TPIStream& tpiStream) PDB_NO_EXCEPT
	: typeIndexBegin(tpiStream.GetFirstTypeIndex()), typeIndexEnd(tpiStream.GetLastTypeIndex()),
	m_recordCount(tpiStream.GetTypeRecordCount())
{
	// Create coalesced stream from TPI stream, so the records can be referenced directly using pointers.
	const PDB::DirectMSFStream& directStream = tpiStream.GetDirectMSFStream();
	m_stream = PDB::CoalescedMSFStream(directStream, directStream.GetSize(), 0);

	// types in the TPI stream are accessed by their index from other streams.
	// however, the index is not stored with types in the TPI stream directly, but has to be built while walking the stream.
	// similarly, because types are variable-length records, there are no direct offsets to access individual types.
	// we therefore walk the TPI stream once, and store pointers to the records for trivial O(1) array lookup by index later.	
	m_records = new const PDB::CodeView::TPI::Record*[m_recordCount];

	// parse the CodeView records
	uint32_t typeIndex = 0u;

	tpiStream.ForEachTypeRecordHeaderAndOffset([this, &typeIndex](const PDB::CodeView::TPI::RecordHeader& header, size_t offset)
		{
			// The header includes the record kind and size, which can be stored along with offset
			// to allow for lazy loading of the types on-demand directly from the TPIStream::GetDirectMSFStream()
			// using DirectMSFStream::ReadAtOffset(...). Thus not needing a CoalescedMSFStream to look up the types.
			(void)header;

			const PDB::CodeView::TPI::Record* record = m_stream.GetDataAtOffset<const PDB::CodeView::TPI::Record>(offset);
			m_records[typeIndex] = record;
			++typeIndex;
		});
}

TypeTable::~TypeTable() PDB_NO_EXCEPT
{
	delete[] m_records;
}

static size_t ExtractLength(const char* ptr)
{
	PDB::CodeView::TPI::TypeRecordKind kind = *(const PDB::CodeView::TPI::TypeRecordKind*)ptr;
	ptr += sizeof(kind);
	if (kind < PDB::CodeView::TPI::TypeRecordKind::LF_NUMERIC)
		return PDB_AS_UNDERLYING(kind);

	switch (kind)
	{
	case PDB::CodeView::TPI::TypeRecordKind::LF_CHAR:
		return *(const uint8_t*)ptr;
	case PDB::CodeView::TPI::TypeRecordKind::LF_SHORT:
		return *(const int16_t*)ptr;
	case PDB::CodeView::TPI::TypeRecordKind::LF_USHORT:
		return *(const uint16_t*)ptr;
	case PDB::CodeView::TPI::TypeRecordKind::LF_LONG:
		return *(const int32_t*)ptr;
	case PDB::CodeView::TPI::TypeRecordKind::LF_ULONG:
		return *(const uint32_t*)ptr;
	case PDB::CodeView::TPI::TypeRecordKind::LF_QUADWORD:
		return *(const int64_t*)ptr;
	case PDB::CodeView::TPI::TypeRecordKind::LF_UQUADWORD:
		return *(const uint64_t*)ptr;
	default:
		fprintf(stderr, "PDB error: bogus type info length type (0x%04x)\n", PDB_AS_UNDERLYING(kind));
	}
	return 0;
}

size_t PDBGetTypeSize(const TypeTable& typeTable, uint32_t typeIndex)
{
	// basic types
	auto typeIndexBegin = typeTable.GetFirstTypeIndex();
	if (typeIndex < typeIndexBegin)
	{
		auto type = static_cast<PDB::CodeView::TPI::TypeIndexKind>(typeIndex);
		switch (type)
		{
		case PDB::CodeView::TPI::TypeIndexKind::T_NOTYPE:
			return 0;
		case PDB::CodeView::TPI::TypeIndexKind::T_HRESULT:
			return 4;
		case PDB::CodeView::TPI::TypeIndexKind::T_32PHRESULT:
			return 4;
		case PDB::CodeView::TPI::TypeIndexKind::T_64PHRESULT:
			return 8;
		case PDB::CodeView::TPI::TypeIndexKind::T_VOID:
			return 0;
		case PDB::CodeView::TPI::TypeIndexKind::T_32PVOID:
			return 4;
		case PDB::CodeView::TPI::TypeIndexKind::T_64PVOID:
			return 8;
		case PDB::CodeView::TPI::TypeIndexKind::T_PVOID:
			return 4;

		case PDB::CodeView::TPI::TypeIndexKind::T_32PBOOL08:
		case PDB::CodeView::TPI::TypeIndexKind::T_32PBOOL16:
		case PDB::CodeView::TPI::TypeIndexKind::T_32PBOOL32:
		case PDB::CodeView::TPI::TypeIndexKind::T_32PBOOL64:
			return 4;
		case PDB::CodeView::TPI::TypeIndexKind::T_64PBOOL08:
		case PDB::CodeView::TPI::TypeIndexKind::T_64PBOOL16:
		case PDB::CodeView::TPI::TypeIndexKind::T_64PBOOL32:
		case PDB::CodeView::TPI::TypeIndexKind::T_64PBOOL64:
			return 8;

		case PDB::CodeView::TPI::TypeIndexKind::T_BOOL08:
			return 1;
		case PDB::CodeView::TPI::TypeIndexKind::T_BOOL16:
			return 2;
		case PDB::CodeView::TPI::TypeIndexKind::T_BOOL32:
			return 4;

		case PDB::CodeView::TPI::TypeIndexKind::T_RCHAR:
		case PDB::CodeView::TPI::TypeIndexKind::T_CHAR:
			return 1;
		case PDB::CodeView::TPI::TypeIndexKind::T_32PRCHAR:
		case PDB::CodeView::TPI::TypeIndexKind::T_32PCHAR:
			return 4;
		case PDB::CodeView::TPI::TypeIndexKind::T_64PRCHAR:
		case PDB::CodeView::TPI::TypeIndexKind::T_64PCHAR:
			return 8;
		case PDB::CodeView::TPI::TypeIndexKind::T_PRCHAR:
		case PDB::CodeView::TPI::TypeIndexKind::T_PCHAR:
			return 4;

		case PDB::CodeView::TPI::TypeIndexKind::T_UCHAR:
			return 1;
		case PDB::CodeView::TPI::TypeIndexKind::T_32PUCHAR:
			return 4;
		case PDB::CodeView::TPI::TypeIndexKind::T_64PUCHAR:
			return 8;
		case PDB::CodeView::TPI::TypeIndexKind::T_PUCHAR:
			return 4;

		case PDB::CodeView::TPI::TypeIndexKind::T_WCHAR:
			return 2;
		case PDB::CodeView::TPI::TypeIndexKind::T_32PWCHAR:
			return 4;
		case PDB::CodeView::TPI::TypeIndexKind::T_64PWCHAR:
			return 8;
		case PDB::CodeView::TPI::TypeIndexKind::T_PWCHAR:
			return 4;

		case PDB::CodeView::TPI::TypeIndexKind::T_CHAR8:
			return 1;
		case PDB::CodeView::TPI::TypeIndexKind::T_PCHAR8:
		case PDB::CodeView::TPI::TypeIndexKind::T_PFCHAR8:
		case PDB::CodeView::TPI::TypeIndexKind::T_PHCHAR8:
		case PDB::CodeView::TPI::TypeIndexKind::T_32PCHAR8:
		case PDB::CodeView::TPI::TypeIndexKind::T_32PFCHAR8:
			return 4;
		case PDB::CodeView::TPI::TypeIndexKind::T_64PCHAR8:
			return 8;

		case PDB::CodeView::TPI::TypeIndexKind::T_CHAR16:
			return 2;
		case PDB::CodeView::TPI::TypeIndexKind::T_PCHAR16:
		case PDB::CodeView::TPI::TypeIndexKind::T_32PCHAR16:
			return 4;
		case PDB::CodeView::TPI::TypeIndexKind::T_64PCHAR16:
			return 8;

		case PDB::CodeView::TPI::TypeIndexKind::T_CHAR32:
			return 4;
		case PDB::CodeView::TPI::TypeIndexKind::T_PCHAR32:
		case PDB::CodeView::TPI::TypeIndexKind::T_32PCHAR32:
			return 4;
		case PDB::CodeView::TPI::TypeIndexKind::T_64PCHAR32:
			return 8;

		case PDB::CodeView::TPI::TypeIndexKind::T_SHORT:
			return 2;
		case PDB::CodeView::TPI::TypeIndexKind::T_32PSHORT:
			return 4;
		case PDB::CodeView::TPI::TypeIndexKind::T_64PSHORT:
			return 8;
		case PDB::CodeView::TPI::TypeIndexKind::T_PSHORT:
			return 4;
		case PDB::CodeView::TPI::TypeIndexKind::T_USHORT:
			return 2;
		case PDB::CodeView::TPI::TypeIndexKind::T_32PUSHORT:
			return 4;
		case PDB::CodeView::TPI::TypeIndexKind::T_64PUSHORT:
			return 8;
		case PDB::CodeView::TPI::TypeIndexKind::T_PUSHORT:
			return 4;
		case PDB::CodeView::TPI::TypeIndexKind::T_LONG:
			return 4;
		case PDB::CodeView::TPI::TypeIndexKind::T_32PLONG:
			return 4;
		case PDB::CodeView::TPI::TypeIndexKind::T_64PLONG:
			return 8;
		case PDB::CodeView::TPI::TypeIndexKind::T_PLONG:
			return 4;
		case PDB::CodeView::TPI::TypeIndexKind::T_ULONG:
			return 4;
		case PDB::CodeView::TPI::TypeIndexKind::T_32PULONG:
			return 4;
		case PDB::CodeView::TPI::TypeIndexKind::T_64PULONG:
			return 8;
		case PDB::CodeView::TPI::TypeIndexKind::T_PULONG:
			return 4;
		case PDB::CodeView::TPI::TypeIndexKind::T_REAL32:
			return 4;
		case PDB::CodeView::TPI::TypeIndexKind::T_32PREAL32:
			return 4;
		case PDB::CodeView::TPI::TypeIndexKind::T_64PREAL32:
			return 8;
		case PDB::CodeView::TPI::TypeIndexKind::T_PREAL32:
			return 4;
		case PDB::CodeView::TPI::TypeIndexKind::T_REAL64:
			return 8;
		case PDB::CodeView::TPI::TypeIndexKind::T_32PREAL64:
			return 4;
		case PDB::CodeView::TPI::TypeIndexKind::T_64PREAL64:
			return 8;
		case PDB::CodeView::TPI::TypeIndexKind::T_PREAL64:
			return 4;
		case PDB::CodeView::TPI::TypeIndexKind::T_QUAD:
			return 8;
		case PDB::CodeView::TPI::TypeIndexKind::T_32PQUAD:
			return 4;
		case PDB::CodeView::TPI::TypeIndexKind::T_64PQUAD:
			return 8;
		case PDB::CodeView::TPI::TypeIndexKind::T_PQUAD:
			return 4;
		case PDB::CodeView::TPI::TypeIndexKind::T_UQUAD:
			return 8;
		case PDB::CodeView::TPI::TypeIndexKind::T_32PUQUAD:
			return 4;
		case PDB::CodeView::TPI::TypeIndexKind::T_64PUQUAD:
			return 8;
		case PDB::CodeView::TPI::TypeIndexKind::T_PUQUAD:
			return 4;
		case PDB::CodeView::TPI::TypeIndexKind::T_INT4:
			return 4;
		case PDB::CodeView::TPI::TypeIndexKind::T_32PINT4:
			return 4;
		case PDB::CodeView::TPI::TypeIndexKind::T_64PINT4:
			return 8;
		case PDB::CodeView::TPI::TypeIndexKind::T_PINT4:
			return 4;
		case PDB::CodeView::TPI::TypeIndexKind::T_UINT4:
			return 4;
		case PDB::CodeView::TPI::TypeIndexKind::T_32PUINT4:
			return 4;
		case PDB::CodeView::TPI::TypeIndexKind::T_64PUINT4:
			return 8;
		case PDB::CodeView::TPI::TypeIndexKind::T_PUINT4:
			return 4;

		case PDB::CodeView::TPI::TypeIndexKind::T_UINT8:
			return 1;
		case PDB::CodeView::TPI::TypeIndexKind::T_PUINT8:
		case PDB::CodeView::TPI::TypeIndexKind::T_PFUINT8:
		case PDB::CodeView::TPI::TypeIndexKind::T_PHUINT8:
		case PDB::CodeView::TPI::TypeIndexKind::T_32PUINT8:
		case PDB::CodeView::TPI::TypeIndexKind::T_32PFUINT8:
			return 4;
		case PDB::CodeView::TPI::TypeIndexKind::T_64PUINT8:
			return 8;

		case PDB::CodeView::TPI::TypeIndexKind::T_INT8:
			return 1;
		case PDB::CodeView::TPI::TypeIndexKind::T_PINT8:
		case PDB::CodeView::TPI::TypeIndexKind::T_PFINT8:
		case PDB::CodeView::TPI::TypeIndexKind::T_PHINT8:
		case PDB::CodeView::TPI::TypeIndexKind::T_32PINT8:
		case PDB::CodeView::TPI::TypeIndexKind::T_32PFINT8:
			return 4;
		case PDB::CodeView::TPI::TypeIndexKind::T_64PINT8:
			return 8;

		default:
			PDB_ASSERT(false, "Unhandled special type %u", typeIndex);
			return 0;
		}
	}

	// more complex types
	auto typeRecord = typeTable.GetTypeRecord(typeIndex);
	if (!typeRecord)
		return 0;

	switch (typeRecord->header.kind)
	{
	case PDB::CodeView::TPI::TypeRecordKind::LF_MODIFIER:
		return PDBGetTypeSize(typeTable, typeRecord->data.LF_MODIFIER.type);
	case PDB::CodeView::TPI::TypeRecordKind::LF_POINTER:
		if (typeRecord->data.LF_POINTER.attr.ptrtype == 0x0c) // CV_PTR_64
			return 8;
		return 4;
	case PDB::CodeView::TPI::TypeRecordKind::LF_PROCEDURE:
		return 0;
	case PDB::CodeView::TPI::TypeRecordKind::LF_BITFIELD:
		return PDBGetTypeSize(typeTable, typeRecord->data.LF_BITFIELD.type);
	case PDB::CodeView::TPI::TypeRecordKind::LF_ARRAY:
		return ExtractLength((const char*)typeRecord->data.LF_ARRAY.data);
	case PDB::CodeView::TPI::TypeRecordKind::LF_CLASS:
	case PDB::CodeView::TPI::TypeRecordKind::LF_STRUCTURE:
		return ExtractLength((const char*)typeRecord->data.LF_CLASS.data);
	case  PDB::CodeView::TPI::TypeRecordKind::LF_UNION:
		return ExtractLength((const char*)typeRecord->data.LF_UNION.data);
	case PDB::CodeView::TPI::TypeRecordKind::LF_ENUM:
		return PDBGetTypeSize(typeTable, typeRecord->data.LF_ENUM.utype);
	default:
		break;
	}
	return 0;
}
