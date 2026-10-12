// Copyright (C) Bas Blokzijl - All rights reserved.

#include "ApartmentFootprintNavModifier.h"

#include "GameFramework/Actor.h"

void UApartmentFootprintNavModifier::SetFootprintBoxes(const TArray<FBox2D>& LocalBoxes, const float LocalMinHeight,
                                                       const float LocalMaxHeight)
{
	M_LocalBoxes.Reset(LocalBoxes.Num());
	for (const FBox2D& LocalBox : LocalBoxes)
	{
		M_LocalBoxes.Emplace(FVector(LocalBox.Min, LocalMinHeight), FVector(LocalBox.Max, LocalMaxHeight));
	}
	// Refreshing alone re-sends the boxes cached at registration, when there were none yet.
	UpdateNavigationBounds();
	RefreshNavigationModifiers();
}

void UApartmentFootprintNavModifier::CalculateBounds() const
{
	ComponentBounds.Reset(M_LocalBoxes.Num());
	Bounds = FBox(ForceInit);
	const AActor* OwnerActor = GetOwner();
	if (not IsValid(OwnerActor))
	{
		return;
	}
	const FTransform OwnerTransform = OwnerActor->GetActorTransform();
	const FQuat OwnerRotation = OwnerTransform.GetRotation();
	const FVector OwnerScale = OwnerTransform.GetScale3D().GetAbs();
	for (const FBox& LocalBox : M_LocalBoxes)
	{
		const FVector WorldCentre = OwnerTransform.TransformPosition(LocalBox.GetCenter());
		const FVector Extent = LocalBox.GetExtent() * OwnerScale;
		// The engine expects each box in a frame that is only rotated, with the rotation handed over separately.
		ComponentBounds.Add(FRotatedBox(FBox::BuildAABB(OwnerRotation.UnrotateVector(WorldCentre), Extent),
		                                OwnerRotation));
		Bounds += FBox::BuildAABB(FVector::ZeroVector, Extent).TransformBy(FTransform(OwnerRotation, WorldCentre));
	}
	if (not Bounds.IsValid)
	{
		// Nothing to mark yet; a valid, empty-handed box keeps the navigation octree happy.
		Bounds = FBox::BuildAABB(OwnerTransform.GetLocation(), FVector::OneVector);
	}
}
