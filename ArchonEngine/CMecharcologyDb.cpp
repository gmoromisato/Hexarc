//	CMecharcologyDb.cpp
//
//	CMecharcologyDb class
//	Copyright (c) 2010 by George Moromisato. All Rights Reserved.

#include "stdafx.h"

DECLARE_CONST_STRING(FIELD_ADDRESS,						"address")
DECLARE_CONST_STRING(FIELD_ID,							"id")
DECLARE_CONST_STRING(FIELD_FILESPEC,					"filespec")
DECLARE_CONST_STRING(FIELD_NAME,						"name")
DECLARE_CONST_STRING(FIELD_STATUS,						"status")
DECLARE_CONST_STRING(FIELD_VERSION,						"version")

DECLARE_CONST_STRING(STR_ERROR_MODULE_ALREADY_LOADED,	"%s has already been loaded.")
DECLARE_CONST_STRING(STR_ERROR_CANNOT_FIND_MODULE_PATH,	"Unable to find module %s.")
DECLARE_CONST_STRING(STR_ERROR_CANNOT_LAUNCH_MODULE,	"Unable to launch module %s : %s")

DECLARE_CONST_STRING(STR_ARCOLOGY_PRIME,				"Arcology Prime")
DECLARE_CONST_STRING(STR_MODULE_EXTENSION,				".exe")
DECLARE_CONST_STRING(STR_DEBUG_SWITCH,					" /debug")
DECLARE_CONST_STRING(STR_UNKNOWN_VERSION,				"unknown")

DECLARE_CONST_STRING(STR_MACHINE_STATUS_ACTIVE,			"running")
DECLARE_CONST_STRING(STR_MACHINE_STATUS_UNKNOWN,		"unknown")

DECLARE_CONST_STRING(STR_MODULE_STATUS_LAUNCHED,		"launched")
DECLARE_CONST_STRING(STR_MODULE_STATUS_ON_START,		"onStart")
DECLARE_CONST_STRING(STR_MODULE_STATUS_REMOVED,			"removed")
DECLARE_CONST_STRING(STR_MODULE_STATUS_RUNNING,			"running")
DECLARE_CONST_STRING(STR_MODULE_STATUS_UNKNOWN,			"unknown")

DECLARE_CONST_STRING(ERR_DUPLICATE_MACHINE,				"Duplicate machine address.")
DECLARE_CONST_STRING(ERR_CANT_JOIN,						"Unable to join another arcology.")

bool CMecharcologyDb::AddMachine (const CString &sDisplayName, const CString &sAddress, const CIPInteger &SecretKey, CString *retsError)

//	AddMachine
//
//	Adds an entry for this machine. We leave machine name and state empty 
//	because we haven't yet heard from the actual machine.

	{
	CSmartLock Lock(m_cs);
	int i;

	//	Make sure we don't have a machine with the same address or the same
	//	secret key.

	for (i = 0; i < m_Machines.GetCount(); i++)
		if (m_Machines[i].SecretKey == SecretKey
				|| strEquals(m_Machines[i].sAddress, sAddress))
			{
			*retsError = ERR_DUPLICATE_MACHINE;
			return false;
			}

	//	Add it

	SMachineEntry *pMachine = m_Machines.Insert();
	pMachine->iStatus = connectNone;
	pMachine->sAddress = sAddress;
	pMachine->sDisplayName = sDisplayName;
	pMachine->SecretKey = SecretKey;

	return true;
	}

bool CMecharcologyDb::AuthenticateMachine (const CString &sAuthName, const CIPInteger &AuthKey)

