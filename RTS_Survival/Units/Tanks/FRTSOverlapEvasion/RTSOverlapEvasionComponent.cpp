#include "RTSOverlapEvasionComponent.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/TrackedTankMaster.h"
#include "Kismet/KismetMathLibrary.h"
#include "NavigationSystem.h"
#include "DrawDebugHelpers.h"
#include "Components/PrimitiveComponent.h"
#include "RTS_Survival/Units/Tanks/TrackedTank/PathFollowingComponent/TrackPathFollowingComponent.h"

namespace
{
	// Scales the evasion distance relative to the unit's inner radius.
	constexpr float GOverlapEvasionDistanceScale = 1.0f;

	// Debounce removal: recheck interval & how far we want them to be before unpausing.
	constexpr float GPendingRemovalCheckInterval = 0.33f; // seconds
	constexpr int32 GRequiredClearSamples = 2; // consecutive clear samples

	// Multiplies the other unit's formation inner radius to define our clearance threshold.
	constexpr float GOverlapClearanceRadiusScale = 0.8f;
}

URTSOverlapEvasionComponent::URTSOverlapEvasionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URTSOverlapEvasionComponent::TrackOverlapMeshOfOwner(ATrackedTankMaster* const InOwner,
                                                          UPrimitiveComponent* const InOverlapMesh)
{
	if (not IsValid(InOwner) || not IsValid(InOverlapMesh))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("URTSOverlapEvasionComponent::InitializeFromOwner invalid owner or mesh."));
		return;
	}
	if (M_TrackedOverlapMeshes.Contains(InOverlapMesh))
	{
		return;
	}

	M_Owner = InOwner;
	SetOwnerInterface(InOwner);
	SetupOwningPlayer(InOwner);
	M_TrackedOverlapMeshes.Add(InOverlapMesh);

	// Bind overlap
	InOverlapMesh->OnComponentBeginOverlap.AddDynamic(this, &URTSOverlapEvasionComponent::OnChassisOverlap);
	InOverlapMesh->OnComponentEndOverlap.AddDynamic(this, &URTSOverlapEvasionComponent::OnChassisEndOverlap);
}

bool URTSOverlapEvasionComponent::GetIsValidOwnerTrackPathFollowingComponent() const
{
	return M_OwnerTrackPathFollowingComponent.IsValid();
}

void URTSOverlapEvasionComponent::SetupTrackPathFollowingCompFromOwner()
{
	APawn* OwnerAsPawn = Cast<APawn>(GetOwner());
	if (IsValid(OwnerAsPawn))
	{
		AController* OwnerController = OwnerAsPawn->GetController();
		if (not OwnerController)
		{
			return;
		}

		UTrackPathFollowingComponent* const PathFollowingComp =
			OwnerController->FindComponentByClass<UTrackPathFollowingComponent>();
		if (not IsValid(PathFollowingComp))
		{
			return;
		}
		M_OwnerTrackPathFollowingComponent = PathFollowingComp;
	}
}

void URTSOverlapEvasionComponent::OnChassisOverlap(UPrimitiveComponent* OverlappedComp,
                                                   AActor* OtherActor,
                                                   UPrimitiveComponent* OtherComp,
                                                   int32 /*OtherBodyIndex*/,
                                                   bool /*bFromSweep*/,
                                                   const FHitResult& /*SweepResult*/)
{
	if (not M_Owner.IsValid() || not IsValid(OtherActor) || not M_OwnerCommandsInterface.GetInterface())
	{
		return;
	}
	if (M_OwnerCommandsInterface->GetIsUnitIdle())
	{
		// Do not try to evade others if idle ourselves
		return;
	}

	URTSComponent* OtherRtsComp = nullptr;
	// Only react to allied actors (same owning player).
	if (!HaveSameOwningPlayer(OtherActor, OtherRtsComp))
	{
		if constexpr (DeveloperSettings::Debugging::GTankOverlaps_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString(
				M_Owner->GetName() + TEXT(" vs ") + OtherActor->GetName() + TEXT(": No Action"));
		}
		return;
	}

	const FVector ContactLocation = IsValid(OtherComp)
		                                ? OtherComp->GetComponentLocation()
		                                : OtherActor->GetActorLocation();
	TryEvasion(OtherActor, OtherRtsComp, ContactLocation);
}

