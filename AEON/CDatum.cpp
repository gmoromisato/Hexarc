//	CDatum.cpp
//
//	CDatum class
//	Copyright (c) 2010 by George Moromisato. All Rights Reserved.

#include "stdafx.h"

DECLARE_CONST_STRING(TYPENAME_DOUBLE,					"double")
DECLARE_CONST_STRING(TYPENAME_INT32,					"integer32")
DECLARE_CONST_STRING(TYPENAME_NIL,						"nil")
DECLARE_CONST_STRING(TYPENAME_STRING,					"string")
DECLARE_CONST_STRING(TYPENAME_TRUE,						"true")

DECLARE_CONST_STRING(STR_NIL,							"nil")
DECLARE_CONST_STRING(STR_TRUE,							"true")

DECLARE_CONST_STRING(ERR_UNKNOWN_FORMAT,				"Unable to determine file format: %s.")
DECLARE_CONST_STRING(ERR_CANT_OPEN_FILE,				"Unable to open file: %s.")
DECLARE_CONST_STRING(ERR_DESERIALIZE_ERROR,				"Unable to parse file: %s.")

static TAllocatorGC<DWORD> g_IntAlloc;
static TAllocatorGC<double> g_DoubleAlloc;
static CGCStringAllocator g_StringAlloc;
static CGCComplexAllocator g_ComplexAlloc;
static CAEONFactoryList *g_pFactories = NULL;
static int g_iUnalignedLiteralCount = 0;
static TArray<MARKPROC> g_MarkList;

CDatum::CDatum (Types iType)

//	CDatum constructor

	{
	switch (iType)
		{
		case typeNil:
			m_dwData = 0;
			break;

		case typeTrue:
			m_dwData = constTrue;
			break;

		case typeInteger32:
			*this = CDatum((int)0);
			break;

		case typeString:
			*this = CDatum(NULL_STR);
			break;

		case typeArray:
			*this = CDatum(new CComplexArray);
			break;

		case typeDouble:
			*this = CDatum((double)0.0);
			break;

		case typeStruct:
			*this = CDatum(new CComplexStruct);
			break;

		case typeDateTime:
			*this = CDatum(CDateTime(CDateTime::Now));
			break;

		case typeIntegerIP:
			*this = CDatum(CIPInteger(0));
			break;

		case typeInteger64:
			*this = CDatum((DWORDLONG)0);
			break;

		default:
			ASSERT(false);
			m_dwData = 0;
			break;
		}
	}

CDatum::CDatum (int iValue)

//	CDatum constructor

	{
	DWORD dwValue = (DWORD)iValue;

	//	If we fit in 28-bit range, then store in m_dwData

	if (dwValue >= AEON_MIN_28BIT || dwValue <= AEON_MAX_28BIT)
		m_dwData = ((dwValue << 4) | AEON_NUMBER_28BIT);

	//	Otherwise, we need to store the number elsewhere

	else
		{
		DWORD dwID = g_IntAlloc.New(dwValue);
		m_dwData = (dwID << 4) | AEON_NUMBER_32BIT;
		}
	}

CDatum::CDatum (DWORD dwValue)

//	CDatum constructor

	{
	//	If we fit in 28-bit range, then store in m_dwData

	if (dwValue >= AEON_MIN_28BIT || dwValue <= AEON_MAX_28BIT)
		m_dwData = ((dwValue << 4) | AEON_NUMBER_28BIT);

	//	Otherwise, we need to store the number elsewhere

	else
		{
		DWORD dwID = g_IntAlloc.New(dwValue);
		m_dwData = (dwID << 4) | AEON_NUMBER_32BIT;
		}
	}

CDatum::CDatum (DWORDLONG ilValue)

//	CDatum constructor

	{
	//	Store as IP integer

	CComplexInteger *pIPInt = new CComplexInteger(ilValue);

	//	Take ownership of the complex type

	g_ComplexAlloc.New(pIPInt);

	//	Store the pointer and assign type

	m_dwData = ((DWORD_PTR)pIPInt | AEON_TYPE_COMPLEX);
	}

CDatum::CDatum (double rValue)

//	CDatum constructor

	{
	DWORD dwID = g_DoubleAlloc.New(rValue);
	m_dwData = (dwID << 4) | AEON_NUMBER_DOUBLE;
	}

CDatum::CDatum (const CString &sString)

//	CDatum constructor

	{
	//	If this is a literal string, then we can just
	//	take the value, because it doesn't need to be freed.
	//	NOTE: We can only do this if the literal pointer is
	//	DWORD aligned.

	if (sString.IsLiteral())
		{
		if (sString.IsEmpty())
			{
			m_dwData = 0;
			return;
			}
		else
			{
			m_dwData = (DWORD_PTR)(LPSTR)sString;
			if ((m_dwData & AEON_TYPE_MASK) == AEON_TYPE_STRING)
				return;

#ifdef DEBUG
			else
				g_iUnalignedLiteralCount++;
#endif
			}
		}

	//	Get a copy of the naked LPSTR out of the string

	CString sNewString(sString);
	LPSTR pString = sNewString.Handoff();

	//	Track it with our allocator (but only if not NULL).
	//	(If pString is NULL then this is represented as Nil).

	if (pString)
		g_StringAlloc.New(pString);

	//	Store the pointer

	m_dwData = (DWORD_PTR)pString;

	//	No need to mark the type (because TYPE_STRING is 0)

	ASSERT(AEON_TYPE_STRING == 0x00);
	}

CDatum::CDatum (IComplexDatum *pValue)

//	CDatum constructor

	{
	ASSERT(pValue);

	//	Take ownership of the complex type

	g_ComplexAlloc.New(pValue);

	//	Store the pointer and assign type

	m_dwData = ((DWORD_PTR)pValue | AEON_TYPE_COMPLEX);
	}

CDatum::CDatum (const CDateTime &DateTime)

//	CDatum constructor

	{
	CComplexDateTime *pDateTime = new CComplexDateTime(DateTime);

	//	Take ownership of the complex type

	g_ComplexAlloc.New(pDateTime);

	//	Store the pointer and assign type

	m_dwData = ((DWORD_PTR)pDateTime | AEON_TYPE_COMPLEX);
	}

