// my version of the custom jewels, fixed for your current GS core
#include "StdAfx.h"
#include "CustomJewel.h"
#include "DSProtocol.h"
#include "ItemManager.h"
#include "Notice.h"
#include "Util.h"
#include "RandomManager.h"
#include "ObjectManager.h"
#include "../../pugixml/pugixml.hpp"
#include <fstream>
#include <vector>
#include <string>
#include <sstream>

CustomJewel gCustomJewelSystem;

namespace
{
	struct NormalItemEntry
	{
		int section;
		int type;
	};

	struct SocketItemEntry
	{
		int section;
		int type;
	};

	struct HEX_ITEM_INFO
	{
		int Source;
		int Target;
	};

	bool IsCommentOrEmpty(const std::string& line)
	{
		if (line.empty())
		{
			return true;
		}

		if (line.size() >= 2 && line[0] == '/' && line[1] == '/')
		{
			return true;
		}

		return false;
	}

	std::vector<NormalItemEntry> LoadAllowedNormalItems(const std::string& filename)
	{
		std::vector<NormalItemEntry> items;
		std::string path = "..\\Data\\Custom\\" + filename;
		std::ifstream file(path);

		if (!file)
		{
			LogAdd(eLogColor::LOG_RED, "[CustomItemJewelsNormal] Unable to open file: %s", path.c_str());
			return items;
		}

		NormalItemEntry entry;
		std::string line;
		int lineNumber = 0;

		while (std::getline(file, line))
		{
			lineNumber++;

			if (IsCommentOrEmpty(line) || line.find("end") != std::string::npos)
			{
				continue;
			}

			std::istringstream iss(line);

			if (!(iss >> entry.section >> entry.type))
			{
				LogAdd(eLogColor::LOG_RED, "[CustomItemJewelsNormal] Line %d, error reading line", lineNumber);
				continue;
			}

			items.push_back(entry);
		}

		return items;
	}

	std::vector<SocketItemEntry> LoadAllowedSocketItems(const std::string& filename)
	{
		std::vector<SocketItemEntry> items;
		std::string path = "..\\Data\\Item\\" + filename;
		std::ifstream file(path);

		if (!file)
		{
			LogAdd(eLogColor::LOG_RED, "[CustomItemJewelsSocket] Unable to open file: %s", path.c_str());
			return items;
		}

		std::string line;

		while (std::getline(file, line))
		{
			// elimină comentarii inline //
			size_t pos = line.find("//");
			if (pos != std::string::npos)
			{
				line = line.substr(0, pos);
			}

			// skip dacă e goală
			if (line.empty())
			{
				continue;
			}

			std::istringstream iss(line);
			SocketItemEntry entry;

			// citește DOAR primele 2 valori (restul ignorat)
			if (iss >> entry.section >> entry.type)
			{
				items.push_back(entry);
			}
		}

		return items;
	}

	void SendNotice(LPOBJ lpObj, char* message)
	{
		gNotice.GCNoticeSend(lpObj->Index, 1, 0, 0, 0, 0, 0, message);
	}

	void SendItemUpdate(LPOBJ lpObj, int slot)
	{
		gItemManager.GCItemModifySend(lpObj->Index, slot);
	}

	void ConsumeJewel(LPOBJ lpObj, int jewelPos)
	{
		if (lpObj->Inventory[jewelPos].m_Durability > 1)
		{
			lpObj->Inventory[jewelPos].m_Durability -= 1;
			gItemManager.GCItemModifySend(lpObj->Index, jewelPos);
		}
		else
		{
			gItemManager.InventoryDelItem(lpObj->Index, jewelPos);
			gItemManager.GCItemDeleteSend(lpObj->Index, jewelPos, 1);
		}
	}

	bool IsAllowedNormalTarget(LPOBJ lpObj, int targetPos, const std::vector<NormalItemEntry>& items)
	{
		for (std::vector<NormalItemEntry>::const_iterator it = items.begin(); it != items.end(); ++it)
		{
			if (lpObj->Inventory[targetPos].m_Index == GET_ITEM(it->section, it->type))
			{
				return true;
			}
		}

		return false;
	}

	bool ApplySimpleFixedResult(LPOBJ lpObj, int jewelPos, int targetPos, char* message)
	{
		SendItemUpdate(lpObj, targetPos);
		ConsumeJewel(lpObj, jewelPos);
		SendNotice(lpObj, message);
		return true;
	}
}

