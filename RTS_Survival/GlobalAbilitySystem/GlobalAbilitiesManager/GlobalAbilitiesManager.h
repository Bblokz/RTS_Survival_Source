// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"
#include "GlobalAbilitiesManager.generated.h"


class UW_GA_Description;
class UGameUnitManager;
struct FTrainingOption;
class UPlayerResourceManager;
class UPlayerTechManager;
class UGlobalAbility;
class UW_GlobalAbilityPanel;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UGlobalAbilitiesManager : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UGlobalAbilitiesManager();
	
	void InitGlobalAbilitiesManager(const int32 OwningPlayer,
	                                const TArray<EGlobalAbility>& GlobalAbilities,
	                                UW_GlobalAbilityPanel* W_GlobalAbilityPanel, TObjectPtr<UPlayerResourceManager> PlayerResourceMgr, TObjectPtr<
	                                UGameUnitManager> GameUnitMgr, TObjectPtr<UPlayerTechManager> PlayerTechMgr);

	// Called when the game starts
	virtual void BeginPlay()  override;
	
	bool QueryRequirementForAbility(TObjectPtr<UGlobalAbility> Ability) const;
	bool QueryCostsForAbility(TObjectPtr<UGlobalAbility> Ability) const;
	
	void OnHoveredAbilityButton(UGlobalAbility* HoveredAbility, const bool bIsHover);
	void OnClickedAbilityButton(UGlobalAbility* ClickedAbility);
	
protected:
	
	
	private:
	UPROPERTY()
	int32 M_OwningPlayer;
	bool IsPlayerAbilityManager() const;
	
	UPROPERTY()
	TWeakObjectPtr<UW_GlobalAbilityPanel> Mw_GlobalAbilityPanel;
	[[nodiscard]] bool EnsureIsValidGlobalAbilityPanel()const;
	
	UPROPERTY()
	TWeakObjectPtr<UW_GA_Description> Mw_GA_Description;
	[[nodiscard]] bool EnsureIsValidGlobalAbilityDescription()const;
	
	UPROPERTY()
	TWeakObjectPtr<UPlayerTechManager> M_PlayerTechManager;
	[[nodiscard]] bool EnsureIsValidPlayerTechManager()const;
	
	UPROPERTY()
	TWeakObjectPtr<UPlayerResourceManager> M_PlayerResourceManager;
	[[nodiscard]] bool EnsureIsValidPlayerResourceManager()const;
	
	UPROPERTY()
	TWeakObjectPtr<UGameUnitManager> M_GameUnitManager;
	[[nodiscard]] bool EnsureIsValidGameUnitManager()const;
	
	void LoadGlobalAbilities(TArray<EGlobalAbility> AbilityEnums);
	void OnLoadedAbilities(TArray<TObjectPtr<UGlobalAbility>> AbilitiesLoadedCopyUObjects);
	
	void InitAbilityPanel(TArray<TObjectPtr<UGlobalAbility>> LoadedAbilities);

	void CheckRequirements() const;
	// Returns true if the player has this unit.
	bool CheckDoesPlayerHaveUnit(const FTrainingOption& UnitId)const;
	
	UPROPERTY()
	TArray<UGlobalAbility*> GlobalAbilities;

};
