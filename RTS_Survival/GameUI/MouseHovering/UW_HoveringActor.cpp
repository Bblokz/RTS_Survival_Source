#include "UW_HoveringActor.h"

#include "LandscapeProxy.h"
#include "Components/BorderSlot.h"
#include "Components/RichTextBlock.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/CaptureMechanic/CaptureInterface/CaptureInterface.h"
#include "RTS_Survival/Game/GameState/GameResourceManager/GetTargetResourceThread/FGetAsyncResource.h"
#include "RTS_Survival/Resources/Harvester/Harvester.h"
#include "RTS_Survival/Resources/ResourceComponent/ResourceComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/CargoMechanic/Cargo/Cargo.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"

void UW_HoveringActor::NativeConstruct()
{
	Super::NativeConstruct();
	HideWidget();
}

FString UW_HoveringActor::GetActorText(AActor* HoveredActor, bool& bOutIsValidText, int32& OutPadding) const
{
	bOutIsValidText = false;
	OutPadding = 15;

	if (not IsValid(HoveredActor))
	{
		return "";
	}

	FString ActorText;


	// Capture info also has a dedicated view.
	if (TryBuildCaptureHoverText(HoveredActor, bOutIsValidText, OutPadding, ActorText))
	{
		return ActorText;
	}

	// Standard RTS units / buildings.
	if (TryBuildRTSActorHoverText(HoveredActor, bOutIsValidText, OutPadding, ActorText))
	{
		return ActorText;
	}

	// Terrain.
	if (TryBuildLandscapeHoverText(HoveredActor, bOutIsValidText, ActorText))
	{
		return ActorText;
	}

	// Pure resource nodes.
	if (TryBuildResourceHoverText(HoveredActor, bOutIsValidText, OutPadding, ActorText))
	{
		return ActorText;
	}
	
	// Cargo after resource (in case of hq which has both): show squad capacity / occupancy.
	if (TryBuildCargoHoverText(HoveredActor, bOutIsValidText, OutPadding, ActorText))
	{
		return ActorText;
	}

	// Drop-offs without RTS component (test actors).
	if (TryBuildDropOffHoverText_NoRTS(HoveredActor, bOutIsValidText, OutPadding, ActorText))
	{
		return ActorText;
	}
	

	// Generic world geometry.
	if (TryBuildStaticMeshHoverText(HoveredActor, bOutIsValidText, ActorText))
	{
		return ActorText;
	}

	return GetUnknownActorText(bOutIsValidText);
}

bool UW_HoveringActor::TryBuildRTSActorHoverText(AActor* HoveredActor, bool& bOutIsValidText, int32& OutPadding,
                                                 FString& OutActorText) const
{
	URTSComponent* RTSComponent = HoveredActor->FindComponentByClass<URTSComponent>();
	if (not IsValid(RTSComponent))
	{
		return false;
	}
	if(RTSComponent->GetOwningPlayer() == 0)
	{
		if(TryBuildCargoHoverText(HoveredActor, bOutIsValidText, OutPadding, OutActorText))
		{
			return true;
		}
		if(TryBuildCaptureHoverText(HoveredActor, bOutIsValidText, OutPadding, OutActorText))
		{
			return true;
		}
	}

	if (UResourceDropOff* DropOffComponent = HoveredActor->FindComponentByClass<UResourceDropOff>())
	{
		bOutIsValidText = true;
		OutPadding = 35;
		OutActorText = GetDropOffText(DropOffComponent, RTSComponent);
		return true;
	}

	const bool bIsEnemy = RTSComponent->GetOwningPlayer() != 1;
	if (bIsEnemy)
	{
		OutActorText = "Enemy";
	}
	else
	{
		if (UHarvester* HarvesterComp = HoveredActor->FindComponentByClass<UHarvester>())
		{
			OutActorText = GetHarvesterText(RTSComponent, HarvesterComp, bOutIsValidText);
			return true;
		}
		OutActorText = "Friendly";
	}

	// Sets the name according to the subtype, also sets whether we could deduce a valid name.
	OutActorText += (" " + RTSComponent->GetDisplayName(bOutIsValidText));
	bOutIsValidText = true;
	return true;
}

