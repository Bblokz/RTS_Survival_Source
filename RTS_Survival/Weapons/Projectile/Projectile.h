// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraSystem.h"
#include "Components/BoxComponent.h"
#include "Engine/DamageEvents.h"
#include "ProjectileVfxSettings/ProjectileVfxSettings.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalText/Pooling/AnimatedTextWidgetPoolManager/AnimatedTextWidgetPoolManager.h"
#include "RTS_Survival/MasterObjects/ActorObjectsMaster.h"
#include "RTS_Survival/RTSComponents/ArmorComponent/Armor.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundBase.h"

#include "Projectile.generated.h"

enum class EProjectileNiagaraSystem : uint8;

struct FProjectilePoolSettings
{
	TWeakObjectPtr<ASmallArmsProjectileManager> ProjectileManager = nullptr;
	int32 ProjectileIndex = INDEX_NONE;
};

class UArmorCalculation;
class UAudioComponent;
class UProjectileMovementComponent;
class UWeaponStateProjectile;

/**
 * Collision of the projectile works by performing async traces
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API AProjectile : public AActorObjectsMaster
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AProjectile(const FObjectInitializer& ObjectInitializer);

	void OverwriteGravityScale(const float NewGravityScale) const;

	void OnCreatedInPoolSetDormant();

	void InitProjectilePoolSettings(
		TWeakObjectPtr<ASmallArmsProjectileManager> Manager,
		const int32 Index);

	void SetupProjectileForNewLaunch(
		UWeaponStateProjectile* NewProjectileOwner,
		const ERTSDamageType DamageType,
		const float Range,
		const float Damage,
		const float ArmorPen,
		const float ArmorPenMaxRange,
		const uint32 ShrapnelParticles,
		const float ShrapnelRange,
		const float ShrapnelDamage,
		const float ShrapnelArmorPen,
		const int32 OwningPlayer,
		const TMap<ERTSSurfaceType, FRTSSurfaceImpactData>& ImpactSurfaceData,
		UNiagaraSystem* BounceEffect,
		USoundCue* BounceSound,
		const FVector& ImpactScale,
		const FVector& BounceScale,
		const float ProjectileSpeed,
		const FVector& LaunchLocation,
		const FRotator& LaunchRotation, USoundAttenuation* ImpactAttenuation,
		USoundConcurrency* ImpactConcurrency, const FProjectileVfxSettings& ProjectileVfxSettings,
		const EWeaponShellType
		ShellType, const TArray<AActor*>& ActorsToIgnore, const int32 WeaponCalibre);

	/**
	 * @brief Launches the pooled projectile along an arced path using the provided arch settings.
	 * @param LaunchLocation Start of the trajectory.
	 * @param TargetLocation Desired impact location after accuracy deviation.
	 * @param ProjectileSpeed Base projectile speed fallback for straight flight.
	 * @param Range Range used for fallback timing when arc cannot be solved.
	 * @param ArchSettings Designer-facing settings controlling apex and curvature.
	 * @param DescentAttenuation Attenuation to reuse for descent audio.
	 * @param DescentConcurrency Concurrency to reuse for descent audio.
	 */
	void SetupArcedLaunch(const FVector& LaunchLocation,
	                      const FVector& TargetLocation,
	                      const float ProjectileSpeed,
	                      const float Range,
	                      const FArchProjectileSettings& ArchSettings,
	                      USoundAttenuation* DescentAttenuation,
	                      USoundConcurrency* DescentConcurrency);

	/**
	 * @brief Two-phase fallback: ascend to apex, then straighten toward the target.
	 * @param LaunchLocation Starting location for the fallback shot.
	 * @param TargetLocation Target location including accuracy deviation.
	 * @param SafeProjectileSpeed Speed used for both the ascent and straight segment.
	 * @param Range Range used for timing when no arc math is possible.
	 * @param ArchSettings Designer-facing arch settings for apex calculation.
	 */
	void LaunchStraightFallback(const FVector& LaunchLocation,
	                            const FVector& TargetLocation,
	                            const float SafeProjectileSpeed,
	                            const float Range,
	                            const FArchProjectileSettings& ArchSettings);

	// Called by abilities using attach rockets to change the mesh on the rocket VFX.
	void SetupAttachedRocketMesh(UStaticMesh* RocketMesh);

	// can be called AFTER SetupProjectileForNewLaunch to accelerate the projetile.
	void ApplyAccelerationFactor(const float Factor, const bool bUseGravityTArch, const float LowestZValue);
	void SetupTraceChannel(int NewOwningPlayer);
	void HandleTimedExplosion();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;


	// Invalidates the async timer.
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void BeginDestroy() override;

	// Obtains reference to the projectile movement component.
	virtual void PostInitializeComponents() override;

	/**
	 * @brief Uses a lerp with factor of distance travelled / range to find the armor pen at the current distance. 
	 * @return The armor penetration depending on the distance this projectile travelled.
	 */
	float GetArmorPenAtRange();

	/** @param DamageMlt Multiplier for damage mainly used for when projectiles did not fully penetrate; usually set to 1.0f */
	void OnHitActor(
		AActor* HitActor,
		const FVector& HitLocation, const FRotator& HitRotation, const ERTSSurfaceType HitSurface,
		const float DamageMlt);

	virtual void DamageActorWithShrapnel(AActor* HitActor);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "VFX")
	TMap<EProjectileNiagaraSystem, UNiagaraSystem*> ProjectileNiagaraSystems;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "VFX")
	FString NiagaraShellColorName = TEXT("ShellColor");

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "VFX")
	FString NiagaraShellWidthName = TEXT("ShellWidth");

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "VFX")
	TMap<EWeaponShellType, FLinearColor> ShellColorMap;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "VFX")
	FLinearColor AAShellColor = FLinearColor(0.0f, 0.0f, 1.0f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "VFX")
	FString NiagaraRocketMeshName = TEXT("RocketMesh");