//	AuthenticateMachine
//
//	A machine wants to authenticate by giving us their secret key. We return 
//	TRUE if we can authenticate the machine. FALSE otherwise.

	{
	CSmartLock Lock(m_cs);
	int i;

	//	Look for a machine with this key

	for (i = 0; i < m_Machines.GetCount(); i++)
		if (m_Machines[i].SecretKey == AuthKey)
			{
			//	If this is not the same name, then we need to reload Mnemosynth,
			//	etc.

			if (!strEquals(m_Machines[i].sName, sAuthName))
				{
#ifdef DEBUG_STARTUP
				printf("[CMecharcologyDb::AuthenticateMachine]: Replacing entry %d from %s to %s\n", i, (LPSTR)m_Machines[i].sName, (LPSTR)sAuthName);
#endif
				//	Keep track of the old name so that we clean up Mnemosynth.
				//
				//	NOTE: For secondary machines, the first time we authenticate Arcology Prime,
				//	we replace blank with the actual name. Thus we only track an old name if it
				//	is valid.

				if (!m_Machines[i].sName.IsEmpty())
					m_OldMachines.Insert(m_Machines[i].sName);

				//	New name

				m_Machines[i].sName = sAuthName;
				m_Machines[i].iStatus = connectAuth;
				}

			//	Otherwise, if we're not yet connected, then we flip to auth mode.

			else if (m_Machines[i].iStatus == connectNone)
				{
				m_Machines[i].iStatus = connectAuth;
				}

			//	Success!

			return true;
			}

	//	Did not authenticate

	return false;
	}

void CMecharcologyDb::Boot (const SInit &Init)

//	Boot
//
//	Boot up the mecharcology with a single machine

	{
	CSmartLock Lock(m_cs);

	ASSERT(m_Machines.GetCount() == 0);

	m_sModulePath = Init.sModulePath;

	SetCurrentModule(Init.sCurrentModule);

	m_MachineDesc.sName = Init.sMachineName;
	m_MachineDesc.sAddress = Init.sMachineHost;

	//	If we've got an Arcology Prime address, then we add an entry.
	//	We only do this for secondary machines (Arcology Prime does not need
	//	to keep an entry for itself).

	if (!Init.sArcologyPrimeAddress.IsEmpty())
		{
		SMachineEntry *pPrime = m_Machines.Insert();
		pPrime->sName = NULL_STR;	//	We don't know the machine name yet, we'll get it during AUTH
		pPrime->iStatus = connectNone;
		pPrime->sAddress = Init.sArcologyPrimeAddress;
		pPrime->sDisplayName = STR_ARCOLOGY_PRIME;

		//	We won't know secret key until we load the config file (or until we 
		//	get a JOIN command). We won't know name until we get our first 
		//	message.

		m_iArcologyPrime = 0;
		}
	else
		m_iArcologyPrime = -1;
	}

bool CMecharcologyDb::DeleteMachine (const CString &sName)

//	DeleteMachine
//
//	Removes the given machine from the arcology. Returns TRUE if the machine was
//	deleted.

	{
	CSmartLock Lock(m_cs);

	int iMachine;
	if ((iMachine = FindMachine(sName)) == -1)
		return false;

	//	Delete the machine

	m_Machines.Delete(iMachine);
	return true;
	}

bool CMecharcologyDb::DeleteMachineByKey (const CIPInteger &Key)

//	DeleteMachineByKey
//
//	Delets the machine with the given secret key.

	{
	CSmartLock Lock(m_cs);
	int i;

	for (i = 0; i < m_Machines.GetCount(); i++)
		if (m_Machines[i].SecretKey == Key)
			{
			m_Machines.Delete(i);
			return true;
			}

	return false;
	}

bool CMecharcologyDb::DeleteModule (const CString &sName)

//	DeleteModule
//
//	Removes the module from the arcology. This is called when the process terminates.
//	Returns TRUE if the modules was deleted.

	{
	CSmartLock Lock(m_cs);

	int iModule;
	if ((iModule = FindModule(sName)) == -1)
		return false;

	//	Delete the module

	m_Modules.Delete(iModule);
	return true;
	}

bool CMecharcologyDb::FindArcologyPrime (SMachineDesc *retDesc)

