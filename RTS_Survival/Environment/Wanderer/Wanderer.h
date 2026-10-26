// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Environment/DestructableEnvActor/DestructableEnvActor.h"
#include "Wanderer.generated.h"

class UStaticMeshComponent;
class UNavigationSystemV1;

// Custom primitive data slots read by the VAT material (M_FarmAnimal_VAT); keep both in sync.
namespace WandererVatPrimitiveData
{
	constexpr int32 StartRow = 0;
	constexpr int32 NumFrames = 1;
	constexpr int32 StartTime = 2;
	constexpr int32 Loop = 3;
	constexpr int32 FramesPerSecond = 4;
}

/** One clip inside the vertex animation textures, in texture rows as written by the Blender bake. */
USTRUCT(BlueprintType)
struct FWandererVatClip
{
	GENERATED_BODY()

	// First texture row of the clip (0 = first baked frame).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VAT", meta=(ClampMin="0"))
	int32 StartRow = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VAT", meta=(ClampMin="1"))
	int32 NumFrames = 1;

	float GetDuration(const float FramesPerSecond) const;
};

/** The clips every wanderer needs; the row ranges come from the <Animations> block of the bake XML. */
USTRUCT(BlueprintType)
struct FWandererVatSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VAT")
	float FramesPerSecond = 30.f;

	// Looping walk cycle, played while moving.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VAT")
	FWandererVatClip WalkClip;

	// Idle clip that starts and ends in the neutral stance; looped a random number of times while resting.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VAT")
	FWandererVatClip IdleClip;

	// Played once on death; the last frame is held.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VAT")
	FWandererVatClip DeathClip;
};

USTRUCT(BlueprintType)
struct FWandererMovementSettings
{
	GENERATED_BODY()

	// Destinations are picked within this radius around the location the wanderer started at.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wander", meta=(ClampMin="0"))
	float WanderRadius = 1500.f;

	// New destinations closer than this to the current location are rejected.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wander", meta=(ClampMin="0"))
	float MinWalkDistance = 300.f;

	// Should match the stride of the walk clip (the Blender report prints it) to avoid foot sliding.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wander", meta=(ClampMin="1"))
	float WalkSpeed = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wander", meta=(ClampMin="1"))
	float TurnSpeedDegrees = 90.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wander", meta=(ClampMin="1"))
	int32 MinIdleLoops = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wander", meta=(ClampMin="1"))
	int32 MaxIdleLoops = 3;

	// Follow navmesh paths when a navmesh exists; otherwise walk in a straight line.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wander")
	bool bUseNavigation = true;

	// When damaged but alive, stop idling and walk to a new destination immediately.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wander")
	bool bStartWalkingWhenDamaged = true;
};

USTRUCT(BlueprintType)
struct FWandererDeathSettings
{
	GENERATED_BODY()

	// Seconds the corpse stays after the death clip finished; zero or less keeps it forever.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Death")
	float CorpseLifeTime = 120.f;

	// Tanks driving over the wanderer kill it (plays the regular death clip).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Death")
	bool bDieWhenCrushedByTanks = true;
};

UENUM(BlueprintType)
enum class EWandererState : uint8
{
	Idle,
	Walking,
	Dead
};

/**
 * @brief Ambient animal (sheep, cow, ...) that wanders around its start location using a VAT static mesh.
 * Place the Blueprint child in a level, set the mesh, VAT clip rows and health; weapons of both sides can kill it.
 * Clip playback is pushed to the VAT material through custom primitive data (see WandererVatPrimitiveData).
 */
UCLASS()
class RTS_SURVIVAL_API AWanderer : public ADestructableEnvActor
{
	GENERATED_BODY()

public:
	AWanderer(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Wander")
	EWandererState GetWandererState() const { return M_State; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category="Wander")
	FVector GetWanderOrigin() const { return M_WanderOrigin; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnUnitDies(const ERTSDeathType DeathType) override;
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator,
	                         AActor* DamageCauser) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Wander")
	TObjectPtr<UStaticMeshComponent> AnimalMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wander")
	FWandererMovementSettings MovementSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wander")
	FWandererVatSettings VatSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wander")
	FWandererDeathSettings DeathSettings;

	// Optional colour variants (e.g. cow patterns); one is picked at random on BeginPlay. Empty keeps the mesh material.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wander")
	TArray<TObjectPtr<UMaterialInterface>> MaterialVariants;

private:
	void BeginPlay_SetupCollision();
	void BeginPlay_SetupAimOffsets();
	void BeginPlay_ApplyRandomMaterialVariant() const;

	void StartIdle(const bool bRandomizeDuration);
	void OnIdleFinished();
	bool TryStartWalking();
	bool BuildPathTo(const FVector& Destination);
	bool PickDestination(FVector& OutDestination) const;

	/** @return The navigation system when navigation is enabled and we stand on a navmesh, else nullptr. */
	UNavigationSystemV1* GetNavigationSystemIfOnNavMesh() const;
	void TickWalking(const float DeltaSeconds);
	void StopWalking();

	/**
	 * @brief Pushes a clip to the VAT material; the GPU derives the frame from game time.
	 * @param Clip Rows to play.
	 * @param bLoop Wrap around instead of holding the last frame.
	 * @param PhaseSeconds Seconds already elapsed in the clip (desyncs herds).
	 */
	void PlayVatClip(const FWandererVatClip& Clip, const bool bLoop, const float PhaseSeconds) const;

	/** @return The ground height under XY, or the current height if the trace misses. */
	float GetGroundHeight(const FVector& Location) const;

	void DisableCorpseCollision() const;

	bool GetIsValidAnimalMesh() const;

	EWandererState M_State = EWandererState::Idle;

	FVector M_WanderOrigin = FVector::ZeroVector;

	// Remaining path points (world space) of the current walk; index into it is M_PathIndex.
	TArray<FVector> M_PathPoints;
	int32 M_PathIndex = 0;

	// Walks that take much longer than expected are aborted (blocked by an obstacle).
	float M_WalkTimeLeft = 0.f;

	FTimerHandle M_IdleTimerHandle;
};
