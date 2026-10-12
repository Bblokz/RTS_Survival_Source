// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/NavCollision/RTSNavCollision.h"

#include "ApartmentFootprintNavModifier.generated.h"

/**
 * @brief Marks one navigation area over a set of boxes given in the owner's space, instead of over the owner's
 * collision; the owning apartment hands it the ring or the core of its footprint with SetFootprintBoxes.
 */
UCLASS(ClassGroup=(Custom))
class RTS_SURVIVAL_API UApartmentFootprintNavModifier : public URTSNavCollision
{
	GENERATED_BODY()

public:
	/**
	 * @brief Replaces the covered area and refreshes the navigation mesh under it.
	 * @param LocalBoxes Ground rectangles in the owner's space.
	 * @param LocalMinHeight Bottom of the area relative to the owner's origin.
	 * @param LocalMaxHeight Top of the area relative to the owner's origin.
	 */
	void SetFootprintBoxes(const TArray<FBox2D>& LocalBoxes, const float LocalMinHeight, const float LocalMaxHeight);

protected:
	// The engine version derives the boxes from the owner's collision, which for an apartment is every loose panel.
	virtual void CalculateBounds() const override;

private:
	// Saved with the level: the owner works them out in the editor, and a cooked collection may not allow that anymore.
	UPROPERTY()
	TArray<FBox> M_LocalBoxes;
};
