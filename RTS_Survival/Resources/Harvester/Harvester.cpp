// Copyright (C) Bas Blokzijl - All rights reserved.

#include "Harvester.h"

#include "AIController.h"
#include "AITypes.h"
#include "NavigationSystem.h"
#include "HarvesterInterface/HarvesterInterface.h"
#include "HarvesterStatus/HarvesterStatus.h"
#include "Navigation/PathFollowingComponent.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/Game/GameState/GameResourceManager/GameResourceManager.h"
#include  "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Resources/ResourceDropOff/ResourceDropOff.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Resources/Harvester/HarvesterInterface/HarvesterInterface.h"
#include "RTS_Survival/Resources/ResourceComponent/ResourceComponent.h"
#include "TimerManager.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalText/Pooling/AnimatedTextWidgetPoolManager/AnimatedTextWidgetPoolManager.h"
#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

// Sets default values for this component's properties
UHarvester::UHarvester()
	: M_HarvestStatus(), M_AllowedResources(),
	  M_PlayerResourceManager(nullptr), M_AIController(nullptr),
	  bM_IsAbleToHarvest(true), M_TargetResourceType(), M_OwnerMeshComponent(nullptr), M_ResourceAcceptanceRadius(0),
	  M_HarvestingTime(0)
{
	TargetResource = nullptr;
	PrimaryComponentTick.bCanEverTick = false;
	M_OccupiedHarvestingLocation.Location = FVector::ZeroVector;
	M_OccupiedHarvestingLocation.Direction = EHarvestLocationDirection::INVALID;
}

TArray<ERTSResourceType> UHarvester::GetAllowedResourceTypes()
{
	return M_AllowedResources;
}

TWeakObjectPtr<UResourceComponent> UHarvester::GetTargetResource() const
{
	return TargetResource;
}

int32 UHarvester::GetDropOffAmount()
{
	if (M_CargoSpace.Contains(M_TargetResourceType))
	{
		return M_CargoSpace[M_TargetResourceType].CurrentAmount;
	}
	FString Name = "Owner Not Valid";
	if (const AActor* Owner = GetOwner())
	{
		Name = Owner->GetName();
	}
	RTSFunctionLibrary::ReportError("Last targeted resource type not in harvester cargo!"
		"\n Resource type: " + Global_GetResourceTypeAsString(M_TargetResourceType) +
		"Harvester: " + Name);
	return int32();
}

int32 UHarvester::SpaceLeftForTargetResource()
{
	if (M_CargoSpace.Contains(M_TargetResourceType))
	{
		return M_CargoSpace[M_TargetResourceType].MaxCapacity - M_CargoSpace[M_TargetResourceType].CurrentAmount;
	}
	return int32();
}

void UHarvester::RotateTowardsTargetLocation(const FVector& TargetLocation)
{
	AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return;
	}
	FRotator OwnerRotation = Owner->GetActorRotation();
	const FRotator TargetRotation = UKismetMathLibrary::FindLookAtRotation(GetHarvesterLocation(), TargetLocation);
	OwnerRotation.Yaw = TargetRotation.Yaw;
	Owner->SetActorRotation(OwnerRotation, ETeleportType::TeleportPhysics);
}

bool UHarvester::SetTargetResource(TWeakObjectPtr<UResourceComponent> NewTargetResource)
{
	if (NewTargetResource.IsValid() && M_CargoSpace.Contains(NewTargetResource->GetResourceType()))
	{
		TargetResource = NewTargetResource;
		M_TargetResourceType = TargetResource->GetResourceType();
		return true;
	}
	return false;
}

bool UHarvester::GetIsTargetResourceHarvestable() const
{
	return TargetResource.IsValid() && TargetResource->StillContainsResources();
}

bool UHarvester::GetHasTargetDropOffCapacity() const
{
	return M_TargetDropOff.IsValid() && M_TargetDropOff->GetIsDropOffActive() && M_TargetDropOff->GetDropOffCapacity(
		M_TargetResourceType) > 0;
}

FVector UHarvester::GetHarvesterLocation() const
{
	if (const AActor* Owner = GetOwner())
	{
		return Owner->GetActorLocation();
	}
	return FVector(0.f, 0.f, 0.f);
}

bool UHarvester::GetIsCargoFull(const ERTSResourceType ResourceType) const
{
	if (M_CargoSpace.Contains(ResourceType))
	{
		return M_CargoSpace[ResourceType].IsFull();
	}
	FString Name = "Owner Not Valid";
	if (const AActor* Owner = GetOwner())
	{
		Name = Owner->GetName();
	}
	RTSFunctionLibrary::ReportError("Provided resource type is not in harvester cargo!"
		"\n Resource type: " + Global_GetResourceTypeAsString(ResourceType) +
		"Harvester: " + Name);
	return false;
}

bool UHarvester::AddHarvestedResourcesToCargo(const int32 Amount, const ERTSResourceType ResourceType)
{
	if (M_CargoSpace.Contains(ResourceType))
	{
		if (M_CargoSpace[ResourceType].MaxCapacity < M_CargoSpace[ResourceType].CurrentAmount + Amount)
		{
			M_CargoSpace[ResourceType].CurrentAmount = M_CargoSpace[ResourceType].MaxCapacity;
			return true;
		}
		M_CargoSpace[ResourceType].AddResource(Amount);
		if (M_IOwnerHarvesterInterface.IsValid())
		{
			M_IOwnerHarvesterInterface->OnResourceStorageChanged(GetPercentageCapacityFilled(ResourceType), ResourceType);
		}
		return M_CargoSpace[ResourceType].IsFull();
	}
	FString Name = "Owner Not Valid";
	if (const AActor* Owner = GetOwner())
	{
		Name = Owner->GetName();
	}
	RTSFunctionLibrary::ReportError(
		"Attempted to add harvester cargo for a resource type this harvester does not support!"
		"Resource type: " + Global_GetResourceTypeAsString(ResourceType) +
		"Harvester: " + Name);
	return true;
}

void UHarvester::ResetHarvesterTargets()
{
	M_TargetResourceType = ERTSResourceType::Resource_None;
	M_TargetDropOff = nullptr;
	TargetResource = nullptr;
	M_TargetResourceTypeAfterDropOff = ERTSResourceType::Resource_None;
}

int32 UHarvester::GetPercentageCapacityFilled(const ERTSResourceType ResourceType) const
{
	if (M_CargoSpace.Contains(ResourceType))
	{
		return (M_CargoSpace[ResourceType].CurrentAmount * 100) / M_CargoSpace[ResourceType].MaxCapacity;
	}
	return int32();
}

