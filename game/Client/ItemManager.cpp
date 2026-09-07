#include "pch.h"
#include "ItemManager.h"
#include "Item.h"
#include "EquipableItem.h"

const vector<int> itemIconID = {
	110102, 110202, 110406, 112101, 201104,
	201205, 201302, 201413, 202106, 202211,
	202409, 203101, 203204, 203205, 203506,
	204102, 204408, 205109, 205211, 401106,
	401109, 401110, 401113, 401121, 401123,
	401124, 401211, 401303, 502104,

	104101, 112103, 130101, 130202, 130303,
	130402, 202206, 202306, 202404, 203102, 
	203411, 204204, 204419, 205101, 205102, 
	205203, 302103, 401101, 401114, 401117, 
	401217, 205312
};

const vector<wstring> itemIconTag = {
	L"목장갑",			L"아이언 너클",		L"디바인 피스트",	L"돌멩이",		L"자전거 헬멧",
	L"안전모",			L"소방 헬멧",		L"비질란테",		L"셔츠",		L"사제복",
	L"제사장의 예복",	L"손목시계",		L"고장난 시계",		L"철사",		L"스포츠 시계",
	L"운동화",			L"타키온 브레이스", L"십자가",			L"비파단도",	L"고철",
	L"마패",			L"배터리",			L"옷감",			L"화약",		L"화학품",
	L"흑연",			L"전자 부품",		L"모터",			L"피아노선",
	
	L"망치",			L"쇠구슬",			L"유리구슬",		L"얼음구슬",	L"이성의 칼",
	L"운명의 수레바퀴", L"덧댄 로브",		L"한복",			L"어사의",		L"붕대", 
	L"나이팅게일",		L"힐리스",			L"델타 레드",		L"깃털",		L"꽃", 
	L"운명의 꽃",		L"얼음",			L"못",				L"원석",		L"종이",
	L"루비",			L"아이테르 깃털"
};

const vector<ITEMGRADE> itemGrade = {
	ITEMGRADE::COMMON,	ITEMGRADE::UNCOMMON,ITEMGRADE::EPIC,	ITEMGRADE::UNCOMMON,ITEMGRADE::COMMON,
	ITEMGRADE::UNCOMMON,ITEMGRADE::RARE,	ITEMGRADE::EPIC,	ITEMGRADE::COMMON,	ITEMGRADE::UNCOMMON,
	ITEMGRADE::EPIC,	ITEMGRADE::COMMON,	ITEMGRADE::UNCOMMON,ITEMGRADE::UNCOMMON,ITEMGRADE::EPIC,
	ITEMGRADE::COMMON,	ITEMGRADE::EPIC,	ITEMGRADE::COMMON,	ITEMGRADE::UNCOMMON,ITEMGRADE::COMMON,
	ITEMGRADE::COMMON,	ITEMGRADE::COMMON,	ITEMGRADE::COMMON,	ITEMGRADE::COMMON,	ITEMGRADE::COMMON,
	ITEMGRADE::COMMON,	ITEMGRADE::UNCOMMON,ITEMGRADE::RARE,	ITEMGRADE::COMMON,


	ITEMGRADE::COMMON,	ITEMGRADE::COMMON,	ITEMGRADE::COMMON,	ITEMGRADE::UNCOMMON,ITEMGRADE::RARE,
	ITEMGRADE::EPIC,	ITEMGRADE::UNCOMMON,ITEMGRADE::RARE,	ITEMGRADE::EPIC,	ITEMGRADE::COMMON,
	ITEMGRADE::EPIC,	ITEMGRADE::UNCOMMON,ITEMGRADE::EPIC,	ITEMGRADE::COMMON,	ITEMGRADE::COMMON,
	ITEMGRADE::UNCOMMON,ITEMGRADE::COMMON,	ITEMGRADE::COMMON,	ITEMGRADE::COMMON,	ITEMGRADE::COMMON,
	ITEMGRADE::UNCOMMON,ITEMGRADE::RARE
};

