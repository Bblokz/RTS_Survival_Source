// Copyright (C) Bas Blokzijl - All rights reserved.


#include "W_TurnCounter.h"

#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

void UW_TurnCounter::InitTurnCounter(const TArray<ERTSResourceType>& ResourceTypes, const int32 TurnCount)
{
	InitResourceWidgets(ResourceTypes);
	SetTurnCounterText(TurnCount);
}

bool UW_TurnCounter::EnsureResourceIsValid(const TObjectPtr<UW_PlayerResource>& ResourceToCheck)
{
	if (not IsValid(ResourceToCheck))
	{
		RTSFunctionLibrary::ReportError("one of the resource widgets in the turn counter is not valid"
			"");
		return false;
	}
	return true;
}

UPlayerResourceManager* UW_TurnCounter::GetPlayerResourceManager() const
{
	UPlayerResourceManager* ResourceManager = FRTS_Statics::GetPlayerResourceManager(this);
	if (not ResourceManager)
	{
		RTSFunctionLibrary::ReportError("Failed to get the PlayerResource Manager for W_TurnCounter"
			"\n InitTurnCounter will fail.");
		return nullptr;
	}
	return ResourceManager;
}

void UW_TurnCounter::InitResourceWidgets(TArray<ERTSResourceType> ResourceTypes)
{
	UPlayerResourceManager* ResourceManager = GetPlayerResourceManager();
	if (not ResourceManager)
	{
		return;
	}
	int32 Index = 0;
	if (EnsureResourceIsValid(BP_W_PlayerResource1) && ResourceTypes.IsValidIndex(Index))
	{
		BP_W_PlayerResource1->InitUwPlayerResource(ResourceManager,
			ResourceTypes[Index]);
	}
	Index++;
	if(EnsureResourceIsValid(BP_W_PlayerResource2) && ResourceTypes.IsValidIndex(Index))
	{
		BP_W_PlayerResource2->InitUwPlayerResource(ResourceManager,
			ResourceTypes[Index]);
	}
	Index++;
	if(EnsureResourceIsValid(BP_W_PlayerResource3) && ResourceTypes.IsValidIndex(Index))
	{
		BP_W_PlayerResource3->InitUwPlayerResource(ResourceManager,
			ResourceTypes[Index]);
	}
	Index++;
	if(EnsureResourceIsValid(BP_W_PlayerResource4) && ResourceTypes.IsValidIndex(Index))
	{
		BP_W_PlayerResource4->InitUwPlayerResource(ResourceManager,
			ResourceTypes[Index]);
	}
	Index++;
	if(EnsureResourceIsValid(BP_W_PlayerResource5) && ResourceTypes.IsValidIndex(Index))
	{
		BP_W_PlayerResource5->InitUwPlayerResource(ResourceManager,
			ResourceTypes[Index]);
	}
	
}

void UW_TurnCounter::SetTurnCounterText(const int32 TurnCount) const
{
	if(not IsValid(M_TurnCounter))
	{
		RTSFunctionLibrary::ReportError("Cannot set turn counter text, M_TurnCounter is not valid"
			"\n at function: UW_TurnCounter::SetTurnCounterText");
		return;
	}
 FString TurnString = FRTSRichTextConverter::MakeRTSRich(FString::Printf(TEXT("Turn: %d"), TurnCount), ERTSRichText::Text_NewGood);
	M_TurnCounter->SetText(FText::FromString(TurnString));
}
