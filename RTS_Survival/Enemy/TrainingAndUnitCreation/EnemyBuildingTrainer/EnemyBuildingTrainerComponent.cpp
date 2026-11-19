// Copyright (C) Bas Blokzijl - All rights reserved.

#include "EnemyBuildingTrainerComponent.h"

#include "Animation/AnimSingleNodeInstance.h"
#include "Components/AudioComponent.h"
#include "Components/LightComponent.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "NavigationSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "TimerManager.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Enemy/EnemyAISettings/EnemyAISettings.h"
#include "RTS_Survival/Enemy/EnemyController/EnemyController.h"
#include "RTS_Survival/Enemy/StrategicAI/Component/EnemyStrategicAIComponent.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorldSubsystem.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"

// One line per queued/started/finished/cancelled unit: how a real match shows where and when the AI trains.
DEFINE_LOG_CATEGORY_STATIC(LogEnemyBuildingTrainer, Log, All);

namespace EnemyBuildingTrainerPrivate
{
	// While idle the tagged components are put back to the idle look this often, so a Blueprint that shows its
	// machinery again after construction or pack-up cannot leave the lights on or the cranes running.
	constexpr float OwnerCheckIntervalSeconds = 1.f;
	// The async spawner normally answers within a second or two; past this the job is closed without a unit.
	constexpr float SpawnWatchdogSeconds = 20.f;
	// Exit points sit next to the building; the first projection stays close, the second finds any nearby navmesh.
	const FVector NearExitProjectionExtent(500.f, 500.f, 1000.f);
	const FVector WideExitProjectionExtent(2500.f, 2500.f, 2000.f);
	constexpr float MinimumTrainingSeconds = 1.f;
}

UEnemyBuildingTrainerComponent::UEnemyBuildingTrainerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnemyBuildingTrainerComponent::BeginPlay()
{
	Super::BeginPlay();
	// Idle look from the first frame: cranes rest, lights off, glow 0.
	ApplyActivityToComponents(false, false, true);
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	World->GetTimerManager().SetTimer(M_OwnerCheckTimer, this, &UEnemyBuildingTrainerComponent::OnOwnerCheck,
	                                  EnemyBuildingTrainerPrivate::OwnerCheckIntervalSeconds, true);
	UpdateCoverKeepOutZone();
}

void UEnemyBuildingTrainerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelAllTraining();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_OwnerCheckTimer);
		World->GetTimerManager().ClearTimer(M_FinishCycleTimer);
	}
	RemoveCoverKeepOutZone();
	Super::EndPlay(EndPlayReason);
}

float UEnemyBuildingTrainerComponent::GetTrainingSecondsForCost(const int32 TrainingPointCost)
{
	using namespace EnemyAISettings::UnitTraining;
	const float Seconds = BuildingTrainingBaseSeconds + BuildingTrainingSecondsPerPoint * FMath::Max(0, TrainingPointCost);
	return FMath::Clamp(Seconds, EnemyBuildingTrainerPrivate::MinimumTrainingSeconds, BuildingTrainingMaxSeconds);
}

bool UEnemyBuildingTrainerComponent::GetCanTrainNow() const
{
	AActor* Owner = GetOwner();
	if (not RTSFunctionLibrary::RTSIsValid(Owner) || Owner->IsActorBeingDestroyed())
	{
		return false;
	}
	if (const ABuildingExpansion* BuildingExpansion = Cast<ABuildingExpansion>(Owner))
	{
		return BuildingExpansion->GetIsStanding();
	}
	return true;
}

bool UEnemyBuildingTrainerComponent::GetCanTrainOption(const FTrainingOption& Option) const
{
	if (not GetCanTrainNow())
	{
		return false;
	}
	switch (TrainerKind)
	{
	case EEnemyBuildingTrainerKind::Infantry:
		return Option.UnitType == EAllUnitType::UNType_Squad;
	case EEnemyBuildingTrainerKind::Vehicles:
		return Option.UnitType == EAllUnitType::UNType_Tank;
	default:
		return false;
	}
}

