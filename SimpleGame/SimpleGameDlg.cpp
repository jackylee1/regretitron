// SimpleGameDlg.cpp : implementation file
//

#include "stdafx.h"
#include "SimpleGame.h"
#include "SimpleGameDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CSimpleGameDlg dialog




CSimpleGameDlg::CSimpleGameDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CSimpleGameDlg::IDD, pParent),
	  totalhumanwon(0), human(P0), bot(P1)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CSimpleGameDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_HCARD0, hCard0);
	DDX_Control(pDX, IDC_HCARD1, hCard1);
	DDX_Control(pDX, IDC_CCARD0, cCard0);
	DDX_Control(pDX, IDC_CCARD1, cCard1);
	DDX_Control(pDX, IDC_CCARD2, cCard2);
	DDX_Control(pDX, IDC_CCARD3, cCard3);
	DDX_Control(pDX, IDC_CCARD4, cCard4);
	DDX_Control(pDX, IDC_BCARD0, bCard0);
	DDX_Control(pDX, IDC_BCARD1, bCard1);
	DDX_Control(pDX, IDC_CHECK1, ShowBotCards);
	DDX_Control(pDX, IDC_EDIT4, PotTotal);
	DDX_Control(pDX, IDC_EDIT5, TotalWon);
	DDX_Control(pDX, IDC_EDIT2, InvestedHum);
	DDX_Control(pDX, IDC_EDIT3, InvestedBot);
	DDX_Control(pDX, IDC_EDIT1, BetAmount);
}

BEGIN_MESSAGE_MAP(CSimpleGameDlg, CDialog)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDC_BUTTON1, &CSimpleGameDlg::OnBnClickedButton1)
	ON_BN_CLICKED(IDC_BUTTON2, &CSimpleGameDlg::OnBnClickedButton2)
	ON_BN_CLICKED(IDC_BUTTON5, &CSimpleGameDlg::OnBnClickedButton5)
	ON_BN_CLICKED(IDC_CHECK1, &CSimpleGameDlg::OnBnClickedCheck1)
	ON_BN_CLICKED(IDC_BUTTON6, &CSimpleGameDlg::OnBnClickedButton6)
	ON_BN_CLICKED(IDC_BUTTON3, &CSimpleGameDlg::OnBnClickedButton3)
	ON_BN_CLICKED(IDC_BUTTON4, &CSimpleGameDlg::OnBnClickedButton4)
END_MESSAGE_MAP()


// CSimpleGameDlg message handlers

BOOL CSimpleGameDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here
	updatetotal();

	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CSimpleGameDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CSimpleGameDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}










CString cardfilename(CardMask m)
{
	int i;
	CString ret;
	for(i=0; i<52; i++)
		if (StdDeck_CardMask_CARD_IS_SET(m, i)) break;
	i = 1 + (12-StdDeck_RANK(i))*4 + StdDeck_SUIT(i);
	ret.Format(TEXT("cards/%d.png"),i);
	return ret;
}
	
void CSimpleGameDlg::printbotcards()
{
	bCard0.LoadFromFile(cardfilename(botcm0));
	bCard1.LoadFromFile(cardfilename(botcm1));
}
void CSimpleGameDlg::printbotcardbacks()
{
	bCard0.LoadFromFile(TEXT("cards/b1fv.png"));
	bCard1.LoadFromFile(TEXT("cards/b1fv.png"));
}
void CSimpleGameDlg::printhumancards()
{
	hCard0.LoadFromFile(cardfilename(humancm0));
	hCard1.LoadFromFile(cardfilename(humancm1));
}
void CSimpleGameDlg::printflop()
{
	cCard0.LoadFromFile(cardfilename(flop0));
	cCard1.LoadFromFile(cardfilename(flop1));
	cCard2.LoadFromFile(cardfilename(flop2));
}
void CSimpleGameDlg::printturn()
{
	cCard3.LoadFromFile(cardfilename(turn));
}
void CSimpleGameDlg::printriver()
{
	cCard4.LoadFromFile(cardfilename(river));
}









void CSimpleGameDlg::updatetotal()
{
	//reprint the total to the screen
	CString val;
	val.Format(TEXT("%.2f"), totalhumanwon);
	TotalWon.SetWindowText(val);
}
void CSimpleGameDlg::updatepot()
{
	//reprint the pot to the screen
	CString val;
	val.Format(TEXT("%.2f"), 2*pot);
	PotTotal.SetWindowText(val);
}
void CSimpleGameDlg::updateinvested()
{
	//reprint the pot to the screen
	CString val;
	val.Format(TEXT("%.2f"), invested[human]);
	InvestedHum.SetWindowText(val);
	val.Format(TEXT("%.2f"), invested[bot]);
	InvestedBot.SetWindowText(val);
}








void CSimpleGameDlg::advanceround()
{
	switch(gameround)
	{
	case PREFLOP: 
		//inform the bot
		MyBot.setflop(flop, pot); 
		//draw the pics
		printflop();
		break;

	case FLOP: 
		MyBot.setturn(turn, pot); 
		printturn();
		break;

	case TURN: 
		MyBot.setriver(river, pot); 
		printriver();
		break;

	case RIVER:
		//game over
		printbotcards();
		if (winner==human)
			totalhumanwon += (pot + invested[bot]);
		else if (winner==bot)
			totalhumanwon -= (pot + invested[bot]);
		updatetotal();
	}
	gameround ++;
}

void CSimpleGameDlg::OnBnClickedButton1()
{
	//Fold/check:
	if(invested[bot]==0) //then we check
	{
		if(human == P0) //first to act, "betting"
			MyBot.advancetree(human,BET,0);
		else //second to act, "calling"
		{
			MyBot.advancetree(human,CALL,0);
			advanceround();
		}
	}
	else
	{
		//else we have folded
		totalhumanwon -= pot + invested[human];
		updatetotal();
	}
}