const vector<ITEMTYPE> itemType = {
	ITEMTYPE::EQUIPABLE,	ITEMTYPE::EQUIPABLE,	ITEMTYPE::EQUIPABLE,	ITEMTYPE::INGREDIENTS,	ITEMTYPE::EQUIPABLE,
	ITEMTYPE::EQUIPABLE,	ITEMTYPE::EQUIPABLE,	ITEMTYPE::EQUIPABLE,	ITEMTYPE::EQUIPABLE,	ITEMTYPE::EQUIPABLE,
	ITEMTYPE::EQUIPABLE,	ITEMTYPE::EQUIPABLE,	ITEMTYPE::INGREDIENTS,	ITEMTYPE::INGREDIENTS,	ITEMTYPE::EQUIPABLE,
	ITEMTYPE::EQUIPABLE,	ITEMTYPE::EQUIPABLE,	ITEMTYPE::INGREDIENTS,	ITEMTYPE::INGREDIENTS,	ITEMTYPE::INGREDIENTS,
	ITEMTYPE::INGREDIENTS,	ITEMTYPE::INGREDIENTS,	ITEMTYPE::INGREDIENTS,	ITEMTYPE::INGREDIENTS,	ITEMTYPE::INGREDIENTS,
	ITEMTYPE::INGREDIENTS,	ITEMTYPE::INGREDIENTS,	ITEMTYPE::INGREDIENTS,	ITEMTYPE::INGREDIENTS,

	ITEMTYPE::INGREDIENTS,	ITEMTYPE::INGREDIENTS,	ITEMTYPE::INGREDIENTS,	ITEMTYPE::EQUIPABLE,	ITEMTYPE::EQUIPABLE,
	ITEMTYPE::EQUIPABLE,	ITEMTYPE::EQUIPABLE,	ITEMTYPE::EQUIPABLE,	ITEMTYPE::EQUIPABLE,	ITEMTYPE::INGREDIENTS,
	ITEMTYPE::EQUIPABLE,	ITEMTYPE::EQUIPABLE,	ITEMTYPE::EQUIPABLE,	ITEMTYPE::INGREDIENTS,	ITEMTYPE::INGREDIENTS,
	ITEMTYPE::INGREDIENTS,	ITEMTYPE::INGREDIENTS,	ITEMTYPE::INGREDIENTS,	ITEMTYPE::INGREDIENTS,	ITEMTYPE::INGREDIENTS,
	ITEMTYPE::INGREDIENTS,	ITEMTYPE::INGREDIENTS
};

const vector<EquipmentType> equipableItemType = {
	EquipmentType::WEAPON,	EquipmentType::WEAPON,	EquipmentType::WEAPON,	EquipmentType::DEFAULT,	EquipmentType::HEAD,
	EquipmentType::HEAD,	EquipmentType::HEAD,	EquipmentType::HEAD,	EquipmentType::CHEST,	EquipmentType::CHEST,
	EquipmentType::CHEST,	EquipmentType::ARM,		EquipmentType::DEFAULT,	EquipmentType::DEFAULT,	EquipmentType::ARM,
	EquipmentType::LEG,		EquipmentType::LEG,		EquipmentType::DEFAULT,	EquipmentType::DEFAULT,	EquipmentType::DEFAULT,
	EquipmentType::DEFAULT,	EquipmentType::DEFAULT,	EquipmentType::DEFAULT,	EquipmentType::DEFAULT,	EquipmentType::DEFAULT,
	EquipmentType::DEFAULT,	EquipmentType::DEFAULT,	EquipmentType::DEFAULT,	EquipmentType::DEFAULT,	

	EquipmentType::DEFAULT,	EquipmentType::DEFAULT,	EquipmentType::DEFAULT,	EquipmentType::WEAPON,	EquipmentType::WEAPON,
	EquipmentType::WEAPON,	EquipmentType::CHEST,	EquipmentType::CHEST,	EquipmentType::CHEST,	EquipmentType::DEFAULT,
	EquipmentType::ARM,		EquipmentType::LEG,		EquipmentType::LEG,		EquipmentType::DEFAULT,	EquipmentType::DEFAULT,
	EquipmentType::DEFAULT,	EquipmentType::DEFAULT,	EquipmentType::DEFAULT,	EquipmentType::DEFAULT,	EquipmentType::DEFAULT,
	EquipmentType::DEFAULT, EquipmentType::DEFAULT
};

