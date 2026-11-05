// Copyright (C) Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FieldWorkshopTestSubsystem.generated.h"

class ABuildingExpansion;
class ACameraActor;
class ATankMaster;
class UFieldWorkshopComponent;
class UGA_CompositeDrop;
class UGlobalAbility;
class UWeaponState;

UENUM()
enum class EFieldWorkshopTestStep : uint8
{
	WaitingForStart,
	// Scenario A: GA_GerFieldWorkshopDrop clicked through the player's controller.
	WaitingForWorkshop,
	WaitingForGuns,
	WaitingForTanks,
	Measuring,
	WaitingForPackUp,
	WaitingForRemoval,
	// Scenario B: a workshop placed on the ground, then killed while it repairs.
	KillWaitingForRepair,
	KillAfterDeath,
	Finished
};

/**
 * @brief -FieldWorkshopValidate: fires GA_GerFieldWorkshopDrop on the anchor (-FieldWorkshopAnchorX= / -Y=, else the
 * camera centre) and checks the dropped workshop (upright, built, player 1, selectable), the two parachuted Pak 40s
 * (deployed, crewed, beside the workshop), repair and rearm of a damaged and a spent friendly tank inside the radius,
 * nothing for a friendly tank outside and an enemy tank inside, the pack-up after its working time, and in a second
 * scenario that killing a workshop stops its repair. Screenshots to Saved/FieldWorkshopTest; logs
 * RTS_FIELD_WORKSHOP_TEST. Not in shipping.
 */
UCLASS()
class RTS_SURVIVAL_API UFieldWorkshopTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	struct FTestTank
	{
		FString Label;
		TWeakObjectPtr<ATankMaster> Tank;
		int32 Owner = 1;
		bool bInside = true;
		bool bDamage = false;
		bool bSpend = false;
		float HealthAtStart = 0.f;
		// Weapons emptied by one shot each; the workshop must refill them, nobody else may.
		TArray<TWeakObjectPtr<UWeaponState>> SpentWeapons;
	};

	EFieldWorkshopTestStep M_Step = EFieldWorkshopTestStep::WaitingForStart;
	float M_StartSeconds = -1.f;
	float M_StepStartSeconds = 0.f;
	int32 M_PassedCheckCount = 0;
	int32 M_FailedCheckCount = 0;
	int32 M_ScreenshotCount = 0;
	FString M_ScenarioLabel = TEXT("setup");
	FVector M_Anchor = FVector::ZeroVector;
	FVector M_CameraTarget = FVector::ZeroVector;
	float M_CameraDistance = 6500.f;

	TSet<TWeakObjectPtr<AActor>> M_PreExistingActors;
	TWeakObjectPtr<ABuildingExpansion> M_Workshop;
	TWeakObjectPtr<UFieldWorkshopComponent> M_WorkshopComponent;
	float M_WorkStartSeconds = -1.f;
	float M_ParentEndSeconds = -1.f;
	float M_PackUpSeconds = -1.f;
	float M_PackUpStartZ = 0.f;
	bool bM_CheckedSinking = false;
	float M_TanksPreparedSeconds = -1.f;
	TArray<FTestTank> M_Tanks;
	bool bM_TookMidShot = false;
	float M_KillSeconds = -1.f;
	float M_HealthAfterKill = -1.f;

	UPROPERTY()
	TObjectPtr<UGA_CompositeDrop> M_Ability;

	UPROPERTY()
	TObjectPtr<ACameraActor> M_Camera;

	void Check(bool bCondition, const FString& Description, const FString& Details = FString());
	float GetNow() const;
	float GetStepSeconds() const;
	void SetStep(EFieldWorkshopTestStep NewStep);
	void Finish();

	void TickWaitingForStart();
	bool FindAnchor(FVector& OutAnchor) const;
	/** Moves the anchor towards the landscape centre until the whole test site has ground and no enemy is near. */
	bool FindClearSite(FVector& InOutAnchor) const;
	bool GetIsSiteClear(const FVector& Candidate, FString& OutReason) const;
	void StartDropScenario();
	bool CheckTemplate(const UGA_CompositeDrop& Ability);
	bool EnsureCanAfford(const UGA_CompositeDrop& Ability, TMap<uint8, int32>& OutBefore);
	void CheckPayment(const UGA_CompositeDrop& Ability, const TMap<uint8, int32>& Before);
	void OnParentEnded(UGlobalAbility* Parent);

	void TickWaitingForWorkshop();
	bool TryFindNewWorkshop();
	void CheckLandedWorkshop();
	void TickWaitingForGuns();
	void CheckGunSquads();
	void RestrictGunFire();
	void TickWaitingForTanks();
	void SpawnTestTanks();
	void AddTestTank(const FString& Label, int32 Owner, bool bInside, bool bDamage, bool bSpend, float AngleDegrees);
	void PrepareTestTanks();
	void SpendTankWeapons(FTestTank& TestTank);
	void TickMeasuring();
	void CheckTankResults();
	void CheckTankBehaviours(const FTestTank& TestTank, bool bExpectWorkshopEffects);
	int32 CountRefilledWeapons(const FTestTank& TestTank) const;
	void TickWaitingForPackUp();
	void TickWaitingForRemoval();

	void StartKillScenario();
	ABuildingExpansion* SpawnWorkshopOnGround(const FVector& Location);
	void TickKillWaitingForRepair();
	void TickKillAfterDeath();

	TArray<AActor*> GetNewActorsOfClass(const UClass* ActorClass) const;
	void SnapshotExistingActors();
	ATankMaster* SpawnTank(const TCHAR* ClassPath, const FVector& Location, float Yaw, int32 Owner) const;
	void UpdateCamera() const;
	void RequestScreenshot(const FString& Label);
};
