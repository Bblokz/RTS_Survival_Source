// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.


#include "W_EnergyBarInfo.h"

#include "Components/RichTextBlock.h"

void UW_EnergyBarInfo::SetSupplyDemand(const int32 EnergySupply, const int32 EnergyDemand)
{
	M_EnergySupply = EnergySupply;
	M_EnergyDemand = EnergyDemand;

	const int32 Total = EnergySupply - EnergyDemand;
	FString TotalStr;

	if (Total > 0)
	{
		TotalStr = FString::Printf(TEXT("<Text_Energy>Energy Supply:</> %d"), Total);
	}
	else
	{
		TotalStr = FString::Printf(TEXT("<Text_NoEnergy>Energy Deficit:</> %d"), -Total);
	}
	TotalStr += "\n <Text_Energy>Production: </> " + FString::FromInt(EnergySupply);
	TotalStr += "\n <Text_Energy>Consumption: </> " + FString::FromInt(EnergyDemand);

	RichText->SetText(FText::FromString(TotalStr));
}
