// Copyright (C) Bas Blokzijl - All rights reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "RTS_Survival/Player/PlayerTechManager/PlayerTechManager.h"
#include "RTS_Survival/Player/PlayerUnitVariantManager/PlayerUnitVariantManager.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Variants/UnitVariantDataAsset.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

/** Befriended by the data asset and the manager so tests fill private state without an editor-only setter. */
struct FUnitVariantTestAccess
{
	static TMap<ETankSubtype, FUnitVariantEntry>& GetTankVariants(UUnitVariantDataAsset& VariantDataAsset)
	{
		return VariantDataAsset.M_TankVariants;
	}

	static TMap<ESquadSubtype, FUnitVariantEntry>& GetSquadVariants(UUnitVariantDataAsset& VariantDataAsset)
	{
		return VariantDataAsset.M_SquadVariants;
	}

	static void InjectVariantDataAsset(UPlayerUnitVariantManager& Manager,
	                                   const UUnitVariantDataAsset* VariantDataAsset)
	{
		Manager.M_VariantDataAsset = VariantDataAsset;
	}
};

namespace UnitVariantTestsPrivate
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	constexpr int32 TwoVariants = 2;
	constexpr int32 FiveVariants = 5;
	constexpr uint8 EnemyOwningPlayer = 2;
	const TCHAR* const ClampErrorFragment = TEXT("is not implemented for");
	const TCHAR* const SuppressionResetMessage = TEXT("UnitVariantTests: resetting error dialog suppression.");

	FUnitVariantEntry MakeEntry(const int32 VariantCount)
	{
		FUnitVariantEntry Entry;
		for (int32 VariantIndex = 0; VariantIndex < VariantCount; ++VariantIndex)
		{
			FUnitVariantDefinition Definition;
			Definition.DisplayName = FText::FromString(FString::Printf(TEXT("Variant %d"), VariantIndex));
			Entry.Variants.Add(Definition);
		}
		return Entry;
	}

	FTrainingOption MakeTank(const ETankSubtype TankSubtype)
	{
		return FTrainingOption(EAllUnitType::UNType_Tank, static_cast<uint8>(TankSubtype));
	}

	FTrainingOption MakeSquad(const ESquadSubtype SquadSubtype)
	{
		return FTrainingOption(EAllUnitType::UNType_Squad, static_cast<uint8>(SquadSubtype));
	}

	/** PzIV H implements Base + VariantA, the Pz38t lists five (clamped to three), the Kar98k squad lists two. */
	UUnitVariantDataAsset* MakeTestDataAsset()
	{
		UUnitVariantDataAsset* VariantDataAsset = NewObject<UUnitVariantDataAsset>(GetTransientPackage());
		TMap<ETankSubtype, FUnitVariantEntry>& TankVariants = FUnitVariantTestAccess::GetTankVariants(*VariantDataAsset);
		TankVariants.Add(ETankSubtype::Tank_PzIV_H, MakeEntry(TwoVariants));
		TankVariants.Add(ETankSubtype::Tank_Pz38t, MakeEntry(FiveVariants));
		FUnitVariantTestAccess::GetSquadVariants(*VariantDataAsset).Add(
			ESquadSubtype::Squad_Ger_JagerTruppKar98k, MakeEntry(TwoVariants));
		return VariantDataAsset;
	}

	UPlayerUnitVariantManager* MakeManager(const UUnitVariantDataAsset* VariantDataAsset)
	{
		UPlayerUnitVariantManager* Manager = NewObject<UPlayerUnitVariantManager>(GetTransientPackage());
		FUnitVariantTestAccess::InjectVariantDataAsset(*Manager, VariantDataAsset);
		return Manager;
	}

	/** A component outside any world, so it never registers with a game state. */
	URTSComponent* MakeRTSComponent(const EAllUnitType UnitType)
	{
		URTSComponent* RTSComponent = NewObject<URTSComponent>(GetTransientPackage());
		RTSComponent->SetUnitType(UnitType);
		return RTSComponent;
	}

	/** Pushes dialog suppression so ReportError logs to LogRTS instead of opening a modal dialog. */
	struct FScopedErrorDialogSuppression
	{
		FScopedErrorDialogSuppression()
		{
			RTSFunctionLibrary::PushModalDialogSuppression();
		}

		~FScopedErrorDialogSuppression()
		{
			RTSFunctionLibrary::PopModalDialogSuppression();
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnitVariantDataAssetCountTest, "RTS.UnitVariants.DataAssetCounts",
                                 UnitVariantTestsPrivate::TestFlags)

bool FUnitVariantDataAssetCountTest::RunTest(const FString& Parameters)
{
	using namespace UnitVariantTestsPrivate;
	const UUnitVariantDataAsset* VariantDataAsset = MakeTestDataAsset();
	const FTrainingOption MissingTank = MakeTank(ETankSubtype::Tank_PzIII_M);
	const FTrainingOption TwoVariantTank = MakeTank(ETankSubtype::Tank_PzIV_H);
	const FTrainingOption FiveVariantTank = MakeTank(ETankSubtype::Tank_Pz38t);

	TestEqual(TEXT("A unit without an entry has only Base"),
	          VariantDataAsset->GetImplementedVariantCount(MissingTank), 1);
	TestEqual(TEXT("A two-element entry has two variants"),
	          VariantDataAsset->GetImplementedVariantCount(TwoVariantTank), TwoVariants);
	TestEqual(TEXT("A five-element entry is clamped to the maximum"),
	          VariantDataAsset->GetImplementedVariantCount(FiveVariantTank), UnitVariants::MaxVariantsPerUnit);

	TestTrue(TEXT("Base is implemented for a unit without an entry"),
	         VariantDataAsset->GetIsVariantImplemented(MissingTank, EUnitVariant::Base));
	TestTrue(TEXT("VariantA is implemented for the two-element entry"),
	         VariantDataAsset->GetIsVariantImplemented(TwoVariantTank, EUnitVariant::VariantA));
	TestFalse(TEXT("VariantB is not implemented for the two-element entry"),
	          VariantDataAsset->GetIsVariantImplemented(TwoVariantTank, EUnitVariant::VariantB));
	TestNull(TEXT("No definition for an unimplemented variant"),
	         VariantDataAsset->FindVariantDefinition(TwoVariantTank, EUnitVariant::VariantB));
	TestNotNull(TEXT("A definition for an implemented variant"),
	            VariantDataAsset->FindVariantDefinition(TwoVariantTank, EUnitVariant::VariantA));

	const TArray<FTrainingOption> UnitsWithChoice = VariantDataAsset->GetUnitsWithChoice();
	TestEqual(TEXT("Every listed entry with more than one variant offers a choice"), UnitsWithChoice.Num(), 3);
	TestFalse(TEXT("A unit without an entry offers no choice"), UnitsWithChoice.Contains(MissingTank));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnitVariantManagerClampTest, "RTS.UnitVariants.ManagerClampsSavedSelection",
                                 UnitVariantTestsPrivate::TestFlags)

bool FUnitVariantManagerClampTest::RunTest(const FString& Parameters)
{
	using namespace UnitVariantTestsPrivate;
	UPlayerUnitVariantManager* Manager = MakeManager(MakeTestDataAsset());
	const FTrainingOption TwoVariantTank = MakeTank(ETankSubtype::Tank_PzIV_H);
	const FTrainingOption TwoVariantSquad = MakeSquad(ESquadSubtype::Squad_Ger_JagerTruppKar98k);

	FUnitVariantSelection UnimplementedSelection;
	UnimplementedSelection.Unit = TwoVariantTank;
	UnimplementedSelection.Variant = EUnitVariant::VariantB;
	FUnitVariantSelection ImplementedSelection;
	ImplementedSelection.Unit = TwoVariantSquad;
	ImplementedSelection.Variant = EUnitVariant::VariantA;

	{
		const FScopedErrorDialogSuppression ErrorDialogSuppression;
		// A user who suppressed error dialogs earlier in this editor session would otherwise hide the clamp error.
		AddExpectedError(SuppressionResetMessage, EAutomationExpectedErrorFlags::Contains, 1);
		RTSFunctionLibrary::ReportError(SuppressionResetMessage, true);

		AddExpectedError(ClampErrorFragment, EAutomationExpectedErrorFlags::Contains, 1);
		Manager->InitFromProfile({UnimplementedSelection, ImplementedSelection});
	}

	TestEqual(TEXT("An unimplemented saved variant clamps to Base"),
	          Manager->GetSelectedVariant(TwoVariantTank), EUnitVariant::Base);
	TestEqual(TEXT("An implemented saved variant is kept"),
	          Manager->GetSelectedVariant(TwoVariantSquad), EUnitVariant::VariantA);
	TestEqual(TEXT("Only non-Base choices are stored"), Manager->GetSelectionsForSave().Num(), 1);

	TestTrue(TEXT("Selecting Base succeeds"), Manager->SetSelectedVariant(TwoVariantSquad, EUnitVariant::Base));
	TestEqual(TEXT("Selecting Base removes the stored choice"), Manager->GetSelectionsForSave().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnitVariantOwnerGateTest, "RTS.UnitVariants.OnlyPlayerUnitsGetVariants",
                                 UnitVariantTestsPrivate::TestFlags)

bool FUnitVariantOwnerGateTest::RunTest(const FString& Parameters)
{
	using namespace UnitVariantTestsPrivate;
	UPlayerUnitVariantManager* Manager = MakeManager(MakeTestDataAsset());
	TestTrue(TEXT("VariantA is selectable for the PzIV H"),
	         Manager->SetSelectedVariant(MakeTank(ETankSubtype::Tank_PzIV_H), EUnitVariant::VariantA));

	URTSComponent* TankRTSComponent = MakeRTSComponent(EAllUnitType::UNType_Tank);
	TankRTSComponent->SetUnitSubtype(ENomadicSubtype::Nomadic_None, ETankSubtype::Tank_PzIV_H);
	TestEqual(TEXT("The component starts owned by the player"), TankRTSComponent->GetOwningPlayer(),
	          UPlayerTechManager::PlayerTechOwnerIndex);
	TestEqual(TEXT("A player-owned PzIV H resolves to the selection"),
	          Manager->ResolveVariantForUnit(TankRTSComponent), EUnitVariant::VariantA);

	TankRTSComponent->SetOwningPlayerRuntime(EnemyOwningPlayer);
	TestEqual(TEXT("An enemy-owned PzIV H resolves to Base"),
	          Manager->ResolveVariantForUnit(TankRTSComponent), EUnitVariant::Base);
	TestEqual(TEXT("A null component resolves to Base"), Manager->ResolveVariantForUnit(nullptr), EUnitVariant::Base);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnitVariantSupportedTypesTest, "RTS.UnitVariants.UnsupportedUnitTypesGetBase",
                                 UnitVariantTestsPrivate::TestFlags)

bool FUnitVariantSupportedTypesTest::RunTest(const FString& Parameters)
{
	using namespace UnitVariantTestsPrivate;
	UPlayerUnitVariantManager* Manager = MakeManager(MakeTestDataAsset());

	TestTrue(TEXT("Tanks are supported"), UPlayerUnitVariantManager::GetIsUnitTypeSupported(EAllUnitType::UNType_Tank));
	TestTrue(TEXT("Squads are supported"),
	         UPlayerUnitVariantManager::GetIsUnitTypeSupported(EAllUnitType::UNType_Squad));
	TestTrue(TEXT("Building expansions are supported"),
	         UPlayerUnitVariantManager::GetIsUnitTypeSupported(EAllUnitType::UNType_BuildingExpansion));
	TestFalse(TEXT("Nomadic trucks are not supported"),
	          UPlayerUnitVariantManager::GetIsUnitTypeSupported(EAllUnitType::UNType_Nomadic));
	TestFalse(TEXT("Aircraft are not supported"),
	          UPlayerUnitVariantManager::GetIsUnitTypeSupported(EAllUnitType::UNType_Aircraft));
	TestFalse(TEXT("Harvesters are not supported"),
	          UPlayerUnitVariantManager::GetIsUnitTypeSupported(EAllUnitType::UNType_Harvester));

	const FTrainingOption NomadicUnit(EAllUnitType::UNType_Nomadic,
	                                  static_cast<uint8>(ENomadicSubtype::Nomadic_GerHq));
	TestFalse(TEXT("A nomadic selection is rejected"), Manager->SetSelectedVariant(NomadicUnit, EUnitVariant::VariantA));

	URTSComponent* NomadicRTSComponent = MakeRTSComponent(EAllUnitType::UNType_Nomadic);
	NomadicRTSComponent->SetUnitSubtype(ENomadicSubtype::Nomadic_GerHq);
	TestEqual(TEXT("A player nomadic truck resolves to Base"),
	          Manager->ResolveVariantForUnit(NomadicRTSComponent), EUnitVariant::Base);

	URTSComponent* AircraftRTSComponent = MakeRTSComponent(EAllUnitType::UNType_Aircraft);
	AircraftRTSComponent->SetUnitSubtype(ENomadicSubtype::Nomadic_None, ETankSubtype::Tank_None,
	                                     ESquadSubtype::Squad_None, EBuildingExpansionType::BXT_Invalid,
	                                     EAircraftSubtype::Aircraft_Bf109);
	TestEqual(TEXT("A player aircraft resolves to Base"),
	          Manager->ResolveVariantForUnit(AircraftRTSComponent), EUnitVariant::Base);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnitVariantComponentApplyOnceTest, "RTS.UnitVariants.ComponentAppliesOnce",
                                 UnitVariantTestsPrivate::TestFlags)

bool FUnitVariantComponentApplyOnceTest::RunTest(const FString& Parameters)
{
	using namespace UnitVariantTestsPrivate;
	URTSComponent* SquadUnitRTSComponent = MakeRTSComponent(EAllUnitType::UNType_Squad);
	int32 BroadcastCount = 0;
	SquadUnitRTSComponent->OnUnitVariantApplied.AddLambda([&BroadcastCount](const EUnitVariant)
	{
		++BroadcastCount;
	});

	SquadUnitRTSComponent->ApplyUnitVariantFromParent(EUnitVariant::VariantB);
	SquadUnitRTSComponent->ApplyUnitVariantFromParent(EUnitVariant::VariantA);
	SquadUnitRTSComponent->ApplyUnitVariant();

	TestTrue(TEXT("The variant is marked applied"), SquadUnitRTSComponent->GetHasAppliedUnitVariant());
	TestEqual(TEXT("The first variant is kept"), SquadUnitRTSComponent->GetUnitVariant(), EUnitVariant::VariantB);
	TestEqual(TEXT("The variant is broadcast once"), BroadcastCount, 1);

	URTSComponent* WorldlessRTSComponent = MakeRTSComponent(EAllUnitType::UNType_Tank);
	WorldlessRTSComponent->ApplyUnitVariant();
	TestTrue(TEXT("Without a player controller the variant still applies"),
	         WorldlessRTSComponent->GetHasAppliedUnitVariant());
	TestEqual(TEXT("Without a player controller the variant is Base"), WorldlessRTSComponent->GetUnitVariant(),
	          EUnitVariant::Base);
	return true;
}

#endif
