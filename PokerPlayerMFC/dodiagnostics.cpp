#include "stdafx.h"
#include "dodiagnostics.h"
#include "../PokerLibrary/rephands.h"
#include "DiagnosticsPage.h"

DiagnosticsPage *MyWindow = NULL;

void populatewindow()
{
	if(MyWindow == NULL)
		MyWindow = new DiagnosticsPage;
}
void destroywindow()
{
	if(MyWindow != NULL)
	{
		MyWindow->DestroyWindow();
		delete MyWindow;
		MyWindow = NULL;
	}
}