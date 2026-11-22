// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyStrongpoint.h"

#include "EngineUtils.h"
#include "Animation/AnimMontage.h"
#include "Engine/Texture2D.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyBaseTrenchComponent/EnemyBaseTrenchComponent.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyDirectControlComponent/EnemyDirectControlComponent.h"
#include "RTS_Survival/Enemy/EnemyWaves/WaveSpawner/EnemyWaveSpawner.h"
#include "RTS_Survival/Enemy/MissionPlacement/EnemyPlacementShapesComponent.h"
#include "RTS_Survival/Enemy/Strongpoint/WorkCircuit/EnemyWorkCircuitRunner.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSBlueprintFunctionLibrary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "UObject/ConstructorHelpers.h"

namespace EnemyStrongpointConstants
{
	const FVector DefaultPostLocation(600.f, 0.f, 0.f);
	constexpr float WatchIntervalSeconds = 0.5f;
	// Used for the trench arc drawing when the map has no enemy controller to read it from.
	constexpr float FallbackTrenchArcDegrees = 140.f;

	// Editor drawing.
	constexpr float PostLiftZ = 70.f;
	const FVector VehiclePostHalfExtent(300.f, 170.f, 90.f);
	constexpr float SquadPostRadius = 150.f;
	constexpr float FacingArrowLength = 650.f;
	const FVector DigInPitHalfExtent(380.f, 240.f, 10.f);
	constexpr float OutlineMargin = 500.f;
	constexpr float MinimumOutlineRadius = 800.f;
	constexpr float OutlineDashSize = 140.f;
	constexpr int32 OutlineSegments = 40;
	constexpr float CentreLinkThickness = 1.5f;
	constexpr float TrenchRingThickness = 2.f;
	constexpr float TrenchArcThickness = 16.f;
	constexpr float ReinforcementDashSize = 220.f;
	constexpr float TrenchBrightness = 0.7f;
	constexpr float WorkPointRadius = 90.f;
	constexpr float WorkCircuitDashSize = 120.f;
	constexpr float WorkCircuitBrightness = 0.8f;
}

namespace
{
	FVector LiftPostPoint(const FVector& Point)
	{
		return Point + FVector(0.f, 0.f, EnemyStrongpointConstants::PostLiftZ);
	}

	bool GetIsSquadPost(const FEnemyStrongpointPost& StrongpointPost)
	{
		// Only squads walk work circuits, so such a post is a squad post even before a mission picks its units.
		if (StrongpointPost.WorkCircuit.bEnabled)
		{
			return true;
		}
		return not StrongpointPost.UnitOptions.IsEmpty()
			&& StrongpointPost.UnitOptions[0].Kind == EEnemyPlacementUnitKind::Squad;
	}
}

AEnemyStrongpoint::AEnemyStrongpoint()
{
	FEnemyStrongpointPost DefaultPost;
	DefaultPost.Post.SetLocation(EnemyStrongpointConstants::DefaultPostLocation);
	Posts = {DefaultPost};
#if WITH_EDITORONLY_DATA
	struct FConstructorStatics
	{
		ConstructorHelpers::FObjectFinderOptional<UTexture2D> StrongpointSprite;

		FConstructorStatics()
			: StrongpointSprite(TEXT("/Engine/EditorResources/S_Player"))
		{
		}
	};
	static FConstructorStatics ConstructorStatics;
	M_StrongpointSprite = ConstructorStatics.StrongpointSprite.Get();
#endif
}

void AEnemyStrongpoint::BeginPlay()
{
	Super::BeginPlay();
	if (Timing.bSpawnOnBeginPlay)
	{
		SpawnGarrison();
	}
}

void AEnemyStrongpoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_WatchTimerHandle);
	}
	StopWorkCircuits();
	RemoveMinimapIcon();
	Super::EndPlay(EndPlayReason);
}

