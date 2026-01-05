// Copyright (C) Bas Blokzijl - All rights reserved.


#include "FieldConstructionAbilityComponent.h"

#include "RTS_Survival/Units/SquadController.h"


UFieldConstructionAbilityComponent::UFieldConstructionAbilityComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UFieldConstructionAbilityComponent::AddStaticPreviewWaitingForConstruction(AStaticPreviewMesh* StaticPreview)
{
	M_StaticPreviewsWaitingForConstruction.Add(StaticPreview);
}


void UFieldConstructionAbilityComponent::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_SetFieldTypeOnConstructionData();
}

void UFieldConstructionAbilityComponent::BeginPlay_SetFieldTypeOnConstructionData()
{
	// This needs to be saved for the player preview mesh system so we can provide back what type of field construction is placed after a valid placement.
	FieldConstructionAbilitySettings.FieldConstructionData.FieldConstructionType = FieldConstructionAbilitySettings.
		FieldConstructionType;
}

void UFieldConstructionAbilityComponent::BeginPlay_SetOwningSquadController()
{
	if (GetOwner())
	{
		ASquadController* OwningSquadController = Cast<ASquadController>(GetOwner());
		M_OwningSquadController = OwningSquadController;
		// Error checking.
		(void)GetIsValidSquadController();
	}
}

bool UFieldConstructionAbilityComponent::GetIsValidSquadController() const
{
	if (not IsValid(M_OwningSquadController))
	{
		RTSFunctionLibrary::ReportError(
			TEXT("UFieldConstructionAbilityComponent::GetIsValidSquadController"),
			TEXT("Owning Squad Controller is not valid."));
		return false;
	}
	return true;
}