void URTSOverlapEvasionComponent::CheckFootprintForOverlaps()
{
	if (!M_Owner.IsValid())
	{
		return;
	}

	TMap<TWeakObjectPtr<AActor>, FVector> AccumulatedContact;
	TMap<TWeakObjectPtr<AActor>, int32> SampleCounts;

	// 1) Gather raw samples from all tracked overlap meshes.
	CheckFootprintForOverlaps_GatherSamples(AccumulatedContact, SampleCounts);

	// 2) Build unique, averaged per-actor list.
	TArray<TPair<TWeakObjectPtr<AActor>, FVector>> UniqueOverlaps;
	CheckFootprintForOverlaps_BuildUniqueList(AccumulatedContact, SampleCounts, UniqueOverlaps);

	// 3) Filter allies + try evasion.
	ProcessFootprintOverlaps(UniqueOverlaps);
}

void URTSOverlapEvasionComponent::SetOverlapEvasionEnabled(const bool bEnabled)
{
	for (auto EachMesh : M_TrackedOverlapMeshes)
	{
		if (not EachMesh.IsValid())
		{
			continue;
		}
		EachMesh->SetGenerateOverlapEvents(bEnabled);
	}
}

void URTSOverlapEvasionComponent::BeginPlay()
{
	Super::BeginPlay();
	SetupTrackPathFollowingCompFromOwner();
}

void URTSOverlapEvasionComponent::SetOwnerInterface(ATrackedTankMaster* InOwner)
{
	if (not InOwner)
	{
		return;
	}
	ICommands* CommandsInterface = Cast<ICommands>(InOwner);
	if (not CommandsInterface)
	{
		return;
	}
	M_OwnerCommandsInterface.SetInterface(CommandsInterface);
	M_OwnerCommandsInterface.SetObject(InOwner);
}

bool URTSOverlapEvasionComponent::HaveSameOwningPlayer(const AActor* B, URTSComponent*& OutOtherRTSComponent) const
{
	if (!IsValid(B))
	{
		return false;
	}

	URTSComponent* RTSB = B->FindComponentByClass<URTSComponent>();
	if (!IsValid(RTSB))
	{
		return false;
	}
	OutOtherRTSComponent = RTSB;

	return M_OwnerOwningPlayer == RTSB->GetOwningPlayer();
}

void URTSOverlapEvasionComponent::OnChassisEndOverlap(UPrimitiveComponent* /*OverlappedComp*/,
                                                      AActor* OtherActor,
                                                      UPrimitiveComponent* /*OtherComp*/,
                                                      int32 /*OtherBodyIndex*/)
{
	// No need to remove if we did not register at the track path following component.
	if (not IsValid(OtherActor) || not GetIsValidOwnerTrackPathFollowingComponent())
	{
		return;
	}
	if (M_Owner.IsValid() && OtherActor == M_Owner.Get())
	{
		// Never schedule pending-removal logic for ourselves.
		return;
	}
	URTSComponent* OtherRtsComp = nullptr;
	// Only react to allied actors (same owning player).
	if (!HaveSameOwningPlayer(OtherActor, OtherRtsComp))
	{
		return;
	}
	SchedulePendingRemoval(OtherActor);
}


bool URTSOverlapEvasionComponent::HasContactOrTooCloseTo(const AActor* OtherActor) const
{
	if (not M_Owner.IsValid() || not IsValid(OtherActor))
	{
		return false;
	}

	// Use the other unit's formation inner radius as baseline clearance.
	const URTSComponent* const OtherRTS = OtherActor->FindComponentByClass<URTSComponent>();
	if (not IsValid(OtherRTS))
	{
		// No RTS component => treat as clear to avoid deadlocks on non-RTS actors.
		return false;
	}

	const float InnerRadius = OtherRTS->GetFormationUnitInnerRadius();
	const float Threshold = InnerRadius * GOverlapClearanceRadiusScale;

	const FVector SelfLoc = M_Owner->GetActorLocation();
	const FVector OtherLoc = OtherActor->GetActorLocation();
	const float Dist2D = FVector::Dist2D(SelfLoc, OtherLoc);

	// "Contact or too close" while inside threshold; clear when farther away.
	return Dist2D <= Threshold;
}


void URTSOverlapEvasionComponent::SchedulePendingRemoval(AActor* OtherActor)
{
	if (not IsValid(OtherActor))
	{
		return;
	}

	// Start/keep entry; timer starts if needed.
	FPendingOverlapRemoval& Entry = M_PendingRemovalActors.FindOrAdd(OtherActor);
	Entry.ClearSamples = 0;

	if (UWorld* World = GetWorld())
	{
		if (not World->GetTimerManager().IsTimerActive(M_PendingRemovalTimerHandle))
		{
			World->GetTimerManager().SetTimer(
				M_PendingRemovalTimerHandle, this,
				&URTSOverlapEvasionComponent::Timer_RecheckPendingOverlaps,
				GPendingRemovalCheckInterval, true);
		}
	}
}

