// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SupplyPointCraneComponent.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "RTS_Survival/SupplyPoints/SupplyPoint.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace SupplyPointCranePrivate
{
	// Shares of one lift, as fractions of its duration.
	constexpr float OverPickFraction = 0.25f;
	constexpr float OnPickFraction = 0.35f;
	constexpr float LiftedFraction = 0.45f;
	constexpr float OverBedFraction = 0.75f;
	constexpr float InBedFraction = 0.90f;
	constexpr float MinSecondsPerLift = 0.5f;
	constexpr float FullPercent = 100.f;
	// A hook closer than this to the trolley would pull the rope mesh inside out.
	constexpr float MinHookDrop = 10.f;

	const ERTSResourceType LoadOrder[] = {
		ERTSResourceType::Resource_Radixite, ERTSResourceType::Resource_Metal, ERTSResourceType::Resource_VehicleParts
	};

	template <typename ComponentType>
	ComponentType* FindComponentByName(const AActor* Owner, const FName ComponentName)
	{
		if (not IsValid(Owner) or ComponentName.IsNone())
		{
			return nullptr;
		}
		TInlineComponentArray<ComponentType*> Components(Owner);
		for (ComponentType* Component : Components)
		{
			if (IsValid(Component) and Component->GetFName() == ComponentName)
			{
				return Component;
			}
		}
		return nullptr;
	}
}

USupplyPointCraneComponent::USupplyPointCraneComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void USupplyPointCraneComponent::BeginPlay()
{
	Super::BeginPlay();
	M_SupplyPoint = Cast<ASupplyPoint>(GetOwner());
	if (not GetIsValidSupplyPoint())
	{
		return;
	}
	BeginPlay_FindParts();
	BeginPlay_CreateLoadComponents();
	BeginPlay_BindSupplyPoint();
	if (GetHasCraneParts())
	{
		M_CurrentPose = MakeRestPose();
		ApplyPose(M_CurrentPose);
	}
}

bool USupplyPointCraneComponent::GetIsValidSupplyPoint() const
{
	if (M_SupplyPoint.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_SupplyPoint", "GetIsValidSupplyPoint",
	                                                             GetOwner());
	return false;
}

bool USupplyPointCraneComponent::GetHasCraneParts() const
{
	return M_UpperWorks.IsValid() and M_Trolley.IsValid() and M_HookBlock.IsValid();
}

void USupplyPointCraneComponent::BeginPlay_FindParts()
{
	using namespace SupplyPointCranePrivate;
	const AActor* Owner = GetOwner();
	M_Base = FindComponentByName<UStaticMeshComponent>(Owner, CraneSettings.BaseComponentName);
	M_UpperWorks = FindComponentByName<USceneComponent>(Owner, CraneSettings.UpperWorksComponentName);
	M_Trolley = FindComponentByName<USceneComponent>(Owner, CraneSettings.TrolleyComponentName);
	M_Rope = FindComponentByName<USceneComponent>(Owner, CraneSettings.RopeComponentName);
	M_HookBlock = FindComponentByName<USceneComponent>(Owner, CraneSettings.HookBlockComponentName);
	if (not GetHasCraneParts())
	{
		RTSFunctionLibrary::ReportError(
			"Supply crane parts not found on " + GetNameSafe(Owner)
			+ ": check the component names in CraneSettings (upper works, trolley, hook block).");
	}
	const FSupplyTruckCargo& SuppliesPerTrip = M_SupplyPoint->GetSupplyPointSettings().SuppliesPerTrip;
	for (const TPair<ERTSResourceType, FSupplyCraneResourceSetup>& Setup : ResourceSetups)
	{
		M_StockComponents.Add(Setup.Key,
		                      FindComponentByName<UStaticMeshComponent>(Owner, Setup.Value.StockComponentName));
		// A depot looks stocked from the start; a kind this point does not supply stays empty.
		FSupplyStockFill& Fill = M_StockFills.Add(Setup.Key);
		Fill.Percent = SuppliesPerTrip.GetAmount(Setup.Key) > 0 ? FullPercent : 0.f;
		ApplyStockMesh(Setup.Key);
	}
}