float UEnemyBuildingTrainerComponent::GetQueuedSecondsRemaining() const
{
	const UWorld* World = GetWorld();
	const float Now = IsValid(World) ? World->GetTimeSeconds() : 0.f;
	float Seconds = 0.f;
	for (const FEnemyBuildingTrainingJob& Job : M_Queue)
	{
		if (Job.StartWorldSeconds < 0.f)
		{
			Seconds += Job.DurationSeconds;
			continue;
		}
		Seconds += FMath::Max(0.f, Job.StartWorldSeconds + Job.DurationSeconds - Now);
	}
	return Seconds;
}

float UEnemyBuildingTrainerComponent::GetActiveTrainingProgress() const
{
	if (M_Queue.IsEmpty())
	{
		return 0.f;
	}
	const FEnemyBuildingTrainingJob& Job = M_Queue[0];
	if (Job.bIsSpawning)
	{
		return 1.f;
	}
	const UWorld* World = GetWorld();
	if (Job.StartWorldSeconds < 0.f || not IsValid(World) || Job.DurationSeconds <= 0.f)
	{
		return 0.f;
	}
	return FMath::Clamp((World->GetTimeSeconds() - Job.StartWorldSeconds) / Job.DurationSeconds, 0.f, 1.f);
}

FTrainingOption UEnemyBuildingTrainerComponent::GetActiveTrainingOption() const
{
	return M_Queue.IsEmpty() ? FTrainingOption() : M_Queue[0].Option;
}

bool UEnemyBuildingTrainerComponent::EnqueueTraining(const FTrainingOption& Option, const int32 TrainingPointCost,
                                                     UEnemyStrategicAIComponent* RequestingStrategicAI)
{
	if (not GetCanTrainOption(Option))
	{
		return false;
	}
	FEnemyBuildingTrainingJob& Job = M_Queue.AddDefaulted_GetRef();
	Job.Option = Option;
	Job.TrainingPointCost = TrainingPointCost;
	Job.DurationSeconds = GetTrainingSecondsForCost(TrainingPointCost) * FMath::Max(0.05f, TrainingTimeMlt);
	Job.JobID = M_NextJobID++;
	Job.RequestingStrategicAI = RequestingStrategicAI;
	Broadcast(EEnemyBuildingTrainingEvent::Queued, Option, nullptr, Job.DurationSeconds);
	if (M_Queue.Num() == 1)
	{
		SetActivity(true);
		StartNextJob();
	}
	return true;
}

void UEnemyBuildingTrainerComponent::StartNextJob()
{
	UWorld* World = GetWorld();
	if (M_Queue.IsEmpty() || not IsValid(World))
	{
		return;
	}
	FEnemyBuildingTrainingJob& Job = M_Queue[0];
	Job.StartWorldSeconds = World->GetTimeSeconds();
	Job.bIsSpawning = false;
	World->GetTimerManager().SetTimer(M_ActiveJobTimer, this,
	                                  &UEnemyBuildingTrainerComponent::OnActiveJobTrainingTimeDone,
	                                  FMath::Max(EnemyBuildingTrainerPrivate::MinimumTrainingSeconds,
	                                             Job.DurationSeconds), false);
	const FTrainingOption Option = Job.Option;
	const float Duration = Job.DurationSeconds;
	Broadcast(EEnemyBuildingTrainingEvent::Started, Option, nullptr, Duration);
	OnTrainingStarted.Broadcast(Option, Duration);
}