void URTSOverlapEvasionComponent::Timer_RecheckPendingOverlaps()
{
	if (M_PendingRemovalActors.Num() == 0)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(M_PendingRemovalTimerHandle);
		}
		return;
	}

	// Copy keys to avoid iterator invalidation while removing.
	TArray<TWeakObjectPtr<AActor>> Keys;
	M_PendingRemovalActors.GetKeys(Keys);

	for (const TWeakObjectPtr<AActor>& WeakActor : Keys)
	{
		AActor* const Other = WeakActor.Get();
		if (not IsValid(Other))
		{
			// No clean up on track component; it will keep track of validity itself.
			M_PendingRemovalActors.Remove(WeakActor);
			continue;
		}

		FPendingOverlapRemoval* Entry = M_PendingRemovalActors.Find(WeakActor);
		if (not Entry)
		{
			continue;
		}

		if (HasContactOrTooCloseTo(Other))
		{
			Entry->ClearSamples = 0; // still touching/too close
			continue;
		}

		// Clear sample
		Entry->ClearSamples += 1;
		if (Entry->ClearSamples >= GRequiredClearSamples)
		{
			M_OwnerTrackPathFollowingComponent->DeregisterOverlapBlockingActor(Other);
			M_PendingRemovalActors.Remove(WeakActor);
		}
	}

	// Stop timer when done.
	if (M_PendingRemovalActors.Num() == 0)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(M_PendingRemovalTimerHandle);
		}
	}
}


void URTSOverlapEvasionComponent::ProcessFootprintOverlaps(
	const TArray<TPair<TWeakObjectPtr<AActor>, FVector>>& ActorsAndContactLocations)
{
	if (!M_Owner.IsValid())
	{
		return;
	}

	for (const TPair<TWeakObjectPtr<AActor>, FVector>& Pair : ActorsAndContactLocations)
	{
		AActor* Other = Pair.Key.Get();
		if (!IsValid(Other))
		{
			continue;
		}

		URTSComponent* OtherRTSComp = nullptr;
		// Only consider allied actors (same owning player).
		if (!HaveSameOwningPlayer(Other, OtherRTSComp))
		{
			continue;
		}

		// Try to push idle allied actor out of our footprint.
		TryEvasion(Other, OtherRTSComp, Pair.Value);
	}
}

void URTSOverlapEvasionComponent::CheckFootprintForOverlaps_GatherSamples(
	TMap<TWeakObjectPtr<AActor>, FVector>& AccumulatedContact,
	TMap<TWeakObjectPtr<AActor>, int32>& SampleCounts) const
{
	for (const TWeakObjectPtr<UPrimitiveComponent>& TrackedWeak : M_TrackedOverlapMeshes)
	{
		UPrimitiveComponent* TrackedComp = TrackedWeak.Get();
		if (!IsValid(TrackedComp))
		{
			continue;
		}

		CheckFootprintForOverlaps_GatherFromMesh(TrackedComp, AccumulatedContact, SampleCounts);
	}
}

void URTSOverlapEvasionComponent::CheckFootprintForOverlaps_GatherFromMesh(
	UPrimitiveComponent* TrackedComp,
	TMap<TWeakObjectPtr<AActor>, FVector>& AccumulatedContact,
	TMap<TWeakObjectPtr<AActor>, int32>& SampleCounts) const
{
	// Query current overlaps for this component.
	TArray<UPrimitiveComponent*> OverlappingComponents;
	TrackedComp->GetOverlappingComponents(OverlappingComponents);

	for (UPrimitiveComponent* OtherComp : OverlappingComponents)
	{
		if (!IsValid(OtherComp))
		{
			continue;
		}

		AActor* OtherOwner = OtherComp->GetOwner();
		if (!IsValid(OtherOwner) || OtherOwner == M_Owner.Get())
		{
			continue; // ignore self
		}

		FVector ContactLoc = OtherOwner->GetActorLocation();
		CheckFootprintForOverlaps_EstimateContact(TrackedComp, OtherComp, ContactLoc);

		// Accumulate (so if an actor overlaps via multiple components, we average once).
		int32& CountRef = SampleCounts.FindOrAdd(OtherOwner);
		FVector& SumRef = AccumulatedContact.FindOrAdd(OtherOwner);
		SumRef += ContactLoc;
		++CountRef;
	}
}