CDatum::CDatum (const CIPInteger &Value)

//	CDatum constructor

	{
	CComplexInteger *pValue = new CComplexInteger(Value);

	//	Take ownership of the complex type

	g_ComplexAlloc.New(pValue);

	//	Store the pointer and assign type

	m_dwData = ((DWORD_PTR)pValue | AEON_TYPE_COMPLEX);
	}

CDatum::operator int () const

//	int cast operator

	{
	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_STRING:
			return (m_dwData == 0 ? 0 : strToInt(raw_GetString()));

		case AEON_TYPE_NUMBER:
			switch (m_dwData & AEON_NUMBER_TYPE_MASK)
				{
				case AEON_NUMBER_CONSTANT:
					{
					switch (m_dwData)
						{
						case constTrue:
							return 1;

						default:
							ASSERT(false);
							return 0;
						}
					}

				case AEON_NUMBER_28BIT:
					return ((int)(m_dwData & AEON_NUMBER_MASK) >> 4);

				case AEON_NUMBER_32BIT:
					return (int)g_IntAlloc.Get(GetNumberIndex());

				case AEON_NUMBER_DOUBLE:
					return (int)g_DoubleAlloc.Get(GetNumberIndex());

				default:
					ASSERT(false);
					return 0;
				}

		case AEON_TYPE_COMPLEX:
			return raw_GetComplex()->CastInteger32();

		default:
			ASSERT(false);
			return 0;
		}
	}

CDatum::operator DWORD () const

//	DWORD cast operator

	{
	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_STRING:
			return (m_dwData == 0 ? 0 : (DWORD)strToInt(raw_GetString()));

		case AEON_TYPE_NUMBER:
			switch (m_dwData & AEON_NUMBER_TYPE_MASK)
				{
				case AEON_NUMBER_CONSTANT:
					{
					switch (m_dwData)
						{
						case constTrue:
							return 1;

						default:
							ASSERT(false);
							return 0;
						}
					}

				case AEON_NUMBER_28BIT:
					return ((int)(m_dwData & AEON_NUMBER_MASK) >> 4);

				case AEON_NUMBER_32BIT:
					return (int)g_IntAlloc.Get(GetNumberIndex());

				case AEON_NUMBER_DOUBLE:
					return (int)g_DoubleAlloc.Get(GetNumberIndex());

				default:
					ASSERT(false);
					return 0;
				}

		case AEON_TYPE_COMPLEX:
			return (DWORD)raw_GetComplex()->CastInteger32();

		default:
			ASSERT(false);
			return 0;
		}
	}

CDatum::operator DWORDLONG () const

//	DWORDLONG operator

	{
	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_STRING:
			return 0;	//	LATER: See if we can convert

		case AEON_TYPE_NUMBER:
			switch (m_dwData & AEON_NUMBER_TYPE_MASK)
				{
				case AEON_NUMBER_CONSTANT:
					{
					switch (m_dwData)
						{
						case constTrue:
							return 1;

						default:
							ASSERT(false);
							return 0;
						}
					}

				case AEON_NUMBER_28BIT:
					return ((DWORDLONG)(m_dwData & AEON_NUMBER_MASK) >> 4);

				case AEON_NUMBER_32BIT:
					return (DWORDLONG)g_IntAlloc.Get(GetNumberIndex());

				case AEON_NUMBER_DOUBLE:
					return (DWORDLONG)g_DoubleAlloc.Get(GetNumberIndex());

				default:
					ASSERT(false);
					return 0;
				}

		case AEON_TYPE_COMPLEX:
			return raw_GetComplex()->CastDWORDLONG();

		default:
			ASSERT(false);
			return 0;
		}
	}

CDatum::operator double () const

//	double cast operator

	{
	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_STRING:
			return (m_dwData == 0 ? 0.0 : strToDouble(raw_GetString()));

		case AEON_TYPE_NUMBER:
			switch (m_dwData & AEON_NUMBER_TYPE_MASK)
				{
				case AEON_NUMBER_CONSTANT:
					{
					switch (m_dwData)
						{
						case constTrue:
							return 1.0;

						default:
							ASSERT(false);
							return 0.0;
						}
					}

				case AEON_NUMBER_28BIT:
					return (double)((int)(m_dwData & AEON_NUMBER_MASK) >> 4);

				case AEON_NUMBER_32BIT:
					return (double)(int)g_IntAlloc.Get(GetNumberIndex());

				case AEON_NUMBER_DOUBLE:
					return g_DoubleAlloc.Get(GetNumberIndex());

				default:
					ASSERT(false);
					return 0.0;
				}

		case AEON_TYPE_COMPLEX:
			return 0.0;

		default:
			ASSERT(false);
			return 0.0;
		}
	}

CDatum::operator const CDateTime & () const

//	CDateTime cast operator

	{
	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_COMPLEX:
			return raw_GetComplex()->CastCDateTime();

		default:
			return NULL_DATETIME;
		}
	}

CDatum::operator const CIPInteger & () const

//	CIPInteger cast operator
//
//	NOTE: The difference between this and AsIPInteger() is that
//	AsIPInteger returns an object by value, which allow for
//	more conversion types. This returns a CIPInteger by reference.

	{
	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_COMPLEX:
			return raw_GetComplex()->CastCIPInteger();

		default:
			return NULL_IPINTEGER;
		}
	}

CDatum::operator const CString & () const

//	CString cast operator
//
//	NOTE: The difference between this and AsString() is that
//	AsString returns a string by value, which allow for
//	more conversion types. This returns a string by reference.

	{
	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_STRING:
			return (m_dwData == 0 ? NULL_STR : raw_GetString());

		case AEON_TYPE_NUMBER:
			switch (m_dwData & AEON_NUMBER_TYPE_MASK)
				{
				case AEON_NUMBER_CONSTANT:
					{
					switch (m_dwData)
						{
						case constTrue:
							return STR_TRUE;

						default:
							ASSERT(false);
							return NULL_STR;
						}
					}

				case AEON_NUMBER_28BIT:
				case AEON_NUMBER_32BIT:
				case AEON_NUMBER_DOUBLE:
					return NULL_STR;

				default:
					ASSERT(false);
					return NULL_STR;
				}

		case AEON_TYPE_COMPLEX:
			return raw_GetComplex()->CastCString();

		default:
			ASSERT(false);
			return NULL_STR;
		}
	}