void USupplyPointCraneComponent::BeginPlay_CreateLoadComponents()
{
	AActor* Owner = GetOwner();
	if (M_HookBlock.IsValid())
	{
		M_CarriedLoad = NewObject<UStaticMeshComponent>(Owner, TEXT("SupplyCraneCarriedLoad"));
		M_CarriedLoad->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M_CarriedLoad->SetupAttachment(M_HookBlock.Get());
		M_CarriedLoad->SetRelativeLocation(FVector(0.f, 0.f, -CraneSettings.HookToLoadBottom));
		M_CarriedLoad->SetVisibility(false);
		M_CarriedLoad->RegisterComponent();
	}
	if (not M_Base.IsValid())
	{
		return;
	}
	for (const TPair<ERTSResourceType, FSupplyCraneResourceSetup>& Setup : ResourceSetups)
	{
		if (not IsValid(Setup.Value.LoadMesh))
		{
			continue;
		}
		const FName ComponentName(*FString::Printf(TEXT("SupplyWaitingLoad_%d"), static_cast<int32>(Setup.Key)));
		UStaticMeshComponent* WaitingLoad = NewObject<UStaticMeshComponent>(Owner, ComponentName);
		WaitingLoad->SetStaticMesh(Setup.Value.LoadMesh);
		WaitingLoad->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WaitingLoad->SetupAttachment(M_Base.Get(), Setup.Value.PickSocketName);
		WaitingLoad->SetVisibility(false);
		WaitingLoad->RegisterComponent();
		M_WaitingLoads.Add(Setup.Key, WaitingLoad);
	}
}

void USupplyPointCraneComponent::BeginPlay_BindSupplyPoint()
{
	M_SupplyPoint->OnSuppliesPreparingNative.AddUObject(this, &USupplyPointCraneComponent::OnSuppliesPreparing);
	M_SupplyPoint->OnSuppliesReadyNative.AddUObject(this, &USupplyPointCraneComponent::OnSuppliesReady);
	M_SupplyPoint->OnTruckLoadingStartedNative.AddUObject(this, &USupplyPointCraneComponent::OnTruckLoadingStarted);
	M_SupplyPoint->OnTruckLoadingFinishedNative.AddUObject(this, &USupplyPointCraneComponent::OnTruckLoadingFinished);
}

void USupplyPointCraneComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (M_SupplyPoint.IsValid())
	{
		M_SupplyPoint->OnSuppliesPreparingNative.RemoveAll(this);
		M_SupplyPoint->OnSuppliesReadyNative.RemoveAll(this);
		M_SupplyPoint->OnTruckLoadingStartedNative.RemoveAll(this);
		M_SupplyPoint->OnTruckLoadingFinishedNative.RemoveAll(this);
	}
	Super::EndPlay(EndPlayReason);
}

void USupplyPointCraneComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
                                               FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	TickStockFill(DeltaTime);
	TickCraneCycle(DeltaTime);
	UpdateTickEnabled();
}

void USupplyPointCraneComponent::UpdateTickEnabled()
{
	SetComponentTickEnabled(bM_IsCycleRunning or bM_IsFilling);
}

int32 USupplyPointCraneComponent::GetShownStockLevel(const ERTSResourceType ResourceType) const
{
	const FSupplyStockFill* Fill = M_StockFills.Find(ResourceType);
	return Fill != nullptr ? FMath::RoundToInt32(Fill->Percent) : 0;
}

// ---------------------------------------------------------------------------------------------------------------------
// Supply point events
// ---------------------------------------------------------------------------------------------------------------------

void USupplyPointCraneComponent::OnSuppliesPreparing(const float Duration)
{
	for (TPair<ERTSResourceType, FSupplyStockFill>& Fill : M_StockFills)
	{
		Fill.Value.FillStartPercent = Fill.Value.Percent;
	}
	M_FillDuration = Duration;
	M_FillSeconds = 0.f;
	bM_IsFilling = Duration > 0.f;
	UpdateTickEnabled();
}

