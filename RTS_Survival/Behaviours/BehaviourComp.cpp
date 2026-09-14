// Copyright (C) Bas Blokzijl - All rights reserved.

#include "BehaviourComp.h"

#include "Behaviour.h"
#include "Mutators/MutatorSettings.h"
#include "DrawDebugHelpers.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalText/Pooling/AnimatedTextWidgetPoolManager/AnimatedTextWidgetPoolManager.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalIcons/AnimatedIconWidgetPoolManager.h"
#include "RTS_Survival/GameUI/ActionUI/ActionUIManager/ActionUIManager.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSDebugBreak/RTSDebugBreak.h"

namespace BehaviourCompConstants
{
	constexpr float ComponentTickIntervalSeconds = 2.f;
	constexpr float DebugDrawHeight = 500.f;
	constexpr uint8 EnemyPlayerId = 2;
}

UBehaviourComp::UBehaviourComp()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickInterval = BehaviourCompConstants::ComponentTickIntervalSeconds;
}

void UBehaviourComp::BeginPlay()
{
	Super::BeginPlay();

	BeginPlay_InitMutations();
	UpdateComponentTickEnabled();
}

void UBehaviourComp::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearAllBehaviours();
	Super::EndPlay(EndPlayReason);
}

void UBehaviourComp::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	bM_IsTickingBehaviours = true;
	TArray<TObjectPtr<UBehaviour>> RemovalQueue;
	// A behaviour tick can destroy the owner, whose EndPlay clears the live array.
	const TArray<TObjectPtr<UBehaviour>> BehavioursToTick = M_Behaviours;
	for (UBehaviour* Behaviour : BehavioursToTick)
	{
		if (not IsValid(Behaviour) || not M_Behaviours.Contains(Behaviour))
		{
			continue;
		}

		HandleTimedExpiry(DeltaTime, *Behaviour, RemovalQueue);
		HandleBehaviourTick(DeltaTime, *Behaviour);
	}

	bM_IsTickingBehaviours = false;

	for (UBehaviour* BehaviourToRemove : RemovalQueue)
	{
		RemoveBehaviourInstance(BehaviourToRemove);
	}

	HandleAnimatedFeedbackTick();
	ProcessPendingOperations();
	UpdateComponentTickEnabled();
}

void UBehaviourComp::AddBehaviour(TSubclassOf<UBehaviour> BehaviourClass)
{
	if (not IsValid(BehaviourClass))
	{
		RTS_ENSUREMSGF(
			false,
			TEXT("Invalid BehaviourClass provided to AddBehaviour: %s"),
			*GetNameSafe(BehaviourClass));
		return;
	}

	AddBehaviourInternal(BehaviourClass, TOptional<float>());
}

void UBehaviourComp::AddBehaviourWithDuration(TSubclassOf<UBehaviour> BehaviourClass, const float CustomLifetimeSeconds)
{
	if (not IsValid(BehaviourClass))
	{
		RTS_ENSUREMSGF(
			false,
			TEXT("Invalid BehaviourClass provided to AddBehaviourWithDuration: %s"),
			*GetNameSafe(BehaviourClass));
		return;
	}

	AddBehaviourInternal(BehaviourClass, CustomLifetimeSeconds);
}

void UBehaviourComp::RemoveBehaviour(TSubclassOf<UBehaviour> BehaviourClass)
{
	if (bM_IsTickingBehaviours)
	{
		QueueRemoveBehaviour(BehaviourClass);
		return;
	}

	for (UBehaviour* Behaviour : M_Behaviours)
	{
		if (Behaviour == nullptr)
		{
			continue;
		}

		if (Behaviour->GetClass() != BehaviourClass)
		{
			continue;
		}

		RemoveBehaviourInstance(Behaviour);
		UpdateComponentTickEnabled();
		return;
	}
}