AEnemyStrongpoint* AEnemyStrongpoint::FindEnemyStrongpoint(const UObject* WorldContextObject,
                                                           const FName StrongpointIdToFind)
{
	const UWorld* World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	if (not IsValid(World) || StrongpointIdToFind.IsNone())
	{
		RTSFunctionLibrary::ReportError("FindEnemyStrongpoint needs a valid world context and a strongpoint ID.");
		return nullptr;
	}
	for (TActorIterator<AEnemyStrongpoint> It(World); It; ++It)
	{
		if (It->StrongpointId == StrongpointIdToFind)
		{
			return *It;
		}
	}
	RTSFunctionLibrary::ReportError("FindEnemyStrongpoint found no strongpoint with ID: " +
		StrongpointIdToFind.ToString());
	return nullptr;
}

// ------------------------------------------------------------------
// Spawning
// ------------------------------------------------------------------

void AEnemyStrongpoint::SpawnGarrison()
{
	if (bM_HasStartedSpawning)
	{
		return;
	}
	bM_HasStartedSpawning = true;
	WaitForEnemyAI(Timing.SpawnDelay);
}

void AEnemyStrongpoint::OnEnemyAIReady(AEnemyController* EnemyController, const ERTSGameDifficulty Difficulty)
{
	M_EnemyController = EnemyController;
	AddTrenchBaseOnce();
	SpawnPostsForDifficulty(Difficulty);
	if (M_Defenders.IsEmpty() && M_PendingSpawns == 0)
	{
		// Allowed: a strongpoint may only be manned on higher difficulties.
		UE_LOG(LogTemp, Log, TEXT("Strongpoint %s has no defenders on this difficulty."), *GetName());
	}
	if (M_PendingSpawns == 0)
	{
		FinishSpawningGarrison();
	}
}

bool AEnemyStrongpoint::GetIsValidEnemyController() const
{
	if (M_EnemyController.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "M_EnemyController",
	                                                      "AEnemyStrongpoint::GetIsValidEnemyController", this);
	return false;
}

void AEnemyStrongpoint::AddTrenchBaseOnce()
{
	if (not Trenches.bDigTrenches || bM_HasAddedTrenchBase || not GetIsValidEnemyController())
	{
		return;
	}
	UEnemyBaseTrenchComponent* BaseTrenches = M_EnemyController->GetEnemyBaseTrenchComponent();
	if (not IsValid(BaseTrenches))
	{
		RTSFunctionLibrary::ReportError("Strongpoint " + GetName() + " wants trenches but the enemy controller has no "
			"base trench component.");
		return;
	}
	bM_HasAddedTrenchBase = true;
	FVector2D ThreatDirection = FVector2D::ZeroVector;
	if (Trenches.bFaceThreatPoint)
	{
		const FVector ToThreat = GetActorTransform().TransformPosition(Trenches.ThreatPoint) - GetActorLocation();
		ThreatDirection = FVector2D(ToThreat.X, ToThreat.Y).GetSafeNormal();
	}
	BaseTrenches->AddFortifiedBase(GetActorLocation(), Trenches.PerimeterRadius, ThreatDirection);
	// The digging normally starts with the strategic AI's thinking, which a mission may never start.
	BaseTrenches->StartBaseTrenchDigging();
}

void AEnemyStrongpoint::SpawnPostsForDifficulty(const ERTSGameDifficulty Difficulty)
{
	TArray<int32> MannedPosts;
	for (int32 PostIndex = 0; PostIndex < Posts.Num(); ++PostIndex)
	{
		if (GetIsPostManned(PostIndex, Difficulty))
		{
			MannedPosts.Add(PostIndex);
		}
	}
	// Counted up front: a loaded class spawns inside the request and its callback lowers the count right away.
	M_PendingSpawns = MannedPosts.Num();
	bM_IsIssuingSpawns = true;
	for (const int32 PostIndex : MannedPosts)
	{
		if (not SpawnPostDefender(PostIndex))
		{
			M_PendingSpawns = FMath::Max(M_PendingSpawns - 1, 0);
		}
	}
	bM_IsIssuingSpawns = false;
}

bool AEnemyStrongpoint::GetIsPostManned(const int32 PostIndex, const ERTSGameDifficulty Difficulty) const
{
	if (not bM_HasPostUnitOverrides)
	{
		return EnemyPlacement::GetIsDifficultyIncluded(Difficulty, Posts[PostIndex].FromDifficulty);
	}
	// The mission's list is already the one for this difficulty.
	const FEnemyStrongpointPostUnits* Override = FindPostUnitOverride(Posts[PostIndex].PostId);
	return Override != nullptr && not Override->UnitOptions.IsEmpty();
}

