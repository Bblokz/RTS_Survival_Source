// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "GliderTankDelivery.generated.h"

class ATankMaster;
class UNiagaraComponent;
class UNiagaraSystem;
class UStaticMesh;
class UStaticMeshComponent;
class UFowComp;
class AGliderTankDelivery;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGliderTankDelivered, AGliderTankDelivery*, ATankMaster*);

UENUM()
enum class EGliderDeliveryPhase : uint8
{
	None,
	TowedApproach,
	Gliding,
	Skidding,
	OpeningNose,
	Lingering,
	Sinking
};

/** @brief Everything a delivery needs to know about one landing run; filled in by the global ability. */
USTRUCT()
struct FGliderDeliveryOrder
{
	GENERATED_BODY()

	// Ground point where the glider touches down.
	FVector TouchdownLocation = FVector::ZeroVector;

	// Ground point where the skid ends; the tank drives out from here.
	FVector StopLocation = FVector::ZeroVector;

	// Horizontal distance from the map edge the tow starts at.
	float ApproachDistance = 0.f;

	int32 OwningPlayer = INDEX_NONE;

	FTrainingOption TankOption;

	// Spawns exactly this class when set, like FAircraftDropTankWithOffset::TankClassOverride.
	TSoftClassPtr<ATankMaster> TankClassOverride;
};

/** @brief Speeds, heights and timings of the run; set in the Blueprint defaults. */
USTRUCT(BlueprintType)
struct FGliderDeliveryTuning
{
	GENERATED_BODY()

	// Height above the touchdown point where the tug releases the glider.
	UPROPERTY(EditDefaultsOnly, meta=(ClampMin="0"))
	float ReleaseAltitude = 3000.f;

	// Horizontal distance of the glide from the release point to touchdown.
	UPROPERTY(EditDefaultsOnly, meta=(ClampMin="1"))
	float GlideDistance = 16000.f;

	UPROPERTY(EditDefaultsOnly, meta=(ClampMin="1"))
	float TowSpeed = 2600.f;

	UPROPERTY(EditDefaultsOnly, meta=(ClampMin="1"))
	float TouchdownSpeed = 1700.f;

	// Cable length between the glider's tow point and the tug's hook.
	UPROPERTY(EditDefaultsOnly, meta=(ClampMin="0"))
	float TowCableLength = 3000.f;

	// The tug flies a little higher than the glider so the cable reads from the RTS camera.
	UPROPERTY(EditDefaultsOnly)
	float TugHeightAboveGlider = 300.f;

	UPROPERTY(EditDefaultsOnly, meta=(ClampMin="0"))
	float TugClimbRate = 450.f;

	UPROPERTY(EditDefaultsOnly, meta=(ClampMin="0"))
	float TugTurnRateDegrees = 14.f;

	UPROPERTY(EditDefaultsOnly, meta=(ClampMin="0"))
	float TugBankDegrees = 25.f;

	// The tug is removed this long after the release; it is off the map by then.
	UPROPERTY(EditDefaultsOnly, meta=(ClampMin="0"))
	float TugLifetimeAfterRelease = 22.f;

	UPROPERTY(EditDefaultsOnly)
	float PropDegreesPerSecond = 1800.f;

	// Vision bubble around the glider for its owner, so the player sees the approach and landing in the fog.
	UPROPERTY(EditDefaultsOnly, meta=(ClampMin="0"))
	float VisionRadius = 4000.f;

	// Track smoke speed multiplier on the skid dust; the tank chassis uses 1-3.
	UPROPERTY(EditDefaultsOnly, meta=(ClampMin="0"))
	float SkidDustSpeedMultiplier = 3.f;

	// Nose-up attitude while skidding, so the belly skids take the ground first.
	UPROPERTY(EditDefaultsOnly)
	float SkidPitchDegrees = 2.f;

	// Distance in front of and behind the glider at which the ground is sampled for its pitch.
	UPROPERTY(EditDefaultsOnly, meta=(ClampMin="1"))
	float GroundSampleDistance = 1100.f;

