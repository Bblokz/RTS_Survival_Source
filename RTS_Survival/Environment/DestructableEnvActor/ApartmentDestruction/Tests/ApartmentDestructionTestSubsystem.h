// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/GeometryDamageComponent/GeometryDamageComponent.h"
#include "Subsystems/WorldSubsystem.h"

#include "ApartmentDestructionTestSubsystem.generated.h"

class AApartmentDestructibleActor;
class ACameraActor;
class AGeometryCollectionActor;
class UGeometryCollectionComponent;
struct FChaosBreakEvent;

/**
 * @brief Geometry damage tuned for a building made of 3 m kit panels instead of a vehicle hull: the strain radius has to
 * reach the centre of the nearest panel wherever a shell lands, because Chaos samples fields at piece centres.
 */
UCLASS()
class RTS_SURVIVAL_API UApartmentTestGeometryDamageComponent : public UGeometryDamageComponent
{
	GENERATED_BODY()

public:
	UApartmentTestGeometryDamageComponent();
};

/** @brief One scripted hit of the prototype run, relative to the centre of the building's footprint on the ground. */
USTRUCT()
struct FApartmentDestructionTestImpact
{
	GENERATED_BODY()

	FString Label;
	FVector OffsetFromSite = FVector::ZeroVector;
	float DamageAmount = 0.f;
	// Hits fired on consecutive frames at the same spot, to see how focused fire accumulates.
	int32 RepeatCount = 1;
	// Where the shell comes from, relative to the impact point; the direct-strain mode traces along it to find the
	// piece that is actually hit.
	FVector TraceStartOffset = FVector::ZeroVector;
};

/** @brief Frame cost and break counts gathered between two scripted hits. */
USTRUCT()
struct FApartmentDestructionTestWindow
{
	GENERATED_BODY()

	int32 FrameCount = 0;
	double TotalFrameSeconds = 0.0;
	double WorstFrameSeconds = 0.0;
	int32 BreakEventCount = 0;
};

/**
 * @brief Command-line prototype run for a destructible apartment: started with -ApartmentGcPrototypeTest it spawns the
 * C2 geometry collection with a GeometryDamageComponent, fires scripted hits, logs RTS_APARTMENT_GC lines and exits.
 * Add -ApartmentGcDirect to test an anchored dynamic collection damaged through the per-piece strain API of the
 * engine instead of a kinematic collection driven by the GeometryDamageComponent.
 */
UCLASS()
class RTS_SURVIVAL_API UApartmentDestructionTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	// The map boots paused; the run has to tick to lift that pause itself.
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	UFUNCTION()
	void OnChaosBreak(const FChaosBreakEvent& BreakEvent);

	bool Tick_UnpauseAndSpawn(UWorld& World);
	void SpawnPrototype(UWorld& World);
	void BuildImpactScript();
	void UpdateCamera(UWorld& World) const;
	void AccumulateFrame();
	void AdvanceScript(UWorld& World);
	void FireImpact(const FApartmentDestructionTestImpact& Impact) const;
	/**
	 * @brief Applies damage the way a projectile would: trace to the collection, then lower the internal strain
	 * (remaining health) of the panels around the impact point of whichever body was hit.
	 * @param Impact The scripted hit.
	 * @param WorldLocation The aimed impact point.
	 */
	void FireImpactDirect(const FApartmentDestructionTestImpact& Impact, const FVector& WorldLocation) const;
	// Spawns the placeable gameplay actor instead of a bare collection, to test the path a real projectile takes.
	void SpawnGameplayActor(UWorld& World);
	/**
	 * @brief Hits the gameplay actor the way a projectile does: a weapon-channel world trace picks the actor, which
	 * then only receives point damage at the hit location.
	 * @param Impact The scripted hit.
	 * @param WorldLocation The aimed impact point.
	 */
	void FireImpactAsProjectile(const FApartmentDestructionTestImpact& Impact, const FVector& WorldLocation) const;
	UGeometryCollectionComponent* GetTestedGeometryComponent() const;
	void ReportWindow(const UWorld& World, const FString& Label);
	void RequestScreenshot(const FString& Label);
	void Finish(UWorld& World);

	UPROPERTY()
	TObjectPtr<AGeometryCollectionActor> M_PrototypeActor = nullptr;
	bool GetIsValidPrototypeActor() const;

	UPROPERTY()
	TObjectPtr<AApartmentDestructibleActor> M_GameplayActor = nullptr;

	UPROPERTY()
	TObjectPtr<UGeometryDamageComponent> M_GeometryDamage = nullptr;
	bool GetIsValidGeometryDamage() const;

	UPROPERTY()
	TObjectPtr<ACameraActor> M_Camera = nullptr;

	TArray<FApartmentDestructionTestImpact> M_Impacts;
	// Prefix of this run's screenshots, so several configurations can be compared side by side.
	FString M_RunName;
	FApartmentDestructionTestWindow M_Window;

	// Index of the next hit to fire; -1 while the intact building is still being observed.
	int32 M_NextImpactIndex = INDEX_NONE;
	int32 M_ScreenshotCount = 0;
	int32 M_TotalBreakEvents = 0;
	float M_NextStepSeconds = 0.f;
	bool bM_HasSpawned = false;
	bool bM_IsFinished = false;
	bool bM_HasLoggedFirstTick = false;
	bool bM_UseDynamicAnchored = false;
	// Damage goes through the per-piece strain API of the engine instead of the GeometryDamageComponent fields.
	bool bM_UseDirectStrain = false;
	// Tests AApartmentDestructibleActor through TakeDamage instead of a bare geometry collection actor.
	bool bM_UseGameplayActor = false;
};
