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
void BotAPI::populatewindow()
{
	//create window if needed

	if(MyWindow == NULL)
		MyWindow = new DiagnosticsPage;

	//set actions

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
			CString val;
			val.Format(TEXT("%s"), 
				CSTR(actionstring(a,currentgr,mynode,multiplier)));
				//probabilities[a]*100);
			MyWindow->ActButton[a].SetWindowText(val);
			MyWindow->ActButton[a].EnableWindow(TRUE);
			MyWindow->ActionBars[a].ShowWindow(SW_SHOW);
			MyWindow->ActionBars[a].SetPos((int)(probabilities[a]*100.0));
		}
		if(answer==a) //non-EnabledWindow's can still be checked
			MyWindow->ActButton[a].SetCheck(BST_CHECKED);
		else
			MyWindow->ActButton[a].SetCheck(BST_UNCHECKED);
	}

	//set sceni beti

	CString val;
	val.Format(TEXT("%d"),currentsceni);
	MyWindow->SceniText.SetWindowText(val);
	val.Format(TEXT("%d"),currentbetinode);
	MyWindow->BetiText.SetWindowText(val);

	//get the indices used to label the known information

	int gr, poti, boardi, handi, bethist[3];
	getindices(currentsceni, gr, poti, boardi, handi, bethist);
	if(gr != currentgr) REPORT("Invalid gr returned by getindices. sceni corrupted?");

	//set bethist

	for(int g=PREFLOP; g<RIVER; g++)
	{
		if(g<currentgr)
			MyWindow->Bethist[g].SetWindowText(CSTR(bethiststr(bethist[g])));
		else
			MyWindow->Bethist[g].SetWindowText(TEXT(""));
	}

	//set pot amount

	if(currentgr==PREFLOP)
		MyWindow->PotSizes.SetWindowText(TEXT("$0.00"));
	else
	{
		int lower, upper;
		CString val;
		resolvepoti(currentgr, poti, lower, upper);
		lower = (int)((double)lower-(double)BB/2.0 + 0.5);
		upper = (int)((double)upper+(double)BB/2.0 + 0.5);
		val.Format(TEXT("$%0.2f - $%0.2f"), 2*multiplier*lower, 2*multiplier*upper);
		MyWindow->PotSizes.SetWindowText(val);
	}

	//set perceived invested amounts

	val.Format(TEXT("Human: $%0.2f"), multiplier*perceived[1-myplayer]);
	MyWindow->PerceivedInvestHum.SetWindowText(val);
	val.Format(TEXT("Bot: $%0.2f"), multiplier*perceived[myplayer]);
	MyWindow->PerceivedInvestBot.SetWindowText(val);

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