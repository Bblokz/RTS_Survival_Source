// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SurvivalWaveSettings.h"

#include "RTS_Survival/Enemy/StrategicAI/StochasticDecisionTree/StochasticDecisionTree.h"

#define LOCTEXT_NAMESPACE "SurvivalWaveSettings"

namespace SurvivalWaveDefaults
{
	struct FCurveKey
	{
		float Minute;
		float Value;
	};

	// Base curves describe Normal; every difficulty compresses time by GrowthPace and scales values.
	const TArray<FCurveKey> BaseWaveIntervalSeconds = {
		{0.f, 100.f}, {5.f, 90.f}, {10.f, 80.f}, {20.f, 70.f}, {40.f, 60.f}, {60.f, 55.f}
	};
	const TArray<FCurveKey> BaseWaveBudget = {
		{0.f, 30.f}, {5.f, 60.f}, {10.f, 100.f}, {20.f, 190.f}, {30.f, 300.f}, {40.f, 420.f}, {60.f, 680.f},
		{90.f, 1100.f}, {120.f, 1500.f}
	};
	const TArray<FCurveKey> BaseEdgesPerWave = {
		{0.f, 1.f}, {6.f, 1.f}, {12.f, 1.5f}, {20.f, 2.f}, {30.f, 2.5f}, {45.f, 3.f}, {60.f, 3.5f}
	};
	const TArray<FCurveKey> BaseSpecialWaveChance = {
		{0.f, 0.f}, {6.f, 0.15f}, {15.f, 0.25f}, {30.f, 0.35f}, {60.f, 0.45f}
	};
	const TArray<FCurveKey> BaseMaxUnitsPerWave = {
		{0.f, 8.f}, {10.f, 14.f}, {20.f, 18.f}, {40.f, 24.f}, {60.f, 28.f}
	};
	const TArray<FCurveKey> BaseStrategicTrainingPointsPerMinute = {
		{0.f, 4.f}, {10.f, 8.f}, {20.f, 12.f}, {40.f, 18.f}, {60.f, 24.f}
	};

	struct FDifficultyScales
	{
		ERTSGameDifficulty Difficulty;
		float GrowthPace;
		float BudgetScale;
		float IntervalScale;
		float MaxUnitsScale;
		float TechPace;
		float AliveLoadCap;
		float StrategicIncomeScale;
		int32 MinPickedUnits;
		int32 MaxPickedUnits;
	};

	const TArray<FDifficultyScales> DifficultyScales = {
		{ERTSGameDifficulty::NewToRTS, 0.75f, 0.6f, 1.25f, 0.8f, 0.7f, 50.f, 0.6f, 3, 6},
		{ERTSGameDifficulty::Normal, 1.f, 1.f, 1.f, 1.f, 1.f, 65.f, 1.f, 3, 8},
		{ERTSGameDifficulty::Hard, 1.15f, 1.25f, 0.92f, 1.1f, 1.2f, 75.f, 1.25f, 2, 10},
		{ERTSGameDifficulty::Brutal, 1.3f, 1.5f, 0.85f, 1.2f, 1.4f, 85.f, 1.5f, 2, 12},
		{ERTSGameDifficulty::Ironman, 1.45f, 1.75f, 0.8f, 1.3f, 1.6f, 90.f, 1.75f, 2, 14},
	};

	void FillCurve(FRuntimeFloatCurve& OutCurve, const TArray<FCurveKey>& Keys, const float TimeDivisor,
	               const float ValueScale)
	{
		FRichCurve* const RichCurve = OutCurve.GetRichCurve();
		if (not RichCurve)
		{
			return;
		}
		RichCurve->Reset();
		const float SafeDivisor = FMath::Max(TimeDivisor, KINDA_SMALL_NUMBER);
		for (const FCurveKey& EachKey : Keys)
		{
			const FKeyHandle Handle = RichCurve->AddKey(EachKey.Minute / SafeDivisor, EachKey.Value * ValueScale);
			RichCurve->SetKeyInterpMode(Handle, RCIM_Linear);
		}
	}