bool UW_HoveringActor::TryBuildCargoHoverText(AActor* HoveredActor, bool& bOutIsValidText, int32& OutPadding,
                                              FString& OutActorText) const
{
	if (not IsValid(HoveredActor))
	{
		return false;
	}

	UCargo* CargoComponent = HoveredActor->FindComponentByClass<UCargo>();
	if (CargoComponent == nullptr)
	{
		return false;
	}

	const int32 MaxSquadsSupported = CargoComponent->GetMaxSquadsSupported();
	const int32 CurrentSquads = CargoComponent->GetCurrentSquads();

	OutActorText = "Squads: " + FString::FromInt(CurrentSquads) + " / " + FString::FromInt(MaxSquadsSupported);
	bOutIsValidText = true;
	OutPadding = 35;
	return true;
}

bool UW_HoveringActor::TryBuildCaptureHoverText(AActor* HoveredActor, bool& bOutIsValidText, int32& OutPadding,
                                                FString& OutActorText) const
{
	if (not IsValid(HoveredActor))
	{
		return false;
	}

	ICaptureInterface* CaptureInterface = Cast<ICaptureInterface>(HoveredActor);
	if (CaptureInterface == nullptr)
	{
		return false;
	}

	const int32 UnitsRequired = CaptureInterface->GetCaptureUnitAmountNeeded();
	OutActorText = "Capturable: " + FString::FromInt(UnitsRequired) + " units required";
	bOutIsValidText = true;
	OutPadding = 35;
	return true;
}

bool UW_HoveringActor::TryBuildResourceHoverText(AActor* HoveredActor, bool& bOutIsValidText, int32& OutPadding,
                                                 FString& OutActorText) const
{
	UResourceComponent* ResourceComponent = HoveredActor->FindComponentByClass<UResourceComponent>();
	if (ResourceComponent == nullptr)
	{
		return false;
	}

	bOutIsValidText = true;
	OutPadding = 35;
	OutActorText = GetResourceText(ResourceComponent);
	return true;
}

bool UW_HoveringActor::TryBuildDropOffHoverText_NoRTS(AActor* HoveredActor, bool& bOutIsValidText, int32& OutPadding,
                                                      FString& OutActorText) const
{
	UResourceDropOff* DropOffComponent = HoveredActor->FindComponentByClass<UResourceDropOff>();
	if (DropOffComponent == nullptr)
	{
		return false;
	}

	bOutIsValidText = true;
	OutPadding = 35;
	// Test drop-offs usually do not have an RTS component.
	OutActorText = GetDropOffText(DropOffComponent, nullptr);
	return true;
}

bool UW_HoveringActor::TryBuildLandscapeHoverText(AActor* HoveredActor, bool& bOutIsValidText,
                                                  FString& OutActorText) const
{
	if (not HoveredActor->IsA(ALandscapeProxy::StaticClass()))
	{
		return false;
	}

	bOutIsValidText = true;
	OutActorText = "Terrain";
	return true;
}

bool UW_HoveringActor::TryBuildStaticMeshHoverText(AActor* HoveredActor, bool& bOutIsValidText,
                                                   FString& OutActorText) const
{
	if (not HoveredActor->IsA(AStaticMeshActor::StaticClass()))
	{
		return false;
	}

	bOutIsValidText = true;
	OutActorText = "World Geometry";
	return true;
}

FString UW_HoveringActor::GetUnknownActorText(bool& bOutIsValidText) const
{
	bOutIsValidText = false;
	return "Could not determine name";
}

FString UW_HoveringActor::GetResourceText(const UResourceComponent* ResourceComponent) const
{
	if (not IsValid(ResourceComponent))
	{
		return "Resource";
	}
	FString ResourceTypeText = Global_GetResourceTypeDisplayString(ResourceComponent->GetResourceType());
	FString AmountRemaining = "Remaining: " + FString::FromInt(ResourceComponent->GetTotalAmount());
	return FRTSRichTextConverter::MakeRTSRich(ResourceTypeText, ERTSRichText::Text_Title) + "\n" +
		FRTSRichTextConverter::MakeRTSRich(AmountRemaining, ERTSRichText::Text_SubTitle);
}

