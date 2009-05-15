#pragma once
#include "afxwin.h"
#include "c:\documents and settings\scott\my documents\pokerbreedinggrounds\trunk\simplegame\picturectrl.h"
#include "c:\documents and settings\scott\my documents\pokerbreedinggrounds\trunk\simplegame\picturectrl.h"
#include "c:\documents and settings\scott\my documents\pokerbreedinggrounds\trunk\simplegame\picturectrl.h"
#include "c:\documents and settings\scott\my documents\pokerbreedinggrounds\trunk\simplegame\picturectrl.h"
#include "c:\documents and settings\scott\my documents\pokerbreedinggrounds\trunk\simplegame\picturectrl.h"
#include "c:\documents and settings\scott\my documents\pokerbreedinggrounds\trunk\simplegame\picturectrl.h"
#include "c:\documents and settings\scott\my documents\pokerbreedinggrounds\trunk\simplegame\picturectrl.h"


// DiagnosticsPage dialog

class DiagnosticsPage : public CDialog
{
	DECLARE_DYNAMIC(DiagnosticsPage)

public:
	DiagnosticsPage(CWnd* pParent = NULL);   // standard constructor
	virtual ~DiagnosticsPage();

// Dialog Data
	enum { IDD = IDD_PROPPAGE_LARGE };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	CStatic PotSizes;
	CStatic BethistPreflop;
	CStatic BethistFlop;
	CStatic BethistTurn;
	CPictureCtrl Card00;
	CPictureCtrl Card01;
	CPictureCtrl Card02;
	CPictureCtrl Card03;
	CPictureCtrl Card04;
	CPictureCtrl Card05;
	CPictureCtrl Card06;
	CStatic Act0Label;
	CStatic Act1Label;
	CStatic Act2Label;
	CStatic Act3Label;
	CStatic Act4Label;
	CStatic Act5Label;
	CStatic Act6Label;
	CStatic Act7Label;
	CStatic Act8Label;
	CStatic Act0Prob;
	CStatic Act1Prob;
	CStatic Act2Prob;
	CStatic Act3Prob;
	CStatic Act4Prob;
	CStatic Act5Prob;
	CStatic Act6Prob;
	CStatic Act7Prob;
	CStatic Act8Prob;
};