	FSurvivalWaveDifficultyProfile BuildProfile(const FDifficultyScales& Scales)
	{
		FSurvivalWaveDifficultyProfile Profile;
		Profile.Difficulty = Scales.Difficulty;
		FillCurve(Profile.WaveIntervalSeconds, BaseWaveIntervalSeconds, Scales.GrowthPace, Scales.IntervalScale);
		FillCurve(Profile.WaveBudget, BaseWaveBudget, Scales.GrowthPace, Scales.BudgetScale);
		FillCurve(Profile.EdgesPerWave, BaseEdgesPerWave, Scales.GrowthPace, 1.f);
		FillCurve(Profile.SpecialWaveChance, BaseSpecialWaveChance, Scales.GrowthPace, 1.f);
		FillCurve(Profile.MaxUnitsPerWave, BaseMaxUnitsPerWave, Scales.GrowthPace, Scales.MaxUnitsScale);
		FillCurve(Profile.StrategicTrainingPointsPerMinute, BaseStrategicTrainingPointsPerMinute, Scales.GrowthPace,
		          Scales.StrategicIncomeScale);
		Profile.TechPaceMultiplier = Scales.TechPace;
		Profile.GrowthPaceMultiplier = Scales.GrowthPace;
		Profile.AliveEnemyLoadCap = Scales.AliveLoadCap;
		Profile.StrategicMinPickedUnitsForAction = Scales.MinPickedUnits;
		Profile.StrategicMaxPickedUnitsForAction = Scales.MaxPickedUnits;
		return Profile;
	}

	FSurvivalWaveUnitEntry MakeTank(const ETankSubtype Subtype, const EEnemyAITechLevel TechLevel,
	                                const ESurvivalWaveUnitRole Role, const float Weight,
	                                const float FadeStartMinute = 0.f)
	{
		FSurvivalWaveUnitEntry Entry;
		Entry.Kind = ESurvivalWaveUnitKind::Tank;
		Entry.TankSubtype = Subtype;
		Entry.TechLevel = TechLevel;
		Entry.Role = Role;
		Entry.Weight = Weight;
		Entry.FadeStartMinute = FadeStartMinute;
		return Entry;
	}

	FSurvivalWaveUnitEntry MakeSquad(const ESquadSubtype Subtype, const EEnemyAITechLevel TechLevel,
	                                 const ESurvivalWaveUnitRole Role, const float Weight,
	                                 const float FadeStartMinute = 0.f)
	{
		FSurvivalWaveUnitEntry Entry;
		Entry.Kind = ESurvivalWaveUnitKind::Squad;
		Entry.SquadSubtype = Subtype;
		Entry.TechLevel = TechLevel;
		Entry.Role = Role;
		Entry.Weight = Weight;
		Entry.FadeStartMinute = FadeStartMinute;
		return Entry;
	}

	FSurvivalRoleWeightMultiplier Role(const ESurvivalWaveUnitRole InRole, const float Multiplier)
	{
		FSurvivalRoleWeightMultiplier RoleMultiplier;
		RoleMultiplier.Role = InRole;
		RoleMultiplier.Multiplier = Multiplier;
		return RoleMultiplier;
	}

	const FSurvivalWaveDifficultyProfile& GetBuiltInNormalProfile()
	{
		static const FSurvivalWaveDifficultyProfile BuiltInNormal = BuildProfile(DifficultyScales[1]);
		return BuiltInNormal;
	}
}

USurvivalWaveSettings::USurvivalWaveSettings()
{
	InitDefaultDifficultyProfiles();
	InitDefaultTechStages();
	InitDefaultUnitPool();
	InitDefaultWaveKinds();
}

const FSurvivalWaveDifficultyProfile& USurvivalWaveSettings::GetProfileForDifficulty(
	const ERTSGameDifficulty Difficulty) const
{
	const FSurvivalWaveDifficultyProfile* NormalProfile = nullptr;
	for (const FSurvivalWaveDifficultyProfile& EachProfile : DifficultyProfiles)
	{
		if (EachProfile.Difficulty == Difficulty)
		{
			return EachProfile;
		}
		if (EachProfile.Difficulty == ERTSGameDifficulty::Normal)
		{
			NormalProfile = &EachProfile;
		}
	}
	if (NormalProfile)
	{
		return *NormalProfile;
	}
	return SurvivalWaveDefaults::GetBuiltInNormalProfile();
}

