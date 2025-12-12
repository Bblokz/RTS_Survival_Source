#include "W_ChoosePlayerStartLocation.h"

#include "RTS_Survival/StartLocation/PlayerStartLocationMngr/PlayerStartLocationManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_ChoosePlayerStartLocation::SetPlayerStartLocationManager(
	UPlayerStartLocationManager* PlayerStartLocationManager)
{
	M_PlayerStartLocationManager = PlayerStartLocationManager;
}

void UW_ChoosePlayerStartLocation::OnNavigateLocationsClicked(const bool bGoToNextLocation) const
{
	if(not GetIsValidPlayerStartLocationManager())
	{
	return;	
	}
	M_PlayerStartLocationManager.Get()->OnClickedLocation(bGoToNextLocation);
}

void UW_ChoosePlayerStartLocation::OnSelectedLocationAsStart() const
{
	if(not GetIsValidPlayerStartLocationManager())
	{
		return;
	}
	M_PlayerStartLocationManager.Get()->OnDecidedToStartAtLocation();
}

bool UW_ChoosePlayerStartLocation::GetIsValidPlayerStartLocationManager() const
{
	if(M_PlayerStartLocationManager.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("PlayerStartLocationManager is not valid in UW_ChoosePlayerStartLocation");
	return false;
}
