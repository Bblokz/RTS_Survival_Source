// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerStartLocationManager.generated.h"


class ACPPController;
class UW_ChoosePlayerStartLocation;
class ARTSLandscapeDivider;
class APlayerStartLocation;

/**
 * @brief Component that registers start locations and drives the selection flow for the player.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RTS_SURVIVAL_API UPlayerStartLocationManager : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UPlayerStartLocationManager();

	void SetPlayerController(ACPPController* PlayerController);

	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void SetWidgetLocationClass(TSubclassOf<UW_ChoosePlayerStartLocation> WidgetClass);

	/**
	 * @brief Registers the location; when all locations are registered, the player can choose a start location.
	 * @param StartLocation The start location to register.
	 */
	void RegisterStartLocation(APlayerStartLocation* StartLocation);

	// Called by UW_ChoosePlayerStartLocation when the player decides to go to the next or previous location.
	void OnClickedLocation(const bool bGoToNextLocation);
	// Called by UW_ChoosePlayerStartLocation when the player has chosen a start location.
	void OnDecidedToStartAtLocation();


protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	TArray<TWeakObjectPtr<APlayerStartLocation>> M_StartLocations;
	// If all locations are registered we add our callback for when the main menu UI is loaded to start the
	// Cycling through potential start locations.
	void CheckIfAllStartLocationsAreRegistered();
	[[nodiscard]] bool EnsureValidLandscapeDivider();
	[[nodiscard]] bool EnsureValidPlayerController();
	[[nodiscard]] bool GetIsValidStartingLocationUI() const;
	// Sets the call back to when the main game ui is loaded.
	void OnAllPlayerStartLocationsRegistered();

	// Creates the UI for selecting a start location.
	void OnMainMenuReady();
	void OnStartLocationChosen(const FVector& StartLocation);

	TWeakObjectPtr<ARTSLandscapeDivider> M_LandscapeDivider;
	TWeakObjectPtr<ACPPController> M_PlayerController;

	// The UI that allows the player to go through the starting locations.
	UPROPERTY()
	UW_ChoosePlayerStartLocation* M_ChoosePlayerStartLocationWidget;

	UPROPERTY()
	TSubclassOf<UW_ChoosePlayerStartLocation> M_ChoosePlayerStartLocationWidgetClass;
	

	void SetInputToFocusWidget(const TObjectPtr<UUserWidget>& WidgetToFocus) const;

	void NavigateCameraToLocation(const int32 Index);
	/**
	 * @brief Validates the index and resolves a live start location for navigation or selection.
	 * @param Index The desired start location index.
	 * @param OutStartLocation The resolved live start location when valid.
	 * @return True when the index is valid and the start location is still alive.
	 */
	[[nodiscard]] bool GetIsValidStartLocationAtIndex(
		const int32 Index,
		APlayerStartLocation*& OutStartLocation) const;

	void OnCouldNotInitStartLocationUI() const;

	// Determines the location the player currently sees when deciding to start at a location.
	int32 M_CurrentStartLocationIndex = 0;

	void LocationChosen_DestroyWidget();
	void LocationChosen_RestoreInputMode();
	void LocationChosen_CleanUpStartLocations();

	void Debug_PickedLocation(const FVector& Location) const;
	void Debug_MenuReady(const TObjectPtr<UW_ChoosePlayerStartLocation>& StartLocationsWidget) const;
	void Debug_Message(const FString& Message) const;
};