TArray<FEnemyPlacementUnit> AEnemyStrongpoint::GetPostUnitOptions(const int32 PostIndex) const
{
	if (not bM_HasPostUnitOverrides)
	{
		return Posts[PostIndex].UnitOptions;
	}
	const FEnemyStrongpointPostUnits* Override = FindPostUnitOverride(Posts[PostIndex].PostId);
	return Override != nullptr ? Override->UnitOptions : TArray<FEnemyPlacementUnit>();
}

const FEnemyStrongpointPostUnits* AEnemyStrongpoint::FindPostUnitOverride(const FName PostId) const
{
	if (PostId.IsNone())
	{
		return nullptr;
	}
	return M_PostUnitOverrides.FindByPredicate([PostId](const FEnemyStrongpointPostUnits& Override)
	{
		return Override.PostId == PostId;
	});
}

void AEnemyStrongpoint::SetPostUnitOverrides(const TArray<FEnemyStrongpointPostUnits>& Overrides)
{
	if (bM_HasStartedSpawning)
	{
		RTSFunctionLibrary::ReportError("Strongpoint " + GetName() + ": SetPostUnitOverrides came after the garrison "
			"started spawning; turn Timing.bSpawnOnBeginPlay off and call SpawnGarrison after it.");
		return;
	}
	M_PostUnitOverrides = Overrides;
	bM_HasPostUnitOverrides = true;
	for (const FEnemyStrongpointPostUnits& Override : Overrides)
	{
		const bool bHasPost = Posts.ContainsByPredicate([&Override](const FEnemyStrongpointPost& StrongpointPost)
		{
			return StrongpointPost.PostId == Override.PostId;
		});
		if (not bHasPost)
		{
			RTSFunctionLibrary::ReportError("Strongpoint " + GetName() + " has no post with PostId " +
				Override.PostId.ToString() + "; its units are not spawned.");
		}
	}
}

bool AEnemyStrongpoint::SpawnPostDefender(const int32 PostIndex)
{
	const FEnemyStrongpointPost& StrongpointPost = Posts[PostIndex];
	const TArray<FTrainingOption> Options = EnemyPlacement::ToTrainingOptions(GetPostUnitOptions(PostIndex));
	ARTSAsyncSpawner* AsyncSpawner = FRTS_Statics::GetAsyncSpawner(this);
	if (Options.IsEmpty() || not IsValid(AsyncSpawner))
	{
		RTSFunctionLibrary::ReportError("Strongpoint " + GetName() + " cannot man post " + FString::FromInt(PostIndex) +
			(Options.IsEmpty() ? TEXT(": no unit picked.") : TEXT(": no async spawner.")));
		return false;
	}
	const FTrainingOption Option = Options[FMath::RandRange(0, Options.Num() - 1)];
	const FVector Location = EnemyPlacement::ProjectToNavigationIfPossible(GetWorld(),
	                                                                       GetPostWorldLocation(StrongpointPost));
	const FRotator Rotation(0.f, GetPostWorldYaw(StrongpointPost), 0.f);
	TWeakObjectPtr<AEnemyStrongpoint> WeakThis(this);
	const TSharedRef<bool> bResolved = MakeShared<bool>(false);
	auto OnSpawned = [WeakThis, bResolved](const FTrainingOption&, AActor* SpawnedUnit, const int32 SpawnedPostIndex)
	{
		// One answer per request, as the wave controller guards the same spawner call.
		if (*bResolved)
		{
			return;
		}
		*bResolved = true;
		if (WeakThis.IsValid())
		{
			WeakThis->HandleDefenderSpawned(SpawnedPostIndex, SpawnedUnit);
		}
	};
	// The garrison belongs to the player of the enemy controller this strongpoint runs on.
	const int32 OwningPlayerOverride = M_EnemyController.IsValid()
		                                   ? M_EnemyController->GetSpawnOwnerOverride()
		                                   : INDEX_NONE;
	const bool bStarted = AsyncSpawner->AsyncSpawnOptionAtLocation(Option, Location, this, PostIndex, OnSpawned,
	                                                               Rotation, OwningPlayerOverride);
	// A request that failed before loading still counts as started when its callback already ran.
	return bStarted || *bResolved;
}