private:
	UPROPERTY()
	UWeaponStateProjectile* M_ProjectileOwner;

	UPROPERTY()
	TWeakObjectPtr<UAnimatedTextWidgetPoolManager> M_AnimatedTextWidgetPoolManager;
	bool GetIsValidWidgetPoolManager() const;
	void PostInit_SetupWidgetPoolManager();

	UPROPERTY()
	int32 M_WeaponCalibre = 0;

	// Begin Projectile Pool Settings

	// The manager and index of this projectile in the pool.
	FProjectilePoolSettings M_ProjectilePoolSettings;

	// Called when the projectile has finished its job for the current projectile owner.
	void OnProjectileDormant();

	void OnRestartProjectile(const FVector& NewLocation, const FRotator& LaunchRotation, const float ProjectileSpeed);

	bool GetIsValidProjectileManager() const;

	// End Projectile Pool Settings

	void OnNewDamageType(const ERTSDamageType DamageType);

	UPROPERTY()
	EWeaponShellType M_ShellType = EWeaponShellType::Shell_None;

	UPROPERTY()
	int32 M_OwningPlayer = 0;

	UPROPERTY()
	ERTSDamageType M_RTSDamageType;

	UPROPERTY()
	FVector M_ProjectileSpawn;

	UPROPERTY()
	TArray<AActor*> M_ActorsToIgnore;

	UPROPERTY()
	float M_Range = 0;

	UPROPERTY()
	float M_FullDamage = 0;

	UPROPERTY()
	float M_ArmorPen = 0;

	UPROPERTY()
	float M_ArmorPenAtMaxRange = 0;

	UPROPERTY()
	FPointDamageEvent M_DamageEvent;

	UPROPERTY()
	TSubclassOf<UDamageType> M_UDamageType;

	UPROPERTY()
	uint32 M_ShrapnelParticles = 0;

	UPROPERTY()
	float M_ShrapnelRange = 0;

	UPROPERTY()
	float M_ShrapnelDamage = 0;

	UPROPERTY()
	float M_ShrapnelArmorPen = 0;

	UPROPERTY()
	TMap<ERTSSurfaceType, FRTSSurfaceImpactData> M_ImpactVfx;

	UPROPERTY()
	UNiagaraSystem* M_BounceVfx = nullptr;

	UPROPERTY()
	USoundCue* M_BounceSound = nullptr;

	UPROPERTY()
	FVector M_ImpactScale = FVector(1, 1, 1);

	UPROPERTY()
	FVector M_BounceScale = FVector(1, 1, 1);

	UPROPERTY()
	USoundAttenuation* M_ImpactAttenuation = nullptr;

	UPROPERTY()
	USoundConcurrency* M_ImpactConcurrency = nullptr;

	/**
	 * @brief Performs an async line trace on the channel set accoording to the owning player.
	 * The line trace interval can be found in developer settings.
	 * We make use of the delta time passed (calculated in tick)
	 */
	void PerformAsyncLineTrace();

	void StartFlightTimers(const float ExpectedFlightTime);

	/**
	* @brief The callback function for the async trace.
	* Calculates armor pen values and handles the hit actor.
	*/
	void OnAsyncTraceComplete(const FTraceHandle& TraceHandle, FTraceDatum& TraceDatum);

	void PlayDescentSound(USoundBase* DescentSound,
	                      USoundAttenuation* DescentAttenuation,
	                      USoundConcurrency* DescentConcurrency);

	/**
	 * @brief Stops any active descent audio and cleans up its component.
	 */
	void StopDescentSound();

	/**
	 * @brief Computes the apex height based on designer settings while preserving target accuracy.
	 * @param LaunchLocation Starting location.
	 * @param TargetLocation Adjusted target location.
	 * @param HorizontalDistance XY distance between launch and target.
	 * @param ArchSettings Designer tuning values.
	 * @return Apex height in world units.
	 */
	float CalculateDesiredApexHeight(const FVector& LaunchLocation,
	                                 const FVector& TargetLocation,
	                                 const float HorizontalDistance,
	                                 const FArchProjectileSettings& ArchSettings) const;

	/**
	 * @brief Applies curvature multiplier while clamping to safe values.
	 * @param BaseVerticalVelocity Velocity derived from apex height.
	 * @param CurvatureVerticalVelocityMultiplier Designer curvature control.
	 * @return Adjusted vertical launch velocity.
	 */
	float ApplyCurvatureToVerticalVelocity(const float BaseVerticalVelocity,
	                                       const float CurvatureVerticalVelocityMultiplier) const;

	/**
	 * @brief Starts the timer for the descent audio so it finishes at impact.
	 * @param TimeToTarget Calculated flight time for the arc.
	 * @param ArchSettings Designer settings containing the descent sound.
	 * @param DescentAttenuation Attenuation to reuse for the descent sound.
	 * @param DescentConcurrency Concurrency to reuse for the descent sound.
	 */
	void ScheduleDescentSound(const float TimeToTarget,
	                          const FArchProjectileSettings& ArchSettings,
	                          USoundAttenuation* DescentAttenuation,
	                          USoundConcurrency* DescentConcurrency);

	/**
	 * @brief Solves arc parameters for the primary arced launch.
	 * @param LaunchLocation Starting point of the projectile.
	 * @param TargetLocation Final target after accuracy deviation.
	 * @param ArchSettings Designer settings for apex planning.
	 * @param OutLaunchVelocity Calculated initial velocity.
	 * @param OutApexLocation Apex world location based on the solution.
	 * @param OutTimeToTarget Time to impact using the arc.
	 * @param OutHorizontalDirection Horizontal direction toward the target.
	 * @param OutGravity Magnitude of gravity used in the calculation.
	 * @return True when a valid arc exists, false when fallback is needed.
	 */
	bool CalculateArcedLaunchParameters(const FVector& LaunchLocation,
	                                    const FVector& TargetLocation,
	                                    const FArchProjectileSettings& ArchSettings,
	                                    FVector& OutLaunchVelocity,
	                                    FVector& OutApexLocation,
	                                    float& OutTimeToTarget,
	                                    FVector& OutHorizontalDirection,
	                                    float& OutGravity);

	void TransitionArcFallbackToStraight(const FVector TargetLocation, const float SafeProjectileSpeed);
	/**
	 * @brief Straight-line fallback when no arcing parameters can be computed.
	 * @param LaunchLocation Spawn location for the projectile.
	 * @param LaunchToTarget Vector pointing from launch to target.
	 * @param SafeProjectileSpeed Projectile speed used for timing.
	 * @param Range Range used for the timed explosion.
	 */
	void LaunchStraightFallbackDirect(const FVector& LaunchLocation,
	                                  const FVector& LaunchToTarget,
	                                  const float SafeProjectileSpeed,
	                                  const float Range);
	/**
	 * @brief Executes the two-phase fallback: initial ascent then straight line.
	 * @param LaunchLocation Starting point of the projectile.
	 * @param TargetLocation Final target after accuracy deviation.
	 * @param LaunchToTarget Vector from launch to target for rotations.
	 * @param FlatDelta Horizontal delta between launch and target.
	 * @param SafeProjectileSpeed Speed budget for both phases.
	 * @param Range Weapon range for timers.
	 * @param Gravity Magnitude of world gravity for calculations.
	 * @param ArchSettings Designer settings for apex planning.
	 */
	void LaunchStraightFallbackStartAscent(const FVector& LaunchLocation,
	                                       const FVector& TargetLocation,
	                                       const FVector& LaunchToTarget,
	                                       const FVector& FlatDelta,
	                                       const float SafeProjectileSpeed,
	                                       const float Range,
	                                       const float Gravity,
	                                       const FArchProjectileSettings& ArchSettings);
	/**
	 * @brief Calculates ascent, apex, and straight-line handover for fallback arcs.
	 * @param LaunchLocation Starting point of the projectile.
	 * @param TargetLocation Final target after accuracy deviation.
	 * @param LaunchToTarget Vector from launch to target for rotations.
	 * @param FlatDelta Horizontal delta between launch and target.
	 * @param SafeProjectileSpeed Speed budget for both phases.
	 * @param Range Weapon range for timers.
	 * @param Gravity Magnitude of world gravity for calculations.
	 * @param ArchSettings Designer settings for apex planning.
	 * @param OutLaunchVelocity Calculated initial velocity for the ascent phase.
	 * @param OutApexLocation World location of the apex.
	 * @param OutTimeToApex Time to reach the apex.
	 * @param OutTotalFlightTime Total time until impact using straight descent.
	 * @return True when valid parameters were built; false triggers straight fallback.
	 */
	bool CalculateFallbackArcParameters(const FVector& LaunchLocation,
	                                    const FVector& TargetLocation,
	                                    const FVector& LaunchToTarget,
	                                    const FVector& FlatDelta,
	                                    const float SafeProjectileSpeed,
	                                    const float Range,
	                                    const float Gravity,
	                                    const FArchProjectileSettings& ArchSettings,
	                                    FVector& OutLaunchVelocity,
	                                    FVector& OutApexLocation,
	                                    float& OutTimeToApex,
	                                    float& OutTotalFlightTime);

	/**
	 * @brief Calculates the angle between the projectile's velocity vector and the surface normal at the point of impact.
 *
 * This angle is essential for determining the interaction between the projectile and the impacted surface.
 * It influences factors such as armor penetration effectiveness and whether the projectile should bounce
 * or be absorbed based on the geometry of the collision.
 *
 * @param Velocity The current velocity vector of the projectile, representing its direction and speed.
 * @param ImpactNormal The normal vector of the surface at the collision point, indicating the orientation of the surface.
 * @return The calculated impact angle in degrees, representing the deviation between the projectile's path and the surface.
 */
	float CalculateImpactAngle(const FVector& Velocity, const FVector& ImpactNormal) const;

	/**
	 * @brief Manages the behavior of the projectile upon colliding with a surface, including reflection and state updates.
	 *
	 * When a projectile hits a surface that allows it to bounce, this function calculates the new trajectory by
	 * reflecting the velocity vector based on the impact normal. It also updates the projectile's state, such as
	 * reducing its armor penetration capability and decrementing the remaining number of allowable bounces.
	 * Additionally, it spawns visual and audio effects to indicate the bounce event.
	 *
	 * @param HitResult The result of the collision trace containing details about the impact, including location and normal.
	 * @param HitActor
	 * @param HitRotation
	 * @return Returns true if the projectile successfully bounces and continues its lifecycle; otherwise, returns false.
	 */
	void HandleProjectileBounce(const FHitResult& HitResult, const EArmorPlate PlateHit, AActor* HitActor,
	                            const FRotator& HitRotation);

	/**
	 * @brief Processes the event when the projectile successfully hits an actor and ensures cleanup of associated timers.
	 *
	 * This function is invoked when the projectile impacts an actor that it should affect, such as dealing damage or triggering
	 * specific gameplay mechanics. It handles the necessary interactions with the hit actor and ensures that any ongoing
	 * trace timers are cleared to prevent further unnecessary processing.
	 *
	 * @param HitResult The result of the collision trace containing information about the impacted actor and the point of contact.
	 * @param HitActor
	 * @param HitLocation
	 * @param HitRotation
	 * @param DamageMlt
	 */
	void HandleHitActorAndClearTimer(AActor* HitActor, const FVector& HitLocation, const ERTSSurfaceType HitSurface,
	                                 const FRotator& HitRotation, const float DamageMlt);

	/**
	 * @brief Initiates and displays an explosion effect at the specified location upon projectile impact.
	 *
	 * This function is responsible for spawning visual and auditory explosion effects to provide feedback to the player
	 * and enhance the visual fidelity of the game. The explosion may include particle systems, sound cues, and other
	 * related effects that signify the destructive impact of the projectile.
	 *
	 * @param Location The world-space coordinates where the explosion effect should be spawned, typically the point of impact.
	 * @param HitRotation
	 * @param HitSurface
	 * @param HitActor
	 */
	void SpawnExplosionHandleAOE(const FVector& Location, const FRotator& HitRotation, const ERTSSurfaceType HitSurface, AActor* HitActor);

	/**
	 * @brief Creates and displays a bounce effect at the given location with the specified directional orientation.
	 *
	 * When a projectile bounces off a surface, this function generates the appropriate visual and audio effects to represent
	 * the bounce event. It ensures that the bounce direction aligns with the reflected trajectory, enhancing the realism
	 * and responsiveness of the projectile's behavior.
	 *
	 * @param Location The world-space coordinates where the bounce effect should be spawned, typically the point of impact.
	 * @param BounceDirection The rotational orientation that dictates the direction and alignment of the bounce effect, based on the reflection.
	 */
	void SpawnBounce(const FVector& Location, const FRotator& BounceDirection) const;

	void OnHitArmorCalcComponent(UArmorCalculation* ArmorCalculation, const FHitResult& HitResult, AActor* HitActor);
	FORCEINLINE void ArmorCalc_KineticProjectile(UArmorCalculation* ArmorCalculation, const FHitResult& HitResult,
	                                             AActor* HitActor);
	FORCEINLINE void ArmorCalc_FireProjectile(UArmorCalculation* ArmorCalculation, const FHitResult& HitResult,
	                                          AActor* HitActor);

	/** @brief Only notifies if the owning player is 1. */
	void ProjectileHitPropagateNotification(const bool bBounced);


	UPROPERTY()
	TObjectPtr<UProjectileMovementComponent> M_ProjectileMovement;

	// Cached gravity scale so pooled projectiles can reset between different weapon types.
	UPROPERTY()
	float M_DefaultGravityScale = 0.0f;

	UPROPERTY()
	UNiagaraComponent* M_NiagaraComponent;

	FProjectileVfxSettings M_ProjectileVfxSettings;

	void SetupNiagaraWithPrjVfxSettings(const FProjectileVfxSettings& NewSettings);
	UNiagaraSystem* GetSystemFromType(const EProjectileNiagaraSystem Type);
	bool GetIsValidProjectileMovement();
	bool GetIsValidNiagara();
	bool GetCanSetParametersOnSystem(const EProjectileNiagaraSystem Type) const;
	FLinearColor GetColorOfShell(const EWeaponShellType ShellType, const EProjectileNiagaraSystem SystemType) const;
	float GetWidthOfShell(const float WeaponCalibre, const EWeaponShellType ShellType) const;

	int32 M_MaxBounces = DeveloperSettings::GameBalance::Weapons::Projectiles::MaxBouncesPerProjectile;

	UPROPERTY()
	FTimerHandle M_ExplosionTimerHandle;

	UPROPERTY()
	FTimerHandle M_LineTraceTimerHandle;

	UPROPERTY()
	FTimerHandle M_DescentSoundTimerHandle;

	UPROPERTY()
	FTimerHandle M_ArcFallbackTimerHandle;

	UPROPERTY()
	TWeakObjectPtr<UAudioComponent> M_DescentAudioComponent;

	ECollisionChannel M_TraceChannel;

	// time (in seconds) at which we last did a trace
	double M_LastTraceTime;

	// To ensure that a projectile cannot trigger a voice line after a bounce.
	bool bM_AlreadyNotified = false;

	void DebugBounce(const FVector& Location) const;
	void DebugProjectile(const FString& Message) const;

	// Set to the value set in the derived blueprint.
	UPROPERTY()
	FVector M_BasicScale = FVector(1.0f, 1.0f, 1.0f);

	void ScaleNiagaraSystemDependingOnType(const EProjectileNiagaraSystem Type) const;

	/**
	 * @brief Checks the armor and rolls dice on whether a He or Heat projectile should explode on bounce.
	 * Will display text to inform the player on what happened.
	 * @param Location Hit location.
	 * @param ArmorPlateHit The armor plate hit used to calculate the armor damage type.
	 * @param HitActor The actor hit.
	 * @param HitRotator The impact rotation for the he heat bounce or damage explosion.
	 * @return True if the explosion and dormancy are handled by this function, false otherwise.
	 */
	bool OnBounce_HandleHeHeatModuleDamage(const FVector& Location, EArmorPlate ArmorPlateHit, AActor* HitActor,
	                                       const FRotator& HitRotator);
	bool CanHeHeatDamageOnBounce(EArmorPlate PlateHit, EArmorPlateDamageType& OutArmorPlateDamageType) const;
	void CreateHeHeatBounceDamageText(const FVector& Location, const EArmorPlateDamageType DamageType) const;
	void OnArmorPen_DisplayText(const FVector& Location, const EArmorPlate PlatePenetrated);
	void OnArmorPen_HeDisplayText(const FVector& Location);

	void HandleAoe(const FVector& HitLocation,AActor* HitActor);
};
