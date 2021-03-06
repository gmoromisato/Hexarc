//	AeonDB.h
//
//	AeonDB Archon Implementation
//	Copyright (c) 2011 by George Moromisato. All Rights Reserved.

#pragma once

#ifdef DEBUG
//#define DEBUG_FILES
//#define DEBUG_FILE_UPLOAD
#endif

class CAeonRowValue;

enum EKeyTypes
	{
	keyInt32,							//	32-bit signed integer
	keyInt64,							//	64-bit unsigned integer
	keyUTF8,							//	UTF8 string
	keyDatetime,						//	CDateTime

	keyListUTF8,						//	List of UTF8 strings
	};

enum EDimensions
	{
	dimX =								0,
	dimY =								1,
	dimZ =								2,

	dimCount =							3,
	};

typedef DWORDLONG SEQUENCENUMBER;

struct SDimensionDesc
	{
	EKeyTypes iKeyType;						//	Key type 
	ESortOptions iSort;						//	Sort order
	};

class CTableDimensions : public TArray<SDimensionDesc>
	{
	};

class CRowKey
	{
	public:
		CRowKey (void);
		CRowKey (const CTableDimensions &Dims, const CString &sKey);
		~CRowKey (void);

		CRowKey &operator= (const CRowKey &Src);

		static void CreateFromDatumAndRowID (const CTableDimensions &Dims, const TArray<CDatum> &Data, SEQUENCENUMBER RowID, CRowKey *retKey);
		static void CreateFromFilePath (const CString &sFilePath, CRowKey *retpKey, bool bIsKey = false);
		static void CreateFromEncodedKey (const CTableDimensions &Dims, const CString &sKey, CRowKey *retKey, bool bMakeCopy = true);
		static void CreateFromEncodedKeyPartial (const CTableDimensions &Dims, int iPartialDims, const CString &sKey, CRowKey *retKey);
		static CString FilePathToKey (const CString &sFilePath);
		static CString KeyToFilePath (const CString &sKey);
		static bool ParseKey (const CTableDimensions &Dims, CDatum dKey, CRowKey *retpKey, CString *retsError);
		static bool ParseKeyForCreate (const CTableDimensions &Dims, CDatum dKey, CRowKey *retpKey, CString *retsError);

		CDatum AsDatum (const CTableDimensions &Dims) const;
		inline const CString &AsEncodedString (void) const { return *m_psKey; }
		inline int GetCount (void) const { return m_iCount; }
		bool HasNullDimensions (const CTableDimensions &Dims) const;
		bool MatchesDimensions (const CTableDimensions &Dims) const;

		static int Compare (const CTableDimensions &Dims, const CRowKey &Key1, const CRowKey &Key2);
		static bool ComparePartial (const CTableDimensions &Dims, const CRowKey &PartialKey, const CRowKey &Key);

	private:
		CDatum AsDatum (char *pPos, EKeyTypes iKeyType, char **retpPos = NULL) const;
		void CleanUp (void);
		static void WriteKeyPart (CStringBuffer &Buffer, EKeyTypes iKeyType, CDatum dKey);

		int m_iCount;						//	Number of parts
		const CString *m_psKey;				//	Parts concatenated into a single buffer
		bool m_bFree;						//	TRUE if we own m_psKey
	};

enum EAeonErrorCodes
	{
	AEONERR_OK =						0x00000000,	//	No error
	AEONERR_FAIL =						0xffffffff,	//	Generic error
	AEONERR_OUT_OF_DATE =				0xfffffffe,	//	Data modified after call (failure of optimistic concurrency)
	AEONERR_ALREADY_EXISTS =			0xfffffffd,	//	Value already exists
	};

typedef DWORD AEONERR;

//	IOrderedRowSet

class IOrderedRowSet : public IRefCounted
	{
	public:
		virtual ~IOrderedRowSet (void) { }

		virtual bool FindData (const CRowKey &Key, CDatum *retData, SEQUENCENUMBER *retRowID = NULL) { return false; }
		virtual bool FindKey (const CRowKey &Key, int *retiIndex) { return false; }
		virtual int GetCount (void) { return 0; }
		virtual CDatum GetData (int iIndex) { return CDatum(); }
		virtual CString GetKey (int iIndex) { return NULL_STR; }
		virtual bool GetRow (int iIndex, CRowKey *retKey, CDatum *retData, SEQUENCENUMBER *retRowID = NULL) { return false; }
		virtual SEQUENCENUMBER GetRowID (int iIndex) { return 0; }
		virtual DWORD GetRowSize (int iIndex) { return 0; }
		virtual void WriteData (IByteStream &Stream, int iIndex, DWORD *retdwSize = NULL, SEQUENCENUMBER *retRowID = NULL) = 0;

		//	Helper methods
//		static int CompareKeys (const CTableDimensions &Dims, const CString &sKey1, const CString &sKey2);
//		static CString PathToKey (const SDimensionPath &Path);
		static void ShiftDimensions (const CTableDimensions &Source, CTableDimensions *retDest);
	};

