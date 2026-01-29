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

#include "HealthComponent.generated.h"

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

	/** Callback tracker for when the widget component is created and ready. */
	FHealthBarWidgetCallbacks M_HealthBarWidgetCallbacks;

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
	inline UWidgetComponent* GetHealthBarWidgetComp() const;

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

	/** @return Whether the unit was healed to full health or not */
	virtual bool Heal(const float HealAmount);

	/** @return Whether the current health needs repairs. */
	bool GetHasDamageToRepair() const;

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
	void InitHealthAndResistance(const FResistanceAndDamageReductionData& ResistanceData,
	                             const float NewMaxHp);

	void ChangeVisibilitySettings(const FHealthBarVisibilitySettings& NewSettings);
	FHealthBarVisibilitySettings GetVisibilitySettings() const;

	virtual void OnOverwiteHealthbarVisiblityPlayer(ERTSPlayerHealthBarVisibilityStrategy Strategy);
	virtual void OnOverwiteHealthbarVisiblityEnemy(ERTSEnemyHealthBarVisibilityStrategy Strategy);

	// Not null checked; may still be loading; if so overwrite OnWigetInitialized.
	UW_HealthBar* GetHealthBarWidget() const;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

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

	void OnUnitHovered() const;
	void OnUnitUnhovered() const;
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
