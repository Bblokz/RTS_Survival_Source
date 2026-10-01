// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "Resistances/Resistances.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/RTSComponents/ArmorComponent/Armor.h"
#include "RTS_Survival/Weapons/WeaponData/RTSDamageTypes/RTSDamageTypes.h"
#include "VehicleModules/VehicleModuleBalance.h"
#include "VehicleModules/VehicleModuleBatches.h"

#include "ArmorCalculation.generated.h"

class UShieldComponent;
class ATankMaster;
class UHealthComponent;
class UWeaponState;
struct FModuleDamageInput;


USTRUCT(BlueprintType)
struct FArmorSettings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	FTransform ArmorBoxTransform = FTransform::Identity;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	FBox ArmorBox = FBox(FVector(-50.0f, -50.0f, -50.0f), FVector(50.0f, 50.0f, 50.0f));

	// Structural armor; module damage never changes it.
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	float ArmorValue = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	EArmorPlate ArmorType = EArmorPlate::Plate_Front;

	// Extra armor supplied by an add-on armor module covering this plate; only its assigned behaviour scales it.
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly)
	float AddOnArmorValue = 0.0f;
};

using DeveloperSettings::GameBalance::Weapons::MaxArmorPlatesPerMesh;

USTRUCT()
struct FArmorSetup
{
	GENERATED_BODY()

	FArmorSetup()
	{
		for (int i = 0; i < MaxArmorPlatesPerMesh; i++)
		{
			ArmorSettings0[i].ArmorValue = 0.0f;
			ArmorSettings1[i].ArmorValue = 0.0f;
			ArmorSettings2[i].ArmorValue = 0.0f;
		}
	}

	UPROPERTY()
	FArmorSettings ArmorSettings0[MaxArmorPlatesPerMesh];
	UPROPERTY()
	FArmorSettings ArmorSettings1[MaxArmorPlatesPerMesh];
	UPROPERTY()
	FArmorSettings ArmorSettings2[MaxArmorPlatesPerMesh];

	// Distinguishes registered plates with zero armor from unused fixed-array entries.
	UPROPERTY()
	int32 NumArmorPlates0 = 0;
	UPROPERTY()
	int32 NumArmorPlates1 = 0;
	UPROPERTY()
	int32 NumArmorPlates2 = 0;

	UPROPERTY()
	TObjectPtr<UMeshComponent> MeshWithArmor0 = nullptr;
	UPROPERTY()
	TObjectPtr<UMeshComponent> MeshWithArmor1 = nullptr;
	UPROPERTY()
	TObjectPtr<UMeshComponent> MeshWithArmor2 = nullptr;
};

/** @brief Per-impact candidate damage with at most one entry per plate candidate; returned by value. */
struct FModuleDamageBatch
{
	struct FEntry
	{
		int32 SlotIndex = INDEX_NONE;
		int32 CandidateIndex = INDEX_NONE;
		float Damage = 0.f;
	};

	FEntry Entries[VehicleModuleBalance::MaxModulesPerPlate];
	int32 Count = 0;

	void Append(const int32 SlotIndex, const int32 CandidateIndex, const float Damage)
	{
		if (Count >= VehicleModuleBalance::MaxModulesPerPlate)
		{
			return;
		}
		Entries[Count].SlotIndex = SlotIndex;
		Entries[Count].CandidateIndex = CandidateIndex;
		Entries[Count].Damage = Damage;
		++Count;
	}
};

/**
 * @brief Conservative fixed-bucket ring of non-penetrating damage applied to one module across all attackers.
 * A bucket is expired only once its whole duration left the window, so the budget never under-counts.
 */
struct FVehicleModuleNonPenBudget
{
	float BucketDamage[VehicleModuleBalance::NonPenWindowBucketCount] = {};
	int32 BucketEpoch[VehicleModuleBalance::NonPenWindowBucketCount] = {};
};

/** @brief Runtime record of one fixed module slot; module HP is derived from the tank's MaxHealth. */
struct FVehicleModule
{
	int32 ModuleId = INDEX_NONE;
	EVehicleModuleTypes Type = EVehicleModuleTypes::None;
	float CurrentHp = 0.f;
	float MaxHp = 0.f;
	EVehicleModuleState State = EVehicleModuleState::Healthy;
	// Incremented on every HP change so observers can detect stale snapshots.
	uint32 Revision = 0;
	bool bInstalled = false;
	FVehicleModuleNonPenBudget NonPenBudget;
};

/** @brief Stable registration of one module slot; component references stay weak. */
USTRUCT()
struct FVehicleModuleBinding
{
	GENERATED_BODY()