void AEnemyStrongpoint::HandleDefenderSpawned(const int32 PostIndex, AActor* SpawnedUnit)
{
	M_PendingSpawns = FMath::Max(M_PendingSpawns - 1, 0);
	if (IsValid(SpawnedUnit) && Posts.IsValidIndex(PostIndex))
	{
		if (M_EnemyController.IsValid())
		{
			M_EnemyController->LogSpawnedUnitOwnerMismatch(SpawnedUnit);
		}
		FEnemyStrongpointDefender& Defender = M_Defenders.AddDefaulted_GetRef();
		Defender.Unit = SpawnedUnit;
		Defender.PostIndex = PostIndex;
		Defender.bDigsIn = Posts[PostIndex].bDigIn;
		if (ASquadController* Squad = Cast<ASquadController>(SpawnedUnit))
		{
			Squad->SetAutomaticCoverBlocked(not Posts[PostIndex].bTakeNearbyCover);
		}
		if (Defender.bDigsIn)
		{
			ScheduleDigIn(SpawnedUnit);
		}
		StartWorkCircuit(PostIndex, SpawnedUnit);
	}
	if (M_PendingSpawns == 0 && not bM_IsIssuingSpawns)
	{
		FinishSpawningGarrison();
	}
}

void AEnemyStrongpoint::ScheduleDigIn(AActor* Defender)
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	TWeakObjectPtr<AActor> WeakDefender(Defender);
	FTimerHandle DigInTimerHandle;
	World->GetTimerManager().SetTimer(DigInTimerHandle, FTimerDelegate::CreateLambda([WeakDefender]()
	{
		ICommands* Commands = Cast<ICommands>(WeakDefender.Get());
		if (Commands == nullptr || not RTSFunctionLibrary::RTSIsValid(WeakDefender.Get()))
		{
			return;
		}
		// Units without the dig-in ability answer with an error; their post simply has no pit.
		const ECommandQueueError Error = Commands->DigIn(true);
		UE_LOG(LogTemp, Verbose, TEXT("Strongpoint defender %s dig-in: %d"), *WeakDefender->GetName(),
		       static_cast<int32>(Error));
	}), FMath::Max(Timing.DigInDelay, KINDA_SMALL_NUMBER), false);
}

void AEnemyStrongpoint::StartWorkCircuit(const int32 PostIndex, AActor* Defender)
{
	const FEnemyStrongpointWorkCircuit& Circuit = Posts[PostIndex].WorkCircuit;
	if (not Circuit.bEnabled || Circuit.Points.IsEmpty() || bM_IsEngaged)
	{
		return;
	}
	ASquadController* Squad = Cast<ASquadController>(Defender);
	if (not IsValid(Squad))
	{
		RTSFunctionLibrary::ReportError("Strongpoint " + GetName() + ": post " + FString::FromInt(PostIndex) +
			" has a work circuit but its defender is not a squad.");
		return;
	}
	TArray<FVector> WorldPoints;
	for (const FVector& Point : Circuit.Points)
	{
		WorldPoints.Add(EnemyPlacement::ProjectToNavigationIfPossible(
			GetWorld(), GetActorTransform().TransformPosition(Point)));
	}
	UEnemyWorkCircuitRunner* WorkCircuit = NewObject<UEnemyWorkCircuitRunner>(this);
	M_WorkCircuits.Add(WorkCircuit);
	WorkCircuit->StartCircuit(Squad, WorldPoints, Circuit.WorkSeconds, Circuit.MaxWalkSeconds,
	                          Circuit.WorkMontage.LoadSynchronous());
}

int32 AEnemyStrongpoint::GetRunningWorkCircuitCount() const
{
	int32 Running = 0;
	for (const UEnemyWorkCircuitRunner* WorkCircuit : M_WorkCircuits)
	{
		Running += IsValid(WorkCircuit) && WorkCircuit->GetIsRunning() ? 1 : 0;
	}
	return Running;
}

