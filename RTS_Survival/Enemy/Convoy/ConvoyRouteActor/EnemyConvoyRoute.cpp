// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyConvoyRoute.h"

#include "EngineUtils.h"
#include "Engine/Texture2D.h"
#include "RTS_Survival/Enemy/Convoy/Convoy.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyDefinition.h"
#include "RTS_Survival/Enemy/Convoy/ConvoyRoute.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementShapesComponent.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementTypes.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "UObject/ConstructorHelpers.h"

namespace EnemyConvoyRouteConstants
{
	// Same spacing survival uses for its convoy routes (USurvivalEventSettings::RouteResampleSpacing).
	constexpr float DrivePathResampleSpacing = 600.f;
	const FVector DefaultExitPoint(8000.f, 0.f, 0.f);
	// Used for the exit ring before a definition is set.
	constexpr float FallbackExitRadius = 1500.f;

	// Editor drawing.
	constexpr float PathLiftZ = 80.f;
	constexpr float RouteThickness = 14.f;
	constexpr float PointStemHeight = 600.f;
	constexpr float StemThickness = 6.f;
	const FVector StartBoxHalfExtent(260.f, 160.f, 90.f);
	constexpr float ViaPointRadius = 110.f;
	constexpr float ExitRingThickness = 8.f;
	constexpr float DrivePathDashSize = 160.f;
	constexpr float DrivePathBrightness = 0.65f;
	constexpr float PreviewMatchTolerance = 1.f;
}

namespace
{
	FVector LiftRoutePoint(const FVector& Point)
	{
		return Point + FVector(0.f, 0.f, EnemyConvoyRouteConstants::PathLiftZ);
	}
}

AEnemyConvoyRoute::AEnemyConvoyRoute()
{
	RoutePoints = {FVector::ZeroVector, EnemyConvoyRouteConstants::DefaultExitPoint};
#if WITH_EDITORONLY_DATA
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinderOptional<UTexture2D> RouteSprite;

		FConstructorStatics()
			: RouteSprite(TEXT("/Engine/EditorResources/S_Pawn"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;
	M_RouteSprite = ConstructorStatics.RouteSprite.Get();
#endif
}

void AEnemyConvoyRoute::BeginPlay()
{
	Super::BeginPlay();
	if (Timing.bStartOnBeginPlay)
	{
		StartRoute();
	}
}

void AEnemyConvoyRoute::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_NextConvoyTimerHandle);
	}
	bM_IsRunning = false;
	UnbindAllConvoys();
	Super::EndPlay(EndPlayReason);
}

// ------------------------------------------------------------------
// Start / stop
// ------------------------------------------------------------------

void AEnemyConvoyRoute::StartRoute()
{
	if (bM_IsRunning)
	{
		return;
	}
	M_SingleConvoyTruckCount = 0;
	BeginRun();
}

bool AEnemyConvoyRoute::StartSingleConvoy(const int32 TruckCount)
{
	if (bM_IsRunning || TruckCount <= 0)
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("Convoy route %s cannot start a single convoy of %d trucks: %s"), *GetName(), TruckCount,
			bM_IsRunning
				? TEXT("it already runs (turn Start On Begin Play off for routes a field battle starts).")
				: TEXT("a convoy needs at least one truck.")));
		return false;
	}
	M_SingleConvoyTruckCount = TruckCount;
	BeginRun();
	return true;
}

void AEnemyConvoyRoute::BeginRun()
{
	bM_IsRunning = true;
	bM_HasLaunchedLastConvoy = false;
	bM_HasBuiltRoute = false;
	bM_HasTriedBranchRoute = false;
	M_BuiltBranchRoute.Points.Reset();
	M_ConvoysThisRun = 0;
	WaitForEnemyAI(Timing.FirstConvoyDelay);
}

void AEnemyConvoyRoute::StopRoute()
{
	if (not bM_IsRunning)
	{
		return;
	}
	CancelWaitForEnemyAI();
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_NextConvoyTimerHandle);
	}
	MarkLastConvoyLaunched();
}

