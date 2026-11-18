// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Audio/RTSVoiceLines/RTSVoicelines.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/TechTree/Technologies/Technologies.h"
#include "RTS_Survival/Units/Variants/UnitVariantTypes.h"
#include "RTSComponent.generated.h"


class AAircraftMaster;
class ABuildingExpansion;
class ANomadicVehicle;
class ASquadController;
class ATankMaster;
class UTacticalAIComp;
class UGameUnitManager;
DECLARE_MULTICAST_DELEGATE(FOnSubTypeInitialized);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnOwningPlayerChanged, uint8 /*OldOwner*/, uint8 /*NewOwner*/);
class RTS_SURVIVAL_API ACPPGameState;
enum class EAllUnitType : uint8;


/**
 * @brief Container for OwningPlayer and UnitType values.
 * @note SET IN BP CONSTRUCTOR
 * @note SetOwningPlayer, SetUnitType
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), BlueprintType)
class RTS_SURVIVAL_API URTSComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URTSComponent();

	// Delegate called when the subtype is initialized.
	FOnSubTypeInitialized OnSubTypeInitialized;

	// Called when the owning player changes after this component began play. By then the unit is registered under
	// its new owner, carries the new owner's collision mask and has the tactical AI its owner needs.
	// A change before BeginPlay only writes the value and does not call this.
	FOnOwningPlayerChanged OnOwningPlayerChanged;

	inline EAllUnitType GetUnitType() const { return UnitType; };
	inline void SetUnitType(const EAllUnitType NewUnitType) { UnitType = NewUnitType; };
	inline ERTSVoiceLineUnitType GetUnitVoiceLineType() const {return UnitVoiceLineType;}
	inline bool GetIsUnitInCombat() const { return bM_IsUnitInCombat; }
	void SetUnitInCombat(const bool bInCombat);
	

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	inline uint8 GetOwningPlayer() const { return OwningPlayer; };

	/**
	 * @brief Runtime-safe wrapper to change owner without exposing protected internals.
	 * @param NewOwningPlayer New owning player id.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetOwningPlayerRuntime(const uint8 NewOwningPlayer);

	inline float GetFormationUnitInnerRadius() const { return FormationUnitInnerRadius; };

	inline uint8 GetUnitSubType() const { return M_UnitSubType; };

	FString GetDisplayName(bool& bOutIsValidString) const;


	void DeregisterFromGameState() const;

	/**
	 * @brief Setup the subtype for this unit. Only provide one field.
	 * @param NewNomadicSubtype The Nomadic Subtype to set.
	 * @param NewTankSubtype The Tank Subtype to set.
	 * @param NewSquadSubtype
	 * @param NewBxpSubType
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetUnitSubtype(
		ENomadicSubtype NewNomadicSubtype = ENomadicSubtype::Nomadic_None,
		ETankSubtype NewTankSubtype = ETankSubtype::Tank_None,
		ESquadSubtype NewSquadSubtype = ESquadSubtype::Squad_None, EBuildingExpansionType
		NewBxpSubType = EBuildingExpansionType::BXT_Invalid,
		EAircraftSubtype NewAircraftSubtype = EAircraftSubtype::Aircarft_None);

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetStandaloneTurretSubtype(EStandaloneTurretSubtype NewStandaloneTurretSubtype);

	/** @return The tank subtype from the stored integer value. */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	ETankSubtype GetSubtypeAsTankSubtype() const;

	/** @return The nomadic subtype from the stored integer value. */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	ENomadicSubtype GetSubtypeAsNomadicSubtype() const;

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	ESquadSubtype GetSubtypeAsSquadSubtype() const;

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	EBuildingExpansionType GetSubtypeAsBxpSubtype() const;
	
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	EAircraftSubtype GetSubtypeAsAircraftSubtype() const;

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	EStandaloneTurretSubtype GetSubtypeAsStandaloneTurretSubtype() const;
	
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	FTrainingOption GetUnitTrainingOption() const;

	/** @brief Get the selection priority of the unit. */
	inline int32 GetSelectionPriority() const { return SelectionPriority; }

	UFUNCTION(BlueprintCallable)
	void SetSelectionPriority(const int32 NewSelectionPriority);

	/** @brief Lets an owner that must be known to the game state (target search, counters) override its Blueprint;
	 * only has effect before BeginPlay, where the unit registers. */
	void SetAddToGameState(const bool bNewAddToGameState) { bAddToGameState = bNewAddToGameState; }

	bool HasTechnologyApplied(const ETechnology Technology) const;
	void AddAppliedTechnology(const ETechnology Technology);

	void OnTankInitializedLookingForUpgrades(ATankMaster* Tank);
	void OnNomadicInitializedLookingForUpgrades(ANomadicVehicle* Nomadic);
	void OnSquadInitializedLookingForUpgrades(ASquadController* Squad);
	void OnBuildingExpansionInitializedLookingForUpgrades(ABuildingExpansion* BuildingExpansion);
	void OnAircraftInitializedLookingForUpgrades(AAircraftMaster* Aircraft);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Unit Variant")
	EUnitVariant GetUnitVariant() const { return UnitVariant; }