void USupplyPointCraneComponent::OnSuppliesReady()
{
	bM_IsFilling = false;
	if (not M_SupplyPoint.IsValid())
	{
		return;
	}
	const FSupplyTruckCargo& SuppliesPerTrip = M_SupplyPoint->GetSupplyPointSettings().SuppliesPerTrip;
	for (const TPair<ERTSResourceType, FSupplyCraneResourceSetup>& Setup : ResourceSetups)
	{
		if (SuppliesPerTrip.GetAmount(Setup.Key) > 0)
		{
			SetStockPercent(Setup.Key, SupplyPointCranePrivate::FullPercent);
		}
	}
	SetWaitingLoadsVisible(true);
	UpdateTickEnabled();
}

void USupplyPointCraneComponent::OnTruckLoadingStarted(ATankMaster* SupplyTruck, const float Duration,
                                                       const FSupplyTruckCargo& Cargo)
{
	M_TruckBeingLoaded = SupplyTruck;
	M_LoadsPlacedInTruck = 0;
	if (not GetHasCraneParts() or not IsValid(SupplyTruck))
	{
		return;
	}
	BuildCraneCycle(SupplyTruck, Duration, Cargo);
	M_CycleSeconds = 0.f;
	M_NextKeyIndex = 1;
	bM_IsCycleRunning = M_CycleKeys.Num() > 1;
	UpdateTickEnabled();
}

void USupplyPointCraneComponent::OnTruckLoadingFinished(const bool bCompleted)
{
	if (bCompleted)
	{
		// The loading time ran out before the crane was done (very short loading times): the rest lands at once,
		// so the truck never leaves with less than it was paid.
		for (int32 KeyIndex = M_NextKeyIndex; bM_IsCycleRunning and KeyIndex < M_CycleKeys.Num(); ++KeyIndex)
		{
			FireKeyEvent(M_CycleKeys[KeyIndex]);
		}
	}
	else if (IsValid(M_CarriedLoad))
	{
		M_CarriedLoad->SetVisibility(false);
	}
	M_TruckBeingLoaded.Reset();
	const bool bIsAlreadyReturning = bM_IsCycleRunning and M_NextKeyIndex == M_CycleKeys.Num() - 1 and bCompleted;
	if (GetHasCraneParts() and not bIsAlreadyReturning)
	{
		StartReturnToRest();
	}
	UpdateTickEnabled();
}

// ---------------------------------------------------------------------------------------------------------------------
// Crane cycle
// ---------------------------------------------------------------------------------------------------------------------

void USupplyPointCraneComponent::BuildCraneCycle(const ATankMaster* SupplyTruck, const float Duration,
                                                 const FSupplyTruckCargo& Cargo)
{
	using namespace SupplyPointCranePrivate;
	M_CycleKeys.Reset();
	AddKey(0.f, M_CurrentPose);

	TArray<ERTSResourceType> KindsToLoad;
	for (const ERTSResourceType ResourceType : LoadOrder)
	{
		if (Cargo.GetAmount(ResourceType) > 0 and ResourceSetups.Contains(ResourceType))
		{
			KindsToLoad.Add(ResourceType);
		}
	}
	if (KindsToLoad.IsEmpty())
	{
		return;
	}
	const float TimeForLifts = FMath::Max(Duration - CraneSettings.ReturnToRestSeconds, 0.f);
	const float LiftSeconds = FMath::Clamp(TimeForLifts / KindsToLoad.Num(), MinSecondsPerLift,
	                                       CraneSettings.MaxSecondsPerLift);
	float LiftStart = 0.f;
	for (const ERTSResourceType ResourceType : KindsToLoad)
	{
		AddLiftKeys(ResourceType, ResourceSetups[ResourceType], SupplyTruck, LiftStart, LiftSeconds);
		LiftStart += LiftSeconds;
	}
	AddKey(LiftStart + CraneSettings.ReturnToRestSeconds, MakeRestPose());
}