AEnemyConvoyRoute* AEnemyConvoyRoute::FindEnemyConvoyRoute(const UObject* WorldContextObject,
                                                           const FName RouteIdToFind)
{
	const UWorld* World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	if (not IsValid(World) || RouteIdToFind.IsNone())
	{
		RTSFunctionLibrary::ReportError("FindEnemyConvoyRoute needs a valid world context and a route ID.");
		return nullptr;
	}
	for (TActorIterator<AEnemyConvoyRoute> It(World); It; ++It)
	{
		if (It->RouteId == RouteIdToFind)
		{
			return *It;
		}
	}
	RTSFunctionLibrary::ReportError("FindEnemyConvoyRoute found no convoy route with ID: " + RouteIdToFind.ToString());
	return nullptr;
}

// ------------------------------------------------------------------
// Launch
// ------------------------------------------------------------------

void AEnemyConvoyRoute::OnEnemyAIReady(AEnemyController* EnemyController, const ERTSGameDifficulty Difficulty)
{
	M_Difficulty = Difficulty;
	if (IsValid(EnemyController))
	{
		M_EnemyController = EnemyController;
	}
	LaunchConvoy();
}

void AEnemyConvoyRoute::LaunchConvoy()
{
	if (not bM_IsRunning || bM_HasLaunchedLastConvoy)
	{
		return;
	}
	FConvoyRoute Route;
	if (not GetIsOwningBuildingAlive() || not TryGetBuiltRoute(Route))
	{
		// No convoy comes from a destroyed building; a broken route was reported by TryGetBuiltRoute.
		MarkLastConvoyLaunched();
		return;
	}
	const int32 ConvoyIndex = M_ConvoysThisRun;
	// Behaviour filters and events use the index over every run, so mission-started convoys are 0, 1, 2...
	const int32 RouteConvoyIndex = M_StartedConvoyCount;
	const FConvoyStartParams Params = BuildStartParams(ConvoyIndex);
	AConvoy* Convoy = AConvoy::StartConvoy(this, Definition, Route, Params);
	if (not IsValid(Convoy))
	{
		RTSFunctionLibrary::ReportError("Convoy route " + GetName() + " could not start a convoy; see the error above.");
		MarkLastConvoyLaunched();
		return;
	}
	// Same frame as the start: the convoy's first tick comes later, so the plan is in place before it drives.
	Convoy->SetBehaviourPlan(BuildBehaviourPlan(RouteConvoyIndex, Route));
	M_StartedConvoyCount++;
	M_ConvoysThisRun++;
	TrackStartedConvoy(Convoy, Params.GuardTierIndex, RouteConvoyIndex);
	OnConvoyStarted.Broadcast(this, Convoy);
	if (not GetHasMoreConvoysToStart())
	{
		MarkLastConvoyLaunched();
		return;
	}
	if (not Timing.bWaitForPreviousConvoy)
	{
		ScheduleNextConvoy();
	}
}

bool AEnemyConvoyRoute::GetIsOwningBuildingAlive() const
{
	if (not Ownership.bHasOwningBuilding)
	{
		return true;
	}
	// A destroyed building is nulled by garbage collection, so "not set" and "destroyed" look the same here;
	// a building that was never set is flagged on the editor label instead.
	return RTSFunctionLibrary::RTSIsValid(Ownership.OwningBuilding);
}

bool AEnemyConvoyRoute::TryGetBuiltRoute(FConvoyRoute& OutRoute)
{
	if (bM_HasBuiltRoute)
	{
		OutRoute = M_BuiltRoute;
		return true;
	}
	const FString Problem = GetConfigurationProblem();
	if (not Problem.IsEmpty())
	{
		RTSFunctionLibrary::ReportError("Convoy route " + GetName() + " cannot start: " + Problem);
		return false;
	}
	bool bReachedEnd = false;
	const FName BuiltRouteId = RouteId.IsNone() ? GetFName() : RouteId;
	M_BuiltRoute = UConvoyRouteLibrary::BuildNavigableRoute(this, GetWorldPoints(RoutePoints),
	                                                        EnemyConvoyRouteConstants::DrivePathResampleSpacing,
	                                                        BuiltRouteId, bReachedEnd);
	if (not bReachedEnd || M_BuiltRoute.Points.Num() < 2)
	{
		// A shortened route would let the trucks "escape" in the middle of the map.
		RTSFunctionLibrary::ReportError("Convoy route " + GetName() + " has no drivable path from its start to its exit;"
			" move the route points onto the vehicle navmesh (Preview Drive Path shows where it stops).");
		return false;
	}
	bM_HasBuiltRoute = true;
	OutRoute = M_BuiltRoute;
	return true;
}

