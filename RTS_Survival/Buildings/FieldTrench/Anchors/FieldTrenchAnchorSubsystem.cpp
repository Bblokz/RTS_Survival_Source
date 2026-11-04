// Copyright (C) Bas Blokzijl - All rights reserved.

#include "FieldTrenchAnchorSubsystem.h"

void UFieldTrenchAnchorSubsystem::RegisterAnchors(UFieldTrenchAnchorComponent& AnchorComponent)
{
	M_AnchorComponents.RemoveAll([](const TWeakObjectPtr<UFieldTrenchAnchorComponent>& Registered)
	{
		return not Registered.IsValid();
	});
	M_AnchorComponents.AddUnique(&AnchorComponent);
}

void UFieldTrenchAnchorSubsystem::UnregisterAnchors(UFieldTrenchAnchorComponent& AnchorComponent)
{
	M_AnchorComponents.Remove(&AnchorComponent);
}

FFieldTrenchAnchorHandle UFieldTrenchAnchorSubsystem::FindFreeAnchorNear(
	const FVector2D& Location,
	const float Radius,
	const int32 OwningPlayer) const
{
	FFieldTrenchAnchorHandle Nearest;
	float NearestDistanceSquared = FMath::Square(Radius);
	for (const TWeakObjectPtr<UFieldTrenchAnchorComponent>& Registered : M_AnchorComponents)
	{
		UFieldTrenchAnchorComponent* AnchorComponent = Registered.Get();
		if (not IsValid(AnchorComponent) || AnchorComponent->GetOwningPlayer() != OwningPlayer)
		{
			continue;
		}
		for (int32 AnchorIndex = 0; AnchorIndex < AnchorComponent->GetAnchorCount(); ++AnchorIndex)
		{
			if (not AnchorComponent->GetIsAnchorFree(AnchorIndex))
			{
				continue;
			}
			const float DistanceSquared = FVector2D::DistSquared(
				Location, AnchorComponent->GetAnchorPort(AnchorIndex).Location);
			if (DistanceSquared <= NearestDistanceSquared)
			{
				NearestDistanceSquared = DistanceSquared;
				Nearest.Component = AnchorComponent;
				Nearest.AnchorIndex = AnchorIndex;
			}
		}
	}
	return Nearest;
}

bool UFieldTrenchAnchorSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}
