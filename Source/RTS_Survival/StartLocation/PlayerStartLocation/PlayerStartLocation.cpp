// Copyright (C) Bas Blokzijl - All rights reserved.


#include "PlayerStartLocation.h"

#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowComp.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowType.h"
#include "RTS_Survival/StartLocation/PlayerStartLocationMngr/PlayerStartLocationManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"


APlayerStartLocation::APlayerStartLocation()
{
	PrimaryActorTick.bCanEverTick = false;

	M_FowComponent = CreateDefaultSubobject<UFowComp>(TEXT("FowComponent"));
}

void APlayerStartLocation::BeginPlay()
{
	Super::BeginPlay();
	if(GetIsValidFOWComponent())
	{
		M_FowComponent->SetVisionRadius(VisionRadiusInFOW);
		M_FowComponent->StartFow(EFowBehaviour::Fow_Active, true);
	}
	RegisterWithPlayerStartLocationManager();
	
}

void APlayerStartLocation::RegisterWithPlayerStartLocationManager()
{
	if(UPlayerStartLocationManager* PlayerStartLocationManager = FRTS_Statics::GetPlayerStartLocationManager(this))
	{
		Debug_RegisterWithPlayerStartLocationManager();
		PlayerStartLocationManager->RegisterStartLocation(this);
		return;
	}
	RTSFunctionLibrary::ReportError("Could not register with PlayerStartLocationManager in APlayerStartLocation"
								 "\n as the PlayerStartLocationManager is not valid.");
}

bool APlayerStartLocation::GetIsValidFOWComponent() const
{
	if(IsValid(M_FowComponent))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("FowComponent is not valid in APlayerStartLocation"
								 "\n Location: " + GetName());
	return false;
}

void APlayerStartLocation::Debug_RegisterWithPlayerStartLocationManager() const
{
	if(DeveloperSettings::Debugging::GPlayerStartLocations_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Registering with player start location manger for StartLocation:"
								  "\n " + GetName() + " \n at location: " + GetActorLocation().ToString());
	}
}