int32 AEnemyStrongpoint::GetWorkingCircuitCount() const
{
	int32 Working = 0;
	for (const UEnemyWorkCircuitRunner* WorkCircuit : M_WorkCircuits)
	{
		Working += IsValid(WorkCircuit) && WorkCircuit->GetPhase() == EEnemyWorkCircuitPhase::Working ? 1 : 0;
	}
	return Working;
}

void AEnemyStrongpoint::StopWorkCircuits()
{
	for (UEnemyWorkCircuitRunner* WorkCircuit : M_WorkCircuits)
	{
		if (IsValid(WorkCircuit))
		{
			WorkCircuit->StopCircuit();
		}
	}
	M_WorkCircuits.Empty();
}

void AEnemyStrongpoint::FinishSpawningGarrison()
{
	if (bM_HasSpawnedGarrison)
	{
		return;
	}
	bM_HasSpawnedGarrison = true;
	const TArray<AActor*> Garrison = GetAliveDefenders();
	OnGarrisonSpawned.Broadcast(this, Garrison);
	if (Garrison.IsEmpty())
	{
		ClearStrongpoint();
		return;
	}
	if (Minimap.bShowOnMinimap)
	{
		AddMinimapIcon();
	}
	if (Release.Release == EEnemyStrongpointRelease::OnSpawn)
	{
		ReleaseGarrisonToEnemyAI();
	}
	StartWatchingGarrison();
}

// ------------------------------------------------------------------
// Release and reinforcements
// ------------------------------------------------------------------

void AEnemyStrongpoint::ReleaseGarrisonToEnemyAI()
{
	if (bM_IsReleased || not GetIsValidEnemyController())
	{
		return;
	}
	UEnemyDirectControlComponent* DirectControl = M_EnemyController->GetEnemyDirectControlComponent();
	if (not IsValid(DirectControl))
	{
		RTSFunctionLibrary::ReportError("Strongpoint " + GetName() + " cannot release its garrison: the enemy "
			"controller has no direct control component.");
		return;
	}
	bM_IsReleased = true;
	// The enemy AI moves released units itself; a work circuit would fight its orders.
	StopWorkCircuits();
	TArray<AActor*> ReleasedUnits;
	for (FEnemyStrongpointDefender& Defender : M_Defenders)
	{
		const bool bStaysInPit = Defender.bDigsIn && not Release.bReleaseDugInDefenders;
		if (bStaysInPit || not RTSFunctionLibrary::RTSIsValid(Defender.Unit.Get()))
		{
			continue;
		}
		Defender.bIsReleased = true;
		ReleasedUnits.Add(Defender.Unit.Get());
	}
	DirectControl->RegisterDirectControlUnits(ReleasedUnits);
	OnGarrisonReleased.Broadcast(this, ReleasedUnits);
}

void AEnemyStrongpoint::CallReinforcements()
{
	if (bM_HasCalledReinforcements)
	{
		return;
	}
	bM_HasCalledReinforcements = true;
	for (AEnemyWaveSpawner* Spawner : Reinforcements.Spawners)
	{
		if (IsValid(Spawner))
		{
			Spawner->StartSpawner();
		}
	}
	OnReinforcementsCalled.Broadcast(this);
}

// ------------------------------------------------------------------
// Watching losses
// ------------------------------------------------------------------

int32 AEnemyStrongpoint::GetAliveDefenderCount() const
{
	return GetAliveDefenders().Num();
}

TArray<AActor*> AEnemyStrongpoint::GetAliveDefenders() const
{
	TArray<AActor*> AliveDefenders;
	for (const FEnemyStrongpointDefender& Defender : M_Defenders)
	{
		AActor* Unit = Defender.Unit.Get();
		if (RTSFunctionLibrary::RTSIsValid(Unit))
		{
			AliveDefenders.Add(Unit);
		}
	}
	return AliveDefenders;
}

void AEnemyStrongpoint::StartWatchingGarrison()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	TWeakObjectPtr<AEnemyStrongpoint> WeakThis(this);
	World->GetTimerManager().SetTimer(M_WatchTimerHandle, FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->WatchGarrison();
		}
	}), EnemyStrongpointConstants::WatchIntervalSeconds, true);
}

