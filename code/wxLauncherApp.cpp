#include "generated/configure_launcher.h"
#include <wx/wx.h>
#include <wx/image.h>
#include <wx/filesys.h>
#include <wx/fs_arc.h>
#include <wx/fs_inet.h>
#include <wx/splash.h>

#if HAS_SDL == 1
#include "SDL.h"
#endif

#include "wxLauncherApp.h"
#include "MainWindow.h"
#include "Skin.h"
#include "Logger.h"
#include "version.h"
#include "ProfileManager.h"
#include "HelpManager.h"

#include "wxLauncherSetup.h" // Last include for memory debugging

IMPLEMENT_APP(wxLauncher);

bool wxLauncher::OnInit() {
	wxInitAllImageHandlers();

#if MSCRTMEMORY
	_CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif
#if HAS_SDL == 1
	if ( 0 != SDL_InitSubSystem(SDL_INIT_VIDEO) ) {
	  wxLogFatalError(_T("SDL_InitSubSystem failed"));
	}
#endif
	// Little hack to deal with people starting the launcher from the bin folder
	wxFileName resourceDir(_T(RESOURCES_PATH));
	if ( !resourceDir.DirExists() ) {
		resourceDir.PrependDir(_T(".."));
		if ( resourceDir.DirExists() ) {
			::wxSetWorkingDirectory(_T(".."));
		}
	}

	wxBitmap splash;
	wxSplashScreen* splashWindow = NULL;
	wxFileName splashFile(_T(RESOURCES_PATH),_T("SCP_Header.png"));
	if (splash.LoadFile(splashFile.GetFullPath(), wxBITMAP_TYPE_ANY)) {
#if NDEBUG
		splashWindow = new wxSplashScreen(splash, wxSPLASH_CENTRE_ON_SCREEN, 0, NULL, wxID_ANY);
#else
		splashWindow = NULL;//new wxSplashScreen(splash, wxSPLASH_CENTRE_ON_SCREEN | wxSPLASH_TIMEOUT, 1000, NULL, wxID_ANY);
#endif
		wxYield();
	} else {
		wxLogFatalError(_T("Unable to load splash image, this normally means that you are running the Launcher from a directory that does not have the images in it."));
		return false;
	}
	
	wxLog::SetActiveTarget(new Logger());
	wxLogInfo(_T("wxLauncher Version %d.%d.%d"), MAJOR_VERSION, MINOR_VERSION, PATCH_VERSION);
	wxLogInfo(_T("Build \"%s\" committed on (%s)"), HGVersion, HGDate);
	wxLogInfo(wxDateTime(time(NULL)).Format(_T("%c")));

	wxLogInfo(_T("Initing profiles..."));
	if ( !ProMan::Initialize() ) {
		wxLogFatalError(_T("ProfileManager failed to initialize. Aborting! See log file for more details."));
		return false;
	}
	wxFileSystem::AddHandler(new wxArchiveFSHandler);
	wxFileSystem::AddHandler(new wxInternetFSHandler);

	wxLogInfo(_T("Initing Skin System..."));
	this->skin = new SkinSystem();

	wxLogInfo(_T("Initing HelpManager..."));
	HelpManager::Initialize();

	wxLogInfo(_T("wxLauncher Starting up."));


	MainWindow* window = new MainWindow(skin);
	wxLogStatus(_T("MainWindow is complete"));
	window->Show(true);
#if NDEBUG // will autodelete when timout runs out in debug
	splashWindow->Show(false);
	splashWindow->Destroy();
#endif

	wxLogStatus(_T("Ready."));
	return true;
}

int wxLauncher::OnExit() {
	if (this->skin != NULL) {
		delete this->skin;
	}

	ProMan::DeInitialize();
	HelpManager::DeInitialize();
	
#if HAS_SDL == 1
	SDL_Quit();
#endif

	wxLogInfo(_T("wxLogger shutdown complete."));

	return 0;
}