class CRowIterator
	{
	public:
		inline CRowIterator (void) : m_bIncludeNil(true) { }
		inline CRowIterator (const CRowIterator &Src) { Copy(Src); }
		~CRowIterator (void);

		inline CRowIterator &operator= (const CRowIterator &Src) { CleanUp(); Copy(Src); return *this; }

		void AddSegment (IOrderedRowSet *pSegment);
		bool Init (const CTableDimensions &Dims);

		bool HasMore (void);
		CString GetKey (void);
		CString GetNextKey (void);
		void GetNextKey (CRowKey *retKey);
		CString GetNextRow (CDatum *retdData);
		void GetNextRow (CRowKey *retKey, CDatum *retdData, SEQUENCENUMBER *retRowID = NULL);
		DWORD GetNextRowSize (void);
		void GetRow (CRowKey *retKey, CDatum *retdData, SEQUENCENUMBER *retRowID = NULL);
		void WriteNextRow (IByteStream &Stream, DWORD *retdwKeySize, DWORD *retdwDataSize, SEQUENCENUMBER *retRowID = NULL);
		void Reset (void);
		bool SelectKey (const CRowKey &Key);
		inline void SetIncludeNil (bool bInclude = true) { m_bIncludeNil = bInclude; }
		void SetLimits (const TArray<int> &Limits);

	private:
		struct SEntry
			{
			IOrderedRowSet *pSegment;
			CString sKey;
			int iPosCursor;
			int iCount;
			};

		struct SLimit
			{
			int iLimit;						//	-1 == no limit
			CRowKey LimitKey;

			int iLeft;						//	Rows left in limit; -1 == no limit
			};

		void Advance (void);
		void AdvanceUntilDifferent (const CRowKey &PartialKey);
		void AdvanceUntilNonNil (void);
		void AdvanceWithLimits (void);
		void CleanUp (void);
		bool CompareToLimit (int *retiDiffKey);
		void Copy (const CRowIterator &Src);
		bool DecrementLimit (int iDimIndex, int *retiDimLeft);
		inline bool HasLimits (void) { return (m_Limits.GetCount() > 0); }

		CTableDimensions m_Dims;
		TArray<SEntry> m_Data;
		int m_iEntryCursor;

		bool m_bIncludeNil;
		TArray<SLimit> m_Limits;
	};

//	CAeonRowValue
//
//	This object manipulates a single row in a table.

class CAeonRowValue
	{
	public:
		CAeonRowValue (void);
		CAeonRowValue (const CAeonRowValue &Src) { Copy(Src); }
		~CAeonRowValue (void);

		inline CAeonRowValue &operator= (const CAeonRowValue &Src) { CleanUp(); Copy(Src); return *this; }

		bool FindValue (const CRowKey &Path, CDatum *retData = NULL);
		int GetSerializedSize (void);
		CDatum GetValue (void);
		void Init (void *pValue = NULL);
		bool IsNil (void);
		void Load (void);
		void Save (void);
		void Serialize (IByteStream &Stream);
		void SetValue (CDatum dValue);

		static DWORD GetSerializedKeySize (const CString &sKey);
		static void SerializeKey (IByteStream &Stream, const CString &sKey, DWORD *retdwSize);

	private:
		struct SItemHeader
			{
			DWORD dwSize;
			};

		void CleanUp (void);
		void Copy (const CAeonRowValue &Src);
		inline SItemHeader *Get0DItem (void) { return (SItemHeader *)(&((SItemHeader *)m_pFixedBlock)[1]); }
		DWORD GetFixedBlockSize (void);
		CDatum ItemToValue (SItemHeader *pItem);

		DWORD m_dwFixedBlockAlloc;			//	Allocation size of fixed block
		void *m_pFixedBlock;				//	Fixed block pointer
	};

//	CAeonRowArray

class CAeonRowArray : public IOrderedRowSet
	{
	public:
		CAeonRowArray (void) : m_dwMemoryUsed(0), m_dwChanges(0) { }

		void DeleteAll (void);
		inline const CTableDimensions &GetDimensions (void) { return m_Dims; }
		inline DWORD GetMemoryUsed (void) { return m_dwMemoryUsed; }
		inline int GetUpdateCount (void) { return m_dwChanges; }
		void Init (const CTableDimensions &Dims);
		bool Insert (const CRowKey &Key, CDatum dData, SEQUENCENUMBER RowID);

		//	IOrderedRowSet virtuals
		virtual bool FindData (const CRowKey &Key, CDatum *retData, SEQUENCENUMBER *retRowID = NULL);
		virtual bool FindKey (const CRowKey &Key, int *retiIndex);
		virtual int GetCount (void) { CSmartLock Lock(m_cs); return m_Order.GetCount(); }
		virtual CDatum GetData (int iIndex);
		virtual CString GetKey (int iIndex) { CSmartLock Lock(m_cs); return m_Rows[m_Order[iIndex]].sKey; }
		virtual bool GetRow (int iIndex, CRowKey *retKey, CDatum *retData, SEQUENCENUMBER *retRowID = NULL);
		virtual SEQUENCENUMBER GetRowID (int iIndex) { CSmartLock Lock(m_cs); return m_Rows[m_Order[iIndex]].RowID; }
		virtual DWORD GetRowSize (int iIndex);
		virtual void WriteData (IByteStream &Stream, int iIndex, DWORD *retdwSize = NULL, SEQUENCENUMBER *retRowID = NULL);

	private:
		struct SEntry
			{
			CString sKey;
			CAeonRowValue Value;
			SEQUENCENUMBER RowID;
			};

		inline bool IsInitialized (void) { return (m_Dims.GetCount() != 0); }

		CCriticalSection m_cs;
		CTableDimensions m_Dims;			//	Dimension descriptors
		DWORD m_dwMemoryUsed;				//	Memory used for all the rows
		DWORD m_dwChanges;					//	Number of updates
		TArray<int> m_Order;				//	Row order
		TArray<SEntry> m_Rows;				//	Row data
	};

