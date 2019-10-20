// Lootboxes for FLHook 3.1.0
// September 2019 by Raikkonen
//
// This is free software; you can redistribute it and/or modify it as
// you wish without restriction. If you do then I would appreciate
// being notified and/or mentioned somewhere.

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Includes
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "main.h"

static int set_iPluginDebug = 0;

/// A return code to indicate to FLHook if we want the hook processing to continue.
PLUGIN_RETURNCODE returncode;

void LoadSettings();

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	srand((uint)time(0));
	// If we're being loaded from the command line while FLHook is running then
	// set_scCfgFile will not be empty so load the settings as FLHook only
	// calls load settings on FLHook startup and .rehash.
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		if (set_scCfgFile.length() > 0)
			LoadSettings();
	}
	else if (fdwReason == DLL_PROCESS_DETACH)
	{
	}
	return true;
}

/// Hook will call this function after calling a plugin function to see if we the
/// processing to continue
EXPORT PLUGIN_RETURNCODE Get_PluginReturnCode()
{
	return returncode;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//STRUCTURES AND DEFINITIONS
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct Lootbox {
	wstring Name;
	uint iItemID;
	map<uint, float> Contents; //uint is iItemID and float is chance the lootbox will turn into this item on redeem
	float chance;
};

static map<int, Lootbox> Lootboxes; // uint is ids_name

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Loading Settings
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void LoadSettings()
{
	returncode = DEFAULT_RETURNCODE;

	map<wstring, Lootbox> LootboxMap;

	// The path to the configuration file.
	char szCurDir[MAX_PATH];
	GetCurrentDirectory(sizeof(szCurDir), szCurDir);
	string scPluginCfgFile = string(szCurDir) + "\\flhook_plugins\\lootbox.cfg";

	INI_Reader ini;
	if (ini.open(scPluginCfgFile.c_str(), false))
	{
		while (ini.read_header())
		{
			if (ini.is_header("lootbox"))
			{
				Lootbox lootbox;
				wstring thelootboxname;
				while (ini.read_value())
				{
					if (ini.is_value("lootbox"))
					{
						lootbox.Name = stows(ini.get_value_string(0));
						lootbox.iItemID = CreateID(ini.get_value_string(1));
						lootbox.chance = ini.get_value_float(2);

					}
					else if (ini.is_value("reward"))
					{
						uint iItemID = CreateID(ini.get_value_string(0));
						lootbox.Contents[iItemID] = ini.get_value_float(1);
					}
				}
				LootboxMap[lootbox.Name] = lootbox;
			}
			else if (ini.is_header("drops"))
			{
				while (ini.read_value())
				{
					if (ini.is_value("drop"))
					{
						int ids_name = ini.get_value_int(0);
						wstring lootboxName = stows(ini.get_value_string(1));
						Lootboxes[ids_name] = LootboxMap[lootboxName];
					}
				}
			}
		}
		ini.close();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Functions
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

float RandomChance()
{
	srand(time(NULL));
	int a = 0;
	int b = 100;
	float c = ((b - a)*((float)rand() / RAND_MAX)) + a;
	return roundf(c * 100) / 100;
}

void SpawnLoot(uint iClientID, Lootbox lootbox) {
	// Check they have enough room for the lootbox
	int iRemHoldSize;
	list<CARGO_INFO> lstCargo;
	HkEnumCargo(ARG_CLIENTID(iClientID), lstCargo, iRemHoldSize);
	if (iRemHoldSize > 0) {
		// Add lootbox to hold
		HK_ERROR hkErr = HkAddCargo(ARG_CLIENTID(iClientID),lootbox.iItemID,1,false);
		if (hkErr == HKE_OK) {
			PrintUserCmdText(iClientID, L"You have received a %s Lootbox!",lootbox.Name.c_str());
		}
	}
}

void __stdcall ShipDestroyed(DamageList *_dmg, DWORD *ecx, uint iKill)
{
	returncode = DEFAULT_RETURNCODE;
	if (iKill)
	{
		CShip *cship = (CShip*)ecx[4];

		DamageList dmg;
		try { dmg = *_dmg; }
		catch (...) { return; }

		// Is the killer a player?
		uint iClientID = HkGetClientIDByShip(dmg.get_inflictor_id());
		if (iClientID) {
			// Check the victim is NPC
			uint iVictimID = cship->GetOwnerPlayer();
			if (!iVictimID) {
				// Can the killed ship drop lootboxes?
				for(map<int, Lootbox>::iterator iter = Lootboxes.begin(); iter != Lootboxes.end(); ++iter) 
				{
					if (iter->first == cship->get_name()) //get_name() is the ids_name
					{
						// If chance success, spawn loot
						if (RandomChance() <= iter->second.chance) 
						{
							SpawnLoot(iClientID, iter->second);
						}
					}
				}
			}
		}
	}
}

bool UserCmd_Process(uint iClientID, const wstring &cmd)
{
	returncode = DEFAULT_RETURNCODE;

	if (!cmd.compare(L"/redeem"))
	{
		int iRemHoldSize;
		list<CARGO_INFO> lstCargo;
		HkEnumCargo(ARG_CLIENTID(iClientID), lstCargo, iRemHoldSize);

		foreach(lstCargo, CARGO_INFO, CargoIt)
		{
			for (map<int, Lootbox>::iterator LootboxIt = Lootboxes.begin(); LootboxIt != Lootboxes.end(); ++LootboxIt)
			{
				if (LootboxIt->second.iItemID == CargoIt->iArchID)
				{
					pub::Player::RemoveCargo(iClientID, CargoIt->iID, CargoIt->iCount);
					for (int i = 0; i < CargoIt->iCount; ++i) {
						uint RewardCargoID;
						float counter = 0;
						for (map<uint, float>::iterator RewardIt = LootboxIt->second.Contents.begin(); RewardIt != LootboxIt->second.Contents.end(); ++RewardIt)
						{
							counter += RewardIt->second;
							if (RandomChance() <= counter) {
								RewardCargoID = RewardIt->first;
							}
						}
						HK_ERROR hkErr = HkAddCargo(ARG_CLIENTID(iClientID), RewardCargoID, 1, false);
						if (hkErr == HKE_OK) {
							string ItemName = EquipmentUtilities::FindNickname(RewardCargoID);
							PrintUserCmdText(iClientID, L"You have redeemed the %s Lootbox and received a %s", LootboxIt->second.Name, ItemName);
						}
					}
					return true;
				}
			}
		}
			PrintUserCmdText(iClientID, L"You don't have any lootboxes to redeem");
			return true;
	}
	return false;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////
//Functions to hook
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

EXPORT PLUGIN_INFO* Get_PluginInfo()
{
	PLUGIN_INFO* p_PI = new PLUGIN_INFO();
	p_PI->sName = "Lootboxes by Raikkonen";
	p_PI->sShortName = "lootbox";
	p_PI->bMayPause = true;
	p_PI->bMayUnload = true;
	p_PI->ePluginReturnCode = &returncode;
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&LoadSettings, PLUGIN_LoadSettings, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&UserCmd_Process, PLUGIN_UserCmd_Process, 0));
	p_PI->lstHooks.push_back(PLUGIN_HOOKINFO((FARPROC*)&ShipDestroyed, PLUGIN_ShipDestroyed, 0));

	return p_PI;
}
