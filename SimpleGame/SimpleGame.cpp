// SimpleGame.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "SimpleGame.h"
#include "SimpleGameDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CSimpleGameApp

BEGIN_MESSAGE_MAP(CSimpleGameApp, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()


// CSimpleGameApp construction

CSimpleGameApp::CSimpleGameApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}


// The one and only CSimpleGameApp object

CSimpleGameApp theApp;


// CSimpleGameApp initialization

BOOL CSimpleGameApp::InitInstance()
{
	// InitCommonControlsEx() is required on Windows XP if an application
	// manifest specifies use of ComCtl32.dll version 6 or later to enable
	// visual styles.  Otherwise, any window creation will fail.
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// Set this to include all the common control classes you want to use
	// in your application.
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	// of your final executable, you should remove from the following
	// the specific initialization routines you do not need
	// Change the registry key under which our settings are stored
	// TODO: You should modify this string to be something appropriate
	// such as the name of your company or organization
	SetRegistryKey(_T("Local AppWizard-Generated Applications"));

	
	//CWinThread::m_pMainWnd
	//http://msdn.microsoft.com/en-us/library/f3ddxzww(VS.80).aspx
	//InitInstanc()
	//http://msdn.microsoft.com/en-us/library/ae6yx0z0.aspx
	//basically, the application will terminate as soon as the m_pMainWnd
	//is null. Before, the SimpleGameDlg was displayed as modal. But that
	//blocked the diagnostics page. So I displayed it as modeless, by calling
	//create and show in CSimpleGameDlg's constructor. But then, the application
	//was exiting right away, as it thought there was no main window. So now
	//we save the main window straight to this variable here. And all should 
	//work well.
	CWinThread::m_pMainWnd = new CSimpleGameDlg;
	return TRUE;
}
