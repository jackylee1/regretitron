#pragma once
#include "afxwin.h"
#include <string>


// UserInput dialog

class UserInput : public CDialog
{
	DECLARE_DYNAMIC(UserInput)

public:
	UserInput(std::string mytext, CWnd* pParent = NULL);   // standard constructor
	virtual ~UserInput();
	virtual BOOL OnInitDialog();

// Dialog Data
	enum { IDD = IDD_USERINPUT };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
private:
	std::string mymytext;
public:
	// the text that the user types into the box!
	CEdit inputbox;
	// The text that is shown to the user!
	CStatic inputtext;
	CString inputtext2;
};
