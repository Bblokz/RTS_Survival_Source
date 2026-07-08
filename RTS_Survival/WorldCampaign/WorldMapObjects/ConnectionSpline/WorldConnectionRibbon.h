// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SplineMeshComponent.h"
#include "WorldConnectionRibbon.generated.h"

class USplineComponent;
class UStaticMesh;
class UMaterialInterface;

/**
 * @brief Art style used to build a single connection ribbon. Owned/authored by the spline renderer manager.
 */
USTRUCT()
struct FWorldConnectionRibbonStyle
{
	GENERATED_BODY()

	/** Mesh deformed along the ribbon spline (one instance per segment). */
	UPROPERTY()
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	/** Optional material override applied to every ribbon segment. */
	UPROPERTY()
	TObjectPtr<UMaterialInterface> Material = nullptr;

	/** Forward axis of the ribbon mesh (matches how the source mesh is authored). */
	UPROPERTY()
	TEnumAsByte<ESplineMeshAxis::Type> ForwardAxis = ESplineMeshAxis::X;

	/** Final world-space width of this ribbon (already width-solved by the manager). */
	UPROPERTY()
	float Width = 120.f;

	/** Native cross-section width of Mesh at unit scale, used to convert Width into a spline mesh scale. */
	UPROPERTY()
	float MeshWidthAtUnitScale = 100.f;

	UPROPERTY()
	bool bCastShadow = false;

	UPROPERTY()
	bool bAllowDecals = true;
};

/**
 * @brief Purely visual spline-mesh ribbon that renders one solved connection path on the campaign map.
 * @note Built at runtime by AWorldConnectionSplineRenderer and attached to its owning AConnection so it
 *       inherits the connection's fog-of-war reveal corridor (the FOW cloud overlays unrevealed ribbons).
 */
UCLASS()
class RTS_SURVIVAL_API AWorldConnectionRibbon : public AActor
{
	GENERATED_BODY()

public:
	AWorldConnectionRibbon();

	/**
	 * @brief Rebuilds the ribbon geometry from a solved world-space path.
	 * @param WorldPathPoints Ordered world-space centerline points (>= 2) produced by the manager's solver.
	 * @param Style Art style and final solved width for this ribbon.
	 */
	void BuildRibbon(const TArray<FVector>& WorldPathPoints, const FWorldConnectionRibbonStyle& Style);

private:
	void ClearSplineMeshes();

	UPROPERTY()
	TObjectPtr<USplineComponent> M_Spline;

	UPROPERTY()
	TArray<TObjectPtr<USplineMeshComponent>> M_SplineMeshes;
};
