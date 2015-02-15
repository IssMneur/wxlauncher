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

#ifndef LAUNCHERLOGGER_H
#define LAUNCHERLOGGER_H

#include <wx/wx.h>
#include <wx/wfstream.h>

#include <boost/smart_ptr/shared_ptr.hpp>

#include "apis/EventHandlers.h"

/** A new message has arrived to display in the status bar.

The message is a StatusBarMessage class embeded as the
ClientData of the wxCommandEvent.
*/
LAUNCHER_DECLARE_EVENT_TYPE(EVT_STATUS_BAR_MESSAGE);

/** Simple structure to hold the message that is to be displayed.
*/
class StatusBarMessage: public wxClientData {
public:
	wxString msg;
	wxLogLevel level;
	StatusBarMessage(wxString _msg, wxLogLevel _level):
	msg(_msg), level(_level)
	{};
	virtual ~StatusBarMessage() {
	};
};

class Logger: public wxLog {
public:
	Logger();
	virtual ~Logger();

	/* Compatiblity with 2.8.x */
#if wxMAJOR_VERSION == 2 && wxMINOR_VERSION >= 8
	virtual void DoLog(
		wxLogLevel level,
		const wxChar *msg,
		time_t time);
#else
	virtual void DoLogRecord(
		wxLogLevel level,
		const wxString& msg,
		const wxLogRecordInfo& info);
#endif
	void AddEventHandler(wxEvtHandler *handler);
	void RemoveEventHandler(wxEvtHandler *handler);
	virtual void Flush();
private:
	wxFFileOutputStream *out;
	wxFFile *outFile;
	wxCriticalSection dologLock;
	EventHandlers eventHandlers;
};

#endif