//	FindArcologyPrime
//
//	Returns Arcology Prime, if we have proper keys

	{
	CSmartLock Lock(m_cs);

	if (m_iArcologyPrime == -1)
		return false;

	retDesc->sAddress = m_Machines[m_iArcologyPrime].sAddress;
	retDesc->sName = m_Machines[m_iArcologyPrime].sName;
	retDesc->Key = m_Machines[m_iArcologyPrime].SecretKey;

	return true;
	}

int CMecharcologyDb::FindMachine (const CString &sName)

//	FindMachine
//
//	Returns the index of the given machine (or -1 if not found).
//	This must be used inside a lock.

	{
	int i;

	CString sNameToFind = strToLower(sName);

	for (i = 0; i < m_Machines.GetCount(); i++)
		if (strEquals(sNameToFind, strToLower(m_Machines[i].sName)))
			return i;

	return -1;
	}

bool CMecharcologyDb::FindMachineByAddress (const CString &sAddress, SMachineDesc *retDesc)

//	FindMachineByAddress
//
//	Looks for the given machine by address.

	{
	CSmartLock Lock(m_cs);
	int i;

	for (i = 0; i < m_Machines.GetCount(); i++)
		if (strEquals(sAddress, m_Machines[i].sAddress))
			{
			if (retDesc)
				{
				retDesc->sAddress = m_Machines[i].sAddress;
				retDesc->sName = m_Machines[i].sName;
				retDesc->Key = m_Machines[i].SecretKey;
				}

			return true;
			}

	return false;
	}

bool CMecharcologyDb::FindMachineByName (const CString &sName, SMachineDesc *retDesc)

//	FindMachineByName
//
//	Looks for the given machine by name.

	{
	CSmartLock Lock(m_cs);
	int i;

	if (sName.IsEmpty())
		return FindArcologyPrime(retDesc);

	for (i = 0; i < m_Machines.GetCount(); i++)
		if (strEquals(sName, m_Machines[i].sName))
			{
			if (retDesc)
				{
				retDesc->sAddress = m_Machines[i].sAddress;
				retDesc->sName = m_Machines[i].sName;
				retDesc->Key = m_Machines[i].SecretKey;
				}

			return true;
			}

	return false;
	}

int CMecharcologyDb::FindModule (const CString &sName) const

//	FindModule
//
//	Returns the index of the given module (or -1 if not found).
//	This must be used inside a lock.

	{
	int i;

	CString sNameToFind = strToLower(sName);

	for (i = 0; i < m_Modules.GetCount(); i++)
		if (strEquals(sNameToFind, strToLower(m_Modules[i].sName)))
			return i;

	return -1;
	}

bool CMecharcologyDb::FindModuleByFilespec (const CString &sFilespec, SModuleDesc *retDesc)

//	FindModuleByFilespec
//
//	Finds the module

	{
	CSmartLock Lock(m_cs);
	int i;

	for (i = 0; i < m_Modules.GetCount(); i++)
		if (fileIsPathEqual(sFilespec, m_Modules[i].sFilespec))
			{
			if (retDesc)
				{
				retDesc->sName = m_Modules[i].sName;
				retDesc->sFilespec = m_Modules[i].sFilespec;
				retDesc->sVersion = m_Modules[i].sVersion;
				retDesc->sStatus = StatusToID(m_Modules[i].iStatus);
				retDesc->dwProcessID = m_Modules[i].hProcess.GetID();
				}

			return true;
			}

	return false;
	}

bool CMecharcologyDb::GetCentralModule (SModuleDesc *retDesc)

//	GetCentralModule
//
//	Returns the process information (NOTE: hProcess is not initialized)

	{
	CSmartLock Lock(m_cs);

	//	Central module is always module 0

	if (m_Modules.GetCount() == 0)
		return false;

	if (retDesc)
		{
		retDesc->sName = m_Modules[0].sName;
		retDesc->sFilespec = m_Modules[0].sFilespec;
		retDesc->sVersion = m_Modules[0].sVersion;
		retDesc->sStatus = StatusToID(m_Modules[0].iStatus);
		retDesc->dwProcessID = m_Modules[0].hProcess.GetID();
		}

	return true;
	}

