#include "stdafx.h"
#include "SimpleGame.h"
#include "SimpleGameDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// --------------------- MFC Class, window stuff ---------------------------

// CSimpleGameDlg dialog
CSimpleGameDlg::CSimpleGameDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CSimpleGameDlg::IDD, pParent),
	  totalhumanwon(0), human(P0), bot(P1),
	  MyBot(false) //turns off diagnostics (default state of checkbox)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	Create(CSimpleGameDlg::IDD, NULL);
	ShowWindow(SW_SHOW);
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
	DDX_Control(pDX, IDC_CHECK2, ShowDiagnostics);
	DDX_Control(pDX, IDC_BUTTON1, FoldCheckButton);
	DDX_Control(pDX, IDC_BUTTON2, CallButton);
	DDX_Control(pDX, IDC_BUTTON3, BetRaiseButton);
	DDX_Control(pDX, IDC_BUTTON4, AllInButton);
	DDX_Control(pDX, IDC_BUTTON6, MakeBotGoButton);
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
	ON_BN_CLICKED(IDC_CHECK2, &CSimpleGameDlg::OnBnClickedCheck2)
END_MESSAGE_MAP()

// CSimpleGameDlg message handlers

//http://www.codeproject.com/KB/dialog/gettingmodeless.aspx
//http://msdn.microsoft.com/en-us/library/sk933a19(VS.80).aspx
//both say to use this function:
//Due to the following, this only frees C++ memory. Not neccesary, but nice.
void CSimpleGameDlg::PostNcDestroy() 
{	
    CDialog::PostNcDestroy();
    delete this;
}
//but then more digging makes me realize that the problem was that
//the window was being "cancelled" not "closed". Closed would have
//destroyed the window, but cancelled does nothing.
//http://msdn.microsoft.com/en-us/library/kw3wtttf.aspx
//"If you implement the Cancel button in a modeless dialog box, 
// you must override the OnCancel method and call DestroyWindow 
// inside it. Do not call the base-class method, because it calls 
// EndDialog, which will make the dialog box invisible but not 
// destroy it."
//Okay.
void CSimpleGameDlg::OnCancel()
{
	MyBot.setdiagnostics(false); //this will DestroyWindow the diagnostics page if it exists.
	DestroyWindow();
}

//this is a hack to make the enter key call the raise button
//handler and do nothing else (like close the window).
//fixes everything in one fell swoop.
//http://www.flounder.com/dialogapp.htm
void CSimpleGameDlg::OnOK()
{
	if(BetRaiseButton.IsWindowEnabled())
		OnBnClickedButton3();
}

BOOL CSimpleGameDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here
	TotalWon.SetWindowText(TEXT("$0.00"));
	graygameover();

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




// -------------------------- Card Printing Functions -------------------------

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
void CSimpleGameDlg::eraseboard()
{
	cCard0.FreeData();
	cCard1.FreeData();
	cCard2.FreeData();
	cCard3.FreeData();
	cCard4.FreeData();
	this->RedrawWindow();
}



// ---------------- On-screen Update Functions -------------------

void CSimpleGameDlg::recalctotal()
{
	//we assume this function updates totalhumanwon as well as print it
	if (winner==human)
		totalhumanwon += (pot + invested[bot]);
	else if (winner==bot)
		totalhumanwon -= (pot + invested[human]);
	//reprint the total to the screen
	CString val;
	val.Format(TEXT("$%.2f"), totalhumanwon);
	TotalWon.SetWindowText(val);
}
void CSimpleGameDlg::updatepot()
{
	//reprint the pot to the screen
	CString val;
	val.Format(TEXT("$%.2f"), 2*pot);
	PotTotal.SetWindowText(val);
}
void CSimpleGameDlg::updateinvested()
{
	//reprint the pot to the screen
	CString val;
	val.Format(TEXT("$%.2f"), invested[human]);
	InvestedHum.SetWindowText(val);
	val.Format(TEXT("$%.2f"), invested[bot]);
	InvestedBot.SetWindowText(val);
}



// ------------------------- Game Logic --------------------------------

