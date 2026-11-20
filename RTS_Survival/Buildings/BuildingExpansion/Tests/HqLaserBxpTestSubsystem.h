// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HqLaserBxpTestSubsystem.generated.h"

class ABuildingExpansion;
class ACPPController;
class ACPPTurretsMaster;
class ANomadicVehicle;
enum class ERTSResourceType : uint8;

UENUM()
enum class EHqLaserBxpTestStage : uint8
{
	WaitingForStart,
	WaitingForTruckSpawn,
	WaitingForDeploy,
	WaitingForBxpLoaded,
	WaitingForBxpBuilt,
	WaitingForLaserDamage,
	Finished
};

/**
 * @brief -HqLaserBxpValidate: the HQ Laser Platform (BTX_GerHQLaser) through the real owner flow. A German HQ is
 * spawned and deployed, must offer the laser as a socket option, which is clicked and placed on a free HQ socket. The
 * test checks the price taken (450 radixite, 150 metal), that the Blueprint reports the new type, that its turret
 * carries the Strahlkanone 39, that it has procedural holes, and that the built laser damages an enemy T-34 in front.
 * Optional -HqLaserAnchorX= / -HqLaserAnchorY=. Log lines RTS_HQLASER_TEST; not in shipping builds.
 */
UCLASS()
class RTS_SURVIVAL_API UHqLaserBxpTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	EHqLaserBxpTestStage M_Stage = EHqLaserBxpTestStage::WaitingForStart;
	float M_BootSeconds = -1.f;
	float M_StageSeconds = 0.f;
	int32 M_PassedCheckCount = 0;
	int32 M_FailedCheckCount = 0;
	bool bM_StageActionDone = false;
	FVector M_Anchor = FVector::ZeroVector;
	int32 M_RadixiteBeforeClick = 0;
	int32 M_MetalBeforeClick = 0;
	float M_EnemyStartHealth = 0.f;

	UPROPERTY()
	TWeakObjectPtr<ANomadicVehicle> M_Hq;

	UPROPERTY()
	TWeakObjectPtr<AActor> M_Enemy;

	void TickWaitingForStart();
	void TickWaitingForTruckSpawn();
	void TickWaitingForDeploy();
	void TickWaitingForBxpLoaded();
	void TickWaitingForBxpBuilt();
	void TickWaitingForLaserDamage();

	void SpawnHqTruck();
	void ClickLaserOption();
	void CheckBuiltLaser(ABuildingExpansion& Laser);
	void SpawnEnemyInFront(const ABuildingExpansion& Laser);
	ABuildingExpansion* GetLaser() const;
	ACPPTurretsMaster* GetLaserTurret() const;

	bool StartGameAndGetIsReady() const;
	bool FindAnchor(FVector& OutAnchor) const;
	void GivePlayerResources() const;
	int32 GetResource(ERTSResourceType ResourceType) const;
	ACPPController* GetPlayerController() const;

	void Check(bool bCondition, const FString& Description, const FString& Details = FString());
	void EnterStage(EHqLaserBxpTestStage NewStage);
	float GetNow() const;
	float GetStageElapsed() const;
	void FailAndFinish(const FString& Description, const FString& Details = FString());
	void Finish();
};
