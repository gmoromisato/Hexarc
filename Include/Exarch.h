//	Exarch.h
//
//	Exarch Archon Implementation
//	Copyright (c) 2010 by George Moromisato. All Rights Reserved.

#pragma once

//	Arcology Membership --------------------------------------------------------

struct SMachineDesc
	{
	CString sName;								//	Unique name of machine
	CString sAddress;							//	Address to reach machine
	CIPInteger Key;								//	Machine key
	};

struct SModuleDesc
	{
	CString sName;								//	Module name (unique in machine)
	CString sFilespec;							//	Absolute filespec to executable
	CString sVersion;							//	Module version (from .exe)
	CString sStatus;							//	Current status
	DWORD dwProcessID;							//	ID of process
	};

class CMecharcologyDb
	{
	public:
		struct SInit
			{
			CString sMachineName;
			CString sMachineHost;

			CString sModulePath;
			CString sCurrentModule;

			CString sArcologyPrimeAddress;
			};

		void Boot (const SInit &Init);
		CDatum GetModuleList (void) const;
		bool SetHostAddress (const CString &sName, const CString &sHostAddress);

		//	Machine functions

		bool AddMachine (const CString &sDisplayName, const CString &sAddress, const CIPInteger &SecretKey, CString *retsError);
		bool AuthenticateMachine (const CString &sAuthName, const CIPInteger &AuthKey);
		bool DeleteMachine (const CString &sName);
		bool DeleteMachineByKey (const CIPInteger &Key);
		bool FindArcologyPrime (SMachineDesc *retDesc = NULL);
		bool FindMachineByAddress (const CString &sAddress, SMachineDesc *retDesc = NULL);
		bool FindMachineByName (const CString &sName, SMachineDesc *retDesc = NULL);
		bool GetMachine (const CString &sName, SMachineDesc *retDesc = NULL);
		bool GetMachine (int iIndex, SMachineDesc *retDesc) const;
		inline int GetMachineCount (void) const { return m_Machines.GetCount(); }
		bool GetStatus (CDatum *retStatus);
		bool HasArcologyKey (void) const;
		bool JoinArcology (const CString &sPrimeName, const CIPInteger &PrimeKey, CString *retsError);
		bool OnCompleteAuth (const CString &sName);
		bool ProcessOldMachines (TArray<CString> &OldMachines);
		bool SetArcologyKey (const CIPInteger &PrimeKey, CString *retsError);

		//	Module functions

		bool DeleteModule (const CString &sName);
		bool FindModuleByFilespec (const CString &sFilespec, SModuleDesc *retDesc = NULL);
		bool GetCentralModule (SModuleDesc *retDesc = NULL);
		bool GetModule (const CString &sName, SModuleDesc *retDesc = NULL);
		inline int GetModuleCount (void) { CSmartLock Lock(m_cs); return m_Modules.GetCount(); }
		inline const CString &GetModuleName (int iIndex) { CSmartLock Lock(m_cs); return m_Modules[iIndex].sName; }
		CProcess *GetModuleProcess (const CString &sName);
		inline CProcess &GetModuleProcess (int iIndex) { CSmartLock Lock(m_cs); return m_Modules[iIndex].hProcess; }
		CTimeSpan GetModuleRunTime (const CString &sName);
		inline DWORD GetModuleSeq (int iIndex) { CSmartLock Lock(m_cs); return m_Modules[iIndex].dwSeq; }
		bool IsModuleRemoved (const CString &sName) const;
		bool LoadModule (const CString &sFilespec, bool bDebug, CString *retsName, CString *retsError);
		void OnMnemosynthUpdated (void);
		void OnModuleStart (const CString &sName, DWORD dwMnemosynthSeq, bool *retbAllComplete);
		void SetModuleRemoved (const CString &sName);

	private:
		enum EConnectStates
			{
			connectNone,						//	No connection

			connectAuth,						//	Authenticated, but not yet fully connected
			connectActive,						//	Connection is active
			};

		struct SMachineEntry
			{
			CString sName;						//	Machine name (generated at connect time)
			CString sDisplayName;				//	Name assigned by user
			CString sAddress;					//	Machine address
			CIPInteger SecretKey;				//	Shared secret

			EConnectStates iStatus;				//	Status of our connection to the machine
			};

		enum EModuleStates
			{
			moduleLaunched,						//	Process has been launched
			moduleOnStart,						//	We've received an OnModuleStart
			moduleRunning,						//	Module is running and we have its initial Mnemosynth state
			moduleRemoved,						//	Module has been removed
			};

		struct SModuleEntry
			{
			CString sName;						//	Module name (unique in machine)
			CString sFilespec;					//	Absolute filespec to executable

			CProcess hProcess;					//	Running process
			EModuleStates iStatus;				//	Current module status
			DWORD dwSeq;						//	Seq (used during startup)
			CString sVersion;					//	Module version
			CDateTime StartTime;				//	Time when module started
			};

		int FindMachine (const CString &sName);
		int FindModule (const CString &sName) const;
		void SetCurrentModule (const CString &sName);
		CString StatusToID (EModuleStates iStatus) const;

		CCriticalSection m_cs;

		CString m_sModulePath;					//	Path of modules

		SMachineDesc m_MachineDesc;				//	Data about our machine
		int m_iArcologyPrime;					//	Index of Arcology Prime in m_Machines
		TArray<SMachineEntry> m_Machines;		//	List of machines and our connection status
		TArray<SModuleEntry> m_Modules;			//	List of modules on this machine (0 is
												//		CentralModule)
		TArray<CString> m_OldMachines;			//	Stale machine names
	};

