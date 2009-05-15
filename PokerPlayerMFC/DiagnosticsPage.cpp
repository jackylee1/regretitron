// DiagnosticsPage.cpp : implementation file
//

#include "stdafx.h"
#include "DiagnosticsPage.h"


// DiagnosticsPage dialog

IMPLEMENT_DYNAMIC(DiagnosticsPage, CDialog)

DiagnosticsPage::DiagnosticsPage(CWnd* pParent /*=NULL*/)
	: CDialog(DiagnosticsPage::IDD, pParent)
{

}

DiagnosticsPage::~DiagnosticsPage()
{
}

void DiagnosticsPage::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TEXT0, PotSizes);
	DDX_Control(pDX, IDC_TEXT1, BethistPreflop);
	DDX_Control(pDX, IDC_TEXT2, BethistFlop);
	DDX_Control(pDX, IDC_TEXT3, BethistTurn);
	DDX_Control(pDX, IDC_CARD00, Card00);
	DDX_Control(pDX, IDC_CARD01, Card01);
	DDX_Control(pDX, IDC_CARD02, Card02);
	DDX_Control(pDX, IDC_CARD03, Card03);
	DDX_Control(pDX, IDC_CARD04, Card04);
	DDX_Control(pDX, IDC_CARD05, Card05);
	DDX_Control(pDX, IDC_CARD06, Card06);
	DDX_Control(pDX, IDC_ACT0, Act0Label);
	DDX_Control(pDX, IDC_ACT1, Act1Label);
	DDX_Control(pDX, IDC_ACT2, Act2Label);
	DDX_Control(pDX, IDC_ACT3, Act3Label);
	DDX_Control(pDX, IDC_ACT4, Act4Label);
	DDX_Control(pDX, IDC_ACT5, Act5Label);
	DDX_Control(pDX, IDC_ACT6, Act6Label);
	DDX_Control(pDX, IDC_ACT7, Act7Label);
	DDX_Control(pDX, IDC_ACT8, Act8Label);
	DDX_Control(pDX, IDC_PROB0, Act0Prob);
	DDX_Control(pDX, IDC_PROB1, Act1Prob);
	DDX_Control(pDX, IDC_PROB2, Act2Prob);
	DDX_Control(pDX, IDC_PROB3, Act3Prob);
	DDX_Control(pDX, IDC_PROB4, Act4Prob);
	DDX_Control(pDX, IDC_PROB5, Act5Prob);
	DDX_Control(pDX, IDC_PROB6, Act6Prob);
	DDX_Control(pDX, IDC_PROB7, Act7Prob);
	DDX_Control(pDX, IDC_PROB8, Act8Prob);
}


BEGIN_MESSAGE_MAP(DiagnosticsPage, CDialog)
END_MESSAGE_MAP()


// DiagnosticsPage message handlers