protected:
	// Set in the class defaults of a variant child Blueprint; the base Blueprint leaves it on Base. Read by UI and
	// portraits; nothing in gameplay branches on it. See Docs/Units/UnitVariants.md.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RTSComponent")
	EUnitVariant UnitVariant = EUnitVariant::Base;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 SelectionPriority = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Audio")
	ERTSVoiceLineUnitType UnitVoiceLineType = ERTSVoiceLineUnitType::None;
	

	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * @brief Sets the Player that owns this unit, which determines the team the unit belongs to.
	 * Additionally the unit will be assigned to the correct storage in the provided GameState Object.
	 * @param NewOwningPlayer The player team that this unit will be assigned to.
	 * @note Before BeginPlay this only writes the value; BeginPlay adds the unit to gameState. After BeginPlay
	 * the unit moves to the storage of the new player right away and OnOwningPlayerChanged is called.
	 */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void ChangeOwningPlayer(const uint8 NewOwningPlayer);

	// Can be set on component to derive what player owns this unit.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RTSComponent")
	uint8 OwningPlayer = 222;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "RTSComponent")
	EAllUnitType UnitType = EAllUnitType::UNType_None;

	// Determines whether the owner of this component will be registered with the GameState.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "RTSComponent")
	bool bAddToGameState = true;

	// Helps determine how far this unit needs to be placed from others in the formation in cm.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "RTSComponent|Formation")
	float FormationUnitInnerRadius = -1;

private:
	/**
	 * @brief Checks if the world is initialised and GameState can be retrieved.
	 * if so, then the OwningActor of this component will be added to the correct container in GameState.
	 * @return True if the unit was successfully added, false otherwise.
	 */
	bool AddUnitToGameState() const;

	// Silent: null when the world or its game state is not there (yet).
	UGameUnitManager* FindGameUnitManager() const;

	/**
	 * @brief Puts the owner in the unit manager's storage of a player; which list depends on the owner's class.
	 * @param GameUnitManager Manager of the world.
	 * @param ParentActor Owner of this component.
	 * @param Player Player the owner is registered for.
	 * @return False when the owner is of a class the unit manager does not keep.
	 */
	bool AddOwnerToUnitManager(UGameUnitManager& GameUnitManager, AActor& ParentActor, uint8 Player) const;

	/**
	 * @brief Takes the owner out of the unit manager's storage, from the list that fits the owner's class.
	 * @param GameUnitManager Manager of the world.
	 * @param ParentActor Owner of this component.
	 * @param Player Player the owner was registered for.
	 */
	void RemoveOwnerFromUnitManager(UGameUnitManager& GameUnitManager, AActor& ParentActor, uint8 Player) const;

	// Moves the owner from the storage of the old player to that of the new one after a runtime owner change.
	void MoveOwnerInGameState(uint8 OldOwningPlayer, uint8 NewOwningPlayer) const;

	// A unit registers with the game unit manager in BeginPlay, before its Blueprint or init function sets its
	// subtype; this moves its count to the subtype it has now.
	void RecountSubtypeInGameState() const;

	// Can be converted to an enum subtype to provide more information about this unit.
	uint8 M_UnitSubType;

	UPROPERTY()
	TArray<ETechnology> M_TechAppliedToUnit;


	// Goes through the UPROPERTYs and checks if they are set in the derived blueprint.
	void CheckExposedUVariables();

	// Whether the owner of this component is in combat.
	// Can be set by associated HealthComponent on the same unit; at bp the health component will look for this rts comp.
	// Can be set by weapons when opening fire.
	bool bM_IsUnitInCombat = false;

	// How long the unit is in combat after in-combat is triggered by being attacked or by attacking. 
	const float M_UnitInCombatTimeElapse = 5.f;

	FTimerHandle M_UnitInCombatTimerHandle;

	// Units of every player but the human get a tactical AI component: at BeginPlay and when a unit changes to
	// such an owner later.
	void DetermineCreateTacticalAI();
	// After a runtime owner change: creates the tactical AI for a unit the human lost, removes it from one the
	// human gained.
	void UpdateTacticalAIForOwner();
	// Silent: units of the human have none.
	bool GetHasTacticalAI() const;
	void CreateTankTacticalAIAtOwner();
	void CreateSquadTacticalAIAtOwner();
	UPROPERTY()
	TWeakObjectPtr<UTacticalAIComp> M_EnemyTatcicalAIComp = nullptr;



	
};