bool UHarvester::CheckHasOtherCargo(TWeakObjectPtr<UResourceComponent> NewTargetResource)
{
	if (not NewTargetResource.IsValid())
	{
		return false;
	}
	for (auto EachResoruce : M_CargoSpace)
	{
		if (EachResoruce.Value.ResourceType != NewTargetResource->GetResourceType() && EachResoruce.Value.CurrentAmount > 0)
		{
			TargetResource = NewTargetResource;
			M_TargetResourceType = EachResoruce.Value.ResourceType;
			M_TargetResourceTypeAfterDropOff = NewTargetResource->GetResourceType();
			HarvestAIExecuteAction(EHarvesterAIAction::AsyncFindDropOff);
			return true;
		}
	}
	return false;
}

void UHarvester::ResetHarvesterLocation()
{
	if (M_OccupiedHarvestingLocation.Location.IsNearlyZero())
	{
		return;
	}
	if (TargetResource.IsValid())
	{
		TargetResource->RegisterOccupiedLocation(M_OccupiedHarvestingLocation, false);
	}
	M_OccupiedHarvestingLocation.Direction = EHarvestLocationDirection::INVALID;
	M_OccupiedHarvestingLocation.Location = FVector::ZeroVector;
}

void UHarvester::OnCargoNotFullAfterHarvesting()
{
	const EResourceValidation Validation = GetCanHarvestResource();
	if (Validation == EResourceValidation::IsValidForHarvest)
	{
		HarvestAIExecuteAction(EHarvesterAIAction::PlayHarvestAnimation);
		return;
	}
	ResetHarvesterLocation();
	const EHarvesterAIAction Action = ResourceValidationIntoAction(Validation);
	HarvestAIExecuteAction(Action);
}

void UHarvester::SetupPlayerResourceManagerReference(const UWorld* World)
{
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (!IsValid(PlayerController))
	{
		return;
	}
	const ACPPController* CPPController = Cast<ACPPController>(PlayerController);
	if (!IsValid(CPPController))
	{
		RTSFunctionLibrary::ReportNullErrorInitialisation(this,
		                                                  "PlayerController",
		                                                  "SetupPlayerResourceManagerReference");
		return;
	}
	M_PlayerResourceManager = CPPController->GetPlayerResourceManager();
	if (!IsValid(M_PlayerResourceManager))
	{
		RTSFunctionLibrary::ReportNullErrorInitialisation(this,
		                                                  "PlayerResourceManager",
		                                                  "SetupPlayerResourceManagerReference");
	}
}

void UHarvester::SetupGameResourceManagerReference(const UWorld* World)
{
	AGameStateBase* GameState = World->GetGameState();
	if (!IsValid(GameState))
	{
		RTSFunctionLibrary::ReportError("Gamestate is null at SetupGameResourceManagerReference in Harvester"
			"\n Harvester: " + GetName());
		return;
	}
	ACPPGameState* CPPGameState = Cast<ACPPGameState>(GameState);
	if (!IsValid(CPPGameState))
	{
		RTSFunctionLibrary::ReportFailedCastError("GameState", "CPPGameState",
		                                          "UHarvester::SetupGameResourceManagerReference");
		return;
	}
	M_GameResourceManager = CPPGameState->GetGameResourceManager();
	if (!IsValid(M_GameResourceManager))
	{
		RTSFunctionLibrary::ReportError("Harvester could not get access to GameResourceManager"
			" in SetupGameResourceManagerReference"
			"\n Harvester: " + GetName());
	}
}

void UHarvester::OnHarvestedAddReturnCargoAbility() const
{
	if (not M_CommandOwner.IsValid() || M_CommandOwner->HasAbility(EAbilityID::IdReturnCargo))
	{
		return;
	}
	M_CommandOwner->AddAbility(EAbilityID::IdReturnCargo);
}

void UHarvester::OnCargoEmptyRemoveReturnCargoAbility() const
{
	if (not M_CommandOwner.IsValid() || not M_CommandOwner->HasAbility(EAbilityID::IdReturnCargo))
	{
		return;
	}
	M_CommandOwner->RemoveAbility(EAbilityID::IdReturnCargo);
}

bool UHarvester::GetIsHarvesterIdle() const
{
	if (not M_CommandOwner.IsValid() || M_CommandOwner->GetActiveCommandID() != EAbilityID::IdIdle)
	{
		return false;
	}
	return true;
}

void UHarvester::CheckHarvesterIdle()
{
	if (!GetIsHarvesterIdle())
	{
		return;
	}
	const ERTSResourceType MostFullType = GetResourceTypeOfMostFullCargo();
	if (MostFullType != ERTSResourceType::Resource_None)
	{
		M_CommandOwner->ReturnCargo(true);
		return;
	}
	if (M_TargetResourceType != ERTSResourceType::Resource_None)
	{
		IdleHarvester_AsyncFindTargetResource();
	}
}

void UHarvester::IdleHarvester_AsyncFindTargetResource()
{
	if (not IsValid(M_GameResourceManager))
	{
		return;
	}
	TWeakObjectPtr<UHarvester> WeakThis(this);
	M_GameResourceManager->AsyncRequestClosestResource(
		GetHarvesterLocation(),
		8,
		M_TargetResourceType,
		[WeakThis](const TArray<TWeakObjectPtr<UResourceComponent>>& Resources)
		{
			if (not WeakThis.IsValid())
			{
				return;
			}
			if (const UHarvester* This = WeakThis.Get())
			{
				This->AsyncOnReceiveResourceForIdle(Resources);
			}
		}
	);
	HarvestDebug("IDLE Started async request to find closest resource!", FColor::Green);
}

void UHarvester::AsyncOnReceiveResourceForIdle(TArray<TWeakObjectPtr<UResourceComponent>> Resources) const
{
	if (!GetIsHarvesterIdle())
	{
		return;
	}
	for (const TWeakObjectPtr<UResourceComponent>& Resource : Resources)
	{
		if (!Resource.IsValid())
		{
			continue;
		}
		// NEW: respect blacklists even for idle auto-pick
		if (M_ResourceBlacklist.Contains(Resource))
		{
			continue;
		}
		M_CommandOwner->HarvestResource(Resource.Get()->GetOwner(), true);
		return;
	}
}