void USupplyPointCraneComponent::AddLiftKeys(const ERTSResourceType ResourceType,
                                             const FSupplyCraneResourceSetup& Setup,
                                             const ATankMaster* SupplyTruck, const float LiftStart,
                                             const float LiftSeconds)
{
	using namespace SupplyPointCranePrivate;
	const FTransform PickTransform = GetPickTransform(Setup);
	const FTransform BedTransform = GetBedSlotTransform(SupplyTruck, Setup);
	const float BedLoadYaw = BedTransform.Rotator().Yaw + CraneSettings.LoadYawInBed;

	const FSupplyCranePose OnPick = MakePoseOverLocation(PickTransform.GetLocation(), PickTransform.Rotator().Yaw);
	FSupplyCranePose OverPick = OnPick;
	OverPick.HookDrop = CraneSettings.RestHookDrop;
	FSupplyCranePose Lifted = OnPick;
	Lifted.HookDrop = CraneSettings.CarryHookDrop;

	const FSupplyCranePose InBed = MakePoseOverLocation(BedTransform.GetLocation(), BedLoadYaw);
	FSupplyCranePose OverBed = InBed;
	OverBed.HookDrop = CraneSettings.CarryHookDrop;
	FSupplyCranePose ClearOfBed = InBed;
	ClearOfBed.HookDrop = CraneSettings.RestHookDrop;

	AddKey(LiftStart + LiftSeconds * OverPickFraction, OverPick);
	AddKey(LiftStart + LiftSeconds * OnPickFraction, OnPick, ESupplyCraneKeyEvent::PickLoad, ResourceType);
	AddKey(LiftStart + LiftSeconds * LiftedFraction, Lifted);
	AddKey(LiftStart + LiftSeconds * OverBedFraction, OverBed);
	AddKey(LiftStart + LiftSeconds * InBedFraction, InBed, ESupplyCraneKeyEvent::ReleaseLoad, ResourceType);
	AddKey(LiftStart + LiftSeconds, ClearOfBed);
}

void USupplyPointCraneComponent::AddKey(const float Time, const FSupplyCranePose& Pose,
                                        const ESupplyCraneKeyEvent Event, const ERTSResourceType ResourceType)
{
	FSupplyCraneKey& Key = M_CycleKeys.AddDefaulted_GetRef();
	Key.Time = Time;
	Key.Pose = Pose;
	Key.Event = Event;
	Key.ResourceType = ResourceType;
}

void USupplyPointCraneComponent::TickCraneCycle(const float DeltaTime)
{
	if (not bM_IsCycleRunning or not GetHasCraneParts())
	{
		return;
	}
	M_CycleSeconds += DeltaTime;
	while (M_CycleKeys.IsValidIndex(M_NextKeyIndex) and M_CycleSeconds >= M_CycleKeys[M_NextKeyIndex].Time)
	{
		// Copy: a key event may rebuild the key list.
		const FSupplyCraneKey ReachedKey = M_CycleKeys[M_NextKeyIndex];
		++M_NextKeyIndex;
		M_CurrentPose = ReachedKey.Pose;
		FireKeyEvent(ReachedKey);
	}
	if (not M_CycleKeys.IsValidIndex(M_NextKeyIndex))
	{
		bM_IsCycleRunning = false;
		ApplyPose(M_CurrentPose);
		return;
	}
	const FSupplyCraneKey& PreviousKey = M_CycleKeys[M_NextKeyIndex - 1];
	const FSupplyCraneKey& NextKey = M_CycleKeys[M_NextKeyIndex];
	const float KeySeconds = FMath::Max(NextKey.Time - PreviousKey.Time, KINDA_SMALL_NUMBER);
	const float Alpha = FMath::Clamp((M_CycleSeconds - PreviousKey.Time) / KeySeconds, 0.f, 1.f);
	// Ease in and out: a crane does not start or stop a slew at full speed.
	M_CurrentPose = LerpPose(PreviousKey.Pose, NextKey.Pose, FMath::SmoothStep(0.f, 1.f, Alpha));
	ApplyPose(M_CurrentPose);
}