float USurvivalWaveSettings::GetTechStageUnlockMinute(const EEnemyAITechLevel TechLevel,
                                                      const float TechPaceMultiplier) const
{
	const float SafePace = FMath::Max(TechPaceMultiplier, KINDA_SMALL_NUMBER);
	for (const FSurvivalTechStageUnlock& EachUnlock : TechStageUnlocks)
	{
		if (EachUnlock.TechLevel == TechLevel)
		{
			return EachUnlock.UnlockMinute / SafePace;
		}
	}
	return TNumericLimits<float>::Max();
}

FText USurvivalWaveSettings::GetTechStageAnnouncement(const EEnemyAITechLevel TechLevel) const
{
	for (const FSurvivalTechStageUnlock& EachUnlock : TechStageUnlocks)
	{
		if (EachUnlock.TechLevel == TechLevel)
		{
			return EachUnlock.AnnouncementText;
		}
	}
	return FText::GetEmpty();
}

float USurvivalWaveSettings::EvaluateCurveAtMinute(const FRuntimeFloatCurve& Curve, const float Minute,
                                                   const float Fallback)
{
	const FRichCurve* const RichCurve = Curve.GetRichCurveConst();
	if (not RichCurve or RichCurve->GetNumKeys() == 0)
	{
		return Fallback;
	}
	return RichCurve->Eval(Minute, Fallback);
}

void USurvivalWaveSettings::InitDefaultDifficultyProfiles()
{
	DifficultyProfiles.Reset();
	for (const SurvivalWaveDefaults::FDifficultyScales& EachScales : SurvivalWaveDefaults::DifficultyScales)
	{
		DifficultyProfiles.Add(SurvivalWaveDefaults::BuildProfile(EachScales));
	}
}

void USurvivalWaveSettings::InitDefaultTechStages()
{
	auto MakeStage = [](const EEnemyAITechLevel TechLevel, const float UnlockMinute, const FText& Text)
	{
		FSurvivalTechStageUnlock Stage;
		Stage.TechLevel = TechLevel;
		Stage.UnlockMinute = UnlockMinute;
		Stage.AnnouncementText = Text;
		return Stage;
	};

	TechStageUnlocks = {
		MakeStage(EEnemyAITechLevel::BasicInfantry, 0.f,
		          LOCTEXT("StageBasicInfantry", "Soviet riflemen and armoured cars approach")),
		MakeStage(EEnemyAITechLevel::LightTanks, 2.f,
		          LOCTEXT("StageLightTanks", "The Soviets field light tanks")),
		MakeStage(EEnemyAITechLevel::MediumTanks, 6.f,
		          LOCTEXT("StageMediumTanks", "T-34s spotted: the Soviets field medium tanks")),
		MakeStage(EEnemyAITechLevel::Tier2, 10.f,
		          LOCTEXT("StageTier2", "Soviet tier 2: KV-1s, rocket artillery and assault infantry")),
		MakeStage(EEnemyAITechLevel::AdvancedInfantry, 14.f,
		          LOCTEXT("StageAdvancedInfantry", "Soviet elite infantry and cyborgs join the attack")),
		MakeStage(EEnemyAITechLevel::Tier3, 20.f,
		          LOCTEXT("StageTier3", "Heavy armour: IS tanks and heavy tank destroyers")),
		MakeStage(EEnemyAITechLevel::Experimental, 32.f,
		          LOCTEXT("StageExperimental", "Super-heavy Soviet tanks are on their way")),
	};
}