//	CSessionManager ------------------------------------------------------------

enum SessionTypes
	{
	stypeUnknown,
	stypeArcology,								//	Connected to another member of the arcology
	};

class CArcologySession : public ISessionCtx
	{
	public:
		CArcologySession (SOCKET hSocket) : ISessionCtx(hSocket) { }

		virtual int GetType (void) { return stypeArcology; }
	};

//	CExarchEngine --------------------------------------------------------------

class CExarchEngine : public TSimpleEngine<CExarchEngine>, public IArchonExarch
	{
	public:
		struct SOptions
			{
			SOptions (void) :
					dwAMP1Port(0)
				{ }

			CString sArcologyPrime;			//	If set, then we should connect to this arcology
			CString sConfigFilename;		//	Name of config file (default to "Config.ars")
			DWORD dwAMP1Port;				//	Port to listen to for AMP1 messages
			};

		CExarchEngine (const SOptions &Options);
		virtual ~CExarchEngine (void);

		bool SendAMP1Command (const CString &sMachineName, const CString &sCommand, CDatum dData);

		static SMessageHandler m_MsgHandlerList[];
		static int m_iMsgHandlerListCount;

		//	IArchonExarch

		virtual bool ExarchAuthenticateMachine (const CString &sMachineName, const CIPInteger &Key);

	protected:
		//	TSimpleEngine override
		virtual void OnBoot (void);
		virtual void OnMarkEx (void);
		virtual void OnStartRunning (void);
		virtual void OnStopRunning (void);

	private:
		//	Message handlers
		void MsgAddMachine (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgAddModule (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgAddVolume (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgCompleteUpgrade (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgCreateTestVolume (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgDeleteTestDrive (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgEsperOnAMP1 (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgGetLogRows (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgGetMachineStatus (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgGetModuleList (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgGetStatus (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgGetStorageList (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgMnemosynthEndpointList (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgMnemosynthOnArcologyUpdated (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgMnemosynthRead (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgOnLog (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgOnModuleStart (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgOnModuleStop (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgRemoveModule (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgRemoveVolume (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgReportDiskError (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgRequestUpgrade (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgRestartMachine (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgRestartModule (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgSendToMachine (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);
		void MsgUploadUpgrade (const SArchonMessage &Msg, const CHexeSecurityCtx *pSecurityCtx);

		//	AMP1
		void AMP1Auth (CDatum dData, CDatum dConnection);
		void AMP1Join (CDatum dData);
		void AMP1Ping (const CString &sSender);
		void AMP1Rejoin (void);
		void AMP1Send (CDatum dData);
		void AMP1Welcome (void);

		//	AI1 Commands
		bool AddMachineConnection (const CString &sHostname, CString *retsError);
		bool AddModule (const CString &sModuleInput, bool bDebug, CString *retsModuleName, CString *retsError);
		bool AddStorage (const CString &sName, const CString &sLocalPath, DWORD dwQuota, const CString &sTestDrive, CString *retsError);
		bool DeleteStorage (const CString &sVolume, CString *retsError);
		bool CreateArcology (const CString &sName, const CString &sHostAddress, CString *retsError);
		bool RemoveModule (const CString &sModuleName, CString *retsError);

		//	Other helper methods
		void CleanUpUpgrade (void);
		bool CheckArcology (CString *retsError);
		void DeleteMachineResources (const CString &sName);
		void DeleteModuleResources (const CString &sName, bool bDeleteModule = false);
		void ExecuteProtocol (ISessionCtx *pCtx, const CString &sInput, CString *retsOutput);
		void ExecuteProtocolAMP1 (CArcologySession *pCtx, const CString &sInput, DWORD dwVersion, CString *retsOutput);
		bool FindVolume (const CString &sVolume, CDatum *retdVolumeDesc = NULL, CString *retsKey = NULL);
		CString GenerateResourceNameFromFilespec (const CString &sFilespec);
		CString GetMachineStatus (void);
		inline bool IsArcologyPrime (void) const { return m_sArcologyPrime.IsEmpty(); }
		bool IsRestartRequired (const CString &sFilename);
		void OnMachineConnection (const CString &sName);
		void OnMachineStart (void);
		bool ParseMachineName (const CString &sValue, CString *retsMachineName) const;
		bool ParseModuleName (const CString &sValue, CString *retsMachineName, CString *retsModuleName) const;
		SessionTypes ParseProtocol (const CString &sInput, CString *retsCommand);
		bool ReadConfig (void);
		void RegisterForMnemosynthUpdate (void);
		bool RestartModule (const CString &sFilename, CString *retsError);
		bool SendAMP1Command (const SMachineDesc &Desc, const CString &sCommand, CDatum dData);
		void SendReadRequest (CDatum dSocket);
		void SendWriteRequest (CDatum dSocket, const CString &sData);
		void SetMachineData (const CString &sHostAddress, const CString &sStatus);
		bool TestFileRead (const CString &sFilespec);
		bool WriteConfig (void);

		//	Storage
		bool CreateStorageConfig (void);
		bool LoadStorageConfig (CDatum dStorageConfig);
		bool WriteStorageConfig (void);

		CCriticalSection m_cs;
		CString m_sConfigFilename;				//	Filename to load configuration from

		CString m_sArcologyPrime;				//	Address for Arcology Prime machine (if NULL, then we are Arcology Prime)
		CIPInteger m_ArcologyKey;				//	Key to use to communicate
		DWORD m_dwAMP1Port;						//	Port on which we listen to AMP1 commands

		CDatum m_dMachineConfig;				//	Configuration for this machine
		CMecharcologyDb m_MecharcologyDb;		//	Machine arcology database
		CSessionManagerOld m_Sessions;			//	Keeping track of sessions
		CProxyPort *m_pLoggingPort;

		DWORD m_dwNextVolume;					//	Next volume number
		TArray<CString> m_DeletedVolumes;		//	Keep track of deleted volumes (for UI purposes)
		bool m_bInStopRunning;					//	If TRUE, we're shutting down the machine
	};