void UBehaviourComp::SwapBehaviour(TSubclassOf<UBehaviour> BehaviourClassToReplace,
                                   TSubclassOf<UBehaviour> BehaviourClassToAdd)
{
	if (bM_IsTickingBehaviours)
	{
		QueueSwapBehaviour(BehaviourClassToReplace, BehaviourClassToAdd);
		return;
	}

	for (UBehaviour* Behaviour : M_Behaviours)
	{
		if (Behaviour == nullptr)
		{
			continue;
		}

		if (Behaviour->GetClass() != BehaviourClassToReplace)
		{
			continue;
		}

		RemoveBehaviourInstance(Behaviour);
		AddBehaviour(BehaviourClassToAdd);
		return;
	}
}

void UBehaviourComp::RefreshAllBehaviours()
{
	TArray<TSubclassOf<UBehaviour>> BehaviourClassesToReAdd;
	for (UBehaviour* Behaviour : M_Behaviours)
	{
		if (Behaviour == nullptr)
		{
			continue;
		}

		const TSubclassOf<UBehaviour> BehaviourClass = Behaviour->GetClass();
		if (BehaviourClass == nullptr)
		{
			continue;
		}

		BehaviourClassesToReAdd.Add(BehaviourClass);
	}

	if (bM_IsTickingBehaviours)
	{
		M_PendingRefreshBehaviourClasses = MoveTemp(BehaviourClassesToReAdd);
		return;
	}

	ClearAllBehaviours();

	for (const TSubclassOf<UBehaviour>& BehaviourClass : BehaviourClassesToReAdd)
	{
		AddBehaviour(BehaviourClass);
	}
}

void UBehaviourComp::ProcessPendingRefreshAllBehaviours()
{
	if (M_PendingRefreshBehaviourClasses.IsEmpty())
	{
		return;
	}

	const TArray<TSubclassOf<UBehaviour>> PendingRefreshBehaviourClasses = M_PendingRefreshBehaviourClasses;
	M_PendingRefreshBehaviourClasses.Empty();

	ClearAllBehaviours();

	for (const TSubclassOf<UBehaviour>& BehaviourClass : PendingRefreshBehaviourClasses)
	{
		AddBehaviour(BehaviourClass);
	}
}

void UBehaviourComp::ProcessPendingOperations()
{
	ProcessPendingRefreshAllBehaviours();
	ProcessPendingRemovals();
	ProcessPendingSwaps();
	ProcessPendingAdds();
}

void UBehaviourComp::ProcessPendingAdds()
{
	TArray<FPendingBehaviourAdd> PendingAddsCopy = M_PendingAdds;
	M_PendingAdds.Empty();

	for (const FPendingBehaviourAdd& PendingAdd : PendingAddsCopy)
	{
		if (PendingAdd.bHasCustomLifetime)
		{
			AddBehaviourInternal(PendingAdd.BehaviourClass, PendingAdd.CustomLifetimeSeconds);
			continue;
		}
		AddBehaviourInternal(PendingAdd.BehaviourClass, TOptional<float>());
	}
}

void UBehaviourComp::ProcessPendingRemovals()
{
	TArray<TSubclassOf<UBehaviour>> PendingRemovalsCopy = M_PendingRemovals;
	M_PendingRemovals.Empty();

	for (const TSubclassOf<UBehaviour>& BehaviourClass : PendingRemovalsCopy)
	{
		RemoveBehaviour(BehaviourClass);
	}
}

void UBehaviourComp::ProcessPendingSwaps()
{
	TArray<TSubclassOf<UBehaviour>> PendingSwapRemoveCopy = M_PendingSwapRemove;
	TArray<TSubclassOf<UBehaviour>> PendingSwapAddCopy = M_PendingSwapAdd;
	M_PendingSwapRemove.Empty();
	M_PendingSwapAdd.Empty();

	const int32 PendingCount = PendingSwapRemoveCopy.Num();
	for (int32 PendingIndex = 0; PendingIndex < PendingCount; PendingIndex++)
	{
		SwapBehaviour(PendingSwapRemoveCopy[PendingIndex], PendingSwapAddCopy[PendingIndex]);
	}
}

void UBehaviourComp::HandleTimedExpiry(const float DeltaTime, UBehaviour& Behaviour,
                                       TArray<TObjectPtr<UBehaviour>>& RemovalQueue) const
{
	if (not Behaviour.IsTimedBehaviour())
	{
		return;
	}

	Behaviour.AdvanceLifetime(DeltaTime);
	if (Behaviour.HasExpired())
	{
		RemovalQueue.Add(&Behaviour);
	}
}

