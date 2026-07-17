// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SplinePieceCollapseProxy.h"

#include "Components/SplineMeshComponent.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "RTS_Survival/Collapse/FRTS_Collapse/FRTS_Collapse.h"
#include "RTS_Survival/Collapse/VerticalCollapse/FRTS_VerticalCollapse.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

ASplinePieceCollapseProxy::ASplinePieceCollapseProxy()
{
	PrimaryActorTick.bCanEverTick = false;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Movable);
	RootComponent = Root;
}

void ASplinePieceCollapseProxy::StartVerticalCollapse(
	UStaticMesh* Mesh,
	UMaterialInterface* MaterialOverride,
	const FVector& LocalStartPos,
	const FVector& LocalStartTangent,
	const FVector& LocalEndPos,
	const FVector& LocalEndTangent,
	const FRTSVerticalCollapseSettings& Settings,
	const FCollapseFX& CollapseFX)
{
	M_ProxySplineMesh = CreatePieceReplica(
		Mesh, MaterialOverride, LocalStartPos, LocalStartTangent, LocalEndPos, LocalEndTangent);
	if (not IsValid(M_ProxySplineMesh))
	{
		Destroy();
		return;
	}

	// Temporary proxies must never leak into the level: always destroy on finish.
	FRTSVerticalCollapseSettings ProxySettings = Settings;
	ProxySettings.bDestroyPostVerticalCollapse = true;

	FRTS_VerticalCollapse::StartVerticalCollapse(this, ProxySettings, CollapseFX);
}

void ASplinePieceCollapseProxy::StartGeometryCollapse(
	UStaticMesh* SourceMesh,
	UMaterialInterface* MaterialOverride,
	const FVector& LocalStartPos,
	const FVector& LocalStartTangent,
	const FVector& LocalEndPos,
	const FVector& LocalEndTangent,
	const TSoftObjectPtr<UGeometryCollection> GeoCollection,
	const FCollapseDuration& CollapseDuration,
	const FCollapseForce& CollapseForce,
	const FCollapseFX& CollapseFX)
{
	M_ProxySplineMesh = CreatePieceReplica(
		SourceMesh, MaterialOverride, LocalStartPos, LocalStartTangent, LocalEndPos, LocalEndTangent);
	M_ProxyGeo = CreateGeoComponent();
	if (not IsValid(M_ProxySplineMesh) || not IsValid(M_ProxyGeo))
	{
		Destroy();
		return;
	}

	// Tie the proxy's lifetime to the rubble: if the collapsed geometry is not kept visible after its
	// lifetime, FRTS_Collapse destroys this proxy right after destroying the geometry component.
	// If the rubble is kept visible, the proxy stays alive as its owner.
	FCollapseDuration ProxyDuration = CollapseDuration;
	ProxyDuration.bDestroyOwningActorAfterCollapse = not ProxyDuration.bKeepGeometryVisibleAfterLifeTime;

	FRTS_Collapse::CollapseMesh(
		this,
		M_ProxyGeo,
		GeoCollection,
		M_ProxySplineMesh,
		ProxyDuration,
		CollapseForce,
		CollapseFX);
}

USplineMeshComponent* ASplinePieceCollapseProxy::CreatePieceReplica(
	UStaticMesh* Mesh,
	UMaterialInterface* MaterialOverride,
	const FVector& LocalStartPos,
	const FVector& LocalStartTangent,
	const FVector& LocalEndPos,
	const FVector& LocalEndTangent)
{
	if (not IsValid(Mesh))
	{
		RTSFunctionLibrary::ReportError(
			"SplinePieceCollapseProxy: invalid piece mesh; cannot replicate the destroyed piece."
			"\n proxy: " + GetName());
		return nullptr;
	}

	USplineMeshComponent* Replica = NewObject<USplineMeshComponent>(this);
	if (not IsValid(Replica))
	{
		RTSFunctionLibrary::ReportError(
			"SplinePieceCollapseProxy: failed to create the spline mesh replica."
			"\n proxy: " + GetName());
		return nullptr;
	}

	Replica->SetMobility(EComponentMobility::Movable);
	Replica->RegisterComponent();
	Replica->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	Replica->SetStaticMesh(Mesh);
	if (IsValid(MaterialOverride))
	{
		Replica->SetMaterial(0, MaterialOverride);
	}
	Replica->SetForwardAxis(ESplineMeshAxis::X);
	Replica->SetStartAndEnd(LocalStartPos, LocalStartTangent, LocalEndPos, LocalEndTangent, true);

	// The proxy is purely cosmetic; the piece's gameplay collision was removed with the original component.
	FRTS_CollisionSetup::SetupSplineNoCollision(Replica);
	return Replica;
}

UGeometryCollectionComponent* ASplinePieceCollapseProxy::CreateGeoComponent()
{
	UGeometryCollectionComponent* GeoComponent = NewObject<UGeometryCollectionComponent>(this);
	if (not IsValid(GeoComponent))
	{
		RTSFunctionLibrary::ReportError(
			"SplinePieceCollapseProxy: failed to create the geometry collection component."
			"\n proxy: " + GetName());
		return nullptr;
	}

	GeoComponent->SetMobility(EComponentMobility::Movable);
	GeoComponent->RegisterComponent();
	GeoComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	// Collision and physics stay disabled until FRTS_Collapse enables them for the fracture.
	FRTS_CollisionSetup::SetupDestructibleEnvActorGeometryComponentCollision(GeoComponent);
	return GeoComponent;
}
