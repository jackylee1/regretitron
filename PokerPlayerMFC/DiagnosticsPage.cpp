// DiagnosticsPage.cpp : implementation file
//

#include "stdafx.h"
#include "DiagnosticsPage.h"
#include "../PokerLibrary/rephands.h" //needed for card printing utilities
//converts from std::string to Cstring
#define CSTR(stdstr) CString((stdstr).c_str())
#include "../PokerLibrary/constants.h"


// DiagnosticsPage dialog

IMPLEMENT_DYNAMIC(DiagnosticsPage, CDialog)

DiagnosticsPage::DiagnosticsPage(CWnd* pParent /*=NULL*/)
	: CDialog(DiagnosticsPage::IDD, pParent)
	//i initialize these to -1 so that if RefreshHands is called prematurely,
	//before sethandindices, it will see they are -1 and not use garbage data.
	, mygr(-1), myhandi(-1), myboardi(-1)
{
	//this is the code to actually make this page display when it is created via new.
	//Boom: http://www.codeproject.com/KB/dialog/gettingmodeless.aspx
	Create(DiagnosticsPage::IDD, GetDesktopWindow());
	ShowWindow(SW_SHOW);
	//and see this thread on how to include a resource from
	//a static library in order to have it be found in an
	//application. See post 20 or last post.
	//http://www.codeguru.com/forum/showthread.php?t=203209&page=2
}

DiagnosticsPage::~DiagnosticsPage()
{
}

void DiagnosticsPage::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_TEXT0, PotSizes);
	DDX_Control(pDX, IDC_TEXT1, Bethist[0]);
	DDX_Control(pDX, IDC_TEXT2, Bethist[1]);
	DDX_Control(pDX, IDC_TEXT3, Bethist[2]);

	DDX_Control(pDX, IDC_CARD00, Card[0][0]);
	DDX_Control(pDX, IDC_CARD01, Card[0][1]);
	DDX_Control(pDX, IDC_CARD02, Card[0][2]);
	DDX_Control(pDX, IDC_CARD03, Card[0][3]);
	DDX_Control(pDX, IDC_CARD04, Card[0][4]);
	DDX_Control(pDX, IDC_CARD05, Card[0][5]);
	DDX_Control(pDX, IDC_CARD06, Card[0][6]);

	DDX_Control(pDX, IDC_CARD1, Card[1][0]);
	DDX_Control(pDX, IDC_CARD2, Card[1][1]);
	DDX_Control(pDX, IDC_CARD3, Card[1][2]);
	DDX_Control(pDX, IDC_CARD4, Card[1][3]);
	DDX_Control(pDX, IDC_CARD5, Card[1][4]);
	DDX_Control(pDX, IDC_CARD6, Card[1][5]);
	DDX_Control(pDX, IDC_CARD7, Card[1][6]);

	DDX_Control(pDX, IDC_CARD9, Card[2][0]);
	DDX_Control(pDX, IDC_CARD8, Card[2][1]);
	DDX_Control(pDX, IDC_CARD10, Card[2][2]);
	DDX_Control(pDX, IDC_CARD12, Card[2][3]);
	DDX_Control(pDX, IDC_CARD11, Card[2][4]);
	DDX_Control(pDX, IDC_CARD13, Card[2][5]);
	DDX_Control(pDX, IDC_CARD14, Card[2][6]);

	DDX_Control(pDX, IDC_CARD16, Card[3][0]);
	DDX_Control(pDX, IDC_CARD15, Card[3][1]);
	DDX_Control(pDX, IDC_CARD17, Card[3][2]);
	DDX_Control(pDX, IDC_CARD19, Card[3][3]);
	DDX_Control(pDX, IDC_CARD18, Card[3][4]);
	DDX_Control(pDX, IDC_CARD20, Card[3][5]);
	DDX_Control(pDX, IDC_CARD21, Card[3][6]);

	DDX_Control(pDX, IDC_CARD23, Card[4][0]);
	DDX_Control(pDX, IDC_CARD22, Card[4][1]);
	DDX_Control(pDX, IDC_CARD24, Card[4][2]);
	DDX_Control(pDX, IDC_CARD26, Card[4][3]);
	DDX_Control(pDX, IDC_CARD25, Card[4][4]);
	DDX_Control(pDX, IDC_CARD27, Card[4][5]);
	DDX_Control(pDX, IDC_CARD28, Card[4][6]);

	DDX_Control(pDX, IDC_CARD30, Card[5][0]);
	DDX_Control(pDX, IDC_CARD29, Card[5][1]);
	DDX_Control(pDX, IDC_CARD31, Card[5][2]);
	DDX_Control(pDX, IDC_CARD33, Card[5][3]);
	DDX_Control(pDX, IDC_CARD32, Card[5][4]);
	DDX_Control(pDX, IDC_CARD34, Card[5][5]);
	DDX_Control(pDX, IDC_CARD35, Card[5][6]);

	DDX_Control(pDX, IDC_RADIO1, ActButton[0]);
	DDX_Control(pDX, IDC_RADIO2, ActButton[1]);
	DDX_Control(pDX, IDC_RADIO3, ActButton[2]);
	DDX_Control(pDX, IDC_RADIO4, ActButton[3]);
	DDX_Control(pDX, IDC_RADIO5, ActButton[4]);
	DDX_Control(pDX, IDC_RADIO6, ActButton[5]);
	DDX_Control(pDX, IDC_RADIO7, ActButton[6]);
	DDX_Control(pDX, IDC_RADIO8, ActButton[7]);
	DDX_Control(pDX, IDC_RADIO9, ActButton[8]);
	DDX_Control(pDX, IDC_INVESTBOT, PerceivedInvestBot);
	DDX_Control(pDX, IDC_INVESTHUM, PerceivedInvestHum);
}


