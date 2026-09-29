// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "HealthComponent_FireResState.h"
#include "Camera/CameraComponent.h"
#include "Components/ActorComponent.h"
#include "DamageReduction/DamageReduction.h"
#include "HealthBarWidgetCallBacks/HealthBarWidgetCallbacks.h"
#include "HealthInterface/HealthBarIcons/HealthBarIcons.h"
#include "RTS_Survival/Game/GameState/HideGameUI/RTSUIElement.h"
#include "RTS_Survival/Game/UserSettings/GameplaySettings/HealthbarVisibilityStrategy/HealthBarVisibilityStrategy.h"
#include "RTS_Survival/GameUI/Healthbar/HealthBarSettings/HealthBarVisibilitySettings.h"
#include "RTS_Survival/Weapons/WeaponData/RTSDamageTypes/RTSDamageTypes.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponShellType/WeaponShellType.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/VehicleModules/VehicleModuleBatches.h"

#include "HealthComponent.generated.h"

enum class EVeterancyIconSet : uint8;
struct FResistanceAndDamageReductionData;
class UWeaponState;
enum class EWeaponShellType : uint8;
class USelectionComponent;
class UWidgetComponent;
class UW_HealthBar;
class URTSComponent;
struct FDamageReductionSettings;
class UActionUIManager;
class IHealthBarOwner;
enum class EHealthLevel : uint8;
class ACameraPawn;
class UProgressBar;
class ATankMaster;
class UHorizontalBox;
class UVehicleModuleSubsystem;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnHealthBarVisibilityChanged, ESlateVisibility);
// Old max health, new max health; broadcast only when SetMaxHealth actually changes the value.
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMaxHealthChanged, float, float);

/** @brief Stack receipt of one accepted healing transaction, sent to a tank with pending module repairs. */
struct FHealthHealingReceipt
{
	// The accepted request, including healing that found no missing hull HP.
	float AcceptedHealingWork = 0.f;
	float AppliedHullHealing = 0.f;
	float HealthAfter = 0.f;
	float MaxHealth = 0.f;
};

/** @brief Desired and displayed module icon state per type for tank healthbars; hidden bars only update desire. */
USTRUCT()
struct FHealthComponentModuleIconState
{
	GENERATED_BODY()

	EVehicleModuleState DesiredStates[VehicleModuleBalance::ModuleTypeCount] = {};
	EVehicleModuleState DisplayedStates[VehicleModuleBalance::ModuleTypeCount] = {};

	// Types whose displayed icon differs from the desired one.
	uint32 DirtyTypeMask = 0;

	// Enabled once for tank owners on clients; squads and buildings never need a ModuleBox.
	bool bIsPresentationEnabled = false;

	// Suspends icon writes after a failed ModuleBox lookup until the widget is reinitialized.
	bool bIsModuleBoxUnavailable = false;

	// Any non-healthy module type counts as damage for the bDisplayOnDamaged policy.
	bool bHasAnyNonHealthyModule = false;
};

USTRUCT()
struct FHealthComponentSelectionDelegateHandles
{
	GENERATED_BODY()

	FDelegateHandle M_OnUnitHoveredHandle;
	FDelegateHandle M_OnUnitUnhoveredHandle;
	FDelegateHandle M_OnUnitSelectedHandle;
	FDelegateHandle M_OnUnitDeselectedHandle;
};

