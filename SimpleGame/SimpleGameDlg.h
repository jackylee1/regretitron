// SimpleGameDlg.h : header file
//

#pragma once

#include "../PokerPlayerMFC/botapi.h"
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
    void updateinvested(Player justacted = (Player)-1);
	double mintotalwager(Player acting);
	double limitbetincrement();
	void dofold(Player pl);
	void docall(Player pl);
	void dobet(Player pl, double amount);
	void dogameover(bool fold);
	void doallin(Player pl);
	void graygameover();
	void graypostact(Player nexttoact);
	//variables
	BotAPI * _mybot;
	MTRand cardrandom;
	int handsplayed;
	const double _sblind;
	const double _bblind;
	double _stacksize;
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

	afx_msg void OnBnClickedButton1();
	afx_msg void OnBnClickedButton3();
	afx_msg void OnBnClickedButton2();
	afx_msg void OnBnClickedButton5();
	afx_msg void OnBnClickedCheck1();
	afx_msg void OnBnClickedButton6();
	afx_msg void OnBnClickedButton4();
	afx_msg void OnBnClickedCheck2();
	afx_msg void OnBnClickedOpenfile();
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
};