	UPROPERTY(EditDefaultsOnly, meta=(ClampMin="0.1"))
	float NoseOpenSeconds = 2.5f;

	// Pitch of the nose around its hinge when fully open; past 90 it folds back over the roof.
	UPROPERTY(EditDefaultsOnly)
	float NoseOpenPitchDegrees = 118.f;

	// How far past the stop point the tank drives on its own after it is released.
	UPROPERTY(EditDefaultsOnly, meta=(ClampMin="0"))
	float TankExitDistance = 2600.f;

	// The empty glider stays on the field this long before it sinks away.
	UPROPERTY(EditDefaultsOnly, meta=(ClampMin="0"))
	float LingerSeconds = 45.f;

	UPROPERTY(EditDefaultsOnly, meta=(ClampMin="0.1"))
	float SinkSeconds = 8.f;

	UPROPERTY(EditDefaultsOnly, meta=(ClampMin="0"))
	float SinkDepth = 900.f;
};

/** @brief Where the parts sit relative to the glider's ground contact point (actor space, X forward, cm). */
USTRUCT(BlueprintType)
struct FGliderDeliveryLayout
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly)
	FVector NoseHingeOffset = FVector(1100.f, 0.f, 530.f);

	UPROPERTY(EditDefaultsOnly)
	FVector TowAttachOffset = FVector(1540.f, 0.f, 300.f);

	// Tow hook on the tug relative to the tug mesh origin.
	UPROPERTY(EditDefaultsOnly)
	FVector TugTowHookOffset = FVector(-270.f, 0.f, -30.f);

	// Propeller hubs on the tug relative to the tug mesh origin (He 111 Z: outer, inner and centre engines).
	UPROPERTY(EditDefaultsOnly)
	TArray<FVector> TugPropOffsets = {
		FVector(555.f, -930.f, -35.f), FVector(555.f, -270.f, -35.f), FVector(555.f, 0.f, -35.f),
		FVector(555.f, 270.f, -35.f), FVector(555.f, 930.f, -35.f)
	};

	// Where the tank is placed inside the hull before the nose opens.
	UPROPERTY(EditDefaultsOnly)
	FVector TankSpawnOffset = FVector(150.f, 0.f, 0.f);

	UPROPERTY(EditDefaultsOnly)
	FVector SkidDustOffset = FVector(-200.f, 0.f, 0.f);

	// Radius of the tow cable.
	UPROPERTY(EditDefaultsOnly, meta=(ClampMin="0.1"))
	float CableRadius = 9.f;
};

/** @brief Run state of one delivery; reset by StartDelivery. */
USTRUCT()
struct FGliderDeliveryRunState
{
	GENERATED_BODY()

	EGliderDeliveryPhase Phase = EGliderDeliveryPhase::None;
	FVector Direction = FVector::ForwardVector;
	FVector ReleaseLocation = FVector::ZeroVector;
	float SkidLength = 0.f;
	float SkidTravelled = 0.f;
	float SkidDeceleration = 0.f;
	float CurrentSpeed = 0.f;
	float PhaseElapsed = 0.f;
	float TugElapsedSinceRelease = 0.f;
	float TugYawTurned = 0.f;
	FVector TugVelocity = FVector::ZeroVector;
	FVector SinkStartLocation = FVector::ZeroVector;
	bool bM_TugReleased = false;
	bool bM_TankRequested = false;
	bool bM_TankReleased = false;
	bool bM_HasBroadcastDelivered = false;
};

