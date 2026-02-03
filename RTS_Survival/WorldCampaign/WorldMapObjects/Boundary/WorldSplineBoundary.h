// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldSplineBoundary.generated.h"

class USplineComponent;

/**
 * @brief Defines a designer-authored spline boundary used to constrain anchor generation.
 */
UCLASS()
class RTS_SURVIVAL_API AWorldSplineBoundary : public AActor
{
	GENERATED_BODY()

public:
	AWorldSplineBoundary();

	USplineComponent* GetBoundarySpline() const;
	void EnsureClosedLoop();
	void GetSampledPolygon2D(float SampleSpacing, TArray<FVector2D>& OutPolygon) const;

private:
	bool GetIsValidBoundarySpline() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Campaign|Boundary",
		meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USplineComponent> M_BoundarySpline;
};
