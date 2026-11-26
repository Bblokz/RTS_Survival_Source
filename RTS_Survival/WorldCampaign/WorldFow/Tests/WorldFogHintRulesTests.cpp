// Copyright (C) Bas Blokzijl - All rights reserved.

#include "Misc/Build.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Misc/AutomationTest.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionBase.h"
#include "RTS_Survival/WorldCampaign/WorldFow/WorldFogHintRules.h"
#include "RTS_Survival/WorldCampaign/WorldFow/WorldFogHintSettings.h"
#include "RTS_Survival/WorldCampaign/WorldFow/WorldFowParticipant.h"
#include "RTS_Survival/WorldCampaign/WorldFow/WorldMapFowComponent.h"
#include "RTS_Survival/WorldCampaign/WorldFow/WorldUnknownMissionMarkerComponent.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldEnemyObject/WorldEnemyObject.h"
#include "RTS_Survival/WorldCampaign/WorldMapObjects/Objects/WorldMissionObject/WorldMissionObject.h"

namespace WorldFogHintTestData
{
	const EWorldMapFowState AllFowStates[] = {
		EWorldMapFowState::NotVisible, EWorldMapFowState::Explorable, EWorldMapFowState::Visible,
		EWorldMapFowState::POIVisible
	};
	const EMapItemType ItemTypesWithoutMarker[] = {
		EMapItemType::None, EMapItemType::Empty, EMapItemType::EnemyItem, EMapItemType::PlayerItem,
		EMapItemType::NeutralItem
	};
	constexpr float Tolerance = 0.01f;
	constexpr float FogRoadHeight = 250.f;
	constexpr float MaxSegmentLength = 300.f;
	// The engine cylinder the marker is made of.
	constexpr float MarkerMeshSize = 100.f;
	// A mission piece is about this wide; the marker has to stay inside that footprint.
	constexpr float MissionPieceWidth = 200.f;