void UHarvester::ExecuteHarvestResourceCommand(UResourceComponent* NewTargetResource)
{
	ResetHarvesterTargets();
	if (CheckHasOtherCargo(NewTargetResource))
	{
		return;
	}
	if (SetTargetResource(NewTargetResource))
	{
		const EResourceValidation Validation = GetCanHarvestResource();
		HarvestDebug("Start validation: " + GetStringResourceValidation(Validation), FColor::Green);
		const EHarvesterAIAction Action = ResourceValidationIntoAction(Validation);
		HarvestDebug("Start action: " + GetStringHarvesterAIStatus(Action), FColor::Green);
		HarvestAIExecuteAction(Action);
		return;
	}
	HarvestAIExecuteAction(EHarvesterAIAction::FinishHarvestCommand);
	HarvestDebug(
		"Terminate harvest command immediately as resource is not valid or resource type not in cargo space",
		FColor::Red);
}

void UHarvester::ExecuteReturnCargoCommand()
{
	const ERTSResourceType MostFullType = GetResourceTypeOfMostFullCargo();
	if (MostFullType == ERTSResourceType::Resource_None)
	{
		FinishReturnCargoCommand();
		return;
	}
	M_TargetResourceType = MostFullType;
	HarvestAIExecuteAction(EHarvesterAIAction::AsyncFindDropOff);
}

void UHarvester::FinishHarvestCommand() const
{
	if (M_CommandOwner.IsValid())
	{
		M_CommandOwner->DoneExecutingCommand(EAbilityID::IdHarvestResource);
	}
}

void UHarvester::FinishReturnCargoCommand() const
{
	if (M_CommandOwner.IsValid())
	{
		M_CommandOwner->DoneExecutingCommand(EAbilityID::IdReturnCargo);
	}
}

void UHarvester::OnHarvestAnimationFinished()
{
	const EResourceValidation Validation = GetCanHarvestResource();
	if (Validation == EResourceValidation::IsValidForHarvest)
	{
		HarvestAIExecuteAction(EHarvesterAIAction::HarvestResource);
		return;
	}
	ResetHarvesterLocation();
	const EHarvesterAIAction Action = ResourceValidationIntoAction(Validation);
	HarvestDebug(
		"Animation finished Harvest action as resource could not be harvested: " +
		GetStringHarvesterAIStatus(Action), FColor::Orange);
	HarvestAIExecuteAction(Action);
}

void UHarvester::TerminateHarvestCommand()
{
	switch (M_HarvestStatus)
	{
	case EHarvesterAIAction::NoHarvestingAction:
		return;
	case EHarvesterAIAction::MoveToResource:
	case EHarvesterAIAction::MoveToDropOff:
	case EHarvesterAIAction::UnstuckTowardsResource:
	case EHarvesterAIAction::UnstuckTowardsDropOff:
		if (GetIsValidAIController())
		{
			if (UPathFollowingComponent* PathFollowingComponent = M_AIController->GetPathFollowingComponent())
			{
				PathFollowingComponent->OnRequestFinished.RemoveAll(this);
			}
			M_AIController->StopMovement();
		}
		break;
	case EHarvesterAIAction::PlayHarvestAnimation:
		if (M_IOwnerHarvesterInterface.IsValid())
		{
			M_IOwnerHarvesterInterface->Execute_StopHarvesterAnimation(GetOwner());
		}
		break;
	case EHarvesterAIAction::AsyncFindResource:
	case EHarvesterAIAction::AsyncFindDropOff:
		if (GetIsValidAIController())
		{
			if (UPathFollowingComponent* PathFollowingComponent = M_AIController->GetPathFollowingComponent())
			{
				PathFollowingComponent->OnRequestFinished.RemoveAll(this);
			}
			M_AIController->StopMovement();
		}
		break;
	case EHarvesterAIAction::HarvestResource:
	case EHarvesterAIAction::DropOff:
	case EHarvesterAIAction::FinishHarvestCommand:
		break;
	}
	HarvestDebug("Harvester terminated logic: " + GetStringHarvesterAIStatus(M_HarvestStatus), FColor::Red);
	ResetHarvesterLocation();
	M_HarvestStatus = EHarvesterAIAction::NoHarvestingAction;
}

int32 UHarvester::GetMaxCapacityForTargetResource(int32& OutCurrentAmount, ERTSResourceType& OutResource) const
{
	if (M_CargoSpace.Contains(M_TargetResourceType))
	{
		OutCurrentAmount = M_CargoSpace[M_TargetResourceType].CurrentAmount;
		OutResource = M_TargetResourceType;
		return M_CargoSpace[M_TargetResourceType].MaxCapacity;
	}
	for (const auto& CargoSlot : M_CargoSpace)
	{
		if (CargoSlot.Value.MaxCapacity != 0)
		{
			OutCurrentAmount = CargoSlot.Value.CurrentAmount;
			OutResource = CargoSlot.Value.ResourceType;
			return CargoSlot.Value.MaxCapacity;
		}
	}
	const FString OwnerName = IsValid(GetOwner()) ? GetOwner()->GetName() : "NULL";
	RTSFunctionLibrary::ReportError("Could not get any non-zero cargo space for any resource in harvester: " + GetName()
		+ "Of owner: " + OwnerName);
	OutCurrentAmount = 0;
	return 0;
}

void UHarvester::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_SetupAnimatedTextWidgetPoolManager();

	if (const UWorld* World = GetWorld())
	{
		SetupPlayerResourceManagerReference(World);
		SetupGameResourceManagerReference(World);
		FTimerDelegate Delegate;
		Delegate.BindUObject(this, &UHarvester::CheckHarvesterIdle);
		World->GetTimerManager().SetTimer(M_CheckHarvesterIdleTimer, Delegate,
		                                  DeveloperSettings::GamePlay::Resources::HarvesterIdleCheckInterval, true,
		                                  0.f);

		// NEW: clear blacklists every 30 seconds
		World->GetTimerManager().SetTimer(
			M_BlacklistClearTimer, this, &UHarvester::ClearHarvestBlacklists, 30.f, true, 30.f);
	}
	M_CommandOwner = Cast<ICommands>(GetOwner());
	if (!M_CommandOwner.IsValid())
	{
		RTSFunctionLibrary::ReportNullErrorInitialisation(this, "ICommands", "BeginPlay");
	}
}

void UHarvester::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_CheckHarvesterIdleTimer);
		World->GetTimerManager().ClearTimer(M_BlacklistClearTimer);
	}
}

