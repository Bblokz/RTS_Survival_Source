// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TeamWeaponDigInComponent.generated.h"

class ATeamWeapon;
class ATeamWeaponDigInWall;
class UHealthComponent;
class UStaticMesh;
struct FStreamableHandle;

UENUM()
enum class ETeamWeaponDigInStatus : uint8
{
	None,
	// The gun's pit mesh is loading; the wall spawns when it is in.
	LoadingWall,
	// The sandbags rise around the gun; no protection yet.
	Building,
	// The pit stands and the gun has its extra health and damage reduction.
	DugIn
};

namespace TeamWeaponDigInDefaults
{
	constexpr float MaxHealthMultiplier = 1.33f;
	constexpr float DamageTakenMultiplier = 0.8f;
	constexpr float BuildSeconds = 10.f;
}

/** What the dig-in added to the gun's health component; taken off again by the same factors when the pit goes. */
USTRUCT()
struct FTeamWeaponDigInProtection
{
	GENERATED_BODY()

	float AppliedMaxHealthMultiplier = 1.f;
	float AppliedDamageTakenMultiplier = 1.f;
	bool bIsApplied = false;
};

/**
 * @brief Sandbag dig-in of a team weapon: spawns the gun's pit (ATeamWeaponDigInWall), raises the gun's max health and
 * lowers the damage it takes while the pit stands, and takes both off again when the pit is removed for any reason.
 * Created by ATeamWeapon; the squad controller starts and removes it and gets OnDigInCompleted through IDigInUnit.
 * The pit mesh is /Game/RTS_Survival/Meshes/Props/TeamWeaponDigIn/SM_TWDigIn_<gun Blueprint> unless overridden.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UTeamWeaponDigInComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTeamWeaponDigInComponent();

	/** @return False when this gun has no pit mesh; an active dig-in is left as it is and counts as started. */
	bool StartDigIn();
	// Removes the pit and the protection; safe to call in any state.
	void RemoveDigIn();

	ETeamWeaponDigInStatus GetDigInStatus() const { return M_Status; }
	bool GetIsDigInActive() const { return M_Status != ETeamWeaponDigInStatus::None; }
	ATeamWeaponDigInWall* GetDigInWall() const { return M_Wall.Get(); }
	float GetMaxHealthMultiplier() const { return MaxHealthMultiplier; }
	float GetDamageTakenMultiplier() const { return DamageTakenMultiplier; }

	/**
	 * @brief Where a guard stands inside the pit; guards beyond the pit's posts share them, a little apart.
	 * @param GuardIndex Index of the guard in the crew.
	 * @param OutLocation World location of the post.
	 * @return False while there is no pit or it has no guard posts.
	 */
	bool TryGetGuardPostLocation(const int32 GuardIndex, FVector& OutLocation) const;

	// Called by the wall once it stands.
	void OnWallBuilt(ATeamWeaponDigInWall& Wall);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Pit mesh to use instead of the one named after the gun Blueprint.
	UPROPERTY(EditDefaultsOnly, Category = "DigIn")
	TSoftObjectPtr<UStaticMesh> WallMeshOverride;

	UPROPERTY(EditDefaultsOnly, Category = "DigIn", meta = (ClampMin = "0.0"))
	float BuildSeconds = TeamWeaponDigInDefaults::BuildSeconds;

	UPROPERTY(EditDefaultsOnly, Category = "DigIn", meta = (ClampMin = "1.0"))
	float MaxHealthMultiplier = TeamWeaponDigInDefaults::MaxHealthMultiplier;

	// Multiplies the damage the gun takes while dug in.
	UPROPERTY(EditDefaultsOnly, Category = "DigIn", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float DamageTakenMultiplier = TeamWeaponDigInDefaults::DamageTakenMultiplier;

private:
	ETeamWeaponDigInStatus M_Status = ETeamWeaponDigInStatus::None;

	UPROPERTY()
	TWeakObjectPtr<ATeamWeaponDigInWall> M_Wall;

	UPROPERTY()
	FTeamWeaponDigInProtection M_Protection;

	TSharedPtr<FStreamableHandle> M_WallMeshLoadHandle;

	ATeamWeapon* GetTeamWeapon() const;
	UHealthComponent* GetHealthComponent() const;
	FSoftObjectPath FindWallMeshPath() const;
	void OnWallMeshLoaded(const FSoftObjectPath WallMeshPath);
	bool SpawnWall(UStaticMesh& WallMesh);

	/**
	 * @brief Places the pit on the gun's heading, tilted to the ground under it and sunk into it far enough that a
	 * slope shows no gap; the pit's footing reaches below the ground for that.
	 * @param WallMesh The pit mesh; its bounds give where the ground is sampled.
	 * @param OutTransform The pit's world transform.
	 * @return False when the ground could not be sampled; the transform is then the gun's upright heading.
	 */
	bool TryGetWallTransform(const UStaticMesh& WallMesh, FTransform& OutTransform) const;

	/**
	 * @brief Ground heights on a grid under the pit, relative to the gun, in the gun's heading frame.
	 * @param TeamWeapon The gun; its ground trace steps through units.
	 * @param Heading The gun's yaw.
	 * @param Bounds The pit mesh bounds.
	 * @param OutLocalGroundPoints x, y in the pit frame and z = ground height minus the gun's height.
	 * @return False when fewer than three points found the ground.
	 */
	bool TrySampleGroundUnderPit(const ATeamWeapon& TeamWeapon, const FRotator& Heading, const FBox& Bounds,
	                             TArray<FVector>& OutLocalGroundPoints) const;
	void ApplyProtection();
	void RemoveProtection();
	void FailDigIn(const FString& Reason);
	void NotifyControllerDigInCompleted() const;
	uint8 GetOwningPlayer() const;
};
