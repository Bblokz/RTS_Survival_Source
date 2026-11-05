// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "ParachutedResourcePayload.generated.h"

class ACPPResourceMaster;
class AScavengeableObject;
class UNiagaraSystem;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;

/** @brief What lands where a parachuted payload touches down. */
UENUM(BlueprintType)
enum class EResourceDropPayloadKind : uint8
{
	// A crate that infantry with a scavenger component empties for fixed rewards.
	ScavengeContainer,
	// A radixite deposit that harvesters mine and bring to a refinery like any map resource.
	HarvestableDeposit
};

/** @brief Looks and timing of a payload on its way down. */
USTRUCT(BlueprintType)
struct FResourceDropFallSettings
{
	GENERATED_BODY()

	// Hangs below the canopy; the pivot is expected at the bottom of the mesh, which is where it touches the ground.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UStaticMesh> FallingMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.01"))
	float FallingMeshScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UStaticMesh> ParachuteMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.01"))
	float ParachuteScale = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.1"))
	float FallSeconds = 6.f;

	// Played where the payload lands: a puff of dust.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UNiagaraSystem> LandingEffect = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<USoundBase> LandingSound = nullptr;
};

/** @brief A scavengeable crate: the generic scavenge object Blueprint with the drop's own mesh and fixed rewards. */
USTRUCT(BlueprintType)
struct FResourceDropContainerSettings
{
	GENERATED_BODY()

	// Must set its own scavenge mesh in its construction script (BP_ScavengeableObject does, on its "Mesh").
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftClassPtr<AScavengeableObject> ContainerClass;

	// Put on the container's scavenge mesh after spawning; empty keeps the Blueprint's mesh.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UStaticMesh> ContainerMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.01"))
	float ContainerMeshScale = 1.f;

	// Fixed reward of one container (before the scavenging squad's reward multiplier; rounded to 5 by scavenging).
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<ERTSResourceType, int32> RewardsPerContainer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1.0"))
	float ScavengeSeconds = 30.f;

	// Player scavenger squads that walk this close start emptying the container on their own; 0 adds no trigger.
	// Keep it below half the spacing of the crates: the trigger also takes a squad off a neighbouring crate.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float AutoScavengeRadius = 0.f;

	// The emptied container stays this long so its reward text can play out, then it is removed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
	float RemoveAfterEmptiedSeconds = 7.f;
};

/** @brief A harvestable deposit: an existing resource Blueprint with its amount replaced. */
USTRUCT(BlueprintType)
struct FResourceDropDepositSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftClassPtr<ACPPResourceMaster> DepositClass;

	// Replaces what the Blueprint's construction script rolled, so the drop is worth the same every time.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
	int32 DepositAmount = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.01"))
	float DepositActorScale = 1.f;
};

/** @brief Everything one resource drop delivers; set on the ability template. */
USTRUCT(BlueprintType)
struct FResourceDropSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EResourceDropPayloadKind PayloadKind = EResourceDropPayloadKind::ScavengeContainer;

	// The same number goes down on any strip, evenly spread along it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
	int32 PayloadCount = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FResourceDropFallSettings FallSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FResourceDropContainerSettings ContainerSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FResourceDropDepositSettings DepositSettings;
};

class AParachutedResourcePayload;
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnResourcePayloadLanded, AParachutedResourcePayload* /*Payload*/,
                                     AActor* /*SpawnedResource*/);

/**
 * @brief A supply container or radixite cache on its way down under a parachute. It is not the resource yet: the
 * scavengeable container or harvestable deposit is spawned where it lands, so nothing can be emptied in the air.
 * Spawned from code only (UGA_ResourceDrop).
 */
UCLASS(NotBlueprintable)
class RTS_SURVIVAL_API AParachutedResourcePayload : public AActor
{
	GENERATED_BODY()

public:
	AParachutedResourcePayload();

	/**
	 * @brief Starts the fall; call once right after spawning.
	 * @param ReleaseLocation Where it leaves the aircraft.
	 * @param GroundLocation Where the payload ends up.
	 * @param PayloadClass Loaded container or deposit class to place on landing.
	 * @param Settings Kind, looks, timing and contents.
	 */
	void StartFall(const FVector& ReleaseLocation, const FVector& GroundLocation, UClass* PayloadClass,
	               const FResourceDropSettings& Settings);

	// Broadcast once on landing; the spawned resource is null when it could not be spawned.
	FOnResourcePayloadLanded OnPayloadLanded;

protected:
	virtual void Tick(float DeltaSeconds) override;

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> M_Root;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_FallingMesh;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_ParachuteMesh;

	UPROPERTY()
	TObjectPtr<UClass> M_PayloadClass;

	UPROPERTY()
	FResourceDropSettings M_Settings;

	FVector M_ReleaseLocation = FVector::ZeroVector;
	FVector M_GroundLocation = FVector::ZeroVector;
	float M_SecondsFalling = 0.f;

	void StartFall_PlaceParachuteAboveMesh() const;
	void Land();
	AActor* SpawnLandedResource() const;
};

/** @brief Spawns what a resource drop leaves on the ground; shared by the falling payload and tests. */
namespace ResourceDropSpawning
{
	/**
	 * @brief Spawns a scavengeable container with the drop's mesh, fixed rewards and auto-scavenge trigger; it removes
	 * itself a few seconds after it was emptied or destroyed.
	 * @return The container, or null when the class could not be spawned.
	 */
	RTS_SURVIVAL_API AScavengeableObject* SpawnScavengeContainer(UWorld& World, UClass* ContainerClass,
	                                                            const FTransform& Transform,
	                                                            const FResourceDropContainerSettings& Settings);

	/**
	 * @brief Spawns a harvestable deposit and replaces its rolled amount with the configured one.
	 * @return The deposit, or null when the class could not be spawned.
	 */
	RTS_SURVIVAL_API ACPPResourceMaster* SpawnHarvestableDeposit(UWorld& World, UClass* DepositClass,
	                                                            const FTransform& Transform,
	                                                            const FResourceDropDepositSettings& Settings);
}
