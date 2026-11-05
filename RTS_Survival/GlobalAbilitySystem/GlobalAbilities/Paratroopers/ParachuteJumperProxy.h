// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ParachuteJumperProxy.generated.h"

class ASquadUnit;
class UAnimMontage;
class USkeletalMeshComponent;
class UStaticMesh;

/**
 * @brief Looks like one soldier of a team weapon crew hanging on his parachute, with the soldier's own meshes and
 * weapon, but is not a unit. Used when the real soldier must stay on the ground: the team weapon controller keeps
 * crewing its gun (snapping operators to their posts, moving guards) while the drop is in the air, which would pull an
 * airborne soldier back to the ground. The real soldier waits hidden at his post and is shown when his proxy lands.
 */
UCLASS(NotBlueprintable)
class RTS_SURVIVAL_API AParachuteJumperProxy : public AActor
{
	GENERATED_BODY()

public:
	AParachuteJumperProxy();

	/**
	 * @brief Spawns a proxy of the soldier at a location, dressed as the soldier and hanging from the parachute.
	 * @param SquadUnit The soldier to copy; his skeletal mesh and everything attached to it are copied.
	 * @param ParachuteMesh Soldier parachute, attached to the proxy body like it is to a jumping soldier.
	 * @param ParachuteMontage Hanging montage; its first animation loops on the proxy.
	 * @param SpawnLocation Where the proxy appears (location of the soldier's mesh, at his feet).
	 * @param Yaw Facing of the proxy.
	 * @return The proxy, or null when the soldier has no skeletal mesh.
	 */
	static AParachuteJumperProxy* SpawnForSquadUnit(
		ASquadUnit* SquadUnit,
		UStaticMesh* ParachuteMesh,
		UAnimMontage* ParachuteMontage,
		const FVector& SpawnLocation,
		float Yaw);

	USkeletalMeshComponent* GetBodyMesh() const { return M_BodyMesh; }

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> M_Root;

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> M_BodyMesh;

	void CopyBody(const USkeletalMeshComponent& SourceMesh);
	void CopyAttachedMeshes(const ASquadUnit& SquadUnit, const USkeletalMeshComponent& SourceMesh);
	void CopyAttachedMesh(const UMeshComponent& SourcePart, const USkeletalMeshComponent& SourceMesh);
	void AttachParachute(UStaticMesh* ParachuteMesh);
	void PlayHangAnimation(const UAnimMontage* ParachuteMontage);
};
