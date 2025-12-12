// Copyright (C) Bas Blokzijl - All rights reserved.


#include "Trainer.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"


FString ITrainer::GetTrainerName()
{
	if (const UTrainerComponent* TrainerComponent = GetTrainerComponent())
	{
		return TrainerComponent->GetName();
	}
	RTSFunctionLibrary::ReportError(
		"Training component is nullptr in GetName. \n At function GetName in Trainer.cpp"
		"Class name: ITrainer.");
	return "TrainerComponent is nullptr";
}