void CDatum::Append (CDatum dValue)

//	Append
//
//	Appends the value to the datum. Note that this modifies the datum in place.
//	Do not use this unless you are sure about who else might have a pointer
//	to the datum.

	{
	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_COMPLEX:
			return raw_GetComplex()->Append(dValue);

		default:
			//	Nothing happens
			;
		}
	}

void CDatum::AsAttributeList (CAttributeList *retAttribs) const

//	AsAttributeList
//
//	Generates an attribute list

	{
	int i;

	retAttribs->DeleteAll();
	for (i = 0; i < GetCount(); i++)
		retAttribs->Insert(GetElement(i));
	}

CDateTime CDatum::AsDateTime (void) const

//	AsDateTime
//
//	Coerces to a CDateTime

	{
	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_STRING:
			{
			if (m_dwData == 0)
				return NULL_DATETIME;

			CDateTime DateTime;
			if (!CComplexDateTime::CreateFromString(raw_GetString(), &DateTime))
				return NULL_DATETIME;

			return DateTime;
			}

		case AEON_TYPE_COMPLEX:
			return raw_GetComplex()->CastCDateTime();

		default:
			return NULL_DATETIME;
		}
	}

CString CDatum::AsString (void) const

//	AsString
//
//	Coerces to a CString

	{
	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_STRING:
			return (m_dwData == 0 ? NULL_STR : raw_GetString());

		case AEON_TYPE_NUMBER:
			switch (m_dwData & AEON_NUMBER_TYPE_MASK)
				{
				case AEON_NUMBER_CONSTANT:
					{
					switch (m_dwData)
						{
						case constTrue:
							return STR_TRUE;

						default:
							ASSERT(false);
							return NULL_STR;
						}
					}

				case AEON_NUMBER_28BIT:
				case AEON_NUMBER_32BIT:
					return strFromInt((int)*this);

				case AEON_NUMBER_DOUBLE:
					return strFromDouble(g_DoubleAlloc.Get(GetNumberIndex()));

				default:
					ASSERT(false);
					return NULL_STR;
				}

		case AEON_TYPE_COMPLEX:
			return raw_GetComplex()->AsString();

		default:
			ASSERT(false);
			return NULL_STR;
		}
	}

TArray<CString> CDatum::AsStringArray (void) const

//	AsStringArray
//
//	Coerces to an array of strings.

	{
	int i;

	TArray<CString> Result;
	Result.InsertEmpty(GetCount());
	for (i = 0; i < GetCount(); i++)
		Result[i] = GetElement(i).AsString();

	return Result;
	}

bool CDatum::CanInvoke (void) const

//	CanInvoke
//
//	Returns TRUE if we can invoke it.

	{
	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_COMPLEX:
			return raw_GetComplex()->CanInvoke();

		default:
			return false;
		}
	}

CDatum CDatum::Clone (void) const

//	Clone
//
//	Returns a (shallow) clone of our datum.

	{
	switch (m_dwData & AEON_TYPE_MASK)
		{
		//	Complex types are the only ones with pointer to other datums

		case AEON_TYPE_COMPLEX:
			return CDatum(raw_GetComplex()->Clone());

		default:
			return *this;
		}
	}

bool CDatum::CreateBinary (IByteStream &Stream, int iSize, CDatum *retDatum)

//	CreateBinary
//
//	Creates a string datum containing binary data from the stream.
//	If iSize is -1 then we read as much as the stream has.
//	Otherwise  we read up to iSize.

	{
	//	LATER: Handle streams more than 2GB. Instead of asking how much space
	//	is left, maybe we should just ask to truncate the size that we're
	//	requesting.
	int iDataRemaining = Stream.GetStreamLength() - Stream.GetPos();
	int iBinarySize = (iSize < 0 ? iDataRemaining : Min(iDataRemaining, iSize));

	//	0-size

	if (iBinarySize == 0)
		{
		*retDatum = CDatum();
		return true;
		}

	//	Read the stream

	CComplexBinary *pBinary;
	try
		{
		pBinary = new CComplexBinary(Stream, iBinarySize);
		}
	catch (...)
		{
		return false;
		}

	//	Done

	*retDatum = CDatum(pBinary);

	//	Done

	return true;
	}

bool CDatum::CreateBinaryFromHandoff (CStringBuffer &Buffer, CDatum *retDatum)

//	CreateBinaryFromHandoff
//
//	Creates a binary datum

	{
	CComplexBinary *pBinary = new CComplexBinary;
	pBinary->TakeHandoff(Buffer);
	*retDatum = CDatum(pBinary);
	return true;
	}

bool CDatum::CreateFromAttributeList (const CAttributeList &Attribs, CDatum *retdDatum)

//	CreateFromAttributeList
//
//	Creates a datum from an attribute list

	{
	int i;

	TArray<CString> AllAttribs;
	Attribs.GetAll(&AllAttribs);

	if (AllAttribs.GetCount() == 0)
		{
		*retdDatum = CDatum();
		return true;
		}

	CComplexArray *pArray = new CComplexArray;
	for (i = 0; i < AllAttribs.GetCount(); i++)
		pArray->Insert(AllAttribs[i]);

	*retdDatum = CDatum(pArray);
	return true;
	}

bool CDatum::CreateFromFile (const CString &sFilespec, ESerializationFormats iFormat, CDatum *retdDatum, CString *retsError)

//	CreateFromFile
//
//	Loads a datum from a file.

	{
	//	Open the file

	CFileBuffer theFile;
	if (!theFile.OpenReadOnly(sFilespec))
		{
		*retsError = strPattern(ERR_CANT_OPEN_FILE, sFilespec);
		return false;
		}

	//	If unknown format, see if we can detect the format

	if (iFormat == formatUnknown)
		{
		if (!DetectFileFormat(sFilespec, theFile, &iFormat, retsError))
			return false;

		//	If format unknown, then error.

		if (iFormat == formatUnknown)
			{
			*retsError = strPattern(ERR_UNKNOWN_FORMAT, sFilespec);
			return false;
			}
		}

	//	Parse it

	if (!Deserialize(iFormat, theFile, retdDatum))
		{
		*retsError = strPattern(ERR_DESERIALIZE_ERROR, sFilespec);
		return false;
		}

	//	Done

	return true;
	}