void USurvivalWaveSettings::InitDefaultUnitPool()
{
	using SurvivalWaveDefaults::MakeSquad;
	using SurvivalWaveDefaults::MakeTank;
	using ETL = EEnemyAITechLevel;
	using ER = ESurvivalWaveUnitRole;

	// Only subtypes that spawned in the unit roster test. Excluded on purpose: team weapons (deploy logic and
	// voice-line errors in attack-moves), Hazmat engineers (pulled off for repairs by direct control), and
	// SU-85L, KV-IS, SU-152, T-44/155, ISU-130, MS-1, Zis-57, Bofors, DShK, LargeMix*, Medic, HazmatBazooka
	// (never spawned in the roster; likely absent from the async spawner).
	UnitPool = {
		// Basic infantry and scouts.
		MakeSquad(ESquadSubtype::Squad_Rus_Mosin, ETL::BasicInfantry, ER::Infantry, 3.f, 20.f),
		MakeSquad(ESquadSubtype::Squad_Rus_Okhotnik, ETL::BasicInfantry, ER::AntiTankInfantry, 2.f, 25.f),
		MakeSquad(ESquadSubtype::Squad_Rus_Dyakonov, ETL::BasicInfantry, ER::Infantry, 2.f, 30.f),
		MakeSquad(ESquadSubtype::Squad_Rus_ROKS3_PPSH, ETL::BasicInfantry, ER::Infantry, 1.5f, 30.f),
		MakeSquad(ESquadSubtype::Squad_Rus_KorovinAssault, ETL::BasicInfantry, ER::Infantry, 1.5f),
		MakeTank(ETankSubtype::Tank_Ba12, ETL::BasicInfantry, ER::ArmoredCar, 1.5f, 15.f),
		MakeTank(ETankSubtype::Tank_Ba14, ETL::BasicInfantry, ER::ArmoredCar, 1.5f, 15.f),
		// Light tanks.
		MakeTank(ETankSubtype::Tank_BT7, ETL::LightTanks, ER::LightTank, 2.f, 20.f),
		MakeTank(ETankSubtype::Tank_T26, ETL::LightTanks, ER::LightTank, 2.f, 18.f),
		MakeTank(ETankSubtype::Tank_BT7_4, ETL::LightTanks, ER::LightTank, 1.5f, 22.f),
		MakeTank(ETankSubtype::Tank_T70, ETL::LightTanks, ER::LightTank, 1.5f, 25.f),
		MakeTank(ETankSubtype::Tank_T70F, ETL::LightTanks, ER::LightTank, 1.f, 25.f),
		MakeTank(ETankSubtype::Tank_SU_76, ETL::LightTanks, ER::TankDestroyer, 1.f, 25.f),
		// Medium tanks.
		MakeTank(ETankSubtype::Tank_T34_76, ETL::MediumTanks, ER::MediumTank, 3.f),
		MakeTank(ETankSubtype::Tank_T28, ETL::MediumTanks, ER::MediumTank, 1.5f, 35.f),
		MakeTank(ETankSubtype::Tank_T28F, ETL::MediumTanks, ER::MediumTank, 1.f, 35.f),
		MakeTank(ETankSubtype::Tank_T34_AA, ETL::MediumTanks, ER::AntiAir, 0.3f),
		// Tier 2.
		MakeTank(ETankSubtype::Tank_T34_85, ETL::Tier2, ER::MediumTank, 3.f),
		MakeTank(ETankSubtype::Tank_T34_76_L, ETL::Tier2, ER::MediumTank, 2.f),
		MakeTank(ETankSubtype::Tank_T34E, ETL::Tier2, ER::MediumTank, 1.5f),
		MakeTank(ETankSubtype::Tank_SU_85, ETL::Tier2, ER::TankDestroyer, 1.5f),
		MakeTank(ETankSubtype::Tank_SU_122, ETL::Tier2, ER::Artillery, 1.f),
		MakeTank(ETankSubtype::Tank_BM13, ETL::Tier2, ER::Artillery, 0.8f),
		MakeTank(ETankSubtype::Tank_KV_1, ETL::Tier2, ER::HeavyTank, 1.5f),
		MakeSquad(ESquadSubtype::Squad_Rus_LargePTRS, ETL::Tier2, ER::AntiTankInfantry, 1.5f),
		MakeSquad(ESquadSubtype::Squad_Rus_RedHammer, ETL::Tier2, ER::Infantry, 1.5f),
		MakeSquad(ESquadSubtype::Squad_Rus_RedHamerPTRS, ETL::Tier2, ER::AntiTankInfantry, 1.5f),
		MakeSquad(ESquadSubtype::Squad_Rus_ToxicGuard, ETL::Tier2, ER::Infantry, 1.f),
		// Advanced infantry.
		MakeSquad(ESquadSubtype::Squad_Rus_GhostsOfStalingrad, ETL::AdvancedInfantry, ER::EliteInfantry, 1.f),
		MakeSquad(ESquadSubtype::Squad_Rus_Kvarc77, ETL::AdvancedInfantry, ER::EliteInfantry, 1.f),
		MakeSquad(ESquadSubtype::Squad_Rus_Tucha12T, ETL::AdvancedInfantry, ER::EliteInfantry, 1.f),
		MakeSquad(ESquadSubtype::Squad_Rus_CortexOfficer, ETL::AdvancedInfantry, ER::EliteInfantry, 0.6f),
		// Tier 3.
		MakeTank(ETankSubtype::Tank_T34_100, ETL::Tier3, ER::MediumTank, 2.f),
		MakeTank(ETankSubtype::Tank_T44_100L, ETL::Tier3, ER::MediumTank, 1.f),
		MakeTank(ETankSubtype::Tank_SU_100, ETL::Tier3, ER::TankDestroyer, 1.5f),
		MakeTank(ETankSubtype::Tank_ISU_152, ETL::Tier3, ER::TankDestroyer, 0.8f),
		MakeTank(ETankSubtype::Tank_KV_2, ETL::Tier3, ER::HeavyTank, 1.f),
		MakeTank(ETankSubtype::Tank_KV_1E, ETL::Tier3, ER::HeavyTank, 1.5f),
		MakeTank(ETankSubtype::Tank_KV_1_Arc, ETL::Tier3, ER::HeavyTank, 1.f),
		MakeTank(ETankSubtype::Tank_IS_1, ETL::Tier3, ER::HeavyTank, 1.5f),
		MakeTank(ETankSubtype::Tank_IS_2, ETL::Tier3, ER::HeavyTank, 1.5f),
		MakeTank(ETankSubtype::Tank_T35, ETL::Tier3, ER::HeavyTank, 0.4f),
		// Experimental.
		MakeTank(ETankSubtype::Tank_IS_3, ETL::Experimental, ER::SuperHeavy, 1.f),
		MakeTank(ETankSubtype::Tank_IS_3_Flame, ETL::Experimental, ER::SuperHeavy, 0.7f),
		MakeTank(ETankSubtype::Tank_KV_5, ETL::Experimental, ER::SuperHeavy, 0.7f),
		MakeTank(ETankSubtype::Tank_IS6, ETL::Experimental, ER::SuperHeavy, 0.7f),
		MakeTank(ETankSubtype::Tank_SMK, ETL::Experimental, ER::SuperHeavy, 0.7f),
	};
}