void USupplyPointCraneComponent::FireKeyEvent(const FSupplyCraneKey& Key)
{
	switch (Key.Event)
	{
	case ESupplyCraneKeyEvent::PickLoad:
		PickLoad(Key.ResourceType);
		break;
	case ESupplyCraneKeyEvent::ReleaseLoad:
		ReleaseLoadIntoTruck(Key.ResourceType);
		break;
	default:
		break;
	}
}

void USupplyPointCraneComponent::StartReturnToRest()
{
	M_CycleKeys.Reset();
	AddKey(0.f, M_CurrentPose);
	AddKey(CraneSettings.ReturnToRestSeconds, MakeRestPose());
	M_CycleSeconds = 0.f;
	M_NextKeyIndex = 1;
	bM_IsCycleRunning = true;
}

FSupplyCranePose USupplyPointCraneComponent::MakePoseOverLocation(const FVector& LoadBottomLocation,
                                                                  const float HookWorldYaw) const
{
	FSupplyCranePose Pose;
	const FVector SlewAxisLocation = M_UpperWorks->GetComponentLocation();
	const FVector ToTarget = LoadBottomLocation - SlewAxisLocation;
	const USceneComponent* SlewParent = M_UpperWorks->GetAttachParent();
	const float ParentWorldYaw = SlewParent != nullptr ? SlewParent->GetComponentRotation().Yaw : 0.f;
	const float TargetWorldYaw = FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Y, ToTarget.X));
	Pose.SlewYaw = FRotator::NormalizeAxis(TargetWorldYaw - ParentWorldYaw);
	Pose.TrolleyRadius = FMath::Clamp(ToTarget.Size2D(), CraneSettings.TrolleyMinRadius,
	                                  CraneSettings.TrolleyMaxRadius);
	const float HookPivotHeight = LoadBottomLocation.Z + CraneSettings.HookToLoadBottom;
	Pose.HookDrop = FMath::Max(M_Trolley->GetComponentLocation().Z - HookPivotHeight,
	                           SupplyPointCranePrivate::MinHookDrop);
	Pose.HookWorldYaw = HookWorldYaw;
	return Pose;
}

FSupplyCranePose USupplyPointCraneComponent::MakeRestPose() const
{
	FSupplyCranePose Pose;
	const USceneComponent* SlewParent = M_UpperWorks->GetAttachParent();
	const float ParentWorldYaw = SlewParent != nullptr ? SlewParent->GetComponentRotation().Yaw : 0.f;
	Pose.SlewYaw = FRotator::NormalizeAxis(CraneSettings.RestSlewYaw);
	Pose.TrolleyRadius = CraneSettings.RestTrolleyRadius;
	Pose.HookDrop = CraneSettings.RestHookDrop;
	// At rest the hook hangs in line with the jib.
	Pose.HookWorldYaw = ParentWorldYaw + Pose.SlewYaw;
	return Pose;
}

FSupplyCranePose USupplyPointCraneComponent::LerpPose(const FSupplyCranePose& From, const FSupplyCranePose& To,
                                                      const float Alpha)
{
	FSupplyCranePose Pose;
	Pose.SlewYaw = From.SlewYaw + FMath::FindDeltaAngleDegrees(From.SlewYaw, To.SlewYaw) * Alpha;
	Pose.TrolleyRadius = FMath::Lerp(From.TrolleyRadius, To.TrolleyRadius, Alpha);
	Pose.HookDrop = FMath::Lerp(From.HookDrop, To.HookDrop, Alpha);
	Pose.HookWorldYaw = From.HookWorldYaw + FMath::FindDeltaAngleDegrees(From.HookWorldYaw, To.HookWorldYaw) * Alpha;
	return Pose;
}