void UBehaviourComp::HandleBehaviourTick(const float DeltaTime, UBehaviour& Behaviour) const
{
	if (not Behaviour.UsesTick())
	{
		return;
	}

	Behaviour.OnTick(DeltaTime);
}

bool UBehaviourComp::ShouldComponentTick() const
{
	if (not M_BehaviourAnimatedFeedbackStates.IsEmpty())
	{
		return true;
	}

	for (const UBehaviour* Behaviour : M_Behaviours)
	{
		if (Behaviour == nullptr)
		{
			continue;
		}

		if (Behaviour->UsesTick() || Behaviour->IsTimedBehaviour())
		{
			return true;
		}
	}

	return false;
}

void UBehaviourComp::UpdateComponentTickEnabled()
{
	const bool bShouldTick = ShouldComponentTick();
	if (bShouldTick == IsComponentTickEnabled())
	{
		return;
	}

	SetComponentTickEnabled(bShouldTick);
}

void UBehaviourComp::QueueAddBehaviour(const TSubclassOf<UBehaviour>& BehaviourClass,
                                       const TOptional<float>& CustomLifetimeSeconds)
{
	FPendingBehaviourAdd PendingAdd;
	PendingAdd.BehaviourClass = BehaviourClass;
	if (CustomLifetimeSeconds.IsSet())
	{
		PendingAdd.bHasCustomLifetime = true;
		PendingAdd.CustomLifetimeSeconds = CustomLifetimeSeconds.GetValue();
	}
	M_PendingAdds.Add(PendingAdd);
}

void UBehaviourComp::QueueRemoveBehaviour(const TSubclassOf<UBehaviour>& BehaviourClass)
{
	M_PendingRemovals.Add(BehaviourClass);
}

void UBehaviourComp::QueueSwapBehaviour(const TSubclassOf<UBehaviour>& BehaviourClassToReplace,
                                        const TSubclassOf<UBehaviour>& BehaviourClassToAdd)
{
	M_PendingSwapRemove.Add(BehaviourClassToReplace);
	M_PendingSwapAdd.Add(BehaviourClassToAdd);
}

bool UBehaviourComp::TryHandleExistingBehaviour(UBehaviour* NewBehaviour)
{
	const TArray<TObjectPtr<UBehaviour>> MatchingBehaviours = FindMatchingBehaviours(NewBehaviour);
	if (MatchingBehaviours.Num() == 0)
	{
		return false;
	}

	bool bHandledExistingBehaviour = false;
	bool bAddedToStack = false;
	switch (NewBehaviour->GetStackRule())
	{
	case EBehaviourStackRule::Exclusive:
		bHandledExistingBehaviour = true;
		break;
	case EBehaviourStackRule::Refresh:
		MatchingBehaviours[0]->OnRefreshed(NewBehaviour);
		MatchingBehaviours[0]->RefreshLifetime();
		bHandledExistingBehaviour = true;
		break;
	case EBehaviourStackRule::Stack:
		bHandledExistingBehaviour = true;
		if (CanStackBehaviour(MatchingBehaviours, NewBehaviour))
		{
			MatchingBehaviours[0]->OnStack(NewBehaviour);
			AddInitialisedBehaviour(NewBehaviour);
			bAddedToStack = true;
		}
		break;
	default:
		break;
	}

	if (bHandledExistingBehaviour && not bAddedToStack)
	{
		NewBehaviour->ConditionalBeginDestroy();
	}

	return bHandledExistingBehaviour;
}

void UBehaviourComp::AddBehaviourInternal(const TSubclassOf<UBehaviour>& BehaviourClass,
                                          const TOptional<float>& CustomLifetimeSeconds)
{
	if (bM_IsTickingBehaviours)
	{
		QueueAddBehaviour(BehaviourClass, CustomLifetimeSeconds);
		return;
	}

	UBehaviour* NewBehaviour = CreateBehaviourInstance(BehaviourClass);
	if (NewBehaviour == nullptr)
	{
		return;
	}

	if (CustomLifetimeSeconds.IsSet())
	{
		NewBehaviour->SetLifetimeDuration(CustomLifetimeSeconds.GetValue());
	}

	if (TryHandleExistingBehaviour(NewBehaviour))
	{
		NotifyActionUIManagerOfBehaviourUpdate();
		return;
	}

	AddInitialisedBehaviour(NewBehaviour);
	NotifyActionUIManagerOfBehaviourUpdate();
}