bool CMecharcologyDb::GetMachine (const CString &sName, SMachineDesc *retDesc)

//	GetMachine
//
//	Returns machine information. Returns FALSE if the machine cannot be found

	{
	CSmartLock Lock(m_cs);

	//	If sName is blank then we return the current machine

	if (sName.IsEmpty())
		{
		if (retDesc)
			*retDesc = m_MachineDesc;
		return true;
		}

	//	Find the machine by name

	int iMachine;
	if ((iMachine = FindMachine(sName)) == -1)
		return false;

	if (retDesc)
		{
		retDesc->sName = m_Machines[iMachine].sName;
		retDesc->sAddress = m_Machines[iMachine].sAddress;
		retDesc->Key = m_Machines[iMachine].SecretKey;
		}

	return true;
	}

bool CMecharcologyDb::GetMachine (int iIndex, SMachineDesc *retDesc) const

//	GetMachine
//
//	Returns the machine information.

	{
	CSmartLock Lock(m_cs);

	if (iIndex < 0 || iIndex >= m_Machines.GetCount())
		return false;

	retDesc->sName = m_Machines[iIndex].sName;
	retDesc->sAddress = m_Machines[iIndex].sAddress;
	retDesc->Key = m_Machines[iIndex].SecretKey;

	return true;
	}

bool CMecharcologyDb::GetModule (const CString &sName, SModuleDesc *retDesc)

//	GetModule
//
//	Returns the process information (NOTE: hProcess is not initialized)

	{
	CSmartLock Lock(m_cs);

	//	Find the module by name

	int iModule;
	if ((iModule = FindModule(sName)) == -1)
		return false;

	if (retDesc)
		{
		retDesc->sName = m_Modules[iModule].sName;
		retDesc->sFilespec = m_Modules[iModule].sFilespec;
		retDesc->sVersion = m_Modules[iModule].sVersion;
		retDesc->sStatus = StatusToID(m_Modules[iModule].iStatus);
		retDesc->dwProcessID = m_Modules[iModule].hProcess.GetID();
		}

	return true;
	}

CDatum CMecharcologyDb::GetModuleList (void) const

//	GetModuleList
//
//	Returns an array of module info

	{
	CSmartLock Lock(m_cs);
	int i;

	CComplexArray *pArray = new CComplexArray;
	for (i = 0; i < m_Modules.GetCount(); i++)
		{
		CComplexStruct *pModuleInfo = new CComplexStruct;

		pModuleInfo->SetElement(FIELD_NAME, m_Modules[i].sName);
		pModuleInfo->SetElement(FIELD_FILESPEC, m_Modules[i].sFilespec);
		pModuleInfo->SetElement(FIELD_VERSION, m_Modules[i].sVersion);
		pModuleInfo->SetElement(FIELD_STATUS, StatusToID(m_Modules[i].iStatus));

		//	Add to list

		pArray->Append(CDatum(pModuleInfo));
		}

	//	Done

	return CDatum(pArray);
	}

CProcess *CMecharcologyDb::GetModuleProcess (const CString &sName)

//	GetModuleProcess
//
//	Returns the module's process (or NULL if not found)

	{
	CSmartLock Lock(m_cs);

	//	Find the module by name

	int iModule;
	if ((iModule = FindModule(sName)) == -1)
		return NULL;

	return &m_Modules[iModule].hProcess;
	}

CTimeSpan CMecharcologyDb::GetModuleRunTime (const CString &sName)

//	GetModuleRunTime
//
//	Return the time that the module has been running.

	{
	CSmartLock Lock(m_cs);

	//	Find the module by name

	int iModule;
	if ((iModule = FindModule(sName)) == -1)
		return CTimeSpan();

	return timeSpan(m_Modules[iModule].StartTime, CDateTime(CDateTime::Now));
	}