void UHarvester::SetupHarvester(
	const TArray<ERTSResourceType> NewAllowedResources,
	TMap<ERTSResourceType, int32> MaxCargoPerResource,
	const float AcceptanceRadiusResource,
	UMeshComponent* OwnerMeshComponent, const float HarvesterRadius)
{
	M_AllowedResources = NewAllowedResources;
	for (ERTSResourceType ResourceType : M_AllowedResources)
	{
		if (MaxCargoPerResource.Contains(ResourceType))
		{
			FHarvesterCargoSlot CargoSlot;
			CargoSlot.ResourceType = ResourceType;
			CargoSlot.MaxCapacity = MaxCargoPerResource[ResourceType];
			CargoSlot.CurrentAmount = 0;
			M_CargoSpace.Add(ResourceType, CargoSlot);
		}
		else
		{
			RTSFunctionLibrary::ReportErrorVariableNotInitialised(this,
			                                                      "MaxCargoPerResource for M_ResourceType: " +
			                                                      Global_GetResourceTypeAsString(ResourceType),
			                                                      "SetupHarvester",
			                                                      this->GetOwner());
		}
	}
	if (AcceptanceRadiusResource <= 0)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "AcceptanceRadiusResource",
		                                                      "SetupHarvester", this->GetOwner());
		M_ResourceAcceptanceRadius = 100.f;
	}
	else
	{
		M_ResourceAcceptanceRadius = AcceptanceRadiusResource;
	}
	IHarvesterInterface* OwnerAsHarvesterInterface = Cast<IHarvesterInterface>(GetOwner());
	if (OwnerAsHarvesterInterface)
	{
		M_IOwnerHarvesterInterface = TWeakInterfacePtr<IHarvesterInterface>(OwnerAsHarvesterInterface);
	}
	else
	{
		RTSFunctionLibrary::ReportFailedCastError("Owner", "IHarvesterInterface", "SetupHarvester");
	}
	M_AIController = Cast<AAIController>(Cast<APawn>(GetOwner())->GetController());
	if (!IsValid(M_AIController))
	{
		RTSFunctionLibrary::ReportNullErrorInitialisation(this, "AIController", "SetupHarvester");
	}
	if (IsValid(OwnerMeshComponent))
	{
		M_OwnerMeshComponent = OwnerMeshComponent;
		M_OwnerMeshComponent->SetGenerateOverlapEvents(true);
		M_OwnerMeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Overlap);
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorInitialisation(this, "OwnerMeshComponent", "SetupHarvester");
	}
	if (HarvesterRadius <= 0)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "HarvesterRadius",
		                                                      "SetupHarvester", this->GetOwner());
		M_HarvesterRadius = 100.f;
	}
	else
	{
		M_HarvesterRadius = HarvesterRadius;
	}
}

void UHarvester::HarvestAIExecuteAction(const EHarvesterAIAction Action)
{
	M_HarvestStatus = Action;
	switch (Action)
	{
	case EHarvesterAIAction::MoveToResource:
		HarvestAIAction_MoveToTargetResource();
		break;
	case EHarvesterAIAction::PlayHarvestAnimation:
		// Success path → clear one-teleport marker for this goal to avoid stale entries.
		if (TargetResource.IsValid())
		{
			M_TeleportedOnce_Resource.Remove(TargetResource);
		}
		HarvestAIAction_PlayHarvesterAnimation();
		break;
	case EHarvesterAIAction::HarvestResource:
		HarvestAIAction_HarvestTargetResource();
		break;
	case EHarvesterAIAction::AsyncFindDropOff:
		HarvestAIAction_AsyncGetDropOff();
		break;
	case EHarvesterAIAction::MoveToDropOff:
		HarvestAIAction_MoveToDropOff();
		break;
	case EHarvesterAIAction::DropOff:
		// Success path → clear one-teleport marker for this goal to avoid stale entries.
		if (M_TargetDropOff.IsValid())
		{
			M_TeleportedOnce_DropOff.Remove(M_TargetDropOff);
		}
		HarvestAIAction_DropOffResources();
		break;
	case EHarvesterAIAction::AsyncFindResource:
		HarvestAIAction_AsyncGetResource();
		break;
	case EHarvesterAIAction::FinishHarvestCommand:
		FinishHarvestCommand();
		break;
	case EHarvesterAIAction::UnstuckTowardsResource:
	case EHarvesterAIAction::UnstuckTowardsDropOff:
		RTSFunctionLibrary::ReportError("Harvester attempted to execute UnstuckTorResource from AIAction"
			"\n This should only happen after a failed movement attempt and not in"
			"function : HarvestAIExecuteAction");
		break;
	case EHarvesterAIAction::NoHarvestingAction:
		break;
	}
}

EResourceValidation UHarvester::GetCanHarvestResource()
{
	if (TargetResource.IsValid())
	{
		if (!GetIsCargoFull(M_TargetResourceType))
		{
			if (TargetResource->StillContainsResources())
			{
				return EResourceValidation::IsValidForHarvest;
			}
			return EResourceValidation::ResourceEmpty;
		}
		return EResourceValidation::CargoFull;
	}
	return EResourceValidation::ResourceNotValid;
}

EHarvesterAIAction UHarvester::ResourceValidationIntoAction(EResourceValidation ResourceValidation)
{
	switch (ResourceValidation)
	{
	case EResourceValidation::IsValidForHarvest:
		return EHarvesterAIAction::MoveToResource;
	case EResourceValidation::ResourceEmpty:
		return EHarvesterAIAction::AsyncFindResource;
	case EResourceValidation::CargoFull:
		return EHarvesterAIAction::AsyncFindDropOff;
	case EResourceValidation::ResourceNotValid:
		{
			const ERTSResourceType MostFullType = GetResourceTypeOfMostFullCargo();
			if (MostFullType == ERTSResourceType::Resource_None)
			{
				return EHarvesterAIAction::AsyncFindResource;
			}
			M_TargetResourceType = MostFullType;
			return EHarvesterAIAction::MoveToDropOff;
		}
	}
	HarvestDebug("Something went wrong in ResourceValidationIntoAction"
	             "\n Switch does not return properly", FColor::Red);
	return EHarvesterAIAction::FinishHarvestCommand;
}

ERTSResourceType UHarvester::GetResourceTypeOfMostFullCargo()
{
	int32 MostFullAmount = 0;
	ERTSResourceType MostFullType = ERTSResourceType::Resource_None;
	for (const auto& CargoSlot : M_CargoSpace)
	{
		if (CargoSlot.Value.CurrentAmount > MostFullAmount)
		{
			MostFullAmount = CargoSlot.Value.CurrentAmount;
			MostFullType = CargoSlot.Value.ResourceType;
		}
	}
	return MostFullType;
}

