/*
 Copyright (C) 2009-2011 wxLauncher Team
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "generated/configure_launcher.h"
#include "apis/FlagListManager.h"
#include "apis/ProfileManager.h"
#include "datastructures/FSOExecutable.h"
#include "tabs/AdvSettingsPage.h" // TODO remove this include once the proxy is working
#include "global/ids.h"

#include "global/MemoryDebugging.h"

/** \class FlagListManager
 FlagListManager is used to notify controls that have registered with it
 that the flag file processing's status has changed. It also extracts
 the data in flag files generated by FreeSpace 2 Open executables. */

DEFINE_EVENT_TYPE(EVT_FLAG_FILE_PROCESSING_STATUS_CHANGED);

#include <wx/listimpl.cpp> // required magic incantation
WX_DEFINE_LIST(FlagFileProcessingEventHandlers);

#include <wx/arrimpl.cpp> // Magic Incantation
WX_DEFINE_OBJARRAY(FlagFileArray);

void FlagListManager::RegisterFlagFileProcessingStatusChanged(wxEvtHandler *handler) {
	this->flagFileProcessingStatusChangedHandlers.Append(handler);
}

void FlagListManager::UnRegisterFlagFileProcessingStatusChanged(wxEvtHandler *handler) {
	this->flagFileProcessingStatusChangedHandlers.DeleteObject(handler);
}

void FlagListManager::GenerateFlagFileProcessingStatusChanged(const FlagFileProcessingStatus& status) {
	wxCommandEvent event(EVT_FLAG_FILE_PROCESSING_STATUS_CHANGED, wxID_NONE);
	event.SetInt(status);

	// FIXME uncomment after removing flag file processing from flag list (until then, the assert will trigger)
//	if (status == FLAG_FILE_PROCESSING_OK) {
//		wxCHECK_RET(this->data != NULL, _T("Attempted to get flag file data after processing succeded but was NULL."));
//		event.SetExtraLong(static_cast<long>(this->data->GetItemCount()));
//	} else {
//		event.SetExtraLong(1); // item count of 1 for the error message that should appear in the flag list box
//	}

	wxLogDebug(_T("Generating EVT_FLAG_FILE_PROCESSING_STATUS_CHANGED event"));
	for (FlagFileProcessingEventHandlers::iterator
		 iter = this->flagFileProcessingStatusChangedHandlers.begin(),
		 end = this->flagFileProcessingStatusChangedHandlers.end(); iter != end; ++iter) {
		wxEvtHandler* current = *iter;
		current->AddPendingEvent(event);
		wxLogDebug(_T(" Sent EVT_FLAG_FILE_PROCESSING_STATUS_CHANGED event to %p"), &(*iter));
	}
}

FlagListManager* FlagListManager::flagListManager = NULL;

bool FlagListManager::Initialize() {
	wxASSERT(!FlagListManager::IsInitialized());
	
	FlagListManager::flagListManager = new FlagListManager();
	return true;
}

void FlagListManager::DeInitialize() {
	wxASSERT(FlagListManager::IsInitialized());
	
	FlagListManager* temp = FlagListManager::flagListManager;
	FlagListManager::flagListManager = NULL;
	delete temp;
}

bool FlagListManager::IsInitialized() {
	return (FlagListManager::flagListManager != NULL); 
}

FlagListManager* FlagListManager::GetFlagListManager() {
	wxCHECK_MSG(FlagListManager::IsInitialized(),
		NULL,
		_T("Attempt to get flag list manager when it has not been initialized."));
	
	return FlagListManager::flagListManager;
}

FlagListManager::FlagListManager()
: data(NULL), proxyData(NULL) {
	this->SetProcessingStatus(INITIAL_STATUS);
}

FlagListManager::~FlagListManager() {
	if (this->data != NULL) {
		FlagFileData* temp = this->data;
		this->data = NULL;
		delete temp;
	}
	
	if (this->proxyData != NULL) {
		ProxyFlagData* temp = this->proxyData;
		this->proxyData = NULL;
		delete temp;
	}
}

