// Copyright (C) Bas Blokzijl - All rights reserved.

#include "W_PlayerResource.h"

#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/GameUI/Resources/WPlayerResource/ResourceDescription/W_ResourceDescription.h"
#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"

void UW_PlayerResource::InitUwPlayerResource(
	UPlayerResourceManager* NewPlayerResourceManager,
	const ERTSResourceType NewResourceType)
{
	if (IsValid(NewPlayerResourceManager))
	{
		PlayerResourceManager = NewPlayerResourceManager;
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorInitialisation(
			this,
			"PlayerResourceManager",
			"UW_PlayerResource::InitUWPlayerResource");
	}
	ResourceType = NewResourceType;
	SetupResource(NewResourceType);
}

void UW_PlayerResource::NativeConstruct()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			ResourceUpdateTimerHandle,
			this,
			&UW_PlayerResource::SetTextOnDescriptionWidget,
			2.f, // Wait unitl loaded.
			false
		);
	}

	Super::NativeConstruct();
}

void UW_PlayerResource::BeginDestroy()
{
	if (GetWorld())
	{
		if (ResourceUpdateTimerHandle.IsValid())
		{
			GetWorld()->GetTimerManager().ClearTimer(ResourceUpdateTimerHandle);
		}
	}
	Super::BeginDestroy();
}

FText UW_PlayerResource::GetResourceText() const
{
	if (IsValid(PlayerResourceManager))
	{
		const int32 RawAmount = PlayerResourceManager->GetResourceAmount(ResourceType);
		const int32 RawStorage = PlayerResourceManager->GetResourceStorage(ResourceType);

		auto FormatNumber = [](int32 Number) -> FString
		{
			if (Number < 10000)
			{
				return FString::FromInt(Number);
			}
			if (Number < 1000000)
			{
				return FString::Printf(TEXT("%.0fk"), Number / 1000.0f);
			}
			return FString::Printf(TEXT("%.0fm"), Number / 1000000.0f);
		};

		const FString FormattedAmount = FormatNumber(RawAmount);
		const FString FormattedStorage = FormatNumber(RawStorage);
		FString Result;
		if (GetIsResourceOfBlueprintType(ResourceType))
		{
			// for blueprints, only show the amount
			Result = FormattedAmount;
		}
		else
		{
			Result = FString::Printf(TEXT("%s/%s"), *FormattedAmount, *FormattedStorage);
		}

		// Check if the storage is full and change the text color accordingly
		if (RawAmount >= RawStorage)
		{
			Result = FRTSRichTextConverter::MakeStringRTSResource(Result, ERTSResourceRichText::Red);
		}
		else
		{
			// The regular styling to portray an amount.
			Result = FRTSRichTextConverter::MakeStringRTSResource(Result, ERTSResourceRichText::DisplayAmount);
		}

		return FText::FromString(Result);
	}
	return FText::FromString("NoPlRsMn");
}

void UW_PlayerResource::SetTextOnDescriptionWidget() const
{
	if(IsValid(ResourceDescriptionWidget))
	{
		FString ResourceFancy = FRTSRichTextConverter::MakeRTSRich(Global_GetResourceTypeDisplayString(ResourceType), ERTSRichText::Text_SubTitle);
		ResourceDescriptionWidget->SetText(FText::FromString(ResourceFancy));
		return;	
	}
	RTSFunctionLibrary::ReportNullErrorInitialisation(
		this,
		"ResourceDescriptionWidget",
		"UW_PlayerResource::SetTextOnDescriptionWidget");
}