void UEnemyBuildingTrainerComponent::OnActiveJobTrainingTimeDone()
{
	if (M_Queue.IsEmpty())
	{
		return;
	}
	if (not GetCanTrainNow())
	{
		CancelAllTraining();
		return;
	}
	FEnemyBuildingTrainingJob& Job = M_Queue[0];
	const FTransform ExitTransform = GetExitTransform();
	FVector SpawnLocation = FVector::ZeroVector;
	ARTSAsyncSpawner* AsyncSpawner = FRTS_Statics::GetAsyncSpawner(this);
	if (not IsValid(AsyncSpawner) || not TryGetNavigableExitLocation(ExitTransform.GetLocation(), SpawnLocation))
	{
		RTSFunctionLibrary::ReportError(
			"Enemy building trainer " + GetOwnerNameForLog() + " found no async spawner or no navigable exit for " +
			Job.Option.GetTrainingName() + "; the unit is dropped and its points refunded.");
		if (UEnemyStrategicAIComponent* StrategicAI = Job.RequestingStrategicAI.Get())
		{
			StrategicAI->RefundTrainingPoints(Job.TrainingPointCost);
		}
		FinishActiveJob(nullptr);
		return;
	}

	Job.bIsSpawning = true;
	const int32 JobID = Job.JobID;
	const FTrainingOption Option = Job.Option;
	const TWeakObjectPtr<UEnemyBuildingTrainerComponent> WeakThis(this);
	const TWeakObjectPtr<UEnemyStrategicAIComponent> WeakStrategicAI = Job.RequestingStrategicAI;
	// The strategic AI owns the callback: a building destroyed while the unit loads still hands the unit to its AI.
	TWeakObjectPtr<UObject> CallbackOwner = WeakStrategicAI.IsValid()
		                                        ? TWeakObjectPtr<UObject>(WeakStrategicAI.Get())
		                                        : TWeakObjectPtr<UObject>(this);
	const bool bRequestStarted = AsyncSpawner->AsyncSpawnOptionAtLocation(
		Option, SpawnLocation, CallbackOwner, JobID,
		[WeakThis, WeakStrategicAI, JobID, SpawnLocation](const FTrainingOption& SpawnedOption, AActor* SpawnedActor,
		                                                   const int32 /*ID*/)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnActiveJobSpawned(JobID, SpawnedOption, SpawnedActor, SpawnLocation);
				return;
			}
			if (WeakStrategicAI.IsValid())
			{
				WeakStrategicAI->OnBuildingTrainerUnitSpawned(SpawnedOption, SpawnedActor, SpawnLocation);
			}
		}, FRotator(0.f, ExitTransform.Rotator().Yaw, 0.f), GetSpawnOwnerOverride());

	if (not bRequestStarted)
	{
		RTSFunctionLibrary::ReportError(
			"Enemy building trainer " + GetOwnerNameForLog() + " could not start the async spawn of " +
			Option.GetTrainingName() + "; points refunded.");
		if (UEnemyStrategicAIComponent* StrategicAI = WeakStrategicAI.Get())
		{
			StrategicAI->RefundTrainingPoints(M_Queue[0].TrainingPointCost);
		}
		FinishActiveJob(nullptr);
		return;
	}

	UWorld* World = GetWorld();
	if (IsValid(World))
	{
		FTimerDelegate WatchdogDelegate;
		WatchdogDelegate.BindUObject(this, &UEnemyBuildingTrainerComponent::OnSpawnWatchdog, JobID);
		World->GetTimerManager().SetTimer(M_SpawnWatchdogTimer, WatchdogDelegate,
		                                  EnemyBuildingTrainerPrivate::SpawnWatchdogSeconds, false);
	}
}

void UEnemyBuildingTrainerComponent::OnActiveJobSpawned(const int32 JobID, const FTrainingOption& Option,
                                                        AActor* SpawnedActor, const FVector& ExitLocation)
{
	const bool bIsActiveJob = not M_Queue.IsEmpty() && M_Queue[0].JobID == JobID;
	UEnemyStrategicAIComponent* StrategicAI = bIsActiveJob ? M_Queue[0].RequestingStrategicAI.Get() : nullptr;
	if (not bIsActiveJob)
	{
		// The watchdog already closed the job; the unit still belongs to this building's AI.
		const AEnemyController* EnemyController = FRTS_Statics::GetEnemyControllerOfUnit(GetOwner());
		StrategicAI = IsValid(EnemyController) ? EnemyController->GetEnemyStrategicAIComponent() : nullptr;
	}
	if (IsValid(StrategicAI) && IsValid(SpawnedActor))
	{
		StrategicAI->OnBuildingTrainerUnitSpawned(Option, SpawnedActor, ExitLocation, this);
	}
	if (bIsActiveJob)
	{
		FinishActiveJob(SpawnedActor);
	}
}

void UEnemyBuildingTrainerComponent::OnSpawnWatchdog(const int32 JobID)
{
	if (M_Queue.IsEmpty() || M_Queue[0].JobID != JobID || not M_Queue[0].bIsSpawning)
	{
		return;
	}
	RTSFunctionLibrary::ReportError(
		"Enemy building trainer " + GetOwnerNameForLog() + " waited " +
		FString::SanitizeFloat(EnemyBuildingTrainerPrivate::SpawnWatchdogSeconds) + " s for the async spawn of " +
		M_Queue[0].Option.GetTrainingName() + "; the building moves on to its next unit.");
	FinishActiveJob(nullptr);
}