//	CRowInsertLog

class CRowInsertLog
	{
	public:
		CRowInsertLog (void) : m_dwVersion(0) { }

		inline void Close (void) { m_File.Close(); }
		bool Create (const CString &sFilename);
		inline const CString &GetFilespec (void) const { return m_File.GetFilespec(); }
		inline DWORD GetVersion (void) { return m_dwVersion; }
		bool Insert (const CRowKey &Key, CDatum dData, SEQUENCENUMBER RowID, CString *retsError = NULL);
		bool Open (const CString &sFilename, CAeonRowArray *pRows, int *retiRowCount, CString *retsError);
		bool Reset (void);

	private:
		struct SHeader
			{
			DWORD dwSignature;
			DWORD dwVersion;
			};

		bool Recover (CAeonRowArray *pRows, int *retiRowCount, CString *retsError);

		CString m_sFilename;
		CFile m_File;
		DWORD m_dwVersion;
	};

//	CSegmentBlockCache

class CSegmentBlockCache
	{
	public:
		CSegmentBlockCache (void) : m_iCacheSize(-1) { }
		~CSegmentBlockCache (void);

		inline DWORDLONG GetFileSize (void) { return m_File.GetSize(); }
		bool Init (const CString &sFilespec, int iCacheSize);
		void LoadBlock (DWORD dwOffset, DWORD dwBlockSize, void **retpBlock);
		void Term (void);
		void UnloadBlock (DWORD dwOffset);

	private:
		struct SEntry
			{
			void *pBlock;
			DWORD dwRefCount;
			DWORD dwLastAccess;
			};

		void FreeUnloadedBlocks (void);

		CFile m_File;

		int m_iCacheSize;
		TSortMap<DWORD, SEntry> m_Cache;
	};

//	CAeonSegment

