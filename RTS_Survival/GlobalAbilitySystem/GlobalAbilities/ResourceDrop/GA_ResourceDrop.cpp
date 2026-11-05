// Copyright (C) Bas Blokzijl - All rights reserved.

#include "GA_ResourceDrop.h"

#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/Resources/Resource.h"
#include "RTS_Survival/Resources/ResourceComponent/ResourceComponent.h"
#include "RTS_Survival/Scavenging/ScavengeObject/ScavengableObject.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSBlueprintFunctionLibrary.h"

namespace ResourceDropConstants
{
	// Payloads alternate sides of the flight line by this fraction of the strip's half width, so they do not land
	// in a single file that one burst could hit.
	constexpr float SideOffsetFraction = 0.35f;
	const FVector NavProjectionExtent(150.0f, 150.0f, 400.0f);
}

void UGA_ResourceDrop::GetStripDropPoints(const FVector& LineStart, const FVector& LineEnd,
                                          TArray<FVector>& OutDropPoints) const
{
	using namespace ResourceDropConstants;
	const FVector Line(LineEnd.X - LineStart.X, LineEnd.Y - LineStart.Y, 0.0f);
	const float StripLength = Line.Size();
	const int32 PayloadCount = FMath::Max(1, M_ResourceDropSettings.PayloadCount);
	if (StripLength <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	const FVector Direction = Line / StripLength;
	const FVector Sideways(-Direction.Y, Direction.X, 0.0f);
	const float SlotLength = StripLength / PayloadCount;
	const float SideOffset = PayloadCount > 1 ? M_AimSettings.AreaAim.AreaRadius * SideOffsetFraction : 0.0f;

	OutDropPoints.Reserve(PayloadCount);
	for (int32 PayloadIndex = 0; PayloadIndex < PayloadCount; ++PayloadIndex)
	{
		const float Side = PayloadIndex % 2 == 0 ? SideOffset : -SideOffset;
		const FVector OnStrip = LineStart + Direction * SlotLength * (PayloadIndex + 0.5f) + Sideways * Side;
		FVector GroundLocation;
		if (not GlobalAbilityHelpers::TryFindGroundLocation(this, OnStrip, GroundLocation))
		{
			continue;
		}
		// Supplies off the navmesh could never be reached by a scavenger or harvester.
		bool bIsOnNavigation = false;
		URTSBlueprintFunctionLibrary::RTSProjectLocationToNavigation(this, GroundLocation, NavProjectionExtent,
		                                                             bIsOnNavigation);
		if (bIsOnNavigation)
		{
			OutDropPoints.Add(GroundLocation);
		}
	}
}

TMap<ERTSResourceType, int32> UGA_ResourceDrop::GetTotalDeliveredResources() const
{
	TMap<ERTSResourceType, int32> Total;
	const int32 PayloadCount = FMath::Max(1, M_ResourceDropSettings.PayloadCount);
	if (M_ResourceDropSettings.PayloadKind == EResourceDropPayloadKind::ScavengeContainer)
	{
		for (const TPair<ERTSResourceType, int32>& Reward : M_ResourceDropSettings.ContainerSettings.RewardsPerContainer)
		{
			Total.FindOrAdd(Reward.Key) += Reward.Value * PayloadCount;
		}
		return Total;
	}
	ERTSResourceType DepositType = ERTSResourceType::Resource_Radixite;
	UClass* DepositClass = M_ResourceDropSettings.DepositSettings.DepositClass.Get();
	const ACPPResourceMaster* DepositDefaults = IsValid(DepositClass)
		                                            ? GetDefault<ACPPResourceMaster>(DepositClass)
		                                            : nullptr;
	if (IsValid(DepositDefaults) && IsValid(DepositDefaults->GetResourceComponent()))
	{
		DepositType = DepositDefaults->GetResourceComponent()->GetResourceType();
	}
	Total.Add(DepositType, M_ResourceDropSettings.DepositSettings.DepositAmount * PayloadCount);
	return Total;
}

void UGA_ResourceDrop::OnStripFlightStarted(const FStripFlightRun& Run)
{
	TArray<FVector> DropPoints;
	GetStripDropPoints(Run.LineStart, Run.LineEnd, DropPoints);
	TArray<FPendingResourcePayload> PendingPayloads;
	PendingPayloads.Reserve(DropPoints.Num());
	for (const FVector& DropPoint : DropPoints)
	{
		FPendingResourcePayload PendingPayload;
		PendingPayload.GroundLocation = DropPoint;
		PendingPayload.DistanceOnStrip = FVector::DotProduct(DropPoint - Run.LineStart, Run.Direction);
		PendingPayloads.Add(PendingPayload);
	}
	M_PendingPayloadsPerRun.Add(Run.RunId, MoveTemp(PendingPayloads));
}

void UGA_ResourceDrop::OnStripFlightProgress(const FStripFlightRun& Run, const float PreviousDistance,
                                             const float NewDistance)
{
	TArray<FPendingResourcePayload>* PendingPayloads = M_PendingPayloadsPerRun.Find(Run.RunId);
	if (not PendingPayloads)
	{
		return;
	}
	// Collected first: dropping spawns actors, which must not happen while the pending list is being edited.
	TArray<FPendingResourcePayload> PayloadsToDrop;
	for (int32 PayloadIndex = PendingPayloads->Num() - 1; PayloadIndex >= 0; --PayloadIndex)
	{
		if ((*PendingPayloads)[PayloadIndex].DistanceOnStrip <= NewDistance)
		{
			PayloadsToDrop.Add((*PendingPayloads)[PayloadIndex]);
			PendingPayloads->RemoveAt(PayloadIndex);
		}
	}
	for (const FPendingResourcePayload& Payload : PayloadsToDrop)
	{
		DropPayload(Run, Payload);
	}
}

void UGA_ResourceDrop::OnStripFlightEnded(const FStripFlightRun& Run)
{
	M_PendingPayloadsPerRun.Remove(Run.RunId);
}

void UGA_ResourceDrop::GetAssetsToLoadBeforeFlight(TArray<FSoftObjectPath>& OutAssets) const
{
	const FSoftObjectPath PayloadClassPath = GetPayloadClassPath();
	if (PayloadClassPath.IsValid())
	{
		OutAssets.Add(PayloadClassPath);
	}
}

FSoftObjectPath UGA_ResourceDrop::GetPayloadClassPath() const
{
	switch (M_ResourceDropSettings.PayloadKind)
	{
	case EResourceDropPayloadKind::ScavengeContainer:
		return M_ResourceDropSettings.ContainerSettings.ContainerClass.ToSoftObjectPath();
	case EResourceDropPayloadKind::HarvestableDeposit:
		return M_ResourceDropSettings.DepositSettings.DepositClass.ToSoftObjectPath();
	}
	return FSoftObjectPath();
}

void UGA_ResourceDrop::DropPayload(const FStripFlightRun& Run, const FPendingResourcePayload& Payload)
{
	UWorld* World = GetWorld();
	const AAircraftMaster* Aircraft = Run.Aircraft.Get();
	if (not IsValid(World) || not IsValid(Aircraft))
	{
		return;
	}
	UClass* PayloadClass = Cast<UClass>(GetPayloadClassPath().ResolveObject());
	if (not IsValid(PayloadClass))
	{
		RTSFunctionLibrary::ReportError(TEXT("UGA_ResourceDrop has no loaded payload class: ")
			+ GetPayloadClassPath().ToString());
		return;
	}
	AParachutedResourcePayload* FallingPayload = World->SpawnActor<AParachutedResourcePayload>(
		AParachutedResourcePayload::StaticClass(), Aircraft->GetActorLocation(), Run.Direction.Rotation());
	if (not IsValid(FallingPayload))
	{
		return;
	}
	FallingPayload->OnPayloadLanded.AddUObject(this, &UGA_ResourceDrop::OnPayloadLanded);
	FallingPayload->StartFall(Aircraft->GetActorLocation(), Payload.GroundLocation, PayloadClass,
	                          M_ResourceDropSettings);
}

void UGA_ResourceDrop::OnPayloadLanded(AParachutedResourcePayload* Payload, AActor* SpawnedResource)
{
	OnResourceLanded.Broadcast(this, SpawnedResource);
}