bool URTSOverlapEvasionComponent::CheckFootprintForOverlaps_EstimateContact(
	UPrimitiveComponent* TrackedComp,
	UPrimitiveComponent* OtherComp,
	FVector& OutContactLoc) const
{
	if (!IsValid(TrackedComp) || !IsValid(OtherComp))
	{
		return false;
	}

	const FVector RefPos = TrackedComp->GetComponentLocation();

	// Primary: closest point on the other collision to our tracked comp location.
	FVector Candidate = OutContactLoc; // start with whatever was passed (often OtherOwner->GetActorLocation())
	const float Dist = OtherComp->GetClosestPointOnCollision(RefPos, Candidate, NAME_None);
	bool bSuccess = (Dist >= 0.f);

	if (!bSuccess)
	{
		// Fallback: inverse query (closest point on our tracked comp to the other comp location)
		FVector Alt = RefPos;
		const float Dist2 = TrackedComp->GetClosestPointOnCollision(OtherComp->GetComponentLocation(), Alt, NAME_None);
		if (Dist2 >= 0.f)
		{
			Candidate = Alt;
			bSuccess = true;
		}
		// else: keep existing OutContactLoc (actor location) as last resort
	}

	OutContactLoc = Candidate;
	return bSuccess;
}

void URTSOverlapEvasionComponent::CheckFootprintForOverlaps_BuildUniqueList(
	const TMap<TWeakObjectPtr<AActor>, FVector>& AccumulatedContact,
	const TMap<TWeakObjectPtr<AActor>, int32>& SampleCounts,
	TArray<TPair<TWeakObjectPtr<AActor>, FVector>>& OutUniqueOverlaps) const
{
	OutUniqueOverlaps.Reset();
	OutUniqueOverlaps.Reserve(AccumulatedContact.Num());

	for (const TPair<TWeakObjectPtr<AActor>, FVector>& It : AccumulatedContact)
	{
		const TWeakObjectPtr<AActor> ActorKey = It.Key;
		const int32* CountPtr = SampleCounts.Find(ActorKey);
		const int32 Count = CountPtr ? *CountPtr : 1;
		const FVector Averaged = (Count > 0) ? (It.Value / static_cast<float>(Count)) : It.Value;

		OutUniqueOverlaps.Emplace(ActorKey, Averaged);
	}
}


// bool URTSOverlapEvasionComponent::GetResolveDeadlockWithOther(const AActor* OtherAlliedActor) const
// {
// 	if (not M_Owner.IsValid() || not IsValid(OtherAlliedActor))
// 	{
// 		return false;
// 	}
//
// 	// Deterministic tie-breaker: only the actor with the higher unique ID registers the overlap.
// 	const int32 OwnerUniqueId = M_Owner->GetUniqueID();
// 	const int32 AlliedUniqueId = OtherAlliedActor->GetUniqueID();
// 	return OwnerUniqueId > AlliedUniqueId;
// }
//
void URTSOverlapEvasionComponent::TryEvasion(AActor* const OtherActor, URTSComponent* OtherRTS,
                                             const FVector& ContactLocation) const
{
	if (not M_Owner.IsValid() || not IsValid(OtherActor))
	{
		return;
	}

	ICommands* const OtherCommands = Cast<ICommands>(OtherActor);
	if (not OtherCommands)
	{
		// Squad unit is not part of ICommands, but its squad controller is.
		TryEvasionSquadUnit(OtherActor, ContactLocation);
		return;
	}

	if (not OtherCommands->GetIsUnitIdle())
	{
		return;
	}

	if (not IsValid(OtherRTS))
	{
		return;
	}
	if constexpr (DeveloperSettings::Debugging::GTankOverlaps_Compile_DebugSymbols)
	{
		if(M_OwnerTrackPathFollowingComponent.IsValid())
		{
			auto ArrayOfBlockers = M_OwnerTrackPathFollowingComponent->GetOverlapBlockingActorsArray();
			for(auto EachBlocker: ArrayOfBlockers)
			{
				if(not EachBlocker.Actor.IsValid())
				{
					continue;
				}
				if(EachBlocker.Actor.Get() == OtherActor)
				{
					RTSFunctionLibrary::PrintString("Found idle ally we already wait for : " + OtherActor->GetName() +
						"\n ordering new movement command to it", FColor::Red, 2.f);
				}
			}
		}
			
	}

	const float InnerRadius = OtherRTS->GetFormationUnitInnerRadius();
	const FVector SelfLoc = M_Owner->GetActorLocation();
	const FVector CollisionLoc = ContactLocation;

	FVector ProjectedLoc;
	if (ComputeEvasionLocation(SelfLoc, CollisionLoc, InnerRadius, ProjectedLoc, OtherActor))
	{
		const FRotator DesiredFinalRotation = OtherActor->GetActorRotation();

		if (OtherCommands->MoveToLocation(ProjectedLoc, true, DesiredFinalRotation, true) == ECommandQueueError::NoError)
		{
			if (GetIsValidOwnerTrackPathFollowingComponent())
			{
				M_OwnerTrackPathFollowingComponent->RegisterIdleBlockingActor(OtherActor);
			}
		}
	}
}

