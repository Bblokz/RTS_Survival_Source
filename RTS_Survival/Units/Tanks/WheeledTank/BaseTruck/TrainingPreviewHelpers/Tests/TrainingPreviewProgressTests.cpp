// Copyright (C) Bas Blokzijl - All rights reserved.

// QA for the training preview progress fill: the glide between the 1 s queue ticks and the material swap that gives
// the German previews the holo progress material. Dev-test-only: UBT defines WITH_DEV_AUTOMATION_TESTS=0 for
// Shipping and Test, so this translation unit is empty in the shipping binary.
#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/TrainingPreviewHelpers/TrainingPreviewHelper.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/TrainingPreviewHelpers/TrainingPreviewSettings/TrainingPreviewSettings.h"

namespace TrainingPreviewProgressTestsPrivate
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	constexpr float Tolerance = 0.0001f;
	constexpr int32 TenSecondUnit = 10;
	const TCHAR* const GermanPreviewMeshMaterial = TEXT(
		"/Game/RTS_Survival/Blueprints/Buildings/ConstructionPreview/Materials/MI_WhileConstructing.MI_WhileConstructing");
	const TCHAR* const SovietPreviewMeshMaterial = TEXT(
		"/Game/RTS_Survival/Blueprints/Buildings/ConstructionPreview/Materials/MI_SovietTrainingPreview.MI_SovietTrainingPreview");
	const TCHAR* const GermanHoloMaterial = TEXT(
		"/Game/RTS_Survival/Blueprints/Buildings/ConstructionPreview/Materials/TrainingPreview/"
		"MI_TrainingPreview_Holo_Ger.MI_TrainingPreview_Holo_Ger");
	// A registered German preview (Train_Tank_Tiger105) with MI_WhileConstructing on its only slot.
	const TCHAR* const KingTiger105Preview = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Heavy/TigerII/"
		"SM_KingTiger105_TrainingPreview.SM_KingTiger105_TrainingPreview");
	const FName BuildProgressParameterName = TEXT("BuildProgress");
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainingPreviewDisplayedProgressTest, "RTS.Training.PreviewProgress.DisplayedProgress",
                                 TrainingPreviewProgressTestsPrivate::TestFlags)

bool FTrainingPreviewDisplayedProgressTest::RunTest(const FString& Parameters)
{
	using namespace TrainingPreviewProgressTestsPrivate;
	const auto Displayed = [](const float Fraction, const int32 Total, const float SecondsSinceChange)
	{
		return FTrainingPreviewHelper::GetDisplayedBuildProgress(Fraction, Total, SecondsSinceChange);
	};
	TestEqual(TEXT("Nothing trained, tick just started"), Displayed(0.f, TenSecondUnit, 0.f), 0.f, Tolerance);
	TestEqual(TEXT("Glides half a tick ahead"), Displayed(0.f, TenSecondUnit, 0.5f), 0.05f, Tolerance);
	TestEqual(TEXT("Glide is capped at one tick (paused queue)"), Displayed(0.5f, TenSecondUnit, 5.f), 0.6f, Tolerance);
	TestEqual(TEXT("Full at the moment the last tick completes"), Displayed(0.9f, TenSecondUnit, 1.f), 1.f, Tolerance);
	TestEqual(TEXT("Never above 1 while waiting for the spawn"), Displayed(1.f, TenSecondUnit, 0.3f), 1.f, Tolerance);
	TestEqual(TEXT("Unknown total time shows the trained fraction"), Displayed(0.4f, 0, 0.5f), 0.4f, Tolerance);
	TestEqual(TEXT("Negative fraction clamps to 0"), Displayed(-0.2f, TenSecondUnit, 0.f), 0.f, Tolerance);
	TestEqual(TEXT("Negative elapsed time adds nothing"), Displayed(0.3f, TenSecondUnit, -1.f), 0.3f, Tolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTrainingPreviewProgressMaterialTest, "RTS.Training.PreviewProgress.GermanHoloMaterial",
                                 TrainingPreviewProgressTestsPrivate::TestFlags)

bool FTrainingPreviewProgressMaterialTest::RunTest(const FString& Parameters)
{
	using namespace TrainingPreviewProgressTestsPrivate;
	const UTrainingPreviewSettings* Settings = GetDefault<UTrainingPreviewSettings>();
	if (not TestNotNull(TEXT("Training preview settings"), Settings))
	{
		return false;
	}

	UMaterialInterface* GermanMeshMaterial = LoadObject<UMaterialInterface>(nullptr, GermanPreviewMeshMaterial);
	UMaterialInterface* SovietMeshMaterial = LoadObject<UMaterialInterface>(nullptr, SovietPreviewMeshMaterial);
	UMaterialInterface* HoloMaterial = LoadObject<UMaterialInterface>(nullptr, GermanHoloMaterial);
	if (not TestNotNull(TEXT("MI_WhileConstructing loads"), GermanMeshMaterial)
		|| not TestNotNull(TEXT("MI_SovietTrainingPreview loads"), SovietMeshMaterial)
		|| not TestNotNull(TEXT("MI_TrainingPreview_Holo_Ger loads"), HoloMaterial))
	{
		return false;
	}

	// The ini entry replaces the constructor default (same pair) instead of appending a second one.
	TestEqual(TEXT("Exactly one progress material is configured"), Settings->ProgressMaterials.Num(), 1);
	TestEqual(TEXT("German previews get the holo progress material"),
	          Settings->ResolveProgressMaterial(GermanMeshMaterial).ToSoftObjectPath().ToString(),
	          FSoftObjectPath(HoloMaterial).ToString());
	TestTrue(TEXT("Soviet previews keep their own material"), Settings->ResolveProgressMaterial(SovietMeshMaterial).IsNull());
	TestTrue(TEXT("No material, no progress material"), Settings->ResolveProgressMaterial(nullptr).IsNull());
	TestTrue(TEXT("The holo material loads with every preview mesh"),
	         Settings->GetProgressMaterialPaths().Contains(FSoftObjectPath(HoloMaterial)));

	float DefaultProgress = -1.f;
	TestTrue(TEXT("Holo material has a BuildProgress scalar"),
	         HoloMaterial->GetScalarParameterValue(FHashedMaterialParameterInfo(BuildProgressParameterName), DefaultProgress));
	TestEqual(TEXT("BuildProgress defaults to 0 (pure holo)"), DefaultProgress, 0.f, Tolerance);

	UMaterialInstanceDynamic* ProgressInstance = UMaterialInstanceDynamic::Create(HoloMaterial, GetTransientPackage());
	if (TestNotNull(TEXT("Dynamic instance of the holo material"), ProgressInstance))
	{
		constexpr float HalfTrained = 0.5f;
		ProgressInstance->SetScalarParameterValue(BuildProgressParameterName, HalfTrained);
		float ReadBack = -1.f;
		ProgressInstance->GetScalarParameterValue(FHashedMaterialParameterInfo(BuildProgressParameterName), ReadBack);
		TestEqual(TEXT("BuildProgress is settable on the dynamic instance"), ReadBack, HalfTrained, Tolerance);
	}

	const UStaticMesh* KingTigerPreview = LoadObject<UStaticMesh>(nullptr, KingTiger105Preview);
	if (TestNotNull(TEXT("SM_KingTiger105_TrainingPreview loads"), KingTigerPreview))
	{
		TestEqual(TEXT("A German preview mesh resolves to the holo material"),
		          Settings->ResolveProgressMaterial(KingTigerPreview->GetMaterial(0)).ToSoftObjectPath().ToString(),
		          FSoftObjectPath(HoloMaterial).ToString());
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