bool CMecharcologyDb::IsModuleRemoved (const CString &sName) const

//	IsModuleRemove
//
//	Returns TRUE if the module has been removed

	{
	CSmartLock Lock(m_cs);

	//	Find the module by name

	int iModule;
	if ((iModule = FindModule(sName)) == -1)
		return true;

	return (m_Modules[iModule].iStatus == moduleRemoved);
	}

bool CMecharcologyDb::GetStatus (CDatum *retStatus)

//	GetStatus
//
//	Returns the status

	{
	CSmartLock Lock(m_cs);
	int i;

	CComplexArray *pList = new CComplexArray;
	CDatum dList(pList);

	//	Add Arcology Prime as the first

	if (m_iArcologyPrime == -1)
		{
		CComplexStruct *pData = new CComplexStruct;
		pData->SetElement(FIELD_ID, m_MachineDesc.sName);
		pData->SetElement(FIELD_NAME, STR_ARCOLOGY_PRIME);
		pData->SetElement(FIELD_ADDRESS, m_MachineDesc.sAddress);
		pData->SetElement(FIELD_STATUS, STR_MACHINE_STATUS_ACTIVE);
		dList.Append(CDatum(pData));
		}

	//	Add all machines

	for (i = 0; i < m_Machines.GetCount(); i++)
		{
		const SMachineEntry &Entry = m_Machines[i];
		CComplexStruct *pData = new CComplexStruct;

		pData->SetElement(FIELD_ID, Entry.sName);
		pData->SetElement(FIELD_NAME, Entry.sDisplayName);
		pData->SetElement(FIELD_ADDRESS, Entry.sAddress);

		switch (Entry.iStatus)
			{
			case connectActive:
				pData->SetElement(FIELD_STATUS, STR_MACHINE_STATUS_ACTIVE);
				break;

			default:
				pData->SetElement(FIELD_STATUS, STR_MACHINE_STATUS_UNKNOWN);
			}

		//	Add to list

		dList.Append(CDatum(pData));
		}

	//	Done

	*retStatus = dList;
	return true;
	}

bool CMecharcologyDb::HasArcologyKey (void) const

//	HasArcologyKey
//
//	Returns TRUE if we have the keys to be part of an arcology

	{
	CSmartLock Lock(m_cs);

	//	If we're Arcology Prime, then we have a key

	if (m_iArcologyPrime == -1)
		return true;

	//	Make sure we have a key

	const SMachineEntry &Prime = m_Machines[m_iArcologyPrime];
	if (Prime.SecretKey.IsEmpty())
		return false;

	//	Done

	return true;
	}

bool CMecharcologyDb::JoinArcology (const CString &sPrimeName, const CIPInteger &PrimeKey, CString *retsError)

//	JoinArcology
//
//	We've been asked by Arcology Prime to join.

	{
	CSmartLock Lock(m_cs);

	if (m_iArcologyPrime == -1
			|| m_iArcologyPrime >= m_Machines.GetCount())
		{
		*retsError = ERR_CANT_JOIN;
		return false;
		}

	SMachineEntry &Prime = m_Machines[m_iArcologyPrime];
	if (!Prime.SecretKey.IsEmpty())
		{
		*retsError = ERR_CANT_JOIN;
		return false;
		}

	Prime.sName = sPrimeName;
	Prime.SecretKey = PrimeKey;

	return true;
	}

bool CMecharcologyDb::LoadModule (const CString &sFilespec, bool bDebug, CString *retsName, CString *retsError)

