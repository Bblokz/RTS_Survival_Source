// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PanzerwerferUnitTestSubsystem.generated.h"

class ACPPController;
class ANomadicVehicle;
class ATankMaster;
class UWeaponState;

UENUM()
enum class EPanzerwerferUnitTestStage : uint8
{
	WaitingForStart,
	WaitingForSpawns,
	WaitingForDeploy,
	WaitingForTrainedUnit,
	WaitingForMove,
	WaitingForShopLoaded,
	WaitingForShopBuilt,
	WaitingForRockets,
	WaitingForPackUp,
	WaitingForWreck,
	Finished
};

/**
 * @brief -PanzerwerferUnitValidate: the Panzerwerfer 42 (Tank_Panzerwerfer) through the real owner flow. A Mechanized
 * Depot trains it with the training-menu click, it drives, fires its rockets at an enemy tank, the Wurfrahmen shop's
 * reload bonus reaches it (and the Pz 38(t) R) and leaves on pack-up, and it dies into its wreck.
 * Optional -PzwAnchorX= / -PzwAnchorY=. Log lines RTS_PZW_UNIT_TEST; not in shipping builds.
 */
UCLASS()
class RTS_SURVIVAL_API UPanzerwerferUnitTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	EPanzerwerferUnitTestStage M_Stage = EPanzerwerferUnitTestStage::WaitingForStart;
	float M_BootSeconds = -1.f;
	float M_StageSeconds = 0.f;
	int32 M_PassedCheckCount = 0;
	int32 M_FailedCheckCount = 0;
	bool bM_StageActionDone = false;
	FVector M_Anchor = FVector::ZeroVector;

	UPROPERTY()
	TWeakObjectPtr<ANomadicVehicle> M_Depot;

	// The unit the depot trained.
	UPROPERTY()
	TWeakObjectPtr<ATankMaster> M_Panzerwerfer;

	// Spawned directly: the other subtype the Wurfrahmen shop speeds up (regression check).
	UPROPERTY()
	TWeakObjectPtr<ATankMaster> M_Pz38tR;

	UPROPERTY()
	TWeakObjectPtr<AActor> M_Enemy;

	FVector M_MoveStart = FVector::ZeroVector;
	FVector M_DeathLocation = FVector::ZeroVector;
	float M_BaseReloadSpeed = 0.f;
	float M_Pz38tRBaseReloadSpeed = 0.f;
	int32 M_RocketsFired = 0;
	int32 M_LastMagazine = INDEX_NONE;

	void Check(bool bCondition, const FString& Description, const FString& Details = FString());
	float GetNow() const;
	float GetStageElapsed() const;
	void EnterStage(EPanzerwerferUnitTestStage NewStage);
	void FailAndFinish(const FString& Description, const FString& Details = FString());
	void Finish();
	ACPPController* GetPlayerController() const;

	void TickWaitingForStart();
	bool StartGameAndGetIsReady() const;
	bool FindAnchor(FVector& OutAnchor) const;
	void GivePlayerResources() const;
	void SpawnDepotAndPz38tR();
	AActor* SpawnUnitForPlayer(const TCHAR* ClassPath, const FTransform& Transform, uint8 OwningPlayer) const;
	void TickWaitingForSpawns();
	void TickWaitingForDeploy();
	void ClickPanzerwerferTrainingOption();

	ATankMaster* FindTrainedPanzerwerfer() const;
	static UWeaponState* GetRocketWeapon(const ATankMaster* Tank);
	void TickWaitingForTrainedUnit();
	void CheckTrainedUnit(ATankMaster& Panzerwerfer);
	void TickWaitingForMove();

	void StartPlacingShop();
	void TickWaitingForShopLoaded();
	void TickWaitingForShopBuilt();
	void CheckReload(const ATankMaster* Tank, float BaseReload, bool bExpectBonus, const FString& Moment);

	void SpawnEnemyAndAttack();
	void KeepAlive(AActor* Actor) const;
	void CountRockets();
	void TickWaitingForRockets();

	void TickWaitingForPackUp();
	void KillPanzerwerfer();
	void TickWaitingForWreck();
	AActor* FindWreckNear(const FVector& Location) const;
};
