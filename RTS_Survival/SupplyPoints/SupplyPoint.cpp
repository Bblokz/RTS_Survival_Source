// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SupplyPoint.h"

#include "Components/MeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "RTS_Survival/FactionSystem/Factions/Factions.h"
#include "RTS_Survival/FOWSystem/FowManager/FowManager.h"
#include "RTS_Survival/GameUI/MiniMap/CustomIcons/MinimapIconTypes.h"
#include "RTS_Survival/GameUI/Pooled_TimedProgressBars/RTSProgressBarType.h"
#include "RTS_Survival/GameUI/Pooled_TimedProgressBars/Pooling/WorldSubSystem/RTSTimedProgressBarWorldSubsystem.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/SupplyPoints/SupplyPointSubsystem.h"
#include "RTS_Survival/SupplyPoints/SupplyTruckComponent.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace SupplyPointPrivate
{
	// How often the capture radius is searched; capture progress advances by this step.
	constexpr float CaptureSearchInterval = 0.25f;
	// The flag catches up with the capture progress a little faster than the capture itself runs.
	constexpr float FlagCatchUpSpeedMultiplier = 1.5f;
	constexpr float MiniMapIconRetryInterval = 1.f;
	constexpr int32 MiniMapIconMaxRetries = 20;
	constexpr float TruckPointDistance = 1400.f;
	const FVector TruckParkDefaultLocation(0.f, 700.f, 0.f);
}

ASupplyPoint::ASupplyPoint()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SupplyPointRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SupplyPointRoot"));
	SetRootComponent(SupplyPointRoot);

	// A drive-through bay beside the point by default; the Blueprint moves the three points to fit its meshes.
	TruckParkPoint = CreateDefaultSubobject<USceneComponent>(TEXT("TruckParkPoint"));
	TruckParkPoint->SetupAttachment(SupplyPointRoot);
	TruckParkPoint->SetRelativeLocation(SupplyPointPrivate::TruckParkDefaultLocation);

	TruckApproachPoint = CreateDefaultSubobject<USceneComponent>(TEXT("TruckApproachPoint"));
	TruckApproachPoint->SetupAttachment(SupplyPointRoot);
	TruckApproachPoint->SetRelativeLocation(
		SupplyPointPrivate::TruckParkDefaultLocation - FVector(SupplyPointPrivate::TruckPointDistance, 0.f, 0.f));

	TruckExitPoint = CreateDefaultSubobject<USceneComponent>(TEXT("TruckExitPoint"));
	TruckExitPoint->SetupAttachment(SupplyPointRoot);
	TruckExitPoint->SetRelativeLocation(
		SupplyPointPrivate::TruckParkDefaultLocation + FVector(SupplyPointPrivate::TruckPointDistance, 0.f, 0.f));
}

void ASupplyPoint::BeginPlay()
{
	Super::BeginPlay();

	if (USupplyPointSubsystem* SupplySubsystem = USupplyPointSubsystem::Get(this))
	{
		SupplySubsystem->RegisterSupplyPoint(this);
	}
	BeginPlay_FindFlag();
	BeginPlay_ApplyInitialOwner();
	BeginPlay_StartCaptureSearch();
	BeginPlay_AddMiniMapIcon();
}

void ASupplyPoint::BeginPlay_FindFlag()
{
	if (M_FlagComponent.IsValid() or FlagComponentName.IsNone())
	{
		return;
	}
	TInlineComponentArray<USceneComponent*> SceneComponents(this);
	for (USceneComponent* SceneComponent : SceneComponents)
	{
		if (IsValid(SceneComponent) and SceneComponent->GetFName() == FlagComponentName)
		{
			InitSupplyPoint(SceneComponent);
			return;
		}
	}
}

void ASupplyPoint::BeginPlay_ApplyInitialOwner()
{
	const uint8 InitialOwner = SupplyPointSettings.InitialOwningPlayer;
	ApplyFlagFaction();
	if (InitialOwner == SupplyPointConstants::NeutralOwner)
	{
		return;
	}
	// The flag is already up when the map starts: no raise animation.
	M_CaptureState.ShownProgress = 1.f;
	ForceCaptureByPlayer(InitialOwner);
}

