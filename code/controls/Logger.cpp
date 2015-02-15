/*
Copyright (C) 2009-2010,2015 wxLauncher Team

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

#include <wx/wx.h>
#include <wx/wfstream.h>
#include <wx/datetime.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>

#include <wchar.h>

#include "controls/Logger.h"

#include "global/MemoryDebugging.h"

////// Logger
const wxString levels[] = {
	wxT_2("FATAL"),
	wxT_2("ERROR"),
	wxT_2("WARN "),
	wxT_2("MSG  "),
	wxT_2("STSBR"),
	wxT_2("INFO "),
	wxT_2("DEBUG"),
};

LAUNCHER_DEFINE_EVENT_TYPE(EVT_STATUS_BAR_MESSAGE);
/** Constructor. */
Logger::Logger() {
	wxFileName outFileName(wxStandardPaths::Get().GetUserDataDir(), wxT_2("wxLauncher.log"));
	if (!outFileName.DirExists() && 
		!wxFileName::Mkdir(outFileName.GetPath(), 0700, wxPATH_MKDIR_FULL) ) {
			wxLogFatalError(_("Unable to create folder to place log in. (%s)"), outFileName.GetPath().c_str());
	}

	this->outFile = new wxFFile(outFileName.GetFullPath(), wxT_2("wb"));
	if (!outFile->IsOpened()) {
		wxLogFatalError(_("Unable to open log output file. (%s)"), outFileName.GetFullPath().c_str());
	}
	this->out = new wxFFileOutputStream(*outFile);
	wxASSERT_MSG(out->IsOk(), wxT_2("Log output file is not valid!"));
	this->out->Write("\357\273\277", 3);
}

/** Destructor. */
Logger::~Logger() {
	wxCriticalSectionLocker locker(this->dologLock);
	char exitmsg[] = "\nLog closed.\n";
	this->out->Write(exitmsg, strlen(exitmsg));
	this->out->Close();
	delete this->out;
	delete this->outFile;
}

/** Overridden as per wxWidgets docs to implement a wxLog. */
/* Compatiblity with 2.8.x */
#if wxMAJOR_VERSION == 2 && wxMINOR_VERSION >= 8
void Logger::DoLog(
		wxLogLevel level,
		const wxChar *msg,
		time_t time)
{
	wxString timestr = wxDateTime(time).Format(
		wxT_2("%y%j%H%M%S"),
		wxDateTime::GMT0);
#else
void Logger::DoLogRecord(
	wxLogLevel level,
	const wxString& msg,
	const wxLogRecordInfo& info)
{
	wxString timestr = wxDateTime(info.timestamp).Format(
		wxT_2("%y%j%H%M%S"),
		wxDateTime::GMT0);
#endif
	wxString str = wxString::Format(
    wxT_2("%s:%s:"), timestr.c_str(), levels[level].c_str());
	wxString buf(msg);

	// Serialize access to the logfile
	wxCriticalSectionLocker locker(this->dologLock);
	out->Write(str.mb_str(wxConvUTF8), str.size());
	out->Write(buf.mb_str(wxConvUTF8), buf.size());
	out->Write("\n", 1);

	// and handler list, because STL types are not thread safe
	// for any operation
	if (level <= 5) {
		EventHandlers::iterator iter = this->eventHandlers.begin();
		while (iter != this->eventHandlers.end()) {
			wxCommandEvent evt(EVT_STATUS_BAR_MESSAGE, wxID_NONE);
			evt.SetClientObject(new StatusBarMessage(buf, level));
			wxEvtHandler* current = *iter;
			current->AddPendingEvent(evt);
			iter++;
		}
	}
}

void Logger::AddEventHandler(wxEvtHandler *handler) {
	wxCriticalSectionLocker locker(this->dologLock);
	wxASSERT_MSG(eventHandlers.IndexOf(handler) == wxNOT_FOUND,
		wxString::Format(
			wxT_2("Logger::AddEventHandler(): Handler at %p already registered."),
			handler));
	this->eventHandlers.Append(handler);
}

void Logger::RemoveEventHandler(wxEvtHandler *handler) {
	wxCriticalSectionLocker locker(this->dologLock);
	wxASSERT_MSG(eventHandlers.IndexOf(handler) != wxNOT_FOUND,
		wxString::Format(
			wxT_2("Logger::RemoveEventHandler(): Handler at %p not registered."),
			handler));
	this->eventHandlers.DeleteObject(handler);
}

void Logger::Flush() {
	// Warning: ignoring return value from Flush().
	outFile->Flush();
}