void FlagListManager::BeginFlagFileProcessing() {
	this->data = new FlagFileData();
	this->proxyData = new ProxyFlagData();
	
	wxString tcPath, exeName;
	wxFileName exeFilename;
	
	wxLogDebug(_T("Initializing flag file processing."));
	
	if ( !ProMan::GetProfileManager()->ProfileRead(PRO_CFG_TC_ROOT_FOLDER, &tcPath) ) {
		this->SetProcessingStatus(MISSING_TC);
		return;
	}
	
	if (!wxFileName::DirExists(tcPath)) {
		this->SetProcessingStatus(NONEXISTENT_TC);
		return;
	}
	
	if (!FSOExecutable::HasFSOExecutables(wxFileName(tcPath, wxEmptyString))) {
		this->SetProcessingStatus(INVALID_TC);
		return;
	}
	
	if ( !ProMan::GetProfileManager()->ProfileRead(PRO_CFG_TC_CURRENT_BINARY, &exeName)) {
		this->SetProcessingStatus(MISSING_EXE);
		return;
	}
	
#if IS_APPLE  // needed because on OSX exeName is a relative path from TC root dir
	exeFilename.Assign(tcPath + wxFileName::GetPathSeparator() + exeName);
#else
	exeFilename.Assign(tcPath, exeName);
#endif
	
	wxLogDebug(_T("exeName: ") + exeName);
	wxLogDebug(_T("exeFilename: ") + exeFilename.GetFullPath());
	
	if (!exeFilename.FileExists()) {
		this->SetProcessingStatus(INVALID_BINARY);
		return;
	}
	// Make sure that the directory that I am going to change to exists
	wxFileName tempExecutionLocation;
	tempExecutionLocation.AssignDir(GET_PROFILE_STORAGEFOLDER());
	tempExecutionLocation.AppendDir(_T("temp_flag_folder"));
	if ( !tempExecutionLocation.DirExists() 
		&& !tempExecutionLocation.Mkdir() ) {
		
		wxLogError(_T("Unable to create flag folder at %s"),
			tempExecutionLocation.GetFullPath().c_str());
		this->SetProcessingStatus(CANNOT_CREATE_FLAGFILE_FOLDER);
		return;
	}
	
	FlagFileArray flagFileLocations;
	flagFileLocations.Add(wxFileName(tcPath, _T("flags.lch")));
	flagFileLocations.Add(wxFileName(tempExecutionLocation.GetFullPath(), _T("flags.lch")));
	
	// remove potential flag files to eliminate any confusion.
	for( size_t i = 0; i < flagFileLocations.Count(); i++ ) {
		bool exists = flagFileLocations[i].FileExists();
		if (exists) {
			::wxRemoveFile(flagFileLocations[i].GetFullPath());
			wxLogDebug(_T(" Cleaned up %s ... %s"),
				flagFileLocations[i].GetFullPath().c_str(),
				(flagFileLocations[i].FileExists())? _T("Failed") : _T("Removed"));
		}
	}
	
	wxString previousWorkingDir(::wxGetCwd());
	// hopefully this doesn't goof anything up
	if ( !::wxSetWorkingDirectory(tempExecutionLocation.GetFullPath()) ) {
		wxLogError(_T("Unable to change working directory to %s"),
			tempExecutionLocation.GetFullPath().c_str());
		this->SetProcessingStatus(CANNOT_CHANGE_WORKING_FOLDER);
		return;
	}
	
	wxString commandline;
	// use "" to correct for spaces in path to exeFilename
	if (exeFilename.GetFullPath().Find(_T(" ")) != wxNOT_FOUND) {
		commandline = _T("\"") + exeFilename.GetFullPath() +  _T("\"") + _T(" -get_flags");
	} else {
		commandline = exeFilename.GetFullPath() + _T(" -get_flags");
	}
	
	wxLogDebug(_T(" Called FreeSpace 2 Open with command line '%s'."), commandline.c_str());
	FlagProcess *process = new FlagProcess(flagFileLocations);
	::wxExecute(commandline, wxEXEC_ASYNC, process);
	
	if ( !::wxSetWorkingDirectory(previousWorkingDir) ) {
		wxLogError(_T("Unable to change back to working directory %s"),
			previousWorkingDir.c_str());
		this->SetProcessingStatus(CANNOT_CHANGE_WORKING_FOLDER);
		return;
	}
	
	this->SetProcessingStatus(WAITING_FOR_FLAG_FILE);	
}

