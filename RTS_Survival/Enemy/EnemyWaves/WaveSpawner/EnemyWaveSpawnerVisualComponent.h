// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Debug/DebugDrawComponent.h"

#include "EnemyWaveSpawnerVisualComponent.generated.h"

/** World-space shapes the spawner shows in the editor; rebuilt by the spawner whenever it is moved or edited. */
struct FEnemyWaveSpawnerVisualData
{
	FColor Color = FColor::Red;
	FVector Origin = FVector::ZeroVector;
	TArray<FVector> SpawnPoints;
	TArray<FVector> Waypoints;

	// Patrol: dashed loop through the points plus a guard ring around each; attack: arrows along the path.
	bool bIsPatrol = false;
	float GuardRadius = 0.f;

	// Attack only: the direction the formation faces at the final waypoint.
	bool bHasFinalDirection = false;
	FVector FinalDirection = FVector::ForwardVector;
};

/**
 * @brief Draws an AEnemyWaveSpawner's spawn points and path in the level editor viewports, colour-coded by wave
 * behaviour, so mission logic on large maps is readable at a glance. Editor-only and never drawn in game views.
 */
UCLASS(ClassGroup = (Custom), HideCategories = (Object, LOD, Lighting, Transform, Sockets, TextureStreaming))
class RTS_SURVIVAL_API UEnemyWaveSpawnerVisualComponent : public UDebugDrawComponent
{
	GENERATED_BODY()

public:
	UEnemyWaveSpawnerVisualComponent();

	void SetVisualData(const FEnemyWaveSpawnerVisualData& VisualData);

protected:
	virtual FDebugRenderSceneProxy* CreateDebugSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

private:
	FEnemyWaveSpawnerVisualData M_VisualData;
};