	// Registered armor mesh slot (0..2) that owns the turret or weapon plates; INDEX_NONE when unbound.
	int32 ArmorMeshSlot = INDEX_NONE;

	bool bRightSide = false;

	UPROPERTY()
	TWeakObjectPtr<UMeshComponent> BoundMesh;

	// Gun of a Weapon module when several guns share the bound mesh; explicitly null falls back to mesh matching.
	UPROPERTY()
	TWeakObjectPtr<UWeaponState> BoundWeapon;

	// Add-on coverage over stable plate registration IDs (mesh slot * plates per mesh + plate index).
	uint64 CoveredPlateMask = 0;

	// Coverage that could not be resolved on load; retained until the mesh registers.
	bool bHasUnresolvedCoverage = false;
};

/**
 * @brief Precomputed target of one plate candidate.
 * Centre plates hit running gear on either side: primary is the left slot, alternate the right slot,
 * and the hull-local lateral hit position picks one at impact time.
 */
struct FModulePlateRoute
{
	int8 PrimarySlot = INDEX_NONE;
	int8 AlternateSlot = INDEX_NONE;
	bool bUsesHitSide = false;
};

/**
 * @brief Register tank mesh armour plates here so weapon hits can resolve armour from the impacted mesh.
 * Tank Blueprints also install vehicle modules here (profile, running gear, SetupModule, add-on coverage)
 * before the tank finalizes module HP from its MaxHealth.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UArmorCalculation : public UActorComponent
{
	GENERATED_BODY()

public:
	UArmorCalculation();

	void SetShieldComponent(UShieldComponent* ShieldComponent);
	UShieldComponent* GetShieldComponent() const { return M_ShieldComponent.Get(); }

	// Removes any registered meshes and their armor plates.
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "ArmorSettings")
	void ClearArmorSetup();

	UFUNCTION(BlueprintCallable, NotBlueprintable )
	float GetRearArmor() const;

	/**
	 * Initializes armor calculation for the provided MeshWithArmor by storing the supplied ArmorSettings.
	 * If all armor slots are used, it reports an error via RTSFunctionLibrary::ReportError.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable, Category = "ArmorSettings")
	void InitArmorCalculation(UMeshComponent* MeshWithArmor, TArray<FArmorSettings> ArmorSettingsForMesh,
	                          const uint8 PlayerOwningArmor);

	/**
	 * @brief Allows Blueprint upgrades to scale current armor on a specific registered mesh.
	 * @param MeshWithArmor Mesh previously supplied to InitArmorCalculation.
	 * @param PlateType Every registered plate of this type on the mesh is updated.
	 * @param ArmorValueMultiplier Finite, nonnegative multiplier applied to each plate's current armor.
	 * @return True if at least one matching plate was updated.
	 */
	UFUNCTION(BlueprintCallable, Category = "ArmorSettings")
	bool MultiplyArmorOfPlateType(UMeshComponent* MeshWithArmor, EArmorPlate PlateType, float ArmorValueMultiplier);

	/**
	 * @brief Allows Blueprint armor changes to replace current values on a specific registered mesh.
	 * @param MeshWithArmor Mesh previously supplied to InitArmorCalculation.
	 * @param PlateType Every registered plate of this type on the mesh is updated, including zero-armor plates.
	 * @param NewArmorValue Finite, nonnegative armor value assigned to each matching plate.
	 * @return True if at least one matching plate was updated.
	 */
	UFUNCTION(BlueprintCallable, Category = "ArmorSettings")
	bool SetArmorOfPlateType(UMeshComponent* MeshWithArmor, EArmorPlate PlateType, float NewArmorValue);

	/**
	 * @brief Applies researched armour changes directly to the stored plate data so later hit calculations use them.
	 * @param ArmorPlatesToAdjust Plate types that should receive the multiplier.
	 * @param ArmorValueMultiplier Multiplier applied once to every matching registered plate.
	 */
	void ApplyArmorValueMultiplierToMatchingPlates(
		const TArray<EArmorPlate>& ArmorPlatesToAdjust,
		float ArmorValueMultiplier);

	/**
	 * Calculates the effective armor at the hit location.
	 *
	 * @param WeakHitComponent                The component that was hit.
	 * @param HitLocation                 World-space location of the hit.
	 * @param ProjectileDirection         Normalized direction of the incoming projectile.
	 * @param ImpactNormal                World-space impact normal.
	 * @param OutRawArmorValue            [out] The raw armor value for the impacted armor plate.
	 * @param OutAdjustedArmorPenForAngle [out] The adjusted armor penetration value for the impact angle.
	 * @param OutPlateHit                 [out] The plate hit; a value outside EArmorPlate if no plate resolved.
	 * @return The effective armor value adjusted for the impact angle.
	 */
	float GetEffectiveArmorOnHit(
		TWeakObjectPtr<UPrimitiveComponent> WeakHitComponent,
		const FVector& HitLocation,
		const FVector& ProjectileDirection,
		const FVector& ImpactNormal,
		float& OutRawArmorValue,
		float& OutAdjustedArmorPenForAngle, EArmorPlate& OutPlateHit);

	/**
	 * @brief Same as above but also returns the stable plate registration used to route module damage.
	 * @param OutPlateRegistrationId [out] Mesh slot * plates per mesh + plate index, or INDEX_NONE.
	 */
	float GetEffectiveArmorOnHit(
		TWeakObjectPtr<UPrimitiveComponent> WeakHitComponent,
		const FVector& HitLocation,
		const FVector& ProjectileDirection,
		const FVector& ImpactNormal,
		float& OutRawArmorValue,
		float& OutAdjustedArmorPenForAngle, EArmorPlate& OutPlateHit, int32& OutPlateRegistrationId);

	float GetEffectiveDamageOnHit(
		ERTSDamageType DamageType,
		const float BaseDamage,
		TWeakObjectPtr<UPrimitiveComponent> WeakHitComponent,
		const FVector& HitLocation) const;

	float GetDamageOnArmorPlateResistanceAdjusted(float BaseDamage, const FArmorSettings* SelectedArmorSettings,
	                                                     FDamageMltPerSide ResistanceMultipliers,
	                                                     const FTransform& MeshTransform,
	                                                     const FVector& HitLocation) const;

	// Draw debug boxes for all registered armor plates.
	UFUNCTION(CallInEditor, BlueprintCallable, NotBlueprintable, Category = "ArmorDebug")
	void DebugArmorPlates() const;


	FArmorSetup GetArmorSetup() const
	{
		return M_ArmorSetup;
	}

	// ------------------------------------------------------------------------------------------------
	// Vehicle modules: setup
	// ------------------------------------------------------------------------------------------------

	/** @brief Call before installing modules; also selects the profile's default running gear. */
	UFUNCTION(BlueprintCallable, Category = "ArmorSettings|VehicleModules")
	bool SetVehicleModuleProfile(EVehicleModuleProfile Profile);

	/** @brief Call before installing modules to override the profile's default running gear. */
	UFUNCTION(BlueprintCallable, Category = "ArmorSettings|VehicleModules")
	bool SetRunningGearType(EVehicleRunningGear RunningGear);

	/**
	 * @brief Installs one module instance and assigns its fixed slot as the stable ID.
	 * @param Setup Type and binding; turret/weapon modules need a registered armor mesh.
	 * @param OutModuleId Assigned ID, or INDEX_NONE if installation fails.
	 * @return True if the module was installed.
	 */
	UFUNCTION(BlueprintCallable, Category = "ArmorSettings|VehicleModules")
	bool SetupModule(const FVehicleModuleSetup& Setup, int32& OutModuleId);

	/**
	 * @brief Allows vehicle-specific armor coverage while retaining shared damage rules.
	 * @param ModuleId Installed AddOnArmor module whose complete coverage is replaced.
	 * @param CoveredPlates Registered mesh/plate pairs; empty clears this module's coverage.
	 * @return True if all bindings were valid and the replacement was committed.
	 */
	UFUNCTION(BlueprintCallable, Category = "ArmorSettings|VehicleModules")
	bool SetAddOnArmorPlateCoverage(int32 ModuleId, const TArray<FAddOnArmorPlateBinding>& CoveredPlates);

	/**
	 * @brief Rebinds a turret or weapon module after mesh replacement, preserving its HP and state.
	 * @param ModuleId Installed Turret or Weapon module.
	 * @param NewMesh Newly registered armor mesh that now owns the module's plates.
	 * @return True if the binding was replaced.
	 */
	UFUNCTION(BlueprintCallable, Category = "ArmorSettings|VehicleModules")
	bool RebindModuleMesh(int32 ModuleId, UMeshComponent* NewMesh);

	/**
	 * @brief Binds a weapon module to one gun so a multi-gun mesh only penalizes that gun; preserves HP and state.
	 * @param ModuleId Installed Weapon module.
	 * @param NewMesh Registered armor mesh that owns the gun's plates.
	 * @param NewWeapon Gun the module's behaviours affect.
	 * @return True if the binding was replaced.
	 */
	bool RebindWeaponModule(int32 ModuleId, UMeshComponent* NewMesh, UWeaponState* NewWeapon);

	/** @return True when installed modules, bindings and running gear form a consistent setup. */
	bool ValidateModuleSetup() const;

	/**
	 * @brief Derives module HP from the tank's initialized MaxHealth; the explicit module readiness step.
	 * @param Tank Owning tank that receives committed module batches.
	 * @param TankMaxHealth Valid, positive tank MaxHealth.
	 * @return True if modules are ready for damage and repair.
	 */
	bool FinalizeVehicleModuleSetup(ATankMaster* Tank, float TankMaxHealth);

	/** @brief Preserves every module's health percentage and state when the tank's MaxHealth changes. */
	void RecalculateModuleMaxHealth(float TankMaxHealth);

	UFUNCTION(BlueprintPure, Category = "ArmorSettings|VehicleModules")
	FVehicleModuleSnapshot GetModuleSnapshot(int32 ModuleId) const;

	FVehicleModuleSnapshot GetModuleSnapshotForSlot(int32 SlotIndex) const;

	// ------------------------------------------------------------------------------------------------
	// Vehicle modules: damage
	// ------------------------------------------------------------------------------------------------

	/**
	 * @brief Kinetic default-source entry point; only valid for unambiguous single-instance plate bindings.
	 * @param PlateHit Plate resolved by the existing armor calculation.
	 * @param EffectiveArmor Validated diagnostic value; never used to scale module HP.
	 * @param bPen Existing penetration result; never recalculated here.
	 * @param DamageDealt Actual applied tank-health damage.
	 * @param ProjectileBaseDamage Weapon base damage before reductions.
	 * @param ProjectileCalibre Calibre in millimetres.
	 */
	UFUNCTION(BlueprintCallable, Category = "ArmorSettings|VehicleModules")
	void CalculateModuleDamage(
		EArmorPlate PlateHit,
		float EffectiveArmor,
		bool bPen,
		float DamageDealt,
		float ProjectileBaseDamage,
		float ProjectileCalibre);

	/**
	 * @brief Production entry point used by weapons; the context supplies plate registration, side and shot identity.
	 * @param PlateHit Plate resolved by the existing armor calculation.
	 * @param EffectiveArmor Validated diagnostic value; never used to scale module HP.
	 * @param bPen Existing penetration result; never recalculated here.
	 * @param DamageDealt Actual applied tank-health damage.
	 * @param ProjectileBaseDamage Weapon base damage before reductions.
	 * @param ProjectileCalibre Calibre in millimetres.
	 * @param HitContext Source rules and identity of this impact.
	 */
	void CalculateModuleDamage(
		EArmorPlate PlateHit,
		float EffectiveArmor,
		bool bPen,
		float DamageDealt,
		float ProjectileBaseDamage,
		float ProjectileCalibre,
		const FVehicleModuleHitContext& HitContext);

	/**
	 * @brief Direct mine hit: rolls only the running gear on the side nearest the explosion.
	 * @param ExplosionLocation World location of the mine.
	 * @param BaseDamage Mine damage before reductions.
	 * @param HitContext Shot identity of this detonation.
	 */
	void CalculateMineModuleDamage(const FVector& ExplosionLocation, float BaseDamage,
	                               const FVehicleModuleHitContext& HitContext);

	/**
	 * @brief Splash damage: external modules only, attenuated by the AOE falloff and occlusion.
	 * @param ExplosionLocation World location of the explosion.
	 * @param AttenuatedDamage Splash damage after the AOE falloff for this victim.
	 * @param HitContext Shot identity of this explosion.
	 */
	void CalculateSplashModuleDamage(const FVector& ExplosionLocation, float AttenuatedDamage,
	                                 const FVehicleModuleHitContext& HitContext);

	/** @brief Designer/debug mutation path; never touches hull health. */
	UFUNCTION(BlueprintCallable, Category = "ArmorSettings|VehicleModules")
	void DamageModule(int32 ModuleId, float Damage);

	/** @brief Valid only when exactly one instance of the type is installed. */
	UFUNCTION(BlueprintCallable, Category = "ArmorSettings|VehicleModules")
	void DamageModuleOfType(EVehicleModuleTypes Type, float Damage);

	/** @return A context with a stable victim ID and hull-local position for the provided world hit. */
	FVehicleModuleHitContext MakeHitContextForWorldHit(const FVector& WorldHitLocation) const;

	// ------------------------------------------------------------------------------------------------
	// Vehicle modules: repair milestones (called by the owning tank only)
	// ------------------------------------------------------------------------------------------------
	void RestoreDestroyedModulesToDamaged(EModuleChangeCause Cause);
	void RestoreAllModulesToHealthy();
	bool RestoreDestroyedModuleToDamaged(int32 ModuleId);

	/** @return The next red module for the crew using the constexpr priority, or INDEX_NONE. */
	int32 SelectNextRedModuleForCrewRepair() const;

	/**
	 * @brief Lets an assigned add-on armor behaviour scale the add-on contribution of its covered plates.
	 * @param ModuleId Installed AddOnArmor module.
	 * @param ContributionMultiplier Finite, nonnegative multiplier; 1 restores full add-on protection.
	 */
	void SetAddOnArmorContributionMultiplier(int32 ModuleId, float ContributionMultiplier);

	// ------------------------------------------------------------------------------------------------
	// Vehicle modules: queries
	// ------------------------------------------------------------------------------------------------
	int32 GetInstalledModuleCount() const { return M_InstalledModuleCount; }
	int32 GetDestroyedModuleCount() const { return M_DestroyedModuleCount; }
	int32 GetNonHealthyModuleCount() const { return M_NonHealthyModuleCount; }
	uint32 GetModuleDamageRevision() const { return M_ModuleDamageRevision; }
	bool GetAreModulesFinalized() const { return bM_AreModulesFinalized; }
	EVehicleModuleState GetAggregateModuleState(EVehicleModuleTypes Type) const;
	FModuleIconStates GetModuleIconStates() const;
	int32 GetModuleSlotById(int32 ModuleId) const;
	UMeshComponent* GetBoundMeshForSlot(int32 SlotIndex) const;
	// Returned weak so callers can tell an explicitly unbound gun from a bound gun that was destroyed.
	TWeakObjectPtr<UWeaponState> GetBoundWeaponForSlot(int32 SlotIndex) const;
	bool GetIsMeshRegisteredForArmor(const UMeshComponent* Mesh) const;

	/** @return ID of the installed module of this type bound to the mesh, or INDEX_NONE. */
	int32 FindModuleIdBoundToMesh(EVehicleModuleTypes Type, const UMeshComponent* Mesh) const;

	/** @return ID of an installed mesh-bound module whose mesh was destroyed or unregistered, or INDEX_NONE. */
	int32 FindOrphanedModuleIdOfType(EVehicleModuleTypes Type) const;

	/** @return ID of the first installed module of this type in slot order, or INDEX_NONE. */
	int32 FindFirstInstalledModuleIdOfType(EVehicleModuleTypes Type) const;

	UFUNCTION(BlueprintCallable, Category = "ArmorSettings|VehicleModules")
	FVehicleModuleSaveData ExportModuleState() const;

	/**
	 * @brief Applies saved module health fractions and coverage after finalization; emits no transition events.
	 * @param SaveData Data previously exported for the same profile, gear and installed modules.
	 * @return True if the save matched this tank's module setup and was applied.
	 */
	bool ImportModuleState(const FVehicleModuleSaveData& SaveData);