const vector<ItemStatus> equipableItemStatus = {
	{50}, {50}, {50}, {0}, {30, 0, 150},
	{30, 0, 150}, {30, 0, 150}, {30, 0, 150}, {50, 0, 300, 150, 0, 0, 5}, {50, 0, 300, 150, 0, 0, 5},
	{50, 0, 300, 150, 0, 0, 5}, {15, 0, 0, 0, 0.2f, 0, 0, 0, 0, 0.15f}, {0}, {0}, {15, 0, 0, 0, 0.2f, 0, 0, 0, 0, 0.15f},
	{10, 0, 0, 0, 0, 0.2f}, {10, 0, 0, 0, 0, 0.2f}, {0}, {0}, {0},
	{0}, {0}, {0}, {0}, {0},
	{0}, {0}, {0}, {0},

	{0}, {0}, {0}, {50}, {50},
	{50}, {50, 0, 300, 150, 0, 0, 5}, {50, 0, 300, 150, 0, 0, 5}, {50, 0, 300, 150, 0, 0, 5}, {0},
	{15, 0, 0, 0, 0.2f, 0, 0, 0, 0, 0.15f}, {10, 0, 0, 0, 0, 0.2f}, {10, 0, 0, 0, 0, 0.2f}, {0}, {0},
	{0}, {0}, {0}, {0}, {0},
	{0}, {0}
};


ItemManager::~ItemManager()
{

}

void ItemManager::Initialize()
{
	LoadResources();
	CreatItems();
}

void ItemManager::LoadResources()
{
	shared_ptr<Shader> shader = make_shared<Shader>(L"ImageShader.fx");

	// 모든 UI 머티리얼에 동일한 설정 적용
	auto SetupUIMaterial = [&](shared_ptr<Material> material) {
		material->SetShader(shader);
		material->SetRenderQueue(RenderQueue::Transparent);
		material->SetTransparent(true);  // 모든 UI에 추가
		material->SetRenderingMode(RenderingMode::Forward);
		};

	wstring prefixPath = L"..\\Resources\\Textures\\UI\\ItemIcon\\";
	wstring prefixTag = L"ItemIcon_";

	for (int i = 0; i < itemIconID.size(); i++)
	{
		shared_ptr<Material> itemIcon = make_shared<Material>();
		SetupUIMaterial(itemIcon);

		wstring tag = L"ItemIcon_" + to_wstring(itemIconID[i]);
		wstring path = prefixPath + tag + L".png";
		auto itemIconTexture = RESOURCES->Load<Texture>(tag, path);

		itemIcon->SetDiffuseMap(itemIconTexture);
		MaterialDesc& itemIconDesc = itemIcon->GetMaterialDesc();
		itemIconDesc.ambient = Vec4(1.f);
		itemIconDesc.diffuse = Vec4(1.f);
		itemIconDesc.specular = Vec4(1.f);
		RESOURCES->Add(tag, itemIcon);
	}
}

void ItemManager::CreatItems()
{
	wstring materialPrefixTag = L"ItemIcon_";

	for (int i = 0; i < itemIconID.size(); i++)
	{
		if (itemType[i] == ITEMTYPE::INGREDIENTS)
		{
			shared_ptr<IngredientItem> ingredientItem = make_shared<IngredientItem>();

			ingredientItem->SetItemID(itemIconID[i]);
			ingredientItem->SetName(itemIconTag[i]);
			ingredientItem->SetItemType(itemType[i]);
			ingredientItem->SetItemGrade(itemGrade[i]);
			
			m_itemsContainerByName[itemIconTag[i]] = ingredientItem;
			m_itemsContainerByID[itemIconID[i]] = ingredientItem;
		}
		else if (itemType[i] == ITEMTYPE::EQUIPABLE)
		{
			shared_ptr<EquipableItem> equipableItem = make_shared<EquipableItem>();
			equipableItem->SetItemID(itemIconID[i]);
			equipableItem->SetName(itemIconTag[i]);
			equipableItem->SetItemType(itemType[i]);
			equipableItem->SetItemGrade(itemGrade[i]);
			equipableItem->SetEquipType(equipableItemType[i]);
			equipableItem->SetStatus(equipableItemStatus[i]);

			m_itemsContainerByName[itemIconTag[i]] = equipableItem;
			m_itemsContainerByID[itemIconID[i]] = equipableItem;
		}
	}
}

shared_ptr<Item> ItemManager::GetItem(const wstring& name)
{
	return m_itemsContainerByName[name];
}

shared_ptr<Item> ItemManager::GetItem(int32 ID)
{
	return m_itemsContainerByID[ID];
}

void ItemManager::LoadItemData()
{
}

void ItemManager::SetupItemRecipes()
{
}
