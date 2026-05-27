#include "EnemyTrainingHelpers.h"

#include "RTS_Survival/Enemy/TrainingAndUnitCreation/EnemyAITechLevel/EnemyAITechLevel.h"
#include "RTS_Survival/GameUI/ActionUI/ActionUIManager/ActionUIManager.h"

int32 EnemyTrainingHelpers::GetSquadTrainingPointCost(const ESquadSubtype SquadType)
{
	switch (SquadType)
	{
	case ESquadSubtype::Squad_Rus_Zis_57MM:
		return 10;
	case ESquadSubtype::Squad_Rus_Zis_76MM:
	case ESquadSubtype::Squad_Rus_Bofors:
		return 15;
	case ESquadSubtype::Squad_Rus_KS30_130MM:
	case ESquadSubtype::Squad_Rus_M1938_122mm:
		return 35;
	case ESquadSubtype::Squad_Rus_80mm_Mortar:
		return 10;
	case ESquadSubtype::Squad_Rus_120mm_Mortar:
		return 12;
	case ESquadSubtype::Squad_Rus_Maxim:
	case ESquadSubtype::Squad_Rus_DShK:
		return 8;
	case ESquadSubtype::Squad_Rus_Mosin:
		return 3;
	case ESquadSubtype::Squad_Rus_HazmatEngineers:
	case ESquadSubtype::Squad_Rus_Okhotnik:
		return 4;
	case ESquadSubtype::Squad_Rus_LargePTRS:
	case ESquadSubtype::Squad_Rus_RedHammer:
		return 10;
	case ESquadSubtype::Squad_Rus_RedHamerPTRS:
		return 12;
	case ESquadSubtype::Squad_Rus_ToxicGuard:
	case ESquadSubtype::Squad_Rus_GhostsOfStalingrad:
		return 15;
	case ESquadSubtype::Squad_Rus_Kvarc77:
	case ESquadSubtype::Squad_Rus_Tucha12T:
		return 25;
	case ESquadSubtype::Squad_Rus_CortexOfficer:
		return 30;
	default:
		break;
	}
	RTSFunctionLibrary::ReportError(TEXT(
		"GetSquadTrainingPointCost was called with an invalid squad type. Returning 10 cost."
		"\n type: " + UEnum::GetValueAsString(SquadType)));
	return 10;
}

int32 EnemyTrainingHelpers::GetTankTrainingPointsCost(const ETankSubtype TankType)
{
	switch (TankType)
	{
	case ETankSubtype::Tank_Ba12:
	case ETankSubtype::Tank_Ba14:
	case ETankSubtype::Tank_BT7:
	case ETankSubtype::Tank_T26:
	case ETankSubtype::Tank_BT7_4:
		return 10;
	case ETankSubtype::Tank_T70:
	case ETankSubtype::Tank_T70F:
	case ETankSubtype::Tank_SU_76:
		return 12;
	case ETankSubtype::Tank_T28:
	case ETankSubtype::Tank_T28F:
		return 15;

	case ETankSubtype::Tank_T34_76:
	case ETankSubtype::Tank_T34_AA:
		return 20;

	case ETankSubtype::Tank_T34E:
	case ETankSubtype::Tank_SU_122:
		return 25;

	case ETankSubtype::Tank_T34_85:
	case ETankSubtype::Tank_T34_76_L:
		return 28;
	case ETankSubtype::Tank_T34_100:
	case ETankSubtype::Tank_SU_85_L:
		return 30;
	case ETankSubtype::Tank_SU_85:
	case ETankSubtype::Tank_T35:
	case ETankSubtype::Tank_KV_1:
	case ETankSubtype::Tank_KV_IS:
		return 28;
	case ETankSubtype::Tank_SU_100:
		return 35;
	case ETankSubtype::Tank_T44_100L:
		return 45;
	case ETankSubtype::Tank_KV_2:
	case ETankSubtype::Tank_KV_1E:
	case ETankSubtype::Tank_KV_1_Arc:
		return 40;
	case ETankSubtype::Tank_IS_1:
		return 50;
	case ETankSubtype::Tank_IS_2:
	case ETankSubtype::Tank_SU_152:
		return 55;
	case ETankSubtype::Tank_IS_3:
	case ETankSubtype::Tank_IS_3_Flame:
	case ETankSubtype::Tank_KV_5:
		return 100;
	default:
		break;
	}
	RTSFunctionLibrary::ReportError(TEXT(
		"GetTankTrainingPointsCost was called with an invalid tank type. Returning 10 cost."
		"\n type: " + UEnum::GetValueAsString(TankType)));
	return 10;
}

bool EnemyTrainingHelpers::GetIsTechLevelUnlockedByBxpCounts(
	const FEnemyTrainingOptionsForTechLevel& TrainingOptions,
	const TMap<EBuildingExpansionType, int32>& BxpCountsByType)
{
	for (const EBuildingExpansionType RequiredBxpType : TrainingOptions.TypesUnlockingThisLevel)
	{
		const int32* const BxpCount = BxpCountsByType.Find(RequiredBxpType);
		if (BxpCount != nullptr && *BxpCount > 0)
		{
			return true;
		}
	}

	return false;
}

void EnemyTrainingHelpers::UpdateTechLevelUnlockedMapForOptions(
	TMap<EEnemyAITechLevel, bool>& TechLevelUnlockedMap,
	const FEnemyTrainingOptionsForTechLevel& TrainingOptions,
	const TMap<EBuildingExpansionType, int32>& BxpCountsByType)
{
	TechLevelUnlockedMap.Add(
		TrainingOptions.TechLevel,
		GetIsTechLevelUnlockedByBxpCounts(TrainingOptions, BxpCountsByType));
}