void USurvivalWaveSettings::InitDefaultWaveKinds()
{
	using SurvivalWaveDefaults::Role;
	using ER = ESurvivalWaveUnitRole;

	RegularWave = FSurvivalWaveKindDefinition();
	RegularWave.Kind = ESurvivalWaveKind::Regular;
	RegularWave.DisplayName = LOCTEXT("KindRegular", "Assault");
	RegularWave.RoleWeightMultipliers = {Role(ER::Artillery, 0.4f), Role(ER::SuperHeavy, 0.5f), Role(ER::AntiAir, 0.5f)};

	MilestoneWave = FSurvivalWaveKindDefinition();
	MilestoneWave.Kind = ESurvivalWaveKind::Milestone;
	MilestoneWave.DisplayName = LOCTEXT("KindMilestone", "Major offensive");
	MilestoneWave.BudgetMultiplier = 1.5f;
	MilestoneWave.MaxUnitsMultiplier = 1.3f;
	MilestoneWave.ExtraEdges = 1;
	MilestoneWave.RoleWeightMultipliers = {Role(ER::HeavyTank, 1.5f), Role(ER::SuperHeavy, 1.5f)};

	FSurvivalWaveKindDefinition Blitz;
	Blitz.Kind = ESurvivalWaveKind::Blitz;
	Blitz.DisplayName = LOCTEXT("KindBlitz", "Blitz");
	Blitz.Weight = 1.f;
	Blitz.MinMinute = 3.f;
	Blitz.ExtraEdges = 1;
	Blitz.RoleWeightMultipliers = {Role(ER::ArmoredCar, 4.f), Role(ER::LightTank, 4.f), Role(ER::MediumTank, 1.5f)};
	Blitz.DefaultRoleMultiplier = 0.15f;

	FSurvivalWaveKindDefinition InfantryRush;
	InfantryRush.Kind = ESurvivalWaveKind::InfantryRush;
	InfantryRush.DisplayName = LOCTEXT("KindInfantryRush", "Infantry rush");
	InfantryRush.Weight = 1.f;
	InfantryRush.MinMinute = 4.f;
	InfantryRush.BudgetMultiplier = 1.2f;
	InfantryRush.MaxUnitsMultiplier = 1.6f;
	InfantryRush.RoleWeightMultipliers = {
		Role(ER::Infantry, 4.f), Role(ER::AntiTankInfantry, 3.f), Role(ER::EliteInfantry, 2.f)
	};
	InfantryRush.DefaultRoleMultiplier = 0.1f;

	FSurvivalWaveKindDefinition Artillery;
	Artillery.Kind = ESurvivalWaveKind::Artillery;
	Artillery.DisplayName = LOCTEXT("KindArtillery", "Artillery strike");
	Artillery.Weight = 0.8f;
	Artillery.MinMinute = 10.f;
	Artillery.RoleWeightMultipliers = {
		Role(ER::Artillery, 6.f), Role(ER::TankDestroyer, 1.5f), Role(ER::Infantry, 1.f),
		Role(ER::AntiTankInfantry, 1.f)
	};
	Artillery.DefaultRoleMultiplier = 0.3f;
	Artillery.GlobalAbilities = {
		EGlobalAbility::GA_SovietSmallBarrage, EGlobalAbility::GA_SovietLargeBarrage,
		EGlobalAbility::GA_SovietLargeBarrageVariation
	};
	Artillery.GlobalAbilityCasts = 1;

	FSurvivalWaveKindDefinition HeavyArmour;
	HeavyArmour.Kind = ESurvivalWaveKind::HeavyArmour;
	HeavyArmour.DisplayName = LOCTEXT("KindHeavyArmour", "Heavy armour");
	HeavyArmour.Weight = 0.8f;
	HeavyArmour.MinMinute = 14.f;
	HeavyArmour.BudgetMultiplier = 1.3f;
	HeavyArmour.RoleWeightMultipliers = {
		Role(ER::HeavyTank, 4.f), Role(ER::SuperHeavy, 3.f), Role(ER::TankDestroyer, 2.f), Role(ER::MediumTank, 1.5f)
	};
	HeavyArmour.DefaultRoleMultiplier = 0.2f;

	FSurvivalWaveKindDefinition AirRaid;
	AirRaid.Kind = ESurvivalWaveKind::AirRaid;
	AirRaid.DisplayName = LOCTEXT("KindAirRaid", "Air raid");
	AirRaid.Weight = 0.7f;
	AirRaid.MinMinute = 8.f;
	AirRaid.BudgetMultiplier = 0.6f;
	AirRaid.GlobalAbilities = {
		EGlobalAbility::GA_2xPE2Strafing, EGlobalAbility::GA_4xPE2Strafing,
		EGlobalAbility::GA_HeavyCarpet_PE8_500Gr, EGlobalAbility::GA_HeavyCarpet_PE8_1000Gr
	};
	AirRaid.GlobalAbilityCasts = 1;
	AirRaid.bRequiresGlobalAbility = true;

	FSurvivalWaveKindDefinition Airborne;
	Airborne.Kind = ESurvivalWaveKind::Airborne;
	Airborne.DisplayName = LOCTEXT("KindAirborne", "Airborne assault");
	Airborne.Weight = 0.6f;
	Airborne.MinMinute = 12.f;
	Airborne.BudgetMultiplier = 0.7f;
	Airborne.GlobalAbilities = {
		EGlobalAbility::GA_SovietParatroopers, EGlobalAbility::GA_SovietInfantryDrop,
		EGlobalAbility::GA_SovietT34Drop, EGlobalAbility::GA_SovietHeavyTankDrop
	};
	Airborne.GlobalAbilityCasts = 1;
	Airborne.bRequiresGlobalAbility = true;
	Airborne.GlobalAbilityTargetOffset = 3500.f;

	SpecialWaves = {Blitz, InfantryRush, Artillery, HeavyArmour, AirRaid, Airborne};
}

#undef LOCTEXT_NAMESPACE