class CAeonSegment : public IOrderedRowSet
	{
	public:
		enum Flags
			{
			FLAG_SECONDARY_VIEW =			0x00000001,	//	Segment is a secondary view
			FLAG_HAS_ROW_ID =				0x00000002,	//	Segment stores a rowID
			};

		struct SInfo
			{
			SEQUENCENUMBER Seq;
			DWORD dwRowCount;
			};

		CAeonSegment (void);
		~CAeonSegment (void);

		bool Create (DWORD dwViewID, const CTableDimensions &Dims, SEQUENCENUMBER Seq, CRowIterator &Rows, const CString &sFilespec, DWORD dwFlags, CString *retsError);
		CDatum DebugDump (void) const;
		inline DWORDLONG GetFileSize (void) { return m_Blocks.GetFileSize(); }
		inline const CString &GetFilespec (void) const { return m_sFilespec; }
		static bool GetInfo (const CString &sFilespec, SInfo *retInfo);
		inline SEQUENCENUMBER GetSequence (void) { return m_Seq; }
		inline DWORD GetViewID (void) { return m_pHeader->dwViewID; }
		inline void MarkForDelete (void) { m_bMarkedForDelete = true; }
		bool Open (const CString &sFilespec, CString *retsError);
		inline void SetDimensions (const CTableDimensions &Dims) { m_Dims = Dims; }

		//	IOrderedRowSet
		virtual bool FindData (const CRowKey &Key, CDatum *retData, SEQUENCENUMBER *retRowID = NULL);
		virtual bool FindKey (const CRowKey &Key, int *retiIndex);
		virtual int GetCount (void) { return (int)m_pHeader->dwRowCount; }
		virtual CDatum GetData (int iIndex);
		virtual CString GetKey (int iIndex);
		virtual bool GetRow (int iIndex, CRowKey *retKey, CDatum *retData, SEQUENCENUMBER *retRowID = NULL);
		virtual DWORD GetRowSize (int iIndex);
		virtual void WriteData (IByteStream &Stream, int iIndex, DWORD *retdwSize = NULL, SEQUENCENUMBER *retRowID = NULL);

	private:
		struct SSegmentHeader
			{
			DWORD dwSignature;				//	Always 'AEOS'
			DWORD dwVersion;
			SEQUENCENUMBER Seq;
			DWORD dwIndexCount;				//	Number of entries in index
			DWORD dwIndexOffset;			//	Offset to index (from start of file)
			DWORD dwIndexSize;				//	Size of index block (including var items)
			DWORD dwRowCount;				//	Total rows in segment
			DWORD dwViewID;					//	ViewID of segment
			DWORD dwFlags;					//	Segment flags

			DWORD dwSpare[6];
			};

		struct SIndexEntry
			{
			DWORD dwKeyOffset;				//	Offset to key (from start of file)
			DWORD dwBlockOffset;			//	Offset to block (from start of file)
			DWORD dwBlockSize;				//	Size of block
			DWORD dwRowCount;				//	Number of rows in block
			};

		struct SBlockHeader
			{
			DWORD dwSize;
			DWORD dwRowCount;
			};

		struct SBlockIndexEntry
			{
			DWORD dwKeyOffset;
			DWORD dwValueOffset;
			};

		struct SBlockIndexEntryExtra
			{
			SEQUENCENUMBER RowID;
			};

		struct SBlockDesc
			{
			int iFirstRow;					//	Index of first row in block (for the last entry, this is 1+ last index).
			DWORD dwBlockSize;				//	Computed size of block (if 0, then this is a dummy block)
			DWORD dwFirstKeySize;			//	Size of first key of block (including DWORD size and padding)
			};

		bool BlockFindData (SBlockHeader *pBlock, const CString &sKey, CDatum *retData, SEQUENCENUMBER *retRowID = NULL);
		bool BlockFindRow (SBlockHeader *pBlock, const CString &sKey, int *retiPos);
		SBlockIndexEntry *BlockGetIndexEntry (SBlockHeader *pBlock, int iPos);
		CString BlockGetKey (SBlockHeader *pBlock, int iPos);
		void *BlockGetValue (SBlockHeader *pBlock, int iPos, SEQUENCENUMBER *retRowID = NULL);
		bool Delete (void);
		SIndexEntry *GetBlockByKey (const CString &sKey, int *retiPos = NULL);
		SIndexEntry *GetBlockByRowPos (int iIndex, int *retiRowInBlock = NULL);
		inline SBlockIndexEntryExtra *GetBlockIndexEntryExtra (SBlockIndexEntry *pEntry) { return (SBlockIndexEntryExtra *)(&pEntry[1]); }
		inline int GetBlockIndexEntrySize (void) { return (sizeof(SBlockIndexEntry) + (HasRowID() ? sizeof(SBlockIndexEntryExtra) : 0)); }
		inline int GetIndexCount (void) const { return m_pHeader->dwIndexCount; }
		inline SIndexEntry *GetIndexEntry (int iIndex) { ASSERT(iIndex >= 0 && iIndex < GetIndexCount()); return &m_pIndex[iIndex]; }
		inline const SIndexEntry *GetIndexEntry (int iIndex) const { ASSERT(iIndex >= 0 && iIndex < GetIndexCount()); return &m_pIndex[iIndex]; }
		inline bool HasRowID (void) { return ((m_pHeader->dwFlags & FLAG_HAS_ROW_ID) ? true : false); }
		CString IndexGetKey (const SIndexEntry *pIndex) const;

		CCriticalSection m_cs;
		CString m_sFilespec;				//	Filespec backing this segment
		SEQUENCENUMBER m_Seq;				//	Sequence number when segment was created
		DWORD m_dwDesiredBlockSize;			//	Block size target
		CTableDimensions m_Dims;			//	Dimension descriptors

		SSegmentHeader *m_pHeader;			//	Loaded header
		SIndexEntry *m_pIndex;				//	Loaded index
		CSegmentBlockCache m_Blocks;		//	Cached blocks

		bool m_bMarkedForDelete;			//	If TRUE, delete on final release
	};

//	CAeonUploadSessions

class CAeonFile : public IByteStream
	{
	public:
		bool Create (const CString &sFilespec, CDatum dFileDesc, CString *retsError);
		bool Open (const CString &sFilespec, CString *retsError);

	private:
		CFile m_File;
	};

