// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"
#include "FlareProjectile.generated.h"

class UFowComp;
class UNiagaraComponent;
class UNiagaraSystem;
class UPointLightComponent;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;

UENUM()
enum class EFlareProjectilePhase : uint8
{
	Inactive,
	// Flies from the launch tube to its height above the target.
	Ascending,
	// Burns under its parachute and gives vision while it sinks to the ground.
	Burning,
	// Out of time: no vision, the light fades and the actor is destroyed shortly after.
	BurntOut
};

/** @brief Looks of a flare; set on the flare aim ability component in the unit's Blueprint. */
USTRUCT(BlueprintType)
struct FFlareVisualSettings
{
	GENERATED_BODY()

	// The launcher (e.g. a smoke discharger rack) shown on the unit; attached to the launch socket at BeginPlay, so
	// its pivot is the mouth of the tube the flare leaves from. Optional.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UStaticMesh> LauncherMesh = nullptr;

	// The flare cartridge; hangs along -Z from its pivot (the hang point under the canopy), burning at the bottom.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UStaticMesh> FlareMesh = nullptr;

	// Canopy opened at the top of the flight; its pivot is where the shroud lines meet the cartridge.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UStaticMesh> ParachuteMesh = nullptr;

	// Played once at the launch socket: flash and smoke of the discharger tube.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UNiagaraSystem> LaunchEffect = nullptr;

	// Attached to the flare while it climbs: the smoke trail of the cartridge.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UNiagaraSystem> TrailEffect = nullptr;

	// Attached to the flare from ignition until it burns out: glow, sparks and smoke.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UNiagaraSystem> BurnEffect = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<USoundBase> LaunchSound = nullptr;

	// Plays at ignition, attached to the flare.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<USoundBase> IgnitionSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0"))
	float LightIntensity = 80000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0"))
	float LightAttenuationRadius = 3500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor LightColor = FLinearColor(1.f, 0.78f, 0.55f);

	// Scale of the flare and parachute meshes; the real cartridge is far too small to read from the RTS camera.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.01"))
	float MeshScale = 1.f;

	// Distance (before MeshScale) below the hang point where the cartridge burns: the glow, sparks and light sit there.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0"))
	float BurnPointDepth = 30.f;
};

/** @brief Gameplay and looks of one flare, handed to the flare when it is launched. */
USTRUCT()
struct FFlareProjectileSettings
{
	GENERATED_BODY()

	// Height above the ground under the target at which the flare ignites.
	UPROPERTY()
	float ApexHeight = 1200.f;

	// Seconds the flare burns: it gives vision and sinks from its apex to the ground in this time.
	UPROPERTY()
	float HangTimeSeconds = 20.f;

	UPROPERTY()
	float VisionRadius = 3000.f;

	// Average speed of the climb, cm/s.
	UPROPERTY()
	float AscentSpeed = 3000.f;

	UPROPERTY()
	FFlareVisualSettings Visuals;
};

/**
 * @brief The path of a flare: a curve from the launch tube to the apex above the target, then a straight, steady
 * fall to the ground under the parachute. Pure math so it can be checked without a world.
 */
struct RTS_SURVIVAL_API FFlareFlightPath
{
	FVector Start = FVector::ZeroVector;
	// Bezier control point; pulls the first part of the climb along the launch tube.
	FVector Control = FVector::ZeroVector;
	FVector Apex = FVector::ZeroVector;
	FVector Landing = FVector::ZeroVector;
	float AscentSeconds = 0.f;
	float HangSeconds = 0.f;

	/**
	 * @param LaunchLocation World location of the launch socket.
	 * @param LaunchDirection Direction of the launch tube; tilted upwards when it points (nearly) sideways or down.
	 * @param GroundTarget Ground location under the aimed spot.
	 * @param ApexHeight Height above GroundTarget at which the flare ignites.
	 * @param AscentSpeed Average climb speed in cm/s.
	 * @param HangSeconds Seconds from ignition until the flare reaches the ground.
	 */
	static FFlareFlightPath Make(const FVector& LaunchLocation, const FVector& LaunchDirection,
	                             const FVector& GroundTarget, const float ApexHeight, const float AscentSpeed,
	                             const float HangSeconds);

	/** @return Location at this time since launch; the climb slows down towards the apex. */
	FVector GetLocationAtTime(const float SecondsSinceLaunch) const;

	/** @return Direction of travel at this time since launch (straight down while burning). */
	FVector GetDirectionAtTime(const float SecondsSinceLaunch) const;

	float GetTotalSeconds() const { return AscentSeconds + HangSeconds; }
};

/**
 * @brief A parachute flare launched by UFlareAimAbilityComponent. It climbs to its apex above the target, ignites,
 * opens its parachute and gives a fog of war vision bubble while it slowly sinks to the ground, then burns out and
 * is destroyed. Spawned from code only; its looks come from FFlareVisualSettings.
 */
UCLASS(NotBlueprintable)
class RTS_SURVIVAL_API AFlareProjectile : public AActor
{
	GENERATED_BODY()

public:
	AFlareProjectile();

	/**
	 * @brief Starts the flight; call once right after spawning.
	 * @param LaunchLocation World location of the launch socket.
	 * @param LaunchDirection Direction of the launch tube.
	 * @param GroundTarget Ground location under the aimed spot.
	 * @param Settings Height, hang time, vision and looks.
	 * @param OwningPlayer Player of the launching unit; player 1 gets the vision bubble, the AI gets enemy vision.
	 */
	void LaunchFlare(const FVector& LaunchLocation, const FVector& LaunchDirection, const FVector& GroundTarget,
	                 const FFlareProjectileSettings& Settings, const uint8 OwningPlayer);

	EFlareProjectilePhase GetPhase() const { return M_Phase; }
	const FFlareFlightPath& GetFlightPath() const { return M_FlightPath; }

protected:
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
#if RTS_WITH_MAP_TESTS
	friend struct FFlareAbilityTestAccess;
#endif

	UPROPERTY()
	TObjectPtr<USceneComponent> M_Root;

	// Swings the flare and parachute like a pendulum under the canopy; the actor itself stays on the path.
	UPROPERTY()
	TObjectPtr<USceneComponent> M_SwayPivot;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_FlareMesh;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_ParachuteMesh;

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> M_TrailEffect;

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> M_BurnEffect;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> M_Light;

	UPROPERTY()
	TObjectPtr<UFowComp> M_FowComponent;

	FFlareFlightPath M_FlightPath;

	UPROPERTY()
	FFlareProjectileSettings M_Settings;

	EFlareProjectilePhase M_Phase = EFlareProjectilePhase::Inactive;
	float M_SecondsSinceLaunch = 0.f;
	float M_SecondsSinceBurnOut = 0.f;
	uint8 M_OwningPlayer = 0;

	void SetupMeshes();
	void StartAscent();
	void Ignite();
	void BurnOut();

	void TickAscent();
	void TickBurning(const float SecondsSinceIgnition);
	void TickBurntOut(const float DeltaSeconds);

	void StartFowVision();
	void StopFowVision();
	void SetLightIntensityScale(const float Scale) const;

	bool GetIsValidFowComponent() const;
};
