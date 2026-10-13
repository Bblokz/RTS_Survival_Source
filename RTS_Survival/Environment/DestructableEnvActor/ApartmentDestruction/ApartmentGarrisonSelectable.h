// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/MasterObjects/SelectableBase/SelectableActorObjectsMaster.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/SelectionComponent.h"

#include "ApartmentGarrisonSelectable.generated.h"

class AApartmentDestructibleActor;
class UBoxComponent;
class UDecalComponent;
class UHealthComponent;
class UMaterialInterface;

/**
 * @brief RTS component of the apartment garrison actor: owned by the player, of the garrison building unit type,
 * and kept out of the unit lists of the game state so nothing targets the actor.
 */
UCLASS()
class RTS_SURVIVAL_API UApartmentGarrisonRTSComponent : public URTSComponent
{
	GENERATED_BODY()

public:
	UApartmentGarrisonRTSComponent();
};

/** @brief Selection component of the apartment garrison actor, set up from C++ instead of from a Blueprint. */
UCLASS()
class RTS_SURVIVAL_API UApartmentGarrisonSelectionComponent : public USelectionComponent
{
	GENERATED_BODY()

public:
	/**
	 * @brief Hands the selection area, the decal and its materials to the base component.
	 * @param SelectionArea Box the marquee tests against.
	 * @param SelectionDecal Decal shown under the building while it is selected.
	 * @param NewSelectedMaterial Decal material while selected.
	 * @param NewDeselectedMaterial Decal material while not selected.
	 */
	void SetupGarrisonSelection(UBoxComponent* SelectionArea, UDecalComponent* SelectionDecal,
	                            UMaterialInterface* NewSelectedMaterial, UMaterialInterface* NewDeselectedMaterial);
};

/**
 * @brief The selectable side of a destructible apartment. The apartment spawns one while the player has soldiers
 * on its cover sockets and destroys it when the last one left; a click on the apartment selects this actor.
 * Its command card holds Exit Cargo, which sends every soldier inside out through the door.
 */
UCLASS()
class RTS_SURVIVAL_API AApartmentGarrisonSelectable : public ASelectableActorObjectsMaster
{
	GENERATED_BODY()

public:
	AApartmentGarrisonSelectable(const FObjectInitializer& ObjectInitializer);

	/**
	 * @brief Binds the actor to its apartment and sizes the selection decal; call before spawning finishes.
	 * @param Apartment The apartment the player's soldiers are in.
	 * @param FootprintExtent Half size of the apartment's footprint, in the apartment's space.
	 */
	void InitGarrison(AApartmentDestructibleActor* Apartment, const FVector& FootprintExtent);

protected:
	virtual void BeginPlay() override;

	virtual void ExecuteExitCargoCommand() override;

	// Decal drawn around the building while it is selected.
	UPROPERTY(EditDefaultsOnly, Category = "Selection")
	TSoftObjectPtr<UMaterialInterface> SelectedDecalMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Selection")
	TSoftObjectPtr<UMaterialInterface> DeselectedDecalMaterial;

	// Card slot of the Exit Cargo button.
	UPROPERTY(EditDefaultsOnly, Category = "Abilities", meta = (ClampMin = "0"))
	int32 ExitCargoAbilityIndex = 0;

private:
	UPROPERTY()
	TObjectPtr<UApartmentGarrisonRTSComponent> M_GarrisonRTSComponent;

	UPROPERTY()
	TObjectPtr<UHealthComponent> M_GarrisonHealthComponent;

	UPROPERTY()
	TObjectPtr<UApartmentGarrisonSelectionComponent> M_GarrisonSelectionComponent;
	bool GetIsValidGarrisonSelectionComponent() const;

	// Small on purpose: a marquee over soldiers in the street must not pick the building up with them.
	UPROPERTY()
	TObjectPtr<UBoxComponent> M_SelectionArea;

	UPROPERTY()
	TObjectPtr<UDecalComponent> M_SelectionDecal;
	bool GetIsValidSelectionDecal() const;

	UPROPERTY()
	TWeakObjectPtr<AApartmentDestructibleActor> M_Apartment;
	bool GetIsValidApartment() const;

	void BeginPlay_SetupSelection();
	// The card holds Exit Cargo only.
	void BeginPlay_SetupCommandCard();
};