double CSimpleGameDlg::mintotalwager(Player acting)
{
	//calling from the SBLIND is a special case of how much we can wager
	if(gameround == PREFLOP && invested[acting]==SBLIND)
		return BBLIND;
	
	//we're going first and nothing's been bet yet
	if(gameround != PREFLOP && acting==P0 && invested[1-acting]==0)
		return 0;

	//otherwise, this is the standard formula that FullTilt seems to follow
	double prevwager = invested[1-acting]-invested[acting];
	return invested[acting] + max(BBLIND, prevwager);
}
void CSimpleGameDlg::dofold(Player pl)
{
	//set the winner to the other player and then recalctotal will handle it
	winner = 1-pl; //1-pl returns the other player, since they are 0 and 1
	recalctotal();
	graygameover();
}
void CSimpleGameDlg::docall(Player pl)
{
	//inform bot unless game is over
	if(!isallin && gameround != RIVER)
		MyBot.advancetree(pl,CALL,invested[1-pl]);
	//increase pot
	pot += invested[1-pl];
	updatepot();
	//reset invested
	invested[bot]=invested[human]=0;
	updateinvested();

	//we may be calling an all-in if the previous player bet that.
	//if so, show our cards and fast forward to the river, 
	if(isallin)
	{
		printbotcards();
		Sleep(3000); //suspense needed.
		printflop();
		Sleep(1000);
		printturn();
		Sleep(1000);
		printriver();
		gameround=RIVER;
	}

	switch(gameround)
	{
	//for the first three rounds, we just inform the bot and then print the cards.
	case PREFLOP: 
		MyBot.setflop(flop, pot); 
		printflop();
		graypostact(P0); //P0 is first to act post flop
		break;

	case FLOP: 
		MyBot.setturn(turn, pot); 
		printturn();
		graypostact(P0);
		break;

	case TURN: 
		MyBot.setriver(river, pot); 
		printriver();
		graypostact(P0);
		break;

	case RIVER:
		//game over
		printbotcards();
		recalctotal();
		graygameover();
		break;

	default:
		REPORT("invalid gameround in docall");
	}

	//finally, move on to the next gameround, rendering it invalid if this was river
	gameround ++;
}
void CSimpleGameDlg::dobet(Player pl, double amount)
{
	//we can check the amount for validity. This is important since we
	//will be getting values from the bot here.
	if (amount < mintotalwager(pl) || amount + pot >= STACKSIZE)
		REPORT("someone bet an illegal amount.");

	//inform the bot
	MyBot.advancetree(pl,BET,amount);
	//set our invested amount
	invested[pl] = amount;
	updateinvested();
	//other players turn now
	graypostact(Player(1-pl));
}
void CSimpleGameDlg::doallin(Player pl)
{
	//inform the bot
	MyBot.advancetree(pl, ALLIN, 0);
	isallin=true;
	//set our invested amount
	invested[pl]=STACKSIZE-pot;
	updateinvested();
	//other players turn now
	graypostact(Player(1-pl));
}

// ---------------------------- Setting grayness -----------------------------

void CSimpleGameDlg::graygameover()
{
	//gray out all but newgame
	FoldCheckButton.SetWindowText(TEXT("Fold/Check"));
	FoldCheckButton.EnableWindow(FALSE);
	CallButton.EnableWindow(FALSE);
	BetRaiseButton.EnableWindow(FALSE);
	AllInButton.EnableWindow(FALSE);
	MakeBotGoButton.EnableWindow(FALSE);
}

void CSimpleGameDlg::graypostact(Player nexttoact)
{
	//new game is NEVER grayed out

	if(nexttoact == bot)
	{
		FoldCheckButton.SetWindowText(TEXT("Fold/Check"));
		FoldCheckButton.EnableWindow(FALSE);
		CallButton.EnableWindow(FALSE);
		BetRaiseButton.EnableWindow(FALSE);
		AllInButton.EnableWindow(FALSE);
		MakeBotGoButton.EnableWindow(TRUE);
	}
	else //next to act is human
	{
		//gray out makebotgo
		MakeBotGoButton.EnableWindow(FALSE);

		if(isallin) //can't go all in twice
			AllInButton.EnableWindow(FALSE);
		else
			AllInButton.EnableWindow(TRUE);

		if(invested[bot]==0 || (gameround == PREFLOP && 
			        invested[bot]==BBLIND && invested[human]==BBLIND))
		{
			//set fold/check to check, not grayed
			FoldCheckButton.SetWindowText(TEXT("Check"));
			FoldCheckButton.EnableWindow(TRUE);
			//gray out call
			CallButton.EnableWindow(FALSE);
		}
		else
		{
			//set fold/check to fold, not grayed
			FoldCheckButton.SetWindowText(TEXT("Fold"));
			FoldCheckButton.EnableWindow(TRUE);
			//call active
			CallButton.EnableWindow(TRUE);
		}

		//if min bet + our pot share < STACKSIZE
		if(mintotalwager(human) + pot < STACKSIZE)
			BetRaiseButton.EnableWindow(TRUE);
		else
			BetRaiseButton.EnableWindow(FALSE);
	}
}