void AEnemyStrongpoint::WatchGarrison()
{
	const int32 AliveDefenders = GetAliveDefenderCount();
	if (AliveDefenders == 0)
	{
		ClearStrongpoint();
		return;
	}
	CheckGarrisonEngaged();
	const float LossFraction = GetLossFraction(AliveDefenders);
	if (not Reinforcements.Spawners.IsEmpty() && LossFraction >= Reinforcements.LossFraction)
	{
		CallReinforcements();
	}
	if (Release.Release == EEnemyStrongpointRelease::OnLosses && LossFraction >= Release.LossFraction)
	{
		ReleaseGarrisonToEnemyAI();
	}
}

void AEnemyStrongpoint::CheckGarrisonEngaged()
{
	if (bM_IsEngaged)
	{
		return;
	}
	const TArray<AActor*> AliveDefenders = GetAliveDefenders();
	bool bIsEngagedNow = AliveDefenders.Num() < M_Defenders.Num();
	for (AActor* Defender : AliveDefenders)
	{
		if (bIsEngagedNow)
		{
			break;
		}
		const ICommands* Commands = Cast<ICommands>(Defender);
		bIsEngagedNow = Commands != nullptr && Commands->GetIsUnitInCombat();
	}
	if (not bIsEngagedNow)
	{
		return;
	}
	bM_IsEngaged = true;
	StopWorkCircuits();
	OnGarrisonEngaged.Broadcast(this);
}

float AEnemyStrongpoint::GetLossFraction(const int32 AliveDefenders) const
{
	if (M_Defenders.IsEmpty())
	{
		return 0.f;
	}
	return 1.f - static_cast<float>(AliveDefenders) / M_Defenders.Num();
}

void AEnemyStrongpoint::ClearStrongpoint()
{
	if (bM_IsCleared)
	{
		return;
	}
	bM_IsCleared = true;
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_WatchTimerHandle);
	}
	StopWorkCircuits();
	RemoveMinimapIcon();
	OnStrongpointCleared.Broadcast(this);
}

void AEnemyStrongpoint::AddMinimapIcon()
{
	const FString IdBase = StrongpointId.IsNone() ? GetName() : StrongpointId.ToString();
	M_MinimapIconId = URTSBlueprintFunctionLibrary::BP_AddCustomMiniMapIcon(
		this, FName(*(TEXT("Strongpoint_") + IdBase)), Minimap.Icon, GetActorLocation(), FRotator::ZeroRotator,
		FMinimapIconTextPayload());
}

void AEnemyStrongpoint::RemoveMinimapIcon()
{
	if (M_MinimapIconId.IsNone())
	{
		return;
	}
	URTSBlueprintFunctionLibrary::BP_RemoveCustomMiniMapIcon(this, M_MinimapIconId);
	M_MinimapIconId = NAME_None;
}

// ------------------------------------------------------------------
// Editor
// ------------------------------------------------------------------

FColor AEnemyStrongpoint::GetPlacementColor() const
{
	return EnemyPlacementColors::Strongpoint;
}

UTexture2D* AEnemyStrongpoint::GetEditorSpriteTexture() const
{
#if WITH_EDITORONLY_DATA
	return M_StrongpointSprite.Get();
#else
	return nullptr;
#endif
}

FString AEnemyStrongpoint::GetConfigurationProblem() const
{
	if (Posts.IsEmpty())
	{
		return TEXT("no posts");
	}
	if (GetHasUnsetPost())
	{
		return bUnitsSetByMission ? TEXT("post with no PostId") : TEXT("post with no unit picked");
	}
	if (bUnitsSetByMission && Timing.bSpawnOnBeginPlay)
	{
		return TEXT("units set by mission but spawns on BeginPlay");
	}
	return GetReinforcementProblem();
}

bool AEnemyStrongpoint::GetHasUnsetPost() const
{
	for (const FEnemyStrongpointPost& StrongpointPost : Posts)
	{
		if (bUnitsSetByMission)
		{
			if (StrongpointPost.PostId.IsNone())
			{
				return true;
			}
			continue;
		}
		if (not EnemyPlacement::GetAreUnitsSet(StrongpointPost.UnitOptions))
		{
			return true;
		}
	}
	return false;
}