void USupplyPointCraneComponent::ApplyPose(const FSupplyCranePose& Pose)
{
	if (not GetHasCraneParts())
	{
		return;
	}
	M_UpperWorks->SetRelativeRotation(FRotator(0.f, Pose.SlewYaw, 0.f));
	FVector TrolleyLocation = M_Trolley->GetRelativeLocation();
	TrolleyLocation.X = Pose.TrolleyRadius;
	M_Trolley->SetRelativeLocation(TrolleyLocation);
	FVector HookLocation = M_HookBlock->GetRelativeLocation();
	HookLocation.Z = -Pose.HookDrop;
	M_HookBlock->SetRelativeLocation(HookLocation);
	M_HookBlock->SetWorldRotation(FRotator(0.f, Pose.HookWorldYaw, 0.f));
	if (M_Rope.IsValid())
	{
		const float RopeScale = Pose.HookDrop / FMath::Max(CraneSettings.RopeMeshLength, KINDA_SMALL_NUMBER);
		M_Rope->SetRelativeScale3D(FVector(1.f, 1.f, RopeScale));
	}
}

FTransform USupplyPointCraneComponent::GetBedSlotTransform(const ATankMaster* SupplyTruck,
                                                           const FSupplyCraneResourceSetup& Setup) const
{
	const USceneComponent* CargoSlot = SupplyPointCranePrivate::FindComponentByName<USceneComponent>(
		SupplyTruck, Setup.TruckCargoSlotName);
	if (CargoSlot != nullptr)
	{
		return CargoSlot->GetComponentTransform();
	}
	// A truck without cargo slots: aim at the middle of the truck, roughly at bed height.
	constexpr float FallbackBedHeight = 118.f;
	FTransform Fallback = SupplyTruck->GetActorTransform();
	Fallback.AddToTranslation(FVector(0.f, 0.f, FallbackBedHeight));
	return Fallback;
}

FTransform USupplyPointCraneComponent::GetPickTransform(const FSupplyCraneResourceSetup& Setup) const
{
	if (M_Base.IsValid() and M_Base->DoesSocketExist(Setup.PickSocketName))
	{
		return M_Base->GetSocketTransform(Setup.PickSocketName);
	}
	// No pick socket: lift from the stockpile itself.
	const TWeakObjectPtr<UStaticMeshComponent>* StockComponent = nullptr;
	for (const TPair<ERTSResourceType, FSupplyCraneResourceSetup>& Candidate : ResourceSetups)
	{
		if (&Candidate.Value == &Setup)
		{
			StockComponent = M_StockComponents.Find(Candidate.Key);
		}
	}
	return StockComponent != nullptr and StockComponent->IsValid()
		       ? (*StockComponent)->GetComponentTransform()
		       : GetOwner()->GetActorTransform();
}

// ---------------------------------------------------------------------------------------------------------------------
// Loads and stockpiles
// ---------------------------------------------------------------------------------------------------------------------

void USupplyPointCraneComponent::SetWaitingLoadsVisible(const bool bVisible)
{
	if (not M_SupplyPoint.IsValid())
	{
		return;
	}
	const FSupplyTruckCargo& SuppliesPerTrip = M_SupplyPoint->GetSupplyPointSettings().SuppliesPerTrip;
	for (const TPair<ERTSResourceType, TObjectPtr<UStaticMeshComponent>>& WaitingLoad : M_WaitingLoads)
	{
		if (IsValid(WaitingLoad.Value))
		{
			WaitingLoad.Value->SetVisibility(bVisible and SuppliesPerTrip.GetAmount(WaitingLoad.Key) > 0);
		}
	}
}

void USupplyPointCraneComponent::PickLoad(const ERTSResourceType ResourceType)
{
	const FSupplyCraneResourceSetup* Setup = ResourceSetups.Find(ResourceType);
	if (Setup == nullptr)
	{
		return;
	}
	if (const TObjectPtr<UStaticMeshComponent>* WaitingLoad = M_WaitingLoads.Find(ResourceType))
	{
		(*WaitingLoad)->SetVisibility(false);
	}
	if (IsValid(M_CarriedLoad))
	{
		M_CarriedLoad->SetStaticMesh(Setup->LoadMesh);
		M_CarriedLoad->SetVisibility(IsValid(Setup->LoadMesh));
	}
	SetStockPercent(ResourceType, Setup->StockLevelAfterLift);
}

