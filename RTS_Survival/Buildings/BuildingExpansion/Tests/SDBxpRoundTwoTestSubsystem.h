// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansionEnums.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "SDBxpRoundTwoTestSubsystem.generated.h"

class ABuildingExpansion;
class ACPPTurretsMaster;
class ACPPController;
class ANomadicVehicle;
class ATankMaster;

UENUM()
enum class ESDBxpRoundTwoTestStage : uint8
{
	WaitingForStart,
	WaitingForSpawns,
	WaitingForDeploy,
	WaitingForBxpLoaded,
	WaitingForBxpsBuilt,
	WaitingForLatePanzerwerfer,
	WaitingForSniperShots,
	WaitingForPackUp,
	WaitingForRedeploy,
	WaitingForReplacedBxps,
	Finished
};

/** @brief One building under test: its truck, the expansions placed on it and where it stands. */
USTRUCT()
struct FSDBxpRoundTwoTestBuilding
{
	GENERATED_BODY()

	ENomadicSubtype Subtype = ENomadicSubtype::Nomadic_None;
	TArray<EBuildingExpansionType> BxpTypes;
	// Slot of each entry of BxpTypes on the building.
	TArray<int32> SlotIndices;
	FVector Location = FVector::ZeroVector;
	float Yaw = 0.f;

	UPROPERTY()
	TWeakObjectPtr<ANomadicVehicle> Truck;
};

/**
 * @brief -SDBxpRoundTwoValidate: the 2026-11-17 Strike Division expansions through the real owner flow. A Mechanized
 * Depot takes the radio van and the Wurfrahmen shop, a Barracks the sniper loft (option click + placement click). Checks
 * vision, the Panzerwerfer reload bonus (live and newly spawned, removed on pack-up, back after redeploy), shell holes
 * (WallHoleDamage debug hits at the aim points), the sniper's arc and fire at an enemy tank, pack-up and redeploy.
 * Optional -SDBxpAnchorX= / -SDBxpAnchorY=. Log lines RTS_SDBXP2_TEST; not in shipping builds.
 */
UCLASS()
class RTS_SURVIVAL_API USDBxpRoundTwoTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	ESDBxpRoundTwoTestStage M_Stage = ESDBxpRoundTwoTestStage::WaitingForStart;
	float M_BootSeconds = -1.f;
	float M_StageSeconds = 0.f;
	int32 M_PassedCheckCount = 0;
	int32 M_FailedCheckCount = 0;
	bool bM_StageActionDone = false;
	FVector M_Anchor = FVector::ZeroVector;

	// Index 0: the Mechanized Depot (radio van, Wurfrahmen shop); index 1: the Barracks (sniper loft).
	UPROPERTY()
	TArray<FSDBxpRoundTwoTestBuilding> M_Buildings;

	int32 M_PlacingBuilding = 0;
	int32 M_PlacingBxp = 0;
	// The buildings pack up and redeploy one after the other (depot first).
	int32 M_PackingBuilding = 0;

	UPROPERTY()
	TWeakObjectPtr<ATankMaster> M_EarlyPanzerwerfer;

	UPROPERTY()
	TWeakObjectPtr<ATankMaster> M_LatePanzerwerfer;

	UPROPERTY()
	TWeakObjectPtr<AActor> M_Enemy;

	float M_BaseReloadSpeed = 0.f;
	int32 M_SniperShots = 0;
	int32 M_LastSniperMagazine = INDEX_NONE;

	void Check(bool bCondition, const FString& Description, const FString& Details = FString());
	float GetNow() const;
	float GetStageElapsed() const;
	void EnterStage(ESDBxpRoundTwoTestStage NewStage);
	void FailAndFinish(const FString& Description, const FString& Details = FString());
	void Finish();
	ACPPController* GetPlayerController() const;
	bool GetBxpOwner(int32 BuildingIndex, TScriptInterface<class IBuildingExpansionOwner>& OutOwner) const;
	ABuildingExpansion* GetPlacedBxp(int32 BuildingIndex, int32 BxpIndex) const;

	void TickWaitingForStart();
	bool StartGameAndGetIsReady() const;
	bool FindAnchor(FVector& OutAnchor) const;
	void GivePlayerResources() const;
	void SetupBuildings();
	void SpawnTrucksAndEarlyPanzerwerfer();
	void SpawnTruck(int32 BuildingIndex);
	void SpawnPanzerwerfer(const FVector& Location, bool bIsLate);
	void TickWaitingForSpawns();

	bool TickDeployAndGetIsBuilding(int32 BuildingIndex);
	void TickWaitingForDeploy();
	bool PickSlotsForBuilding(int32 BuildingIndex);
	void StartPlacingNextBxp();
	void TickWaitingForBxpLoaded();
	void TickWaitingForBxpsBuilt();
	bool GetAreAllBxpsBuilt(int32 BuildingIndex) const;

	void CheckBuiltBxps();
	void CheckVision(const ABuildingExpansion& Bxp, float ExpectedVisionRadius);
	void CheckHoles(ABuildingExpansion& Bxp);
	float GetPanzerwerferReloadSpeed(const ATankMaster* Panzerwerfer) const;
	void CheckPanzerwerferReload(const ATankMaster* Panzerwerfer, bool bExpectBonus, const FString& Moment);
	void TickWaitingForLatePanzerwerfer();

	void CheckSniperTurretAndSpawnEnemy();
	ACPPTurretsMaster* GetSniperTurret() const;
	AActor* SpawnUnitForPlayer(const TCHAR* ClassPath, const FTransform& Transform, uint8 OwningPlayer) const;
	void TickWaitingForSniperShots();
	void CountSniperShots(ACPPTurretsMaster& SniperTurret);

	void StartPackUp(int32 BuildingIndex);
	void TickWaitingForPackUp();
	void TickWaitingForRedeploy();
	void TickWaitingForReplacedBxps();
};
