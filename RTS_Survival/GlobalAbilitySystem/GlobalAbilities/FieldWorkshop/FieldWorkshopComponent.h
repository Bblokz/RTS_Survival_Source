// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Subsystems/RadiusSubsystem/ERTSRadiusType.h"
#include "FieldWorkshopComponent.generated.h"

class ABuildingExpansion;
class UAOEBehaviourComponent;
class UFieldWorkshopRearmAura;
class URTSRadiusPoolSubsystem;

UENUM(BlueprintType)
enum class EFieldWorkshopState : uint8
{
	// Falling under its canopies (or not yet built): the auras are off.
	WaitingForLanding,
	// Repairs and rearms the owner's vehicles in its radius.
	Working,
	// Time is up: the auras are gone and the building plays its pack-up animation, then removes itself.
	PackingUp,
	// Killed: the auras are gone, the Blueprint's death plays out.
	Destroyed
};

USTRUCT(BlueprintType)
struct FFieldWorkshopSettings
{
	GENERATED_BODY()

	// How long the workshop repairs and rearms once it has landed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1.0"))
	float WorkingSeconds = 90.f;

	// Length of the pack-up animation (construction materials creeping back) before the building is removed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.5"))
	float PackUpSeconds = 6.f;

	// The ground disc shown for the whole working time, sized to the rearm aura's radius.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ERTSRadiusType WorkingRadiusType = ERTSRadiusType::FullCircle_AmmoResupplyAura;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.05"))
	float LandingCheckIntervalSeconds = 0.25f;
};

/**
 * @brief Turns a building expansion into a timed field workshop (UGA_BuildingParachuteDrop drops it): keeps the
 * actor's AOE auras (URadiusRepairAura + UFieldWorkshopRearmAura) off while the building falls, runs them for
 * WorkingSeconds after it landed with the radius drawn on the ground, then packs the building up and removes it.
 * When the building is killed first, the auras are torn down at once, which strips their behaviours from the units.
 * Landing = built, collision back on and no cargo canopy still descending, so a building placed without a drop
 * starts working at once.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), BlueprintType)
class RTS_SURVIVAL_API UFieldWorkshopComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFieldWorkshopComponent();

	EFieldWorkshopState GetWorkshopState() const { return M_State; }
	/** @return Seconds of work left; WorkingSeconds before landing, 0 once packing up or destroyed. */
	float GetWorkingSecondsRemaining() const;
	/** @return World time the workshop started working; negative before it landed. */
	float GetWorkStartSeconds() const { return M_WorkStartSeconds; }
	const FFieldWorkshopSettings& GetWorkshopSettings() const { return M_WorkshopSettings; }
	/** @return Radius of the rearm aura (the disc's size); 0 without one. */
	float GetWorkshopRadius() const;
	bool GetIsShowingWorkingRadius() const { return M_RadiusId > 0; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = "true"))
	FFieldWorkshopSettings M_WorkshopSettings;

	EFieldWorkshopState M_State = EFieldWorkshopState::WaitingForLanding;
	float M_WorkStartSeconds = -1.f;

	UPROPERTY()
	TWeakObjectPtr<ABuildingExpansion> M_Workshop;

	// The owner's auras; torn down (DestroyComponent) at the end so their EndPlay strips what they applied.
	UPROPERTY()
	TArray<TWeakObjectPtr<UAOEBehaviourComponent>> M_Auras;

	UPROPERTY()
	TWeakObjectPtr<UFieldWorkshopRearmAura> M_RearmAura;

	UPROPERTY()
	TWeakObjectPtr<URTSRadiusPoolSubsystem> M_RadiusPoolSubsystem;

	int32 M_RadiusId = INDEX_NONE;

	FTimerHandle M_LandingCheckHandle;
	FTimerHandle M_WorkingTimerHandle;
	FTimerHandle M_PackUpTimerHandle;

	void BeginPlay_CacheWorkshopParts();
	void BeginPlay_BindWorkshopDeath();
	void BeginPlay_StartLandingChecks();

	void CheckForLanding();
	bool GetHasLanded() const;
	void StartWorking();
	void StartPackingUp();
	void FinishPackingUp() const;
	void OnWorkshopDies();

	void SetAurasEnabled(bool bEnabled) const;
	void TearDownAuras();
	void ShowWorkingRadius();
	void HideWorkingRadius();
	void ClearTimers();

	bool GetIsValidWorkshop() const;
	bool GetIsValidRadiusPoolSubsystem() const;
};
