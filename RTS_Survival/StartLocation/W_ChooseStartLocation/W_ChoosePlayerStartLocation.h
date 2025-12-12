#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "W_ChoosePlayerStartLocation.generated.h"

class UPlayerStartLocationManager;

UCLASS()
class UW_ChoosePlayerStartLocation : public UUserWidget
{
public:
	GENERATED_BODY()

	void SetPlayerStartLocationManager(UPlayerStartLocationManager* PlayerStartLocationManager);

protected:
	/** @param bGoToNextLocation - If true, go to the next location, go to previous otherwise. */
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void OnNavigateLocationsClicked(const bool bGoToNextLocation) const;

	// Notifies the player start location manager that the current location is selected as the start location.
	UFUNCTION(BlueprintCallable, NotBlueprintable)
	void OnSelectedLocationAsStart() const;

private:
	TWeakObjectPtr<UPlayerStartLocationManager> M_PlayerStartLocationManager;
	bool GetIsValidPlayerStartLocationManager() const;
};