void ASupplyPoint::BeginPlay_StartCaptureSearch()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	TWeakObjectPtr<ASupplyPoint> WeakThis(this);
	World->GetTimerManager().SetTimer(M_CaptureSearchTimerHandle, FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->UpdateCapture();
		}
	}), SupplyPointPrivate::CaptureSearchInterval, true);
}

void ASupplyPoint::BeginPlay_AddMiniMapIcon()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	// The fog of war manager can come up after the level actors; retry until it exists.
	TWeakObjectPtr<ASupplyPoint> WeakThis(this);
	TSharedRef<int32> RetriesLeft = MakeShared<int32>(SupplyPointPrivate::MiniMapIconMaxRetries);
	TSharedRef<FTimerHandle> RetryHandle = MakeShared<FTimerHandle>();
	World->GetTimerManager().SetTimer(*RetryHandle, FTimerDelegate::CreateLambda([WeakThis, RetriesLeft, RetryHandle]()
	{
		if (not WeakThis.IsValid() or WeakThis->GetWorld() == nullptr)
		{
			return;
		}
		AFowManager* FowManager = FRTS_Statics::GetFowManager(WeakThis.Get());
		--(*RetriesLeft);
		if (not IsValid(FowManager) and *RetriesLeft > 0)
		{
			return;
		}
		WeakThis->GetWorld()->GetTimerManager().ClearTimer(*RetryHandle);
		if (not IsValid(FowManager))
		{
			return;
		}
		const FName IconId(*FString::Printf(TEXT("SupplyPoint_%s"), *WeakThis->GetName()));
		WeakThis->M_MiniMapIconId = FowManager->AddCustomMiniMapIcon(
			IconId, WeakThis->GetMiniMapIconOfHolder(), WeakThis->GetActorLocation(), FRotator::ZeroRotator,
			FMinimapIconTextPayload());
	}), SupplyPointPrivate::MiniMapIconRetryInterval, true);
}

void ASupplyPoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_CaptureSearchTimerHandle);
		World->GetTimerManager().ClearTimer(M_SupplyTimerHandle);
	}
	StopProgressBar();
	StopCaptureProgressBar();
	if (not M_MiniMapIconId.IsNone())
	{
		if (AFowManager* FowManager = FRTS_Statics::GetFowManager(this))
		{
			FowManager->RemoveCustomMiniMapIcon(M_MiniMapIconId);
		}
	}
	if (USupplyPointSubsystem* SupplySubsystem = USupplyPointSubsystem::Get(this))
	{
		SupplySubsystem->UnregisterSupplyPoint(this);
	}
	Super::EndPlay(EndPlayReason);
}

void ASupplyPoint::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateFlagVisuals(DeltaSeconds);
}

void ASupplyPoint::InitSupplyPoint(USceneComponent* FlagComponent)
{
	if (not IsValid(FlagComponent))
	{
		RTSFunctionLibrary::ReportError("InitSupplyPoint got no flag component on " + GetName());
		return;
	}
	M_FlagComponent = FlagComponent;
	M_FlagLoweredRelativeLocation = FlagComponent->GetRelativeLocation();
	ApplyFlagFaction();
	ApplyShownFlagProgress();
}

FVector ASupplyPoint::GetTruckApproachLocation() const
{
	return TruckApproachPoint->GetComponentLocation();
}

FTransform ASupplyPoint::GetTruckParkTransform() const
{
	return TruckParkPoint->GetComponentTransform();
}

FVector ASupplyPoint::GetTruckParkOrderLocation() const
{
	return TruckParkPoint->GetComponentLocation()
		+ TruckParkPoint->GetForwardVector() * SupplyPointSettings.TruckStopShortDistance;
}

FVector ASupplyPoint::GetTruckExitLocation() const
{
	return TruckExitPoint->GetComponentLocation();
}

// ---------------------------------------------------------------------------------------------------------------------
// Capture
// ---------------------------------------------------------------------------------------------------------------------

