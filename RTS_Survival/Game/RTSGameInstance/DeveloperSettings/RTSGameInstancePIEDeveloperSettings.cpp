#include "RTSGameInstancePIEDeveloperSettings.h"

URTSGameInstancePIEDeveloperSettings::URTSGameInstancePIEDeveloperSettings()
{
	CategoryName = TEXT("Game");
	SectionName = TEXT("RTS PIE Startup");

	WorldMissionContext.OperationType = EMapItemType::EnemyItem;
	WorldMissionContext.EnemyObjectType = EMapEnemyItem::Barracks;
	WorldMissionContext.bIsInitialized = true;
}