class CAeonUploadSessions
	{
	public:
		struct SReceipt
			{
			int iComplete;					//	% complete
			CString sError;					//	Error (if ProcessUpload returns false)

			//	If 100% complete
			CString sFilePath;				//	File path
			DWORD dwVersion;				//	Current version to overwrite
			CDatum dFileDesc;				//	File descriptor
			int iFileSize;					//	Total size of file
			bool bNewFile;					//	New file created
			CString sFilespec;				//	Uploaded file
			};

		CAeonUploadSessions (void) : m_pStorage(NULL) { }
		~CAeonUploadSessions (void);

		void AbortUpload (const SReceipt &Receipt);
		inline void Init (CMachineStorage *pStorage, const CString &sTableName, const CString &sPrimaryVolume, const CString &sBackupVolume) { m_pStorage = pStorage; m_sTableName = sTableName; m_sPrimaryVolume = sPrimaryVolume; m_sBackupVolume = sBackupVolume; }
		void Mark (void);
		bool ProcessUpload (CMsgProcessCtx &Ctx, const CString &sSessionID, const CString &sFilePath, CDatum dUploadDesc, CDatum dData, const CString &sCurrentFilespec, SReceipt *retReceipt);
		void SetVolumes (const CString &sPrimaryVolume, const CString &sBackupVolume);

	private:
		struct SFileRange
			{
			int iPos;
			int iLength;
			};

		struct SUploadSessionCtx
			{
			CString sSessionID;				//	Unique session ID
			DWORD dwLastActivity;			//	Tick when we last processed this session

			CString sFilePath;				//	File path in database
			DWORD dwVersion;				//	Version that we expect
			CDatum dFileDesc;				//	Uploaded fileDesc

			int iUploadLen;					//	Total length of upload (in bytes)
			TArray<SFileRange> DataExpected;//	Data ranges still left to upload

			int iTempFileSize;				//	Final size of uploaded file
			bool bTempFileCreated;			//	TRUE if we created a new file this upload
			CString sTempFilespec;			//	Filespec of uploaded file
			int iTempFileOffset;			//	Position of upload within file.
			};

		int CalcUploadCompletion (SUploadSessionCtx *pSession);
		bool CreateTempFile (CString *retsFilespec, CFileMultiplexer *retFile, CString *retsError);
		void Delete (const CString &sSessionID);
		void DeleteTempFile (SUploadSessionCtx *pSession);
		void DeleteTempFile (SUploadSessionCtx *pSession, CFileMultiplexer *pFile);

		CMachineStorage *m_pStorage;
		CString m_sTableName;
		CString m_sPrimaryVolume;
		CString m_sBackupVolume;

		CCriticalSection m_cs;
		TSortMap<CString, SUploadSessionCtx *> m_Sessions;
	};

//	CAeonView

class CAeonView
	{
	public:
		enum Flags
			{
			FLAG_EXCLUDE_MEMORY_ROWS =		0x00000001,
			FLAG_EXCLUDE_SEGMENTS =			0x00000002,
			};

		CAeonView (void);
		~CAeonView (void);

		CAeonView &operator= (const CAeonView &Obj) { CleanUp(); Copy(Obj); return *this; }

		bool CanInsert (const CRowKey &Path, CDatum dData, CString *retsError);
		void CloseRecovery (void);
		void CloseSegments (bool bMarkForDelete = false);
		bool CreateSecondaryRows (const CTableDimensions &PrimaryDims, CHexeProcess &Process, const CRowKey &PrimaryKey, CDatum dFullData, SEQUENCENUMBER RowID, CAeonRowArray *Rows);
		bool CreateSegment (const CString &sFilespec, SEQUENCENUMBER Seq, IOrderedRowSet *pRows, CAeonSegment **retpNewSeg, CString *retsError);
		CDatum DebugDump (void) const;
		bool GetData (const CRowKey &Path, CDatum *retData, SEQUENCENUMBER *retRowID, CString *retsError);
		inline const CTableDimensions &GetDimensions (void) { return m_Dims; }
		inline DWORD GetID (void) { return m_dwID; }
		inline const CString &GetName (void) { return m_sName; }
		inline DWORD GetRecoveryFileVersion (void) { return m_Recovery.GetVersion(); }
		inline const CAeonSegment &GetSegment (int iIndex) const { return *m_Segments[iIndex]; }
		inline int GetSegmentCount (void) { return m_Segments.GetCount(); }
		bool GetSegmentsToMerge (CAeonSegment **retpSeg1, CAeonSegment **retpSeg2);
		inline int GetUpdateCount (void) { return m_pRows->GetUpdateCount(); }
		inline bool HasRowID (void) { return !IsSecondaryView(); }
		inline bool HasUnsavedRows (void) { return (m_pRows->GetCount() > 0); }
		bool InitAsFileView (const CString &sRecoveryFilespec, int *retiRowsRecovered, CString *retsError);
		bool InitAsPrimaryView (CDatum dDesc, const CString &sRecoveryFilespec, int *retiRowsRecovered, CString *retsError);
		bool InitAsSecondaryView (CDatum dDesc, CHexeProcess &Process, const CString &sRecoveryFilespec, bool bForceUpdate, CString *retsError);
		bool InitIterator (CRowIterator *retIterator, DWORD dwFlags = 0);
		void Insert (const CTableDimensions &PrimaryDims, CHexeProcess &Process, const CRowKey &PrimaryKey, CDatum dData, CDatum dOldData, SEQUENCENUMBER RowID, bool *retbRecoveryFailed, CString *retsError = NULL);
		inline void InsertSegment (CAeonSegment *pSeg) { pSeg->SetDimensions(m_Dims); m_Segments.Insert(pSeg->GetSequence(), pSeg); }
		inline bool IsSecondaryView (void) { return (m_Keys.GetCount() > 0); }
		inline bool IsUpToDate (void) { return !m_bUpdateNeeded; }
		inline bool IsValid (void) const { return !m_bInvalid; }
		bool LoadRecoveryFile (const CString &sRecoveryFilespec, CAeonRowArray **retpRows, int *retiRowsRecovered, CString *retsError);
		void Mark (void);
		void SegmentMergeComplete (CAeonSegment *pSeg1, CAeonSegment *pSeg2, CAeonSegment *pNewSeg);
		void SegmentSaveComplete (CAeonSegment *pSeg);
		inline void SetID (DWORD dwID) { m_dwID = dwID; }
		void SetUnsavedRows (CAeonRowArray *pRows);
		inline void SetUpToDate (void) { m_bUpdateNeeded = false; }
		void WriteDesc (CComplexStruct *pDesc);

	private:
		void CleanUp (void);
		CDatum ComputeColumns (CHexeProcess &Process, CDatum dRowData);
		void Copy (const CAeonView &Src);
		void CreatePermutedKeys (const TArray<CDatum> &KeyData, int iDim, const TArray<CDatum> &PrevKey, SEQUENCENUMBER RowID, TArray<CRowKey> *retKeys);
		void CreateSecondaryData (const CTableDimensions &PrimaryDims, const CRowKey &PrimaryKey, CDatum dFullData, SEQUENCENUMBER RowID, CDatum *retdData);
		bool CreateSecondaryKeys (CHexeProcess &Process, CDatum dData, SEQUENCENUMBER RowID, TArray<CRowKey> *retKeys);
		bool InitRows (const CString &sRecoveryFilespec, int *retiRowsRecovered, CString *retsError);

		DWORD m_dwID;						//	ID of view
		CString m_sName;					//	Name of view

		CTableDimensions m_Dims;			//	Dimensions of view
		CAeonRowArray *m_pRows;				//	Rows
		TSortMap<SEQUENCENUMBER, CAeonSegment *> m_Segments;
		CRowInsertLog m_Recovery;			//	Recovery file
		bool m_bInvalid;					//	If TRUE, view is not valid.

		//	Used by secondary views only
		TArray<CDatum> m_Keys;				//	Fields to use as keys (one for each dimension)
		TArray<CString> m_Columns;			//	Fields to store (if empty, store primaryKey only)
		CDatum m_ComputedColumns;			//	Computed fields (may be nil)
		bool m_bExcludeNil;					//	If TRUE, rows with one or more nil keys are excluded
		bool m_bUsesListKeys;				//	If TRUE, we use list keys, which means more work

		//	Used when updating a view
		bool m_bUpdateNeeded;				//	If TRUE then we are updating the view
											//	to add segments saved before the given
											//	sequence number
	};

