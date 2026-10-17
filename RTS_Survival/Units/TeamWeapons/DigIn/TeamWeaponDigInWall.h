// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TeamWeaponDigInWall.generated.h"

class URTSNavCollisionEnvObstacle;
class UStaticMeshComponent;
class UTeamWeaponDigInComponent;

namespace TeamWeaponDigInWallSockets
{
	// Wall runs that block navigation: socket location = box centre, socket scale = half extent / NavBoxScale.
	constexpr const TCHAR* NavBoxPrefix = TEXT("NavBox_");
	constexpr float NavBoxScale = 100.f;
	// Spots inside the pit where the gun's guards stand while dug in.
	constexpr const TCHAR* GuardPostPrefix = TEXT("GuardPost_");
}

/** Build progress of a team weapon dig-in wall: the sandbags rise out of the ground over the build time. */
USTRUCT()
struct FTeamWeaponDigInWallBuildState
{
	GENERATED_BODY()

	float BuildSeconds = 0.f;
	float ElapsedSeconds = 0.f;
	// How far below its place the wall starts; the full wall height so nothing shows at the start.
	float SunkDepthCm = 0.f;
	bool bIsBuilt = false;
};

/**
 * @brief The sandbag pit around a dug-in team weapon, spawned and removed by the gun's UTeamWeaponDigInComponent.
 * It is not a unit: shots that hit the sandbags hurt the gun (whose protection the dig-in raises) and its wall runs
 * block navigation, so units walk in through the rear entrance. The mesh is generated per gun (SM_TWDigIn_<Gun>).
 */
UCLASS(NotBlueprintable)
class RTS_SURVIVAL_API ATeamWeaponDigInWall : public AActor
{
	GENERATED_BODY()

public:
	ATeamWeaponDigInWall();

	/**
	 * @brief Sets the wall up around its gun and starts raising it; the dig-in component is told when it stands.
	 * @param WallMesh The gun's generated pit mesh.
	 * @param OwningDigIn The gun's dig-in component; it owns this wall.
	 * @param OwningPlayer Player of the gun: the walls block the opposing player's weapons.
	 * @param BuildSeconds Time until the wall stands; zero builds it at once.
	 */
	void StartBuilding(UStaticMesh* WallMesh, UTeamWeaponDigInComponent* OwningDigIn, const uint8 OwningPlayer,
	                   const float BuildSeconds);

	bool GetIsBuilt() const { return M_BuildState.bIsBuilt; }
	float GetBuildProgress() const;
	UStaticMeshComponent* GetWallMeshComponent() const { return M_WallMesh; }
	int32 GetGuardPostCount() const;
	bool TryGetGuardPostLocation(const int32 GuardPostIndex, FVector& OutLocation) const;
	// World location of a socket of the pit mesh at its built height.
	bool TryGetBuiltSocketLocation(const FName SocketName, FVector& OutLocation) const;

	virtual void Tick(float DeltaSeconds) override;

	// Hits on the sandbags hurt the gun they protect; the gun's dig-in protection applies to them.
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator,
	                         AActor* DamageCauser) override;

private:
	// The pit's place on the ground; the mesh rises relative to it while the wall is built.
	UPROPERTY(VisibleDefaultsOnly, Category = "DigIn")
	TObjectPtr<USceneComponent> M_PitRoot;

	UPROPERTY(VisibleDefaultsOnly, Category = "DigIn")
	TObjectPtr<UStaticMeshComponent> M_WallMesh;

	UPROPERTY()
	TArray<TObjectPtr<URTSNavCollisionEnvObstacle>> M_NavObstacles;

	UPROPERTY()
	TWeakObjectPtr<UTeamWeaponDigInComponent> M_OwningDigIn;

	UPROPERTY()
	FTeamWeaponDigInWallBuildState M_BuildState;

	bool GetIsValidWallMesh() const;
	bool GetIsValidOwningDigIn() const;
	void SetupWallCollision(const uint8 OwningPlayer) const;
	void CreateNavObstacles();
	void SetRiseProgress(const float Progress) const;
	void CompleteBuilding();
	bool TryGetSocketRelativeLocation(const FName SocketName, FVector& OutRelativeLocation) const;
};