CustomJewel::CustomJewel(void)
{
	this->m_JewelOfHexEnable = 0;
	this->m_JewelOfHexCheckClass = 1;
	this->m_JewelOfHexRequireSameSlot = 1;
	this->m_JewelOfHexAllowExcellent = 0;
	this->m_JewelOfHexAllowAncient = 0;
	this->m_JewelOfHexAllowSocket = 0;
	this->m_JewelOfHexSuccessRate = 100;

	this->m_HexItemMap.clear();
}

CustomJewel::~CustomJewel(void)
{
}

bool CustomJewel::IsCustomJewel(int itemId)
{
	return (itemId >= GET_ITEM(14, 474) && itemId <= GET_ITEM(14, 508));
}

bool CustomJewel::ProcessCustomJewel(LPOBJ lpObj, int JewelPos, int TargetPos)
{
	if (JewelPos < 0 || JewelPos >= INVENTORY_SIZE)
	{
		return false;
	}

	if (TargetPos < 0 || TargetPos >= INVENTORY_SIZE)
	{
		return false;
	}

	if (lpObj->Inventory[JewelPos].IsItem() == 0 || lpObj->Inventory[TargetPos].IsItem() == 0)
	{
		return false;
	}

	int JewelCode = lpObj->Inventory[JewelPos].m_Index;

	switch (JewelCode)
	{
	case GET_ITEM(14, 474):
		return this->InsertJewelOfElement(lpObj, JewelPos, TargetPos);
	case GET_ITEM(14, 475):
		return this->InsertJewelOfHex(lpObj, JewelPos, TargetPos);
	case GET_ITEM(14, 500):
		return this->InsertJewelOfDarkBless(lpObj, JewelPos, TargetPos);
	case GET_ITEM(14, 501):
		return this->InsertJewelOfDarkSoul(lpObj, JewelPos, TargetPos);
	case GET_ITEM(14, 502):
		return this->InsertJewelOfDarkLife(lpObj, JewelPos, TargetPos);
	case GET_ITEM(14, 503):
		return this->InsertJewelOfExcess(lpObj, JewelPos, TargetPos);
	case GET_ITEM(14, 504):
		return this->InsertJewelOfLuck(lpObj, JewelPos, TargetPos);
	case GET_ITEM(14, 505):
		return this->InsertJewelOfScience(lpObj, JewelPos, TargetPos);
	case GET_ITEM(14, 506):
		return this->InsertJewelOfKundun(lpObj, JewelPos, TargetPos);
	case GET_ITEM(14, 507):
		return this->InsertJewelOfKondar(lpObj, JewelPos, TargetPos);
	case GET_ITEM(14, 508):
		return this->InsertJewelOfWisdom(lpObj, JewelPos, TargetPos);
	default:
		return false;
	}
}

bool CustomJewel::InsertJewelOfElement(LPOBJ lpObj, int JewelPos, int TargetPos)
{
	CItem* lpItem = &lpObj->Inventory[TargetPos];

	if (lpItem->IsPentagramItem() != 0 || lpItem->IsPentagramJewel() != 0 || lpItem->IsPentagramMithril() != 0)
	{
		int ElementalOption = 0;
		CRandomManager RandomManager;

		RandomManager.AddElement(ELEMENTAL_ATTRIBUTE_FIRE, 100);
		RandomManager.AddElement(ELEMENTAL_ATTRIBUTE_WATER, 100);
		RandomManager.AddElement(ELEMENTAL_ATTRIBUTE_EARTH, 100);
		RandomManager.AddElement(ELEMENTAL_ATTRIBUTE_WIND, 100);
		RandomManager.AddElement(ELEMENTAL_ATTRIBUTE_DARK, 100);
		RandomManager.GetRandomElement(&ElementalOption);

		lpItem->m_SocketOptionBonus = ElementalOption;

		lpItem->Convert(lpItem->m_Index,
			lpItem->m_Option1,
			lpItem->m_Option2,
			lpItem->m_Option3,
			lpItem->m_NewOption,
			lpItem->m_SetOption,
			lpItem->m_JewelOfHarmonyOption,
			lpItem->m_ItemOptionEx,
			lpItem->m_SocketOption,
			lpItem->m_SocketOptionBonus);

		gItemManager.GCItemChangeSend(lpObj->Index, TargetPos);
		gObjectManager.CharacterMakePreviewCharSet(lpObj->Index);

		return ApplySimpleFixedResult(lpObj, JewelPos, TargetPos, "Use Jewel of Element successful.");
	}

	SendNotice(lpObj, "This item can not use Jewel of Element.");
	return false;
}

