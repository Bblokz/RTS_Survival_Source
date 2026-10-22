// Copyright (C) Bas Blokzijl - All rights reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Texture2D.h"
#include "Misc/AutomationTest.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Player/PlayerUnitVariantManager/PlayerUnitVariantManager.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/Variants/UnitVariantDataAsset.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "Slate/SlateBrushAsset.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

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

	static TArray<FUnitVariantSelection>& GetDebugForcedVariants(UUnitVariantDataAsset& VariantDataAsset)
	{
		return VariantDataAsset.M_DebugForcedVariants;
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
	constexpr int32 VariantAIndex = 1;
	const TCHAR* const ClampErrorFragment = TEXT("is not implemented for");
	const TCHAR* const SuppressionResetMessage = TEXT("UnitVariantTests: resetting error dialog suppression.");
	const TCHAR* const VariantTitle = TEXT("Pz IV H (L/48)");

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

	/** Base plus one VariantA that spawns VariantClass. */
	FUnitVariantEntry MakeEntryWithVariantClass(const UClass* VariantClass)
	{
		FUnitVariantEntry Entry = MakeEntry(TwoVariants);
		Entry.Variants[VariantAIndex].UnitClass = TSoftClassPtr<AActor>(FSoftObjectPath(VariantClass));
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

	FUnitVariantSelection MakeSelection(const FTrainingOption& Unit, const EUnitVariant Variant)
	{
		FUnitVariantSelection Selection;
		Selection.Unit = Unit;
		Selection.Variant = Variant;
		return Selection;
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

	/** A transient brush whose image is a transient texture, like the brush assets DT_TrainingOptions uses. */
	USlateBrushAsset* MakeTextureBrush()
	{
		USlateBrushAsset* BrushAsset = NewObject<USlateBrushAsset>(GetTransientPackage());
		BrushAsset->Brush.SetResourceObject(NewObject<UTexture2D>(GetTransientPackage()));
		return BrushAsset;
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

#if WITH_EDITOR
	bool GetHasIssueContaining(const FDataValidationContext& Context, const TCHAR* Fragment)
	{
		for (const FDataValidationContext::FIssue& Issue : Context.GetIssues())
		{
			if (Issue.Message.ToString().Contains(Fragment))
			{
				return true;
			}
		}
		return false;
	}
#endif
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
	// A test asset with an empty debug list, so a forced entry left in DA_UnitVariants cannot change the result.
	UPlayerUnitVariantManager* Manager = MakeManager(MakeTestDataAsset());
	const FTrainingOption TwoVariantTank = MakeTank(ETankSubtype::Tank_PzIV_H);
	const FTrainingOption TwoVariantSquad = MakeSquad(ESquadSubtype::Squad_Ger_JagerTruppKar98k);

	{
		const FScopedErrorDialogSuppression ErrorDialogSuppression;
		// A user who suppressed error dialogs earlier in this editor session would otherwise hide the clamp error.
		AddExpectedError(SuppressionResetMessage, EAutomationExpectedErrorFlags::Contains, 1);
		RTSFunctionLibrary::ReportError(SuppressionResetMessage, true);

		AddExpectedError(ClampErrorFragment, EAutomationExpectedErrorFlags::Contains, 1);
		Manager->InitFromProfile({
			MakeSelection(TwoVariantTank, EUnitVariant::VariantB),
			MakeSelection(TwoVariantSquad, EUnitVariant::VariantA)
		});
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnitVariantForcedFromAssetTest, "RTS.UnitVariants.ForcedVariantsFromAsset",
                                 UnitVariantTestsPrivate::TestFlags)

bool FUnitVariantForcedFromAssetTest::RunTest(const FString& Parameters)
{
	using namespace UnitVariantTestsPrivate;
	UUnitVariantDataAsset* VariantDataAsset = MakeTestDataAsset();
	const FTrainingOption TwoVariantTank = MakeTank(ETankSubtype::Tank_PzIV_H);
	const FTrainingOption TwoVariantSquad = MakeSquad(ESquadSubtype::Squad_Ger_JagerTruppKar98k);
	TArray<FUnitVariantSelection>& ForcedVariants = FUnitVariantTestAccess::GetDebugForcedVariants(*VariantDataAsset);
	ForcedVariants.Add(MakeSelection(TwoVariantTank, EUnitVariant::VariantA));
	ForcedVariants.Add(MakeSelection(TwoVariantSquad, EUnitVariant::VariantB));
	UPlayerUnitVariantManager* Manager = MakeManager(VariantDataAsset);

	{
		const FScopedErrorDialogSuppression ErrorDialogSuppression;
		AddExpectedError(SuppressionResetMessage, EAutomationExpectedErrorFlags::Contains, 1);
		RTSFunctionLibrary::ReportError(SuppressionResetMessage, true);

		AddExpectedError(ClampErrorFragment, EAutomationExpectedErrorFlags::Contains, 1);
		Manager->InitFromProfile({});
	}

	TestEqual(TEXT("An implemented forced variant is applied"),
	          Manager->GetSelectedVariant(TwoVariantTank), EUnitVariant::VariantA);
	TestEqual(TEXT("An unimplemented forced variant clamps to Base"),
	          Manager->GetSelectedVariant(TwoVariantSquad), EUnitVariant::Base);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnitVariantFindClassTest, "RTS.UnitVariants.FindVariantClass",
                                 UnitVariantTestsPrivate::TestFlags)

bool FUnitVariantFindClassTest::RunTest(const FString& Parameters)
{
	using namespace UnitVariantTestsPrivate;
	UUnitVariantDataAsset* VariantDataAsset = NewObject<UUnitVariantDataAsset>(GetTransientPackage());
	FUnitVariantTestAccess::GetTankVariants(*VariantDataAsset).Add(
		ETankSubtype::Tank_PzIV_H, MakeEntryWithVariantClass(ATankMaster::StaticClass()));
	const FTrainingOption VariantTank = MakeTank(ETankSubtype::Tank_PzIV_H);

	TestTrue(TEXT("Base has no variant class"),
	         VariantDataAsset->FindVariantClass(VariantTank, EUnitVariant::Base).IsNull());
	TestEqual(TEXT("VariantA resolves to its class"),
	          VariantDataAsset->FindVariantClass(VariantTank, EUnitVariant::VariantA).ToSoftObjectPath(),
	          FSoftObjectPath(ATankMaster::StaticClass()));
	TestTrue(TEXT("An unimplemented VariantB has no class"),
	         VariantDataAsset->FindVariantClass(VariantTank, EUnitVariant::VariantB).IsNull());
	TestTrue(TEXT("A unit without an entry has no class"),
	         VariantDataAsset->FindVariantClass(MakeTank(ETankSubtype::Tank_PzIII_M), EUnitVariant::VariantA).IsNull());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnitVariantTrainingPresentationTest, "RTS.UnitVariants.TrainingPresentation",
                                 UnitVariantTestsPrivate::TestFlags)

bool FUnitVariantTrainingPresentationTest::RunTest(const FString& Parameters)
{
	using namespace UnitVariantTestsPrivate;
	UUnitVariantDataAsset* VariantDataAsset = NewObject<UUnitVariantDataAsset>(GetTransientPackage());
	TMap<ETankSubtype, FUnitVariantEntry>& TankVariants = FUnitVariantTestAccess::GetTankVariants(*VariantDataAsset);

	FUnitVariantEntry TitleOnlyEntry = MakeEntryWithVariantClass(ATankMaster::StaticClass());
	TitleOnlyEntry.Variants[VariantAIndex].bOverrideTrainingTitle = true;
	TitleOnlyEntry.Variants[VariantAIndex].TrainingTitle = FText::FromString(VariantTitle);
	TankVariants.Add(ETankSubtype::Tank_PzIV_H, TitleOnlyEntry);

	USlateBrushAsset* VariantBrush = MakeTextureBrush();
	FUnitVariantEntry BrushEntry = MakeEntryWithVariantClass(ATankMaster::StaticClass());
	BrushEntry.Variants[VariantAIndex].bOverrideTrainingImageBrush = true;
	BrushEntry.Variants[VariantAIndex].TrainingImageBrush = VariantBrush;
	TankVariants.Add(ETankSubtype::Tank_PzI_Harvester, BrushEntry);

	const FTrainingOption TitleTank = MakeTank(ETankSubtype::Tank_PzIV_H);
	const FUnitVariantTrainingPresentation TitlePresentation =
		VariantDataAsset->GetTrainingPresentation(TitleTank, EUnitVariant::VariantA);
	TestEqual(TEXT("The requested variant is echoed"), TitlePresentation.Variant, EUnitVariant::VariantA);
	TestTrue(TEXT("The ticked title override is on"), TitlePresentation.bOverrideTitle);
	TestEqual(TEXT("The title is the variant's"), TitlePresentation.Title.ToString(), FString(VariantTitle));
	TestFalse(TEXT("The unticked image brush override is off"), TitlePresentation.bOverrideImageBrush);
	TestFalse(TEXT("The unticked description override is off"), TitlePresentation.bOverrideDescription);
	TestFalse(TEXT("The unticked tooltip override is off"), TitlePresentation.bOverrideTooltip);

	const FUnitVariantTrainingPresentation BasePresentation =
		VariantDataAsset->GetTrainingPresentation(TitleTank, EUnitVariant::Base);
	TestFalse(TEXT("Base never overrides the title"), BasePresentation.bOverrideTitle);
	const FUnitVariantTrainingPresentation MissingPresentation =
		VariantDataAsset->GetTrainingPresentation(MakeTank(ETankSubtype::Tank_PzIII_M), EUnitVariant::VariantA);
	TestFalse(TEXT("A unit without an entry overrides nothing"), MissingPresentation.bOverrideTitle);

	// The training menu lists the harvester as UNType_Harvester; the manager stores and finds it as a tank.
	UPlayerUnitVariantManager* Manager = MakeManager(VariantDataAsset);
	const FTrainingOption MenuHarvester(EAllUnitType::UNType_Harvester,
	                                    static_cast<uint8>(ETankSubtype::Tank_PzI_Harvester));
	TestTrue(TEXT("A harvester variant can be selected through its menu key"),
	         Manager->SetSelectedVariant(MenuHarvester, EUnitVariant::VariantA));
	TestEqual(TEXT("The harvester selection is stored under the tank key"),
	          Manager->GetSelectedVariant(MakeTank(ETankSubtype::Tank_PzI_Harvester)), EUnitVariant::VariantA);
	const FUnitVariantTrainingPresentation HarvesterPresentation = Manager->GetTrainingPresentation(MenuHarvester);
	TestTrue(TEXT("The harvester's ticked image brush override is on"), HarvesterPresentation.bOverrideImageBrush);
	TestTrue(TEXT("The presentation holds the variant brush asset"),
	         HarvesterPresentation.ImageBrush.Get() == VariantBrush);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnitVariantTrainingBrushGCTest, "RTS.UnitVariants.TrainingBrushSurvivesGC",
                                 UnitVariantTestsPrivate::TestFlags)

bool FUnitVariantTrainingBrushGCTest::RunTest(const FString& Parameters)
{
	using namespace UnitVariantTestsPrivate;
	// Stand-in for UPlayerUnitVariantManager::M_VariantDataAsset, the only strong owner in a mission.
	TStrongObjectPtr<UUnitVariantDataAsset> VariantDataAsset(NewObject<UUnitVariantDataAsset>(GetTransientPackage()));
	FUnitVariantEntry BrushEntry = MakeEntryWithVariantClass(ATankMaster::StaticClass());
	BrushEntry.Variants[VariantAIndex].bOverrideTrainingImageBrush = true;
	BrushEntry.Variants[VariantAIndex].TrainingImageBrush = MakeTextureBrush();
	FUnitVariantTestAccess::GetTankVariants(*VariantDataAsset).Add(ETankSubtype::Tank_PzIV_H, BrushEntry);

	const FUnitVariantTrainingPresentation Presentation =
		VariantDataAsset->GetTrainingPresentation(MakeTank(ETankSubtype::Tank_PzIV_H), EUnitVariant::VariantA);
	const TWeakObjectPtr<USlateBrushAsset> WeakBrush(Presentation.ImageBrush.Get());
	if (not TestTrue(TEXT("The presentation holds a brush"), WeakBrush.IsValid()))
	{
		return false;
	}
	const TWeakObjectPtr<UObject> WeakTexture(WeakBrush->Brush.GetResourceObject());
	BrushEntry = FUnitVariantEntry();

	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	TestTrue(TEXT("The brush survives GC while the data asset is referenced"), WeakBrush.IsValid());
	TestTrue(TEXT("The brush texture survives GC while the data asset is referenced"), WeakTexture.IsValid());

	// Proves the test depends on the data asset's reference and not on something else keeping the brush alive.
	VariantDataAsset.Reset();
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	TestFalse(TEXT("The brush is collected once the data asset is released"), WeakBrush.IsValid());
	TestFalse(TEXT("The texture is collected once the data asset is released"), WeakTexture.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnitVariantUnsupportedTypesTest, "RTS.UnitVariants.UnsupportedUnitTypesRejected",
                                 UnitVariantTestsPrivate::TestFlags)

bool FUnitVariantUnsupportedTypesTest::RunTest(const FString& Parameters)
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

	const FTrainingOption NomadicUnit(EAllUnitType::UNType_Nomadic, static_cast<uint8>(ENomadicSubtype::Nomadic_GerHq));
	const FTrainingOption AircraftUnit(EAllUnitType::UNType_Aircraft,
	                                   static_cast<uint8>(EAircraftSubtype::Aircraft_Bf109));
	TestFalse(TEXT("A nomadic selection is rejected"), Manager->SetSelectedVariant(NomadicUnit, EUnitVariant::VariantA));
	TestFalse(TEXT("An aircraft selection is rejected"),
	          Manager->SetSelectedVariant(AircraftUnit, EUnitVariant::VariantA));
	TestEqual(TEXT("Rejected selections are not stored"), Manager->GetSelectionsForSave().Num(), 0);
	return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUnitVariantDataAssetClassRulesTest, "RTS.UnitVariants.DataAssetClassRules",
                                 UnitVariantTestsPrivate::TestFlags)

bool FUnitVariantDataAssetClassRulesTest::RunTest(const FString& Parameters)
{
	using namespace UnitVariantTestsPrivate;
	UUnitVariantDataAsset* ValidAsset = NewObject<UUnitVariantDataAsset>(GetTransientPackage());
	FUnitVariantEntry ValidEntry = MakeEntryWithVariantClass(ATankMaster::StaticClass());
	ValidEntry.Variants[VariantAIndex].bOverrideTrainingImageBrush = true;
	ValidEntry.Variants[VariantAIndex].TrainingImageBrush = MakeTextureBrush();
	ValidEntry.Variants[VariantAIndex].bOverrideTrainingTooltip = true;
	ValidEntry.Variants[VariantAIndex].TrainingTooltip = FText::FromString(VariantTitle);
	FUnitVariantTestAccess::GetTankVariants(*ValidAsset).Add(ETankSubtype::Tank_PzIV_H, ValidEntry);
	FDataValidationContext ValidContext;
	TestNotEqual(TEXT("A correct asset validates"), ValidAsset->IsDataValid(ValidContext),
	             EDataValidationResult::Invalid);
	TestEqual(TEXT("A correct asset reports no errors"), ValidContext.GetNumErrors(), 0u);

	UUnitVariantDataAsset* BrokenAsset = NewObject<UUnitVariantDataAsset>(GetTransientPackage());
	TMap<ETankSubtype, FUnitVariantEntry>& TankVariants = FUnitVariantTestAccess::GetTankVariants(*BrokenAsset);
	FUnitVariantEntry BaseWithClassEntry = MakeEntry(TwoVariants);
	BaseWithClassEntry.Variants[0].UnitClass = TSoftClassPtr<AActor>(FSoftObjectPath(ATankMaster::StaticClass()));
	BaseWithClassEntry.Variants[0].bOverrideTrainingTitle = true;
	BaseWithClassEntry.Variants[VariantAIndex].bOverrideTrainingDescription = true;
	TankVariants.Add(ETankSubtype::Tank_PzIV_H, BaseWithClassEntry);
	// Wrong parent for the tank map, and the same class again in the squad map.
	TankVariants.Add(ETankSubtype::Tank_Pz38t, MakeEntryWithVariantClass(ASquadController::StaticClass()));
	FUnitVariantTestAccess::GetSquadVariants(*BrokenAsset).Add(
		ESquadSubtype::Squad_Ger_JagerTruppKar98k, MakeEntryWithVariantClass(ASquadController::StaticClass()));

	FDataValidationContext BrokenContext;
	TestEqual(TEXT("A broken asset is invalid"), BrokenAsset->IsDataValid(BrokenContext),
	          EDataValidationResult::Invalid);
	TestTrue(TEXT("Base with a class is reported"), GetHasIssueContaining(BrokenContext, TEXT("Base must not have")));
	TestTrue(TEXT("A variant without a class is reported"), GetHasIssueContaining(BrokenContext, TEXT("no unit class")));
	TestTrue(TEXT("A wrong parent class is reported"), GetHasIssueContaining(BrokenContext, TEXT("is not a child of")));
	TestTrue(TEXT("A class used twice is reported"), GetHasIssueContaining(BrokenContext, TEXT("variant definitions")));
	TestTrue(TEXT("A training override on Base is reported"),
	         GetHasIssueContaining(BrokenContext, TEXT("untick its training overrides")));
	TestTrue(TEXT("A ticked override with an empty value is reported"),
	         GetHasIssueContaining(BrokenContext, TEXT("empty TrainingDescription")));
	return true;
}
#endif

#endif
