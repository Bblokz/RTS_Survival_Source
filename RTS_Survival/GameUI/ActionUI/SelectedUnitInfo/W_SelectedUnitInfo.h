// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Border.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/TargetPreference/TargetPreference.h"
#include "RTS_Survival/RTSComponents/RTSTargetAcquisition/RTSEngagementStance/RTSAggroBehaviour.h"
#include "W_SelectedUnitInfo.generated.h"

enum class ERTSAggroBehaviour : uint8;
enum class ETargetPreference : uint8;
enum class EBuildingExpansionType : uint8;
class UActionUIManager;
enum class EVeterancyIconSet : uint8;
enum class ESquadSubtype : uint8;
enum class ETankSubtype : uint8;
enum class ENomadicSubtype : uint8;
class UW_SelectedUnitDescription;
enum class EAllUnitType : uint8;
class UImage;
class UButton;
class ASquadController;

/**
 * @brief Icons the stance and target preference buttons show for the primary selected unit's current mode.
 * Soft references so the widget only loads them once a unit is selected.
 */
USTRUCT(BlueprintType)
struct FSelectedUnitInfoModeIcons
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, meta=(AllowedClasses="/Script/Engine.Texture2D,/Script/Engine.MaterialInterface"))
	TSoftObjectPtr<UObject> StanceAggressive = TSoftObjectPtr<UObject>(FSoftObjectPath(
		TEXT("/Game/RTS_Survival/Textures/GameUI/TargetPrefStance/T_UI_AggressiveStance.T_UI_AggressiveStance")));

	UPROPERTY(EditDefaultsOnly, meta=(AllowedClasses="/Script/Engine.Texture2D,/Script/Engine.MaterialInterface"))
	TSoftObjectPtr<UObject> StanceHoldPosition = TSoftObjectPtr<UObject>(FSoftObjectPath(
		TEXT("/Game/RTS_Survival/Textures/GameUI/TargetPrefStance/T_UI_HoldPosition.T_UI_HoldPosition")));

	UPROPERTY(EditDefaultsOnly, meta=(AllowedClasses="/Script/Engine.Texture2D,/Script/Engine.MaterialInterface"))
	TSoftObjectPtr<UObject> PreferInfantry = TSoftObjectPtr<UObject>(FSoftObjectPath(
		TEXT("/Game/RTS_Survival/Textures/GameUI/TargetPrefStance/T_UI_TargetPreference_Infantry.T_UI_TargetPreference_Infantry")));

	UPROPERTY(EditDefaultsOnly, meta=(AllowedClasses="/Script/Engine.Texture2D,/Script/Engine.MaterialInterface"))
	TSoftObjectPtr<UObject> PreferVehicles = TSoftObjectPtr<UObject>(FSoftObjectPath(
		TEXT("/Game/RTS_Survival/Textures/GameUI/TargetPrefStance/T_UI_TargetPreference_Vehicles.T_UI_TargetPreference_Vehicles")));

	UPROPERTY(EditDefaultsOnly, meta=(AllowedClasses="/Script/Engine.Texture2D,/Script/Engine.MaterialInterface"))
	TSoftObjectPtr<UObject> PreferStructures = TSoftObjectPtr<UObject>(FSoftObjectPath(
		TEXT("/Game/RTS_Survival/Textures/GameUI/TargetPrefStance/T_UI_TargetPreference_Structures.T_UI_TargetPreference_Structures")));

	// Anti-air weapons are locked on aircraft; this icon tells the player the preference is fixed.
	UPROPERTY(EditDefaultsOnly, meta=(AllowedClasses="/Script/Engine.Texture2D,/Script/Engine.MaterialInterface"))
	TSoftObjectPtr<UObject> AntiAir = TSoftObjectPtr<UObject>(FSoftObjectPath(
		TEXT("/Game/RTS_Survival/Textures/GameUI/UnitIcons/Tactical/T_AntiAirClass_Icon_V3_NoStar.T_AntiAirClass_Icon_V3_NoStar")));
};

/**
 * @brief Selected-unit panel: portrait, health, experience and the stance / target preference buttons.
 * The buttons cycle the mode of the whole selection; their tooltip explains the primary unit's current mode.
 * @note OnClickedAggroStanceButton: call from the ButtonStance OnClicked event in blueprint.
 * @note OnClickedTargetPreferenceButton: call from the ButtonTargetPreference OnClicked event in blueprint.
 */
