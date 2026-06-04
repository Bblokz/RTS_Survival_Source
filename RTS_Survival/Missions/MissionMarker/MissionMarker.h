// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "MissionMarker.generated.h"

class UW_MissionMarker;

/**
 * @brief Actor marker that keeps an associated UI widget positioned on-screen for mission objectives.
 */
UCLASS()
class RTS_SURVIVAL_API AMissionMarker : public AActor
{
	GENERATED_BODY()

public:
	AMissionMarker();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	// Used to spawn the on-screen marker widget
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ScreenMarker")
	TSubclassOf<UW_MissionMarker> MarkerWidgetClass;

private:
	UPROPERTY()
	TObjectPtr<UW_MissionMarker> M_MarkerWidget = nullptr;
	bool GetIsValidMarkerWidget() const;

	FVector2D M_ViewportSize;

	void BeginPlay_SetViewPortSize();
	void BeginPlay_CreateMarkerWidget();

	bool EnsureMarkerWidgetClassIsValid() const;

	void UpdateScreenMarker() const;
	/**
	 * @brief Keeps off-screen markers visible by projecting the target ray onto the viewport rectangle.
	 * @param ScreenPosition Projected marker position before edge clamping.
	 * @param ViewportCenter Cached center used as the ray origin.
	 * @return Viewport edge position, or a safe clamped fallback when no edge ray intersects.
	 */
	FVector2D GetClampedOffscreenPosition(const FVector2D& ScreenPosition, const FVector2D& ViewportCenter) const;

	/**
	 * @brief Adds a valid edge intersection candidate so the closest visible marker position can be chosen.
	 * @param EdgeHits Mutable list of candidate intersections sorted by caller.
	 * @param DirectionToMarker Normalized direction from viewport center to projected marker.
	 * @param ViewportCenter Cached center used as the ray origin.
	 * @param EdgeValue X or Y edge coordinate depending on bClampX.
	 * @param bClampX True when testing a vertical viewport edge.
	 */
	void AddMarkerEdgeHit(TArray<TPair<float, FVector2D>>& EdgeHits,
	                      const FVector2D& DirectionToMarker,
	                      const FVector2D& ViewportCenter,
	                      const float EdgeValue,
	                      const bool bClampX) const;

};
