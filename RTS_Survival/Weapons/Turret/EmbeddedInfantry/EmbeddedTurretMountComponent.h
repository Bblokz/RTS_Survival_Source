// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
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
	TankHullStaticMesh
};

/**
 * @brief Add to a tank or building expansion Blueprint to give it an extra turret at a mesh socket at runtime,
 * e.g. an infantry gunner (AEmbeddedInfantryTurret) in a cupola. The spawned turret is registered with the host
 * like its own turrets, follows its attack orders and auto-engages.
 * @note Set EmbeddedTurretClass and MountSocketName in the Blueprint component details.
 */
UCLASS(ClassGroup = (RTS), meta = (BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UEmbeddedTurretMountComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEmbeddedTurretMountComponent();

	ACPPTurretsMaster* GetMountedTurret() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Embedded Turret Mount")
	TSubclassOf<ACPPTurretsMaster> EmbeddedTurretClass;

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

private:
	// The host's own turrets and meshes only all exist (and finished their BeginPlay) a tick later.
	void BeginPlay_ScheduleMount();
	void MountTurret();

	UMeshComponent* FindMountMesh() const;
	UMeshComponent* FindTankTurretMeshWithSocket(const TSubclassOf<UMeshComponent> MeshClass) const;
	UMeshComponent* FindOwnerMeshWithSocket(const TSubclassOf<UMeshComponent> MeshClass) const;
	bool GetMeshHasMountSocket(const UMeshComponent* Mesh, const TSubclassOf<UMeshComponent> MeshClass) const;

	UChildActorComponent* CreateMountChildActorComponent(UMeshComponent* TargetMesh);
	void RegisterTurretWithOwner(ACPPTurretsMaster* Turret) const;
	void ReportMountMeshNotFound() const;

	bool GetIsValidMountChildActorComponent() const;

	// Created on the owner at runtime so the turret's GetParentActor is the host (owning player, turret owner).
	UPROPERTY()
	TObjectPtr<UChildActorComponent> M_MountChildActorComponent;

	FTimerHandle M_MountTimerHandle;
};