void URTSOverlapEvasionComponent::TryEvasionSquadUnit(AActor* OtherActor, const FVector& ContactLocation) const
{
	ASquadUnit* SquadUnit = Cast<ASquadUnit>(OtherActor);
	if (not IsValid(SquadUnit) || not SquadUnit->GetIsSquadUnitIdleAndNotEvading())
	{
		return;
	}
	ASquadController* SquadController = SquadUnit->GetSquadControllerChecked();
	if (not SquadController)
	{
		return;
	}
	const URTSComponent* const OtherRTS = SquadController->FindComponentByClass<URTSComponent>();
	if (not IsValid(OtherRTS))
	{
		return;
	}

	const float InnerRadius = OtherRTS->GetFormationUnitInnerRadius();
	const FVector SelfLoc = M_Owner->GetActorLocation();
	const FVector CollisionLoc = ContactLocation;

	FVector ProjectedLoc;
	if (ComputeEvasionLocation(SelfLoc, CollisionLoc, InnerRadius, ProjectedLoc, OtherActor))
	{
		if (GetIsValidOwnerTrackPathFollowingComponent())
		{
			M_OwnerTrackPathFollowingComponent->RegisterIdleBlockingActor(OtherActor);
		}
		SquadUnit->MoveToEvasionLocation(ProjectedLoc);
	}
}

void URTSOverlapEvasionComponent::SetupOwningPlayer(ATrackedTankMaster* Owner)
{
	if (not Owner)
	{
		return;
	}
	URTSComponent* const RTSComp = Owner->FindComponentByClass<URTSComponent>();
	if (IsValid(RTSComp))
	{
		M_OwnerOwningPlayer = RTSComp->GetOwningPlayer();
	}
}

bool URTSOverlapEvasionComponent::ComputeEvasionLocation(const FVector& SelfLoc, const FVector& ContactLocation,
                                                         const float Radius, FVector& OutProjected,
                                                         AActor* const OtherActor) const
{
	// Direction opposite of the collision location (i.e., away from the owner).
	const FVector OppositeCollisionDir = (ContactLocation - SelfLoc).GetSafeNormal();
	const float MoveDist = Radius * GOverlapEvasionDistanceScale;

	// Try 1: away from us (opposite of collision location).
	const FVector FallbackDir = OppositeCollisionDir.IsNearlyZero() ? FVector::ForwardVector : OppositeCollisionDir;
	if (TryProjectDirection(ContactLocation, FallbackDir, MoveDist, OutProjected, OtherActor))
	{
		return true;
	}

	// Try 2: in the direction the other actor is facing.
	FVector FacingDir = FVector::ZeroVector;
	if (IsValid(OtherActor))
	{
		FacingDir = OtherActor->GetActorForwardVector().GetSafeNormal();
	}
	if (!FacingDir.IsNearlyZero() &&
		TryProjectDirection(ContactLocation, FacingDir, MoveDist, OutProjected, OtherActor))
	{
		return true;
	}

	return false;
}

bool URTSOverlapEvasionComponent::TryProjectDirection(const FVector& Start, const FVector& Dir,
                                                      const float Distance, FVector& OutProj,
                                                      AActor* const OtherActor) const
{
	const FVector TestLoc = Start + Dir * Distance;
	bool bWasSuccessful = false;
	const FVector Projected = RTSFunctionLibrary::GetLocationProjected(OtherActor, TestLoc, true, bWasSuccessful, 2.f);
	if (bWasSuccessful)
	{
		OutProj = Projected;
		return true;
	}
	return false;
}