void ASupplyPoint::UpdateCapture()
{
	uint8 SinglePlayerPresent = SupplyPointConstants::NeutralOwner;
	const bool bIsContested = FindPlayersInCaptureRadius(SinglePlayerPresent);
	const bool bIsEnemyAlone = SinglePlayerPresent != SupplyPointConstants::NeutralOwner
		and SinglePlayerPresent != SupplyPointConstants::PlayerOwner;
	// Both sides in the radius, or an enemy that may not capture: the flag stays where it is.
	const bool bIsStalled = bIsContested or (bIsEnemyAlone and not SupplyPointSettings.bEnemyCanCapture);
	if (not bIsStalled)
	{
		const float PreviousProgress = M_CaptureState.Progress;
		if (SinglePlayerPresent == SupplyPointConstants::NeutralOwner)
		{
			RelaxCapture(SupplyPointPrivate::CaptureSearchInterval);
		}
		else
		{
			AdvanceCapture(SinglePlayerPresent, SupplyPointPrivate::CaptureSearchInterval);
		}
		if (not FMath::IsNearlyEqual(PreviousProgress, M_CaptureState.Progress))
		{
			SetActorTickEnabled(true);
		}
	}
	const bool bIsPlayerFlagUp = M_CaptureState.ProgressPlayer == SupplyPointConstants::PlayerOwner
		and M_CaptureState.Progress >= 1.f;
	const bool bIsPlayerCapturing = not bIsStalled and SinglePlayerPresent == SupplyPointConstants::PlayerOwner
		and not bIsPlayerFlagUp;
	UpdateCaptureProgressBar(bIsPlayerCapturing);
}

float ASupplyPoint::GetPlayerCaptureSecondsLeft() const
{
	const bool bIsForeignFlagUp = M_CaptureState.ProgressPlayer != SupplyPointConstants::PlayerOwner
		and M_CaptureState.Progress > 0.f;
	// A foreign flag comes down first, then the player's goes all the way up.
	const float ProgressLeft = bIsForeignFlagUp ? M_CaptureState.Progress + 1.f : 1.f - M_CaptureState.Progress;
	return ProgressLeft * SupplyPointSettings.CaptureDuration;
}

void ASupplyPoint::UpdateCaptureProgressBar(const bool bIsPlayerCapturing)
{
	if (not bIsPlayerCapturing)
	{
		StopCaptureProgressBar();
		return;
	}
	if (M_CaptureBarActivationId != INDEX_NONE)
	{
		// Already running: the capture and the bar advance at the same speed.
		return;
	}
	const UWorld* World = GetWorld();
	URTSTimedProgressBarWorldSubsystem* ProgressSubsystem =
		World != nullptr ? World->GetSubsystem<URTSTimedProgressBarWorldSubsystem>() : nullptr;
	const float SecondsLeft = GetPlayerCaptureSecondsLeft();
	if (ProgressSubsystem == nullptr or SecondsLeft <= 0.f)
	{
		return;
	}
	const bool bIsForeignFlagUp = M_CaptureState.ProgressPlayer != SupplyPointConstants::PlayerOwner
		and M_CaptureState.Progress > 0.f;
	M_CaptureState.bPlayerCaptureIncludesLowering |= bIsForeignFlagUp;
	constexpr float LoweringAndRaising = 2.f;
	const float TotalSeconds = SupplyPointSettings.CaptureDuration
		* (M_CaptureState.bPlayerCaptureIncludesLowering ? LoweringAndRaising : 1.f);
	const float RatioStart = FMath::Clamp(1.f - SecondsLeft / FMath::Max(TotalSeconds, KINDA_SMALL_NUMBER), 0.f, 1.f);
	constexpr bool bUsePercentageText = true;
	constexpr bool bUseDescriptionText = true;
	M_CaptureBarActivationId = ProgressSubsystem->ActivateTimedProgressBarAnchored(
		SupplyPointRoot, SupplyPointSettings.ProgressBarOffset, RatioStart, SecondsLeft, bUsePercentageText,
		ERTSProgressBarType::Default, bUseDescriptionText,
		NSLOCTEXT("SupplyPoint", "Capturing", "Capturing").ToString(), SupplyPointSettings.ProgressBarScale);
}

