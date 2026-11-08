// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GA_CompositeDrop.h"

#include "TimerManager.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace CompositeDropConstants
{
	// Same projection reach as the extra aircraft of UGA_AircraftDrop.
	constexpr float OffsetProjectionScale = 7.0f;
}

void UGA_CompositeDrop::ExecuteAbilityAtLocation(const FVector& TargetLocation)
{
	StartRun(TargetLocation, false, TargetLocation, TargetLocation);
}

void UGA_CompositeDrop::ExecuteAbilityAlongLine(const FVector& LineStart, const FVector& LineEnd)
{
	StartRun((LineStart + LineEnd) * 0.5f, true, LineStart, LineEnd);
}

void UGA_CompositeDrop::GetStripDropPoints(const FVector& LineStart, const FVector& LineEnd,
                                           TArray<FVector>& OutDropPoints) const
{
	for (const FCompositeDropChild& Entry : M_CompositeDropSettings.Children)
	{
		if (not IsValid(Entry.Ability) || Entry.AimMode == ECompositeDropChildAim::Point)
		{
			continue;
		}
		const FVector Offset(Entry.TargetOffset.X, Entry.TargetOffset.Y, 0.f);
		Entry.Ability->GetStripDropPoints(LineStart + Offset, LineEnd + Offset, OutDropPoints);
	}
}

TArray<UGlobalAbility*> UGA_CompositeDrop::GetRunningChildren() const
{
	TArray<UGlobalAbility*> RunningChildren;
	for (const FCompositeDropRun& Run : M_Runs)
	{
		for (UGlobalAbility* Child : Run.Children)
		{
			if (IsValid(Child))
			{
				RunningChildren.Add(Child);
			}
		}
	}
	return RunningChildren;
}