bool CDatum::CreateFromStringValue (const CString &sValue, CDatum *retdDatum)

//	CreateFromStringValue
//
//	Creates either a string or a number depending on the value.

	{
	CDatum dDatum;

	switch (GetStringValueType(sValue))
		{
		case typeNil:
			break;

		case typeInteger32:
			dDatum = CDatum(strToInt(sValue, 0));
			break;

		case typeIntegerIP:
			{
			CIPInteger Value;
			Value.InitFromString(sValue);
			CDatum::CreateIPIntegerFromHandoff(Value, &dDatum);
			break;
			}

		case typeDouble:
			dDatum = CDatum(strToDouble(sValue));
			break;

		case typeString:
			dDatum = CDatum(sValue);
			break;

		default:
			return false;
		}

	if (retdDatum)
		*retdDatum = dDatum;

	return true;
	}

bool CDatum::CreateIPInteger (const CIPInteger &Value, CDatum *retdDatum)

//	CreateIPInteger
//
//	Creates an IPInteger datum

	{
	CComplexInteger *pIPInt = new CComplexInteger(Value);
	*retdDatum = CDatum(pIPInt);
	return true;
	}

bool CDatum::CreateIPIntegerFromHandoff (CIPInteger &Value, CDatum *retdDatum)

//	CreateIPInteger
//
//	Creates an IPInteger by taking a handoff

	{
	CComplexInteger *pIPInt = new CComplexInteger;
	pIPInt->TakeHandoff(Value);

	*retdDatum = CDatum(pIPInt);
	return true;
	}

bool CDatum::CreateStringFromHandoff (CString &sString, CDatum *retDatum)

//	CreateStringFromHandoff
//
//	Creates a string by taking a handoff from a string

	{
	if (sString.IsLiteral())
		{
		if (sString.IsEmpty())
			retDatum->m_dwData = 0;
		else
			retDatum->m_dwData = (DWORD_PTR)(LPSTR)sString;
		}
	else
		{
		//	Take ownership of the data

		LPSTR pString = sString.Handoff();

		//	Track it with our allocator (but only if not NULL).
		//	(If pString is NULL then this is represented as Nil).

		if (pString)
			g_StringAlloc.New(pString);

		//	Store the pointer

		retDatum->m_dwData = (DWORD_PTR)pString;
		}

	//	Done

	return true;
	}

bool CDatum::CreateStringFromHandoff (CStringBuffer &String, CDatum *retDatum)

//	CreateStringFromHandoff
//
//	Creates a string by taking a handoff from a string buffer

	{
	//	Take ownership of the data

	LPSTR pString = String.Handoff();

	//	Track it with our allocator (but only if not NULL).
	//	(If pString is NULL then this is represented as Nil).

	if (pString)
		g_StringAlloc.New(pString);

	//	Store the pointer

	retDatum->m_dwData = (DWORD_PTR)pString;

	//	Done

	return true;
	}

int CDatum::DefaultCompare (void *pCtx, const CDatum &dKey1, const CDatum &dKey2)

//	DefaultCompare
//
//	Default comparison routine used for sorting. Returns:
//
//	-1:		If dKey1 < dKey2
//	0:		If dKey1 == dKey2
//	1:		If dKey1 > dKey2
//
//	NOTES:
//
//	Nil == ""
//	Nil == {}
//	Nil == ()
//	"abc" != "ABC"

	{
	int i;

	//	If both are the same datatype, then compare

	CDatum::Types iType1 = dKey1.GetBasicType();
	CDatum::Types iType2 = dKey2.GetBasicType();

	//	If both types are equal, then compare

	if (iType1 == iType2)
		{
		switch (iType1)
			{
			case CDatum::typeNil:
			case CDatum::typeTrue:
				return 0;

			case CDatum::typeInteger32:
				if ((int)dKey1 > (int)dKey2)
					return 1;
				else if ((int)dKey1 < (int)dKey2)
					return -1;
				else
					return 0;

			case CDatum::typeInteger64:
				if ((DWORDLONG)dKey1 > (DWORDLONG)dKey2)
					return 1;
				else if ((DWORDLONG)dKey1 < (DWORDLONG)dKey2)
					return -1;
				else
					return 0;

			case CDatum::typeDouble:
				if ((double)dKey1 > (double)dKey2)
					return 1;
				else if ((double)dKey1 < (double)dKey2)
					return -1;
				else
					return 0;

			case CDatum::typeIntegerIP:
				return KeyCompare((const CIPInteger &)dKey1, (const CIPInteger &)dKey2);

			case CDatum::typeString:
				return KeyCompare((const CString &)dKey1, (const CString &)dKey2);

			case CDatum::typeDateTime:
				return ((const CDateTime &)dKey1).Compare((const CDateTime &)dKey2);

			case CDatum::typeArray:
				if (dKey1.GetCount() > dKey2.GetCount())
					return 1;
				else if (dKey1.GetCount() < dKey2.GetCount())
					return -1;
				else
					{
					for (i = 0; i < dKey1.GetCount(); i++)
						{
						CDatum dItem1 = dKey1.GetElement(i);
						CDatum dItem2 = dKey2.GetElement(i);
						int iItemCompare = CDatum::DefaultCompare(pCtx, dItem1, dItem2);
						if (iItemCompare != 0)
							return iItemCompare;
						}

					return 0;
					}

			case CDatum::typeStruct:
				if (dKey1.GetCount() > dKey2.GetCount())
					return 1;
				else if (dKey1.GetCount() < dKey2.GetCount())
					return -1;
				else
					{
					for (i = 0; i < dKey1.GetCount(); i++)
						{
						CString sItemKey1 = dKey1.GetKey(i);
						CString sItemKey2 = dKey2.GetKey(i);
						int iKeyCompare = KeyCompare(sItemKey1, sItemKey2);
						if (iKeyCompare != 0)
							return iKeyCompare;

						CDatum dItem1 = dKey1.GetElement(i);
						CDatum dItem2 = dKey2.GetElement(i);
						int iItemCompare = CDatum::DefaultCompare(pCtx, dItem1, dItem2);
						if (iItemCompare != 0)
							return iItemCompare;
						}

					return 0;
					}

			//	LATER: Not yet supported

			default:
				return 0;
			}
		}

	//	If one of the types is nil, then compare

	else if (iType1 == CDatum::typeNil || iType2 == CDatum::typeNil)
		{
		CDatum dNonNil;
		int iResult;
		if (iType2 == CDatum::typeNil)
			{
			dNonNil = dKey1;
			Swap(iType1, iType2);
			iResult = 1;
			}
		else
			{
			dNonNil = dKey2;
			iResult = -1;
			}

		switch (iType2)
			{
			case CDatum::typeString:
				if (((const CString &)dNonNil).IsEmpty())
					return 0;
				else
					return iResult;

			case CDatum::typeArray:
			case CDatum::typeStruct:
				if (dNonNil.GetCount() == 0)
					return 0;
				else
					return iResult;

			default:
				//	nil is always less
				return iResult;
			}
		}

	//	If one of the types is a number, then compare as numbers

	else if (dKey1.IsNumber() || dKey2.IsNumber())
		{
		CNumberValue Number1(dKey1);
		CNumberValue Number2(dKey2);

		if (Number1.IsValidNumber() && Number2.IsValidNumber())
			return Number1.Compare(Number2);
		else if (Number1.IsValidNumber())
			return 1;
		else if (Number2.IsValidNumber())
			return -1;
		else
			return 0;
		}

	//	Otherwise, cannot compare

	else
		return 0;
	}

