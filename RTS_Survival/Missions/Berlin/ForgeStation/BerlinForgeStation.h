// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "BerlinForgeStation.generated.h"

class ABerlinForgeStation;
class AEnemyStrongpoint;
class UAudioComponent;
class ULocalLightComponent;
class UMaterialParameterCollection;
class UNiagaraComponent;
class UNiagaraSystem;
class USoundAttenuation;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;

/** The three counted targets of the station; dressing linked to one stops when that target dies. */
UENUM(BlueprintType)
enum class EForgeStationPartRole : uint8
{
	None,
	Rig,
	CryoBlock,
	CommandBunker
};

UENUM(BlueprintType)
enum class EForgeStationDressingUse : uint8
{
	// Runs from BeginPlay until the station stops or its linked target dies.
	Ambient,
	// Runs only during the vent step of the sedation cycle.
	SedationVent
};

/** A counted target placed in the level (an enemy building) and what is left of it when it dies. */
USTRUCT(BlueprintType)
struct FForgeStationTarget
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target")
	EForgeStationPartRole Role = EForgeStationPartRole::None;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Target")
	TObjectPtr<AActor> Target = nullptr;

	// Left standing where the target stood.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target")
	TSoftObjectPtr<UStaticMesh> DestroyedMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target")
	TSoftObjectPtr<UNiagaraSystem> DeathEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Target")
	float DeathEffectScale = 1.f;
};

/** A mesh that turns about its own Z (the dish, the drill string). */
USTRUCT(BlueprintType)
struct FForgeStationRotatingPart
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Part")
	TSoftObjectPtr<UStaticMesh> Mesh;

	// Relative to the station.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Part", meta = (MakeEditWidget = "true"))
	FTransform RelativeTransform;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Part")
	float DegreesPerSecond = 36.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Part")
	EForgeStationPartRole LinkedRole = EForgeStationPartRole::None;
};

USTRUCT(BlueprintType)
struct FForgeStationLight
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light", meta = (MakeEditWidget = "true"))
	FVector RelativeLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light")
	FRotator RelativeRotation = FRotator(-60.f, 0.f, 0.f);

	// Off: a point light.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light")
	bool bSpotLight = true;

	// Candela.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light")
	float Intensity = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light")
	FLinearColor Color = FLinearColor(1.f, 0.86f, 0.62f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light")
	float AttenuationRadius = 2200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light", meta = (EditCondition = "bSpotLight"))
	float OuterConeAngle = 38.f;

	// On: the intensity breathes (the beacon on the shard mast).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light")
	bool bPulse = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light", meta = (EditCondition = "bPulse"))
	float PulseSeconds = 2.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Light")
	EForgeStationPartRole LinkedRole = EForgeStationPartRole::None;
};

USTRUCT(BlueprintType)
struct FForgeStationEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	TSoftObjectPtr<UNiagaraSystem> System;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect", meta = (MakeEditWidget = "true"))
	FTransform RelativeTransform;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	EForgeStationDressingUse Use = EForgeStationDressingUse::Ambient;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	EForgeStationPartRole LinkedRole = EForgeStationPartRole::None;
};

USTRUCT(BlueprintType)
struct FForgeStationSound
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TSoftObjectPtr<USoundBase> Sound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (MakeEditWidget = "true"))
	FVector RelativeLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	EForgeStationDressingUse Use = EForgeStationDressingUse::Ambient;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	EForgeStationPartRole LinkedRole = EForgeStationPartRole::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	float VolumeMultiplier = 1.f;

	// Ambient sounds that are not loops are replayed after this many seconds (0 = play once).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	float RepeatSeconds = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TSoftObjectPtr<USoundAttenuation> Attenuation;
};

/**
 * The cryo-sedation cycle at the rig: the samples pulse brighter, the lances vent frost, the crystals go dull and
 * frosted and slowly thaw. Crystal materials read the two scalars from the parameter collection.
 */
USTRUCT(BlueprintType)
struct FForgeSedationCycle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sedation")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sedation", meta = (ClampMin = "0.0"))
	float FirstCycleDelay = 20.f;

	// Seconds from one cycle start to the next.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sedation", meta = (ClampMin = "10.0"))
	float IntervalSeconds = 75.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sedation", meta = (ClampMin = "0.1"))
	float PulseSeconds = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sedation", meta = (ClampMin = "0.1"))
	float VentSeconds = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sedation", meta = (ClampMin = "0.1"))
	float ThawSeconds = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sedation")
	TSoftObjectPtr<UMaterialParameterCollection> ParameterCollection;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sedation")
	FName PulseParameter = TEXT("SedationPulse");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sedation")
	FName FrostParameter = TEXT("SedationFrost");

	// The cycle stops for good when this target dies.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sedation")
	EForgeStationPartRole LinkedRole = EForgeStationPartRole::Rig;
};

UENUM(BlueprintType)
enum class EForgeSedationPhase : uint8
{
	Idle,
	Pulse,
	Vent,
	Thaw
};

/** Runtime handle of one built piece of dressing and the target it belongs to. */
USTRUCT()
struct FForgeStationRuntimeDressing
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<USceneComponent> Component = nullptr;

	EForgeStationPartRole LinkedRole = EForgeStationPartRole::None;
	EForgeStationDressingUse Use = EForgeStationDressingUse::Ambient;
	// Rotating parts: degrees per second about Z.
	float DegreesPerSecond = 0.f;
	// Lights: the intensity to pulse around; 0 = no pulse.
	float BaseIntensity = 0.f;
	float PulseSeconds = 0.f;
	// Sounds: replay interval for one-shot sounds.
	float RepeatSeconds = 0.f;
	float SecondsToReplay = 0.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnForgeStationTargetDestroyed, ABerlinForgeStation*, Station,
                                             EForgeStationPartRole, Role);

