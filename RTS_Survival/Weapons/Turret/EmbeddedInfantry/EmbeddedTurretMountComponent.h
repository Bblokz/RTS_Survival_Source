// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/TechTree/Technologies/Technologies.h"
#include "EmbeddedTurretMountComponent.generated.h"

class ACPPTurretsMaster;
class UChildActorComponent;
class UMeshComponent;

// Which mesh of the host tank carries the mount socket.
UENUM(BlueprintType)
enum class EEmbeddedTurretMountMesh : uint8
{
	// A skeletal mesh on one of the tank's turret actors.
	TankTurretSkeletalMesh,
	// A static mesh on one of the tank's turret actors (the turret body with the cupola on most tanks).
	TankTurretStaticMesh,
	// A static mesh on the tank itself.
	TankHullStaticMesh,
	// Any mesh of the tank's turret actors, else any mesh of the tank itself (the hull). One mount on a base tank then
	// serves variants whose turret lacks the socket, such as an open flak turret: the variant's hull carries it.
	TankTurretThenHullMesh
};

/**
 * @brief Add to a tank or building expansion Blueprint to give it an extra turret at a mesh socket at runtime,
 * e.g. an infantry gunner (AEmbeddedInfantryTurret) in a cupola. The spawned turret is registered with the host
 * like its own turrets, follows its attack orders and auto-engages.
 * With RequiredTechnology set the mount waits until that technology is applied to the host (a field modification
 * kit, see UTE_EmbeddedGunnerKit); UnmountOnTechnology removes a gunner the kit replaces.
 * @note Set EmbeddedTurretClasses and MountSocketName in the Blueprint component details.
 */
UCLASS(ClassGroup = (RTS), meta = (BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UEmbeddedTurretMountComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEmbeddedTurretMountComponent();

	ACPPTurretsMaster* GetMountedTurret() const;

	ETechnology GetRequiredTechnology() const { return RequiredTechnology; }
	ETechnology GetUnmountOnTechnology() const { return UnmountOnTechnology; }

	/**
	 * @brief Mounts the turret of a technology-gated mount. Safe to call more than once and before BeginPlay: the mount
	 * then happens a tick after BeginPlay, once the host's turret actors exist.
	 */
	void RequestMount();

	/** @brief Removes the mounted turret from the host for good, and cancels a mount that is still scheduled. */
	void Unmount();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// One class is picked at random per mount, so vehicles of the same type can carry different gunners.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Embedded Turret Mount")
	TArray<TSubclassOf<ACPPTurretsMaster>> EmbeddedTurretClasses;

	// True for a turret on a tank; picks the tank mesh with MountMesh. Otherwise any mesh of the owner with the socket.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Embedded Turret Mount")
	bool bIsTankInfantryTurret = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Embedded Turret Mount",
		meta = (EditCondition = "bIsTankInfantryTurret", EditConditionHides))
	EEmbeddedTurretMountMesh MountMesh = EEmbeddedTurretMountMesh::TankTurretStaticMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Embedded Turret Mount")
	FName MountSocketName = TEXT("Infantry");

	// Offset from the socket; the default sinks an infantry gunner so only his upper body shows above the hatch.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Embedded Turret Mount")
	FVector MountLocalOffset = FVector(0.f, 0.f, -80.f);

	// Turret actors of these classes are never searched for the socket. A variant can keep the base turret's collision
	// mesh (and with it the base's hatch socket) while its turret has no hatch, e.g. an open flak turret with its crew;
	// with TankTurretThenHullMesh the gunner then goes to the hull.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Embedded Turret Mount")
	TArray<TSubclassOf<ACPPTurretsMaster>> TurretClassesWithoutMount;

	// Tech_NONE mounts at BeginPlay. Otherwise the mount waits for this technology (a field modification kit).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Embedded Turret Mount|Field Kit")
	ETechnology RequiredTechnology = ETechnology::Tech_NONE;

	// This technology removes the mounted turret, e.g. a kit that replaces the variant's own gunner with its own.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Embedded Turret Mount|Field Kit")
	ETechnology UnmountOnTechnology = ETechnology::Tech_NONE;

private:
	// The host's own turrets and meshes only all exist (and finished their BeginPlay) a tick later.
	void BeginPlay_ScheduleMount();
	void ScheduleMount();
	void MountTurret();
	void UnregisterTurretFromOwner(ACPPTurretsMaster* Turret) const;

	TSubclassOf<ACPPTurretsMaster> GetRandomEmbeddedTurretClass() const;
	UMeshComponent* FindMountMesh() const;
	UMeshComponent* FindTankTurretMeshWithSocket(const TSubclassOf<UMeshComponent> MeshClass) const;
	UMeshComponent* FindOwnerMeshWithSocket(const TSubclassOf<UMeshComponent> MeshClass) const;
	UMeshComponent* FindTankTurretThenHullMeshWithSocket() const;
	bool GetMeshHasMountSocket(const UMeshComponent* Mesh, const TSubclassOf<UMeshComponent> MeshClass) const;
	bool GetIsTurretWithoutMount(const ACPPTurretsMaster* Turret) const;

	UChildActorComponent* CreateMountChildActorComponent(
		UMeshComponent* TargetMesh,
		const TSubclassOf<ACPPTurretsMaster> TurretClass);
	void RegisterTurretWithOwner(ACPPTurretsMaster* Turret) const;
	void ReportMountMeshNotFound() const;

	bool GetIsValidMountChildActorComponent() const;

	// Created on the owner at runtime so the turret's GetParentActor is the host (owning player, turret owner).
	UPROPERTY()
	TObjectPtr<UChildActorComponent> M_MountChildActorComponent;

	FTimerHandle M_MountTimerHandle;

	// RequestMount came before BeginPlay; BeginPlay schedules the mount.
	bool bM_IsMountRequested = false;

	// Unmount was called: no turret is mounted from then on, also not by a mount that was already scheduled.
	bool bM_IsMountBlocked = false;
};