int32 AEnemyConvoyRoute::GetBaseGuardTierForDifficulty() const
{
	switch (M_Difficulty)
	{
	case ERTSGameDifficulty::NewToRTS:
		return Escort.GuardTierNewToRTS;
	case ERTSGameDifficulty::Normal:
		return Escort.GuardTierNormal;
	case ERTSGameDifficulty::Hard:
		return Escort.GuardTierHard;
	default:
		// Ironman uses the Brutal tier.
		return Escort.GuardTierBrutal;
	}
}

int32 AEnemyConvoyRoute::GetGuardTierIndex(const int32 ConvoyIndex) const
{
	const int32 WantedTier = GetBaseGuardTierForDifficulty() + Escort.GuardTierStepPerConvoy * ConvoyIndex;
	const int32 LastTier = IsValid(Definition) ? FMath::Max(Definition->GuardTiers.Num() - 1, 0) : 0;
	return FMath::Clamp(WantedTier, 0, LastTier);
}

FConvoyStartParams AEnemyConvoyRoute::BuildStartParams(const int32 ConvoyIndex) const
{
	FConvoyStartParams Params;
	// The convoy drives for the AI player this route is placed for.
	Params.OwningPlayer = GetEnemyPlayer();
	Params.GuardTierIndex = GetGuardTierIndex(ConvoyIndex);
	Params.TruckCountOverride = M_SingleConvoyTruckCount;
	Params.CargoMultiplier = Escort.CargoMultiplier;
	Params.bShowOnMinimap = bShowOnMinimap;
	const FString IdBase = RouteId.IsNone() ? GetName() : RouteId.ToString();
	Params.DebugLabel = FString::Printf(TEXT("%s#%d"), *IdBase, ConvoyIndex + 1);
	return Params;
}

void AEnemyConvoyRoute::TrackStartedConvoy(AConvoy* Convoy, const int32 GuardTierIndex, const int32 ConvoyIndex)
{
	FEnemyConvoyRouteConvoyTally& Tally = M_Convoys.AddDefaulted_GetRef();
	Tally.Convoy = Convoy;
	Tally.ConvoyId = Convoy->GetConvoyId();
	Tally.ConvoyIndex = ConvoyIndex;
	Tally.TruckCount = Definition->GetTruckColumnForStart(GuardTierIndex,
	                                                      Convoy->GetStartParams().TruckCountOverride).Num();
	Tally.EventHandle = Convoy->OnConvoyEventNative.AddUObject(this, &AEnemyConvoyRoute::HandleConvoyEvent);
	Tally.BehaviourHandle = Convoy->OnBehaviourEventNative.AddUObject(this,
	                                                                  &AEnemyConvoyRoute::HandleConvoyBehaviourEvent);
	Convoy->OnConvoyFinished.AddDynamic(this, &AEnemyConvoyRoute::HandleConvoyFinished);
}

bool AEnemyConvoyRoute::GetHasMoreConvoysToStart() const
{
	if (M_SingleConvoyTruckCount > 0)
	{
		return false;
	}
	switch (Timing.Repeat)
	{
	case EEnemyWaveSpawnerRepeat::Once:
		return false;
	case EEnemyWaveSpawnerRepeat::FixedCount:
		return M_ConvoysThisRun < FMath::Max(Timing.ConvoyCount, 1);
	default:
		return true;
	}
}