void UBehaviourComp::AddInitialisedBehaviour(UBehaviour* NewBehaviour)
{
	NewBehaviour->InitializeBehaviour(this);
	M_Behaviours.Add(NewBehaviour);
	NewBehaviour->OnAdded(GetOwner());
	if (IsValid(NewBehaviour) && M_Behaviours.Contains(NewBehaviour))
	{
		HandleBehaviourAddedFeedback(*NewBehaviour);
	}
	UpdateComponentTickEnabled();
}

bool UBehaviourComp::CanStackBehaviour(const TArray<TObjectPtr<UBehaviour>>& MatchingBehaviours,
                                       const UBehaviour* NewBehaviour) const
{
	const int32 MaxStacks = NewBehaviour->GetMaxStackCount();
	return MatchingBehaviours.Num() < MaxStacks;
}

TArray<TObjectPtr<UBehaviour>> UBehaviourComp::FindMatchingBehaviours(const UBehaviour* NewBehaviour) const
{
	TArray<TObjectPtr<UBehaviour>> MatchingBehaviours;
	for (UBehaviour* Behaviour : M_Behaviours)
	{
		if (Behaviour == nullptr)
		{
			continue;
		}

		if (Behaviour->IsSameBehaviour(*NewBehaviour))
		{
			MatchingBehaviours.Add(Behaviour);
		}
	}

	return MatchingBehaviours;
}

void UBehaviourComp::RemoveBehaviourInstance(UBehaviour* BehaviourInstance)
{
	if (not IsValid(BehaviourInstance))
	{
		return;
	}

	// Remove first so callbacks and stale removal queues cannot remove this instance twice.
	if (M_Behaviours.Remove(BehaviourInstance) == 0)
	{
		return;
	}

	HandleBehaviourRemovedFeedback(*BehaviourInstance);
	BehaviourInstance->OnRemoved(GetOwner());
	BehaviourInstance->ConditionalBeginDestroy();
	NotifyActionUIManagerOfBehaviourUpdate();
}

UBehaviour* UBehaviourComp::CreateBehaviourInstance(const TSubclassOf<UBehaviour>& BehaviourClass)
{
	if (BehaviourClass == nullptr)
	{
		return nullptr;
	}

	UBehaviour* NewBehaviour = NewObject<UBehaviour>(this, BehaviourClass);
	if (NewBehaviour == nullptr)
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "BehaviourClass", __FUNCTION__, nullptr);
		return nullptr;
	}

	return NewBehaviour;
}

void UBehaviourComp::ClearAllBehaviours()
{
	// Blueprint removal callbacks may add/remove behaviours or re-enter this cleanup.
	TArray<TObjectPtr<UBehaviour>> BehavioursToRemove;
	Swap(BehavioursToRemove, M_Behaviours);
	M_BehaviourAnimatedFeedbackStates.Reset();
	for (UBehaviour* Behaviour : BehavioursToRemove)
	{
		if (not IsValid(Behaviour))
		{
			continue;
		}

		Behaviour->OnRemoved(GetOwner());
		Behaviour->ConditionalBeginDestroy();
	}
}