//	CAeonTable

struct STableDesc
	{
	CString sName;							//	Name of table
	CTableDimensions Dims;					//	Dimension descriptors
	};

class CCommitLog
	{
	public:

	private:
		
	};

class CAeonTable
	{
	public:
		enum Constants
			{
			DEFAULT_VIEW =				0,	//	Default view is always ID 0
			};

		enum Flags
			{
			FLAG_MORE_ROWS =			0x00000001,	//	GetRows: Start with first row AFTER key
			FLAG_INCLUDE_KEY =			0x00000002,	//	GetRows: Include key in data (instead of interleaving)
			FLAG_NO_KEY =				0x00000004,	//	GetRows: Just return the data
			};

		enum Types
			{
			typeStandard,					//	A standard table with no special properties
			typeFile,						//	A table for storing large files.
			};

		CAeonTable (void);
		~CAeonTable (void);

		bool Create (IArchonProcessCtx *pProcess, CMachineStorage *pStorage, CDatum dDesc, CString *retsError);
		bool DebugDumpView (DWORD dwViewID, CDatum *retdResult) const;
		bool Delete (void);
		bool DeleteView (DWORD dwViewID, CString *retsError);
		bool FileDirectory (const CString &sDirKey, CDatum dRequestedFields, CDatum dOptions, CDatum *retResult, CString *retsError);
		bool FindView (const CString &sView, DWORD *retdwViewID);
		bool FindViewAndPath (const CString &sView, DWORD *retdwViewID, CDatum dKey, CRowKey *retKey, CString *retsError);
		bool GetData (DWORD dwViewID, const CRowKey &Path, CDatum *retData, SEQUENCENUMBER *retRowID, CString *retsError);
		CDatum GetDesc (void);
		bool GetFileData (const CString &sFilePath, int iMaxSize, int iPos, const CDateTime &IfModifiedAfter, CDatum *retdFileDownloadDesc, bool bTranspace, CString *retsError);
		bool GetFileDesc (const CString &sFilePath, CDatum *retdFileDesc, CString *retsError);
		bool GetKeyRange (int iCount, CDatum *retdResult, CString *retsError);
		inline const CString &GetName (void) { return m_sName; }
		bool GetRows (DWORD dwViewID, CDatum dLastKey, int iRowCount, const TArray<int> &Limits, DWORD dwFlags, CDatum *retdResult, CString *retsError);
		inline Types GetType (void) const { return m_iType; }
		bool GetViewStatus (DWORD dwViewID, bool *retbUpToDate, CString *retsError);
		inline bool HasSecondaryViews (void) { return (m_Views.GetCount() > 1); }
		bool Housekeeping (DWORD dwMaxMemoryUse);
		bool InitIterator (DWORD dwViewID, CRowIterator *retIterator, CTableDimensions *retDims = NULL, CString *retsError = NULL);
		AEONERR Insert (const CRowKey &Path, CDatum dData, bool bInsertNew, CString *retsError);
		inline bool Insert (const CRowKey &Path, CDatum dData, CString *retsError) { return (Insert(Path, dData, false, retsError) == AEONERR_OK); }
		void Mark (void);
		AEONERR Mutate (const CRowKey &Path, CDatum dData, CDatum dMutateDesc, CDatum *retdResult, CString *retsError);
		bool OnVolumesChanged (const TArray<CString> &VolumesDeleted);
		bool Open (IArchonProcessCtx *pProcess, CMachineStorage *pStorage, const CString &sName, const TArray<CString> &Volumes, CString *retsError);
		bool ParseDimensionPath (const CString &sView, CDatum dPath, CRowKey *retPath, CString *retsError);
		bool ParseDimensionPathForCreate (CDatum dPath, CRowKey *retPath, CString *retsError);
		bool RecoverTableRows (CString *retsError);
		bool Recreate (IArchonProcessCtx *pProcess, CDatum dDesc, bool *retbUpdated, CString *retsError);
		bool Save (CString *retsError);
		AEONERR UploadFile (CMsgProcessCtx &Ctx, const CString &sSessionID, const CString &sFilePath, CDatum dUploadDesc, CDatum dData, int *retiComplete, CString *retsError);

		static CDatum GetDimensionDesc (SDimensionDesc &Dim);
		static CDatum GetDimensionDescForSecondaryView (SDimensionDesc &Dim, CDatum dKey);
		static CDatum GetDimensionPath (const CTableDimensions &Dims, const CString &sKey);
		static bool LastComponentOfKeyIsDirectory (const CString &sKey);
		static bool ParseDimensionDesc (CDatum dDimDesc, SDimensionDesc *retDimDesc, CString *retsError);
		static bool ParseDimensionDescForSecondaryView (CDatum dDimDesc, CHexeProcess &Process, SDimensionDesc *retDimDesc, CDatum *retdKey, CString *retsError);
		static bool ParseFilePath (const CString &sPath, CString *retsTable, CString *retsFilePath, CString *retsError);
		static bool ParseFilePathForCreate (const CString &sPath, CString *retsTable, CString *retsFilePath, CString *retsError);
		static CDatum PrepareFileDesc (const CString &sTable, const CString &sFilePath, CDatum dFileDesc, bool bTranspace = false);
		static bool ValidateTableName (const CString &sName);

	private:
		enum EHousekeepingState
			{
			stateReady,						//	We are ready for housekeeping
			stateCreatingSegment,			//	We are saving out a new segment
			stateMergingSegments,			//	We are merging two segments.
			stateBackup,					//	We are creating a backup.
			stateRestore,					//	We are restoring the primary from backup.
			stateUpdatingView,				//	We are updating a newly created secondary view.
			};

		void CloseSegments (bool bMarkForDelete = false);
		void CollectGarbage (void);
		bool CopyDirectory (const CString &sFromPath, const CString &sToPath, CString *retsError);
		bool CopyVolume (const CString &sFrom, const CString &sTo, CString *retsError);
		bool Create (const CString &sVolume, CDatum dDesc, CString *retsError);
		bool CreateCoreDirectories (const CString &sVolume, CDatum dDesc, CString *retsTablePath, CString *retsError);
		bool CreatePrimaryKey (const CTableDimensions &Dims, CDatum dMutateDesc, SEQUENCENUMBER RowID, CRowKey *retKey, CString *retsError);
		bool Delete (const CString &sVolume);
		bool DiffDesc (CDatum dDesc, TArray<CDatum> *retNewViews, CString *retsError);
		bool FindTableVolumes (TArray<CString> *retVolumes);
		bool FindView (const CString &sView, CAeonView **retpView);
		int FindVolumeToOpen (const TArray<CString> &Volumes, TArray<SEQUENCENUMBER> *retSeq = NULL);
		CString GetRecoveryFilespec (DWORD dwViewID);
		CString GetRecoveryFilespec (const CString &sTablePath, DWORD dwViewID);
		void GetSegmentFilespecs (const CString &sTablePath, TArray<CString> *retList);
		CString GetTableFilenamePrefix (void);
		bool GetTablePath (const CString &sVolume, CString *retsTablePath, CString *retsError);
		CString GetUniqueSegmentFilespec (CString *retsBackup);
		SEQUENCENUMBER GetVolumeSeq (const CString &sVolume);
		bool Init (const CString &sTablePath, CDatum dDesc, CString *retsError);
		bool MoveToScrap (const CString &sFilespec);
		bool OpenDesc (const CString &sFilespec, CDatum *retdDesc, CString *retsError);
		bool OpenSegments (const CString &sVolume, SEQUENCENUMBER *retHighSeq, CString *retsError);
		bool ParseTableType (const CString &sType, Types *retiType, CString *retsError);
		bool RecoveryBackup (void);
		bool RecoveryFailure (void);
		bool RecoveryRestore (void);
		bool RepairFromBackup (const TArray<CString> &Volumes, CString *retsError);
		bool RowExists (const CTableDimensions &Dims, CDatum dKey);
		bool SaveDesc (void);
		bool SaveDesc (CDatum dDesc, const CString &sFilespec, CString *retsError);
		bool ValidateVolume (const CString &sVolume, CString *retsError) const;

		static CDatum GetDimensionPathElement (EKeyTypes iKeyType, char **iopPos, char *pPosEnd);
		static void SetDimensionDesc (CComplexStruct *pDesc, const SDimensionDesc &Dim);

		IArchonProcessCtx *m_pProcess;		//	Process pointer
		CMachineStorage *m_pStorage;		//	Local storage

		CCriticalSection m_cs;
		CString m_sName;					//	Name of table
		Types m_iType;						//	Table type
		SEQUENCENUMBER m_Seq;				//	Current sequence number

		TSortMap<DWORD, CAeonView> m_Views;	//	Views

		CString m_sPrimaryVolume;			//	Primary volume where table lives
		CString m_sBackupVolume;			//	Backup volume
		bool m_bPrimaryLost;				//	If TRUE the primary volume is invalid or missing.
		bool m_bBackupLost;					//	If TRUE the backup volume is invalid or missing.
		bool m_bBackupNeeded;				//	If TRUE back volume is there, but empty (or invalid)
		bool m_bValidateBackup;				//	If TRUE validate the backup on next housekeeping.

		EHousekeepingState m_iHousekeeping;	//	If not stateReady then we are busy doing something.
		CAeonUploadSessions m_UploadSessions;
		int m_iRowsRecovered;				//	Number of rows recovered on open.

		CHexeProcess m_Process;				//	Hexe process for evaluation
	};