bool CDatum::Deserialize (ESerializationFormats iFormat, IByteStream &Stream, IAEONParseExtension *pExtension, CDatum *retDatum)

//	Deserialize
//
//	Deserialize from the given format

	{
	try
		{
		switch (iFormat)
			{
			case formatAEONScript:
			case formatAEONLocal:
				return DeserializeAEONScript(Stream, pExtension, retDatum);

			case formatJSON:
				return DeserializeJSON(Stream, retDatum);

			case formatTextUTF8:
				return DeserializeTextUTF8(Stream, retDatum);

			default:
				ASSERT(false);
				return false;
			}
		}
	catch (...)
		{
		return false;
		}
	}

bool CDatum::DeserializeTextUTF8 (IByteStream &Stream, CDatum *retDatum)

//	DeserializeTextUTF8
//
//	Loads straight UTF-8 into a single string value.

	{
	CStringBuffer Buffer;

	//	See if we have an encoding mark

	BYTE BOM[3];
	Stream.Read(BOM, sizeof(BOM));
	if (BOM[0] == 0xef && BOM[1] == 0xbb && BOM[2] == 0xbf)
		;	//	UTF-8

	//	Otherwise, not an encoding mark, so write it to the buffer

	else
		Buffer.Write(BOM, sizeof(BOM));

	//	Write the rest

	Buffer.Write(Stream, Stream.GetStreamLength());
	return CreateStringFromHandoff(Buffer, retDatum);
	}

bool CDatum::DetectFileFormat (const CString &sFilespec, IMemoryBlock &Data, ESerializationFormats *retiFormat, CString *retsError)

//	DetectFileFormat
//
//	Try to figure out the file format. If we can't figure it out, we return TRUE
//	but retiFormat is formatUnknown. We only return FALSE if we have a read
//	error of some sort.

	{
	//	LATER.
	//	See: http://stackoverflow.com/questions/1031645/how-to-detect-utf-8-in-plain-c

	if (retiFormat)
		*retiFormat = formatUnknown;

	return true;
	}

bool CDatum::Find (CDatum dValue, int *retiIndex) const

//	Find
//
//	Looks for an element in an array and returns the index.

	{
	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_COMPLEX:
			return raw_GetComplex()->Find(dValue, retiIndex);

		default:
			return false;
		}
	}

bool CDatum::FindElement (const CString &sKey, CDatum *retpValue)

//	FindElement
//
//	Looks for the element by key.

	{
	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_COMPLEX:
			return raw_GetComplex()->FindElement(sKey, retpValue);

		default:
			return false;
		}
	}

bool CDatum::FindExternalType (const CString &sTypename, IComplexFactory **retpFactory)

//	FindExternalType
//
//	Finds a factory for the type

	{
	return (g_pFactories == NULL ? false : g_pFactories->FindFactory(sTypename, retpFactory));
	}

int CDatum::GetArrayCount (void) const

//	GetArrayCount
//
//	Returns the number of items in the datum.
//	
//	NOTE: Unlike GetCount, this only returns 1 for everything except arrays.

	{
	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_STRING:
			return (m_dwData == 0 ? 0 : 1);

		case AEON_TYPE_NUMBER:
			return 1;

		case AEON_TYPE_COMPLEX:
			if (GetBasicType() == typeArray)
				return raw_GetComplex()->GetCount();
			else
				return 1;

		default:
			ASSERT(false);
			return 0;
		}
	}

CDatum CDatum::GetArrayElement (int iIndex) const

//	GetArrayElement
//
//	Gets the appropriate element

	{
	ASSERT(iIndex >= 0);

	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_COMPLEX:
			{
			if (GetBasicType() == typeArray)
				return raw_GetComplex()->GetElement(iIndex);
			else
				return (iIndex == 0 ? *this : CDatum());
			}

		default:
			return (iIndex == 0 ? *this : CDatum());
		}
	}

CDatum::Types CDatum::GetBasicType (void) const

//	GetBasicType
//
//	Returns the type

	{
	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_STRING:
			return (m_dwData == 0 ? typeNil : typeString);

		case AEON_TYPE_NUMBER:
			switch (m_dwData & AEON_NUMBER_TYPE_MASK)
				{
				case AEON_NUMBER_CONSTANT:
					{
					switch (m_dwData)
						{
						case constTrue:
							return typeTrue;

						default:
							ASSERT(false);
							return typeUnknown;
						}
					}

				case AEON_NUMBER_28BIT:
				case AEON_NUMBER_32BIT:
					return typeInteger32;

				case AEON_NUMBER_DOUBLE:
					return typeDouble;

				default:
					ASSERT(false);
					return typeUnknown;
				}

		case AEON_TYPE_COMPLEX:
			return raw_GetComplex()->GetBasicType();

		default:
			ASSERT(false);
			return typeUnknown;
		}
	}