void UEnemyBuildingTrainerComponent::FinishActiveJob(AActor* TrainedUnit)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_ActiveJobTimer);
		World->GetTimerManager().ClearTimer(M_SpawnWatchdogTimer);
	}
	if (M_Queue.IsEmpty())
	{
		return;
	}
	const FTrainingOption Option = M_Queue[0].Option;
	M_Queue.RemoveAt(0);
	if (IsValid(TrainedUnit))
	{
		Broadcast(EEnemyBuildingTrainingEvent::Finished, Option, TrainedUnit);
		OnTrainingFinished.Broadcast(Option, TrainedUnit);
	}
	else
	{
		Broadcast(EEnemyBuildingTrainingEvent::Cancelled, Option);
		OnTrainingCancelled.Broadcast(Option);
	}
	if (M_Queue.IsEmpty())
	{
		SetActivity(false);
		return;
	}
	StartNextJob();
}

void UEnemyBuildingTrainerComponent::CancelAllTraining()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_ActiveJobTimer);
		World->GetTimerManager().ClearTimer(M_SpawnWatchdogTimer);
	}
	if (M_Queue.IsEmpty())
	{
		return;
	}
	const TArray<FEnemyBuildingTrainingJob> DroppedJobs = MoveTemp(M_Queue);
	M_Queue.Reset();
	for (const FEnemyBuildingTrainingJob& Job : DroppedJobs)
	{
		// The unit in training is lost with its building; units still waiting give their points back.
		const bool bHadStarted = Job.StartWorldSeconds >= 0.f;
		if (not bHadStarted)
		{
			if (UEnemyStrategicAIComponent* StrategicAI = Job.RequestingStrategicAI.Get())
			{
				StrategicAI->RefundTrainingPoints(Job.TrainingPointCost);
			}
		}
		Broadcast(EEnemyBuildingTrainingEvent::Cancelled, Job.Option);
		OnTrainingCancelled.Broadcast(Job.Option);
	}
	SetActivity(false);
}

void UEnemyBuildingTrainerComponent::OnOwnerCheck()
{
	// Follows a building that moved or changed owner.
	UpdateCoverKeepOutZone();
	if (GetIsTraining())
	{
		if (not GetCanTrainNow())
		{
			CancelAllTraining();
		}
		return;
	}
	ApplyActivityToComponents(false, false, false);
}

void UEnemyBuildingTrainerComponent::UpdateCoverKeepOutZone()
{
	using namespace EnemyAISettings::UnitTraining;
	UWorld* const World = GetWorld();
	URTSCoverFinderWorldSubsystem* const CoverFinder = IsValid(World)
		                                                   ? World->GetSubsystem<URTSCoverFinderWorldSubsystem>()
		                                                   : nullptr;
	if (not IsValid(CoverFinder) || not IsValid(GetOwner()))
	{
		return;
	}
	const FTransform Exit = GetExitTransform();
	const FVector Forward = Exit.GetRotation().GetForwardVector().GetSafeNormal2D();
	FRTSCoverKeepOutZone Zone;
	Zone.ZoneId = M_CoverKeepOutZoneId;
	Zone.OwningPlayer = FRTSPlayers::GetOwningPlayerOfActor(GetOwner());
	Zone.Start = FVector2D(Exit.GetLocation().X, Exit.GetLocation().Y);
	Zone.Forward = Forward.IsNearlyZero() ? FVector2D(1.f, 0.f) : FVector2D(Forward.X, Forward.Y);
	Zone.BehindLength = ExitCoverKeepOutBehindCm;
	Zone.AheadLength = StandSpotExitLaneLengthCm;
	Zone.HalfWidth = TrainerKind == EEnemyBuildingTrainerKind::Vehicles
		                 ? StandSpotVehicleExitLaneHalfWidthCm
		                 : StandSpotInfantryExitLaneHalfWidthCm;
	if (M_CoverKeepOutZoneId != 0 && CoverFinder->UpdateCoverKeepOutZone(Zone))
	{
		return;
	}
	M_CoverKeepOutZoneId = CoverFinder->AddCoverKeepOutZone(Zone);
}

