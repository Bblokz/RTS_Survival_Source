// Copyright (C) Bas Blokzijl - All rights reserved.

#include "BehaviourComp.h"

#include "Behaviour.h"
#include "DrawDebugHelpers.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalText/Pooling/AnimatedTextWidgetPoolManager/AnimatedTextWidgetPoolManager.h"
#include "RTS_Survival/GameUI/ActionUI/ActionUIManager/ActionUIManager.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace BehaviourCompConstants
{
	constexpr float ComponentTickIntervalSeconds = 2.f;
	constexpr float DebugDrawHeight = 500.f;
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

	BeginPlay_InitAnimatedTextWidgetPoolManager();
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
	for (UBehaviour* Behaviour : M_Behaviours)
	{
		if (Behaviour == nullptr)
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

	HandleAnimatedTextTick(DeltaTime);
	ProcessPendingOperations();
	UpdateComponentTickEnabled();
}

void UBehaviourComp::AddBehaviour(TSubclassOf<UBehaviour> BehaviourClass)
{
	if (bM_IsTickingBehaviours)
	{
		QueueAddBehaviour(BehaviourClass);
		return;
	}

	UBehaviour* NewBehaviour = CreateBehaviourInstance(BehaviourClass);
	if (NewBehaviour == nullptr)
	{
		return;
	}

	if (TryHandleExistingBehaviour(NewBehaviour))
	{
		NotifyActionUIManagerOfBehaviourUpdate();
		return;
	}

	AddInitialisedBehaviour(NewBehaviour);
	NotifyActionUIManagerOfBehaviourUpdate();
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

void UBehaviourComp::ProcessPendingOperations()
{
	ProcessPendingRemovals();
	ProcessPendingSwaps();
	ProcessPendingAdds();
}

void UBehaviourComp::ProcessPendingAdds()
{
	TArray<TSubclassOf<UBehaviour>> PendingAddsCopy = M_PendingAdds;
	M_PendingAdds.Empty();

	for (const TSubclassOf<UBehaviour>& BehaviourClass : PendingAddsCopy)
	{
		AddBehaviour(BehaviourClass);
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
	if (M_BehaviourAnimatedTextStates.Num() > 0)
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

void UBehaviourComp::QueueAddBehaviour(const TSubclassOf<UBehaviour>& BehaviourClass)
{
	M_PendingAdds.Add(BehaviourClass);
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

void UBehaviourComp::AddInitialisedBehaviour(UBehaviour* NewBehaviour)
{
	NewBehaviour->InitializeBehaviour(this);
	M_Behaviours.Add(NewBehaviour);
	NewBehaviour->OnAdded(GetOwner());
	HandleBehaviourAddedText(*NewBehaviour);
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
	if (BehaviourInstance == nullptr)
	{
		return;
	}

	BehaviourInstance->OnRemoved(GetOwner());
	HandleBehaviourRemovedText(*BehaviourInstance);
	BehaviourInstance->ConditionalBeginDestroy();
	M_Behaviours.Remove(BehaviourInstance);
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
	for (UBehaviour* Behaviour : M_Behaviours)
	{
		if (Behaviour == nullptr)
		{
			continue;
		}

		Behaviour->OnRemoved(GetOwner());
		Behaviour->ConditionalBeginDestroy();
	}

	M_Behaviours.Empty();
	M_BehaviourAnimatedTextStates.Empty();
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

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_AnimatedTextWidgetPoolManager",
		"UBehaviourComp::GetIsValidAnimatedTextWidgetPoolManager",
		GetOwner());
	return false;
}

void UBehaviourComp::BeginPlay_InitAnimatedTextWidgetPoolManager()
{
	M_AnimatedTextWidgetPoolManager = FRTS_Statics::GetVerticalAnimatedTextWidgetPoolManager(this);
}

void UBehaviourComp::HandleBehaviourAddedText(UBehaviour& Behaviour)
{
	const FRepeatedBehaviourTextSettings& AnimatedTextSettings = Behaviour.GetAnimatedTextSettings();
	if (not AnimatedTextSettings.TextSettings.bUseText)
	{
		return;
	}

	if (not GetIsValidAnimatedTextWidgetPoolManager())
	{
		return;
	}

	if (not ShowAnimatedTextForOwner(AnimatedTextSettings.TextSettings))
	{
		return;
	}

	if (not ShouldRegisterAnimatedTextState(AnimatedTextSettings))
	{
		return;
	}

	const int32 RemainingRepeats = GetInitialRemainingRepeats(AnimatedTextSettings);
	RegisterAnimatedTextState(Behaviour, AnimatedTextSettings, RemainingRepeats);
}

void UBehaviourComp::HandleBehaviourRemovedText(const UBehaviour& Behaviour)
{
	M_BehaviourAnimatedTextStates.Remove(&Behaviour);
}

void UBehaviourComp::HandleAnimatedTextTick(const float DeltaTime)
{
	if (M_BehaviourAnimatedTextStates.IsEmpty())
	{
		return;
	}

	if (not GetIsValidAnimatedTextWidgetPoolManager())
	{
		return;
	}

	for (auto It = M_BehaviourAnimatedTextStates.CreateIterator(); It; ++It)
	{
		const TWeakObjectPtr<UBehaviour> WeakBehaviour = It.Key();
		if (not WeakBehaviour.IsValid())
		{
			It.RemoveCurrent();
			continue;
		}

		FBehaviourCompAnimatedTextState& TextState = It.Value();
		TextState.TimeSinceLastTextSeconds += DeltaTime;
		if (TextState.TimeSinceLastTextSeconds < TextState.RepeatIntervalSeconds)
		{
			continue;
		}

		TextState.TimeSinceLastTextSeconds = 0.f;
		if (not ShouldRepeatAnimatedText(TextState))
		{
			It.RemoveCurrent();
			continue;
		}

		if (not ShowAnimatedTextForOwner(TextState.TextSettings))
		{
			It.RemoveCurrent();
			continue;
		}

		if (TextState.RepeatStrategy == EBehaviourRepeatedVerticalTextStrategy::PerAmountRepeats)
		{
			TextState.RemainingRepeats -= 1;
			if (TextState.RemainingRepeats <= 0)
			{
				It.RemoveCurrent();
			}
		}
	}
}

void UBehaviourComp::RegisterAnimatedTextState(
	UBehaviour& Behaviour,
	const FRepeatedBehaviourTextSettings& AnimatedTextSettings,
	const int32 RemainingRepeats)
{
	FBehaviourCompAnimatedTextState TextState;
	TextState.TextSettings = AnimatedTextSettings.TextSettings;
	TextState.RepeatStrategy = AnimatedTextSettings.RepeatStrategy;
	TextState.RepeatIntervalSeconds = AnimatedTextSettings.RepeatInterval;
	TextState.TimeSinceLastTextSeconds = 0.f;
	TextState.RemainingRepeats = RemainingRepeats;
	M_BehaviourAnimatedTextStates.Add(&Behaviour, MoveTemp(TextState));
}

bool UBehaviourComp::ShouldRegisterAnimatedTextState(const FRepeatedBehaviourTextSettings& AnimatedTextSettings) const
{
	if (AnimatedTextSettings.RepeatInterval <= 0.f)
	{
		return false;
	}

	if (AnimatedTextSettings.RepeatStrategy == EBehaviourRepeatedVerticalTextStrategy::InfiniteRepeats)
	{
		return true;
	}

	return AnimatedTextSettings.AmountRepeats > 1;
}

bool UBehaviourComp::ShowAnimatedTextForOwner(const FBehaviourTextSettings& TextSettings) const
{
	if (not GetIsValidAnimatedTextWidgetPoolManager())
	{
		return false;
	}

	AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return false;
	}

	return M_AnimatedTextWidgetPoolManager->ShowAnimatedTextAttachedToActor(
		TextSettings.TextOnSubjects,
		Owner,
		TextSettings.TextOffset,
		TextSettings.bAutoWrap,
		TextSettings.InWrapAt,
		TextSettings.InJustification,
		TextSettings.InSettings);
}

bool UBehaviourComp::ShouldRepeatAnimatedText(const FBehaviourCompAnimatedTextState& TextState) const
{
	if (TextState.RepeatStrategy == EBehaviourRepeatedVerticalTextStrategy::InfiniteRepeats)
	{
		return true;
	}

	return TextState.RemainingRepeats > 0;
}

int32 UBehaviourComp::GetInitialRemainingRepeats(const FRepeatedBehaviourTextSettings& AnimatedTextSettings) const
{
	if (AnimatedTextSettings.RepeatStrategy == EBehaviourRepeatedVerticalTextStrategy::InfiniteRepeats)
	{
		return 0;
	}

	const int32 RemainingRepeats = AnimatedTextSettings.AmountRepeats - 1;
	return RemainingRepeats > 0 ? RemainingRepeats : 0;
}
