// Copyright (C) 2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/HealthInterface/HealthBarOwner.h"
#include "RTS_Survival/UnitData/ArmorAndResistanceData.h"
#include "RTS_Survival/Units/RTSDeathType/RTSDeathType.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalText/RTSVerticalAnimatedText/RTSVerticalAnimatedText.h"
#include "RTS_Survival/Weapons/AimOffsetProvider/AimOffsetProvider.h"
#include "RTS_Survival/Weapons/Turret/Embedded/EmbeddedTurretsMaster.h"
#include "RTS_Survival/Navigation/RTSNavAI/IRTSNavAI.h"
#include "RTS_Survival/GameUI/Healthbar/HealthBarSettings/HealthBarVisibilitySettings.h"
#include "RTS_Survival/Weapons/Turret/Embedded/EmbededTurretInterface.h"
#include "TeamWeapon.generated.h"

class UTeamWeaponAnimationInstance;
enum class ETeamWeaponMontage : uint8;
class UHealthComponent;
class URTSComponent;
class UTeamWeaponMover;
class ATeamWeaponController;
class UDecalComponent;
class UMaterialInterface;
class UCrewPosition;
struct FDestroySpawnActorsParameters;
enum class ESquadSubtype : uint8;

DECLARE_MULTICAST_DELEGATE(FOnTeamWeaponUnitDies);

UENUM(BlueprintType)
enum class ETeamWeaponMovementType : uint8
{
	CrewFollowsWeapon,
	PushedWeaponLeads
};

USTRUCT(BlueprintType)
struct FTeamWeaponYawArcSettings
{
	GENERATED_BODY()

	// Minimum yaw (degrees) relative to the weapon's forward direction.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon")
	float M_MinYaw = -20.f;

	// Maximum yaw (degrees) relative to the weapon's forward direction. If 0, the weapon can rotate full 360.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon")
	float M_MaxYaw = 20.f;
};


USTRUCT(BlueprintType)
struct FTeamWeaponPitchSettings
{
	GENERATED_BODY()

	// Minimum yaw (degrees) relative to the weapon's forward direction.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon")
	float M_MinPitch = -10.f;

	// Maximum yaw (degrees) relative to the weapon's forward direction. If 0, the weapon can rotate full 360.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon")
	float M_MaxPitch = 15.f;
};

USTRUCT(BlueprintType)
struct FTeamWeaponEmbeddedSettings
{
	GENERATED_BODY()

	// how high the weapon of the turret is from the ground.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon|EmbeddedTurret")
	float TurretHeight = 50.f;

	// used for pitch calculations.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon|EmbeddedTurret")
	bool bIsArtillery = false;
	// used for pitch calculations.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon|EmbeddedTurret")
	float ArtilleryDistanceUseMaxPitch = 500;
};


USTRUCT(BlueprintType)
struct FTeamWeaponConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "TeamWeapon")
	FTeamWeaponYawArcSettings M_YawArc;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "TeamWeapon")
	FTeamWeaponPitchSettings M_PitchSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon|EmbeddedTurret")
	FTeamWeaponEmbeddedSettings M_EmbeddedSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "TeamWeapon")
	FResistanceAndDamageReductionData M_ResistanceData;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "TeamWeapon")
	float TeamWeaponHp = 200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "TeamWeapon")
	float M_DeploymentTime = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "TeamWeapon")
	int32 M_OperatorCount = 0;

	// Rotation speed used for when packed and rotating towards a certain position.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "TeamWeapon")
	float M_TurnSpeedYaw = 20.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "TeamWeapon|Movement")
	ETeamWeaponMovementType M_MovementType = ETeamWeaponMovementType::CrewFollowsWeapon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "TeamWeapon|Movement", meta = (ClampMin = "0.0"))
	float M_PushedMoveSpeedCmPerSec = 250.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "TeamWeapon|Movement", meta = (ClampMin = "0.0"))
	float M_PathAcceptanceRadiusCm = 60.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "TeamWeapon|Movement", meta = (ClampMin = "0.0"))
	float M_GroundTraceStartHeightCm = 120.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "TeamWeapon|Movement", meta = (ClampMin = "0.0"))
	float M_GroundTraceLengthCm = 400.f;

	// Extra forward distance guards may use while the team weapon repositions to engage a target.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "TeamWeapon|Movement", meta = (ClampMin = "0.0"))
	float M_GuardEngageFlowDistanceCm = 150.f;

	// Used for rotating the weapon within the limits of the turret yaw settings as implemented in the embedded turret base.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "TeamWeapon")
	float WeaponYawRotationSpeed = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "TeamWeapon|Text")
	FString M_DeployingAnimatedText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "TeamWeapon|Text")
	FString M_PackingAnimatedText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "TeamWeapon|Text")
	FRTSVerticalAnimTextSettings M_AnimatedTextSettings;
};

USTRUCT(BlueprintType)
struct FTeamWeaponAbandonedHealthBarState
{
	GENERATED_BODY()