int CDatum::GetBinarySize (void) const

//	GetBinarySize
//
//	For binary blob objects, this returns the total size in bytes.

	{
	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_COMPLEX:
			return raw_GetComplex()->GetBinarySize();

		default:
			{
			const CString &sData = *this;
			return sData.GetLength();
			}
		}
	}

void CDatum::GrowToFit (int iCount)

//	GrowToFit
//
//	Grow array to be able to fit the given number of additional elements

	{
	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_COMPLEX:
			raw_GetComplex()->GrowToFit(iCount);
			break;
		}
	}

bool CDatum::IsMemoryBlock (void) const

//	IsMemoryBlock
//
//	Returns TRUE if we can represent this as a contiguous block of memory.
//	If we cannot, then we need to use functions like WriteBinaryToStream.

	{
	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_COMPLEX:
			return raw_GetComplex()->IsMemoryBlock();

		default:
			{
			const CString &sData = *this;
			return (sData.GetLength() > 0);
			}
		}
	}

void CDatum::WriteBinaryToStream (IByteStream &Stream, int iPos, int iLength, IProgressEvents *pProgress) const

//	WriteBinaryToStream
//
//	Writes a binary blob object to a stream.

	{
	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_COMPLEX:
			raw_GetComplex()->WriteBinaryToStream(Stream, iPos, iLength, pProgress);
			break;

		default:
			{
			const CString &sData = *this;

			if (iPos >= sData.GetLength())
				return;

			if (iLength == -1)
				iLength = Max(0, sData.GetLength() - iPos);
			else
				iLength = Min(iLength, sData.GetLength() - iPos);

            if (pProgress)
                pProgress->OnProgressStart();

			Stream.Write(sData.GetPointer() + iPos, iLength);

            if (pProgress)
                pProgress->OnProgressDone();
			break;
			}
		}
	}

CDatum::ECallTypes CDatum::GetCallInfo (CDatum *retdCodeBank, DWORD **retpIP) const

//	GetCallInfo
//
//	Returns info about invocation

	{
	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_COMPLEX:
			return raw_GetComplex()->GetCallInfo(retdCodeBank, retpIP);

		default:
			return funcNone;
		}
	}

IComplexDatum *CDatum::GetComplex (void) const

//	GetComplex
//
//	Returns a pointer to the complex type

	{
	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_COMPLEX:
			return raw_GetComplex();

		default:
			return NULL;
		}
	}

int CDatum::GetCount (void) const

//	GetCount
//
//	Returns the number of items in the datum

	{
	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_STRING:
			return (m_dwData == 0 ? 0 : 1);

		case AEON_TYPE_NUMBER:
			return 1;

		case AEON_TYPE_COMPLEX:
			return raw_GetComplex()->GetCount();

		default:
			ASSERT(false);
			return 0;
		}
	}

CDatum CDatum::GetElement (IInvokeCtx *pCtx, int iIndex) const

//	GetElement
//
//	Gets the appropriate element

	{
	ASSERT(iIndex >= 0);

	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_COMPLEX:
			{
			if (iIndex == 0)
				{
				IComplexDatum *pComplex = raw_GetComplex();
				if (pComplex->IsArray())
					return raw_GetComplex()->GetElement(pCtx, 0);
				else
					return *this;
				}
			else
				return raw_GetComplex()->GetElement(pCtx, iIndex);
			}

		default:
			return (iIndex == 0 ? *this : CDatum());
		}
	}

CDatum CDatum::GetElement (int iIndex) const

//	GetElement
//
//	Gets the appropriate element

	{
	ASSERT(iIndex >= 0);

	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_COMPLEX:
			{
			if (iIndex == 0)
				{
				IComplexDatum *pComplex = raw_GetComplex();
				if (pComplex->IsArray())
					return raw_GetComplex()->GetElement(0);
				else
					return *this;
				}
			else
				return raw_GetComplex()->GetElement(iIndex);
			}

		default:
			return (iIndex == 0 ? *this : CDatum());
		}
	}

CDatum CDatum::GetElement (IInvokeCtx *pCtx, const CString &sKey) const

//	GetElement
//
//	Gets the appropriate element

	{
	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_COMPLEX:
			return raw_GetComplex()->GetElement(pCtx, sKey);

		default:
			return CDatum();
		}
	}

CDatum CDatum::GetElement (const CString &sKey) const

//	GetElement
//
//	Gets the appropriate element

	{
	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_COMPLEX:
			return raw_GetComplex()->GetElement(sKey);

		default:
			return CDatum();
		}
	}

CString CDatum::GetKey (int iIndex) const

//	GetKey
//
//	Returns the key at the given index

	{
	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_COMPLEX:
			return raw_GetComplex()->GetKey(iIndex);

		default:
			return NULL_STR;
		}
	}

CDatum::Types CDatum::GetNumberType (int *retiValue, CDatum *retdConverted) const

//	GetNumberType
//
//	Returns the most appropriate number type for the datum and optimistically
//	returns the value if the type is integer.
//
//	The number type is the best number type that the datum can be cast
//	to without loss of data, following this order (from most preferable to
//	least):
//
//	typeInteger32
//	typeDouble
//	typeIntegerIP
//
//	If the original datum is a string then we try to parse it and return the
//	best number type. retdConverted will be a datum representing the parsed
//	number.
//
//	If the value cannot be converted to a number we return typeUnknown.

	{
	//	Pre-init

	if (retdConverted)
		*retdConverted = *this;

	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_STRING:
			{
			if (m_dwData == 0)
				{
				if (retiValue)
					*retiValue = 0;
				return typeInteger32;
				}
			else
				{
				CDatum dNumberValue;
				if (!CDatum::CreateFromStringValue(*this, &dNumberValue)
						|| !dNumberValue.IsNumber())
					return typeUnknown;

				if (retdConverted)
					*retdConverted = dNumberValue;

				return dNumberValue.GetNumberType(retiValue);
				}
			}

		case AEON_TYPE_NUMBER:
			switch (m_dwData & AEON_NUMBER_TYPE_MASK)
				{
				case AEON_NUMBER_CONSTANT:
					{
					switch (m_dwData)
						{
						case constTrue:
							if (retiValue)
								*retiValue = 1;
							return typeInteger32;

						default:
							ASSERT(false);
							return typeUnknown;
						}
					}

				case AEON_NUMBER_28BIT:
					if (retiValue)
						*retiValue = ((int)(m_dwData & AEON_NUMBER_MASK) >> 4);
					return typeInteger32;

				case AEON_NUMBER_32BIT:
					if (retiValue)
						*retiValue = (int)g_IntAlloc.Get(GetNumberIndex());
					return typeInteger32;

				case AEON_NUMBER_DOUBLE:
					return typeDouble;

				default:
					ASSERT(false);
					return typeUnknown;
				}

		case AEON_TYPE_COMPLEX:
			return raw_GetComplex()->GetNumberType(retiValue);

		default:
			ASSERT(false);
			return typeUnknown;
		}
	}