void CustomJewel::LoadJewelOfHex(char* path)
{
	this->m_HexItemMap.clear();

	pugi::xml_document file;

	pugi::xml_parse_result res = file.load_file(path);

	if (res == false)
	{
		LogAdd(eLogColor::LOG_RED, "[JewelOfHex] Could not load file: %s", path);
		return;
	}

	pugi::xml_node mainNode = file.child("JewelOfHex");

	if (mainNode.empty() != 0)
	{
		LogAdd(eLogColor::LOG_RED, "[JewelOfHex] Main node missing: %s", path);
		return;
	}

	pugi::xml_node settings = mainNode.child("Settings");

	if (settings.empty() == 0)
	{
		this->m_JewelOfHexEnable = settings.attribute("Enable").as_int(0);
		this->m_JewelOfHexCheckClass = settings.attribute("CheckClass").as_int(1);
		this->m_JewelOfHexRequireSameSlot = settings.attribute("RequireSameSlot").as_int(1);
		this->m_JewelOfHexAllowExcellent = settings.attribute("AllowExcellent").as_int(0);
		this->m_JewelOfHexAllowAncient = settings.attribute("AllowAncient").as_int(0);
		this->m_JewelOfHexAllowSocket = settings.attribute("AllowSocket").as_int(0);
		this->m_JewelOfHexSuccessRate = settings.attribute("SuccessRate").as_int(100);
	}

	pugi::xml_node itemList = mainNode.child("ItemList");

	for (pugi::xml_node item = itemList.child("Item"); item; item = item.next_sibling("Item"))
	{
		int source = item.attribute("Source").as_int(-1);
		int target = item.attribute("Target").as_int(-1);

		if (source < 0 || target < 0)
		{
			continue;
		}

		this->m_HexItemMap[source] = target;
	}
}

bool CustomJewel::GetJewelOfHexTarget(int source, int& target)
{
	std::map<int, int>::iterator it = this->m_HexItemMap.find(source);

	if (it == this->m_HexItemMap.end())
	{
		return false;
	}

	target = it->second;
	return true;
}

bool CustomJewel::InsertJewelOfHex(LPOBJ lpObj, int JewelPos, int TargetPos)
{
	if (this->m_JewelOfHexEnable == 0)
	{
		return false;
	}

	CItem* lpItem = &lpObj->Inventory[TargetPos];

	if (lpItem->IsItem() == 0)
	{
		return false;
	}

	int source = lpItem->m_Index;
	int target = -1;

	if (this->GetJewelOfHexTarget(source, target) == false)
	{
		return false;
	}

	// blocare excellent
	if (this->m_JewelOfHexAllowExcellent == 0 && lpItem->m_NewOption != 0)
	{
		return false;
	}

	// blocare ancient
	if (this->m_JewelOfHexAllowAncient == 0 && lpItem->m_SetOption != 0)
	{
		return false;
	}

	// blocare socket
	if (this->m_JewelOfHexAllowSocket == 0)
	{
		for (int n = 0; n < MAX_SOCKET_OPTION; n++)
		{
			if (lpItem->m_SocketOption[n] != 0xFF && lpItem->m_SocketOption[n] != 0xFE)
			{
				return false;
			}
		}
	}

	// success rate
	if (this->m_JewelOfHexSuccessRate < 100)
	{
		if ((GetLargeRand() % 100) >= this->m_JewelOfHexSuccessRate)
		{
			gItemManager.InventoryDelItem(lpObj->Index, JewelPos);
			gItemManager.GCItemDeleteSend(lpObj->Index, JewelPos, 1);
			return false;
		}
	}

	BYTE SocketOption[MAX_SOCKET_OPTION];

	for (int n = 0; n < MAX_SOCKET_OPTION; n++)
	{
		SocketOption[n] = lpItem->m_SocketOption[n];
	}

	lpItem->m_Index = target;

	lpItem->Convert(lpItem->m_Index,
		lpItem->m_Option1,
		lpItem->m_Option2,
		lpItem->m_Option3,
		lpItem->m_NewOption,
		lpItem->m_SetOption,
		lpItem->m_JewelOfHarmonyOption,
		lpItem->m_ItemOptionEx,
		SocketOption,
		lpItem->m_SocketOptionBonus);

	gItemManager.GCItemModifySend(lpObj->Index, TargetPos);
	gObjectManager.CharacterCalcAttribute(lpObj->Index);

	// STERGI JEWEL
	return ApplySimpleFixedResult(lpObj, JewelPos, TargetPos, "Use Jewel of Hex successful. Your item has been transformed.");
}