void UEnemyBuildingTrainerComponent::RemoveCoverKeepOutZone()
{
	UWorld* const World = GetWorld();
	URTSCoverFinderWorldSubsystem* const CoverFinder = IsValid(World)
		                                                   ? World->GetSubsystem<URTSCoverFinderWorldSubsystem>()
		                                                   : nullptr;
	if (IsValid(CoverFinder) && M_CoverKeepOutZoneId != 0)
	{
		CoverFinder->RemoveCoverKeepOutZone(M_CoverKeepOutZoneId);
	}
	M_CoverKeepOutZoneId = 0;
}

FTransform UEnemyBuildingTrainerComponent::GetExitTransform() const
{
	const AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return FTransform::Identity;
	}
	TInlineComponentArray<USceneComponent*> SceneComponents(Owner);
	for (const USceneComponent* SceneComponent : SceneComponents)
	{
		if (IsValid(SceneComponent) && SceneComponent->GetFName() == ExitPointComponentName)
		{
			return SceneComponent->GetComponentTransform();
		}
	}
	FVector BoundsOrigin = Owner->GetActorLocation();
	FVector BoundsExtent = FVector::ZeroVector;
	Owner->GetActorBounds(true, BoundsOrigin, BoundsExtent);
	const FVector Forward = Owner->GetActorForwardVector().GetSafeNormal2D();
	const float ExtentAlongForward = FMath::Abs(Forward.X) * BoundsExtent.X + FMath::Abs(Forward.Y) * BoundsExtent.Y;
	FVector ExitLocation = BoundsOrigin + Forward * (ExtentAlongForward + FallbackExitDistanceCm);
	ExitLocation.Z = Owner->GetActorLocation().Z;
	return FTransform(Forward.Rotation(), ExitLocation);
}

bool UEnemyBuildingTrainerComponent::TryGetNavigableExitLocation(const FVector& DesiredLocation,
                                                                 FVector& OutLocation) const
{
	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(GetWorld());
	if (not IsValid(NavSystem))
	{
		return false;
	}
	FNavLocation ProjectedLocation;
	if (NavSystem->ProjectPointToNavigation(DesiredLocation, ProjectedLocation,
	                                        EnemyBuildingTrainerPrivate::NearExitProjectionExtent)
		|| NavSystem->ProjectPointToNavigation(DesiredLocation, ProjectedLocation,
		                                       EnemyBuildingTrainerPrivate::WideExitProjectionExtent))
	{
		OutLocation = ProjectedLocation.Location;
		return true;
	}
	return false;
}

int32 UEnemyBuildingTrainerComponent::GetSpawnOwnerOverride() const
{
	return FRTSUnitOwnerSpawning::GetOwnerOverrideForAIPlayer(FRTSPlayers::GetOwningPlayerOfActor(GetOwner()));
}

void UEnemyBuildingTrainerComponent::RefreshActivityState()
{
	ApplyActivityToComponents(GetIsTraining(), false, true);
}

void UEnemyBuildingTrainerComponent::SetActivity(const bool bActive)
{
	if (bM_IsActivityOn == bActive)
	{
		return;
	}
	bM_IsActivityOn = bActive;
	if (bActive)
	{
		if (UWorld* World = GetWorld())
		{
			// Cranes still finishing their last cycle keep running: they stay in step with each other.
			World->GetTimerManager().ClearTimer(M_FinishCycleTimer);
		}
	}
	ApplyActivityToComponents(bActive, true, true);
	Broadcast(bActive ? EEnemyBuildingTrainingEvent::ActivityOn : EEnemyBuildingTrainingEvent::ActivityOff,
	          FTrainingOption());
	OnTrainingActiveChanged.Broadcast(bActive);
}

