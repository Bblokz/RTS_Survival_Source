#include "W_TrainingDescription.h"

#include "RTS_Survival/GameUI/MainGameUI.h"
#include "RTS_Survival/GameUI/CostWidget/W_CostDisplay.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"


void UW_TrainingDescription::SetTrainingDescriptionVisibility(const bool bVisible, const FTrainingWidgetState& TrainingWidgetState)
{
	const ESlateVisibility NewVisibility = bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	SetVisibility(NewVisibility);
	if(bVisible)
	{
		OnUpdateForWidgetState(TrainingWidgetState);
	}
	
}

void UW_TrainingDescription::SetTrainingMenuManager(UTrainingMenuManager* TrainingMenuManager)
{
	M_TrainingMenuManager = TrainingMenuManager;
}

void UW_TrainingDescription::SetResourceManager(const TWeakObjectPtr<UPlayerResourceManager> PlayerResourceManager)
{
	M_PlayerResourceManager = PlayerResourceManager;
}


FText UW_TrainingDescription::GetTrainingTimeDisplay(const FTrainingOption TrainingOption, const float TimeToTrain) const
{

	if(GetIsValidTrainingMenuManager())
	{
		const FTrainingOptionState OptionState =  M_TrainingMenuManager->GetTrainingOptionState(TrainingOption);
		const FText TrainText = OptionState.UnitType == EAllUnitType::UNType_Squad ? FText::FromString("train") : FText::FromString("build");
		const FText TimeText = FText::FromString(FString::SanitizeFloat(TimeToTrain));
		return FText::Format(FText::FromString("Time to {0}: <DisplayAmount>{1} sec</>"), TrainText, TimeText);
	}
	return FText::FromString("Error");
}

bool UW_TrainingDescription::GetIsValidTrainingMenuManager() const
{
	if(IsValid(M_TrainingMenuManager))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("TrainingMenuManager is not valid in GetIsValidTrainingMenuManager"
		"\n at function UW_TrainingDescription::GetIsValidTrainingMenuManager");
	return false;
}

bool UW_TrainingDescription::GetIsValidCostDisplay() const
{
	if(IsValid(CostDisplay))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("CostDisplay is not valid in GetIsValidCostDisplay"
		"\n at function UW_TrainingDescription::GetIsValidCostDisplay");
	return false;
}

void UW_TrainingDescription::UpdateCostDisplayForTrainingOption(const FTrainingOption& TrainingOption) const
{
	if(not GetIsValidPlayerResourceManager() || not GetIsValidCostDisplay())
	{
		return;	
	}
	bool bIsValidData = false;
	const TMap<ERTSResourceType, int32>  Costs = M_PlayerResourceManager->GetResourceCostsOfTrainingOption(TrainingOption, bIsValidData);
	if(not bIsValidData)
	{
		RTSFunctionLibrary::ReportError("Training option is not valid in UpdateCostDisplayForTrainingOption"
			"\n at function UW_TrainingDescription::UpdateCostDisplayForTrainingOption");
		return;
	}
	CostDisplay->SetupCost(Costs);
	
}

void UW_TrainingDescription::OnUpdateForWidgetState(const FTrainingWidgetState& TrainingWidgetState)
{
	OnShowTrainingDescription(TrainingWidgetState);
	UpdateCostDisplayForTrainingOption(TrainingWidgetState.ItemID);
}

bool UW_TrainingDescription::GetIsValidPlayerResourceManager() const
{
	if (M_PlayerResourceManager.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("Player resource manager is not valid."
		"\n see UW_TrainingDescription::GetIsValidPlayerResourceManager.");
	return false;
}