/**
 * @brief Scripted Mammut delivery: an He 111 Z tows the glider in, releases it, the glider skids to a stop on the
 * ordered run, opens its nose and lets the tank drive out. Spawned by UGA_GliderTankDrop, which calls StartDelivery.
 * The meshes and offsets are set in the Blueprint defaults; nothing here is selectable or blocks units.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API AGliderTankDelivery : public AActor
{
	GENERATED_BODY()

public:
	AGliderTankDelivery();

	/**
	 * @brief Places the tow at the start of its approach and starts the run.
	 * @param Order Landing run, owner and tank to deliver.
	 */
	void StartDelivery(const FGliderDeliveryOrder& Order);

	EGliderDeliveryPhase GetDeliveryPhase() const { return M_Run.Phase; }

	// How far the tank drives out of the nose on its own; read from the class defaults to plan a landing run.
	float GetTankExitDistance() const { return M_Tuning.TankExitDistance; }

	/** The tank this delivery carries once it spawned in the hull; null before that or when it failed. */
	ATankMaster* GetDeliveredTank() const { return M_DeliveredTank.Get(); }

	/** Where the released tank drives to: TankExitDistance ahead of the glider along its landing run. */
	FVector GetTankExitLocation() const { return GetActorLocation() + M_Run.Direction * M_Tuning.TankExitDistance; }

	// Fires once: with the tank when it drives out, or with null when the delivery failed.
	FOnGliderTankDelivered OnTankDelivered;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Glider")
	TObjectPtr<USceneComponent> M_Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Glider")
	TObjectPtr<UStaticMeshComponent> M_GliderBody;

	// Pivots on the hinge; its mesh origin is the hinge.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Glider")
	TObjectPtr<UStaticMeshComponent> M_GliderNose;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Glider")
	TObjectPtr<UStaticMeshComponent> M_TugBody;

	// Stretched between the glider and the tug while towing; the engine cylinder (100 cm tall, 50 cm radius).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Glider")
	TObjectPtr<UStaticMeshComponent> M_TowCable;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Glider")
	TObjectPtr<UNiagaraComponent> M_SkidDust;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Glider")
	TObjectPtr<UFowComp> M_FowComponent;

	UPROPERTY(EditDefaultsOnly, Category="Glider")
	TObjectPtr<UStaticMesh> M_TugPropMesh;

	UPROPERTY(EditDefaultsOnly, Category="Glider")
	FGliderDeliveryTuning M_Tuning;

	UPROPERTY(EditDefaultsOnly, Category="Glider")
	FGliderDeliveryLayout M_Layout;

private:
	UPROPERTY()
	FGliderDeliveryRunState M_Run;

	UPROPERTY()
	FGliderDeliveryOrder M_Order;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> M_TugProps;

	UPROPERTY()
	TWeakObjectPtr<ATankMaster> M_DeliveredTank;

	FTimerHandle M_LingerTimerHandle;

	void BeginPlay_SetupNoCollision() const;
	void BeginPlay_CreateTugProps();
	void PlaceTugOnCable() const;

	void EnterPhase(EGliderDeliveryPhase NewPhase);
	void TickTowedApproach(float DeltaSeconds);
	void TickGliding(float DeltaSeconds);
	void TickSkidding(float DeltaSeconds);
	void TickOpeningNose(float DeltaSeconds);
	void TickSinking(float DeltaSeconds);
	void TickReleasedTug(float DeltaSeconds);
	void SpinProps(float DeltaSeconds) const;
	void UpdateTowCable() const;
	void ReleaseTow();
	void StartFowForOwner() const;
	void StartSkidDust() const;

	/**
	 * @brief Puts the glider on the ground below a point of the run, pitched to the slope under it.
	 * @param RunLocation Point on the run; only X and Y are used.
	 */
	void PlaceGliderOnGround(const FVector& RunLocation);
	float GetGroundHeight(const FVector& Location, float Fallback) const;
	void SetGliderTransform(const FVector& Location, float PitchDegrees);

	void RequestTankSpawn();
	void OnTankSpawned(AActor* SpawnedActor);
	void PrepareTankInsideHull(ATankMaster* Tank) const;
	void ReleaseTankIfReady();
	void BroadcastDelivered(ATankMaster* Tank);
	void StartLinger();

	FVector GetTugHookWorldLocation() const;
	FVector GetTowAttachWorldLocation() const;

	bool GetIsValidGliderBody() const;
	bool GetIsValidGliderNose() const;
	bool GetIsValidTugBody() const;
	bool GetIsValidTowCable() const;
};