void AEnemyConvoyRoute::ScheduleNextConvoy()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	TWeakObjectPtr<AEnemyConvoyRoute> WeakThis(this);
	World->GetTimerManager().SetTimer(M_NextConvoyTimerHandle, FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->LaunchConvoy();
		}
	}), FMath::Max(Timing.ConvoyInterval, 1.f), false);
}

void AEnemyConvoyRoute::MarkLastConvoyLaunched()
{
	bM_HasLaunchedLastConvoy = true;
	TryFinishRoute();
}

void AEnemyConvoyRoute::TryFinishRoute()
{
	if (not bM_IsRunning || not bM_HasLaunchedLastConvoy || not M_Convoys.IsEmpty())
	{
		return;
	}
	bM_IsRunning = false;
	OnRouteFinished.Broadcast(this);
}

// ------------------------------------------------------------------
// Convoy callbacks
// ------------------------------------------------------------------

void AEnemyConvoyRoute::HandleConvoyEvent(const FConvoyEvent& Event)
{
	if (Event.Type == EConvoyEventType::TruckCaptured)
	{
		CountCapturedTruck(Event);
		return;
	}
	if (Event.Type != EConvoyEventType::TruckDestroyed)
	{
		return;
	}
	FEnemyConvoyRouteConvoyTally* Tally = FindTally(Event.ConvoyId);
	if (Tally == nullptr)
	{
		return;
	}
	Tally->DestroyedTruckCount++;
	M_DestroyedTruckCount++;
}

void AEnemyConvoyRoute::CountCapturedTruck(const FConvoyEvent& Event)
{
	FEnemyConvoyRouteConvoyTally* Tally = FindTally(Event.ConvoyId);
	if (Tally == nullptr)
	{
		return;
	}
	Tally->CapturedTruckCount++;
	M_CapturedTruckCount++;
	OnConvoyTruckCaptured.Broadcast(this, Tally->Convoy.Get(), Event.Actor.Get());
}

void AEnemyConvoyRoute::HandleConvoyFinished(AConvoy* Convoy, const EConvoyOutcome Outcome)
{
	if (not IsValid(Convoy))
	{
		return;
	}
	const int32 TallyIndex = M_Convoys.IndexOfByPredicate([Convoy](const FEnemyConvoyRouteConvoyTally& Tally)
	{
		return Tally.Convoy.Get() == Convoy;
	});
	if (TallyIndex == INDEX_NONE)
	{
		return;
	}
	const FEnemyConvoyRouteConvoyTally Tally = M_Convoys[TallyIndex];
	M_Convoys.RemoveAt(TallyIndex);
	UnbindConvoy(Tally);
	FConvoyCargoOutcome CargoOutcome = Convoy->GetCargoOutcome();
	CargoOutcome.ConvoyIndex = Tally.ConvoyIndex;
	// Counted before the broadcasts, so listeners read the route totals with this convoy in them.
	CountFinishedConvoy(Tally, Outcome, CargoOutcome.EscapedTrucks);
	OnConvoyCargoOutcome.Broadcast(this, Convoy, CargoOutcome);
	OnConvoyFinished.Broadcast(this, Convoy, Outcome);
	if (bM_IsRunning && not bM_HasLaunchedLastConvoy && Timing.bWaitForPreviousConvoy)
	{
		ScheduleNextConvoy();
	}
	TryFinishRoute();
}

FEnemyConvoyRouteConvoyTally* AEnemyConvoyRoute::FindTally(const int32 ConvoyId)
{
	return M_Convoys.FindByPredicate([ConvoyId](const FEnemyConvoyRouteConvoyTally& Tally)
	{
		return Tally.ConvoyId == ConvoyId;
	});
}

void AEnemyConvoyRoute::CountFinishedConvoy(const FEnemyConvoyRouteConvoyTally& Tally, const EConvoyOutcome Outcome,
                                            const int32 EscapedTruckCount)
{
	// The convoy counts the trucks it removed at the exit itself: captured and branch-delivered trucks are not in it.
	M_EscapedTruckCount += FMath::Max(EscapedTruckCount, 0);
	if (Outcome == EConvoyOutcome::Escaped)
	{
		M_EscapedConvoyCount++;
		return;
	}
	if (Outcome == EConvoyOutcome::TrucksDestroyed)
	{
		M_DestroyedConvoyCount++;
		return;
	}
	if (Outcome == EConvoyOutcome::TrucksStopped)
	{
		M_StoppedConvoyCount++;
	}
}