UCLASS()
class RTS_SURVIVAL_API UW_SelectedUnitInfo : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintImplementableEvent, Category="SetUp")
	void SetupUnitInfoForNewUnit(
		const EAllUnitType UnitType,
		const float HealthPercentage,
		const float MaxHp,
		const float CurrentHp,
		const ENomadicSubtype NomadicSubtype,
		const ETankSubtype TankSubtype, const ESquadSubtype SquadSubtype, const EBuildingExpansionType BxpSubtype);
	void SetupTargetPrefAndAgroStanceForNewUnit(AActor* SelectedActor);

	void InitSelectedUnitInfo(UW_SelectedUnitDescription* UnitDesc, UActionUIManager* ActionUIManager);

	void SetupUnitDescriptionForNewUnit(const AActor* SelectedActor,
	                                    const EAllUnitType PrimaryUnitType,
	                                    const ENomadicSubtype NomadicSubtype,
	                                    const ETankSubtype TankSubtype,
	                                    const ESquadSubtype SquadSubtype, const EBuildingExpansionType BxpSubtype) const;

	/**
	 * @brief Only updates the health bar in the UI.
	 * @param HealthPercentage The percentage of the health of the unit.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="UpdateHealth")
	void UpdateHealthBar(const float HealthPercentage, const float MaxHp, const float CurrentHp);

	/**
    	 * Blueprint event to update the experience bar.
    	 * @param ExperiencePercentage The percentage progress toward the next level.
    	 * @param CumulativeExp The cumulative experience the unit has earned.
    	 * @param ExpNeededForNextLevel The additional experience needed to reach the next level.
    	 * @param CurrentLevel
    	 * @param MaxLevel
    	 * @param VeterancyIconSet
    	 */
	UFUNCTION(BlueprintImplementableEvent, Category="UpdateExperience")
	void UpdateExperienceBar(const float ExperiencePercentage, const int32 CumulativeExp,
	                         const int32 ExpNeededForNextLevel, const int32 CurrentLevel, const int32 MaxLevel,
	                         const EVeterancyIconSet
	                         VeterancyIconSet);

protected:
	// Set by main menu.
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UW_SelectedUnitDescription> UnitDescription;

	// The image of this widget that is a child of the button.
	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	UImage* M_UnitInfoImage;

	UPROPERTY(meta = (BindWidget), BlueprintReadOnly)
	TObjectPtr<UBorder> UnitInfoBorder;

	// Cycles Aggressive / Hold Position; its first child image shows the current stance.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly)
	TObjectPtr<UButton> ButtonStance;

	// Cycles Infantry / Vehicles / Structures; its first child image shows the current preference.
	UPROPERTY(meta = (BindWidgetOptional), BlueprintReadOnly)
	TObjectPtr<UButton> ButtonTargetPreference;

	UPROPERTY(EditDefaultsOnly, Category="Stance and Target Preference")
	FSelectedUnitInfoModeIcons ModeIcons;

    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	
	void UpdateTargetPreference(const ETargetPreference TargetPreference); 
	void UpdateAggroStance(const ERTSAggroBehaviour CurrentAggroStance);
	UFUNCTION(BlueprintImplementableEvent)
	void BP_UpdateTargetPreference(const ETargetPreference TargetPreference);
	UFUNCTION(BlueprintImplementableEvent)
	void BP_UpdateAgroStance(const ERTSAggroBehaviour AggroStance);
	UFUNCTION(blueprintCallable, NotBlueprintable) 
	void OnClickedAggroStanceButton();
	UFUNCTION(blueprintCallable, NotBlueprintable) 
	void OnClickedTargetPreferenceButton();
private:
	
	bool EnsureIsValidUnitDescription() const;

	// The texture (or material) of the portrait brush the Blueprint set for the selected unit.
	const UObject* GetPortraitResource() const;
	
	static ETargetPreference GetSquadOrTeamWeaponTargetPreference(ASquadController* SquadController);

	ERTSAggroBehaviour RotateAggroStance(const ERTSAggroBehaviour CurrentAggroStance) const;
	ETargetPreference RotateTargetPreference(const ETargetPreference TargetPreference) const;

	void RefreshStanceButton(const ERTSAggroBehaviour CurrentAggroStance) const;
	void RefreshTargetPreferenceButton(const ETargetPreference TargetPreference) const;

	FText GetStanceTooltip(const ERTSAggroBehaviour CurrentAggroStance) const;
	FText GetTargetPreferenceTooltip(const ETargetPreference TargetPreference) const;

	const TSoftObjectPtr<UObject>& GetStanceIcon(const ERTSAggroBehaviour CurrentAggroStance) const;
	const TSoftObjectPtr<UObject>& GetTargetPreferenceIcon(const ETargetPreference TargetPreference) const;

	/** @brief Puts the icon on the button's image; an empty icon hides the image so no stale mode shows. */
	static void SetButtonIcon(const UButton* Button, const TSoftObjectPtr<UObject>& Icon);

	// True while the cursor is on a mode button, so the unit description does not cover its tooltip.
	bool GetIsHoveringModeButton() const;

	UPROPERTY()
	TWeakObjectPtr<UActionUIManager> M_ActionUIManager;
	
	ETargetPreference LastTargetPreference = ETargetPreference::Infantry;
	ERTSAggroBehaviour LastEngagedStance = ERTSAggroBehaviour::Stance_HoldPosition;

	bool bM_IsInitialized = false;

	bool EnsureIsValidActionUIManager()const;
	
};
