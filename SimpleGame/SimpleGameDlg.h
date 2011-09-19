// SimpleGameDlg.h : header file
//

#pragma once

#ifdef USE_NETWORK_CLIENT
#include "../NetworkClient/clientapi.h"
#else
#include "../PokerPlayerMFC/botapi.h"
#endif
#include "../PokerPlayerMFC/picturectrl.h"
#include "../PokerLibrary/constants.h"
#include "afxwin.h"
#include "afxext.h"

// CSimpleGameDlg dialog
class CSimpleGameDlg : public CDialog
{
// Construction
public:
	CSimpleGameDlg(CWnd* pParent = NULL);	// standard constructor
	//~CSimpleGameDlg();

// Dialog Data
	enum { IDD = IDD_SIMPLEGAME_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()

	//needed to fully delete memory, which is nice.
	void PostNcDestroy();
	//needed to call DestroyWindow on this window and any others (diagnosticswindow), 
	//which actually exists app
	void OnCancel();
	void OnOK();

private:
	//functions
	void printbotcards();
	void printbotcardbacks();
	void printhumancards();
	void printflop();
	void printturn();
	void printriver();
	void eraseboard();
	void updatepot();
	void updatess();
    void updateinvested(Player justacted = (Player)-1);
	bool loadbotfile();
	double mintotalwager(Player acting);
	double betincrement();
	void dofold(Player pl);
	void docall(Player pl);
	void dobet(Player pl, double amount);
	void dogameover(bool fold);
	void graygameover();
	void graypostact(Player nexttoact);
	//variables
#ifdef USE_NETWORK_CLIENT
	ClientAPI * _mybot;
#else
	BotAPI * _mybot;
	ConsoleLogger m_logger;
#endif
	MTRand cardrandom;
	int handsplayed;
	int handsplayedtourney;
	double _sblind;
	double _bblind;
	double cashsblind;
	double cashbblind;
	double humanstacksize;
	double botstacksize;
	double effstacksize;
	Player human, bot;
	CardMask humancards, botcards, flop, turn, river;
	CardMask botcm0, botcm1, humancm0, humancm1;
	CardMask flop0, flop1, flop2;
	bool _isallin;
	int _winner;
	int _gameround;
	double invested[2];
	double totalhumanwon;
	double pot;
	bool istournament;

	afx_msg void OnBnClickedButton1();
	afx_msg void OnBnClickedButton3();
	afx_msg void OnBnClickedButton2();
	afx_msg void OnBnClickedButton5();
	afx_msg void OnBnClickedCheck1();
	afx_msg void OnBnClickedButton6();
	afx_msg void OnBnClickedButton4();
	afx_msg void OnBnClickedCheck2();
	afx_msg void OnMenuNewTourney();
	afx_msg void OnMenuPlayCashGame();
	afx_msg void OnMenuSetBotBankroll();
	afx_msg void OnMenuSetAllBankroll();
	afx_msg void OnMenuSetYourBankroll();
	afx_msg void OnLoadbotfile();
	afx_msg void OnShowbotfile();
	afx_msg void OnMenuExit();
	// card images
	CPictureCtrl hCard0;
	CPictureCtrl hCard1;
	CPictureCtrl cCard0;
	CPictureCtrl cCard1;
	CPictureCtrl cCard2;
	CPictureCtrl cCard3;
	CPictureCtrl cCard4;
	CPictureCtrl bCard0;
	CPictureCtrl bCard1;
	CButton ShowBotCards;
	CButton ShowDiagnostics;
	CEdit TotalWon;
	CEdit InvestedHum;
	CEdit InvestedBot;
	CEdit BetAmount;
	CButton FoldCheckButton;
	CButton CallButton;
	CButton BetRaiseButton;
	CButton AllInButton;
	CButton MakeBotGoButton;
	CStatic BotStack;
	CStatic HumanStack;
	CBitmapButton OpenBotButton;
	CButton NewGameButton;
	CButton AutoNewGame;
	CButton AutoBotPlay;
	CStatic PotValue;
	CPictureCtrl HumanDealerChip;
	CPictureCtrl BotDealerChip;
	CMenu MyMenu;
};