void UHarvester::HarvestAIAction_MoveToTargetResource()
{
	const FVector HarvesterLocation = GetHarvesterLocation();
	if (not GetIsValidAIController() || not TargetResource.IsValid())
	{
		HarvestDebug("Failed to move to resource, cancelling harvesting command...", FColor::Red);
		HarvestAIExecuteAction(EHarvesterAIAction::FinishHarvestCommand);
		return;
	}

	if (TargetResource->IsResourceFullyOccupiedByHarvesters())
	{
		OnUnableToMoveToResource(ECannotMoveToResource::ResourceFullyOccupied);
		return;
	}
	if (not TargetResource->GetHarvestLocationClosestTo(HarvesterLocation, this, M_HarvesterRadius,
	                                                    M_OccupiedHarvestingLocation))
	{
		OnUnableToMoveToResource(ECannotMoveToResource::CannotProjectLocation);
		return;
	}

	const EPathFollowingRequestResult::Type MoveResult = M_AIController->MoveToLocation(
		M_OccupiedHarvestingLocation.Location, M_ResourceAcceptanceRadius);
	if (MoveResult == EPathFollowingRequestResult::Type::RequestSuccessful)
	{
		if (UPathFollowingComponent* PathFollowingComponent = M_AIController->GetPathFollowingComponent())
		{
			HarvestDebug("Successfully made request to move to resource", FColor::Green);
			PathFollowingComponent->OnRequestFinished.AddUObject(
				this, &UHarvester::OnMoveToTargetResourceFinished);
			TargetResource->RegisterOccupiedLocation(M_OccupiedHarvestingLocation, true);
			return;
		}
	}
	else if ((FVector::Distance(HarvesterLocation, M_OccupiedHarvestingLocation.Location) < M_ResourceAcceptanceRadius))
	{
		HarvestDebug("Move request to resource failed but Harvester is close enough!", FColor::Orange);
		OnMoveToTargetResourceFinished(FAIRequestID(), FPathFollowingResult(EPathFollowingResult::Success));
		return;
	}
	HarvestDebug("Not close enough: Harvester failed to REQUEST move to RESOURCE!"
	             "\n Unstuck...", FColor::Orange);
	UnstuckHarvesterTowardsLocation(HarvesterLocation,
	                                M_OccupiedHarvestingLocation.Location,
	                                EHarvesterAIAction::MoveToResource);
}

void UHarvester::OnUnableToMoveToResource(const ECannotMoveToResource Reason)
{
	if constexpr (DeveloperSettings::Debugging::GHarvestResources_Compile_DebugSymbols)
	{
		const FString ReasonString = Global_GetCannotMoveToResourceAsString(Reason);
		HarvestDebug("Harvester cannot move to resource: " + ReasonString, FColor::Orange);
		if (not M_NotReachableResources.Contains(TargetResource))
		{
			M_NotReachableResources.Add(TargetResource);
		}
	}
	HarvestAIExecuteAction(EHarvesterAIAction::AsyncFindResource);
}

void UHarvester::OnMoveToTargetResourceFinished(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	if (M_HarvestStatus == NoHarvestingAction)
	{
		return;
	}
	if (GetIsValidAIController())
	{
		if (UPathFollowingComponent* PathFollowingComponent = M_AIController->GetPathFollowingComponent())
		{
			PathFollowingComponent->OnRequestFinished.RemoveAll(this);
		}
	}
	if (!GetIsTargetResourceHarvestable())
	{
		HarvestDebug("Resource not valid or empty, find new one!", FColor::Cyan);
		HarvestAIExecuteAction(EHarvesterAIAction::AsyncFindResource);
		return;
	}
	const FVector HarvesterLocation = GetHarvesterLocation();
	if (Result.Code != EPathFollowingResult::Success)
	{
		const float AcceptanceRadius = M_ResourceAcceptanceRadius;
		if (FVector::Distance(HarvesterLocation, M_OccupiedHarvestingLocation.Location) < AcceptanceRadius)
		{
			HarvestDebug("Harvester failed to reach resource by request but is close enough!", FColor::Orange);
		}
		else
		{
			// NEW: Try a real unstuck first (to a projected midpoint), not immediate teleport.
			ResetHarvesterLocation(); // free the spot before trying recovery
			HarvestDebug("Move to resource failed; attempt UNSTUCK (no instant teleport)", FColor::Orange);
			UnstuckHarvesterTowardsLocation(HarvesterLocation,
			                                M_OccupiedHarvestingLocation.Location,
			                                EHarvesterAIAction::MoveToResource);
			return;
		}
	}
	HarvestAIExecuteAction(EHarvesterAIAction::PlayHarvestAnimation);
}

void UHarvester::HarvestAIAction_PlayHarvesterAnimation()
{
	if (M_IOwnerHarvesterInterface.IsValid())
	{
		if (TargetResource.IsValid())
		{
			RotateTowardsTargetLocation(TargetResource->GetResourceLocationNotThreadSafe());
		}
		M_IOwnerHarvesterInterface->Execute_PlayHarvesterAnimation(GetOwner());
	}
	else
	{
		HarvestDebug("Harvester interface not valid, cannot play harvesting animation", FColor::Red);
		HarvestAIExecuteAction(EHarvesterAIAction::FinishHarvestCommand);
	}
}

void UHarvester::HarvestAIAction_AsyncGetResource()
{
	if (IsValid(M_GameResourceManager))
	{
		TWeakObjectPtr<UHarvester> WeakThis(this);
		M_GameResourceManager->AsyncRequestClosestResource(
			GetHarvesterLocation(),
			32,
			M_TargetResourceType,
			[WeakThis](const TArray<TWeakObjectPtr<UResourceComponent>>& Resources)
			{
				if (not WeakThis.IsValid())
				{
					return;
				}
				if (UHarvester* This = WeakThis.Get())
				{
					This->AsyncOnReceiveResource(Resources);
				}
			}
		);
		HarvestDebug("Started async request to find closest resource!", FColor::Green);
	}
	else
	{
		HarvestDebug(
			"HarvestAIAction_AsyncStartRequestGetResource:: No valid game resource manager; terminate harvest command",
			FColor::Red);
		HarvestAIExecuteAction(EHarvesterAIAction::FinishHarvestCommand);
	}
}

void UHarvester::AsyncOnReceiveResource(TArray<TWeakObjectPtr<UResourceComponent>> Resources)
{
	if (M_HarvestStatus != EHarvesterAIAction::AsyncFindResource)
	{
		return;
	}
	for (auto EachFoundResource : Resources)
	{
		if (!EachFoundResource.IsValid())
		{
			continue;
		}
		if (M_NotReachableResources.Contains(EachFoundResource) || M_ResourceBlacklist.Contains(EachFoundResource))
		{
			continue;
		}
		if (EachFoundResource->GetResourceType() == M_TargetResourceType && EachFoundResource->StillContainsResources()
			&& !EachFoundResource->IsResourceFullyOccupiedByHarvesters())
		{
			SetTargetResource(EachFoundResource);
			HarvestDebug("Found valid resource to harvest!", FColor::Green);
			HarvestAIExecuteAction(EHarvesterAIAction::MoveToResource);
			return;
		}
	}
	if (M_CargoSpace.Contains(M_TargetResourceType) && M_CargoSpace[M_TargetResourceType].CurrentAmount > 0)
	{
		HarvestDebug("Harvester could not find valid resource but has resources in cargo; find drop off",
		             FColor::Orange);
		HarvestAIExecuteAction(EHarvesterAIAction::AsyncFindDropOff);
		return;
	}
	HarvestDebug("NO Resource found and nothing to drop off; cancelling harvesting command", FColor::Red);
	HarvestAIExecuteAction(EHarvesterAIAction::FinishHarvestCommand);
}

