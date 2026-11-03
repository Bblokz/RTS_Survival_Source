// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTSRagdollSubsystem.generated.h"

class ARTSPooledRagdoll;
class UMaterialInterface;
class UPhysicalMaterial;
class USkeletalMesh;
struct FRTSCrewBodyType;
struct FRTSCrewThrow;

enum class ERTSCrewSide : uint8
{
	German,
	Soviet
};

/** A dying vehicle asking for its crew to be thrown out. */
struct FRTSCrewBodyRequest
{
	// Top of the vehicle: bodies start above the hull, as one woken inside the wreck would be flung out violently.
	FVector TopCentre = FVector::ZeroVector;
	// How far around the centre the bodies may start.
	float SpreadRadius = 0.f;
	ERTSCrewSide Side = ERTSCrewSide::German;
	bool bIsCookOff = false;
};

/** The parked bodies and looks of one side. */
USTRUCT()
struct FRTSCrewPool
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<USkeletalMesh> Mesh;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInterface>> Materials;

	UPROPERTY()
	TArray<TObjectPtr<ARTSPooledRagdoll>> ParkedBodies;

	int32 BodiesLeftToPrewarm = 0;
};

/** A woken body on its way back to the pool. */
USTRUCT()
struct FRTSActiveCrewBody
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<ARTSPooledRagdoll> Body;

	int32 PoolIndex = 0;
	float SecondsAlive = 0.f;
	float SecondsSinking = 0.f;
	bool bIsSinking = false;
};

/**
 * @brief Throws crew bodies out of dying vehicles from a pool that never spawns in play, and counts every simulating
 * ragdoll, soldiers included, against one cap. Starting a body is cheap; simulating many at once is what costs frames.
 */
UCLASS()
class RTS_SURVIVAL_API URTSRagdollSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/**
	 * @brief Throws out as many bodies as the budget and the pool allow; zero is fine, as nobody counts them.
	 * @param Request The vehicle that died.
	 * @return How many bodies were thrown.
	 */
	int32 RequestCrewBodies(const FRTSCrewBodyRequest& Request);

	/** @return Whether one more ragdoll fits under the cap; soldiers that are refused keep their montage. */
	bool GetCanStartRagdoll() const;

	void NotifySoldierRagdollStarted() { ++M_SimulatingSoldiers; }
	void NotifySoldierRagdollEnded() { M_SimulatingSoldiers = FMath::Max(0, M_SimulatingSoldiers - 1); }

	int32 GetSimulatingRagdollCount() const { return M_SimulatingCrew + M_SimulatingSoldiers; }
	int32 GetParkedBodyCount(ERTSCrewSide Side) const;
	int32 GetBodiesRefused() const { return M_BodiesRefused; }

private:
	void LoadPool(FRTSCrewPool& OutPool, const FRTSCrewBodyType& CrewType, int32 PoolSize) const;
	void PrewarmStep();
	ARTSPooledRagdoll* SpawnParkedBody(FRTSCrewPool& Pool) const;
	int32 GetBodyCountForRequest(const FRTSCrewThrow& Throw) const;
	void WakeBody(int32 PoolIndex, const FRTSCrewBodyRequest& Request, const FRTSCrewThrow& Throw);
	void UpdateActiveBodies();
	void UpdateFlyingBody(FRTSActiveCrewBody& ActiveBody) const;
	bool UpdateSinkingBody(FRTSActiveCrewBody& ActiveBody);

	// One pool per ERTSCrewSide, in enum order.
	UPROPERTY()
	TArray<FRTSCrewPool> M_Pools;

	UPROPERTY()
	TArray<FRTSActiveCrewBody> M_ActiveBodies;

	UPROPERTY()
	TObjectPtr<UPhysicalMaterial> M_CorpseMaterial;

	FTimerHandle M_PrewarmTimer;
	FTimerHandle M_UpdateTimer;
	int32 M_SimulatingCrew = 0;
	int32 M_SimulatingSoldiers = 0;
	int32 M_BodiesRefused = 0;
};