void USupplyPointCraneComponent::ReleaseLoadIntoTruck(const ERTSResourceType ResourceType)
{
	if (IsValid(M_CarriedLoad))
	{
		M_CarriedLoad->SetVisibility(false);
	}
	++M_LoadsPlacedInTruck;
	const FSupplyCraneResourceSetup* Setup = ResourceSetups.Find(ResourceType);
	ATankMaster* Truck = M_TruckBeingLoaded.Get();
	if (Setup == nullptr or not IsValid(Truck) or not IsValid(Setup->LoadMesh))
	{
		return;
	}
	USceneComponent* CargoSlot = SupplyPointCranePrivate::FindComponentByName<USceneComponent>(
		Truck, Setup->TruckCargoSlotName);
	if (CargoSlot == nullptr)
	{
		return;
	}
	// The load now belongs to the truck: it rides home in the bed and goes when the truck goes.
	UStaticMeshComponent* TruckLoad = NewObject<UStaticMeshComponent>(Truck);
	TruckLoad->SetStaticMesh(Setup->LoadMesh);
	TruckLoad->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TruckLoad->SetupAttachment(CargoSlot);
	TruckLoad->SetRelativeRotation(FRotator(0.f, CraneSettings.LoadYawInBed, 0.f));
	TruckLoad->RegisterComponent();
}

void USupplyPointCraneComponent::TickStockFill(const float DeltaTime)
{
	if (not bM_IsFilling or not M_SupplyPoint.IsValid())
	{
		return;
	}
	M_FillSeconds += DeltaTime;
	const float Alpha = FMath::Clamp(M_FillSeconds / FMath::Max(M_FillDuration, KINDA_SMALL_NUMBER), 0.f, 1.f);
	const FSupplyTruckCargo& SuppliesPerTrip = M_SupplyPoint->GetSupplyPointSettings().SuppliesPerTrip;
	for (const TPair<ERTSResourceType, FSupplyCraneResourceSetup>& Setup : ResourceSetups)
	{
		const FSupplyStockFill* Fill = M_StockFills.Find(Setup.Key);
		if (Fill == nullptr or SuppliesPerTrip.GetAmount(Setup.Key) <= 0)
		{
			continue;
		}
		SetStockPercent(Setup.Key, FMath::Lerp(Fill->FillStartPercent, SupplyPointCranePrivate::FullPercent, Alpha));
	}
	bM_IsFilling = Alpha < 1.f;
}

void USupplyPointCraneComponent::SetStockPercent(const ERTSResourceType ResourceType, const float Percent)
{
	FSupplyStockFill* Fill = M_StockFills.Find(ResourceType);
	if (Fill == nullptr)
	{
		return;
	}
	Fill->Percent = Percent;
	ApplyStockMesh(ResourceType);
}

void USupplyPointCraneComponent::ApplyStockMesh(const ERTSResourceType ResourceType)
{
	const FSupplyCraneResourceSetup* Setup = ResourceSetups.Find(ResourceType);
	const TWeakObjectPtr<UStaticMeshComponent>* StockComponent = M_StockComponents.Find(ResourceType);
	const FSupplyStockFill* Fill = M_StockFills.Find(ResourceType);
	if (Setup == nullptr or StockComponent == nullptr or not StockComponent->IsValid() or Fill == nullptr)
	{
		return;
	}
	UStaticMesh* LevelMesh = nullptr;
	// Levels run from the highest to the lowest: the first one reached is the fullest that fits.
	for (const FStorageMeshLevel& Level : Setup->StockLevels)
	{
		if (Fill->Percent >= Level.Level)
		{
			LevelMesh = Level.Mesh;
			break;
		}
	}
	if ((*StockComponent)->GetStaticMesh() != LevelMesh)
	{
		(*StockComponent)->SetStaticMesh(LevelMesh);
	}
}
