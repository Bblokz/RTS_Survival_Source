// Copyright (C) Bas Blokzijl - All rights reserved.

#include "WorldCampaign/WorldMapObjects/AnchorPoint/AnchorPoint.h"

#include "DrawDebugHelpers.h"
#include "WorldCampaign/WorldMapObjects/Connection/Connection.h"

AAnchorPoint::AAnchorPoint()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AAnchorPoint::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	EnsureAnchorKeyIsInitialized();
}

void AAnchorPoint::PostLoad()
{
	Super::PostLoad();

	EnsureAnchorKeyIsInitialized();
}

int32 AAnchorPoint::GetConnectionCount() const
{
	return M_Connections.Num();
}

void AAnchorPoint::ClearConnections()
{
	M_Connections.Reset();
	M_NeighborAnchors.Reset();
}

void AAnchorPoint::AddConnection(AConnection* Connection, AAnchorPoint* NeighborAnchor)
{
	if (not IsValid(Connection))
	{
		return;
	}

	if (not M_Connections.Contains(Connection))
	{
		M_Connections.Add(Connection);
	}

	if (not IsValid(NeighborAnchor))
	{
		return;
	}

	if (not M_NeighborAnchors.Contains(NeighborAnchor))
	{
		M_NeighborAnchors.Add(NeighborAnchor);
	}
}

void AAnchorPoint::SortNeighborsByKey()
{
	M_NeighborAnchors.Sort([](const TObjectPtr<AAnchorPoint>& Left, const TObjectPtr<AAnchorPoint>& Right)
	{
		if (not IsValid(Left))
		{
			return false;
		}

		if (not IsValid(Right))
		{
			return true;
		}

		return IsAnchorKeyLess(Left->GetAnchorKey(), Right->GetAnchorKey());
	});
}

bool AAnchorPoint::IsAnchorKeyLess(const FGuid& Left, const FGuid& Right)
{
	if (Left.A != Right.A)
	{
		return Left.A < Right.A;
	}

	if (Left.B != Right.B)
	{
		return Left.B < Right.B;
	}

	if (Left.C != Right.C)
	{
		return Left.C < Right.C;
	}

	return Left.D < Right.D;
}

void AAnchorPoint::DebugDrawAnchorState(const FString& Label, const FColor& Color, float Duration) const
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	const FVector AnchorLocation = GetActorLocation();
	constexpr float DebugSphereRadius = 75.f;
	constexpr int32 DebugSphereSegments = 12;
	constexpr float DebugTextHeightOffset = 100.f;
	DrawDebugSphere(World, AnchorLocation, DebugSphereRadius, DebugSphereSegments, Color, false, Duration);
	DrawDebugString(World, AnchorLocation + FVector(0.f, 0.f, DebugTextHeightOffset), Label, nullptr, Color, Duration);
}

void AAnchorPoint::DebugDrawConnectionTo(const AAnchorPoint* OtherAnchor, const FColor& Color, float Duration) const
{
	if (not IsValid(OtherAnchor))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	constexpr float DebugLineThickness = 6.f;
	DrawDebugLine(World, GetActorLocation(), OtherAnchor->GetActorLocation(), Color, false, Duration, 0, DebugLineThickness);
}

void AAnchorPoint::OnEnemyItemPromotion(EMapEnemyItem EnemyItemType)
{
	BP_OnEnemyItemPromotion(EnemyItemType);
}

void AAnchorPoint::OnNeutralItemPromotion(EMapNeutralObjectType NeutralObjectType)
{
	BP_OnNeutralItemPromotion(NeutralObjectType);
}

void AAnchorPoint::OnMissionPromotion(EMapMission MissionType)
{
	BP_OnMissionPromotion(MissionType);
}

void AAnchorPoint::EnsureAnchorKeyIsInitialized()
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	if (M_AnchorKey.IsValid())
	{
		return;
	}

	M_AnchorKey = FGuid::NewGuid();
}