void UBehaviourComp::DebugDrawBehaviours(const float DurationSeconds) const
{
	const AActor* OwnerActor = GetOwner();
	if (OwnerActor == nullptr)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	struct FBehaviourDebugGroup
	{
		UBehaviour* Representative = nullptr;
		int32 StackCount = 0;
	};

	TArray<FBehaviourDebugGroup> BehaviourGroups;
	for (UBehaviour* Behaviour : M_Behaviours)
	{
		if (Behaviour == nullptr)
		{
			continue;
		}

		bool bFoundGroup = false;
		for (FBehaviourDebugGroup& Group : BehaviourGroups)
		{
			if (Group.Representative != nullptr && Group.Representative->IsSameBehaviour(*Behaviour))
			{
				Group.StackCount++;
				bFoundGroup = true;
				break;
			}
		}

		if (not bFoundGroup)
		{
			FBehaviourDebugGroup& NewGroup = BehaviourGroups.Emplace_GetRef();
			NewGroup.Representative = Behaviour;
			NewGroup.StackCount = 1;
		}
	}

	FString DebugString = TEXT("Behaviours:");
	for (const FBehaviourDebugGroup& Group : BehaviourGroups)
	{
		if (Group.Representative == nullptr)
		{
			continue;
		}

		const FString BehaviourName = Group.Representative->GetClass()->GetName();
		const FString UsesTickLabel = Group.Representative->UsesTick() ? TEXT("Tick:Yes") : TEXT("Tick:No");
		const FString StackLabel = FString::Printf(TEXT("Stacks:%d"), Group.StackCount);
		DebugString += FString::Printf(TEXT("\n%s | %s | %s"), *BehaviourName, *UsesTickLabel, *StackLabel);
	}

	const FVector DebugLocation = OwnerActor->GetActorLocation() + FVector(
		0.f, 0.f, BehaviourCompConstants::DebugDrawHeight);
	DrawDebugString(World, DebugLocation, DebugString, nullptr, FColor::Green, DurationSeconds, true);
}

UBehaviour* UBehaviourComp::GetBehaviourByClass(const TSubclassOf<UBehaviour>& BehaviourClass) const
{
	if (BehaviourClass == nullptr)
	{
		return nullptr;
	}

	for (UBehaviour* Behaviour : M_Behaviours)
	{
		if (Behaviour == nullptr)
		{
			continue;
		}

		if (Behaviour->GetClass() == BehaviourClass)
		{
			return Behaviour;
		}
	}

	return nullptr;
}

TArray<UBehaviour*> UBehaviourComp::GetBehavioursByClass(const TSubclassOf<UBehaviour>& BehaviourClass) const
{
	TArray<UBehaviour*> MatchingBehaviours;

	if (BehaviourClass == nullptr)
	{
		return MatchingBehaviours;
	}

	for (UBehaviour* Behaviour : M_Behaviours)
	{
		if (Behaviour == nullptr)
		{
			continue;
		}

		if (Behaviour->GetClass() == BehaviourClass)
		{
			MatchingBehaviours.Add(Behaviour);
		}
	}

	return MatchingBehaviours;
}

void UBehaviourComp::OnBehaviourHovered(const bool bIsHovering, const FBehaviourUIData& BehaviourUIData)
{
	for (UBehaviour* Behaviour : M_Behaviours)
	{
		if (Behaviour == nullptr)
		{
			continue;
		}

		FBehaviourUIData CachedUIData;
		Behaviour->GetUIData(CachedUIData);
		if (CachedUIData.BehaviourIcon != BehaviourUIData.BehaviourIcon)
		{
			continue;
		}

		Behaviour->OnBehaviorHover(bIsHovering);
		return;
	}
}


void UBehaviourComp::RegisterActionUIManager(UActionUIManager* ActionUIManager)
{
	M_ActionUIManager = ActionUIManager;
	bM_HasActionUIManagerRegistration = ActionUIManager != nullptr;
}

void UBehaviourComp::NotifyActionUIManagerOfBehaviourUpdate()
{
	if (not bM_HasActionUIManagerRegistration)
	{
		return;
	}

	if (not GetIsValidActionUIManager())
	{
		return;
	}

	M_ActionUIManager->RefreshBehaviourUIForComponent(this);
}

void UBehaviourComp::BeginPlay_InitMutations()
{
	if (not FRTS_Statics::AreMutationsOn(this))
	{
		return;
	}

	const URTSComponent* RTSComponent = GetOwnerRTSComponent();
	if (RTSComponent == nullptr)
	{
		return;
	}

	if (RTSComponent->GetOwningPlayer() != BehaviourCompConstants::EnemyPlayerId)
	{
		return;
	}

	EMutatorClass MutatorClass = EMutatorClass::Squad;
	if (not TryGetMutationClassForRTSComponent(*RTSComponent, MutatorClass))
	{
		return;
	}

	const UMutatorSettings* MutatorSettings = UMutatorSettings::Get();
	if (MutatorSettings == nullptr)
	{
		return;
	}

	AddRandomMutationFromMutators(MutatorSettings->GetMutatorsForClass(MutatorClass));
}

