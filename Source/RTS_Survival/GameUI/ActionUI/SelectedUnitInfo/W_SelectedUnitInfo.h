// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Border.h"
#include "W_SelectedUnitInfo.generated.h"

enum class EBuildingExpansionType : uint8;
class UActionUIManager;
enum class EVeterancyIconSet : uint8;
enum class ESquadSubtype : uint8;
enum class ETankSubtype : uint8;
enum class ENomadicSubtype : uint8;
class UW_SelectedUnitDescription;
enum class EAllUnitType : uint8;
class UImage;
/**
 * 
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

    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
private:
	bool EnsureIsValidUnitDescription() const;

	UPROPERTY()
	TWeakObjectPtr<UActionUIManager> M_ActionUIManager;

	bool bM_IsInitialized = false;

	bool EnsureIsValidActionUIManager()const;
	
};
