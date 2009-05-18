#pragma once
#include "afxwin.h"
#include "..\picturectrl.h"
#include "resource.h"


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
	//these are the control variables for things on the page that I access
	//directly from BotAPI. No wrapper functions or anything are used. they are
	//called straight from outside this class
	CStatic PotSizes;
	CStatic Bethist[3];
	CButton ActButton[9];
	CStatic PerceivedInvestBot;
	CStatic PerceivedInvestHum;

	//these are the interface for updating the example hands. i did these differently
	//because i want the button on the page to refresh them. because of that, it makes
	//sense to write the code once and put it in just one place. that neccesitated
	//these wrapper functions so that BotAPI could send over the neccesary info
	//and then cause this page to draw the hands.
	void sethandindices(int gr, int handi, int boardi);
	afx_msg void RefreshHands();

private:
	//set by sethandindices, read by RefreshHands. 
	int mygr, myhandi, myboardi;

	//used by RefreshHands.
	CPictureCtrl Card[6][7];
	void drawpreflop(int whichset, CardMask hand);
	void drawflop(int whichset, CardMask flop);
};