	FString GetStateName(const EWorldMapFowState FowState)
	{
		return StaticEnum<EWorldMapFowState>()->GetNameStringByValue(static_cast<int64>(FowState));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldFogHintMissionMarkerTest, "RTS.WorldCampaign.FogHints.MissionMarkerByState",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldFogHintMissionMarkerTest::RunTest(const FString& Parameters)
{
	using namespace WorldFogHintTestData;
	for (const EWorldMapFowState FowState : AllFowStates)
	{
		const bool bExpectsMarker = FowState == EWorldMapFowState::NotVisible;
		TestEqual(FString::Printf(TEXT("Mission marker in state %s"), *GetStateName(FowState)),
		          WorldFogHintRules::GetShowsUnknownMarker(EMapItemType::Mission, FowState, false, false, true), bExpectsMarker);
		TestFalse(FString::Printf(TEXT("Open fog shows the real piece in state %s"), *GetStateName(FowState)),
		          WorldFogHintRules::GetShowsUnknownMarker(EMapItemType::Mission, FowState, true, false, true));
		TestFalse(FString::Printf(TEXT("A won mission shows no marker in state %s"), *GetStateName(FowState)),
		          WorldFogHintRules::GetShowsUnknownMarker(EMapItemType::Mission, FowState, false, true, true));
		TestFalse(FString::Printf(TEXT("The setting switches the marker off in state %s"), *GetStateName(FowState)),
		          WorldFogHintRules::GetShowsUnknownMarker(EMapItemType::Mission, FowState, false, false, false));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldFogHintEnemyStaysHiddenTest, "RTS.WorldCampaign.FogHints.EnemyStaysHidden",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldFogHintEnemyStaysHiddenTest::RunTest(const FString& Parameters)
{
	using namespace WorldFogHintTestData;
	for (const EMapItemType ItemType : ItemTypesWithoutMarker)
	{
		for (const EWorldMapFowState FowState : AllFowStates)
		{
			TestFalse(FString::Printf(TEXT("Item type %d gets no marker in state %s"), static_cast<int32>(ItemType),
			                          *GetStateName(FowState)),
			          WorldFogHintRules::GetShowsUnknownMarker(ItemType, FowState, false, false, true));
		}
	}

	// The marker is created by the mission object at runtime; no map piece class carries one of its own.
	const AWorldEnemyObject* const EnemyObjectDefaults = GetDefault<AWorldEnemyObject>();
	const AWorldDivisionBase* const DivisionDefaults = GetDefault<AWorldDivisionBase>();
	const AWorldMissionObject* const MissionDefaults = GetDefault<AWorldMissionObject>();
	TestNull(TEXT("An enemy object has no unknown marker component"),
	         EnemyObjectDefaults->FindComponentByClass<UWorldUnknownMissionMarkerComponent>());
	TestNull(TEXT("A division has no unknown marker component"),
	         DivisionDefaults->FindComponentByClass<UWorldUnknownMissionMarkerComponent>());
	TestNull(TEXT("A mission has no marker until the fog asks for one"), MissionDefaults->GetUnknownMarker());
	TestFalse(TEXT("A mission starts as its real piece"), MissionDefaults->GetIsShownAsUnknownMarker());
	// Divisions are hidden by the fog cloud alone; they take no part in the fog states the marker follows.
	TestFalse(TEXT("A division is no fog participant"),
	          AWorldDivisionBase::StaticClass()->ImplementsInterface(UWorldFowParticipant::StaticClass()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldFogHintMissionPresentationTest, "RTS.WorldCampaign.FogHints.MissionPresentation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldFogHintMissionPresentationTest::RunTest(const FString& Parameters)
{
	using namespace WorldFogHintTestData;
	const EWorldUnknownMissionLook RealPiece = EWorldUnknownMissionLook::RealPiece;
	const EWorldUnknownMissionLook PlainMarker = EWorldUnknownMissionLook::PlainMarker;
	for (const EWorldMapFowState FowState : AllFowStates)
	{
		const bool bIsFullyHidden = FowState == EWorldMapFowState::NotVisible;
		const FString StateName = GetStateName(FowState);
		TestTrue(FString::Printf(TEXT("Real piece look in state %s"), *StateName),
		         WorldFogHintRules::GetMissionFogPresentation(EMapItemType::Mission, FowState, false, false, true,
		                                                      RealPiece, false)
		         == (bIsFullyHidden ? EWorldMissionFogPresentation::RealPieceThroughFog
			             : EWorldMissionFogPresentation::AsFogState));
		TestTrue(FString::Printf(TEXT("Plain marker look in state %s"), *StateName),
		         WorldFogHintRules::GetMissionFogPresentation(EMapItemType::Mission, FowState, false, false, true,
		                                                      PlainMarker, false)
		         == (bIsFullyHidden ? EWorldMissionFogPresentation::PlainMarker : EWorldMissionFogPresentation::AsFogState));
		// The opening would show a hidden enemy piece: the mission falls back to the marker, which opens nothing.
		TestTrue(FString::Printf(TEXT("A hidden enemy piece next to the mission in state %s"), *StateName),
		         WorldFogHintRules::GetMissionFogPresentation(EMapItemType::Mission, FowState, false, false, true,
		                                                      RealPiece, true)
		         == (bIsFullyHidden ? EWorldMissionFogPresentation::PlainMarker : EWorldMissionFogPresentation::AsFogState));
		TestTrue(FString::Printf(TEXT("Open fog, a won mission and the switch leave state %s alone"), *StateName),
		         WorldFogHintRules::GetMissionFogPresentation(EMapItemType::Mission, FowState, true, false, true,
		                                                      RealPiece, false) == EWorldMissionFogPresentation::AsFogState
		         && WorldFogHintRules::GetMissionFogPresentation(EMapItemType::Mission, FowState, false, true, true,
		                                                         RealPiece, false) == EWorldMissionFogPresentation::AsFogState
		         && WorldFogHintRules::GetMissionFogPresentation(EMapItemType::Mission, FowState, false, false, false,
		                                                         RealPiece, false) == EWorldMissionFogPresentation::AsFogState);
		for (const EMapItemType ItemType : ItemTypesWithoutMarker)
		{
			TestTrue(FString::Printf(TEXT("Item type %d gets no opening and no marker in state %s"),
			                         static_cast<int32>(ItemType), *StateName),
			         WorldFogHintRules::GetMissionFogPresentation(ItemType, FowState, false, false, true, RealPiece, false)
			         == EWorldMissionFogPresentation::AsFogState);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldFogHintOpeningTest, "RTS.WorldCampaign.FogHints.OpeningKeepsEnemyHidden",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldFogHintOpeningTest::RunTest(const FString& Parameters)
{
	using namespace WorldFogHintTestData;
	constexpr float OpeningRadius = 120.f;
	constexpr float OpeningFalloff = 50.f;
	constexpr float MaskPixelSize = 40.f;
	constexpr float EnemyPieceHalfWidth = 100.f;
	const float ExposureRadius = WorldFogHintRules::GetMissionOpeningExposureRadius(
		OpeningRadius, OpeningFalloff, MaskPixelSize, EnemyPieceHalfWidth);
	TestEqual(TEXT("Opening, soft edge, one mask pixel and half an enemy piece"), ExposureRadius, 310.f, Tolerance);

	const FVector MissionLocation(1000.f, 1000.f, 0.f);
	// Height is ignored: the map is looked at from above.
	const TArray<FVector> EnemyJustInside = {FVector(5000.f, 0.f, 0.f), FVector(1000.f + ExposureRadius - 1.f, 1000.f, 300.f)};
	const TArray<FVector> EnemyJustOutside = {FVector(1000.f + ExposureRadius + 1.f, 1000.f, 0.f)};
	TestTrue(TEXT("An enemy piece the opening would touch is found"),
	         WorldFogHintRules::GetIsAnyPieceInsideRadius(MissionLocation, EnemyJustInside, ExposureRadius));
	TestFalse(TEXT("An enemy piece clear of the opening is not"),
	          WorldFogHintRules::GetIsAnyPieceInsideRadius(MissionLocation, EnemyJustOutside, ExposureRadius));
	TestFalse(TEXT("No enemy pieces"),
	          WorldFogHintRules::GetIsAnyPieceInsideRadius(MissionLocation, TArray<FVector>(), ExposureRadius));

	// The two rules together: the enemy inside keeps the fog closed, so it stays hidden.
	const bool bIsEnemyInside = WorldFogHintRules::GetIsAnyPieceInsideRadius(MissionLocation, EnemyJustInside, ExposureRadius);
	TestTrue(TEXT("No opening is cut next to a hidden enemy piece"),
	         WorldFogHintRules::GetMissionFogPresentation(EMapItemType::Mission, EWorldMapFowState::NotVisible, false,
	                                                      false, true, EWorldUnknownMissionLook::RealPiece, bIsEnemyInside)
	         == EWorldMissionFogPresentation::PlainMarker);

	const UWorldFogHintSettings* const Settings = UWorldFogHintSettings::Get();
	TestTrue(TEXT("The real piece is the default look"), Settings->UnknownMissionLook == EWorldUnknownMissionLook::RealPiece);
	TestTrue(TEXT("The opening covers a mission piece"), Settings->MissionOpeningRadius >= MissionPieceWidth / 2.f);
	TestTrue(TEXT("The opening stays tight: under one piece width past the piece"),
	         Settings->MissionOpeningRadius + Settings->MissionOpeningFalloff <= MissionPieceWidth);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldFogHintStateAccessTest, "RTS.WorldCampaign.FogHints.HiddenStateBlocksClickAndOutline",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldFogHintStateAccessTest::RunTest(const FString& Parameters)
{
	using namespace WorldFogHintTestData;
	UWorldMapFowComponent* const FowComponent = NewObject<UWorldMapFowComponent>();
	int32 AppliedCount = 0;
	FowComponent->OnFowStateApplied.AddLambda([&AppliedCount](EWorldMapFowState)
	{
		++AppliedCount;
	});

	for (const EWorldMapFowState FowState : AllFowStates)
	{
		FowComponent->SetCurrentFowState(FowState);
		const bool bIsUnknownMarkerState =
			WorldFogHintRules::GetShowsUnknownMarker(EMapItemType::Mission, FowState, false, false, true);
		if (bIsUnknownMarkerState)
		{
			// The marker state is exactly the one in which the mission can neither be clicked nor outlined.
			TestFalse(TEXT("An unknown mission cannot be clicked"), FowComponent->GetCanPrimaryClickInteractForCurrentState());
			TestFalse(TEXT("An unknown mission gets no outline"), FowComponent->GetCanBeOutlinedForCurrentState());
			continue;
		}

		TestTrue(FString::Printf(TEXT("State %s keeps its outline"), *GetStateName(FowState)),
		         FowComponent->GetCanBeOutlinedForCurrentState());
	}

	// Applying the same state twice must still tell the owner: the first fog pass applies the default state.
	FowComponent->SetCurrentFowState(EWorldMapFowState::NotVisible);
	FowComponent->SetCurrentFowState(EWorldMapFowState::NotVisible);
	TestEqual(TEXT("Every applied state is broadcast"), AppliedCount, static_cast<int32>(UE_ARRAY_COUNT(AllFowStates)) + 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldFogHintRoadsTest, "RTS.WorldCampaign.FogHints.RoadsUnderFog",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldFogHintRoadsTest::RunTest(const FString& Parameters)
{
	using namespace WorldFogHintTestData;
	for (const EWorldMapFowState FowState : AllFowStates)
	{
		const bool bExpectsFogRoad = FowState == EWorldMapFowState::NotVisible;
		TestEqual(FString::Printf(TEXT("Dim road under closed fog in state %s"), *GetStateName(FowState)),
		          WorldFogHintRules::GetShowsRoadUnderFog(FowState, false, true), bExpectsFogRoad);
		TestFalse(FString::Printf(TEXT("No dim road where the fog is open in state %s"), *GetStateName(FowState)),
		          WorldFogHintRules::GetShowsRoadUnderFog(FowState, true, true));
		TestFalse(FString::Printf(TEXT("The setting switches dim roads off in state %s"), *GetStateName(FowState)),
		          WorldFogHintRules::GetShowsRoadUnderFog(FowState, false, false));
	}

	const FLinearColor RoadColor(0.8f, 0.6f, 0.4f, 1.f);
	const FLinearColor FogRoadColor = WorldFogHintRules::GetFogRoadColor(RoadColor, 0.5f, 0.25f);
	TestEqual(TEXT("Red is dimmed"), FogRoadColor.R, 0.4f, Tolerance);
	TestEqual(TEXT("Green is dimmed"), FogRoadColor.G, 0.3f, Tolerance);
	TestEqual(TEXT("Blue is dimmed"), FogRoadColor.B, 0.2f, Tolerance);
	TestEqual(TEXT("Alpha takes the opacity"), FogRoadColor.A, 0.25f, Tolerance);
	const FLinearColor ClampedColor = WorldFogHintRules::GetFogRoadColor(RoadColor, 3.f, -1.f);
	TestEqual(TEXT("A road under fog is never brighter than a visible road"), ClampedColor.R, RoadColor.R, Tolerance);
	TestEqual(TEXT("Opacity does not go below zero"), ClampedColor.A, 0.f, Tolerance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldFogHintRoadPathTest, "RTS.WorldCampaign.FogHints.FogRoadPath",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldFogHintRoadPathTest::RunTest(const FString& Parameters)
{
	using namespace WorldFogHintTestData;
	// A straight road of 1000 units that climbs, then a short piece.
	const TArray<FVector> RoadPath = {FVector(0.f, 0.f, 5.f), FVector(1000.f, 0.f, 40.f), FVector(1000.f, 100.f, 40.f)};
	TArray<FVector> FogRoadPath;
	WorldFogHintRules::BuildFogRoadPath(RoadPath, FogRoadHeight, MaxSegmentLength, FogRoadPath);
	// 1000 / 300 rounds up to 4 pieces, the short piece stays whole: 1 + 4 + 1 points.
	TestEqual(TEXT("Point count"), FogRoadPath.Num(), 6);
	if (FogRoadPath.Num() != 6)
	{
		return false;
	}

	for (int32 PointIndex = 0; PointIndex < FogRoadPath.Num(); ++PointIndex)
	{
		TestEqual(FString::Printf(TEXT("Point %d lies at the fog road height"), PointIndex),
		          static_cast<float>(FogRoadPath[PointIndex].Z), FogRoadHeight, Tolerance);
	}
	for (int32 PointIndex = 1; PointIndex < FogRoadPath.Num(); ++PointIndex)
	{
		const float PieceLength = FVector::Dist(FogRoadPath[PointIndex - 1], FogRoadPath[PointIndex]);
		TestTrue(FString::Printf(TEXT("Piece %d is not longer than the maximum"), PointIndex),
		         PieceLength <= MaxSegmentLength + Tolerance);
	}
	TestEqual(TEXT("First cut"), static_cast<float>(FogRoadPath[1].X), 250.f, Tolerance);
	TestEqual(TEXT("The path keeps its corner"), static_cast<float>(FogRoadPath[4].X), 1000.f, Tolerance);
	TestEqual(TEXT("The path keeps its end"), static_cast<float>(FogRoadPath[5].Y), 100.f, Tolerance);

	TArray<FVector> EmptyPath;
	WorldFogHintRules::BuildFogRoadPath({FVector::ZeroVector}, FogRoadHeight, MaxSegmentLength, EmptyPath);
	TestEqual(TEXT("One point is no road"), EmptyPath.Num(), 0);
	TArray<FVector> ZeroLengthLimitPath;
	WorldFogHintRules::BuildFogRoadPath({FVector::ZeroVector, FVector(10.f, 0.f, 0.f)}, FogRoadHeight, 0.f,
	                                    ZeroLengthLimitPath);
	TestEqual(TEXT("A zero length limit is clamped, not divided by"), ZeroLengthLimitPath.Num(), 11);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldFogHintMarkerShapeTest, "RTS.WorldCampaign.FogHints.MarkerShapeAndDefaults",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWorldFogHintMarkerShapeTest::RunTest(const FString& Parameters)
{
	using namespace WorldFogHintTestData;
	const FVector MarkerScale = WorldFogHintRules::GetUnknownMarkerScale(170.f, 22.f, MarkerMeshSize);
	TestEqual(TEXT("Width scale X"), static_cast<float>(MarkerScale.X), 1.7f, Tolerance);
	TestEqual(TEXT("Width scale Y"), static_cast<float>(MarkerScale.Y), 1.7f, Tolerance);
	TestEqual(TEXT("Height scale"), static_cast<float>(MarkerScale.Z), 0.22f, Tolerance);
	TestEqual(TEXT("The marker stands on the fog"), WorldFogHintRules::GetUnknownMarkerCenterZ(300.f, 6.f, 22.f), 317.f,
	          Tolerance);

	// The settings object the game reads: defaults from the constructor plus anything in DefaultGame.ini.
	const UWorldFogHintSettings* const Settings = UWorldFogHintSettings::Get();
	// The owner may tune it up to the full colour in DefaultGame.ini; brighter than a visible road is never right.
	TestTrue(TEXT("A road under fog is not brighter than a visible road"), Settings->FogRoadBrightness <= 1.f);
	TestTrue(TEXT("A road under fog is still readable"), Settings->FogRoadBrightness > 0.f && Settings->FogRoadOpacity > 0.f);
	TestTrue(TEXT("Dim roads draw over the fog cloud"), Settings->FogRoadSortPriority > 0);
	TestTrue(TEXT("Hidden real roads draw under the fog cloud"), Settings->RoadBelowFogSortPriority < 0);
	TestTrue(TEXT("The marker is a low puck"),
	         Settings->UnknownMissionMarkerHeight < Settings->UnknownMissionMarkerDiameter);
	TestTrue(TEXT("The marker stays inside the footprint of a mission piece"),
	         Settings->UnknownMissionMarkerDiameter <= MissionPieceWidth);
	TestFalse(TEXT("The hover tag has a text"), Settings->UnknownMissionHoverText.IsEmpty());
	const FLinearColor& MarkerColor = Settings->UnknownMissionMarkerColor;
	TestTrue(TEXT("The marker is plain grey: no colour that could stand for a country or a side"),
	         FMath::IsNearlyEqual(MarkerColor.R, MarkerColor.G, Tolerance)
	         && FMath::IsNearlyEqual(MarkerColor.G, MarkerColor.B, Tolerance));
	return true;
}

#endif