protected:
	virtual void BeginPlay() override;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FVehicleModuleTestAccess;
#endif

	// Optional non-owning shield link supplied by the tank during component initialization.
	UPROPERTY()
	TWeakObjectPtr<UShieldComponent> M_ShieldComponent;

	// Holds the armor configuration for up to three meshes.
	UPROPERTY()
	FArmorSetup M_ArmorSetup;

	// Multiplier of special damage types per armor plate type.
	UPROPERTY()
	FLaserRadiationDamageMlt M_LaserRadiationDamageMlt;

	// Special cache for rear armor used by mines.
	UPROPERTY()
	float M_RearArmor = 0.f;


	FDamageMltPerSide GetResistanceForDamageType(const ERTSDamageType DamageType) const;
	 float GetDamageFromHitPlate(const FArmorSettings& ArmorPlate,
	                                                  const FDamageMltPerSide ResistanceMultipliers, const float BaseDamage) const;


	// Helper: Calculates the impact angle (in degrees) between the projectile direction and the impact normal.
	float CalculateImpactAngle(const FVector& ProjectileDirection, const FVector& ImpactNormal) const;

	// Helper: Adjusts the given ArmorValue by the impact angle.
	// The OutPenetrationAdjustment (input as a base value) is modified by an exponential decay factor.
	float GetArmorAtAngle(float ArmorValue, float AngleDegrees, float& OutPenetrationAdjustment) const;

	static void ApplyArmorValueMultiplierToArmorSettings(
		FArmorSettings* ArmorSettings,
		int32 NumRegisteredPlates,
		const TArray<EArmorPlate>& ArmorPlatesToAdjust,
		float ArmorValueMultiplier);

	void RefreshRearArmorCache();
	TArrayView<FArmorSettings> GetMutableArmorSettingsForMesh(const UMeshComponent* MeshWithArmor);
	bool TryRefreshRearArmorCacheFromSettings(int32 ArmorMeshSlot);

	// Helper: Identifies which registered mesh corresponds to the hit component.
	bool IdentifyHitMesh(const UPrimitiveComponent* HitComponent, const FArmorSettings*& OutSelectedArmorSettings,
	                     UMeshComponent*& OutRegisteredMesh) const;

	/**
	 * @brief Iterates the registered plates of one mesh to find the one that was hit.
	 * @param ArmorMeshSlot Registration slot of the hit mesh.
	 * @param MeshTransform World transform of the hit mesh.
	 * @param HitLocation World-space location of the hit.
	 * @param ProjectileDirection Direction of the incoming projectile.
	 * @param ImpactNormal World-space impact normal.
	 * @param OutRawArmorValue [out] Armor of the plate hit, including its current add-on contribution.
	 * @param OutAdjustedArmorPenForAngle [out] Penetration adjusted for the impact angle.
	 * @param OutPlateHit [out] Plate type hit.
	 * @param OutPlateIndex [out] Index of the plate within the mesh registration, or INDEX_NONE.
	 * @return The effective armor value adjusted for the impact angle.
	 */
	float EvaluateArmorPlatesForHit(int32 ArmorMeshSlot,
	                                const FTransform& MeshTransform,
	                                const FVector& HitLocation,
	                                const FVector& ProjectileDirection,
	                                const FVector& ImpactNormal,
	                                float& OutRawArmorValue,
	                                float& OutAdjustedArmorPenForAngle, EArmorPlate& OutPlateHit,
	                                int32& OutPlateIndex) const;

	float GetEffectiveArmor(const FVector& HitLocation,
	                        const FVector& ProjectileDirection,
	                        const FVector& ImpactNormal, float RawArmorValue, float& OutAdjustedArmorPenForAngle) const;

	/**
	 * @brief Falls back to the closest registered plate when the hit is outside every armor box.
	 * @param ArmorMeshSlot Registration slot of the hit mesh.
	 * @param MeshTransform World transform of the hit mesh.
	 * @param HitLocation World-space location of the hit.
	 * @param ProjectileDirection Direction of the incoming projectile.
	 * @param ImpactNormal World-space impact normal.
	 * @param OutRawArmorValue [out] Armor of the closest plate.
	 * @param OutAdjustedArmorPenForAngle [out] Penetration adjusted for the impact angle.
	 * @param OutPlateHit [out] Plate type of the closest plate.
	 * @param OutPlateIndex [out] Index of the closest plate, or INDEX_NONE when the mesh has no plates.
	 * @return The effective armor value adjusted for the impact angle.
	 */
	float NoArmorHitGetClosest(int32 ArmorMeshSlot, const FTransform& MeshTransform,
	                           const FVector& HitLocation, const FVector& ProjectileDirection,
	                           const FVector& ImpactNormal, float& OutRawArmorValue,
	                           float& OutAdjustedArmorPenForAngle, EArmorPlate& OutPlateHit,
	                           int32& OutPlateIndex) const;

	bool GetIsRearHullArmor(const EArmorPlate Plate) const;


	/**
     * Sorts the input array of armor settings based on the static armor hierarchy.
     * The highest hierarchy value comes first.
     *
     * @param OutSortedArmorSettings The array to sort.
     */
	void SortArmorArray(TArray<FArmorSettings>& OutSortedArmorSettings) const;
	void Debug_PostSort(TArray<FArmorSettings>& ArmorSettingsSorted) const;

	// ------------------------------------------------------------------------------------------------
	// Armor registration helpers
	// ------------------------------------------------------------------------------------------------
	int32 GetArmorMeshSlot(const UPrimitiveComponent* Component) const;
	UMeshComponent* GetArmorMeshForSlot(int32 ArmorMeshSlot) const;
	const FArmorSettings* GetArmorSettingsForSlot(int32 ArmorMeshSlot) const;
	int32 GetRegisteredPlateCountForSlot(int32 ArmorMeshSlot) const;
	int32 GetRegisteredPlateCount(const FArmorSettings* Settings) const;
	static int32 MakePlateRegistrationId(int32 ArmorMeshSlot, int32 PlateIndex);

	/** @return Structural armor plus the plate's add-on armor scaled by its covering zone's behaviour. */
	float GetPlateArmorValue(int32 ArmorMeshSlot, int32 PlateIndex) const;

	// ------------------------------------------------------------------------------------------------
	// Vehicle module storage
	// ------------------------------------------------------------------------------------------------

	// Fixed module records; never grow during play.
	FVehicleModule M_Modules[VehicleModuleBalance::MaxModuleInstances];

	UPROPERTY()
	FVehicleModuleBinding M_ModuleBindings[VehicleModuleBalance::MaxModuleInstances];

	// Precomputed candidate targets per stable plate registration; rebuilt after setup/registration changes.
	FModulePlateRoute M_PlateRoutes[VehicleModuleBalance::MaxPlateBindings][VehicleModuleBalance::MaxModulesPerPlate];

	// Rule row of each registered plate; INDEX_NONE for unused registrations.
	int8 M_PlateRuleRows[VehicleModuleBalance::MaxPlateBindings];

	// Scales AddOnArmorValue per plate registration; derived from the covering zone's multiplier below.
	float M_PlateAddOnContribution[VehicleModuleBalance::MaxPlateBindings];

	// Add-on contribution multiplier per module slot; only an assigned add-on behaviour changes it.
	float M_AddOnContributionBySlot[VehicleModuleBalance::MaxModuleInstances];

	// Immutable constexpr table selected once for this tank's profile and running gear.
	const VehicleModuleBalance::FPlateModuleRuleSet* M_SelectedProfilePlateRules = nullptr;

	EVehicleModuleProfile M_ModuleProfile = VehicleModuleBalance::DefaultModuleProfile;
	EVehicleRunningGear M_RunningGear = EVehicleRunningGear::Tracks;
	// An explicit SetRunningGearType call overrides the profile default regardless of call order.
	bool bM_HasExplicitRunningGear = false;
	bool bM_HasModuleInstallationStarted = false;
	bool bM_AreModulesFinalized = false;
	float M_TankMaxHealth = 0.f;

	int32 M_InstalledModuleCount = 0;
	int32 M_DestroyedModuleCount = 0;
	int32 M_NonHealthyModuleCount = 0;
	int32 M_DamagedCountByType[VehicleModuleBalance::ModuleTypeCount] = {};
	int32 M_DestroyedCountByType[VehicleModuleBalance::ModuleTypeCount] = {};

	// Incremented on every accepted module damage so the tank can reset unfinished finishing work.
	uint32 M_ModuleDamageRevision = 0;

	// Serial for impacts without weapon-supplied identity; saved so rolls stay deterministic.
	uint32 M_DefaultImpactSerial = 0;

	// Transitions of the mutation in progress; copied before dispatch.
	FModuleChangeBatch M_PendingModuleChanges;

	// Rejects coverage edits and nested damage while a mutation or its dispatch is running.
	bool bM_IsMutatingModules = false;
	bool bM_IsDispatchingModuleChanges = false;

	UPROPERTY()
	TWeakObjectPtr<ATankMaster> M_ModuleTank;

	bool GetIsValidModuleTank() const;

	// ------------------------------------------------------------------------------------------------
	// Vehicle module setup helpers
	// ------------------------------------------------------------------------------------------------
	bool GetCanChangeModuleConfiguration(const TCHAR* FunctionName) const;
	bool GetIsValidModuleSetupInput(const FVehicleModuleSetup& Setup) const;
	int32 FindFreeSlotForSetup(const FVehicleModuleSetup& Setup) const;
	void InitializeModuleSlot(int32 SlotIndex, const FVehicleModuleSetup& Setup, int32 ArmorMeshSlot);
	bool TryResolveCoverageMask(const TArray<FAddOnArmorPlateBinding>& CoveredPlates, uint64& OutCoverageMask) const;
	bool GetIsCoverageOwnedByOtherModule(int32 SlotIndex, uint64 CoverageMask) const;
	uint64 GetCoverageMaskForPlateType(int32 ArmorMeshSlot, EArmorPlate PlateType) const;
	void ApplyAddOnContributionForSlot(int32 SlotIndex);
	int32 FindFirstRedModuleOfType(EVehicleModuleTypes Type) const;
	void RebuildPlateModuleBindings();
	void RefreshBoundMeshSlots();
	void DisableAddOnCoverageForClearedRegistration();
	void RebuildRoutesForPlate(int32 PlateRegistrationId, int32 RuleRow);
	FModulePlateRoute MakeRouteForCandidate(int32 PlateRegistrationId, int32 RuleRow, int32 CandidateIndex) const;
	FModulePlateRoute MakeRunningGearRoute(EArmorPlate Plate, EVehicleModuleTypes RunningGearType) const;
	int32 FindBoundSlotOfType(EVehicleModuleTypes Type, int32 ArmorMeshSlot) const;
	int32 FindSingletonSlotOfType(EVehicleModuleTypes Type) const;
	int32 FindCoveringAddOnSlot(int32 PlateRegistrationId) const;
	void RefreshArmorContributionsAndRearCache();
	float GetModuleMaxHpForType(EVehicleModuleTypes Type, float TankMaxHealth) const;

	// ------------------------------------------------------------------------------------------------
	// Vehicle module damage pipeline (fixed storage only; no heap allocation)
	// ------------------------------------------------------------------------------------------------
	FModuleChangeBatch ResolveModuleDamage(const FModuleDamageInput& Input);
	bool ValidateImpactInputsAndLivingOwner(const FModuleDamageInput& Input) const;
	float CalculateModuleDamageEnergy(const FModuleDamageInput& Input) const;
	void TryAppendCandidateDamage(const FPlateModuleDamage& Rule, int32 CandidateIndex, float Energy,
	                              const FModuleDamageInput& Input, FModuleDamageBatch& Batch) const;
	int32 ResolveModuleSlot(const FModulePlateRoute& Route, const FModuleDamageInput& Input) const;
	bool GetIsCandidateEligibleForSource(const FPlateModuleDamage& Rule, const FModuleDamageInput& Input) const;
	float CalculateCandidateProbability(const FPlateModuleDamage& Rule, const FModuleDamageInput& Input) const;
	static float GetDeterministicModuleRoll(uint32 ShotKey, int32 CandidateIndex);
	static uint32 MakeShotKey(const FVehicleModuleHitContext& HitContext);
	static void LimitBatchToEnergyBudget(float Energy, FModuleDamageBatch& Batch);
	void ApplyPerModuleDamageCaps(bool bPen, FModuleDamageBatch& Batch) const;
	void ApplyNonPenRateBudgetIfNeeded(const FModuleDamageInput& Input, FModuleDamageBatch& Batch);
	float ConsumeNonPenBudget(FVehicleModuleNonPenBudget& Budget, float MaxHp, float RequestedDamage) const;
	void LimitNewDestroyedModules(FModuleDamageBatch& Batch) const;
	int32 FindGreatestNormalizedFailure(const FModuleDamageBatch& Batch,
	                                    const bool (&bAllowedToDestroy)[VehicleModuleBalance::MaxModulesPerPlate]) const;
	bool GetWouldEntryDestroyModule(const FModuleDamageBatch::FEntry& Entry) const;
	FModuleChangeBatch CommitModuleDamageBatch(const FModuleDamageBatch& Batch,
	                                           EWeaponShellType DamageShellType);
	bool TryMakeDefaultModuleHitContext(EArmorPlate PlateHit, FVehicleModuleHitContext& OutHitContext);
	int32 FindUniquePlateRegistration(EArmorPlate PlateHit) const;
	int32 FindPlateRegistrationFacingLocation(const FVector& WorldLocation) const;
	int32 FindRunningGearSlotNearest(const FVector& WorldLocation) const;
	bool GetIsSplashOccluded(const FVector& ExplosionLocation, const FVector& TargetLocation) const;

	// ------------------------------------------------------------------------------------------------
	// Vehicle module state mutation and dispatch
	// ------------------------------------------------------------------------------------------------
	void SetModuleHealth(int32 SlotIndex, float NewHp, EModuleChangeCause Cause,
	                     EWeaponShellType DamageShellType);
	void UpdateModuleStateCounts(EVehicleModuleTypes Type, EVehicleModuleState PreviousState,
	                             EVehicleModuleState NewState);
	FModuleChangeBatch TakePendingModuleChanges();
	void DispatchModuleChangesAfterMutation(const FModuleChangeBatch& Changes);
	void ShowModuleStateChangePopups(const FModuleChangeBatch& Changes) const;
	void ShowModuleDamagedPopup(const ATankMaster& Tank, EVehicleModuleTypes Type) const;
	void ShowModuleDestroyedPopup(const ATankMaster& Tank, EVehicleModuleTypes Type) const;
	void AppendSavedCoverageForSlot(int32 SlotIndex, FVehicleModuleSaveData& OutSaveData) const;
	void ApplyImportedModuleHealth(const FVehicleModuleSaveData& SaveData);
	bool GetIsValidSaveData(const FVehicleModuleSaveData& SaveData) const;
	int32 GetSlotForSavedModuleId(const FVehicleModuleSaveData& SaveData, int32 SavedModuleId) const;
	bool TryApplySavedCoverage(const FVehicleModuleSaveData& SaveData);
};