//	LoadModule
//
//	Loads the given module. Returns TRUE if successful.

	{
	CSmartLock Lock(m_cs);

	//	Get a module name by taking just the filename (without extension)

	CString sModuleName;
	fileGetExtension(fileGetFilename(sFilespec), &sModuleName);

	//	Don't load the module if it has already been loaded

	if (FindModule(sModuleName) != -1)
		{
		//	LATER: Due to a race condition, this module might have already terminated.
		//	Check to see if it has, and if so, just replace the module.

		*retsError = strPattern(STR_ERROR_MODULE_ALREADY_LOADED, sModuleName);
		return false;
		}

	//	Make sure we have an extension

	CString sModule = fileAppendExtension(sFilespec, STR_MODULE_EXTENSION);

	//	Generate a path using the module directory

	CString sModuleFilespec = fileAppend(m_sModulePath, sModule);

	//	Make sure it exists

	if (!fileExists(sModuleFilespec))
		{
		*retsError = strPattern(STR_ERROR_CANNOT_FIND_MODULE_PATH, sModule);
		return false;
		}

	//	Create a module entry

	int iIndex = m_Modules.GetCount();
	SModuleEntry *pModule = m_Modules.Insert();
	pModule->sName = sModuleName;
	pModule->sFilespec = sModule;
	pModule->StartTime = CDateTime(CDateTime::Now);

	//	Get the version

	SFileVersionInfo VersionInfo;
	if (fileGetVersionInfo(sModuleFilespec, &VersionInfo))
		pModule->sVersion = VersionInfo.sProductVersion;
	else
		pModule->sVersion = STR_UNKNOWN_VERSION;

	//	Generate the command line

	CString sCmdLine;
	if (bDebug)
		sCmdLine = strPattern("%s /machine:%s /debug", sModuleFilespec, m_MachineDesc.sName);
	else
		sCmdLine = strPattern("%s /machine:%s", sModuleFilespec, m_MachineDesc.sName);

	//	Create the process

	try
		{
		pModule->hProcess.Create(sCmdLine);
		}
	catch (CException e)
		{
		m_Modules.Delete(iIndex);

		*retsError = strPattern(STR_ERROR_CANNOT_LAUNCH_MODULE, sModule, e.GetErrorString());
		return false;
		}

	//	Set state

	pModule->iStatus = moduleLaunched;

	//	Return the name of the module

	*retsName = sModuleName;

	return true;
	}

bool CMecharcologyDb::OnCompleteAuth (const CString &sName)

//	OnCompleteAuth
//
//	Returns TRUE if we need to complete the authentication by adding an 
//	endpoint to Mnemosynth.

	{
	CSmartLock Lock(m_cs);
	int i;

	//	Look for a machine by name

	for (i = 0; i < m_Machines.GetCount(); i++)
		if (strEquals(m_Machines[i].sName, sName))
			{
			//	If we're still in connectAuth mode, then we return TRUE

			if (m_Machines[i].iStatus == connectAuth)
				{
				m_Machines[i].iStatus = connectActive;
				return true;
				}

			//	Otherwise, we return false (because we've already completed this
			//	step).

			return false;
			}

	//	If we did not find the machine, then we can't complete auth

	return false;
	}

void CMecharcologyDb::OnModuleStart (const CString &sName, DWORD dwMnemosynthSeq, bool *retbAllComplete)

//	OnModuleStart
//
//	The given module has started running

	{
	CSmartLock Lock(m_cs);
	int i;

	int iModule;
	if ((iModule = FindModule(sName)) == -1)
		return;

	m_Modules[iModule].iStatus = moduleOnStart;
	m_Modules[iModule].dwSeq = dwMnemosynthSeq;

	//	See if all modules have completed this

	*retbAllComplete = true;
	for (i = 0; i < m_Modules.GetCount(); i++)
		if (m_Modules[i].iStatus != moduleOnStart && m_Modules[i].iStatus != moduleRunning)
			{
			*retbAllComplete = false;
			break;
			}
	}

void CMecharcologyDb::OnMnemosynthUpdated (void)

//	OnMnemosynthUpdated
//
//	All modules are now running.

	{
	CSmartLock Lock(m_cs);
	int i;

	for (i = 0; i < m_Modules.GetCount(); i++)
		{
		if (m_Modules[i].iStatus == moduleOnStart)
			m_Modules[i].iStatus = moduleRunning;
		}
	}