class CAeonEngine : public TSimpleEngine<CAeonEngine>
	{
	public:
		CAeonEngine (void);
		virtual ~CAeonEngine (void);

		bool GetViewStatus (const CString &sTable, DWORD dwViewID, bool *retbUpToDate, CString *retsError);
		inline void SetConsoleMode (const CString &sStorage) { m_sConsoleStorage = sStorage; m_bConsoleMode = true; }

		virtual CString ConsoleCommand (const CString &sCmd, const TArray<CDatum> &Args) override;

		static SMessageHandler m_MsgHandlerList[];
		static int m_iMsgHandlerListCount;

	protected:
		//	TSimpleEngine override
		virtual void OnBoot (void);
		virtual void OnMarkEx (void);
		virtual void OnStartRunning (void);
		virtual void OnStopRunning (void);

	private:
		//	Database commands
		bool CreateTable (const SArchonMessage &Msg, CDatum dDesc);
		bool CreateTable (CDatum dDesc, CAeonTable **retpTable, bool *retbExists, CString *retsError);
		bool FlushTableRows (void);
		bool GetKeyRange (const SArchonMessage &Msg, const CString &sTableName, int iCount);
		bool InitConsoleMode (const CString &sStoragePath, CString *retsError);
		bool Open (void);
		bool OpenLocalVolumes (void);
		bool OpenTableDefinitions (void);

		//	Message handlers
		void MsgCreateTable (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgDeleteTable (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgDeleteView (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgFileDirectory (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgFileDownload (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgFileGetDesc (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgFileUpload (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgFlushDb (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgGetData (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgGetKeyRange (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgGetRows (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgGetTables (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgGetViewInfo (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgHousekeeping (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgInsert (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgInsertNew (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgMutate (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgOnMachineStart (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgOnMnemosynthModified (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgRecoverTableTest (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgTranspaceDownload (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgWaitForView (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgWaitForVolume (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);

		//	Helper routines
		bool FindTable (const CString &sName, CAeonTable **retpTable);
		void GetTables (TArray<CAeonTable *> *retTables);
		bool ParseTableAndView (const SArchonMessage &Msg, 
								const CHexeSecurityCtx *pSecurityCtx, 
								CDatum dTableAndView, 
								CAeonTable **retpTable, 
								DWORD *retdwViewID,
								CDatum dKey = CDatum(),
								CRowKey *retKey = NULL);
		bool ValidateAdminAccess (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		bool ValidateTableAccess (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx, const CString &sTable);

		CCriticalSection m_cs;
		bool m_bMachineStarted = false;				//	TRUE if we've received Exarch.onMachineStart message
		bool m_bReady = false;						//	TRUE if we are serving requests
		bool m_bConsoleMode = false;				//	TRUE if we're in console mode
		DWORD m_dwMaxMemoryUse = 0;
		CMachineStorage m_LocalVolumes;
		TSortMap<CString, CAeonTable *> m_Tables;

		CString m_sConsoleStorage;					//	If in console mode, this is the root of our storage
	};
