// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Collapse/CollapseFXParameters.h"
#include "RTS_Survival/Collapse/VerticalCollapse/RTSVerticalCollapseSettings.h"
#include "SplinePieceCollapseProxy.generated.h"

class USplineMeshComponent;
class UStaticMeshComponent;
class UGeometryCollectionComponent;
class UGeometryCollection;
class UStaticMesh;
class UMaterialInterface;

/**
 * @brief Temporary, tick-less actor that performs the collapse of a single destructible spline piece.
 * Spawned by ADestructibleSplineActor at the destroyed piece's transform (piece center, spline rotation/scale).
 * Recreates the exact deformed spline-mesh shape from the cached SetStartAndEnd parameters (no visual pop),
 * then drives the existing collapse utilities (FRTS_VerticalCollapse / FRTS_Collapse) with the shared FX contract.
 * Owns no gameplay state; lifetime is managed by the collapse utilities (see StartVerticalCollapse/StartGeometryCollapse).
 */
UCLASS(NotBlueprintable)
class RTS_SURVIVAL_API ASplinePieceCollapseProxy : public AActor
{
	GENERATED_BODY()

public:
	ASplinePieceCollapseProxy();

	/**
	 * @brief Replicates the piece as a deformed spline mesh and sinks it via FRTS_VerticalCollapse (with FX).
	 * The proxy is destroyed automatically when the collapse finishes (bDestroyPostVerticalCollapse is forced
	 * to true so temporary proxies can never leak into the level).
	 * @param Mesh The piece mesh (forward +X) to deform.
	 * @param MaterialOverride Optional material override for slot 0; pass nullptr to keep mesh materials.
	 * @param LocalStartPos Deform start position in proxy-local space (piece-center relative).
	 * @param LocalStartTangent Deform start tangent in proxy-local space.
	 * @param LocalEndPos Deform end position in proxy-local space.
	 * @param LocalEndTangent Deform end tangent in proxy-local space.
	 * @param Settings Vertical collapse tuning shared by all pieces of the owning spline.
	 * @param CollapseFX SFX/VFX played at the proxy's location when the collapse starts.
	 */
	void StartVerticalCollapse(
		UStaticMesh* Mesh,
		UMaterialInterface* MaterialOverride,
		const FVector& LocalStartPos,
		const FVector& LocalStartTangent,
		const FVector& LocalEndPos,
		const FVector& LocalEndTangent,
		const FRTSVerticalCollapseSettings& Settings,
		const FCollapseFX& CollapseFX);

	/**
	 * @brief Swaps the piece for a Chaos Geometry Collection via FRTS_Collapse::CollapseMesh (with FX).
	 * A deformed spline-mesh replica acts as MeshToCollapse so the visual matches until the fracture swaps in.
	 * Proxy lifetime: if the collapsed geometry is not kept visible, the proxy destroys itself after
	 * CollapsedGeometryLifeTime; otherwise it remains as the owner of the visible rubble.
	 * @param SourceMesh The piece mesh (forward +X) used for the pre-fracture replica.
	 * @param MaterialOverride Optional material override for slot 0; pass nullptr to keep mesh materials.
	 * @param LocalStartPos Deform start position in proxy-local space (piece-center relative).
	 * @param LocalStartTangent Deform start tangent in proxy-local space.
	 * @param LocalEndPos Deform end position in proxy-local space.
	 * @param LocalEndTangent Deform end tangent in proxy-local space.
	 * @param GeoCollection Fractured Chaos asset authored from the base (straight) piece mesh.
	 * @param CollapseDuration Geometry lifetime / keep-visible tuning shared by all pieces of the owning spline.
	 * @param CollapseForce Radial strain force applied at the proxy's location (piece center).
	 * @param CollapseFX SFX/VFX played at the proxy's location when the fracture starts.
	 */
	void StartGeometryCollapse(
		UStaticMesh* SourceMesh,
		UMaterialInterface* MaterialOverride,
		const FVector& LocalStartPos,
		const FVector& LocalStartTangent,
		const FVector& LocalEndPos,
		const FVector& LocalEndTangent,
		TSoftObjectPtr<UGeometryCollection> GeoCollection,
		const FCollapseDuration& CollapseDuration,
		const FCollapseForce& CollapseForce,
		const FCollapseFX& CollapseFX);

private:
	/**
	 * @brief Creates and registers the deformed spline-mesh replica of the destroyed piece.
	 * @return The replica component, or nullptr on failure (reported).
	 */
	USplineMeshComponent* CreatePieceReplica(
		UStaticMesh* Mesh,
		UMaterialInterface* MaterialOverride,
		const FVector& LocalStartPos,
		const FVector& LocalStartTangent,
		const FVector& LocalEndPos,
		const FVector& LocalEndTangent);

	/** @brief Creates and registers the geometry collection component used by the fracture path. */
	UGeometryCollectionComponent* CreateGeoComponent();

	// Deformed replica of the destroyed piece (both collapse paths use it as the visible mesh).
	UPROPERTY()
	TObjectPtr<USplineMeshComponent> M_ProxySplineMesh = nullptr;

	// Fracture simulation component (geometry-collection path only).
	UPROPERTY()
	TObjectPtr<UGeometryCollectionComponent> M_ProxyGeo = nullptr;
};
