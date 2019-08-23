#pragma warning(disable: 4146 4996)
struct BOUNTY_HUNT
{
	static int sBhCounter;
	BOUNTY_HUNT(){
		iCounter = sBhCounter++;
	};
	int		iCounter;
	uint	uiTargetID;
	uint	uiInitiatorID;
	wstring wscTarget;
	wstring wscInitiator; 
	uint	uiCredits;
	mstime  msEnd;
	bool operator==(struct BOUNTY_HUNT const & sT);
};

void PrintUniverseText(wstring wscText, ...);
void BhKillCheck(uint uiClientID, uint uiKillerID);
void CancelBh(uint uiClientID);
void BhTimeOutCheck();