CDatum::Types CDatum::GetStringValueType (const CString &sValue)

//	GetStringValueType
//
//	Returns one of the following:
//
//	typeNil if sValue is empty
//	typeInteger32 if sValue is a 32-bit integer
//	typeIntegerIP if sValue is an integer (which may or may not be > 64-bits)
//	typeDouble if sValue is a double
//	typeString otherwise.

	{
	enum EStates
		{
		stateStart,
		stateHex0,
		stateHex,
		stateInteger,
		stateDoubleFrac,
		stateDoubleExp,
		stateDoubleExpSign,
		};

	char *pPos = sValue.GetParsePointer();
	char *pPosEnd = pPos + sValue.GetLength();
	int iState = stateStart;

	while (pPos < pPosEnd)
		{
		switch (iState)
			{
			case stateStart:
				{
				//	If 0 then we might be a hex number

				if (*pPos == '0')
					iState = stateHex0;

				//	If -, +, or a digit, we might be an integer

				else if (*pPos == '-' || *pPos == '+' || strIsDigit(pPos))
					iState = stateInteger;

				//	If . then we might be a double

				else if (*pPos == '.')
					iState = stateDoubleFrac;

				//	Otherwise, we are a string

				else
					return typeString;

				break;
				}

			case stateHex0:
				{
				if (*pPos == 'x' || *pPos == 'X')
					iState = stateHex;
				else if (strIsDigit(pPos))
					iState = stateInteger;
				else if (*pPos == '.')
					iState = stateDoubleFrac;
				else if (*pPos == 'e' || *pPos == 'E')
					iState = stateDoubleExp;
				else
					return typeString;

				break;
				}

			case stateHex:
				{
				if (strIsDigit(pPos)
						|| (*pPos >= 'A' && *pPos <= 'F')
						|| (*pPos >= 'a' && *pPos <= 'f'))
					NULL;
				else
					return typeString;

				break;
				}

			case stateInteger:
				{
				if (strIsDigit(pPos))
					NULL;
				else if (*pPos == '.')
					iState = stateDoubleFrac;
				else if (*pPos == 'e' || *pPos == 'E')
					iState = stateDoubleExp;
				else
					return typeString;

				break;
				}

			case stateDoubleFrac:
				{
				if (strIsDigit(pPos))
					NULL;
				else if (*pPos == 'e' || *pPos == 'E')
					iState = stateDoubleExp;
				else
					return typeString;

				break;
				}

			case stateDoubleExp:
				{
				if (*pPos == '+' || *pPos == '-' || strIsDigit(pPos))
					iState = stateDoubleExpSign;
				else
					return typeString;

				break;
				}

			case stateDoubleExpSign:
				{
				if (strIsDigit(pPos))
					NULL;
				else
					return typeString;

				break;
				}
			}

		pPos++;
		}

	switch (iState)
		{
		case stateStart:
			return typeNil;

		case stateHex:
			//	LATER:
			return typeString;

		case stateInteger:
			if (strOverflowsInteger32(sValue))
				return typeIntegerIP;
			else
				return typeInteger32;

		case stateDoubleFrac:
		case stateDoubleExpSign:
			return typeDouble;

		default:
			return typeString;
		}
	}

const CString &CDatum::GetTypename (void) const

//	GetTypename
//
//	Gets the typename of the object

	{
	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_STRING:
			if (m_dwData == 0)
				return TYPENAME_NIL;
			else
				return TYPENAME_STRING;

		case AEON_TYPE_NUMBER:
			switch (m_dwData & AEON_NUMBER_TYPE_MASK)
				{
				case AEON_NUMBER_CONSTANT:
					{
					switch (m_dwData)
						{
						case constTrue:
							return TYPENAME_TRUE;

						default:
							ASSERT(false);
							return NULL_STR;
						}
					}

				case AEON_NUMBER_28BIT:
					return TYPENAME_INT32;

				case AEON_NUMBER_32BIT:
					return TYPENAME_INT32;

				case AEON_NUMBER_DOUBLE:
					return TYPENAME_DOUBLE;

				default:
					ASSERT(false);
					return NULL_STR;
				}

		case AEON_TYPE_COMPLEX:
			return raw_GetComplex()->GetTypename();

		default:
			ASSERT(false);
			return NULL_STR;
		}
	}

bool CDatum::Invoke (IInvokeCtx *pCtx, CDatum dLocalEnv, CDatum *retdResult)

//	Invoke
//
//	Invokes the function

	{
	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_COMPLEX:
			return raw_GetComplex()->Invoke(pCtx, dLocalEnv, retdResult);

		default:
			return false;
		}
	}

bool CDatum::InvokeContinues (IInvokeCtx *pCtx, CDatum dContext, CDatum dResult, CDatum *retdResult)

//	InvokeContinues
//
//	Invokes the function

	{
	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_COMPLEX:
			return raw_GetComplex()->InvokeContinues(pCtx, dContext, dResult, retdResult);

		default:
			return false;
		}
	}

bool CDatum::IsEqual (CDatum dValue) const

