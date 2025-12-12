// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "W_OpenTechTree.h"

#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/Player/CPPController.h"

void UW_OpenTechTree::OnButtonClicked()
{
	if (GetIsValidMainGameUI())
	{
		M_MainGameUI->OpenTechTree();
		return;
	}
	RTSFunctionLibrary::ReportError("No valid main game ui found for W_OpenTechTree");
}

void UW_OpenTechTree::NativeConstruct()
{
    Super::NativeConstruct();  // Call the base class's NativeConstruct

    // Ensure that the MainGameUI reference is set correctly when this widget is constructed
    GetIsValidMainGameUI();
}

bool UW_OpenTechTree::GetIsValidMainGameUI()
{
    if (IsValid(M_MainGameUI))
    {
        return true;
    }

    if(APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0); IsValid(PC))
    {
	    if(ACPPController* CPPController = Cast<ACPPController>(PC))
	    {
	    	M_MainGameUI = CPPController->GetMainMenuUI();
	    }
    }
	return IsValid(M_MainGameUI);
}