void ASupplyPoint::StopCaptureProgressBar()
{
	if (M_CaptureBarActivationId == INDEX_NONE)
	{
		return;
	}
	const UWorld* World = GetWorld();
	if (URTSTimedProgressBarWorldSubsystem* ProgressSubsystem =
		World != nullptr ? World->GetSubsystem<URTSTimedProgressBarWorldSubsystem>() : nullptr)
	{
		ProgressSubsystem->ForceProgressBarDormant(M_CaptureBarActivationId);
	}
	M_CaptureBarActivationId = INDEX_NONE;
}

bool ASupplyPoint::FindPlayersInCaptureRadius(uint8& OutSinglePlayerPresent) const
{
	OutSinglePlayerPresent = SupplyPointConstants::NeutralOwner;
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(COLLISION_OBJ_PLAYER);
	ObjectParams.AddObjectTypesToQuery(COLLISION_OBJ_ENEMY);
	FCollisionQueryParams QueryParams(TEXT("SupplyPointCapture"), false, this);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, GetActorLocation(), FQuat::Identity, ObjectParams,
	                                FCollisionShape::MakeSphere(SupplyPointSettings.CaptureRadius), QueryParams);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		uint8 UnitOwner = SupplyPointConstants::NeutralOwner;
		if (not GetCanActorCapture(Overlap.GetActor(), UnitOwner))
		{
			continue;
		}
		if (OutSinglePlayerPresent != SupplyPointConstants::NeutralOwner and OutSinglePlayerPresent != UnitOwner)
		{
			return true;
		}
		OutSinglePlayerPresent = UnitOwner;
	}
	return false;
}

bool ASupplyPoint::GetCanActorCapture(AActor* Actor, uint8& OutOwningPlayer) const
{
	if (not RTSFunctionLibrary::RTSIsValid(Actor))
	{
		return false;
	}
	if (const ASquadUnit* SquadUnit = Cast<ASquadUnit>(Actor))
	{
		OutOwningPlayer = static_cast<uint8>(SquadUnit->GetOwningPlayer());
		return OutOwningPlayer != SupplyPointConstants::NeutralOwner;
	}
	const ATankMaster* Tank = Cast<ATankMaster>(Actor);
	if (Tank == nullptr or Tank->FindComponentByClass<USupplyTruckComponent>() != nullptr)
	{
		// Aircraft, buildings and the supply trucks themselves do not capture.
		return false;
	}
	const ANomadicVehicle* Nomadic = Cast<ANomadicVehicle>(Tank);
	if (Nomadic != nullptr and Nomadic->GetNomadicStatus() != ENomadStatus::Truck)
	{
		return false;
	}
	const URTSComponent* TankRTSComponent = Tank->GetRTSComponent();
	if (not IsValid(TankRTSComponent))
	{
		return false;
	}
	OutOwningPlayer = TankRTSComponent->GetOwningPlayer();
	return OutOwningPlayer != SupplyPointConstants::NeutralOwner;
}

void ASupplyPoint::AdvanceCapture(const uint8 CapturingPlayer, const float DeltaSeconds)
{
	const float ProgressStep = DeltaSeconds / FMath::Max(SupplyPointSettings.CaptureDuration, KINDA_SMALL_NUMBER);
	if (M_CaptureState.ProgressPlayer != CapturingPlayer and M_CaptureState.Progress > 0.f)
	{
		// Somebody else's flag is on the pole: it comes down first.
		M_CaptureState.Progress = FMath::Max(0.f, M_CaptureState.Progress - ProgressStep);
		if (M_CaptureState.Progress <= 0.f)
		{
			SetOwningPlayer(SupplyPointConstants::NeutralOwner);
			M_CaptureState.ProgressPlayer = CapturingPlayer;
			ApplyFlagFaction();
		}
		return;
	}
	if (M_CaptureState.ProgressPlayer != CapturingPlayer)
	{
		M_CaptureState.ProgressPlayer = CapturingPlayer;
		ApplyFlagFaction();
	}
	if (M_CaptureState.Progress >= 1.f)
	{
		return;
	}
	M_CaptureState.Progress = FMath::Min(1.f, M_CaptureState.Progress + ProgressStep);
	if (M_CaptureState.Progress >= 1.f)
	{
		SetOwningPlayer(CapturingPlayer);
	}
}

