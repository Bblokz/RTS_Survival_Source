// Copyright (C) Bas Blokzijl - All rights reserved.

#include "WallGateNavigation.h"

#include "NavigationData.h"
#include "NavAreas/NavArea_Null.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "WallGate.h"

namespace WallGateNavigationConstants
{
	// The human player; every other owning player is the AI side (UGameUnitManager splits units the same way).
	constexpr uint8 HumanPlayerIndex = 1;
	const FColor PlayerSideAreaColour = FColor(40, 160, 255);
	const FColor EnemySideAreaColour = FColor(255, 120, 40);
}

UNavArea_WallGatePlayerSide::UNavArea_WallGatePlayerSide(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DrawColor = WallGateNavigationConstants::PlayerSideAreaColour;
}

UNavArea_WallGateEnemySide::UNavArea_WallGateEnemySide(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DrawColor = WallGateNavigationConstants::EnemySideAreaColour;
}

URTSWallGateNavQueryFilter::URTSWallGateNavQueryFilter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Units of both sides share these filters, so the gate exclusion is decided per querier, not cached per class.
	bInstantiateForQuerier = true;
}

void URTSWallGateNavQueryFilter::InitializeFilter(const ANavigationData& NavData, const UObject* Querier,
                                                  FNavigationQueryFilter& Filter) const
{
	Super::InitializeFilter(NavData, Querier, Filter);
	uint8 OwningPlayer = 0;
	if (not TryGetQuerierOwningPlayer(Querier, OwningPlayer))
	{
		// Unknown side: no closed gate lets it through.
		ExcludeArea(NavData, UNavArea_WallGatePlayerSide::StaticClass(), Filter);
		ExcludeArea(NavData, UNavArea_WallGateEnemySide::StaticClass(), Filter);
		return;
	}
	const bool bIsHumanPlayer = OwningPlayer == WallGateNavigationConstants::HumanPlayerIndex;
	ExcludeArea(NavData, bIsHumanPlayer
		                     ? UNavArea_WallGateEnemySide::StaticClass()
		                     : UNavArea_WallGatePlayerSide::StaticClass(), Filter);
}

bool URTSWallGateNavQueryFilter::TryGetQuerierOwningPlayer(const UObject* Querier, uint8& OutOwningPlayer)
{
	const AActor* QuerierActor = Cast<AActor>(Querier);
	if (const AController* const QuerierController = Cast<AController>(Querier))
	{
		QuerierActor = QuerierController->GetPawn();
	}
	if (not IsValid(QuerierActor))
	{
		return false;
	}
	const URTSComponent* const RTSComponent = QuerierActor->FindComponentByClass<URTSComponent>();
	if (not IsValid(RTSComponent))
	{
		return false;
	}
	OutOwningPlayer = RTSComponent->GetOwningPlayer();
	return true;
}

void URTSWallGateNavQueryFilter::ExcludeArea(const ANavigationData& NavData, const UClass* AreaClass,
                                             FNavigationQueryFilter& Filter)
{
	const int32 AreaId = NavData.GetAreaID(AreaClass);
	// Not registered with this navigation data: no gate uses the area, nothing to exclude.
	if (AreaId < 0 || AreaId > MAX_uint8)
	{
		return;
	}
	Filter.SetExcludedArea(static_cast<uint8>(AreaId));
}

UWallGateNavModifierBase::UWallGateNavModifierBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWallGateNavModifierBase::AddOrientedBox(const FBox& LocalBox, const FTransform& LocalToWorld,
                                              const float WorldPadding) const
{
	const FQuat Rotation = LocalToWorld.GetRotation();
	const FVector WorldCentre = LocalToWorld.TransformPosition(LocalBox.GetCenter());
	const FVector WorldExtent = LocalBox.GetExtent() * LocalToWorld.GetScale3D().GetAbs() + FVector(WorldPadding);
	// The engine expects each box in a frame that is only rotated, with the rotation handed over separately.
	ComponentBounds.Add(FRotatedBox(FBox::BuildAABB(Rotation.UnrotateVector(WorldCentre), WorldExtent), Rotation));
	Bounds += FBox::BuildAABB(FVector::ZeroVector, WorldExtent).TransformBy(FTransform(Rotation, WorldCentre));
}

void UWallGateNavModifierBase::SetEmptyBounds() const
{
	const AActor* const OwnerActor = GetOwner();
	const FVector Location = IsValid(OwnerActor) ? OwnerActor->GetActorLocation() : FVector::ZeroVector;
	Bounds = FBox::BuildAABB(Location, FVector::OneVector);
}

UWallGateNavModifier::UWallGateNavModifier(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UWallGateNavModifier::CalculateBounds() const
{
	ComponentBounds.Reset();
	Bounds = FBox(ForceInit);
	const AWallGate* const Gate = Cast<AWallGate>(GetOwner());
	FBox LeafSpaceBox;
	FTransform LeafTransform;
	if (not IsValid(Gate) || not Gate->GetDoorwayNavBox(LeafSpaceBox, LeafTransform))
	{
		SetEmptyBounds();
		return;
	}
	// The gate already padded the box in leaf space.
	AddOrientedBox(LeafSpaceBox, LeafTransform, 0.f);
}

UWallGateFrameNavModifier::UWallGateFrameNavModifier(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AreaClass = UNavArea_Null::StaticClass();
}

void UWallGateFrameNavModifier::CalculateBounds() const
{
	ComponentBounds.Reset();
	Bounds = FBox(ForceInit);
	const AWallGate* const Gate = Cast<AWallGate>(GetOwner());
	TArray<FBox> FrameSpaceBoxes;
	FTransform FrameTransform;
	if (not IsValid(Gate) || not Gate->GetFrameNavBoxes(FrameSpaceBoxes, FrameTransform))
	{
		SetEmptyBounds();
		return;
	}
	for (const FBox& FrameSpaceBox : FrameSpaceBoxes)
	{
		AddOrientedBox(FrameSpaceBox, FrameTransform, Gate->GetFrameNavBoxPadding());
	}
}
