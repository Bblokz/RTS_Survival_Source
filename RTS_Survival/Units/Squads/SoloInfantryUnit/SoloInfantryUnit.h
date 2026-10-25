// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "SoloInfantryUnit.generated.h"

class UCargo;

/**
 * @brief Place this infantry unit directly in a level when no squad controller should own it.
 * It keeps the squad-unit animation and weapon setup, but independently holds its placed or startup cover position.
 */
UCLASS(Blueprintable)
class RTS_SURVIVAL_API ASoloInfantryUnit : public ASquadUnit
{
	GENERATED_BODY()

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
	friend struct FSoloInfantryUnitTestAccess;
#endif

public:
	ASoloInfantryUnit(const FObjectInitializer& ObjectInitializer);

	virtual bool GetIsUnitIdle() const override;
	virtual void OnSpecificTargetOutOfRange(const FVector& TargetLocation) override;
	virtual void OnSpecificTargetDestroyedOrInvisible() override;

	bool GetIsInsideCargo() const { return bM_IsInsideCargo; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	virtual bool GetIsValidSquadController() const override;
	virtual bool GetCanUseAutomaticCover() const override;
	virtual bool GetMayKeepAutomaticCover() const override;
	virtual bool GetMayRelocateFromCoverForStress() const override;
	virtual bool GetIsSquadOnHoldPosition() const override;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Start Game",
		meta = (AllowPrivateAccess = "true", DisplayName = "Hide In Nearest Free Cover"))
	bool bM_HideInNearestFreeCover = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Start Game",
		meta = (AllowPrivateAccess = "true", DisplayName = "Enter Cargo"))
	TWeakObjectPtr<AActor> M_EnterCargoActor;

	UPROPERTY()
	TWeakObjectPtr<UCargo> M_CurrentCargo;

	FTimerHandle M_StartCoverTimer;
	bool bM_HasAttemptedStartingCover = false;
	bool bM_HasStartCargoRequest = false;
	bool bM_IsInsideCargo = false;

	void BeginPlay_SetupWeaponForSoloUse();
	void BeginPlay_StartConfiguredAction();
	void BeginPlay_WaitForCoverGeneration();
	void TryStartGameCover();
	void TryEnterStartCargo();
	bool AttachToCargoSeat(UCargo& CargoComponent, FName SeatSocketName);
	void ReleaseCargoSeat();

	bool GetIsValidEnterCargoActor() const;
	bool GetIsValidCurrentCargo() const;

	static bool GetIsFiniteVector(const FVector& Vector);
	static bool GetIsFiniteRotator(const FRotator& Rotator);
	static bool GetIsFiniteTransform(const FTransform& Transform);
};