void AEnemyConvoyRoute::UnbindConvoy(const FEnemyConvoyRouteConvoyTally& Tally)
{
	AConvoy* Convoy = Tally.Convoy.Get();
	if (not IsValid(Convoy))
	{
		return;
	}
	Convoy->OnConvoyEventNative.Remove(Tally.EventHandle);
	Convoy->OnBehaviourEventNative.Remove(Tally.BehaviourHandle);
	Convoy->OnConvoyFinished.RemoveDynamic(this, &AEnemyConvoyRoute::HandleConvoyFinished);
}

void AEnemyConvoyRoute::UnbindAllConvoys()
{
	for (const FEnemyConvoyRouteConvoyTally& Tally : M_Convoys)
	{
		UnbindConvoy(Tally);
	}
	M_Convoys.Empty();
}

// ------------------------------------------------------------------
// Editor
// ------------------------------------------------------------------

FColor AEnemyConvoyRoute::GetPlacementColor() const
{
	return EnemyPlacementColors::Convoy;
}

UTexture2D* AEnemyConvoyRoute::GetEditorSpriteTexture() const
{
#if WITH_EDITORONLY_DATA
	return M_RouteSprite.Get();
#else
	return nullptr;
#endif
}

FString AEnemyConvoyRoute::GetConfigurationProblem() const
{
	if (not IsValid(Definition))
	{
		return TEXT("no convoy definition");
	}
	const TArray<FString> DefinitionProblems = Definition->Validate();
	if (not DefinitionProblems.IsEmpty())
	{
		return TEXT("definition: ") + DefinitionProblems[0];
	}
	if (RoutePoints.Num() < 2)
	{
		return TEXT("needs a start and an exit point");
	}
	if (Ownership.bHasOwningBuilding && Ownership.OwningBuilding == nullptr)
	{
		return TEXT("no owning building");
	}
#if WITH_EDITORONLY_DATA
	if (GetIsPreviewCurrent() && not bM_PreviewReachedExit)
	{
		return TEXT("drive path does not reach the exit");
	}
#endif
	return FString();
}

float AEnemyConvoyRoute::GetExitRadius() const
{
	return IsValid(Definition) ? Definition->Speed.ExitRadius : EnemyConvoyRouteConstants::FallbackExitRadius;
}

bool AEnemyConvoyRoute::GetIsPreviewCurrent() const
{
#if WITH_EDITORONLY_DATA
	const TArray<FVector> WorldRoutePoints = GetWorldPoints(RoutePoints);
	if (M_PreviewDrivePath.IsEmpty() || WorldRoutePoints.Num() != M_PreviewControlPoints.Num())
	{
		return false;
	}
	for (int32 PointIndex = 0; PointIndex < WorldRoutePoints.Num(); ++PointIndex)
	{
		if (not WorldRoutePoints[PointIndex].Equals(M_PreviewControlPoints[PointIndex],
		                                            EnemyConvoyRouteConstants::PreviewMatchTolerance))
		{
			return false;
		}
	}
	return true;
#else
	return false;
#endif
}

void AEnemyConvoyRoute::BuildEditorShapes(FEnemyPlacementShapes& OutSummary, FEnemyPlacementShapes& OutDetail) const
{
	const TArray<FVector> WorldRoutePoints = GetWorldPoints(RoutePoints);
	AddRoutePointShapes(WorldRoutePoints, OutSummary, OutDetail);
	AddPreviewShapes(OutDetail);
	AddBehaviourShapes(OutSummary, OutDetail);
}