const URTSComponent* UBehaviourComp::GetOwnerRTSComponent() const
{
	const AActor* OwnerActor = GetOwner();
	if (OwnerActor == nullptr)
	{
		return nullptr;
	}

	return OwnerActor->FindComponentByClass<URTSComponent>();
}

bool UBehaviourComp::TryGetMutationClassForRTSComponent(const URTSComponent& RTSComponent,
                                                        EMutatorClass& OutMutatorClass) const
{
	switch (RTSComponent.GetUnitType())
	{
	case EAllUnitType::UNType_Squad:
		OutMutatorClass = EMutatorClass::Squad;
		return true;
	case EAllUnitType::UNType_Tank:
		return TryGetMutationClassForTankSubtype(RTSComponent.GetSubtypeAsTankSubtype(), OutMutatorClass);
	default:
		return false;
	}
}

bool UBehaviourComp::TryGetMutationClassForTankSubtype(const ETankSubtype TankSubtype,
                                                       EMutatorClass& OutMutatorClass) const
{
	if (Global_GetIsTankDestroyer(TankSubtype))
	{
		OutMutatorClass = EMutatorClass::TankDestroyer;
		return true;
	}

	if (Global_GetIsArmoredCar(TankSubtype))
	{
		OutMutatorClass = EMutatorClass::ArmoredCar;
		return true;
	}

	if (Global_GetIsLightTank(TankSubtype))
	{
		OutMutatorClass = EMutatorClass::LightTank;
		return true;
	}

	if (Global_GetIsMediumTank(TankSubtype))
	{
		OutMutatorClass = EMutatorClass::MediumTank;
		return true;
	}

	if (Global_GetIsHeavyTank(TankSubtype))
	{
		OutMutatorClass = EMutatorClass::HeavyTank;
		return true;
	}

	return false;
}

void UBehaviourComp::AddRandomMutationFromMutators(const TArray<TSubclassOf<UBehaviour>>& Mutators)
{
	TArray<TSubclassOf<UBehaviour>> ValidMutators;
	for (const TSubclassOf<UBehaviour>& Mutator : Mutators)
	{
		if (Mutator == nullptr)
		{
			continue;
		}

		ValidMutators.Add(Mutator);
	}

	if (ValidMutators.IsEmpty())
	{
		return;
	}

	const int32 MutatorIndex = FMath::RandRange(0, ValidMutators.Num() - 1);
	AddBehaviour(ValidMutators[MutatorIndex]);
}

bool UBehaviourComp::GetIsValidActionUIManager() const
{
	if (M_ActionUIManager.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_ActionUIManager",
		"UBehaviourComp::GetIsValidActionUIManager",
		GetOwner());
	return false;
}

bool UBehaviourComp::GetIsValidAnimatedTextWidgetPoolManager() const
{
	if (M_AnimatedTextWidgetPoolManager.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, TEXT("M_AnimatedTextWidgetPoolManager"),
		TEXT("GetIsValidAnimatedTextWidgetPoolManager"), this);
	return false;
}

bool UBehaviourComp::GetIsValidAnimatedIconWidgetPoolManager() const
{
	if (M_AnimatedIconWidgetPoolManager.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, TEXT("M_AnimatedIconWidgetPoolManager"),
		TEXT("GetIsValidAnimatedIconWidgetPoolManager"), this);
	return false;
}

bool UBehaviourComp::EnsureAnimatedTextWidgetPoolManager()
{
	const UAnimatedTextWidgetPoolManager* CachedManager = M_AnimatedTextWidgetPoolManager.Get();
	if (not IsValid(CachedManager))
	{
		M_AnimatedTextWidgetPoolManager = FRTS_Statics::GetVerticalAnimatedTextWidgetPoolManager(this);
	}
	return GetIsValidAnimatedTextWidgetPoolManager();
}

