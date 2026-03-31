#include "user.h"
#include <map>

class CustomJewel
{
public:
    CustomJewel(void);

    virtual ~CustomJewel(void);
    bool IsCustomJewel(int itemId);
    bool ProcessCustomJewel(LPOBJ lpObj, int JewelPos, int TargetPos);

	bool InsertJewelOfElement(LPOBJ lpObj, int JewelPos, int TargetPos);

    void LoadJewelOfHex(char* path);
    bool GetJewelOfHexTarget(int source, int& target);
	bool InsertJewelOfHex(LPOBJ lpObj, int JewelPos, int TargetPos);

    bool InsertJewelOfDarkBless(LPOBJ lpObj, int JewelPos, int TargetPos);
    bool InsertJewelOfDarkSoul(LPOBJ lpObj, int JewelPos, int TargetPos);
    bool InsertJewelOfDarkLife(LPOBJ lpObj, int JewelPos, int TargetPos);
    bool InsertJewelOfExcess(LPOBJ lpObj, int JewelPos, int TargetPos);
    bool InsertJewelOfLuck(LPOBJ lpObj, int JewelPos, int TargetPos);
    bool InsertJewelOfScience(LPOBJ lpObj, int JewelPos, int TargetPos);
    bool InsertJewelOfKundun(LPOBJ lpObj, int JewelPos, int TargetPos);
    bool InsertJewelOfKondar(LPOBJ lpObj, int JewelPos, int TargetPos);
    bool InsertJewelOfWisdom(LPOBJ lpObj, int JewelPos, int TargetPos);

private:
    int m_JewelOfHexEnable;
    int m_JewelOfHexCheckClass;
    int m_JewelOfHexRequireSameSlot;
    int m_JewelOfHexAllowExcellent;
    int m_JewelOfHexAllowAncient;
    int m_JewelOfHexAllowSocket;
    int m_JewelOfHexSuccessRate;
    std::map<int, int> m_HexItemMap;

};
extern CustomJewel gCustomJewelSystem;