wxString FlagListManager::GetStatusMessage() const {
	wxCHECK_MSG(!this->IsProcessingOK(), wxEmptyString,
		_T("status message requested, even though processing succeeded"));
	
	wxString msg;
	
	switch(this->GetProcessingStatus()) {
		case INITIAL_STATUS:
			msg = _("Waiting for flag file processing to be initialized.");
			break;
		case MISSING_TC:
			msg = _("No FreeSpace 2 installation or total conversion\nhas been selected.\nSelect a FreeSpace 2 installation or a total conversion\non the Basic Settings page.");
			break;
		case NONEXISTENT_TC:
			msg = _("The selected root folder does not exist.\nSelect a different FreeSpace 2 installation\nor total conversion on the Basic Settings page.");
			break;
		case INVALID_TC:
			msg = wxString(_("The selected root folder\nhas no FreeSpace 2 Open executables.\n")) +
			_("Either add FS2 Open executables to the root folder\nand refresh the list of executables\non the Basic Settings page,\nor select a different FreeSpace 2 installation\nor total conversion on the Basic Settings page.");
			break;
		case MISSING_EXE:
			msg = _("No FreeSpace 2 Open executable has been selected.\nSelect an executable on the Basic Settings page.");
			break;
		case INVALID_BINARY:
			msg = _("The selected FreeSpace 2 Open executable does not exist.\nSelect another on the Basic Settings page.");
			break;
		case WAITING_FOR_FLAG_FILE:
			msg = _("Waiting for flag file to be generated and parsed.");
			break;
		case FLAG_FILE_NOT_GENERATED:
			msg = _("The executable did not generate a flag file.\nMake sure that the executable\nis a FreeSpace 2 Open executable.");
			break;
		case FLAG_FILE_NOT_VALID:
			msg = _("Generated flag file was not complete.\nPlease talk to a maintainer of this launcher,\nsince you probably found a bug.");
			break;
		case FLAG_FILE_NOT_SUPPORTED:
			msg = _("Generated flag file is not supported.\nUpdate the launcher or talk to a maintainer of this launcher\nif you have the most recent version.");
			break;
		default:
			msg = wxString::Format(
				_("Unknown error occurred while obtaining the flag file\nfrom the FreeSpace 2 Open executable (%d)"),
				this->GetProcessingStatus());
			break;
	}
	
	return msg;
}

FlagFileData* FlagListManager::GetFlagFileData() {
	wxCHECK_MSG(this->IsProcessingOK(), NULL,
		_T("attempt to get flag file data even though processing hasn't succeeded"));
	
	wxCHECK_MSG(this->data != NULL, NULL,
		_T("attempt to get flag file data after it has already been retrieved"));
	
	FlagFileData* temp = this->data;
	this->data = NULL;
	return temp;
}

ProxyFlagData* FlagListManager::GetProxyFlagData() {
	wxCHECK_MSG(this->IsProcessingOK(), NULL,
		_T("attempt to get proxy flag data list even though processing hasn't succeeded"));

	wxCHECK_MSG(this->proxyData != NULL, NULL,
		_T("attempt to get proxy flag data list after it has already been retrieved"));
	
	ProxyFlagData* temp = this->proxyData;
	this->proxyData = NULL;
	return temp;
}