bool UBehaviourComp::EnsureAnimatedIconWidgetPoolManager()
{
	const UAnimatedIconWidgetPoolManager* CachedManager = M_AnimatedIconWidgetPoolManager.Get();
	if (not IsValid(CachedManager))
	{
		M_AnimatedIconWidgetPoolManager = FRTS_Statics::GetVerticalAnimatedIconWidgetPoolManager(this);
	}
	return GetIsValidAnimatedIconWidgetPoolManager();
}

void UBehaviourComp::HandleBehaviourAddedFeedback(UBehaviour& Behaviour)
{
	if (Behaviour.GetAnimatedIconSettings().IconSettings.bUseIcons)
	{
		HandleBehaviourAddedIcon(Behaviour);
		return;
	}
	HandleBehaviourAddedText(Behaviour);
}

void UBehaviourComp::HandleBehaviourAddedText(UBehaviour& Behaviour)
{
	const FRepeatedBehaviourTextSettings& Settings = Behaviour.GetAnimatedTextSettings();
	if (not Settings.TextSettings.bUseText)
	{
		return;
	}

	FBehaviourCompAnimatedFeedbackState State;
	State.Behaviour = &Behaviour;
	State.DisplaySettings.Set<FBehaviourTextSettings>(Settings.TextSettings);
	State.RepeatIntervalSeconds = Settings.RepeatInterval;
	State.RemainingRepeats = Settings.RepeatStrategy == EBehaviourRepeatedVerticalTextStrategy::InfiniteRepeats
		? INDEX_NONE : FMath::Max(1, Settings.AmountRepeats) - 1;
	if (not ShowAnimatedFeedback(State))
	{
		return;
	}
	RegisterAnimatedFeedbackState(MoveTemp(State));
}

void UBehaviourComp::HandleBehaviourAddedIcon(UBehaviour& Behaviour)
{
	const FRepeatedBehaviourIconSettings& Settings = Behaviour.GetAnimatedIconSettings();
	FBehaviourCompAnimatedFeedbackState State;
	State.Behaviour = &Behaviour;
	State.DisplaySettings.Set<FBehaviourIconSettings>(Settings.IconSettings);
	State.RepeatIntervalSeconds = Settings.RepeatInterval;
	State.RemainingRepeats = Settings.RepeatStrategy == EBehaviourRepeatedVerticalIconStrategy::InfiniteRepeats
		? INDEX_NONE : FMath::Max(1, Settings.AmountRepeats) - 1;
	if (not ShowAnimatedFeedback(State))
	{
		return;
	}
	RegisterAnimatedFeedbackState(MoveTemp(State));
}

void UBehaviourComp::RegisterAnimatedFeedbackState(FBehaviourCompAnimatedFeedbackState&& State)
{
	UBehaviour* Behaviour = State.Behaviour.Get();
	if (State.RemainingRepeats == 0 || not FMath::IsFinite(State.RepeatIntervalSeconds)
		|| State.RepeatIntervalSeconds <= 0.0f || not IsValid(Behaviour) || not M_Behaviours.Contains(Behaviour))
	{
		return;
	}
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	State.NextDisplayTimeSeconds = World->GetTimeSeconds() + static_cast<double>(State.RepeatIntervalSeconds);
	M_BehaviourAnimatedFeedbackStates.Add(MoveTemp(State));
}

void UBehaviourComp::HandleBehaviourRemovedFeedback(const UBehaviour& Behaviour)
{
	const TWeakObjectPtr<const UBehaviour> RemovedBehaviour = &Behaviour;
	M_BehaviourAnimatedFeedbackStates.RemoveAllSwap(
		[RemovedBehaviour](const FBehaviourCompAnimatedFeedbackState& State)
		{
			return State.Behaviour == RemovedBehaviour;
		}, EAllowShrinking::No);
}

