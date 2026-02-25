// Copyright (C) 2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/HealthInterface/HealthBarOwner.h"
#include "RTS_Survival/UnitData/ArmorAndResistanceData.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalText/RTSVerticalAnimatedText/RTSVerticalAnimatedText.h"
#include "RTS_Survival/Weapons/AimOffsetProvider/AimOffsetProvider.h"
#include "RTS_Survival/Weapons/Turret/Embedded/EmbeddedTurretsMaster.h"
#include "RTS_Survival/Navigation/RTSNavAI/IRTSNavAI.h"
#include "RTS_Survival/Weapons/Turret/Embedded/EmbededTurretInterface.h"
#include "TeamWeapon.generated.h"

class UHealthComponent;
class URTSComponent;
class UTeamWeaponMover;
class ATeamWeaponController;

USTRUCT(BlueprintType)
struct FTeamWeaponYawArcSettings
{
	GENERATED_BODY()

	// Minimum yaw (degrees) relative to the weapon's forward direction.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon")
	float M_MinYaw = 0.0f;

	// Maximum yaw (degrees) relative to the weapon's forward direction. If 0, the weapon can rotate full 360.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon")
	float M_MaxYaw = 0.0f;
};

USTRUCT(BlueprintType)
struct FTeamWeaponConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon")
	FTeamWeaponYawArcSettings M_YawArc;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon")
	FResistanceAndDamageReductionData M_ResistanceData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon")
	float M_DeploymentTime = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon")
	int32 M_OperatorCount = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon")
	float M_TurnSpeedYaw = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon|Text")
	FString M_DeployingAnimatedText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon|Text")
	FString M_PackingAnimatedText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon|Text")
	FRTSVerticalAnimTextSettings M_AnimatedTextSettings;
};

/**
 * @brief Team weapon actor used by team-weapon squads to manage turret weapons, health, and crew sockets.
 * @note InitEmbeddedTurret: call in blueprint to bind the turret mesh and embedded owner for yaw/pitch updates.
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

	/**
	 * @brief Applies balance data so designers can tune the weapon without touching code.
	 * Keeps the runtime state aligned with the latest blueprint configuration.
	 *
	 * @details Copies the config into the actor and propagates the values to dependent components
	 *          (health, resistance, deploy time) so the runtime stays deterministic.
	 *
	 * @param NewConfig Configuration payload applied to this instance.
	 */
	UFUNCTION(BlueprintCallable, Category = "TeamWeapon")
	void ApplyTeamWeaponConfig(const FTeamWeaponConfig& NewConfig);

	const FTeamWeaponConfig& GetTeamWeaponConfig() const { return M_TeamWeaponConfig; }
	int32 GetRequiredOperators() const { return M_TeamWeaponConfig.M_OperatorCount; }
	float GetDeploymentTime() const { return M_TeamWeaponConfig.M_DeploymentTime; }
	float GetTurnSpeedYaw() const { return M_TeamWeaponConfig.M_TurnSpeedYaw; }

	void SetTeamWeaponController(ATeamWeaponController* NewController);
	UTeamWeaponMover* GetTeamWeaponMover() const { return M_TeamWeaponMover; }

	void SetTurretOwnerActor(AActor* NewOwner);

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

	UFUNCTION(BlueprintImplementableEvent, Category = "TeamWeapon")
	void BP_OnHealthChanged(const EHealthLevel PercentageLeft, const bool bIsHealing);

private:
	bool GetIsValidHealthComponent() const;
	bool GetIsValidRTSComponent() const;
	bool GetIsValidTeamWeaponMover() const;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Reference", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHealthComponent> M_HealthComponent;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Reference", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<URTSComponent> M_RTSComponent;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Reference", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTeamWeaponMover> M_TeamWeaponMover;

	UPROPERTY()
	TWeakObjectPtr<ATeamWeaponController> M_TeamWeaponController;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TeamWeapon", meta = (AllowPrivateAccess = "true"))
	FTeamWeaponConfig M_TeamWeaponConfig;

	UPROPERTY()
	TSubclassOf<UNavigationQueryFilter> M_DefaultQueryFilter = nullptr;
};