FString UW_HoveringActor::GetDropOffText(const UResourceDropOff* DropOffComponent,
                                         const URTSComponent* RTSComponent) const
{
	if (not IsValid(DropOffComponent))
	{
		return "Invalid Drop Off";
	}
	// Allow for an invalid rts component as some testing drop offs will not have one.
	FString UnitName = "DropOff";
	if (IsValid(RTSComponent))
	{
		bool bIsValidRTSName = false;
		FString RTSName = RTSComponent->GetDisplayName(bIsValidRTSName);
		UnitName = bIsValidRTSName ? RTSName : UnitName;
	}
	UnitName = FRTSRichTextConverter::MakeRTSRich(UnitName, ERTSRichText::Text_Title);
	UnitName += "\n";
	for (const auto EachResourceCapacity : DropOffComponent->GetResourceDropOffCapacity())
	{
		const FString ResourceType = Global_GetResourceTypeDisplayString(EachResourceCapacity.Key);
		const FString Amount = FString::FromInt(EachResourceCapacity.Value.CurrentAmount);
		const FString MaxCapacity = FString::FromInt(EachResourceCapacity.Value.MaxCapacity);
		const ERTSRichText ResourceRichText = FRTSRichTextConverter::ConvertResourceToRTSRichText(
			EachResourceCapacity.Key);
		UnitName += "\n" +
			FRTSRichTextConverter::MakeRTSRich(ResourceType, ResourceRichText) + "\n" +
			FRTSRichTextConverter::MakeRTSRich("Amount: " + Amount, ERTSRichText::Text_SubTitle) + " " +
			FRTSRichTextConverter::MakeRTSRich("Capacity: " + MaxCapacity, ERTSRichText::Text_SubTitle)
			+ "\n";
	}
	return UnitName;
}

FString UW_HoveringActor::GetHarvesterText(
	const URTSComponent* RTSComp,
	const UHarvester* HarvesterComp,
	bool& bOutIsValidString) const
{
	if (not IsValid(HarvesterComp))
	{
		return RTSComp->GetDisplayName(bOutIsValidString);
	}
	const FString UnitName = RTSComp->GetDisplayName(bOutIsValidString);
	int32 HarvesterAmount;
	ERTSResourceType ResourceType = ERTSResourceType::Resource_None;
	const int32 HarvesterCapacity = HarvesterComp->GetMaxCapacityForTargetResource(HarvesterAmount, ResourceType);
	return FRTSRichTextConverter::MakeRTSRich(UnitName, ERTSRichText::Text_Title) + "\n" +
		FRTSRichTextConverter::MakeRTSRich("Capacity: " + FString::FromInt(HarvesterCapacity),
		                                    ERTSRichText::Text_SubTitle) + "\n" +
		FRTSRichTextConverter::MakeRTSRich(
			Global_GetResourceTypeDisplayString(ResourceType) +
			": " + FString::FromInt(HarvesterAmount),
			ERTSRichText::Text_SubTitle);
}

void UW_HoveringActor::SetPaddingForText(const int32 PaddingText) const
{
	if (PaddingText <= 0)
	{
		return;
	}
	UBorderSlot* BorderSlot = Cast<UBorderSlot>(M_ActorInfoText->Slot);
	if (BorderSlot)
	{
		BorderSlot->SetPadding(PaddingText);
	}
}

void UW_HoveringActor::SetHoveredActor(AActor* HoveredActor)
{
	// Always remember the last hovered actor.
	M_CurrentHoveredActor = HoveredActor;

	// Global hide wins: keep hidden and don’t do any work.
	if (bM_WasHiddenByAllGameUI)
	{
		HideWidget();
		return;
	}

	if (not RTSFunctionLibrary::RTSIsValid(HoveredActor))
	{
		if constexpr (DeveloperSettings::Debugging::GMouseHover_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString("The provided hover actor is not valid or has no HP");
		}
		HideWidget();
		return;
	}

	bool bIsValidText = false;
	int32 PaddingText = 15; // basic default
	const FString ActorInfo = GetActorText(HoveredActor, bIsValidText, PaddingText);

	if (not bIsValidText)
	{
		if constexpr (DeveloperSettings::Debugging::GMouseHover_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString("The provided hover actor has no valid text");
		}
		HideWidget();
		return;
	}

	if (IsValid(M_ActorInfoText))
	{
		M_ActorInfoText->SetText(FText::FromString(ActorInfo));
		SetPaddingForText(PaddingText);
	}
	SetVisibility(ESlateVisibility::Visible);
}

void UW_HoveringActor::HideWidget()
{
	SetVisibility(ESlateVisibility::Hidden);
}

void UW_HoveringActor::OnHideAllGameUI(const bool bHide)
{
	if (bHide)
	{
		if (bM_WasHiddenByAllGameUI)
		{
			return;
		}
		bM_WasHiddenByAllGameUI = true;
		SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	// Un-hide
	if (not bM_WasHiddenByAllGameUI)
	{
		return;
	}
	bM_WasHiddenByAllGameUI = false;

	// If we were hovering something, re-evaluate immediately; otherwise stay hidden.
	if (M_CurrentHoveredActor.IsValid())
	{
		SetHoveredActor(M_CurrentHoveredActor.Get());
	}
	else
	{
		SetVisibility(ESlateVisibility::Hidden);
	}
}