void UHarvester::HarvestAIAction_AsyncGetDropOff()
{
	if (not IsValid(M_GameResourceManager))
	{
		HarvestDebug(
			"HarvestAIAction_AsyncStartRequestGetDropOff:: No valid game resource manager; terminate harvest command",
			FColor::Red);
		HarvestAIExecuteAction(EHarvesterAIAction::FinishHarvestCommand);
		return;
	}

	TWeakObjectPtr<UHarvester> WeakThis(this);
	M_GameResourceManager->AsyncRequestClosestDropOffs(
		GetHarvesterLocation(),
		8,
		1,
		GetDropOffAmount(),
		M_TargetResourceType,
		[WeakThis](const TArray<TWeakObjectPtr<UResourceDropOff>>& DropOffs)
		{
			if (not WeakThis.IsValid())
			{
				return;
			}
			if (UHarvester* This = WeakThis.Get())
			{
				This->AsyncOnReceivedDropOffs(DropOffs);
			}
		}
	);
	HarvestDebug("Started async request to find closest drop-off!", FColor::Green);
}

void UHarvester::AsyncOnReceivedDropOffs(const TArray<TWeakObjectPtr<UResourceDropOff>>& DropOffs)
{
	if (M_HarvestStatus != EHarvesterAIAction::AsyncFindDropOff)
	{
		return;
	}
	for (auto EachDropOff : DropOffs)
	{
		OnNoDropOffsFound_DisplayMessage();		
		if (!EachDropOff.IsValid() || !EachDropOff->GetIsDropOffActive())
		{
			continue;
		}
		if (M_DropOffBlacklist.Contains(EachDropOff))
		{
			continue;
		}
		M_TargetDropOff = EachDropOff;
		HarvestAIExecuteAction(EHarvesterAIAction::MoveToDropOff);
		return;
	}
	HarvestDebug("No valid drop-off available; terminate harvest command", FColor::Red);
	HarvestAIExecuteAction(EHarvesterAIAction::FinishHarvestCommand);
}

void UHarvester::HarvestAIAction_MoveToDropOff()
{
	if (not GetIsValidAIController())
	{
		HarvestDebug("Failed to move to drop off, cancelling harvesting command...", FColor::Red);
		HarvestAIExecuteAction(EHarvesterAIAction::FinishHarvestCommand);
		return;
	}
	if (M_TargetDropOff.IsValid())
	{
		const FVector HarvesterLocation = GetHarvesterLocation();
		const FVector TargetLocation = M_TargetDropOff->GetDropOffLocationNotThreadSafe();
		const float AcceptanceRadius = M_ResourceAcceptanceRadius;

		const EPathFollowingRequestResult::Type MoveResult = M_AIController->MoveToLocation(
			TargetLocation, AcceptanceRadius);
		if (MoveResult == EPathFollowingRequestResult::RequestSuccessful)
		{
			if (UPathFollowingComponent* PathFollowingComponent = M_AIController->GetPathFollowingComponent())
			{
				PathFollowingComponent->OnRequestFinished.
				                        AddUObject(this, &UHarvester::OnMoveToDropOffFinished);
			}
		}
		else if ((FVector::Distance(HarvesterLocation, TargetLocation) < AcceptanceRadius))
		{
			HarvestDebug("Move request to DropOff failed but harver is close enough!", FColor::Orange);
			OnMoveToDropOffFinished(FAIRequestID(), FPathFollowingResult(EPathFollowingResult::Success));
		}
		else
		{
			HarvestDebug("Harvester failed to REQUEST move to DROPOFF and is not close enough!"
			             "\n Unstuck...", FColor::Orange);
			UnstuckHarvesterTowardsLocation(HarvesterLocation,
			                                TargetLocation,
			                                EHarvesterAIAction::MoveToDropOff);
		}
	}
}

void UHarvester::OnMoveToDropOffFinished(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	if (M_HarvestStatus == NoHarvestingAction)
	{
		return;
	}
	if (GetIsValidAIController())
	{
		if (UPathFollowingComponent* PathFollowingComponent = M_AIController->GetPathFollowingComponent())
		{
			PathFollowingComponent->OnRequestFinished.RemoveAll(this);
		}
	}

	if (!GetHasTargetDropOffCapacity())
	{
		HarvestDebug("Drop off not valid or FULL, find new one!", FColor::Cyan);
		HarvestAIExecuteAction(EHarvesterAIAction::AsyncFindDropOff);
		return;
	}

	if (Result.Code == EPathFollowingResult::Success)
	{
		HarvestDebug("at drop off location; Drop off resources", FColor::Green);
		HarvestAIExecuteAction(EHarvesterAIAction::DropOff);
		return;
	}

	const FVector HarvesterLocation = GetHarvesterLocation();
	const FVector TargetLocation = M_TargetDropOff->GetDropOffLocationNotThreadSafe();
	if (FVector::Distance(HarvesterLocation, TargetLocation) < M_ResourceAcceptanceRadius)
	{
		HarvestDebug("Harvester failed to reach drop of by request but is close enough!", FColor::Orange);
		HarvestAIExecuteAction(EHarvesterAIAction::DropOff);
		return;
	}
	// NEW: Try proper unstuck first; only teleport if allowed once for this goal.
	HarvestDebug("Move to drop-off failed; attempt UNSTUCK (no instant teleport)", FColor::Orange);
	UnstuckHarvesterTowardsLocation(HarvesterLocation,
	                                TargetLocation,
	                                EHarvesterAIAction::MoveToDropOff);
}

