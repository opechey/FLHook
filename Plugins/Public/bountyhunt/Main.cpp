#include <windows.h>
#include <stdio.h>
#include <string>
#include <FLHook.h>
#include <plugin.h>
#include "header.h"

#define PRINT_ERROR_NORETURN() { for(uint i = 0; (i < sizeof(wscError)/sizeof(wstring)); i++) PrintUserCmdText(iClientID, wscError[i]); }
#define PRINT_DISABLED() PrintUserCmdText(iClientID, L"Command disabled");

PLUGIN_RETURNCODE returncode;
bool set_bBhEnabled;
uint	set_iBhAuto;
int     set_iLowLevelProtect;
list<BOUNTY_HUNT> lstBountyHunt;

EXPORT PLUGIN_RETURNCODE Get_PluginReturnCode()
{
	return returncode;
}

EXPORT void UserCmd_Help(uint iClientID, const wstring &wscParam)
{
	PrintUserCmdText(iClientID, L"/bountyhunt <charname> <credits> [<minutes>]");
	PrintUserCmdText(iClientID, L"/bountyhuntid <id> <credits> [<minutes>]");
}

EXPORT void LoadSettings()
{
	string set_scCfgGeneralFile;
    char szCurDir[MAX_PATH];
    GetCurrentDirectory(sizeof(szCurDir), szCurDir);
    set_scCfgGeneralFile = string(szCurDir) + "\\flhook_plugins\\bountyhunt.cfg";
	set_iBhAuto = IniGetI(set_scCfgGeneralFile, "General", "AutoBountyHunt", 0);
	set_bBhEnabled = IniGetB(set_scCfgGeneralFile, "General", "EnableBountyHunt", false);
	set_iLowLevelProtect = IniGetI(set_scCfgGeneralFile, "General", "lvlprotect", 0);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if(fdwReason == DLL_PROCESS_ATTACH)
	LoadSettings();
	return true;
}

EXPORT void SendDeathMsg(const wstring &wscMsg, uint iSystemID, uint iClientIDVictim, uint iClientIDKiller)
{
	returncode = DEFAULT_RETURNCODE;
	if(set_bBhEnabled)
	{
		BhKillCheck(iClientIDVictim,iClientIDKiller);
	}
}

