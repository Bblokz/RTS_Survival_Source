// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/Buildings/BuildingExpansion/B4HowitzerPit/Tests/B4HowitzerCaptureRig.h"
#include "FieldKitGunnerTestSubsystem.generated.h"

class AEmbeddedInfantryTurret;
class UEmbeddedTurretMountComponent;

UENUM()
enum class EFieldKitGunnerTestPhase : uint8
{
	WaitingForStart,
	// The hosts are out; the kit is not researched yet, so no kit gunner may show.
	BeforeResearch,
	// The kit is researched; every host must get exactly one MG 42 gunner on the expected mesh.
	WaitingForMounts,
	// Each host attacks the ground in front of it; its gunner must fire.
	AttackingGround,
	Finished
};

/** One tank of the MG 42 hatch gunner kit test, with where its kit gunner must end up. */
USTRUCT()
struct FFieldKitGunnerHostUnderTest
{
	GENERATED_BODY()

	FString Label;
	FString ClassPath;
	// Substring of the static mesh asset the gunner's mount must hang on; empty for a control tank (no gunner).
	FString ExpectedMountMesh;
	// The variant's own gunner the kit replaces (Pz IV H StandardPlus); empty when none.
	FString ReplacedGunnerClassName;
	bool bSpawnedAfterResearch = false;

	UPROPERTY()
	TWeakObjectPtr<AActor> Tank;

	UPROPERTY()
	TWeakObjectPtr<AEmbeddedInfantryTurret> Gunner;

	TArray<int32> LastMagazineCounts;
	int32 Shots = 0;
	FVector GroundTarget = FVector::ZeroVector;
};

/**
 * @brief Map test for the MG 42 hatch gunner field kit, started with -FieldKitGunnerValidate on RTS_ToCopySmall:
 * spawns every kit tank and variant plus the Pz III J and Pz IV F1 controls, researches the kit through the player tech
 * manager and checks that each tank gets exactly one MG 42 gunner on the expected turret or hull mesh (the StandardPlus
 * loses its MG 34 gunner), that a tank built afterwards gets one too, and that every gunner fires on an attack-ground
 * order. -FKCapture=<folder> saves pictures of every host. Logs RTS_FIELDKIT_TEST; not in shipping.
 */
UCLASS()
class RTS_SURVIVAL_API UFieldKitGunnerTestSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

private:
	EFieldKitGunnerTestPhase M_Phase = EFieldKitGunnerTestPhase::WaitingForStart;
	float M_PhaseStartSeconds = -1.f;
	int32 M_FailedCheckCount = 0;
	int32 M_PassedCheckCount = 0;
	FVector M_Anchor = FVector::ZeroVector;
	bool bM_ResearchedThroughTechManager = false;

	UPROPERTY()
	TArray<FFieldKitGunnerHostUnderTest> M_Hosts;

	FB4HowitzerCaptureRig M_CaptureRig;

	void Check(const bool bCondition, const FString& Description, const FString& Details = FString());
	float GetNow() const;
	void EnterPhase(EFieldKitGunnerTestPhase NewPhase);
	void Finish();

	void TickWaitingForStart();
	void TickBeforeResearch();
	void TickWaitingForMounts();
	void TickAttackingGround();

	void InitHostList();
	bool SpawnHost(FFieldKitGunnerHostUnderTest& Host, const int32 SlotIndex);
	void ResearchKit();
	void CheckHostAfterResearch(FFieldKitGunnerHostUnderTest& Host);
	void CheckRepeatedMountRequestAddsNothing(const FFieldKitGunnerHostUnderTest& Host);
	void CheckGunnerIsClean(const FFieldKitGunnerHostUnderTest& Host);
	void StartAttackGround();
	void CountShots(FFieldKitGunnerHostUnderTest& Host) const;
	void LogHostMeshes(const FFieldKitGunnerHostUnderTest& Host) const;

	TArray<AEmbeddedInfantryTurret*> GetMountedGunners(const AActor* Tank) const;
	TArray<UEmbeddedTurretMountComponent*> GetKitMounts(const AActor* Tank) const;
	bool GetGroundAt(const FVector2D& Location2D, FVector& OutGround) const;
	AActor* SpawnUnitForPlayer(const FString& ClassPath, const FTransform& Transform, uint8 OwningPlayer) const;
	void KeepHostsAlive() const;
	void CaptureHost(const FFieldKitGunnerHostUnderTest& Host);
};