/**
 * @brief Container for Health and primitive armor values.
 * @note SET IN BP
 * @note SetMaxHealth, SetArmor
 * @note The target type icon is set in the owning actor defaults.
 * @note the shell type is only updated if the unit owning the component is owned by player 1.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), BlueprintType)
class RTS_SURVIVAL_API UHealthComponent : public UActorComponent, public IRTSUIElement
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UHealthComponent();

	void OnUnitHovered() const;
	void OnUnitUnhovered() const;

	/** Callback tracker for when the widget component is created and ready. */
	FHealthBarWidgetCallbacks M_HealthBarWidgetCallbacks;

	// If the derived BP of this healthbar supports a rank icon then this will update it.
	void UpdateRankIcon(const int32 NewRankLevel, const EVeterancyIconSet VeterancyIconSet) const;

	/** Permanently prevents this component's world-space health widget from being rendered again. */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void MakeHealthBarInvisible() const;

	virtual void OnHideAllGameUI(const bool bHide) override;
	void HideHealthBar();

	UFUNCTION(BlueprintCallable, NotBlueprintable, Category="Debugging")
	void DebugHealthComponentAtLocation(const FVector Location) const;

	/**
	 * @brief Updates whether this health component is the primary selected component that updates the UI.
	 * @param bSetSelected Whether this hp component is the primary selected component that updates the UI.
	 * @param ActionUIManager The action UI manager that will be used to update the UI, this is only set when bIsSelected is true.
	 */
	void SetHealthBarSelected(const bool bSetSelected, TObjectPtr<UActionUIManager> ActionUIManager);


	FVector GetLocalLocation() const;
	void SetLocalLocation(const FVector& NewLocation) const;
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	inline float GetHealthPercentage() const { return CurrentHealth / MaxHealth; };
	UWidgetComponent* GetHealthBarWidgetComp() const;

	void ChangeTargetIconType(const ETargetTypeIcon NewIconType);


	/**
	 * @brief Subtracts the damage from CurrentHealth.
	 * @param InOutDamage: Will contain the amount of actual damage dealt.
	 * @return Whether The unit died.
	 */
	virtual bool TakeDamage(float& InOutDamage, const ERTSDamageType DamageType);

	UFUNCTION(BlueprintCallable)
	inline float GetCurrentHealth() const { return CurrentHealth; };

	/**
	 * @brief Sets the CurrentHealth to the provided value.
	 * @param NewCurrentHealth The value currentHealth will be adjusted to.
	 * @note If NewCurrentHealth is higher than MaxHealth, CurrentHealth will be set to max health.
	 */
	UFUNCTION(BlueprintCallable)
	virtual void SetCurrentHealth(const float NewCurrentHealth);

	/**
	 * @brief The single entry point of every healing source (repairs, auras, heal behaviours).
	 * For tanks with vehicle modules, accepted healing also drives module recovery and finishing work.
	 * @param HealAmount Healing work to apply.
	 * @return Whether the unit is fully repaired; for module tanks this means hull and every module.
	 */
	virtual bool Heal(const float HealAmount);

	/** @return Whether a living owner still needs repairs, including damaged vehicle modules of a tank. */
	bool GetHasDamageToRepair() const;

	/** @brief Called once by a tank after module finalization; enables module-aware healing receipts. */
	void InitializeTankRepairOwner(ATankMaster* Tank);

	/** @brief Mirrors the tank's non-healthy module count so repair eligibility needs no tank queries. */
	void SetTankPendingModuleRepairCount(int32 NonHealthyModuleCount);

	FOnMaxHealthChanged& GetOnMaxHealthChanged() { return M_OnMaxHealthChanged; }

	/** @brief Enables the tank-only module icons in the healthbar's ModuleBox; rejects non-tank owners. */
	void InitializeTankModulePresentation(ATankMaster* Tank);

	/** @brief Stores the new desired icon states; the widget updates now if visible, otherwise on reveal. */
	void ApplyModuleIconStateChanges(const FModuleIconDeltaBatch& Changes);

	/** @brief Replaces every desired icon state, e.g. on presentation initialization or load. */
	void SynchronizeModuleIconSnapshot(const FModuleIconStates& Snapshot);

	UFUNCTION(BlueprintCallable)
	inline float GetMaxHealth() const { return MaxHealth; };

	/**
	 * @brief Sets the maximum health of the unit to the provided value and adjusts current health
	 * in percentage of the previous max health. If NewMaxHealth > CurrentHealth, then CurrentHealth will
	 * be set to the new MaxHealth value.
	 * @param NewMaxHealth Max health value for the unit.
	 */
	UFUNCTION(BlueprintCallable)
	virtual void SetMaxHealth(const float NewMaxHealth);
	
	UFUNCTION(BlueprintCallable)
	void AddHealth(const float HealthToAdd, const float DamageReductionToAdd = 0);
	

	UFUNCTION(BlueprintCallable)
	void InitHealthAndResistance(const FResistanceAndDamageReductionData& ResistanceData,
	                             const float NewMaxHp);

	void ChangeVisibilitySettings(const FHealthBarVisibilitySettings& NewSettings);
	FHealthBarVisibilitySettings GetVisibilitySettings() const;
	void ChangeCustomizationSettings(const FHealthBarCustomization& NewSettings);
	FHealthBarCustomization GetCustomizationSettings() const;

	virtual void OnOverwiteHealthbarVisiblityPlayer(ERTSPlayerHealthBarVisibilityStrategy Strategy);
	virtual void OnOverwiteHealthbarVisiblityEnemy(ERTSEnemyHealthBarVisibilityStrategy Strategy);

	// Not null checked; may still be loading; if so overwrite OnWigetInitialized.
	UW_HealthBar* GetHealthBarWidget() const;

	/** @return The actual current health widget visibility, or Hidden before widget creation. */
	ESlateVisibility GetHealthBarVisibility() const;

	/** Broadcasts every actual health-widget visibility update for dependent UI such as shield bars. */
	FOnHealthBarVisibilityChanged& GetOnHealthBarVisibilityChanged()
	{
		return M_OnHealthBarVisibilityChanged;
	}

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void BeginPlay_ApplyUserSettingsHealthBarVisibility();

	// Leave this empty if we do not want to use a health widget on this actor.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HealthWidget")
	TSubclassOf<UW_HealthBar> HealthBarWidgetClass = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HealthWidget|Render")
	FVector2D WidgetXYScales = FVector2D(1.0f, 1.0f);

	// Offset location added to actor location to position the health bar.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HealthWidget|Render")
	FVector RelativeWidgetOffset = FVector(0.0f, 0.0f, 0.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HealthWidget|Render")
	FHealthBarVisibilitySettings VisibilitySettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "HealthWidget|Render")
	FHealthBarCustomization CustomizationSettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	FDamageReductionSettings DamageReductionSettings;


	// Amount of health remaining.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float CurrentHealth;

	// Max amount of health for the unit.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float MaxHealth;

	// When the health reaches below one of the percentages for the first time
	// we notify the owner of the health component.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	TArray<EHealthLevel> HealthLevelsToNotifyOn;

	virtual void OnWidgetInitialized();
	

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FVehicleModuleTestAccess;
#endif

	// ---- Healing transaction ----
	bool ApplyHealingInternal(float HealAmount);
	bool ApplyTankHealingTransaction(float HealAmount);
	// Existing hull-only healing for owners without vehicle modules.
	bool ApplyHullOnlyHealing(float HealAmount);
	bool GetIsFullyRepairedLivingOwner() const;

	// Set once a tank finalized its modules; receipts are only sent while module repairs are pending.
	UPROPERTY()
	TWeakObjectPtr<ATankMaster> M_TankRepairOwner;

	int32 M_TankNonHealthyModuleCount = 0;

	// Re-entrant healing is summed here and applied after the current transaction and its notifications.
	bool bM_IsApplyingHealingTransaction = false;
	float M_DeferredHealingWork = 0.f;

	FOnMaxHealthChanged M_OnMaxHealthChanged;

	// ---- Tank module icons ----
	void Widget_OnModulePresentationReady();
	bool Widget_FindAndCacheModuleBox();
	bool Widget_GetIsValidModuleBox() const;
	void Widget_ReleaseModuleIcons() const;
	void Widget_FlushModuleIconChanges() const;
	bool Widget_FlushModuleIconType(int32 TypeIndex, bool& bOutMembershipChanged) const;
	bool Widget_AddModuleIcon(EVehicleModuleTypes Type, EVehicleModuleState State) const;
	bool Widget_ChangeModuleIcon(EVehicleModuleTypes Type, EVehicleModuleState State) const;
	void Widget_RebuildModuleIconChildren() const;
	bool GetShouldFlushModuleIcons() const;
	bool GetIsValidVehicleModuleSubsystem() const;
	void SetDesiredModuleIconState(int32 TypeIndex, EVehicleModuleState State);
	void OnDesiredModuleIconsChanged();
	static bool GetIsVisibleSlateVisibility(ESlateVisibility Visibility);

	// Mutable: visibility changes arrive through const paths but must flush pending icon state.
	UPROPERTY()
	mutable FHealthComponentModuleIconState M_ModuleIconState;

	// Looked up once per widget instance; never on damage or repair ticks.
	UPROPERTY()
	mutable TWeakObjectPtr<UHorizontalBox> M_ModuleBox;

	UPROPERTY()
	TWeakObjectPtr<UVehicleModuleSubsystem> M_VehicleModuleSubsystem;

	void Widget_CreateHealthBar();

	bool CanTolerateFireDamage(const float Damage);

	void InitFireThresholdData(const float NewMaxFireThreshold, const float NewFireRecovery);
	void InitTargetTypeIconData(const ETargetTypeIcon NewIconType);
	void InitDamageReductionSettings(const FDamageReductionSettings& NewSettings);


	// Note that this value gets read in from the unit data at begin play.
	// Set by health component with
	UPROPERTY()
	ETargetTypeIcon M_TargetTypeIcon = ETargetTypeIcon::None;

	UPROPERTY()
	FHealthComp_FireThresholdState M_FireThresholdState;

	UPROPERTY()
	TWeakObjectPtr<UW_HealthBar> M_HealthBarWidget;
	bool Widget_GetIsValidHealthBarWidget() const;

	TWeakObjectPtr<UWidgetComponent> M_OwnerHpWidgetComp;
	bool Widget_GetIsValidWidgetComponent() const;
	bool Widget_GetIsValidWidgetClass() const;
	void Widget_OnFailedToCreateWidgetComponent() const;
	void SetHealthBarVisibility(ESlateVisibility NewVisibility) const;


	/** @brief Updates the visuals for the heathBar to correctly represent the amount of health that is left. */
	UFUNCTION()
	void UpdateHealthBar();

	void UpdateVisibilityOnHealthChange(const float NewPercentage) const;

	bool ShouldDisplayHealthForPercentage(const float NewPercentage) const;

	// Updated notification function that now handles overshooting of notify levels.
	FORCEINLINE void NotifyHealthLevelChange(const float Percentage);

	UPROPERTY()
	TObjectPtr<AActor> M_OwnerActor;

	EHealthLevel M_HealthLevel;

	FTimerHandle M_FireRecoveryTimerHandle;
	void StartFireRecoveryIfNeeded();
	void OnFireRecoveryTick();

	// EWeaponShellType M_ShellTypeToDisplay = EWeaponShellType::Shell_None;

	void BeginPlay_CheckDamageReductionSettings();

	/**
	 * @brief Store a weak ptr to the RTS component to Notify on when the unit gets damaged so it is regisered as the unit
	 * being in combat.
	 */
	void BeginPlay_SetupAssociatedRTSComponent();

	/**
	 * @brief Store a weak ptr to the selection component to determine whether the health bar should be displayed.
	 */
	void BeginPlay_BindHoverToSelectionComponent();

	void UpdateSelectionComponentBindings();
	void ClearSelectionComponentBindings();
	void UpdateVisibilityAfterSettingsChange();
	void RestoreUnitDefaultHealthBarVisibilitySettings();

	// The RTS Component of the same owner that needs to know whether the unit is in combat due to being damaged.
	TWeakObjectPtr<URTSComponent> M_RTSComponent;

	void RegisterCallBackForUnitName(URTSComponent* RTSComponent);

	// The selection component used to determine whether the HealthBar should be displayed.
	TWeakObjectPtr<USelectionComponent> M_SelectionComponent;

	mutable FOnHealthBarVisibilityChanged M_OnHealthBarVisibilityChanged;

	void OnUnitSelected() const;
	void OnUnitDeselected() const;

	void OnUnitInCombat() const;

	UPROPERTY()
	TObjectPtr<UActionUIManager> M_ActionUIManager;

	// For the provided percentage health get the closest health level rounded up: 100-76 -> 100, 75-67->75, 66-51->66 etc. 
	static TMap<int32, EHealthLevel> M_PercentageToHealthLevel;

	// Provides the numeric value of the threshold
	static TMap<EHealthLevel, int32> M_HealthLevelToThresholdValue;

	TScriptInterface<IHealthBarOwner> M_IHealthOwner;

	inline bool GetIsValidHeathBarOwner() const;

	static void InitializeHealthLevelMap();

	UPROPERTY()
	bool bWasHiddenByAllGameUI = false;

	// Defers visibility changes until the widget has finished initializing.
	UPROPERTY()
	bool bM_ShouldApplyVisibilitySettingsOnWidgetInit = false;

	UPROPERTY()
	bool bM_IsHealthBarWidgetInitialized = false;

	// Remains set after permanent removal so delayed visibility updates cannot recreate or reveal the widget.
	UPROPERTY(Transient)
	mutable bool bM_IsHealthBarPermanentlyInvisible = false;

	UPROPERTY()
	FHealthBarVisibilitySettings M_UnitDefaultHealthBarVisibilitySettings;

	FHealthComponentSelectionDelegateHandles M_SelectionDelegateHandles;

	/**
	 * @brief Helper function that maps an EHealthLevel to its numeric threshold value.
	 * For example, Level_100Percent returns 100, Level_75Percent returns 75, etc.
	 */
	int32 GetThresholdValue(EHealthLevel Level) const;

	EHealthLevel CalculateCurrentComputedLevel(const float Percentage) const;
	bool IsHealthDecreasing(const EHealthLevel NewComputedLevel) const;
	bool FindOvershotNotifyLevel(const EHealthLevel PreviousLevel, const EHealthLevel NewComputedLevel,
	                             EHealthLevel& OutOvershotLevel) const;
	void NotifyOwner(const EHealthLevel NewLevel, const bool bIsHealing);

	void Debug(const FString& Message, const FColor& Color) const;

	/** Applies final visibility considering global-all-UI hide and health-based policy. */
	void ApplyHealthBarVisibilityPolicy(const float CurrentPct) const;
};
