#include <vector>
void ClearClientMark(uint iClientID);
void HkUnMarkAllObjects(uint iClientID);
char HkUnMarkObject(uint iClientID, uint iObject);
char HkMarkObject(uint iClientID, uint iObject);

struct MARK_INFO
{
    bool bMarkEverything;
    bool bIgnoreGroupMark;
    float fAutoMarkRadius;
    vector<uint> vMarkedObjs; 
    vector<uint> vDelayedSystemMarkedObjs;
    vector<uint> vAutoMarkedObjs; 
    vector<uint> vDelayedAutoMarkedObjs;
};

struct DELAY_MARK { uint iObj; mstime time; };
string ftos(float f);