//	IsEqual
//
//	Returns TRUE if the values are equal

	{
	switch (GetBasicType())
		{
		case typeNil:
			return dValue.IsNil();

		case typeTrue:
			return !dValue.IsNil();

		case typeInteger32:
		case typeInteger64:
		case typeIntegerIP:
		case typeDouble:
			return (dValue.IsNumber() && CNumberValue(*this).Compare(dValue) == 0);

		case typeString:
			return (dValue.GetBasicType() == typeString && strEquals(*this, dValue));

		case typeDateTime:
			return (dValue.GetBasicType() == typeDateTime && ((const CDateTime &)*this == (const CDateTime &)dValue));

		//	LATER
		case typeArray:
		case typeBinary:
		case typeStruct:
		case typeSymbol:
			return false;

		default:
			ASSERT(false);
			return false;
		}
	}

bool CDatum::IsError (void) const

//	IsError
//
//	Returns TRUE if this is an error value

	{
	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_COMPLEX:
			return raw_GetComplex()->IsError();

		default:
			return false;
		}
	}

bool CDatum::IsNil (void) const

//	IsNil
//
//	Returns TRUE if this is a nil value

	{
	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_STRING:
			return (m_dwData == 0 ? true : false);

		case AEON_TYPE_COMPLEX:
			return raw_GetComplex()->IsNil();

		default:
			return false;
		}
	}

bool CDatum::IsNumber (void) const

//	IsNumber
//
//	Returns TRUE if this is a number

	{
	switch (GetBasicType())
		{
		case typeInteger32:
		case typeInteger64:
		case typeIntegerIP:
		case typeDouble:
			return true;

		default:
			return false;
		}
	}

void CDatum::Mark (void)

//	Mark
//
//	Marks the datum so that we know that we cannot free it

	{
	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_STRING:
			if (m_dwData != 0)
				g_StringAlloc.Mark((LPSTR)m_dwData);
			break;

		case AEON_TYPE_NUMBER:
			switch (m_dwData & AEON_NUMBER_TYPE_MASK)
				{
				case AEON_NUMBER_32BIT:
					g_IntAlloc.Mark(GetNumberIndex());
					break;

				case AEON_NUMBER_DOUBLE:
					g_DoubleAlloc.Mark(GetNumberIndex());
					break;
				}
			break;

		case AEON_TYPE_COMPLEX:
			g_ComplexAlloc.Mark(raw_GetComplex());
			break;
		}
	}

void CDatum::MarkAndSweep (void)

//	MarkAndSweep
//
//	After we've marked all the extended objects that we are using,
//	we sweep away everything not being used.
//	(Sweeping also clears the marks)

	{
	int i;

	//	Mark all object that we know about

	for (i = 0; i < g_MarkList.GetCount(); i++)
		g_MarkList[i]();

	//	Sweep

	g_DoubleAlloc.Sweep();
	g_IntAlloc.Sweep();
	g_StringAlloc.Sweep();
	g_ComplexAlloc.Sweep();
	}

double CDatum::raw_GetDouble (void) const

//	raw_GetDouble
//
//	Returns a double

	{
	return g_DoubleAlloc.Get(GetNumberIndex());
	}

int CDatum::raw_GetInt32 (void) const

//	raw_GetInt32
//
//	Returns an integer

	{
	return (int)g_IntAlloc.Get(GetNumberIndex());
	}

bool CDatum::RegisterExternalType (const CString &sTypename, IComplexFactory *pFactory)

//	RegisterExternalType
//
//	Registers an external type

	{
	if (g_pFactories == NULL)
		g_pFactories = new CAEONFactoryList;

	g_pFactories->RegisterFactory(sTypename, pFactory);

	return true;
	}

void CDatum::RegisterMarkProc (MARKPROC fnProc)

//	RegisterMarkProc
//
//	Register a procedure that will mark data in use

	{
	g_MarkList.Insert(fnProc);
	}

void CDatum::Serialize (ESerializationFormats iFormat, IByteStream &Stream) const

//	Serialize
//
//	Serializes the datum

	{
	switch (iFormat)
		{
		case formatAEONScript:
		case formatAEONLocal:
			SerializeAEONScript(iFormat, Stream);
			break;

		case formatJSON:
			SerializeJSON(Stream);
			break;

		default:
			ASSERT(false);
		}
	}

CString CDatum::SerializeToString (ESerializationFormats iFormat) const

//	SerializeToString
//
//	Returns the serialization of the datum as a string

	{
	CStringBuffer Output;
	Serialize(iFormat, Output);
	return CString::CreateFromHandoff(Output);
	}

void CDatum::SetElement (IInvokeCtx *pCtx, const CString &sKey, CDatum dValue)

//	SetElement
//
//	Sets the value to the datum. Note that this modifies the datum in place.
//	Do not use this unless you are sure about who else might have a pointer
//	to the datum.

	{
	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_COMPLEX:
			return raw_GetComplex()->SetElement(pCtx, sKey, dValue);

		default:
			//	Nothing happens
			;
		}
	}

void CDatum::SetElement (const CString &sKey, CDatum dValue)

//	SetElement
//
//	Sets the value to the datum. Note that this modifies the datum in place.
//	Do not use this unless you are sure about who else might have a pointer
//	to the datum.

	{
	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_COMPLEX:
			return raw_GetComplex()->SetElement(sKey, dValue);

		default:
			//	Nothing happens
			;
		}
	}

void CDatum::SetElement (int iIndex, CDatum dValue)

//	SetElement
//
//	Sets the value to the datum. Note that this modifies the datum in place.
//	Do not use this unless you are sure about who else might have a pointer
//	to the datum.

	{
	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_COMPLEX:
			return raw_GetComplex()->SetElement(iIndex, dValue);

		default:
			//	Nothing happens
			;
		}
	}

void CDatum::Sort (ESortOptions Order, TArray<CDatum>::COMPAREPROC pfCompare, void *pCtx)

//	Sort
//
//	Sorts the datum in place

	{
	if (pfCompare == NULL)
		pfCompare = CDatum::DefaultCompare;

	switch (m_dwData & AEON_TYPE_MASK)
		{
		case AEON_TYPE_COMPLEX:
			return raw_GetComplex()->Sort(Order, pfCompare, pCtx);

		default:
			//	Nothing happens
			;
		}
	}

