// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/Environment/GroundSurface/GroundSurfaceTypes.h"
#include "GroundSurfaceSubsystem.generated.h"

/**
 * @brief Answers which ground is under a world point from the map's baked SurfaceMap_<MapName>, with one array read
 * and no traces, so every vehicle can ask a few times a second. Get it with World->GetSubsystem<>() and call
 * GetSmokeLookAt for the track smoke; maps without a baked surface map answer EGroundSurfaceType::None.
 */
UCLASS()
class RTS_SURVIVAL_API UGroundSurfaceSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	EGroundSurfaceType GetSurfaceTypeAt(const FVector& WorldLocation) const;

	const FGroundSurfaceSmokeLook& GetSmokeLookAt(const FVector& WorldLocation) const;

	bool GetHasSurfaceMap() const { return bM_HasSurfaceMap; }

private:
	void BeginPlay_ReadSurfaceMap(const UWorld& InWorld);
	void BeginPlay_BuildSmokeLooks();

	bool bM_HasSurfaceMap = false;

	FVector2D M_Origin = FVector2D::ZeroVector;
	FIntPoint M_CellCount = FIntPoint::ZeroValue;
	float M_InverseCellSize = 0.f;

	// Copy of the baked cells, so the data asset can unload; one EGroundSurfaceType per cell.
	TArray<uint8> M_SurfaceCells;

	// Indexed by EGroundSurfaceType; built from the settings once, so a lookup never searches the definitions.
	TArray<FGroundSurfaceSmokeLook> M_SmokeLookPerSurface;
};