void UEnemyBuildingTrainerComponent::ApplyActivityToComponents(const bool bActive, const bool bIsStateChange,
                                                                const bool bApplyGlow)
{
	AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return;
	}
	float LongestRemainingCycle = 0.f;
	TInlineComponentArray<UActorComponent*> Components(Owner);
	for (UActorComponent* Component : Components)
	{
		if (not IsValid(Component))
		{
			continue;
		}
		if (Component->ComponentHasTag(ActivityTag))
		{
			if (USkeletalMeshComponent* SkeletalMesh = Cast<USkeletalMeshComponent>(Component))
			{
				ApplySkeletalActivity(SkeletalMesh, bActive, bIsStateChange, LongestRemainingCycle);
			}
			else if (ULightComponent* Light = Cast<ULightComponent>(Component))
			{
				if (Light->IsVisible() != bActive)
				{
					Light->SetVisibility(bActive);
				}
			}
			else if (UFXSystemComponent* Effect = Cast<UFXSystemComponent>(Component))
			{
				if (bActive && not Effect->IsActive())
				{
					Effect->Activate(true);
				}
				else if (not bActive && Effect->IsActive())
				{
					Effect->Deactivate();
				}
			}
			else if (UAudioComponent* Audio = Cast<UAudioComponent>(Component))
			{
				if (bActive && not Audio->IsPlaying())
				{
					Audio->Play();
				}
				else if (not bActive && Audio->IsPlaying())
				{
					if (bIsStateChange && AudioFadeOutSeconds > 0.f)
					{
						Audio->FadeOut(AudioFadeOutSeconds, 0.f);
					}
					else if (not bIsStateChange && Audio->GetPlayState() != EAudioComponentPlayState::FadingOut)
					{
						Audio->Stop();
					}
				}
			}
			else if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
			{
				if (SceneComponent->IsVisible() != bActive)
				{
					SceneComponent->SetVisibility(bActive, true);
				}
			}
		}
		if (Component->ComponentHasTag(HideWhileTrainingTag))
		{
			if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
			{
				if (SceneComponent->IsVisible() == bActive)
				{
					SceneComponent->SetVisibility(not bActive, true);
				}
			}
		}
		if (bApplyGlow && Component->ComponentHasTag(GlowTag))
		{
			if (UMeshComponent* Mesh = Cast<UMeshComponent>(Component))
			{
				Mesh->SetScalarParameterValueOnMaterials(ActivityMaterialParameterName, bActive ? 1.f : 0.f);
			}
		}
	}

	UWorld* World = GetWorld();
	if (not bActive && bIsStateChange && LongestRemainingCycle > 0.f && IsValid(World))
	{
		World->GetTimerManager().SetTimer(M_FinishCycleTimer, this,
		                                  &UEnemyBuildingTrainerComponent::OnFinishCycleTimer,
		                                  LongestRemainingCycle, false);
	}
}

void UEnemyBuildingTrainerComponent::ApplySkeletalActivity(USkeletalMeshComponent* SkeletalMesh, const bool bActive,
                                                           const bool bIsStateChange,
                                                           float& InOutLongestRemainingCycle) const
{
	// Meshes driven by an animation Blueprint follow OnTrainingActiveChanged instead.
	if (SkeletalMesh->GetAnimationMode() != EAnimationMode::AnimationSingleNode)
	{
		return;
	}
	if (bActive)
	{
		if (SkeletalMesh->IsPlaying())
		{
			return;
		}
		if (IdleAnimation != EEnemyTrainerIdleAnimation::PauseInPlace)
		{
			// Every tagged mesh starts at the rest time on the same call: loops of the same length stay in step.
			SkeletalMesh->SetPosition(GetRestPosition(SkeletalMesh), false);
		}
		SkeletalMesh->Play(true);
		return;
	}

	const UWorld* World = GetWorld();
	const bool bIsFinishingCycle = IsValid(World) && World->GetTimerManager().IsTimerActive(M_FinishCycleTimer);
	if (bIsStateChange && IdleAnimation == EEnemyTrainerIdleAnimation::FinishCycle)
	{
		UAnimSingleNodeInstance* SingleNode = SkeletalMesh->GetSingleNodeInstance();
		if (SkeletalMesh->IsPlaying() && IsValid(SingleNode))
		{
			// Play on until the loop comes round to the rest time again.
			const float Length = SingleNode->GetLength();
			const float Remaining = Length > KINDA_SMALL_NUMBER
				                        ? FMath::Fmod(GetRestPosition(SkeletalMesh) - SkeletalMesh->GetPosition() + Length,
				                                      Length)
				                        : 0.f;
			InOutLongestRemainingCycle = FMath::Max(InOutLongestRemainingCycle, Remaining);
		}
		return;
	}
	if (bIsFinishingCycle)
	{
		return;
	}
	const bool bShouldRestAtRestTime = IdleAnimation != EEnemyTrainerIdleAnimation::PauseInPlace;
	const float RestPosition = GetRestPosition(SkeletalMesh);
	if (SkeletalMesh->IsPlaying())
	{
		SkeletalMesh->Stop();
		if (bShouldRestAtRestTime)
		{
			SkeletalMesh->SetPosition(RestPosition, false);
		}
		return;
	}
	if (bShouldRestAtRestTime && not FMath::IsNearlyEqual(SkeletalMesh->GetPosition(), RestPosition, 0.01f))
	{
		SkeletalMesh->SetPosition(RestPosition, false);
	}
}