FlagListManager::ProcessingStatus FlagListManager::ParseFlagFile(const wxFileName& flagfilename) {
	if (!flagfilename.FileExists()) {
		wxLogError(_T("The FreeSpace 2 Open executable did not generate a flag file."));
		return FLAG_FILE_NOT_GENERATED;
	}
	
	wxFile flagfile(flagfilename.GetFullPath());
	wxLogDebug(_T("Reading flag file %s."), flagfilename.GetFullPath().c_str());
	// Flagfile requires that we use 32 bit little-endian numbers
	wxInt32 easy_flag_size, flag_size, num_easy_flags, num_flags;
	size_t bytesRead;
	
	bytesRead = flagfile.Read(&easy_flag_size, sizeof(easy_flag_size));
	if ( (size_t)wxInvalidOffset == bytesRead || bytesRead != sizeof(easy_flag_size) ) {
		wxLogError(_T(" Flag file is too short (failed to read easy_flag_size)"));
		return FLAG_FILE_NOT_VALID;
	}
	if ( easy_flag_size != 32 ) {
		wxLogError(_T("  Easy flag size (%d) is not supported"), easy_flag_size);
		return FLAG_FILE_NOT_SUPPORTED;
	}
	
	bytesRead = flagfile.Read(&flag_size, sizeof(flag_size));
	if ( (size_t)wxInvalidOffset == bytesRead || bytesRead != sizeof(flag_size) ) {
		wxLogError(_T(" Flag file is too short (failed to read flag_size)"));
		return FLAG_FILE_NOT_VALID;
	}
	if ( flag_size != 344 ) {
		wxLogError(_T(" Exe flag structure (%d) size is not supported"), flag_size);
		return FLAG_FILE_NOT_SUPPORTED;
	}
	
	bytesRead = flagfile.Read(&num_easy_flags, sizeof(num_easy_flags));
	if ( (size_t)wxInvalidOffset == bytesRead || bytesRead != sizeof(num_easy_flags) ) {
		wxLogError(_T(" Flag file is too short (failed to read num_easy_flags)"));
		return FLAG_FILE_NOT_VALID;
	}
	
	for ( int i = 0; i < num_easy_flags; i++ ) {
		char easy_flag[32];
		bytesRead = flagfile.Read(&easy_flag, sizeof(easy_flag));
		if ( (size_t)wxInvalidOffset == bytesRead || bytesRead != sizeof(easy_flag) ) {
			wxLogError(_T(" Flag file is too short, expected %d, got %d bytes (easy_flag)"), sizeof(easy_flag), bytesRead);
			return FLAG_FILE_NOT_VALID;
		}
		wxString easyFlagStr(easy_flag, wxConvUTF8, strlen(easy_flag));
		this->data->AddEasyFlag(easyFlagStr);
	}
	
	bytesRead = flagfile.Read(&num_flags, sizeof(num_flags));
	if ( (size_t)wxInvalidOffset == bytesRead || bytesRead != sizeof(num_flags) ) {
		wxLogError(_T(" Flag file is too short (failed to read num_flags)"));
		return FLAG_FILE_NOT_VALID;
	}
	
	for ( int i = 0; i < num_flags; i++ ) {
		char flag_string[20];
		char description[40];
		wxInt32 fso_only, easy_on_flags, easy_off_flags;
		char easy_catagory[16], web_url[256];
		
		bytesRead = flagfile.Read(&flag_string, sizeof(flag_string));
		if ( (size_t)wxInvalidOffset == bytesRead || bytesRead != sizeof(flag_string) ) {
			wxLogError(_T(" Flag file is too short, expected %d, got %d bytes (flag_string)"), sizeof(flag_string), bytesRead);
			return FLAG_FILE_NOT_VALID;
		}
		
		bytesRead = flagfile.Read(&description, sizeof(description));
		if ( (size_t)wxInvalidOffset == bytesRead || bytesRead != sizeof(description) ) {
			wxLogError(_T(" Flag file is too short, expected %d, got %d bytes (description)"), sizeof(description), bytesRead);
			return FLAG_FILE_NOT_VALID;
		}
		
		bytesRead = flagfile.Read(&fso_only, sizeof(fso_only));
		if ( (size_t)wxInvalidOffset == bytesRead || bytesRead != sizeof(fso_only) ) {
			wxLogError(_T(" Flag file is too short, expected %d, got %d bytes (fso_only)"), sizeof(fso_only), bytesRead);
			return FLAG_FILE_NOT_VALID;
		}
		
		bytesRead = flagfile.Read(&easy_on_flags, sizeof(easy_on_flags));
		if ( (size_t)wxInvalidOffset == bytesRead || bytesRead != sizeof(easy_on_flags) ) {
			wxLogError(_T(" Flag file is too short, expected %d, got %d bytes (easy_on_flags)"), sizeof(easy_on_flags), bytesRead);
			return FLAG_FILE_NOT_VALID;
		}
		
		bytesRead = flagfile.Read(&easy_off_flags, sizeof(easy_off_flags));
		if ( (size_t)wxInvalidOffset == bytesRead || bytesRead != sizeof(easy_off_flags) ) {
			wxLogError(_T(" Flag file is too short, expected %d, got %d bytes (easy_off_flags)"), sizeof(easy_off_flags), bytesRead);
			return FLAG_FILE_NOT_VALID;
		}
		
		bytesRead = flagfile.Read(&easy_catagory, sizeof(easy_catagory));
		if ( (size_t)wxInvalidOffset == bytesRead || bytesRead != sizeof(easy_catagory) ) {
			wxLogError(_T(" Flag file is too short, expected %d, got %d bytes (easy_category)"), sizeof(easy_catagory), bytesRead);
			return FLAG_FILE_NOT_VALID;
		}
		
		bytesRead = flagfile.Read(&web_url, sizeof(web_url));
		if ( (size_t)wxInvalidOffset == bytesRead || bytesRead != sizeof(web_url) ) {
			wxLogError(_T(" Flag file is too short, expected %d, got %d bytes (web_url)"), sizeof(web_url), bytesRead);
			return FLAG_FILE_NOT_VALID;
		}
		
		flag_string[sizeof(flag_string)-1] = _T('\0');
		description[sizeof(description)-1] = _T('\0');
		easy_catagory[sizeof(easy_catagory)-1] = _T('\0');
		web_url[sizeof(web_url)-1] = _T('\0');
		
		Flag* flag = new Flag();
		
		flag->flagString = wxString(flag_string, wxConvUTF8, strlen(flag_string));
		flag->shortDescription = wxString(description, wxConvUTF8, strlen(description));
		flag->webURL = wxString(web_url, wxConvUTF8, strlen(web_url));
		flag->fsoCatagory = wxString(easy_catagory, wxConvUTF8, strlen(easy_catagory));
		flag->isRecomendedFlag = false; // much better from a UI point of view than "true"
		
		flag->easyEnable = easy_on_flags;
		flag->easyDisable = easy_off_flags;
		
		this->data->AddFlag(flag);
	}		
	
	wxLogDebug(_T(" easy_flag_size: %d, %d; flag_size: %d, %d; num_easy_flags: %d, %d; num_flags: %d, %d"),
		easy_flag_size, sizeof(easy_flag_size),
		flag_size, sizeof(flag_size),
		num_easy_flags, sizeof(num_easy_flags),
		num_flags, sizeof(num_flags));
	
	// build capabilities, which are needed for supporting the new sound code
	wxByte buildCaps;
	bytesRead = flagfile.Read(&buildCaps, sizeof(buildCaps));
	if ( (size_t)wxInvalidOffset == bytesRead ) {
		wxLogInfo(_T(" Old build that does not output its capabilities, must not support OpenAL"));
		buildCaps = 0;
	}
	
	this->data->GenerateFlagSets();
	
	this->proxyData = this->data->GenerateProxyFlagData();
	
	return PROCESSING_OK;
}

