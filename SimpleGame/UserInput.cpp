// UserInput.cpp : implementation file
//

#include "stdafx.h"
#include "SimpleGame.h"
#include "UserInput.h"


// UserInput dialog

IMPLEMENT_DYNAMIC(UserInput, CDialog)

UserInput::UserInput(std::string mytext, CWnd* pParent /*=NULL*/)
	: CDialog(UserInput::IDD, pParent),
	mymytext(mytext)

	, inputtext2(_T(""))
{
}

UserInput::~UserInput()
{
}

BOOL UserInput::OnInitDialog()
{
	CDialog::OnInitDialog();
	inputtext.SetWindowText(mymytext.c_str());
	return TRUE;
} 

void UserInput::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT1, inputbox);
	DDX_Control(pDX, IDC_INPUTTEXT, inputtext);
	DDX_Text(pDX, IDC_EDIT1, inputtext2);
}


BEGIN_MESSAGE_MAP(UserInput, CDialog)
END_MESSAGE_MAP()


// UserInput message handlers