void UBehaviourComp::HandleAnimatedFeedbackTick()
{
	if (M_BehaviourAnimatedFeedbackStates.IsEmpty())
	{
		return;
	}
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	// Widget callbacks may request behaviour mutations. Defer those like gameplay OnTick mutations.
	TGuardValue<bool> FeedbackTickGuard(bM_IsTickingBehaviours, true);
	const double NowSeconds = World->GetTimeSeconds();
	for (int32 StateIndex = M_BehaviourAnimatedFeedbackStates.Num() - 1; StateIndex >= 0; --StateIndex)
	{
		AdvanceAnimatedFeedbackState(StateIndex, NowSeconds);
	}
}

void UBehaviourComp::AdvanceAnimatedFeedbackState(const int32 StateIndex, const double NowSeconds)
{
	// Owner destruction can clear the entire scheduler during a widget visibility callback.
	if (not M_BehaviourAnimatedFeedbackStates.IsValidIndex(StateIndex))
	{
		return;
	}
	FBehaviourCompAnimatedFeedbackState& State = M_BehaviourAnimatedFeedbackStates[StateIndex];
	if (not State.Behaviour.IsValid())
	{
		M_BehaviourAnimatedFeedbackStates.RemoveAtSwap(StateIndex, 1, EAllowShrinking::No);
		return;
	}
	if (NowSeconds < State.NextDisplayTimeSeconds)
	{
		return;
	}

	// Copy only when due. No array-held reference is used after calling the widget pool.
	const FBehaviourCompAnimatedFeedbackState DisplayState = State;
	State.NextDisplayTimeSeconds = NowSeconds + State.RepeatIntervalSeconds;
	if (State.RemainingRepeats > 0)
	{
		--State.RemainingRepeats;
	}
	if (State.RemainingRepeats == 0)
	{
		M_BehaviourAnimatedFeedbackStates.RemoveAtSwap(StateIndex, 1, EAllowShrinking::No);
	}
	if (ShowAnimatedFeedback(DisplayState))
	{
		return;
	}
	const TWeakObjectPtr<UBehaviour> FailedBehaviour = DisplayState.Behaviour;
	M_BehaviourAnimatedFeedbackStates.RemoveAllSwap(
		[FailedBehaviour](const FBehaviourCompAnimatedFeedbackState& RemainingState)
		{
			return RemainingState.Behaviour == FailedBehaviour;
		}, EAllowShrinking::No);
}

bool UBehaviourComp::ShowAnimatedFeedback(const FBehaviourCompAnimatedFeedbackState& State)
{
	if (State.DisplaySettings.IsType<FBehaviourIconSettings>())
	{
		return ShowAnimatedIconForOwner(State.DisplaySettings.Get<FBehaviourIconSettings>());
	}
	return ShowAnimatedTextForOwner(State.DisplaySettings.Get<FBehaviourTextSettings>());
}

bool UBehaviourComp::ShowAnimatedTextForOwner(const FBehaviourTextSettings& TextSettings)
{
	AActor* Owner = GetOwner();
	if (not IsValid(Owner) || not EnsureAnimatedTextWidgetPoolManager())
	{
		return false;
	}
	return M_AnimatedTextWidgetPoolManager->ShowAnimatedTextAttachedToActor(
		TextSettings.TextOnSubjects, Owner, TextSettings.TextOffset, TextSettings.bAutoWrap,
		TextSettings.InWrapAt, TextSettings.InJustification, TextSettings.InSettings);
}

bool UBehaviourComp::ShowAnimatedIconForOwner(const FBehaviourIconSettings& IconSettings)
{
	if (IconSettings.IconType == ERTSVerticalAnimatedIcon::None)
	{
		return false;
	}
	AActor* Owner = GetOwner();
	if (not IsValid(Owner) || not EnsureAnimatedIconWidgetPoolManager())
	{
		return false;
	}
	if (IconSettings.bOverrideAnimationSettings)
	{
		return M_AnimatedIconWidgetPoolManager->ShowAnimatedIconAttachedToActorWithSettings(
			IconSettings.IconType, Owner, IconSettings.LocalOffset, IconSettings.AnimationSettings);
	}
	return M_AnimatedIconWidgetPoolManager->ShowAnimatedIconAttachedToActor(
		IconSettings.IconType, Owner, IconSettings.LocalOffset);
}
