#pragma once
#ifdef _MFC_VER
#include "afxwin.h"
#include "picturectrl.h"
#include "strategy.h"
#include <vector>
using std::vector;
#include "resource.h"
#include "afxcmn.h"
#include <poker_defs.h>
#include "../PokerLibrary/floaterfile.h"
#include "afxext.h"


// DiagnosticsPage dialog

class DiagnosticsPage : public CDialog
{
	DECLARE_DYNAMIC(DiagnosticsPage)

public:
	DiagnosticsPage(CWnd* pParent = NULL);   // standard constructor
	virtual ~DiagnosticsPage();
	BOOL OnInitDialog();

// Dialog Data
	enum { IDD = IDD_PROPPAGE_LARGE };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	//these are the control variables for things on the page that I access
	//directly from BotAPI. No wrapper functions or anything are used. they are
	//called straight from outside this class
	CStatic PotSize;
	CButton ActButton[9];
	CProgressCtrl ActionBars[9];
	CStatic PerceivedInvestHum;
	CStatic BinNumber;
	CStatic BinMax;
	CStatic BetHistory;
	CStatic BoardScore;

	//these are the interface for updating the example hands. i did these differently
	//because i want the button on the page to refresh them. because of that, it makes
	//sense to write the code once and put it in just one place. that neccesitated
	//these wrapper functions so that BotAPI could send over the neccesary info
	//and then cause this page to draw the hands.
	void setcardstoshow(Strategy * currstrat, int gr, const vector<CardMask> &cards);

private:
	afx_msg void RefreshCards() { RefreshCards(false); }
	void RefreshCards(bool newcards);
	void setcardinfo(int row, const vector<CardMask> &cards, int gr);

	//set by setcardstoshow, read by RefreshCards. 
	Strategy * mycurrstrat;
	int mygr;
	vector<CardMask> mycards;

	//used by RefreshCards.
	vector<FloaterFile*> myfloaters;
	static void decomposecm(CardMask in, vector<CardMask> &out);
	void drawpreflop(int whichset, CardMask hand);
	void drawflop(int whichset, CardMask flop);
	CPictureCtrl Card[5][7]; //5 rows of 7 cards (only 4 rows used now)
	CStatic CardInfo[4][4]; //4 rows of 4 rounds of card info
	CFont HSSFont;
};

#endif