void CSimpleGameDlg::OnBnClickedButton2()
{
	//Call:

	//then this ends the game
	if(isallin)
	{
		printbotcards();
		Sleep(1000); //suspense needed.
		printflop();
		Sleep(1000);
		printturn();
		Sleep(1000);
		printriver();
		gameround=RIVER; //hack
		advanceround();
	}
	//if this is preflop and we are calling from the SB, 
	// well, that's actually a "bet" as it doesn't end round
	else if(gameround == PREFLOP && invested[human] == SBLIND
		&& invested[bot] == BBLIND)
	{
		MyBot.advancetree(human,BET,BBLIND);
		invested[human]=BBLIND;
		updateinvested();
	}
	else
	{
		//inform bot
		MyBot.advancetree(human,CALL,invested[bot]);
		//increase pot
		pot += invested[bot];
		updatepot();
		//reset invested
		invested[bot]=invested[human]=0;
		updateinvested();

		advanceround();
	}
}

void CSimpleGameDlg::OnBnClickedButton5()
{
	//New Game:
	//change positions
	std::swap(human,bot);
	//set gameround
	gameround = PREFLOP;
	//no one is all-in
	isallin=false;
	//reset pot
	pot = 0;
	updatepot();
	//set blinds
	invested[P0] = BBLIND;
	invested[P1] = SBLIND;
	updateinvested();
	//deal cards
	{
		CardMask usedcards, fullhuman, fullbot;
		HandVal r0, r1;

		//deal out the cards randomly.
		CardMask_RESET(usedcards);
		//set the cm's corresponding to individual cards (for displaying)
#define DEALANDOR(card) MONTECARLO_N_CARDS_D(card, usedcards, 1, 1, ); CardMask_OR(usedcards, usedcards, card);
		DEALANDOR(botcm0)
		DEALANDOR(botcm1)
		DEALANDOR(humancm0)
		DEALANDOR(humancm1)
		DEALANDOR(flop0)
		DEALANDOR(flop1)
		DEALANDOR(flop2)
		DEALANDOR(turn)
		DEALANDOR(river)
#undef DEALANDOR

		//set the cm's corresponding to groups of cards
		CardMask_OR(botcards, botcm0, botcm1);
		CardMask_OR(humancards, humancm0, humancm1);
		CardMask_OR(flop, flop0, flop1);
		CardMask_OR(flop, flop, flop2);

		//set the cm's corresponding to full final 7-card hands
		CardMask_OR(fullhuman, humancards, flop);
		CardMask_OR(fullhuman, fullhuman, turn);
		CardMask_OR(fullhuman, fullhuman, river);

		CardMask_OR(fullbot, botcards, flop);
		CardMask_OR(fullbot, fullbot, turn);
		CardMask_OR(fullbot, fullbot, river);

		//compute would-be winner, and we're done.
		r0=Hand_EVAL_N(fullhuman, 7);
		r1=Hand_EVAL_N(fullbot, 7);

		if (r0>r1)
			winner = human;
		else if(r1>r0)
			winner = bot;
		else
			winner = -1;
	}
	//inform bot of the new game
	MyBot.setnewgame(bot, botcards, SBLIND, BBLIND);
	//display the cards
	printhumancards();
	if(ShowBotCards.GetCheck())
		printbotcards();
	else
		printbotcardbacks();
}

void CSimpleGameDlg::OnBnClickedCheck1()
{
	// clicked the check box to show/unshow bot cards
	if(ShowBotCards.GetCheck())
		printbotcards();
	else
		printbotcardbacks();
}

void CSimpleGameDlg::OnBnClickedButton6()
{
	//make bot go
	float val;
	Action act;
	//get answer from bot
	act = MyBot.getanswer(val);
	switch(act)
	{
	case FOLD:
		totalhumanwon += pot + invested[bot];
		updatetotal();
		break;

	case CALL:
		if(isallin)
		{
			printbotcards();
			Sleep(1000); //suspense needed.
			printflop();
			Sleep(1000);
			printturn();
			Sleep(1000);
			printriver();
			gameround=RIVER; //hack
			advanceround();
		}
		else
		{
			//inform bot
			MyBot.advancetree(bot,CALL,val);
			//increase pot
			pot += invested[human];
			updatepot();
			//reset invested
			invested[bot]=invested[human]=0;
			updateinvested();

			advanceround();
		}
		break;

	case BET:
		if (val <= invested[human] || val <=invested[bot] || val + pot >= STACKSIZE) 
			REPORT("bot made a bad decision.");
		invested[bot]=val;
		updateinvested();
		MyBot.advancetree(bot,BET,val);
		break;

	case ALLIN:
		invested[bot]=STACKSIZE-pot;
		updateinvested();
		MyBot.advancetree(bot,ALLIN,0);
		isallin=true;
		break;
	}
}

void CSimpleGameDlg::OnBnClickedButton3()
{
	// Bet/Raise
	//get value of edit box
	CString valstr;
	double val;
	BetAmount.GetWindowText(valstr);
	val = wcstod(valstr, NULL);
	//check to make sure it's not shitty
	if (val <= invested[human] || val <= invested[bot]) return;
	if (val + pot >= STACKSIZE) return;
	//reset our invested amount
	invested[human]=val;
	updateinvested();
	//inform the bot
	MyBot.advancetree(human, BET, val);
}

void CSimpleGameDlg::OnBnClickedButton4()
{
	//All-in
	//set our invested amount
	invested[human]=STACKSIZE-pot;
	updateinvested();
	//inform the bot
	MyBot.advancetree(human, ALLIN, 0);
	isallin=true;
}