void FlagListManager::SetProcessingStatus(const ProcessingStatus& processingStatus) {
	this->processingStatus = processingStatus;
	wxLogDebug(_T("current flag file processing status: %d"), processingStatus);
	this->GenerateFlagFileProcessingStatusChanged(this->GetFlagFileProcessingStatus());
}

FlagListManager::FlagFileProcessingStatus FlagListManager::GetFlagFileProcessingStatus() const {
	const ProcessingStatus& processingStatus = this->GetProcessingStatus();
	
	if (processingStatus == PROCESSING_OK) {
		return FlagListManager::FLAG_FILE_PROCESSING_OK;
	} else if (processingStatus == INITIAL_STATUS || processingStatus == WAITING_FOR_FLAG_FILE) {
		return FlagListManager::FLAG_FILE_PROCESSING_WAITING;
	} else {
		return FlagListManager::FLAG_FILE_PROCESSING_ERROR;
	}
}

FlagListManager::FlagProcess::FlagProcess(FlagFileArray flagFileLocations)
: flagFileLocations(flagFileLocations) {
}

void FlagListManager::FlagProcess::OnTerminate(int pid, int status) {
	wxLogDebug(_T(" FreeSpace 2 Open returned %d when polled for the flags"), status);
	
	// Find the flag file
	wxFileName flagfile;
	for( size_t i = 0; i < flagFileLocations.Count(); i++ ) {
		bool exists = flagFileLocations[i].FileExists();
		if (exists) {
			flagfile = flagFileLocations[i];
			wxLogDebug(_T(" Searching for flag file at %s ... %s"),
				flagFileLocations[i].GetFullPath().c_str(),
				(flagFileLocations[i].FileExists())? _T("Located") : _T("Not Here"));
		}
	}
	
	if ( !flagfile.FileExists() ) {
		FlagListManager::GetFlagListManager()->SetProcessingStatus(FLAG_FILE_NOT_GENERATED);
		wxLogError(_T(" FreeSpace 2 Open did not generate a flag file."));
		return;
	}
	
	FlagListManager::GetFlagListManager()->SetProcessingStatus(
		FlagListManager::GetFlagListManager()->ParseFlagFile(flagfile));
	
	if ( FlagListManager::GetFlagListManager()->IsProcessingOK() ) {
		::wxRemoveFile(flagfile.GetFullPath());
	}
	
	delete this;
}