FString AEnemyStrongpoint::GetReinforcementProblem() const
{
	for (const AEnemyWaveSpawner* Spawner : Reinforcements.Spawners)
	{
		if (Spawner == nullptr)
		{
			return TEXT("empty reinforcement slot");
		}
		if (Spawner->GetStartsOnBeginPlay())
		{
			return TEXT("reinforcement ") + Spawner->GetSpawnerId().ToString() + TEXT(" also starts on BeginPlay");
		}
	}
	return FString();
}

FVector AEnemyStrongpoint::GetPostWorldLocation(const FEnemyStrongpointPost& StrongpointPost) const
{
	return GetActorTransform().TransformPosition(StrongpointPost.Post.GetLocation());
}

float AEnemyStrongpoint::GetPostWorldYaw(const FEnemyStrongpointPost& StrongpointPost) const
{
	return (StrongpointPost.Post * GetActorTransform()).Rotator().Yaw;
}

void AEnemyStrongpoint::BuildEditorShapes(FEnemyPlacementShapes& OutSummary, FEnemyPlacementShapes& OutDetail) const
{
	AddPostShapes(OutSummary, OutDetail);
	AddTrenchShapes(OutDetail);
	AddReinforcementLinkShapes(OutSummary);
}

void AEnemyStrongpoint::AddPostShapes(FEnemyPlacementShapes& OutSummary, FEnemyPlacementShapes& OutDetail) const
{
	using namespace EnemyStrongpointConstants;
	const FColor Color = GetPlacementColor();
	const FVector Centre = GetActorLocation();
	float OutlineRadius = MinimumOutlineRadius;
	for (const FEnemyStrongpointPost& StrongpointPost : Posts)
	{
		const FVector Location = LiftPostPoint(GetPostWorldLocation(StrongpointPost));
		const float Yaw = GetPostWorldYaw(StrongpointPost);
		// Filled: manned on every difficulty. Outline: only from a harder difficulty up.
		const bool bAlwaysManned = StrongpointPost.FromDifficulty == ERTSGameDifficulty::NewToRTS;
		if (GetIsSquadPost(StrongpointPost))
		{
			OutSummary.Spheres.Emplace(SquadPostRadius, Location, FLinearColor(Color),
			                           bAlwaysManned ? FDebugRenderSceneProxy::EDrawType::SolidAndWireMeshes
			                                         : FDebugRenderSceneProxy::EDrawType::WireMesh);
		}
		else
		{
			OutSummary.AddFlatBox(Location, VehiclePostHalfExtent, Yaw, Color, bAlwaysManned);
		}
		OutDetail.ArrowLines.Emplace(Location, Location + FRotator(0.f, Yaw, 0.f).Vector() * FacingArrowLength,
		                             EnemyPlacementColors::Facing);
		OutDetail.Lines.Emplace(LiftPostPoint(Centre), Location, Color, CentreLinkThickness);
		if (StrongpointPost.bDigIn)
		{
			OutDetail.AddFlatBox(GetPostWorldLocation(StrongpointPost), DigInPitHalfExtent, Yaw, Color, false);
		}
		OutlineRadius = FMath::Max(OutlineRadius, FVector::Dist2D(Centre, Location) + OutlineMargin);
		AddWorkCircuitShapes(StrongpointPost, OutDetail);
	}
	// The closed ring is the strongpoint's shape language: it holds an area instead of moving through it.
	for (int32 Segment = 0; Segment < OutlineSegments; ++Segment)
	{
		const float StartYaw = 360.f * Segment / OutlineSegments;
		const float EndYaw = 360.f * (Segment + 1) / OutlineSegments;
		OutSummary.DashedLines.Emplace(LiftPostPoint(Centre + FRotator(0.f, StartYaw, 0.f).Vector() * OutlineRadius),
		                               LiftPostPoint(Centre + FRotator(0.f, EndYaw, 0.f).Vector() * OutlineRadius),
		                               Color, OutlineDashSize);
	}
}