void AEnemyConvoyRoute::AddRoutePointShapes(const TArray<FVector>& WorldRoutePoints, FEnemyPlacementShapes& OutSummary,
                                            FEnemyPlacementShapes& OutDetail) const
{
	using namespace EnemyConvoyRouteConstants;
	if (WorldRoutePoints.IsEmpty())
	{
		return;
	}
	const FColor Color = GetPlacementColor();
	// A thick road line with an arrow per leg: the convoy drives one way, start to exit.
	for (int32 PointIndex = 1; PointIndex < WorldRoutePoints.Num(); ++PointIndex)
	{
		const FVector LegStart = LiftRoutePoint(WorldRoutePoints[PointIndex - 1]);
		const FVector LegEnd = LiftRoutePoint(WorldRoutePoints[PointIndex]);
		OutSummary.Lines.Emplace(LegStart, LegEnd, Color, RouteThickness);
		OutSummary.ArrowLines.Emplace(LegStart, LegEnd, Color);
	}
	const FVector Start = WorldRoutePoints[0];
	const FVector FirstLegDirection = WorldRoutePoints.Num() > 1 ? WorldRoutePoints[1] - Start : FVector::ForwardVector;
	OutSummary.AddFlatBox(LiftRoutePoint(Start), StartBoxHalfExtent, FirstLegDirection.Rotation().Yaw, Color, true);
	OutSummary.Lines.Emplace(Start, Start + FVector(0.f, 0.f, PointStemHeight), Color, StemThickness);
	for (int32 PointIndex = 1; PointIndex < WorldRoutePoints.Num() - 1; ++PointIndex)
	{
		OutDetail.Spheres.Emplace(ViaPointRadius, WorldRoutePoints[PointIndex], FLinearColor(Color),
		                          FDebugRenderSceneProxy::EDrawType::WireMesh);
	}
	if (WorldRoutePoints.Num() < 2)
	{
		return;
	}
	const FVector Exit = WorldRoutePoints.Last();
	OutSummary.AddCircle(LiftRoutePoint(Exit), GetExitRadius(), Color, ExitRingThickness);
	OutSummary.Lines.Emplace(Exit, Exit + FVector(0.f, 0.f, PointStemHeight), Color, StemThickness);
}

void AEnemyConvoyRoute::AddPreviewShapes(FEnemyPlacementShapes& OutDetail) const
{
#if WITH_EDITORONLY_DATA
	using namespace EnemyConvoyRouteConstants;
	if (not GetIsPreviewCurrent())
	{
		return;
	}
	const FColor PreviewColor = bM_PreviewReachedExit
		                            ? EnemyPlacementColors::GetDimmed(GetPlacementColor(), DrivePathBrightness)
		                            : EnemyPlacementColors::Problem;
	for (int32 PointIndex = 1; PointIndex < M_PreviewDrivePath.Num(); ++PointIndex)
	{
		OutDetail.DashedLines.Emplace(LiftRoutePoint(M_PreviewDrivePath[PointIndex - 1]),
		                              LiftRoutePoint(M_PreviewDrivePath[PointIndex]), PreviewColor, DrivePathDashSize);
	}
#endif
}

#if WITH_EDITOR
void AEnemyConvoyRoute::PreviewDrivePath()
{
	bool bReachedEnd = false;
	const TArray<FVector> WorldRoutePoints = GetWorldPoints(RoutePoints);
	const FConvoyRoute Route = UConvoyRouteLibrary::BuildNavigableRoute(
		this, WorldRoutePoints, EnemyConvoyRouteConstants::DrivePathResampleSpacing, RouteId, bReachedEnd);
	if (Route.Points.Num() < 2)
	{
		RTSFunctionLibrary::ReportError("Convoy route " + GetName() + ": no drive path; the start is off the vehicle "
			"navmesh or the navigation is not built in this map.");
	}
	M_PreviewDrivePath = Route.Points;
	M_PreviewControlPoints = WorldRoutePoints;
	bM_PreviewReachedExit = bReachedEnd;
	RefreshEditorVisuals();
}

void AEnemyConvoyRoute::ClearDrivePathPreview()
{
	M_PreviewDrivePath.Empty();
	M_PreviewControlPoints.Empty();
	bM_PreviewReachedExit = true;
	RefreshEditorVisuals();
}
#endif