void ASupplyPoint::RelaxCapture(const float DeltaSeconds)
{
	const float ProgressStep = DeltaSeconds / FMath::Max(SupplyPointSettings.CaptureDecayDuration, KINDA_SMALL_NUMBER);
	const bool bIsHeld = M_CaptureState.OwningPlayer != SupplyPointConstants::NeutralOwner;
	// A held point pulls its flag back up; an unfinished capture falls back down.
	M_CaptureState.Progress = bIsHeld
		                          ? FMath::Min(1.f, M_CaptureState.Progress + ProgressStep)
		                          : FMath::Max(0.f, M_CaptureState.Progress - ProgressStep);
}

void ASupplyPoint::ForceCaptureByPlayer(const uint8 NewOwningPlayer)
{
	M_CaptureState.ProgressPlayer = NewOwningPlayer;
	M_CaptureState.Progress = NewOwningPlayer == SupplyPointConstants::NeutralOwner ? 0.f : 1.f;
	ApplyFlagFaction();
	SetActorTickEnabled(true);
	SetOwningPlayer(NewOwningPlayer);
}

void ASupplyPoint::SetOwningPlayer(const uint8 NewOwningPlayer)
{
	const uint8 PreviousOwner = M_CaptureState.OwningPlayer;
	if (PreviousOwner == NewOwningPlayer)
	{
		return;
	}
	StopSupplyCycle();
	M_CaptureState.OwningPlayer = NewOwningPlayer;
	if (NewOwningPlayer != SupplyPointConstants::NeutralOwner)
	{
		// A flag is fully up: whatever capture comes next starts a fresh bar.
		M_CaptureState.bPlayerCaptureIncludesLowering = false;
		StopCaptureProgressBar();
	}
	UpdateMiniMapIcon();
	BP_OnOwnerChanged(NewOwningPlayer, PreviousOwner);
	if (NewOwningPlayer != SupplyPointConstants::NeutralOwner)
	{
		StartPreparingSupplies();
	}
	// Last: listeners (the supply subsystem) see the point in its new state.
	OnSupplyPointOwnerChanged.Broadcast(this, PreviousOwner);
}

void ASupplyPoint::UpdateFlagVisuals(const float DeltaSeconds)
{
	const float CatchUpSpeed = SupplyPointPrivate::FlagCatchUpSpeedMultiplier
		/ FMath::Max(SupplyPointSettings.CaptureDuration, KINDA_SMALL_NUMBER);
	M_CaptureState.ShownProgress = FMath::FInterpConstantTo(M_CaptureState.ShownProgress, M_CaptureState.Progress,
	                                                        DeltaSeconds, CatchUpSpeed);
	ApplyShownFlagProgress();
	if (FMath::IsNearlyEqual(M_CaptureState.ShownProgress, M_CaptureState.Progress))
	{
		SetActorTickEnabled(false);
	}
}

EMinimapIconType ASupplyPoint::GetMiniMapIconOfHolder() const
{
	if (M_CaptureState.OwningPlayer != SupplyPointConstants::PlayerOwner)
	{
		switch (SupplyPointSettings.EnemyDirector)
		{
		case EEnemyDirector::DirectorOfForge: return EMinimapIconType::SupplyFlag_DirectorOfForge;
		case EEnemyDirector::DirectorOfHarvest: return EMinimapIconType::SupplyFlag_DirectorOfHarvest;
		default: return EMinimapIconType::SupplyFlag_DirectorOfShield;
		}
	}
	switch (FRTS_Statics::GetPlayerFaction(this))
	{
	case ERTSFaction::GerBreakthroughDoctrine: return EMinimapIconType::SupplyFlag_GerBreakthrough;
	// The Italian faction has no flag asset yet (SupplyFlag_GerItalian has no icon): it shows the default below.
	default: return EMinimapIconType::SupplyFlag_GerStrikeDivision;
	}
}

void ASupplyPoint::UpdateMiniMapIcon() const
{
	if (M_MiniMapIconId.IsNone())
	{
		return;
	}
	if (AFowManager* FowManager = FRTS_Statics::GetFowManager(this))
	{
		FowManager->SwapCustomMiniMapIcon(M_MiniMapIconId, GetMiniMapIconOfHolder());
	}
}