bool CustomJewel::InsertJewelOfDarkBless(LPOBJ lpObj, int JewelPos, int TargetPos)
{
	if (lpObj->Inventory[TargetPos].m_Level >= 6)
	{
		SendNotice(lpObj, "Use with items level 5 or lower.");
		return false;
	}

	std::vector<NormalItemEntry> allowedItems = LoadAllowedNormalItems("CustomJewelItems.ini");

	if (IsAllowedNormalTarget(lpObj, TargetPos, allowedItems) == false)
	{
		SendNotice(lpObj, "This item can not use Jewel of Dark Bless.");
		return false;
	}

	lpObj->Inventory[TargetPos].m_Level = 6;
	return ApplySimpleFixedResult(lpObj, JewelPos, TargetPos, "Use Jewel of Dark Bless successful. Your item is now +6");
}

bool CustomJewel::InsertJewelOfDarkSoul(LPOBJ lpObj, int JewelPos, int TargetPos)
{
	if (lpObj->Inventory[TargetPos].m_Level < 6 || lpObj->Inventory[TargetPos].m_Level >= 9)
	{
		SendNotice(lpObj, "Use with items level 6 to 8.");
		return false;
	}

	std::vector<NormalItemEntry> allowedItems = LoadAllowedNormalItems("CustomJewelItems.ini");

	if (IsAllowedNormalTarget(lpObj, TargetPos, allowedItems) == false)
	{
		SendNotice(lpObj, "This item can not use Jewel of Dark Soul.");
		return false;
	}

	lpObj->Inventory[TargetPos].m_Level = 9;
	return ApplySimpleFixedResult(lpObj, JewelPos, TargetPos, "Use Jewel of Dark Soul successful. Your item is now +9");
}

bool CustomJewel::InsertJewelOfDarkLife(LPOBJ lpObj, int JewelPos, int TargetPos)
{
	if (lpObj->Inventory[TargetPos].m_Option3 >= 7)
	{
		SendNotice(lpObj, "Use with items Opt level 6 or lower.");
		return false;
	}

	std::vector<NormalItemEntry> allowedItems = LoadAllowedNormalItems("CustomJewelItems.ini");

	if (IsAllowedNormalTarget(lpObj, TargetPos, allowedItems) == false)
	{
		SendNotice(lpObj, "This item can not use Jewel of Dark Life.");
		return false;
	}

	lpObj->Inventory[TargetPos].m_Option3 = 7;
	return ApplySimpleFixedResult(lpObj, JewelPos, TargetPos, "Use Jewel of Dark Life successful. Your item is now +7 Opt");
}

bool CustomJewel::InsertJewelOfExcess(LPOBJ lpObj, int JewelPos, int TargetPos)
{
	if (lpObj->Inventory[TargetPos].m_NewOption >= 63)
	{
		SendNotice(lpObj, "Use with items 5 or less Exe Opt.");
		return false;
	}

	std::vector<NormalItemEntry> allowedItems = LoadAllowedNormalItems("CustomJewelItems.ini");

	if (IsAllowedNormalTarget(lpObj, TargetPos, allowedItems) == false)
	{
		SendNotice(lpObj, "This item can not use Jewel of Excess.");
		return false;
	}

	lpObj->Inventory[TargetPos].m_NewOption = 63;
	return ApplySimpleFixedResult(lpObj, JewelPos, TargetPos, "Use Jewel of Excess successful. Your item is now FO");
}