bool CMecharcologyDb::ProcessOldMachines (TArray<CString> &OldMachines)

//	ProcessOldMachines
//
//	Returns a list of old machine names and clears the internal list. The caller
//	should remove the machines from Mnemosynth. We return TRUE if there are any
//	machines in the list.

	{
	CSmartLock Lock(m_cs);

	OldMachines = m_OldMachines;
	m_OldMachines.DeleteAll();

	return (OldMachines.GetCount() > 0);
	}

bool CMecharcologyDb::SetArcologyKey (const CIPInteger &PrimeKey, CString *retsError)

//	SetArcologyKey
//
//	Sets the key

	{
	CSmartLock Lock(m_cs);

	if (m_iArcologyPrime == -1
			|| m_iArcologyPrime >= m_Machines.GetCount())
		{
		*retsError = ERR_CANT_JOIN;
		return false;
		}

	SMachineEntry &Prime = m_Machines[m_iArcologyPrime];
	if (!Prime.SecretKey.IsEmpty())
		{
		*retsError = ERR_CANT_JOIN;
		return false;
		}

	Prime.SecretKey = PrimeKey;

	return true;
	}

void CMecharcologyDb::SetCurrentModule (const CString &sName)

//	SetCurrentModule
//
//	Sets the current module. This is only ever called by Exarch when
//	it is booting. [Remember that only the CentralModule has an
//	Exarch, and thus a mecharcology.]

	{
	CSmartLock Lock(m_cs);

	ASSERT(m_Modules.GetCount() == 0);

	SModuleEntry *pModule = m_Modules.Insert();
	pModule->sName = sName;
	pModule->sFilespec = fileGetFilename(fileGetExecutableFilespec());
	pModule->hProcess.CreateCurrentProcess();

	//	Get the version

	SFileVersionInfo VersionInfo;
	if (fileGetVersionInfo(fileGetExecutableFilespec(), &VersionInfo))
		pModule->sVersion = VersionInfo.sProductVersion;
	else
		pModule->sVersion = STR_UNKNOWN_VERSION;

	//	CentralModule doesn't need an OnStart because it launches
	//	all other modules. We set our state to OnStart so that the loop
	//	in OnModuleStart can see that all modules (including this one)
	//	have started.

	pModule->iStatus = moduleOnStart;

	//	We don't yet know the sequence number to check for; we will
	//	figure it out in OnModuleStart

	pModule->dwSeq = 0;
	}

bool CMecharcologyDb::SetHostAddress (const CString &sName, const CString &sHostAddress)

//	SetHostAddress
//
//	Sets the host address for the given machine.

	{
	CSmartLock Lock(m_cs);

	//	If NULL_STR then we mean the current machine

	if (sName.IsEmpty())
		{
		m_MachineDesc.sAddress = sHostAddress;
		return true;
		}

	//	Find the machine

	int iMachine;
	if ((iMachine = FindMachine(sName)) == -1)
		return false;

	m_Machines[iMachine].sAddress = sHostAddress;

	return true;
	}

void CMecharcologyDb::SetModuleRemoved (const CString &sName)

//	SetModuleRemove
//
//	Marks the module as having been removed.

	{
	CSmartLock Lock(m_cs);

	//	Find the module by name

	int iModule;
	if ((iModule = FindModule(sName)) == -1)
		return;

	m_Modules[iModule].iStatus = moduleRemoved;
	}

CString CMecharcologyDb::StatusToID (EModuleStates iStatus) const

//	StatusToID
//
//	Convert to a status string.

	{
	switch (iStatus)
		{
		case moduleLaunched:
			return STR_MODULE_STATUS_LAUNCHED;

		case moduleOnStart:
			return STR_MODULE_STATUS_ON_START;

		case moduleRemoved:
			return STR_MODULE_STATUS_REMOVED;

		case moduleRunning:
			return STR_MODULE_STATUS_RUNNING;

		default:
			return STR_MODULE_STATUS_UNKNOWN;
		}
	}
