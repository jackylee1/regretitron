#include "stdafx.h"
#include "botapi.h"
#include "DiagnosticsPage.h"
#include "../PokerLibrary/rephands.h"
#include "../PokerLibrary/treenolimit.h"
#include "lookupanswer.h"

#define CSTR(stdstr) CString((stdstr).c_str())

//this function is a member of BotAPI so that it has access to all the
//current state of the bot. MyWindow is also a member of BotAPI, and has public
//controls and wrapper functions to allow me to update the page from outside of its class.
void BotAPI::populatewindow(CWnd* parentwin)
{
	CString text; //used throughout

	//create window if needed

	if(MyWindow == NULL && parentwin != NULL)
		MyWindow = new DiagnosticsPage(parentwin);

	//check if game has started

	if(mynode==NULL) return;

	//set actions

	if(mynode->playertoact == myplayer)
	{
		bool isvalid[9]={false,false,false, false,false,false, false,false,false};
		int temppot = roundtobb(currentpot);
		double probabilities[9];
		getvalidity(temppot,mynode,isvalid);
		readstrategy(currentsceni, currentbetinode, probabilities, mynode->numacts);
		for(int a=0; a<9; a++)
		{
			if(!isvalid[a])
			{
				MyWindow->ActButton[a].SetWindowText(TEXT(""));
				MyWindow->ActButton[a].EnableWindow(FALSE);
				MyWindow->ActionBars[a].ShowWindow(SW_HIDE);
			}
			else
			{
				MyWindow->ActButton[a].SetWindowText(CSTR(actionstring(a,currentgr,mynode,multiplier)));
				MyWindow->ActButton[a].EnableWindow(TRUE);
				MyWindow->ActionBars[a].ShowWindow(SW_SHOW);
				MyWindow->ActionBars[a].SetPos((int)(probabilities[a]*100.0));
			}
			if(answer==a) //non-EnabledWindow's can still be checked
				MyWindow->ActButton[a].SetCheck(BST_CHECKED);
			else
				MyWindow->ActButton[a].SetCheck(BST_UNCHECKED);
		}
	}
	else
	{
		for(int a=0; a<9; a++)
		{
			if(mynode==pfn) //start of game, human is first
			{
				MyWindow->ActButton[a].SetWindowText(TEXT(""));
				MyWindow->ActButton[a].SetCheck(BST_UNCHECKED);
				MyWindow->ActionBars[a].SetPos(0);
			}
			MyWindow->ActButton[a].EnableWindow(FALSE);
		}
	}

	//set sceni beti

	text.Format(TEXT("%d"),currentsceni);
	MyWindow->SceniText.SetWindowText(text);
	text.Format(TEXT("%d"),currentbetinode);
	MyWindow->BetiText.SetWindowText(text);

	//get the indices used to label the known information

	int gr, poti, boardi, handi, bethist[3];
	getindices(currentsceni, gr, poti, boardi, handi, bethist);
	if(gr != currentgr) REPORT("Invalid gr returned by getindices. sceni corrupted?");

	//set bin number

	if(currentgr==PREFLOP)
	{
		MyWindow->BinMax.SetWindowText(TEXT("Bin number:"));
		MyWindow->BinNumber.SetWindowText(TEXT("-"));
	}
	else
	{
		text.Format(TEXT("%d"),handi+1);
		MyWindow->BinNumber.SetWindowText(text);

		if(currentgr==FLOP)
			text.Format(TEXT("Bin number(1-%d):"),BIN_FLOP_MAX);
		else if(currentgr==TURN)
			text.Format(TEXT("Bin number(1-%d):"),BIN_TURN_MAX);
		else if(currentgr==RIVER)
			text.Format(TEXT("Bin number(1-%d):"),BIN_RIVER_MAX);

		MyWindow->BinMax.SetWindowText(text);
	}

	//set bethist

	if(PREFLOP < currentgr && myplayer==P1)
		text.Format(TEXT("bot first - %s"), CSTR(bethiststr(bethist[PREFLOP])));
	else if(PREFLOP < currentgr && myplayer==P0)
		text.Format(TEXT("human first - %s"), CSTR(bethiststr(bethist[PREFLOP])));
	else
		text = L"-";
	MyWindow->Bethist[PREFLOP].SetWindowText(text);

	for(int g=FLOP; g<RIVER; g++)
	{
		if(g < currentgr && myplayer==P0)
			text.Format(TEXT("bot first - %s"), CSTR(bethiststr(bethist[g])));
		else if(g < currentgr && myplayer==P1)
			text.Format(TEXT("human first - %s"), CSTR(bethiststr(bethist[g])));
		else
			text = TEXT("-");
		MyWindow->Bethist[g].SetWindowText(text);
	}

	//set pot amount

	if(currentgr==PREFLOP)
		MyWindow->PotSizes.SetWindowText(TEXT("$0.00"));
	else
	{
		int lower, upper;
		resolvepoti(currentgr, poti, lower, upper);
		lower = (int)((double)lower-(double)BB/2.0 + 0.5);
		upper = (int)((double)upper+(double)BB/2.0 + 0.5);
		text.Format(TEXT("$%0.2f - $%0.2f"), 2*multiplier*lower, 2*multiplier*upper);
		MyWindow->PotSizes.SetWindowText(text);
	}

	//set perceived invested amounts

	text.Format(TEXT("Bot: $%0.2f\nHuman: $%0.2f"), 
		multiplier*perceived[myplayer],
		multiplier*perceived[1-myplayer]);
	MyWindow->PerceivedInvestHum.SetWindowText(text);

	//show representative hands

	MyWindow->sethandindices(currentgr, handi, boardi); //must be called before next function
	MyWindow->RefreshHands(); //same function as is called when button is clicked

	//if preflop, may have stale images of board cards still up

	if(currentgr==PREFLOP)
		MyWindow->RedrawWindow();
}

void BotAPI::destroywindow()
{
	if(MyWindow != NULL)
	{
		MyWindow->DestroyWindow();
		delete MyWindow;
		MyWindow = NULL;
	}
}