void AEnemyStrongpoint::AddWorkCircuitShapes(const FEnemyStrongpointPost& StrongpointPost,
                                             FEnemyPlacementShapes& OutDetail) const
{
	using namespace EnemyStrongpointConstants;
	const FEnemyStrongpointWorkCircuit& Circuit = StrongpointPost.WorkCircuit;
	if (not Circuit.bEnabled || Circuit.Points.IsEmpty())
	{
		return;
	}
	const FColor CircuitColor = EnemyPlacementColors::GetDimmed(GetPlacementColor(), WorkCircuitBrightness);
	FVector Previous = LiftPostPoint(GetPostWorldLocation(StrongpointPost));
	for (const FVector& Point : Circuit.Points)
	{
		const FVector WorldPoint = LiftPostPoint(GetActorTransform().TransformPosition(Point));
		OutDetail.Spheres.Emplace(WorkPointRadius, WorldPoint, FLinearColor(CircuitColor),
		                          FDebugRenderSceneProxy::EDrawType::WireMesh);
		OutDetail.DashedLines.Emplace(Previous, WorldPoint, CircuitColor, WorkCircuitDashSize);
		Previous = WorldPoint;
	}
	// Closes the loop: the squad starts again at the first point.
	OutDetail.DashedLines.Emplace(Previous, LiftPostPoint(GetActorTransform().TransformPosition(Circuit.Points[0])),
	                              CircuitColor, WorkCircuitDashSize);
}

void AEnemyStrongpoint::AddTrenchShapes(FEnemyPlacementShapes& OutDetail) const
{
	using namespace EnemyStrongpointConstants;
	if (not Trenches.bDigTrenches)
	{
		return;
	}
	const FColor TrenchColor = EnemyPlacementColors::GetDimmed(GetPlacementColor(), TrenchBrightness);
	const FVector Centre = LiftPostPoint(GetActorLocation());
	OutDetail.AddCircle(Centre, Trenches.PerimeterRadius, TrenchColor, TrenchRingThickness);
	if (not Trenches.bFaceThreatPoint)
	{
		return;
	}
	const FVector ThreatWorld = LiftPostPoint(GetActorTransform().TransformPosition(Trenches.ThreatPoint));
	OutDetail.AddArc(Centre, Trenches.PerimeterRadius, ThreatWorld - Centre, GetTrenchArcDegrees(), TrenchColor,
	                 TrenchArcThickness);
	OutDetail.ArrowLines.Emplace(Centre, ThreatWorld, TrenchColor);
}

void AEnemyStrongpoint::AddReinforcementLinkShapes(FEnemyPlacementShapes& OutSummary) const
{
	using namespace EnemyStrongpointConstants;
	for (const AEnemyWaveSpawner* Spawner : Reinforcements.Spawners)
	{
		if (not IsValid(Spawner))
		{
			continue;
		}
		// In the spawner's own colour, so it reads as "that attack/patrol wave comes to help here".
		OutSummary.DashedLines.Emplace(LiftPostPoint(Spawner->GetActorLocation()), LiftPostPoint(GetActorLocation()),
		                               Spawner->GetPlacementColor(), ReinforcementDashSize);
	}
}

float AEnemyStrongpoint::GetTrenchArcDegrees() const
{
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return EnemyStrongpointConstants::FallbackTrenchArcDegrees;
	}
	// The arc of the controller this strongpoint runs on; in a map with one controller, that one's.
	const UEnemyBaseTrenchComponent* FirstBaseTrenches = nullptr;
	for (TActorIterator<AEnemyController> It(World); It; ++It)
	{
		const UEnemyBaseTrenchComponent* BaseTrenches = It->GetEnemyBaseTrenchComponent();
		if (not IsValid(BaseTrenches))
		{
			continue;
		}
		if (It->GetControlledPlayer() == GetEnemyPlayer())
		{
			return BaseTrenches->GetSettings().PerimeterArcDegrees;
		}
		if (FirstBaseTrenches == nullptr)
		{
			FirstBaseTrenches = BaseTrenches;
		}
	}
	if (FirstBaseTrenches != nullptr)
	{
		return FirstBaseTrenches->GetSettings().PerimeterArcDegrees;
	}
	return EnemyStrongpointConstants::FallbackTrenchArcDegrees;
}