	FTeamWeaponAbandonedHealthBarState()
	{
		M_AbandonedHealthBarCustomization.bUseGreenRedGradient = false;
		M_AbandonedHealthBarCustomization.bUseEnemyColorIfEnemy = false;
		M_AbandonedHealthBarCustomization.OverWriteGradientColor = FLinearColor::White;
	}

	// Applied while the team weapon is abandoned so the state is visually distinct from crewed weapons.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon|HealthBar")
	FHealthBarCustomization M_AbandonedHealthBarCustomization;

	UPROPERTY(Transient)
	FHealthBarCustomization M_CachedCrewedHealthBarCustomization;

	UPROPERTY(Transient)
	bool bM_HasCachedCrewedHealthBarCustomization = false;
};

USTRUCT(BlueprintType)
struct FTeamWeaponDecalManager
{
	GENERATED_BODY()

public:
	void CreateDecal(ATeamWeapon* TeamWeapon);
	void DestroyDecal(const ATeamWeapon* TeamWeapon);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon|Decal")
	FVector M_DecalSize = FVector(32.f, 250.f, 250.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon|Decal")
	FVector M_DecalOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon|Decal")
	TObjectPtr<UMaterialInterface> M_DecalMaterial = nullptr;

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<UDecalComponent> M_TeamWeaponDecalComponent;
};

/**
 * @brief Team weapon actor used by team-weapon squads to manage turret weapons, health, and crew sockets.
 * @note is derived from embeddedturret for which InitEmbeddedTurret DO NOT CALL IN BP this is done in cpp internally (using settings struct).
 * @note InitChildTurret: call in blueprint to set rotation/pitch limits and idle rotation style.
 * @note ApplyTeamWeaponConfig: call in blueprint to push config data to components (health/resistance/deploy time).
 */
UCLASS()
class RTS_SURVIVAL_API ATeamWeapon : public AEmbeddedTurretsMaster, public IRTSNavAIInterface,
                                     public IAimOffsetProvider, public IHealthBarOwner, public IEmbeddedTurretInterface
{
	GENERATED_BODY()

public:
	ATeamWeapon();
	// To be called at beginplay; to init the various settings.
	void BeginPlay_ApplyTeamWeaponConfig(const FTeamWeaponConfig& NewConfig);
	void PlayFireAnim() const;
	const FTeamWeaponConfig& GetTeamWeaponConfig() const { return M_TeamWeaponConfig; }
	int32 GetRequiredOperators() const { return M_TeamWeaponConfig.M_OperatorCount; }
	float GetDeploymentTime() const { return M_TeamWeaponConfig.M_DeploymentTime; }
	float GetTurnSpeedYaw() const { return M_TeamWeaponConfig.M_TurnSpeedYaw; }
	ETeamWeaponMovementType GetMovementType() const { return M_TeamWeaponConfig.M_MovementType; }
	float GetPushedMoveSpeedCmPerSec() const { return M_TeamWeaponConfig.M_PushedMoveSpeedCmPerSec; }
	float GetPathAcceptanceRadiusCm() const { return M_TeamWeaponConfig.M_PathAcceptanceRadiusCm; }
	float GetGroundTraceStartHeightCm() const { return M_TeamWeaponConfig.M_GroundTraceStartHeightCm; }
	float GetGroundTraceLengthCm() const { return M_TeamWeaponConfig.M_GroundTraceLengthCm; }
	float GetGuardEngageFlowDistanceCm() const { return M_TeamWeaponConfig.M_GuardEngageFlowDistanceCm; }
	void OnTeamWeaponAbandoned();
	void OnTeamWeaponRemanned();
	UFUNCTION(BlueprintCallable, Category = "TeamWeapon|Abandoned")
	bool ForceSetAbandonedStateForCapture(
		TSubclassOf<ATeamWeaponController> DefaultLastTeamWeaponControllerClass = nullptr);
	bool GetIsAbandoned() const { return bM_IsAbandoned; }
	TSubclassOf<ATeamWeaponController> GetLastTeamWeaponControllerClass() const { return M_LastTeamWeaponControllerClass; }
	ESquadSubtype GetSquadSubtypeFromRTSComponent() const;
	bool SetOwningPlayerRuntime(const uint8 NewOwningPlayer);

	void SetTeamWeaponController(ATeamWeaponController* NewController);
	ATeamWeaponController* GetTeamWeaponController() const { return M_TeamWeaponController.Get(); }
	UTeamWeaponMover* GetTeamWeaponMover() const { return M_TeamWeaponMover; }

	void SetTurretOwnerActor(AActor* NewOwner);
	void GetCrewPositions(TArray<UCrewPosition*>& OutCrewPositions) const;
	void PlayPackingMontage(const bool bWaitForMontage) const;
	void PlayDeployingMontage(const bool bWaitForMontage) const;
	void NotifyMoverMovementState(const bool bIsMoving, const FVector& WorldVelocity);
	virtual bool GetUsesWheelMovementMontage() const;
	void SetWeaponsEnabledForTeamWeaponState(const bool bEnableWeapons);
	void SetSpecificEngageTarget(AActor* TargetActor);
	AActor* GetSpecificEngageTarget() const { return M_SpecificEngageTarget.Get(); }
	void SetDigInHullRotationLocked(const bool bLocked);
	virtual float TakeDamage(
		float DamageAmount,
		FDamageEvent const& DamageEvent,
		AController* EventInstigator,
		AActor* DamageCauser) override;

	// Delegate called when the team weapon dies.
	FOnTeamWeaponUnitDies OnUnitDies;

	// Embedded turret interface ---------------------------
	virtual float GetCurrentTurretAngle_Implementation() const override;
    virtual void SetTurretAngle_Implementation(float NewAngle) override;
    virtual void UpdateTargetPitch_Implementation(float NewPitch) override;
    virtual bool TurnBase_Implementation(float Degrees) override;
    virtual void PlaySingleFireAnimation_Implementation(int32 WeaponIndex) override;
    virtual void PlayBurstFireAnimation_Implementation(int32 WeaponIndex) override;


	/**
	 * @brief Guards fire logic by enforcing the configured yaw arc for this emplacement.
	 * Prevents firing when the target would require exceeding the intended mechanical limits.
	 *
	 * @details Converts the target vector into local space and compares the yaw to the min/max
	 *          arc values so the gunner logic can keep predictable constraints.
	 *
	 * @param TargetLocation World location to evaluate against the yaw arc.
	 * @return True when the target location is inside the permitted yaw arc.
	 */
	bool GetIsTargetWithinYawArc(const FVector& TargetLocation) const;

	virtual void SetDefaultQueryFilter(const TSubclassOf<UNavigationQueryFilter> NewDefaultFilter) override;
	virtual void GetAimOffsetPoints(TArray<FVector>& OutLocalOffsets) const override;
	virtual void OnHealthChanged(const EHealthLevel PercentageLeft, const bool bIsHealing) override;

protected:
	virtual void BeginPlay() override;
	virtual void PostInitializeComponents() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon|Decal")
	FTeamWeaponDecalManager M_TeamWeaponDecalManager;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon|HealthBar")
	FTeamWeaponAbandonedHealthBarState M_AbandonedHealthBarState;

	UFUNCTION(BlueprintImplementableEvent, Category = "TeamWeapon")
	void BP_OnHealthChanged(const EHealthLevel PercentageLeft, const bool bIsHealing);

	UFUNCTION(BlueprintImplementableEvent, Category = "TeamWeapon")
	void BP_OnUnitDies();

	/** @brief Used to call death for this unit, override in childs*/
	virtual void UnitDies(const ERTSDeathType DeathType);

	
	/**
	 * @brief Destroy the bxp and spawn actors around the location.
	 * @param SpawnParams Defines what actors to spawn and where.
	 * @param CollapseFX Optional FX to play.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	virtual void DestroyAndSpawnActors(
		const FDestroySpawnActorsParameters& SpawnParams,
		FCollapseFX CollapseFX);

private:
	void ApplyAbandonedHealthBarCustomization();
	void RestoreCrewedHealthBarCustomization();
	[[nodiscard]] bool GetIsValidHealthComponent() const;
	[[nodiscard]] bool GetIsValidRTSComponent() const;
	[[nodiscard]] bool GetIsValidTeamWeaponMover() const;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Reference", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHealthComponent> M_HealthComponent;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Reference", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URTSComponent> M_RTSComponent;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Reference", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTeamWeaponMover> M_TeamWeaponMover;

	UPROPERTY()
	TWeakObjectPtr<ATeamWeaponController> M_TeamWeaponController;

	UPROPERTY()
	TSubclassOf<ATeamWeaponController> M_LastTeamWeaponControllerClass = nullptr;

	UPROPERTY()
	bool bM_IsAbandoned = false;

	[[nodiscard]] bool GetIsValidTeamWeaponController() const;
	[[nodiscard]] bool GetIsControllerReadyDeployed() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon", meta = (AllowPrivateAccess = "true"))
	FTeamWeaponConfig M_TeamWeaponConfig;

	UPROPERTY()
	TSubclassOf<UNavigationQueryFilter> M_DefaultQueryFilter = nullptr;

	UPROPERTY()
	TWeakObjectPtr<USkeletalMeshComponent> M_TeamWeaponMesh;
	[[nodiscard]] bool GetIsValidTeamWeaponMesh() const;

	void BeginPlay_InitAnimInstance();
	void BeginPlay_InitTeamWeaponCollision();
	void BeginPlay_ForceStartPackedState();

	UPROPERTY()
	TWeakObjectPtr<UTeamWeaponAnimationInstance> M_AnimInstance = nullptr;
	[[nodiscard]] bool GetIsValidAnimInstance() const;

	UPROPERTY()
	bool bM_IsMoverMoving = false;

	UPROPERTY()
	bool bM_LastForwardMovement = true;

	// Cached target used to re-enter specific engage after packing/moving temporarily disables turret logic.
	UPROPERTY()
	TWeakObjectPtr<AActor> M_SpecificEngageTarget;
	// Mirrors assault tank behavior: while dug in the embedded turret cannot rotate the weapon base.
	UPROPERTY()
	bool bM_IsHullRotationLocked = false;
};