bool CustomJewel::InsertJewelOfLuck(LPOBJ lpObj, int JewelPos, int TargetPos)
{
	if (lpObj->Inventory[TargetPos].m_Option2 != 0)
	{
		SendNotice(lpObj, "Use with items with no Luck.");
		return false;
	}

	std::vector<NormalItemEntry> allowedItems = LoadAllowedNormalItems("CustomJewelItems.ini");

	if (IsAllowedNormalTarget(lpObj, TargetPos, allowedItems) == false)
	{
		SendNotice(lpObj, "This item can not use Jewel of Luck.");
		return false;
	}

	lpObj->Inventory[TargetPos].m_Option2 = 1;
	return ApplySimpleFixedResult(lpObj, JewelPos, TargetPos, "Use Jewel of Luck successful. Your item now has Luck");
}

bool CustomJewel::InsertJewelOfScience(LPOBJ lpObj, int JewelPos, int TargetPos)
{
	if (lpObj->Inventory[TargetPos].m_Option1 != 0)
	{
		SendNotice(lpObj, "Use with items without Skill.");
		return false;
	}

	if (
		!(
			(lpObj->Inventory[TargetPos].m_Index >= GET_ITEM(0, 3) && lpObj->Inventory[TargetPos].m_Index <= GET_ITEM(0, 37)) ||
			(lpObj->Inventory[TargetPos].m_Index >= GET_ITEM(0, 42) && lpObj->Inventory[TargetPos].m_Index <= GET_ITEM(0, 57)) ||
			lpObj->Inventory[TargetPos].m_Index == GET_ITEM(1, 2) ||
			lpObj->Inventory[TargetPos].m_Index == GET_ITEM(1, 3) ||
			(lpObj->Inventory[TargetPos].m_Index >= GET_ITEM(1, 5) && lpObj->Inventory[TargetPos].m_Index <= GET_ITEM(1, 8)) ||
			lpObj->Inventory[TargetPos].m_Index == GET_ITEM(2, 1) ||
			(lpObj->Inventory[TargetPos].m_Index >= GET_ITEM(2, 3) && lpObj->Inventory[TargetPos].m_Index <= GET_ITEM(2, 6)) ||
			(lpObj->Inventory[TargetPos].m_Index >= GET_ITEM(2, 8) && lpObj->Inventory[TargetPos].m_Index <= GET_ITEM(2, 12)) ||
			lpObj->Inventory[TargetPos].m_Index == GET_ITEM(2, 14) ||
			lpObj->Inventory[TargetPos].m_Index == GET_ITEM(2, 15) ||
			(lpObj->Inventory[TargetPos].m_Index >= GET_ITEM(2, 17) && lpObj->Inventory[TargetPos].m_Index <= GET_ITEM(2, 26)) ||
			lpObj->Inventory[TargetPos].m_Index == GET_ITEM(3, 0) ||
			lpObj->Inventory[TargetPos].m_Index == GET_ITEM(3, 4) ||
			(lpObj->Inventory[TargetPos].m_Index >= GET_ITEM(3, 7) && lpObj->Inventory[TargetPos].m_Index <= GET_ITEM(3, 24)) ||
			(lpObj->Inventory[TargetPos].m_Index >= GET_ITEM(4, 0) && lpObj->Inventory[TargetPos].m_Index <= GET_ITEM(4, 6)) ||
			(lpObj->Inventory[TargetPos].m_Index >= GET_ITEM(4, 8) && lpObj->Inventory[TargetPos].m_Index <= GET_ITEM(4, 14)) ||
			(lpObj->Inventory[TargetPos].m_Index >= GET_ITEM(4, 16) && lpObj->Inventory[TargetPos].m_Index <= GET_ITEM(4, 31)) ||
			(lpObj->Inventory[TargetPos].m_Index >= GET_ITEM(6, 4) && lpObj->Inventory[TargetPos].m_Index <= GET_ITEM(6, 13)) ||
			lpObj->Inventory[TargetPos].m_Index == GET_ITEM(6, 17) ||
			lpObj->Inventory[TargetPos].m_Index == GET_ITEM(6, 18) ||
			lpObj->Inventory[TargetPos].m_Index == GET_ITEM(6, 21) ||
			lpObj->Inventory[TargetPos].m_Index == GET_ITEM(6, 22) ||
			(lpObj->Inventory[TargetPos].m_Index >= GET_ITEM(6, 25) && lpObj->Inventory[TargetPos].m_Index <= GET_ITEM(6, 32)) ||
			(lpObj->Inventory[TargetPos].m_Index >= GET_ITEM(6, 38) && lpObj->Inventory[TargetPos].m_Index <= GET_ITEM(6, 41))
			)
		)
	{
		SendNotice(lpObj, "Use with items without Skill.");
		return false;
	}

	std::vector<NormalItemEntry> allowedItems = LoadAllowedNormalItems("CustomJewelItems.ini");

	if (IsAllowedNormalTarget(lpObj, TargetPos, allowedItems) == false)
	{
		SendNotice(lpObj, "This item can not use Jewel of Science.");
		return false;
	}

	lpObj->Inventory[TargetPos].m_Option1 = 1;
	return ApplySimpleFixedResult(lpObj, JewelPos, TargetPos, "Use Jewel of Science successful. Your item now has Skill");
}

