// Player Control plugin for FLHookPlugin
// Feb 2010 by Cannon
//
// This is free software; you can redistribute it and/or modify it as
// you wish without restriction. If you do then I would appreciate
// being notified and/or mentioned somewhere.
//
// This file includes code that was not written by me but I can't find
// the original author (I know they posted on the-starport.net about it).

#include <windows.h>
#include <stdio.h>
#include <string>
#include <time.h>
#include <math.h>
#include <float.h>
#include <FLHook.h>
#include <plugin.h>
#include <math.h>
#include <list>
#include <set>

#include <PluginUtilities.h>
#include "Main.h"

#include "Shlwapi.h"

#include <FLCoreServer.h>
#include <FLCoreCommon.h>

namespace Restart
{
	static map<wstring, int> shipPrices;

	// Players with a cash above this value cannot use the restart command.
	static int set_iMaxCash;

	void Restart::LoadSettings(const string &scPluginCfgFile)
	{
		set_iMaxCash = IniGetI(scPluginCfgFile, "Restart", "MaxCash", 1000000);
		LoadShipPrices();
	}

	void Restart::LoadShipPrices() {

		// The path to the configuration file.
		char szCurDir[MAX_PATH];
		GetCurrentDirectory(sizeof(szCurDir), szCurDir);
		string scPluginCfgFile = string(szCurDir) + "\\flhook_plugins\\restart_prices.cfg";

		INI_Reader ini;
		if (ini.open(scPluginCfgFile.c_str(), false))
		{
			while (ini.read_header())
			{
				if (ini.is_header("restarts"))
				{
					while (ini.read_value())
					{
						if (ini.is_value("restart"))
						{
							string setnpcname = ini.get_value_string(0);
							wstring thenpcname = stows(setnpcname);
							int amount = ini.get_value_int(1);
							shipPrices[thenpcname] = amount;
						}
					}
				}
			}
		}
	}

	bool Restart::UserCmd_ShowRestarts(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
	{
		WIN32_FIND_DATA FileData;
		HANDLE hSearch;

		char szCurDir[MAX_PATH];
		GetCurrentDirectory(sizeof(szCurDir), szCurDir);
		string scRestartFiles = string(szCurDir) + "\\flhook_plugins\\restart\\*.fl";

		// Start searching for .fl files in the current directory. 
		hSearch = FindFirstFile(scRestartFiles.c_str(), &FileData);
		if (hSearch == INVALID_HANDLE_VALUE)
		{
			PrintUserCmdText(iClientID, L"Restart files not found");
			return true;
		}

		PrintUserCmdText(iClientID, L"The following restarts are available:");

		do
		{
			wstring wscMsg = L"";
			// add filename
			string scFileName = FileData.cFileName;
			size_t len = scFileName.length();
			scFileName.erase(len - 3, len);
			wstring restartName = stows(scFileName);
			if (scFileName[0] != '_') {
				wscMsg += stows(scFileName) + L" - $";
				wscMsg += stows(itos(shipPrices[restartName]));
				PrintUserCmdText(iClientID, L"%s", wscMsg.c_str());
			}
		} while (FindNextFile(hSearch, &FileData));

		FindClose(hSearch);
		return true;
	}

	struct RESTART
	{
		wstring wscCharname;
		string scRestartFile;
		wstring wscDir;
		wstring wscCharfile;
		int money;
	};
	std::list<RESTART> pendingRestarts;

	bool Restart::UserCmd_Restart(uint iClientID, const wstring &wscCmd, const wstring &wscParam, const wchar_t *usage)
	{
		wstring wscFaction = GetParam(wscParam, ' ', 0);
		if (!wscFaction.length())
		{
			PrintUserCmdText(iClientID, L"Use /showrestarts to view available templates");
			PrintUserCmdText(iClientID, usage);
			return true;
		}

		// Get the character name for this connection.
		RESTART restart;
		restart.wscCharname = (const wchar_t*)Players.GetActiveCharacterName(iClientID);

		// Searching restart
		char szCurDir[MAX_PATH];
		GetCurrentDirectory(sizeof(szCurDir), szCurDir);
		restart.scRestartFile = string(szCurDir) + "\\flhook_plugins\\restart\\" + wstos(wscFaction) + ".fl";
		if (!::PathFileExistsA(restart.scRestartFile.c_str()))
		{
			restart.scRestartFile = string(szCurDir) + "\\flhook_plugins\\restart\\_" + wstos(wscFaction) + ".fl";
			if (!PathFileExistsA(restart.scRestartFile.c_str()))
			{
				PrintUserCmdText(iClientID, L"Template does not exist. Use /showrestarts to view available templates");
				return true;
			}
		}

		// Saving the characters forces an anti-cheat checks and fixes 
		// up a multitude of other problems.
		HkSaveChar(iClientID);
		if (!HkIsValidClientID(iClientID))
			return true;

		uint iBaseID;
		pub::Player::GetBase(iClientID, iBaseID);
		if (!iBaseID)
		{
			PrintUserCmdText(iClientID, L"You must be docked to use this command");
			return true;
		}

		HK_ERROR err;
		int iCash = 0;
		if ((err = HkGetCash(restart.wscCharname, iCash)) != HKE_OK)
		{
			PrintUserCmdText(iClientID, L"ERR " + HkErrGetText(err));
			return true;
		}
		if (iCash < shipPrices[wscFaction])
		{
			PrintUserCmdText(iClientID, L"You need $" + stows(itos((shipPrices[wscFaction] - iCash))) + L" more credits to use this template");
			return true;
		}
		restart.money = iCash - shipPrices[wscFaction];
		CAccount *acc = Players.FindAccountFromClientID(iClientID);
		if (acc)
		{
			HkGetAccountDirName(acc, restart.wscDir);
			HkGetCharFileName(restart.wscCharname, restart.wscCharfile);
			pendingRestarts.push_back(restart);
			HkKickReason(restart.wscCharname, L"Updating character, please wait 10 seconds before reconnecting");
		}
		return true;
	}

	void Timer()
	{
		while (pendingRestarts.size())
		{
			RESTART restart = pendingRestarts.front();
			if (HkGetClientIdFromCharname(restart.wscCharname) != -1)
				return;

			pendingRestarts.pop_front();

			try
			{
				// Overwrite the existing character file
				string scCharFile = scAcctPath + wstos(restart.wscDir) + "\\" + wstos(restart.wscCharfile) + ".fl";
				string scTimeStampDesc = IniGetS(scCharFile, "Player", "description", "");
				string scTimeStamp = IniGetS(scCharFile, "Player", "tstamp", "0");
				if (!::CopyFileA(restart.scRestartFile.c_str(), scCharFile.c_str(), FALSE))
					throw ("copy template");

				flc_decode(scCharFile.c_str(), scCharFile.c_str());
				IniWriteW(scCharFile, "Player", "name", restart.wscCharname);
				IniWrite(scCharFile, "Player", "description", scTimeStampDesc);
				IniWrite(scCharFile, "Player", "tstamp", scTimeStamp);
				IniWrite(scCharFile, "Player", "money", itos(restart.money));
				if (!set_bDisableCharfileEncryption)
					flc_encode(scCharFile.c_str(), scCharFile.c_str());

				AddLog("NOTICE: User restart %s for %s", restart.scRestartFile.c_str(), wstos(restart.wscCharname).c_str());
			}
			catch (char *err)
			{
				AddLog("ERROR: User restart failed (%s) for %s", err, wstos(restart.wscCharname).c_str());
			}
			catch (...)
			{
				AddLog("ERROR: User restart failed for %s", wstos(restart.wscCharname).c_str());
			}
		}
	}
}