void UserCmd_BountyHunt(uint iClientID, const wstring &wscParam)
{
	if(!set_bBhEnabled)
	{
		PRINT_DISABLED();
		return;
	}

	wstring wscError[] = 
	{
		L"Usage: /bountyhunt <charname> <credits> [<minutes>]",
		L"or: /bountyhuntid <id> <credits> [<minutes>]",
		L" -> Offers a reward <credits> for killing <charname> within the next <minutes>",
	};


	mstime nowtime = timeInMS();
	wstring wscTarget = GetParam(wscParam, ' ', 0);
	wstring wscCredits = GetParam(wscParam, ' ', 1);
	wstring wscTime = GetParam(wscParam, ' ', 2);
	if(!wscTarget.length() || !wscCredits.length())
	{
		PRINT_ERROR_NORETURN();
		if(lstBountyHunt.begin() != lstBountyHunt.end()){
			PrintUserCmdText(iClientID, L"Offered Bounty Hunts:");
			foreach(lstBountyHunt, BOUNTY_HUNT, it)
			{
				PrintUserCmdText(iClientID,L"Kill %s and earn %u credits (%u minutes left)",it->wscTarget.c_str(),it->uiCredits,(uint)((it->msEnd - nowtime) / 60000));
			}
		}
		return;
	}
	
	uint uiTargetID = 0, uiPrize = 0, uiTime = 30;
	uiPrize = wcstol(wscCredits.c_str(),NULL,10);
	uiTime = wcstol(wscTime.c_str(),NULL,10);
	uiTargetID = HkGetClientIdFromCharname(wscTarget);

	int iRankTarget;
	pub::Player::GetRank(uiTargetID, iRankTarget);

	if(uiTargetID == -1 || HkIsInCharSelectMenu(uiTargetID) )
	{
		PrintUserCmdText(iClientID, L"%s is not online", wscTarget.c_str());
		return;
	}

	if(iRankTarget < set_iLowLevelProtect)
	{
		PrintUserCmdText(iClientID, L"Low level character may not be hunted");
		return;
	}
	if(uiPrize > 50000000 || uiPrize == 0)
	{
		PrintUserCmdText(iClientID, L"Prize %u not allowed (min:1 max:50000000)", uiPrize);
		return;
	}
	if(uiTime < 1 || uiTime > 240)
	{
		PrintUserCmdText(iClientID, L"Hunting time: 30 minutes");
		uiTime = 30;
	}
	wstring wscInitiatior = L"Player";
	if(iClientID)
	{
		uint uiClientCash;
		pub::Player::InspectCash(iClientID, (int &)uiClientCash);
		if(uiClientCash < uiPrize)
		{
			PrintUserCmdText(iClientID, L"You do not possess enough credits");
			return;
		}
		pub::Player::AdjustCash(iClientID,-uiPrize);
		wscInitiatior = (wchar_t*)Players.GetActiveCharacterName(iClientID);
	}


	BOUNTY_HUNT bh;
	bh.uiInitiatorID = iClientID;
	bh.msEnd = nowtime + (mstime)(uiTime * 60000);
	bh.wscInitiator = wscInitiatior;
	bh.uiCredits = uiPrize;
	bh.wscTarget = wscTarget;
	bh.uiTargetID = uiTargetID;
	lstBountyHunt.push_back(bh);

	PrintUniverseText(L"%s offers %u credits for killing %s in %u minutes.",bh.wscInitiator.c_str(),bh.uiCredits,bh.wscTarget.c_str(),uiTime);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void UserCmd_BountyHuntId(uint iClientID, const wstring &wscParam)
{
	if(!set_bBhEnabled)
	{
		PRINT_DISABLED();
		return;
	}

	wstring wscError[] = 
	{
		L"Usage: /bountyhunt <charname> <credits> [<minutes>]",
		L"or: /bountyhuntid <id> <credits> [<minutes>]",
		L" -> offers a reward <credits> for killing <charname|id> within the next <minutes>",
	};


	mstime nowtime = timeInMS();
	wstring wscTarget = GetParam(wscParam, ' ', 0);
	wstring wscCredits = GetParam(wscParam, ' ', 1);
	wstring wscTime = GetParam(wscParam, ' ', 2);
	if(!wscTarget.length() || !wscCredits.length())
	{
		PRINT_ERROR_NORETURN();
		if(lstBountyHunt.begin() != lstBountyHunt.end())
		{
			PrintUserCmdText(iClientID, L"Offered Bounty Hunts: ");
			foreach(lstBountyHunt, BOUNTY_HUNT, it)
			{
				PrintUserCmdText(iClientID,L" kill %s and earn %u credits (%u minutes left)",it->wscTarget.c_str(),it->uiCredits,(uint)((it->msEnd - nowtime) / 60000));
			}
		}
		return;
	}

	uint iClientIDTarget = ToInt(wscTarget);
	if(!HkIsValidClientID(iClientIDTarget) || HkIsInCharSelectMenu(iClientIDTarget))
	{
		PrintUserCmdText(iClientID, L"Error: Invalid client id");
		return;
	}

	wstring wscParamNew;
	wstring wscCharName;
	wscCharName = (wchar_t*)Players.GetActiveCharacterName(iClientIDTarget);
	wscParamNew = wstring(wscCharName + L" "+ wscCredits + L" " +wscTime);
	UserCmd_BountyHunt(iClientID, wscParamNew);
}

int BOUNTY_HUNT::sBhCounter = 0;
bool BOUNTY_HUNT::operator ==(struct BOUNTY_HUNT const & sT)
{
	if(iCounter  == sT.iCounter)
	{
		return true;
	}
	return false;
}

void BhTimeOutCheck()
{
	mstime nowtime = timeInMS();
	foreach(lstBountyHunt, BOUNTY_HUNT, it)
	{
		if(it->msEnd < nowtime)
		{
			HkAddCash(it->wscTarget, it->uiCredits);
			PrintUniverseText(L"%s was not hunted down and earned %u credits",it->wscTarget.c_str(),it->uiCredits);
			lstBountyHunt.remove(*it);
			BhTimeOutCheck();
			break;
		}
	}
}

void BhKillCheck(uint uiClientID, uint uiKillerID)
{
	mstime nowtime = timeInMS();
	foreach(lstBountyHunt, BOUNTY_HUNT, it)
	{
		if(it->uiTargetID == uiClientID)
		{
			if(uiKillerID == 0 || uiClientID == uiKillerID)
			{
				PrintUniverseText(L"The hunt for %s still goes on",it->wscTarget.c_str());
			}
			else
			{
				wstring wscWinnerCharname;
				wscWinnerCharname = (wchar_t*)Players.GetActiveCharacterName(uiKillerID);
				if(wscWinnerCharname.size() > 0)
				{
					HkAddCash(wscWinnerCharname, it->uiCredits);
					PrintUniverseText(L"%s has killed %s and earned %u credits",wscWinnerCharname.c_str(),it->wscTarget.c_str(),it->uiCredits);
				}
				else
				{
					HkAddCash(it->wscInitiator, it->uiCredits);
				}

			lstBountyHunt.remove(*it);
			BhKillCheck(uiClientID, uiKillerID);
			break;
			}
		}
	}
}

void CancelBh(uint uiClientID)
{
	mstime nowtime = timeInMS();
	foreach(lstBountyHunt, BOUNTY_HUNT, it){
		if(it->uiTargetID == uiClientID)
		{
			if( it->uiInitiatorID != 0)
			{
				HkAddCash(it->wscInitiator, it->uiCredits);
			}
			PrintUniverseText(L"Dissatisfied, %s withdraws his offer.",it->wscInitiator.c_str());
			lstBountyHunt.remove(*it);
			CancelBh(uiClientID);
			break;
		}
	}
}

void PrintUniverseText(wstring wscText, ...)
{
	wchar_t wszBuf[1024*8] = L"";
	va_list marker;
	va_start(marker, wscText);
	_vsnwprintf(wszBuf, sizeof(wszBuf)-1, wscText.c_str(), marker);
	wstring wscXML = L"<TRA data=\"" + set_wscAdminCmdStyle + L"\" mask=\"-1\"/><TEXT>" + XMLText(wszBuf) + L"</TEXT>";
	HkFMsgU( wscXML);
}

namespace HkIServerImpl
{
	typedef void (*_TimerFunc)();
	struct TIMER
	{
		_TimerFunc	proc;
		mstime		tmIntervallMS;
		mstime		tmLastCall;
	};

	TIMER Timers[] = 
	{
		{BhTimeOutCheck,		2017,		0},
	};

	EXPORT int __stdcall Update()
	{
		returncode = DEFAULT_RETURNCODE;
		
		for(uint i = 0; (i < sizeof(Timers)/sizeof(TIMER)); i++)
		{
			if((timeInMS() - Timers[i].tmLastCall) >= Timers[i].tmIntervallMS)
			{
				Timers[i].tmLastCall = timeInMS();
				Timers[i].proc();
			}
		}
		return 0;
	}

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef void (*_UserCmdProc)(uint, const wstring &);

struct USERCMD
{
	wchar_t *wszCmd;
	_UserCmdProc proc;
};

USERCMD UserCmds[] =
{
	{ L"/bountyhunt",		UserCmd_BountyHunt},
	{ L"/bountyhuntid",     UserCmd_BountyHuntId},
};

EXPORT bool UserCmd_Process(uint iClientID, const wstring &wscCmd)
{

	wstring wscCmdLower = ToLower(wscCmd);

	
	for(uint i = 0; (i < sizeof(UserCmds)/sizeof(USERCMD)); i++)
	{
		if(wscCmdLower.find(ToLower(UserCmds[i].wszCmd)) == 0)
		{
			wstring wscParam = L"";
			if(wscCmd.length() > wcslen(UserCmds[i].wszCmd))
			{
				if(wscCmd[wcslen(UserCmds[i].wszCmd)] != ' ')
					continue;
				wscParam = wscCmd.substr(wcslen(UserCmds[i].wszCmd) + 1);
			}
			UserCmds[i].proc(iClientID, wscParam);

			returncode = SKIPPLUGINS_NOFUNCTIONCALL;
			return true;
		}
	}
	
	returncode = DEFAULT_RETURNCODE;
	return false;
}

EXPORT PLUGIN_INFO* Get_PluginInfo()
{
	PLUGIN_INFO *p_PI = new PLUGIN_INFO();
	p_PI->sName = "Bounty Hunt Plugin";
	p_PI->sShortName = "bountyhunt";
	p_PI->bMayPause = false;
	p_PI->bMayUnload = false;
	p_PI->ePluginReturnCode = &returncode;
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&HkIServerImpl::Update, PLUGIN_HkIServerImpl_Update,0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&UserCmd_Process, PLUGIN_UserCmd_Process,0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&UserCmd_Help, PLUGIN_UserCmd_Help,0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&SendDeathMsg, PLUGIN_SendDeathMsg,0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&LoadSettings, PLUGIN_LoadSettings,0));
	return p_PI;
}