/**
 * @brief Placed once on Ger_Berlin over the station shell: makes the Forge survey station look alive (lights, steam,
 * sound, turning dish and drill string, the cryo-sedation cycle), swaps each counted target for its wreck when it
 * dies and calms down when the garrison is engaged. UMission_BerlinForgeStation reads the targets from it.
 * @note Set Targets[].Target to the three placed target buildings in the level.
 */
UCLASS()
class RTS_SURVIVAL_API ABerlinForgeStation : public AActor
{
	GENERATED_BODY()

public:
	ABerlinForgeStation();

	virtual void Tick(float DeltaSeconds) override;

	/** @brief Switches all dressing off (the mission is over); wrecks stay. */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "Forge Station")
	void StopStation();

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Forge Station")
	TArray<AActor*> GetTargetActors() const;

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Forge Station")
	int32 GetAliveTargetCount() const;

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Forge Station")
	int32 GetTargetCount() const { return Targets.Num(); }

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Forge Station")
	EForgeSedationPhase GetSedationPhase() const { return M_SedationPhase; }

	UFUNCTION(BlueprintPure, NotBlueprintable, Category = "Forge Station")
	bool GetIsAlerted() const { return bM_IsAlerted; }

	FName GetStrongpointId() const { return StrongpointId; }

	/** @return The first station in the world, or null. */
	static ABerlinForgeStation* FindForgeStation(const UObject* WorldContextObject);

	UPROPERTY(BlueprintAssignable, Category = "Forge Station")
	FOnForgeStationTargetDestroyed OnTargetDestroyed;

	// Counts for tests: built components per kind.
	int32 GetRuntimeDressingCount() const { return M_RuntimeDressing.Num(); }
	int32 GetWreckCount() const { return M_Wrecks.Num(); }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// The indestructible shell (gate wall, hut, receivers, sample bunker, console).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Forge Station")
	TObjectPtr<UStaticMeshComponent> ShellMesh;

	// The station's garrison; its first fight calms the station (no new sedation cycles).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forge Station")
	FName StrongpointId = TEXT("ForgeStation");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forge Station")
	TArray<FForgeStationTarget> Targets;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forge Station|Dressing")
	TArray<FForgeStationRotatingPart> RotatingParts;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forge Station|Dressing")
	TArray<FForgeStationLight> Lights;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forge Station|Dressing")
	TArray<FForgeStationEffect> Effects;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forge Station|Dressing")
	TArray<FForgeStationSound> Sounds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Forge Station|Sedation")
	FForgeSedationCycle Sedation;

private:
	UPROPERTY()
	TArray<FForgeStationRuntimeDressing> M_RuntimeDressing;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> M_Wrecks;

	UPROPERTY()
	TWeakObjectPtr<AEnemyStrongpoint> M_Strongpoint = nullptr;

	UPROPERTY()
	TObjectPtr<UMaterialParameterCollection> M_SedationCollection = nullptr;

	// Where each target stood at BeginPlay, by index into Targets; the wreck goes there.
	TArray<FTransform> M_TargetTransforms;
	TArray<bool> M_IsTargetDestroyed;

	FTimerHandle M_WatchTargetsTimerHandle;
	FTimerHandle M_FinishTargetsTimerHandle;
	FTimerHandle M_FindStrongpointTimerHandle;
	EForgeSedationPhase M_SedationPhase = EForgeSedationPhase::Idle;
	float M_SedationPhaseSeconds = 0.f;
	float M_SecondsToNextSedation = 0.f;
	float M_RunningSeconds = 0.f;
	bool bM_IsStopped = false;
	bool bM_IsAlerted = false;
	bool bM_IsSedationStopped = false;

	// ----- BeginPlay -----
	void BeginPlay_CacheTargets();
	static void SilenceTargetEffects(const AActor& Target);
	// Placed buildings start hidden and unbuilt; finish them the way standalone field defenses are finished.
	void FinishTargetConstruction();
	void BeginPlay_BuildRotatingParts();
	void BeginPlay_BuildLights();
	void BeginPlay_BuildEffects();
	void BeginPlay_BuildSounds();
	void BeginPlay_StartSedation();
	void BeginPlay_StartTimers();

	void AddRuntimeDressing(USceneComponent* Component, const EForgeStationPartRole LinkedRole,
	                        const EForgeStationDressingUse Use);
	USceneComponent* AttachNewComponent(USceneComponent* Component, const FTransform& RelativeTransform);

	// ----- Targets -----
	void WatchTargets();
	void HandleTargetDestroyed(const int32 TargetIndex);
	void SpawnWreck(const int32 TargetIndex);
	void StopDressingLinkedTo(const EForgeStationPartRole PartRole);
	bool GetIsTargetRoleDestroyed(const EForgeStationPartRole PartRole) const;

	// ----- Strongpoint -----
	void TryBindStrongpoint();
	UFUNCTION()
	void HandleGarrisonEngaged(AEnemyStrongpoint* Strongpoint);

	// ----- Tick -----
	void TickRotatingParts(const float DeltaSeconds);
	void TickPulsingLights() const;
	void TickRepeatingSounds(const float DeltaSeconds);
	void TickSedation(const float DeltaSeconds);
	void EnterSedationPhase(const EForgeSedationPhase NewPhase);
	void SetSedationVentActive(const bool bActive);
	void StopSedation();
	void WriteSedationParameters(const float Pulse, const float Frost);
	float GetSedationPulseValue() const;
	float GetSedationFrostValue() const;

	static void DeactivateDressing(FForgeStationRuntimeDressing& Dressing);
};