BEGIN_MESSAGE_MAP(DiagnosticsPage, CDialog)
	ON_BN_CLICKED(IDC_REFRESH, &DiagnosticsPage::RefreshHands)
END_MESSAGE_MAP()


//this function is called first (on change of bot state) to set these indices
//this is the way that teh BotAPI communicates with the diagnostics page,
//so that the RefreshHands handler doesn't have to call into BotAPI or something
void DiagnosticsPage::sethandindices(int gr, int handi, int boardi)
{
	mygr = gr;
	myhandi = handi;
	myboardi = boardi;
}

//this is a helper function for RefreshHands it simply decomposes
// the cardmask hand into 2 cardmasks each with a single card, and then
// prints them to the appropriate fields using the cardfilename function in rephands
void DiagnosticsPage::drawpreflop(int whichset, CardMask hand)
{
	CardMask singles[2];
	decomposecm(hand, singles);
	Card[whichset][0].LoadFromFile(CSTR(cardfilename(singles[0])));
	Card[whichset][1].LoadFromFile(CSTR(cardfilename(singles[1])));
}

//this is a helper function for RefreshHands. it decomposes teh flop
//into 3 cardmasks wit a single card each, and prints them to the appropriate fields
void DiagnosticsPage::drawflop(int whichset, CardMask flop)
{
	CardMask singles[3];
	decomposecm(flop, singles);
	Card[whichset][2].LoadFromFile(CSTR(cardfilename(singles[0])));
	Card[whichset][3].LoadFromFile(CSTR(cardfilename(singles[1])));
	Card[whichset][4].LoadFromFile(CSTR(cardfilename(singles[2])));
}

// DiagnosticsPage message handlers

//this is the event handler for the refresh button. it gets the information it needs
//from the private data set by sethandindices(). that function must be called first.
//it uses the two helper functions above to help print the preflop and cthe flop,
//since those muste be decomposed, but does the turn and river itself for each case.
void DiagnosticsPage::RefreshHands()
{
	//we have had no info sent to us yet.
	if(mygr == -1) return;

	for(int n=0; n<6; n++) //6 sets of hands
	{
		if(mygr==PREFLOP)
		{
			drawpreflop(n, findpreflophand(myhandi));
			for(int i=2; i<7; i++)
				Card[n][i].FreeData();
		}
		else if(mygr==FLOP)
		{
			CardMask hand, flop;
			findflophand(myhandi, myboardi, hand, flop);
			drawpreflop(n, hand);
			drawflop(n, flop);
		}
		else if(mygr==TURN)
		{
			CardMask hand, flop, turn;
			findturnhand(myhandi, myboardi, hand, flop, turn);
			drawpreflop(n, hand);
			drawflop(n, flop);
			Card[n][5].LoadFromFile(CSTR(cardfilename(turn)));
		}
		else if(mygr==RIVER)
		{
			CardMask hand, flop, turn, river;
			findriverhand(myhandi, myboardi, hand, flop, turn, river);
			drawpreflop(n, hand);
			drawflop(n, flop);
			Card[n][5].LoadFromFile(CSTR(cardfilename(turn)));
			Card[n][6].LoadFromFile(CSTR(cardfilename(river)));
		}
		else
			REPORT("Invalid gameround in refresh hands");
	}
}