void UHarvester::HarvestAIAction_HarvestTargetResource()
{
	if (not TargetResource.IsValid())
	{
		HarvestDebug("On harvesting resource: no valid resource, find new one...", FColor::Orange);
		HarvestAIExecuteAction(EHarvesterAIAction::AsyncFindResource);
	}

	const int32 AmountHarvested = TargetResource->HarvestResource(SpaceLeftForTargetResource());
	if (AmountHarvested > 0)
	{
		OnHarvestedAddReturnCargoAbility();
	}
	if (AddHarvestedResourcesToCargo(AmountHarvested, M_TargetResourceType))
	{
		HarvestDebug("Cargo is full! Find drop off", FColor::Green);
		ResetHarvesterLocation();
		HarvestAIExecuteAction(EHarvesterAIAction::AsyncFindDropOff);
		return;
	}
	HarvestDebug("Cargo not full!", FColor::Green);
	OnCargoNotFullAfterHarvesting();
}

void UHarvester::HarvestAIAction_DropOffResources()
{
	if (not M_TargetDropOff.IsValid())
	{
		HarvestDebug("Drop off not valid at dropoff action!, find new one...", FColor::Orange);
		HarvestAIExecuteAction(EHarvesterAIAction::AsyncFindDropOff);
		return;
	}
	RotateTowardsTargetLocation(M_TargetDropOff->GetDropOffLocationNotThreadSafe());
	const int32 AmountLeft = M_TargetDropOff->DropOffResources(M_TargetResourceType, GetDropOffAmount());
	if (M_CargoSpace.Contains(M_TargetResourceType))
	{
		M_CargoSpace[M_TargetResourceType].CurrentAmount = AmountLeft;
		if (M_IOwnerHarvesterInterface.IsValid())
		{
			M_IOwnerHarvesterInterface->OnResourceStorageChanged(GetPercentageCapacityFilled(M_TargetResourceType),
			                                                     M_TargetResourceType);
		}
	}
	HarvestDebug("Amount left after drop off: " + FString::FromInt(AmountLeft), FColor::Green);
	if (AmountLeft)
	{
		HarvestDebug("Positive amount left after drop off, find new drop off location...", FColor::Orange);
		HarvestAIExecuteAction(EHarvesterAIAction::AsyncFindDropOff);
	}
	else
	{
		OnDroppedOffAllTargetResources();
	}
}

void UHarvester::OnDroppedOffAllTargetResources()
{
	OnCargoEmptyRemoveReturnCargoAbility();
	if (M_TargetResourceTypeAfterDropOff != ERTSResourceType::Resource_None)
	{
		M_TargetResourceType = M_TargetResourceTypeAfterDropOff;
		M_TargetResourceTypeAfterDropOff = ERTSResourceType::Resource_None;
		HarvestDebug("Switched ResourceType to Target Before Drop off Old Cargo", FColor::Blue);
	}
	const EResourceValidation Validation = GetCanHarvestResource();
	HarvestDebug("Resource validation after drop off: " + GetStringResourceValidation(Validation),
	             FColor::Green);
	const EHarvesterAIAction Action = ResourceValidationIntoAction(Validation);
	HarvestDebug("Action after drop off: " + GetStringHarvesterAIStatus(Action), FColor::Green);
	HarvestAIExecuteAction(Action);
}

void UHarvester::UnstuckHarvesterTowardsLocation(
	const FVector& HarvesterLocation,
	const FVector& GoalLocation,
	const EHarvesterAIAction ActionOnUnstuck)
{
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());

	const FVector Midpoint = (HarvesterLocation + GoalLocation) / 2;

	FNavLocation ProjectedMidpoint;
	const FVector Extent(500.f, 500.f, 100.f);

	if (GetIsValidAIController() && NavSys && NavSys->ProjectPointToNavigation(Midpoint, ProjectedMidpoint, Extent))
	{
		const EPathFollowingRequestResult::Type MoveResult = M_AIController->MoveToLocation(
			/* FIX: actually move to the projected midpoint */ ProjectedMidpoint.Location, M_ResourceAcceptanceRadius);
		if (MoveResult == EPathFollowingRequestResult::Type::RequestSuccessful)
		{
			if (UPathFollowingComponent* PathFollowingComponent = M_AIController->GetPathFollowingComponent())
			{
				M_HarvestStatus = ActionOnUnstuck == EHarvesterAIAction::MoveToResource
					                  ? EHarvesterAIAction::UnstuckTowardsResource
					                  : EHarvesterAIAction::UnstuckTowardsDropOff;
				HarvestDebug("Successfully made request to move to UNSTUCK midpoint", FColor::Orange);
				PathFollowingComponent->OnRequestFinished.AddUObject(
					this, &UHarvester::OnFinishedUnStuck);
				return;
			}
		}
	}
	// If we can't even issue the unstuck move, consider teleport (guarded).
	TeleportHarvesterTowardsLocation(HarvesterLocation, GoalLocation, ActionOnUnstuck);
}

void UHarvester::OnFinishedUnStuck(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	if (M_HarvestStatus == NoHarvestingAction)
	{
		return;
	}
	if (GetIsValidAIController())
	{
		if (UPathFollowingComponent* PathFollowingComponent = M_AIController->GetPathFollowingComponent())
		{
			PathFollowingComponent->OnRequestFinished.RemoveAll(this);
		}
	}
	const EHarvesterAIAction ActionAfterUnstuck = M_HarvestStatus == EHarvesterAIAction::UnstuckTowardsResource
		                                              ? EHarvesterAIAction::MoveToResource
		                                              : EHarvesterAIAction::MoveToDropOff;
	if (Result.Flags == EPathFollowingResult::Success)
	{
		HarvestAIExecuteAction(ActionAfterUnstuck);
		return;
	}
	HarvestDebug("OnFinishedUnStuck:: no successful move, attempt to teleport harvester (guarded)", FColor::Orange);
	if (M_HarvestStatus == EHarvesterAIAction::UnstuckTowardsResource)
	{
		if (TargetResource.IsValid() && TargetResource->GetHarvestLocationClosestTo(GetHarvesterLocation(),
			this, M_HarvesterRadius, M_OccupiedHarvestingLocation))
		{
			TeleportHarvesterTowardsLocation(GetHarvesterLocation(),
			                                 M_OccupiedHarvestingLocation.Location,
			                                 ActionAfterUnstuck);
			return;
		}
		OnUnableToMoveToResource(ECannotMoveToResource::CannotProjectLocation);
	}
	else
	{
		if (M_TargetDropOff.IsValid())
		{
			TeleportHarvesterTowardsLocation(GetHarvesterLocation(),
			                                 M_TargetDropOff->GetDropOffLocationNotThreadSafe(),
			                                 ActionAfterUnstuck);
			return;
		}
		HarvestAIExecuteAction(EHarvesterAIAction::AsyncFindDropOff);
	}
}

/**
 * @brief Guarded teleport:
 *  - Only allow one teleport per current goal (resource or drop-off).
 *  - If a second teleport would be needed, blacklist the goal and branch to re-acquire a new one.
 */