void ASupplyPoint::ApplyFlagFaction() const
{
	UMeshComponent* FlagMesh = Cast<UMeshComponent>(M_FlagComponent.Get());
	if (FlagMesh == nullptr)
	{
		return;
	}
	// The flag on the pole belongs to whoever the capture progress belongs to; game flags pick their faction by
	// material, like the flag pole attachment of the vehicles.
	const bool bIsPlayerFlag = M_CaptureState.ProgressPlayer == SupplyPointConstants::PlayerOwner;
	const TObjectPtr<UMaterialInterface>* FlagMaterial = bIsPlayerFlag
		                                                     ? PlayerFlagMaterials.Find(
			                                                     FRTS_Statics::GetPlayerFaction(this))
		                                                     : EnemyFlagMaterials.Find(SupplyPointSettings.EnemyDirector);
	if (FlagMaterial == nullptr or not IsValid(*FlagMaterial))
	{
		return;
	}
	FlagMesh->SetMaterial(FlagMaterialSlot, *FlagMaterial);
}

void ASupplyPoint::ApplyShownFlagProgress()
{
	if (M_FlagComponent.IsValid())
	{
		const FVector RaisedOffset(0.f, 0.f, M_CaptureState.ShownProgress * SupplyPointSettings.FlagRaiseHeight);
		M_FlagComponent->SetRelativeLocation(M_FlagLoweredRelativeLocation + RaisedOffset);
	}
	BP_OnCaptureProgressChanged(M_CaptureState.ShownProgress, M_CaptureState.ProgressPlayer);
}

// ---------------------------------------------------------------------------------------------------------------------
// Supply cycle
// ---------------------------------------------------------------------------------------------------------------------

void ASupplyPoint::SetSupplyPointState(const ESupplyPointState NewState)
{
	M_SupplyPointState = NewState;
}

void ASupplyPoint::StartPreparingSupplies()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}
	SetSupplyPointState(ESupplyPointState::PreparingSupplies);
	const float Duration = SupplyPointSettings.SuppliesReadyTime;
	BP_OnSuppliesPreparing(Duration);
	OnSuppliesPreparingNative.Broadcast(Duration);
	if (Duration <= 0.f)
	{
		OnSuppliesReady();
		return;
	}
	StartProgressBar(Duration, NSLOCTEXT("SupplyPoint", "PreparingSupplies", "Supplies"));
	TWeakObjectPtr<ASupplyPoint> WeakThis(this);
	World->GetTimerManager().SetTimer(M_SupplyTimerHandle, FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->OnSuppliesReady();
		}
	}), Duration, false);
}

void ASupplyPoint::OnSuppliesReady()
{
	StopProgressBar();
	SetSupplyPointState(ESupplyPointState::SuppliesReady);
	BP_OnSuppliesReady();
	OnSuppliesReadyNative.Broadcast();
	if (M_ParkedTruck.IsValid())
	{
		StartLoadingTruck();
	}
}

bool ASupplyPoint::OnSupplyTruckParked(USupplyTruckComponent* SupplyTruck)
{
	if (not IsValid(SupplyTruck))
	{
		return false;
	}
	const bool bIsHeldByTruckOwner = M_CaptureState.OwningPlayer != SupplyPointConstants::NeutralOwner
		and M_CaptureState.OwningPlayer == SupplyTruck->GetTruckOwningPlayer();
	const bool bBayIsTaken = M_ParkedTruck.IsValid() and M_ParkedTruck.Get() != SupplyTruck;
	if (not bIsHeldByTruckOwner or bBayIsTaken or M_SupplyPointState == ESupplyPointState::Inactive)
	{
		return false;
	}
	M_ParkedTruck = SupplyTruck;
	if (M_SupplyPointState == ESupplyPointState::SuppliesReady)
	{
		StartLoadingTruck();
	}
	return true;
}

void ASupplyPoint::OnSupplyTruckLeftBay(const USupplyTruckComponent* SupplyTruck)
{
	if (M_ParkedTruck.Get() != SupplyTruck)
	{
		return;
	}
	M_ParkedTruck.Reset();
	if (M_SupplyPointState != ESupplyPointState::LoadingTruck)
	{
		return;
	}
	// The truck was lost under the crane: the load stays at the point for the next truck.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_SupplyTimerHandle);
	}
	StopProgressBar();
	BP_OnTruckLoadingFinished(false);
	OnTruckLoadingFinishedNative.Broadcast(false);
	SetSupplyPointState(ESupplyPointState::SuppliesReady);
}