void UEnemyBuildingTrainerComponent::OnFinishCycleTimer()
{
	if (bM_IsActivityOn)
	{
		return;
	}
	StopTaggedSkeletalMeshes(true);
}

void UEnemyBuildingTrainerComponent::StopTaggedSkeletalMeshes(const bool bReturnToStart) const
{
	const AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return;
	}
	TInlineComponentArray<USkeletalMeshComponent*> SkeletalMeshes(Owner);
	for (USkeletalMeshComponent* SkeletalMesh : SkeletalMeshes)
	{
		if (not IsValid(SkeletalMesh) || not SkeletalMesh->ComponentHasTag(ActivityTag)
			|| SkeletalMesh->GetAnimationMode() != EAnimationMode::AnimationSingleNode)
		{
			continue;
		}
		SkeletalMesh->Stop();
		if (bReturnToStart)
		{
			SkeletalMesh->SetPosition(GetRestPosition(SkeletalMesh), false);
		}
	}
}

float UEnemyBuildingTrainerComponent::GetRestPosition(USkeletalMeshComponent* SkeletalMesh) const
{
	UAnimSingleNodeInstance* SingleNode = IsValid(SkeletalMesh) ? SkeletalMesh->GetSingleNodeInstance() : nullptr;
	const float Length = IsValid(SingleNode) ? SingleNode->GetLength() : 0.f;
	return FMath::Clamp(IdleRestSeconds, 0.f, FMath::Max(0.f, Length));
}

void UEnemyBuildingTrainerComponent::Broadcast(const EEnemyBuildingTrainingEvent Event, const FTrainingOption& Option,
                                               AActor* Unit, const float Duration)
{
	const TCHAR* EventName = nullptr;
	switch (Event)
	{
	case EEnemyBuildingTrainingEvent::Queued: EventName = TEXT("queued");
		break;
	case EEnemyBuildingTrainingEvent::Started: EventName = TEXT("started");
		break;
	case EEnemyBuildingTrainingEvent::Finished: EventName = TEXT("finished");
		break;
	case EEnemyBuildingTrainingEvent::Cancelled: EventName = TEXT("cancelled");
		break;
	default:
		break;
	}
	if (EventName != nullptr)
	{
		const UWorld* World = GetWorld();
		UE_LOG(LogEnemyBuildingTrainer, Log, TEXT("AI_TRAIN %s %s at %s (P%d) t=%.1f duration=%.1f unit=%s exit=%s"),
		       EventName, *Option.GetDisplayName(), *GetOwnerNameForLog(),
		       FRTSPlayers::GetOwningPlayerOfActor(GetOwner()), IsValid(World) ? World->GetTimeSeconds() : 0.f, Duration,
		       *GetNameSafe(Unit), *GetExitTransform().GetLocation().ToCompactString());
	}
	OnTrainingEventNative.Broadcast(Event, Option, Unit, Duration);
}

FString UEnemyBuildingTrainerComponent::GetOwnerNameForLog() const
{
	return IsValid(GetOwner()) ? GetOwner()->GetName() : FString(TEXT("<no owner>"));
}

#if RTS_WITH_MAP_TESTS
int32 UEnemyBuildingTrainerComponent::MapTest_GetTaggedActivityComponentCount() const
{
	const AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return 0;
	}
	int32 Count = 0;
	TInlineComponentArray<UActorComponent*> Components(Owner);
	for (const UActorComponent* Component : Components)
	{
		if (IsValid(Component) && (Component->ComponentHasTag(ActivityTag) ||
			Component->ComponentHasTag(HideWhileTrainingTag) || Component->ComponentHasTag(GlowTag)))
		{
			++Count;
		}
	}
	return Count;
}
#endif