bool CustomJewel::InsertJewelOfKundun(LPOBJ lpObj, int JewelPos, int TargetPos)
{
	if (lpObj->Inventory[TargetPos].m_Level >= 15)
	{
		SendNotice(lpObj, "Use with items level 14 or lower.");
		return false;
	}

	std::vector<NormalItemEntry> allowedItems = LoadAllowedNormalItems("CustomJewelItems.ini");

	if (IsAllowedNormalTarget(lpObj, TargetPos, allowedItems) == false)
	{
		SendNotice(lpObj, "This item can not use Jewel of Kundun.");
		return false;
	}

	lpObj->Inventory[TargetPos].m_Level = 15;
	return ApplySimpleFixedResult(lpObj, JewelPos, TargetPos, "Use Jewel of Kundun successful. Your item is now +15");
}

bool CustomJewel::InsertJewelOfKondar(LPOBJ lpObj, int JewelPos, int TargetPos)
{
	if (lpObj->Inventory[TargetPos].m_Level >= 15 &&
		lpObj->Inventory[TargetPos].m_Option1 > 0 &&
		lpObj->Inventory[TargetPos].m_Option2 > 0 &&
		lpObj->Inventory[TargetPos].m_Option3 >= 7 &&
		lpObj->Inventory[TargetPos].m_NewOption >= 63)
	{
		SendNotice(lpObj, "Your item is already True FO.");
		return false;
	}

	std::vector<NormalItemEntry> allowedItems = LoadAllowedNormalItems("CustomJewelItems.ini");

	if (IsAllowedNormalTarget(lpObj, TargetPos, allowedItems) == false)
	{
		SendNotice(lpObj, "This item can not use Jewel of Kondar.");
		return false;
	}

	lpObj->Inventory[TargetPos].m_Level = 15;
	lpObj->Inventory[TargetPos].m_Option1 = 1;
	lpObj->Inventory[TargetPos].m_Option2 = 1;
	lpObj->Inventory[TargetPos].m_Option3 = 7;
	lpObj->Inventory[TargetPos].m_NewOption = 63;
	lpObj->Inventory[TargetPos].m_Durability = lpObj->Inventory[TargetPos].m_BaseDurability;

	return ApplySimpleFixedResult(lpObj, JewelPos, TargetPos, "Use Jewel of Kondar successful. Your item is now True FO");
}

bool CustomJewel::InsertJewelOfWisdom(LPOBJ lpObj, int JewelPos, int TargetPos)
{
	std::vector<SocketItemEntry> socketItems = LoadAllowedSocketItems("SocketItemType.txt");

	bool socketable = false;

	for (std::vector<SocketItemEntry>::const_iterator it = socketItems.begin(); it != socketItems.end(); ++it)
	{
		if (lpObj->Inventory[TargetPos].m_Index == GET_ITEM(it->section, it->type))
		{
			socketable = true;
			break;
		}
	}

	if (socketable == false)
	{
		SendNotice(lpObj, "Item is not socketable!");
		return false;
	}

	if (lpObj->Inventory[TargetPos].m_SocketOption[4] != 255)
	{
		SendNotice(lpObj, "Can not add more sockets!");
		return false;
	}

	for (int n = 0; n < 5; n++)
	{
		if (lpObj->Inventory[TargetPos].m_SocketOption[n] == 255)
		{
			lpObj->Inventory[TargetPos].m_SocketOption[n] = 254;
			SendItemUpdate(lpObj, TargetPos);
			ConsumeJewel(lpObj, JewelPos);
			return true;
		}
	}

	SendNotice(lpObj, "Item is not socketable!");
	return false;
}