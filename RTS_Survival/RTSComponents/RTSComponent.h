// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Audio/RTSVoiceLines/RTSVoicelines.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTSComponent.generated.h"


DECLARE_MULTICAST_DELEGATE(FOnSubTypeInitialized);
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

	inline EAllUnitType GetUnitType() const { return UnitType; };
	inline void SetUnitType(const EAllUnitType NewUnitType) { UnitType = NewUnitType; };
	inline ERTSVoiceLineUnitType GetUnitVoiceLineType() const {return UnitVoiceLineType;}
	inline bool GetIsUnitInCombat() const { return bM_IsUnitInCombat; }
	void SetUnitInCombat(const bool bInCombat);
	

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	inline uint8 GetOwningPlayer() const { return OwningPlayer; };

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
	FTrainingOption GetUnitTrainingOption() const;

	/** @brief Get the selection priority of the unit. */
	inline int32 GetSelectionPriority() const { return SelectionPriority; }

	UFUNCTION(BlueprintCallable)
	void SetSelectionPriority(const int32 NewSelectionPriority);

protected:
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
	 * @note If the game has started this function will add the unit to gameState, if this fails it will try
	 * again in BeginPlay.
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

	// Can be converted to an enum subtype to provide more information about this unit.
	uint8 M_UnitSubType;

	// Goes through the UPROPERTYs and checks if they are set in the derived blueprint.
	void CheckExposedUVariables();

	// Whether the owner of this component is in combat.
	// Can be set by associated HealthComponent on the same unit; at bp the health component will look for this rts comp.
	// Can be set by weapons when opening fire.
	bool bM_IsUnitInCombat = false;

	// How long the unit is in combat after in-combat is triggered by being attacked or by attacking. 
	const float M_UnitInCombatTimeElapse = 5.f;

	FTimerHandle M_UnitInCombatTimerHandle;
	
};
