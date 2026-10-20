// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AimAbilityComponent/AimAbilityComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AimAbilityComponent/FlareAimAbility/FlareProjectile.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"
#include "FlareAimAbilityComponent.generated.h"

class ATankMaster;
class UStaticMeshComponent;

/** @brief Settings of the flare aim ability; the base aim ability settings hold its type, cooldown and card slot. */
USTRUCT(BlueprintType)
struct FFlareAimAbilitySettings
{
	GENERATED_BODY()

	// The unit drives closer until the target is within this distance, then launches the flare.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="100"))
	float FireRange = 7000.f;

	// How high above the ground at the target the flare goes before it ignites and opens its parachute. Keep it
	// below the RTS camera (about 28 m above the ground it looks at).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="100"))
	float ApexHeight = 1200.f;

	// How long the flare stays up: seconds of vision while it sinks from its apex to the ground.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.5"))
	float HangTimeSeconds = 20.f;

	// Radius of the fog of war vision bubble around the burning flare.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="100"))
	float VisionRadius = 3000.f;

	// Average climb speed of the cartridge in cm/s.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="100"))
	float AscentSpeed = 3000.f;

	// Socket the flare leaves from: on the unit, its turret or any child actor. Its +X axis is the tube direction.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName LaunchSocketName = TEXT("FlareLaunch");

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FFlareVisualSettings Visuals;
};

/**
 * @brief Aim ability that launches a parachute flare at the aimed spot instead of firing weapons. The unit drives
 * into FireRange, the flare leaves the launch socket (a smoke discharger tube on the turret) and the command
 * completes; the flare (AFlareProjectile) climbs to ApexHeight above the target and gives vision in VisionRadius
 * for HangTimeSeconds while it sinks to the ground. Weapons are not touched. Supports tank owners.
 * Set the base settings' AimAbilityType to FlareLaunch.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UFlareAimAbilityComponent : public UAimAbilityComponent
{
	GENERATED_BODY()

public:
	UFlareAimAbilityComponent();

	// The aim preview shows the vision bubble.
	virtual float GetAimAbilityRadius() const override;

	const FFlareAimAbilitySettings& GetFlareSettings() const { return M_FlareSettings; }

protected:
	virtual void PostInitProperties() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual bool GetUsesWeaponBehaviour() const override;
	virtual bool GetAbilityMaxRange(float& OutMaxRange) const override;
	virtual void ExecuteAbilityInRange(const FVector& TargetLocation) override;
	virtual void OnAbilityInRangeFinished() override;
	virtual void RequestMoveToLocation(const FVector& MoveToLocation) override;
	virtual void StopMovementForAbility() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Flare")
	FFlareAimAbilitySettings M_FlareSettings;

private:
#if RTS_WITH_MAP_TESTS
	friend struct FFlareAbilityTestAccess;
#endif

	UPROPERTY()
	TWeakObjectPtr<ATankMaster> M_TankMaster;

	// The flare of the last launch, while it exists.
	UPROPERTY()
	TWeakObjectPtr<AFlareProjectile> M_LastLaunchedFlare;

	// Shows Visuals.LauncherMesh on the launch socket.
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> M_LauncherMeshComponent;

	FTimerHandle M_AttachLauncherTimerHandle;
	int32 M_AttachLauncherAttempts = 0;

	void BeginPlay_ValidateFlareSettings() const;

	/** The turret carrying the socket is a child actor that may spawn after BeginPlay; retries for a while. */
	void BeginPlay_AttachLauncherMesh();
	void TryAttachLauncherMesh();

	/**
	 * @brief Finds the component carrying the launch socket on the owner or one of its child actors (the turret).
	 * @return The component, or null when no component has the socket.
	 */
	USceneComponent* FindLaunchSocketComponent() const;

	/**
	 * @brief Where and in which direction the flare leaves the unit.
	 * @param OutLocation Socket location, or above the unit without a socket.
	 * @param OutDirection Socket +X, or straight up without a socket.
	 * @return The component carrying the socket; null when the fallback above the unit is used.
	 */
	USceneComponent* GetLaunchTransform(FVector& OutLocation, FVector& OutDirection) const;

	FVector GetGroundTargetLocation(const FVector& TargetLocation) const;
	AFlareProjectile* LaunchFlareAt(const FVector& TargetLocation);
	void PlayLaunchEffects(USceneComponent* SocketComponent, const FVector& LaunchLocation,
	                       const FVector& LaunchDirection) const;
	uint8 GetOwningPlayer() const;

	bool GetIsValidTankMaster() const;
};