// ------------------------- Button/Event Handlers ---------------------------

// ------------------------------ Check Boxes --------------------------------
// clicked the check box to show/unshow bot cards
void CSimpleGameDlg::OnBnClickedCheck1()
{
	//we indescriminately print cards or cardbacks. 
	//if called before any game starts, this will print garbage cards. that's ok.
	if(ShowBotCards.GetCheck())
		printbotcards();
	else
		printbotcardbacks();
}

// clicked the check box to show/unshow diagnostics window
void CSimpleGameDlg::OnBnClickedCheck2()
{
	//all window handling is done by MyBot. 
	//This function is our only interaction
	//(it decides when it's appropriate to pop up the window, and it closes it)
	if(ShowDiagnostics.GetCheck())
		MyBot.setdiagnostics(true);
	else
		MyBot.setdiagnostics(false);
}

// ---------------------------- Action Buttons -------------------------------
// This is the fold/check button.
void CSimpleGameDlg::OnBnClickedButton1()
{
	//if this is true, we are checking
	if(invested[bot]==invested[human])
	{
		//if we are first to act, then this is a "bet" as the gr continues
		if((gameround == PREFLOP && human==P1) ||
			(gameround !=PREFLOP && human == P0))
			dobet(human, 0);
		//otherwise we are second to act. then we are calling a check and the gr ends.
		else
			docall(human);
	}
	//there is a difference in invested which we are unwilling to reconcile
	else if (invested[bot] > invested[human])
		dofold(human);
	else
		REPORT("inconsistant invested amounts");
}

// This is the call button.
void CSimpleGameDlg::OnBnClickedButton2()
{
	//if this is preflop and we are calling from the SB, 
	// then that's actually a "bet" as it doesn't end the gr
	if(gameround == PREFLOP && invested[human] == SBLIND)
		dobet(human, BBLIND);
	//all other cases, a call is a call
	else
		docall(human);
}

// This is the bet/raise button
void CSimpleGameDlg::OnBnClickedButton3()
{
	//the bet/raise button always has the meaning of betting/raising,
	// (that is, meaning as defined by my poker api or, rather, BotAPI)

	//first get value of edit box
	CString valstr;
	double val;
	BetAmount.GetWindowText(valstr);
	val = wcstod(valstr, NULL); //converts string to double

	//input checking: make sure it's not too small or too big, then do it.
	if(val >= mintotalwager(human) && val + pot < STACKSIZE)
		dobet(human, val);
}

// This is the all-in button
void CSimpleGameDlg::OnBnClickedButton4()
{
	//we can do an all-in at any time, for any reason, baby.
	doallin(human);
}

// This is the new game button
void CSimpleGameDlg::OnBnClickedButton5()
{
	CardMask usedcards, fullhuman, fullbot;
	HandVal r0, r1;

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

	//inform bot of the new game
	MyBot.setnewgame(bot, botcards, SBLIND, BBLIND, STACKSIZE);

	//display the cards pics
	eraseboard();
	printhumancards();
	if(ShowBotCards.GetCheck())
		printbotcards();
	else
		printbotcardbacks();

	//finally set grayness
	graypostact(P1); //we'll just call the blinds an action as far as the name of that function goes
}

//This is the "make bot go" button.
void CSimpleGameDlg::OnBnClickedButton6()
{
	double val;
	Action act;
	//get answer from bot
	act = MyBot.getanswer(val);

	//the bot uses the same semantics, so this is easy
	switch(act)
	{
	case FOLD: dofold(bot); break;
	case CALL: docall(bot); break;
	case BET: dobet(bot, val); break;
	case ALLIN: doallin(bot); break;
	}
}
