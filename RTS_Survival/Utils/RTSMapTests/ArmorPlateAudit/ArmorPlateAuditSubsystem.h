// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RTS_Survival/RTSComponents/ArmorComponent/Armor.h"

#include "ArmorPlateAuditSubsystem.generated.h"

class ACPPController;
class ATankMaster;
class UArmorCalculation;
class UMeshComponent;

enum class EArmorPlateAuditStep : uint8
{
	WaitingForGameStart,
	SpawnNext,
	WaitingForModules,
	Done
};

/** The side of the vehicle a probe shell comes from, in actor space. */
enum class EArmorPlateAuditSide : uint8
{
	Left,
	Right,
	Front,
	Rear
};

/** One probe shell: where it came from, where it hit and the plate the armour calculation resolved. */
struct FArmorPlateAuditProbe
{
	EArmorPlateAuditSide Side = EArmorPlateAuditSide::Left;
	// Offset along the side, as a fraction of the vehicle's length (sides) or width (front and rear).
	float AlongFraction = 0.f;
	float HeightFraction = 0.f;
	bool bHitMesh = false;
	FString HitMeshName;
	FVector ActorSpaceHit = FVector::ZeroVector;
	EArmorPlate PlateHit = EArmorPlate::Plate_Front;
	float RawArmor = 0.f;
	int32 PlateRegistrationId = INDEX_NONE;
};

/**
 * @brief Audits the armour boxes of many vehicle Blueprints in one offscreen game run: started with -ArmorPlateAudit
 * (Tools/ArmorPlateAudit/run_armor_audit.sh). Spawns each class in turn, logs its module state, every registered plate
 * box in actor space and the plates that shells from the left, right, front and rear resolve; grep RTS_ARMOR_AUDIT.
 * -ArmorPlateAuditList=<file> names one class path per line. -ArmorPlateAuditAsDrawn also probes each vehicle again with
 * its boxes re-registered at the size the debug draw shows (unit scale), to preview that migration. Not created in shipping.
 */
UCLASS()
class RTS_SURVIVAL_API UArmorPlateAuditSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }

private:
	EArmorPlateAuditStep M_Step = EArmorPlateAuditStep::WaitingForGameStart;
	double M_StepStartRealSeconds = 0.0;
	float M_StepStartGameSeconds = 0.f;

	TArray<FString> M_VehicleClassPaths;
	int32 M_NextVehicleIndex = 0;
	int32 M_VehiclesWithWrongPlates = 0;
	int32 M_VehiclesWithoutModules = 0;

	TWeakObjectPtr<ACPPController> M_PlayerController;
	TWeakObjectPtr<ATankMaster> M_Tank;
	TWeakObjectPtr<UArmorCalculation> M_Armor;
	FVector M_BaseLocation = FVector::ZeroVector;
	bool bM_IsSuppressingErrorDialogs = false;
	bool bM_RunAsDrawn = false;

	void EnterStep(EArmorPlateAuditStep NewStep);
	float GetGameSecondsInStep() const;
	bool TryStartGame();
	void KeepGameRunning() const;
	bool LoadVehicleList();
	void FindBaseLocation();

	void SpawnNextVehicle();
	void AuditCurrentVehicle();
	void LogModules(const FString& VehicleName) const;
	void LogPlateBoxes(const FString& VehicleName) const;

	/**
	 * @brief Collects the registered armour meshes with their actor-space bounds, which place the probe shells.
	 * @param OutMeshes Registered meshes of the current vehicle.
	 * @return Actor-space bounds of all registered meshes; invalid when nothing is registered.
	 */
	FBox GetRegisteredMeshes(TArray<UMeshComponent*>& OutMeshes) const;

	/**
	 * @brief Fires the probe pattern at the current vehicle and logs each resolved plate with a verdict.
	 * @param VehicleName Name used in the log lines.
	 * @return The number of probes that resolved a plate on the opposite side.
	 */
	int32 RunProbes(const FString& VehicleName) const;

	/**
	 * @brief What-if for the unit-scale migration: legacy boxes cover extent * scale^2, the debug draw shows
	 * extent * scale. Re-registers every plate at the drawn size with unit scale; only for the audited copy.
	 */
	void ReRegisterPlatesAtDrawnSize() const;

	FArmorPlateAuditProbe ResolveProbe(EArmorPlateAuditSide Side, float AlongFraction, float HeightFraction,
	                                   const FBox& ActorBounds, const TArray<UMeshComponent*>& Meshes) const;
	void Finish();
};