void UHarvester::TeleportHarvesterTowardsLocation(
	const FVector& HarvesterLocation,
	const FVector& GoalLocation,
	const EHarvesterAIAction ActionOnTeleportSuccessful)
{
	// First, decide if teleport is allowed for this goal, or whether we must blacklist instead.
	if (!ConsumeTeleportAllowanceOrBlacklist(ActionOnTeleportSuccessful))
	{
		// Goal was blacklisted; re-acquire a new one.
		if (ActionOnTeleportSuccessful == EHarvesterAIAction::MoveToResource
			|| ActionOnTeleportSuccessful == EHarvesterAIAction::UnstuckTowardsResource)
		{
			ResetHarvesterLocation();
			HarvestAIExecuteAction(EHarvesterAIAction::AsyncFindResource);
		}
		else
		{
			HarvestAIExecuteAction(EHarvesterAIAction::AsyncFindDropOff);
		}
		return;
	}

	// Compute a point 200 units towards the goal and try to project+teleport there.
	const FVector Direction = (GoalLocation - HarvesterLocation).GetSafeNormal();
	const FVector DirectionPoint = HarvesterLocation + Direction * 200.f;

	// Try to project DirectionPoint to nav and use that location if possible
	bool bProjectedOK = false;
	FVector ProjectedPoint = DirectionPoint;
	RTSFunctionLibrary::GetLocationProjected(this, DirectionPoint, true, bProjectedOK, 2);
	// NOTE: RTSFunctionLibrary::GetLocationProjected does not return the out location,
	// so we fall back to DirectionPoint even on success. Teleport is physics-safe anyway.

	if (TryTeleportHarvesterToLocation(ProjectedPoint))
	{
		HarvestDebug("Successfully teleported harvester closer to Goal (guarded)", FColor::Purple);
		HarvestAIExecuteAction(ActionOnTeleportSuccessful);
		return;
	}

	HarvestDebug("Failed to teleport harvester closer to position, cancelling harvesting command...", FColor::Red);
	HarvestAIExecuteAction(EHarvesterAIAction::FinishHarvestCommand);
}

bool UHarvester::TryTeleportHarvesterToLocation(const FVector& TeleportLocation)
{
	if (AActor* Owner = GetOwner())
	{
		return Owner->SetActorLocation(TeleportLocation, false, nullptr, ETeleportType::ResetPhysics);
	}
	return false;
}

bool UHarvester::GetIsValidAIController()
{
	if (IsValid(M_AIController))
	{
		return true;
	}
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		M_AIController = Cast<AAIController>(OwnerPawn->GetController());
		if (IsValid(M_AIController))
		{
			return true;
		}
	}
	RTSFunctionLibrary::ReportError("AI controller is not valid in Harvester!"
		"\n Harvester: " + GetName());
	return false;
}

// ---------------- NEW: Blacklist helpers ----------------

void UHarvester::ClearHarvestBlacklists()
{
	M_ResourceBlacklist.Empty();
	M_DropOffBlacklist.Empty();
	M_TeleportedOnce_Resource.Empty();
	M_TeleportedOnce_DropOff.Empty();
	HarvestDebug("Cleared harvester blacklists & teleport markers", FColor::Cyan);
}

bool UHarvester::ConsumeTeleportAllowanceOrBlacklist(const EHarvesterAIAction ActionGroup)
{
	// Resource goal?
	if (ActionGroup == EHarvesterAIAction::MoveToResource || ActionGroup == EHarvesterAIAction::UnstuckTowardsResource)
	{
		if (!TargetResource.IsValid())
		{
			return false;
		}
		if (!M_TeleportedOnce_Resource.Contains(TargetResource))
		{
			M_TeleportedOnce_Resource.Add(TargetResource);
			return true; // first teleport allowed
		}
		// second teleport request -> blacklist
		if (!M_ResourceBlacklist.Contains(TargetResource))
		{
			M_ResourceBlacklist.Add(TargetResource);
			const FString RName = TargetResource.IsValid() && TargetResource->GetOwner()
				                     ? TargetResource->GetOwner()->GetName()
				                     : FString("UnknownResource");
			HarvestDebug("BLACKLIST RESOURCE (multiple teleports needed): " + RName, FColor::Red);
		}
		return false;
	}

	// DropOff goal?
	if (ActionGroup == EHarvesterAIAction::MoveToDropOff || ActionGroup == EHarvesterAIAction::UnstuckTowardsDropOff)
	{
		if (!M_TargetDropOff.IsValid())
		{
			return false;
		}
		if (!M_TeleportedOnce_DropOff.Contains(M_TargetDropOff))
		{
			M_TeleportedOnce_DropOff.Add(M_TargetDropOff);
			return true; // first teleport allowed
		}
		// second teleport request -> blacklist
		if (!M_DropOffBlacklist.Contains(M_TargetDropOff))
		{
			M_DropOffBlacklist.Add(M_TargetDropOff);
			const FString DName = M_TargetDropOff.IsValid() && M_TargetDropOff->GetOwner()
				                     ? M_TargetDropOff->GetOwner()->GetName()
				                     : FString("UnknownDropOff");
			HarvestDebug("BLACKLIST DROPOFF (multiple teleports needed): " + DName, FColor::Red);
		}
		return false;
	}

	// Default: disallow
	return false;
}

void UHarvester::OnNoDropOffsFound_DisplayMessage()
{
	if(not GetWorld())
	{
		return;
	}
	const float TimeNow = GetWorld()->GetTimeSeconds();
	if (TimeNow - M_LastNoDropOffMessageTime > DeveloperSettings::GamePlay::Navigation::MinTimeBetweenDropOffNotificationHarvester)
	{
		FRTSVerticalAnimTextSettings Settings;
		Settings.DeltaZ = 75;
		Settings.VisibleDuration = 1.5f;
		Settings.FadeOutDuration = 1.0f;
		M_LastNoDropOffMessageTime = TimeNow;
		FString Text = FRTSRichTextConverter::MakeRTSRich("No Drop-Offs Available!", ERTSRichText::Text_Bad14);
		if (M_AnimatedTextWidgetPoolManager.IsValid())
		{
			M_AnimatedTextWidgetPoolManager->ShowAnimatedText(
				Text,
				GetHarvesterLocation(),
				false,
				400,
				ETextJustify::Type::Center,
				Settings);
		}
	}
}

void UHarvester::BeginPlay_SetupAnimatedTextWidgetPoolManager()
{
	M_AnimatedTextWidgetPoolManager =  FRTS_Statics::GetVerticalAnimatedTextWidgetPoolManager(this);
}
