#include "EnemyAITechLevel.h"

#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"

FEnemyLevelTraining::FEnemyLevelTraining()
{
	BasicInfantryOptions.TechLevel = EEnemyAITechLevel::BasicInfantry;
	BasicInfantryOptions.TypesUnlockingThisLevel = {
		EBuildingExpansionType::BTX_RusBarracks,
	};
	BasicInfantryOptions.TrainableSquadSubtypes = {
		ESquadSubtype::Squad_Rus_HazmatEngineers,
		ESquadSubtype::Squad_Rus_Mosin,
		ESquadSubtype::Squad_Rus_Okhotnik
	};
	BasicInfantryOptions.TrainableTankSubtypes = {};

	LightTankOptions.TechLevel = EEnemyAITechLevel::LightTanks;
	LightTankOptions.TypesUnlockingThisLevel = {
		EBuildingExpansionType::BTX_RusFactory,
		EBuildingExpansionType::BTX_RusCoolingTowers
	};
	LightTankOptions.TrainableSquadSubtypes = {};
	LightTankOptions.TrainableTankSubtypes = {
		ETankSubtype::Tank_Ba12,
		ETankSubtype::Tank_Ba14,
		ETankSubtype::Tank_BT7,
		ETankSubtype::Tank_T26,
		ETankSubtype::Tank_BT7_4,
		ETankSubtype::Tank_T70,
		ETankSubtype::Tank_T70F,
		// todo finish this unit type.
		// ETankSubtype::Tank_SU_76,
	};

	MediumTankOptions.TechLevel = EEnemyAITechLevel::MediumTanks;
	MediumTankOptions.TypesUnlockingThisLevel = {};
	MediumTankOptions.TrainableSquadSubtypes = {};
	MediumTankOptions.TrainableTankSubtypes = {};

	Tier2Options.TechLevel = EEnemyAITechLevel::Tier2;
	Tier2Options.TypesUnlockingThisLevel = {
		EBuildingExpansionType::BTX_RusResearchCenter,
	};
	Tier2Options.TrainableSquadSubtypes = {
	ESquadSubtype::Squad_Rus_LargePTRS,
	ESquadSubtype::Squad_Rus_RedHammer,
	ESquadSubtype::Squad_Rus_RedHamerPTRS,
	ESquadSubtype::Squad_Rus_ToxicGuard};
	Tier2Options.TrainableTankSubtypes = {
		ETankSubtype::Tank_T34_85,
		ETankSubtype::Tank_T34_76,
		ETankSubtype::Tank_T34_AA,
		ETankSubtype::Tank_T34_76_L,
		ETankSubtype::Tank_T34E,
		ETankSubtype::Tank_SU_85,
		ETankSubtype::Tank_SU_85_L,
		ETankSubtype::Tank_SU_122,
		ETankSubtype::Tank_T28,
		ETankSubtype::Tank_T28F,
		ETankSubtype::Tank_KV_1,
	};

	AdvancedInfantryOptions.TechLevel = EEnemyAITechLevel::AdvancedInfantry;
	AdvancedInfantryOptions.TypesUnlockingThisLevel = {
		// todo.
	};
	AdvancedInfantryOptions.TrainableSquadSubtypes = {
	ESquadSubtype::Squad_Rus_GhostsOfStalingrad,
	ESquadSubtype::Squad_Rus_Kvarc77,
	ESquadSubtype::Squad_Rus_Tucha12T,
	ESquadSubtype::Squad_Rus_CortexOfficer};
	AdvancedInfantryOptions.TrainableTankSubtypes = {
	};

	Tier3Options.TechLevel = EEnemyAITechLevel::Tier3;
	
	Tier3Options.TrainableSquadSubtypes = {};
	Tier3Options.TrainableTankSubtypes = {
		ETankSubtype::Tank_SU_85,
		ETankSubtype::Tank_SU_85_L,
		ETankSubtype::Tank_SU_122,
		// Not trainable by default.
		// ETankSubtype::Tank_T35,
		ETankSubtype::Tank_T28,
		ETankSubtype::Tank_T28F,
		ETankSubtype::Tank_KV_1,
		ETankSubtype::Tank_KV_2,
		ETankSubtype::Tank_T34_100,
		ETankSubtype::Tank_KV_1E,
		ETankSubtype::Tank_KV_1_Arc,
		ETankSubtype::Tank_IS_1,
		// ETankSubtype::Tank_KV_IS,
		ETankSubtype::Tank_IS_2,
		ETankSubtype::Tank_SU_100,
		// ETankSubtype::Tank_SU_152,
	};

	ExperimentalOptions.TechLevel = EEnemyAITechLevel::Experimental;
	ExperimentalOptions.TrainableSquadSubtypes = {};
	ExperimentalOptions.TrainableTankSubtypes = {
		ETankSubtype::Tank_IS_3,
		ETankSubtype::Tank_KV_5,
		// todo finish unit type.
		// ETankSubtype::Tank_T44_100L,
		
	};
}

TArray<EBuildingExpansionType> FEnemyLevelTraining::GetUniqueBuildingTypesForTechLevels() const
{
	TArray<EBuildingExpansionType> UniqueTypes;

	auto AddUniqueBuildingTypes = [&UniqueTypes](const FEnemyTrainingOptionsForTechLevel& TrainingOptions)
	{
		for (const EBuildingExpansionType EachType : TrainingOptions.TypesUnlockingThisLevel)
		{
			UniqueTypes.AddUnique(EachType);
		}
	};

	AddUniqueBuildingTypes(BasicInfantryOptions);
	AddUniqueBuildingTypes(LightTankOptions);
	AddUniqueBuildingTypes(MediumTankOptions);
	AddUniqueBuildingTypes(Tier2Options);
	AddUniqueBuildingTypes(AdvancedInfantryOptions);
	AddUniqueBuildingTypes(Tier3Options);
	AddUniqueBuildingTypes(ExperimentalOptions);
	return UniqueTypes;
}