void UGA_CompositeDrop::StartRun(const FVector& Target, const bool bIsStrip, const FVector& LineStart,
                                 const FVector& LineEnd)
{
	ResetAbilityEndedBroadcast();
	if (not GetIsValidGlobalAbilityManager())
	{
		return;
	}
	// Parent marker and execute sound; the children carry none.
	Super::ExecuteAbilityAtLocation(Target);

	FCompositeDropRun NewRun;
	NewRun.RunId = M_NextRunId++;
	NewRun.Target = Target;
	NewRun.bM_IsStrip = bIsStrip;
	NewRun.LineStart = LineStart;
	NewRun.LineEnd = LineEnd;
	NewRun.ApproachDirection = bIsStrip ? (LineEnd - LineStart).GetSafeNormal2D() : GetApproachDirection(Target);
	if (NewRun.ApproachDirection.IsNearlyZero())
	{
		NewRun.ApproachDirection = FVector::ForwardVector;
	}
	NewRun.bM_IsStarting = true;
	for (const FCompositeDropChild& Entry : M_CompositeDropSettings.Children)
	{
		UGlobalAbility* ChildCopy = CreateChildCopy(Entry.Ability);
		NewRun.Children.Add(ChildCopy);
		NewRun.ChildHasEnded.Add(not IsValid(ChildCopy));
		NewRun.PendingChildCount += IsValid(ChildCopy) ? 1 : 0;
	}
	if (NewRun.PendingChildCount == 0)
	{
		RTSFunctionLibrary::ReportError(TEXT("UGA_CompositeDrop has no valid child abilities: ")
			+ UEnum::GetValueAsString(GetAbilityType()));
		if (M_Runs.IsEmpty())
		{
			QueueEndedBroadcast();
		}
		return;
	}
	const int32 RunId = NewRun.RunId;
	M_Runs.Add(MoveTemp(NewRun));

	UWorld* World = GetWorld();
	const int32 ChildCount = M_CompositeDropSettings.Children.Num();
	for (int32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
	{
		const float Delay = M_CompositeDropSettings.Children[ChildIndex].StartDelaySeconds;
		if (Delay <= 0.f || not IsValid(World))
		{
			StartChild(RunId, ChildIndex);
			continue;
		}
		TWeakObjectPtr<UGA_CompositeDrop> WeakThis(this);
		FTimerHandle StartHandle;
		World->GetTimerManager().SetTimer(StartHandle, FTimerDelegate::CreateLambda([WeakThis, RunId, ChildIndex]()
		{
			if (WeakThis.IsValid())
			{
				WeakThis->StartChild(RunId, ChildIndex);
			}
		}), Delay, false);
	}
	if (FCompositeDropRun* Run = FindRun(RunId))
	{
		Run->bM_IsStarting = false;
	}
	FinishRunIfDone(RunId);
}

FCompositeDropRun* UGA_CompositeDrop::FindRun(const int32 RunId)
{
	return M_Runs.FindByPredicate([RunId](const FCompositeDropRun& Run) { return Run.RunId == RunId; });
}

UGlobalAbility* UGA_CompositeDrop::CreateChildCopy(const UGlobalAbility* ChildTemplate)
{
	if (not IsValid(ChildTemplate))
	{
		return nullptr;
	}
	// A fresh copy per execution: children keep per-run state (aircraft counts, drop runs), and the templates
	// must never be executed.
	UGlobalAbility* ChildCopy = DuplicateObject<UGlobalAbility>(ChildTemplate, this);
	if (not IsValid(ChildCopy))
	{
		return nullptr;
	}
	UGlobalAbilitiesManager* Manager = GetGlobalAbilityManager();
	// The same context the manager gives a top-level ability (an enemy manager's owner is no ACPPController).
	ChildCopy->InitGlobalAbility(GetOwningPlayer(), Manager, Cast<ACPPController>(Manager->GetOwner()));
	ChildCopy->OnGlobalAbilityEnded.AddUObject(this, &UGA_CompositeDrop::OnChildEnded);
	return ChildCopy;
}

FVector UGA_CompositeDrop::GetApproachDirection(const FVector& Target) const
{
	const UGlobalAbilitiesManager* Manager = GetGlobalAbilityManager();
	if (not IsValid(Manager))
	{
		return FVector::ForwardVector;
	}
	const FVector FriendlyEdge = Manager->GetAircraftBombingSpawnLocation(this, Target);
	return (Target - FriendlyEdge).GetSafeNormal2D();
}

void UGA_CompositeDrop::StartChild(const int32 RunId, const int32 ChildIndex)
{
	FCompositeDropRun* Run = FindRun(RunId);
	if (Run == nullptr || not Run->Children.IsValidIndex(ChildIndex)
		|| not M_CompositeDropSettings.Children.IsValidIndex(ChildIndex))
	{
		return;
	}
	UGlobalAbility* Child = Run->Children[ChildIndex];
	if (not IsValid(Child) || Run->ChildHasEnded[ChildIndex])
	{
		return;
	}
	// Copied: the child may end at once, and finishing the run removes it from M_Runs.
	const FCompositeDropRun RunCopy = *Run;
	OnChildStarted.Broadcast(this, ChildIndex, Child);
	ExecuteChild(*Child, M_CompositeDropSettings.Children[ChildIndex], RunCopy);
}

void UGA_CompositeDrop::ExecuteChild(UGlobalAbility& Child, const FCompositeDropChild& Entry,
                                     const FCompositeDropRun& Run) const
{
	if (Entry.AimMode == ECompositeDropChildAim::Point)
	{
		Child.ExecuteAbilityAtLocation(GetOffsetTarget(Run.Target, Entry.TargetOffset));
		return;
	}
	FVector ChildLineStart;
	FVector ChildLineEnd;
	GetChildStrip(Entry, Child, Run, ChildLineStart, ChildLineEnd);
	Child.ExecuteAbilityAlongLine(ChildLineStart, ChildLineEnd);
}

FVector UGA_CompositeDrop::GetOffsetTarget(const FVector& Target, const FVector2D& Offset) const
{
	if (Offset.IsNearlyZero())
	{
		return Target;
	}
	FVector Ground;
	const bool bHasLandscape = GlobalAbilityHelpers::TryFindGroundLocation(
		this, Target + FVector(Offset.X, Offset.Y, 0.f), Ground);
	bool bWasProjected = false;
	const FVector Projected = RTSFunctionLibrary::GetLocationProjected(
		this, Ground, true, bWasProjected, CompositeDropConstants::OffsetProjectionScale);
	if (bWasProjected)
	{
		return Projected;
	}
	// Aimed next to the map edge, the offset point can lie past it, with neither navmesh nor landscape: that part
	// of the drop would come down on nothing and fall out of the world, so it lands at the composite's own target.
	return bHasLandscape ? Ground : Target;
}

void UGA_CompositeDrop::GetChildStrip(const FCompositeDropChild& Entry, const UGlobalAbility& Child,
                                      const FCompositeDropRun& Run, FVector& OutLineStart,
                                      FVector& OutLineEnd) const
{
	const FVector Offset(Entry.TargetOffset.X, Entry.TargetOffset.Y, 0.f);
	if (Run.bM_IsStrip)
	{
		OutLineStart = Run.LineStart + Offset;
		OutLineEnd = Run.LineEnd + Offset;
		return;
	}
	const FVector Target = GetOffsetTarget(Run.Target, Entry.TargetOffset);
	const float Length = Child.GetAimSettings().AreaAim.MaxStripLength;
	const FVector Along = Run.ApproachDirection * Length;
	if (Entry.AimMode == ECompositeDropChildAim::StripCentredOnTarget)
	{
		OutLineStart = Target - Along * 0.5f;
		OutLineEnd = Target + Along * 0.5f;
		return;
	}
	OutLineStart = Target - Along;
	OutLineEnd = Target;
}

void UGA_CompositeDrop::OnChildEnded(UGlobalAbility* Child)
{
	for (FCompositeDropRun& Run : M_Runs)
	{
		const int32 ChildIndex = Run.Children.IndexOfByKey(Child);
		if (ChildIndex == INDEX_NONE)
		{
			continue;
		}
		if (not Run.ChildHasEnded[ChildIndex])
		{
			Run.ChildHasEnded[ChildIndex] = true;
			Run.PendingChildCount = FMath::Max(0, Run.PendingChildCount - 1);
		}
		const int32 RunId = Run.RunId;
		FinishRunIfDone(RunId);
		return;
	}
}

void UGA_CompositeDrop::FinishRunIfDone(const int32 RunId)
{
	const FCompositeDropRun* Run = FindRun(RunId);
	if (Run == nullptr || Run->bM_IsStarting || Run->PendingChildCount > 0)
	{
		return;
	}
	for (UGlobalAbility* Child : Run->Children)
	{
		if (IsValid(Child))
		{
			Child->OnGlobalAbilityEnded.RemoveAll(this);
		}
	}
	M_Runs.RemoveAll([RunId](const FCompositeDropRun& Candidate) { return Candidate.RunId == RunId; });
	if (M_Runs.IsEmpty())
	{
		QueueEndedBroadcast();
	}
}

void UGA_CompositeDrop::QueueEndedBroadcast()
{
	// Next frame: every listener of the last child's end has run before the parent reports its own end.
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		BroadcastAbilityEnded();
		return;
	}
	TWeakObjectPtr<UGA_CompositeDrop> WeakThis(this);
	M_EndedBroadcastHandle = World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (WeakThis.IsValid() && WeakThis->M_Runs.IsEmpty())
		{
			WeakThis->BroadcastAbilityEnded();
		}
	}));
}