void ASupplyPoint::StartLoadingTruck()
{
	UWorld* World = GetWorld();
	if (World == nullptr or not M_ParkedTruck.IsValid())
	{
		return;
	}
	SetSupplyPointState(ESupplyPointState::LoadingTruck);
	const float Duration = SupplyPointSettings.TruckLoadingTime;
	M_ParkedTruck->OnLoadingStarted(Duration);
	BP_OnTruckLoadingStarted(M_ParkedTruck->GetTruck(), Duration, SupplyPointSettings.SuppliesPerTrip);
	OnTruckLoadingStartedNative.Broadcast(M_ParkedTruck->GetTruck(), Duration, SupplyPointSettings.SuppliesPerTrip);
	if (Duration <= 0.f)
	{
		OnTruckLoadingFinished();
		return;
	}
	StartProgressBar(Duration, NSLOCTEXT("SupplyPoint", "LoadingTruck", "Loading"));
	TWeakObjectPtr<ASupplyPoint> WeakThis(this);
	World->GetTimerManager().SetTimer(M_SupplyTimerHandle, FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->OnTruckLoadingFinished();
		}
	}), Duration, false);
}

void ASupplyPoint::OnTruckLoadingFinished()
{
	StopProgressBar();
	BP_OnTruckLoadingFinished(true);
	OnTruckLoadingFinishedNative.Broadcast(true);
	USupplyTruckComponent* LoadedTruck = M_ParkedTruck.Get();
	M_ParkedTruck.Reset();
	if (SupplyPointSettings.bRepeatSupplyRuns)
	{
		StartPreparingSupplies();
	}
	else
	{
		SetSupplyPointState(ESupplyPointState::Inactive);
	}
	if (IsValid(LoadedTruck))
	{
		LoadedTruck->OnLoadingFinished(SupplyPointSettings.SuppliesPerTrip);
	}
}

void ASupplyPoint::StopSupplyCycle()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_SupplyTimerHandle);
	}
	StopProgressBar();
	if (M_SupplyPointState == ESupplyPointState::LoadingTruck)
	{
		BP_OnTruckLoadingFinished(false);
		OnTruckLoadingFinishedNative.Broadcast(false);
	}
	// The truck (if any) is sent home by the supply subsystem when it hears about the owner change.
	M_ParkedTruck.Reset();
	SetSupplyPointState(ESupplyPointState::Inactive);
}

void ASupplyPoint::StartProgressBar(const float Duration, const FText& Description)
{
	StopProgressBar();
	// Only the player sees the timers of their own points.
	if (M_CaptureState.OwningPlayer != SupplyPointConstants::PlayerOwner)
	{
		return;
	}
	const UWorld* World = GetWorld();
	URTSTimedProgressBarWorldSubsystem* ProgressSubsystem =
		World != nullptr ? World->GetSubsystem<URTSTimedProgressBarWorldSubsystem>() : nullptr;
	if (ProgressSubsystem == nullptr)
	{
		return;
	}
	constexpr float RatioStart = 0.f;
	constexpr bool bUsePercentageText = false;
	constexpr bool bUseDescriptionText = true;
	M_ProgressBarActivationId = ProgressSubsystem->ActivateTimedProgressBarAnchored(
		SupplyPointRoot, SupplyPointSettings.ProgressBarOffset, RatioStart, Duration, bUsePercentageText,
		ERTSProgressBarType::Default, bUseDescriptionText, Description.ToString(),
		SupplyPointSettings.ProgressBarScale);
}

void ASupplyPoint::StopProgressBar()
{
	if (M_ProgressBarActivationId == INDEX_NONE)
	{
		return;
	}
	const UWorld* World = GetWorld();
	if (URTSTimedProgressBarWorldSubsystem* ProgressSubsystem =
		World != nullptr ? World->GetSubsystem<URTSTimedProgressBarWorldSubsystem>() : nullptr)
	{
		ProgressSubsystem->ForceProgressBarDormant(M_ProgressBarActivationId);
	}
	M_ProgressBarActivationId = INDEX_NONE;
}
