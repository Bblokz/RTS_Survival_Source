// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "W_IdleUnitButton.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "Styling/CoreStyle.h"
#include "UObject/UObjectIterator.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Resources/Harvester/Harvester.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace IdleUnitButtonConstants
{
	constexpr float RefreshIntervalSeconds = 0.5f;
	constexpr uint8 PlayerOwnerIndex = 1;
	constexpr int32 CountFontSize = 12;
	const FMargin CountPadding(0.0f, 0.0f, 3.0f, 0.0f);
	const FVector2D TextShadowOffset(1.0f, 1.0f);
	const FLinearColor TextShadowColor(0.0f, 0.0f, 0.0f, 0.85f);
	const FLinearColor DisabledTint(0.35f, 0.35f, 0.35f, 0.9f);
}

void UW_IdleUnitButton::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BuildWidgetTree();
}

void UW_IdleUnitButton::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshIdleCount();
	UWorld* const World = GetWorld();
	if (not IsValid(World) || IsDesignTime())
	{
		return;
	}
	World->GetTimerManager().SetTimer(
		M_RefreshTimerHandle, this, &UW_IdleUnitButton::RefreshIdleCount,
		IdleUnitButtonConstants::RefreshIntervalSeconds, true);
}

void UW_IdleUnitButton::NativeDestruct()
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_RefreshTimerHandle);
	}
	Super::NativeDestruct();
}

void UW_IdleUnitButton::BuildWidgetTree()
{
	if (not IsValid(WidgetTree) || IsValid(WidgetTree->RootWidget))
	{
		return;
	}
	USizeBox* const SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	SizeBox->SetWidthOverride(M_ButtonSize);
	SizeBox->SetHeightOverride(M_ButtonSize);
	WidgetTree->RootWidget = SizeBox;

	UOverlay* const Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	SizeBox->SetContent(Overlay);

	M_Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	M_Button->SetStyle(MakeGlassButtonStyle());
	M_Button->OnClicked.AddDynamic(this, &UW_IdleUnitButton::OnIdleButtonClicked);
	UOverlaySlot* const ButtonSlot = Overlay->AddChildToOverlay(M_Button);
	ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
	ButtonSlot->SetVerticalAlignment(VAlign_Fill);

	M_CountText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	M_CountText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", IdleUnitButtonConstants::CountFontSize));
	M_CountText->SetShadowOffset(IdleUnitButtonConstants::TextShadowOffset);
	M_CountText->SetShadowColorAndOpacity(IdleUnitButtonConstants::TextShadowColor);
	M_CountText->SetVisibility(ESlateVisibility::HitTestInvisible);
	UOverlaySlot* const CountSlot = Overlay->AddChildToOverlay(M_CountText);
	CountSlot->SetHorizontalAlignment(HAlign_Right);
	CountSlot->SetVerticalAlignment(VAlign_Bottom);
	CountSlot->SetPadding(IdleUnitButtonConstants::CountPadding);
}

FButtonStyle UW_IdleUnitButton::MakeGlassButtonStyle() const
{
	FButtonStyle Style;
	Style.SetNormal(MakeGlassBrush(M_NormalTexture, FLinearColor::White));
	Style.SetHovered(MakeGlassBrush(M_HoveredTexture ? M_HoveredTexture : M_NormalTexture, FLinearColor::White));
	Style.SetPressed(MakeGlassBrush(M_PressedTexture ? M_PressedTexture : M_NormalTexture, FLinearColor::White));
	Style.SetDisabled(MakeGlassBrush(M_NormalTexture, IdleUnitButtonConstants::DisabledTint));
	Style.SetNormalPadding(FMargin(0.0f));
	Style.SetPressedPadding(FMargin(0.0f));
	return Style;
}

FSlateBrush UW_IdleUnitButton::MakeGlassBrush(UTexture2D* Texture, const FLinearColor& Tint) const
{
	FSlateBrush Brush;
	Brush.SetResourceObject(Texture);
	Brush.ImageSize = FVector2D(M_ButtonSize, M_ButtonSize);
	Brush.DrawAs = Texture ? ESlateBrushDrawType::Image : ESlateBrushDrawType::NoDrawType;
	Brush.TintColor = FSlateColor(Tint);
	return Brush;
}

void UW_IdleUnitButton::RefreshIdleCount()
{
	if (not GetIsValidButton() || not GetIsValidCountText())
	{
		return;
	}
	TArray<AActor*> IdleUnits;
	if (not IsDesignTime())
	{
		GatherIdleUnits(IdleUnits);
	}
	const int32 IdleCount = IdleUnits.Num();
	M_Button->SetIsEnabled(IdleCount > 0);
	M_Button->SetToolTipText(GetTooltipText(IdleCount));
	M_CountText->SetText(FText::AsNumber(IdleCount));
	M_CountText->SetVisibility(IdleCount > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UW_IdleUnitButton::OnIdleButtonClicked()
{
	ACPPController* const PlayerController = Cast<ACPPController>(GetOwningPlayer());
	if (not IsValid(PlayerController))
	{
		return;
	}
	TArray<AActor*> IdleUnits;
	GatherIdleUnits(IdleUnits);
	if (IdleUnits.IsEmpty())
	{
		RefreshIdleCount();
		return;
	}
	AActor* const NextIdleUnit = IdleUnits[M_CycleIndex % IdleUnits.Num()];
	M_CycleIndex = (M_CycleIndex + 1) % IdleUnits.Num();
	PlayerController->SelectSingleUnitFromUI(NextIdleUnit);
	PlayerController->FocusCameraOnActor(NextIdleUnit);
}

void UW_IdleUnitButton::GatherIdleUnits(TArray<AActor*>& OutIdleUnits) const
{
	if (M_IdleUnitType == EIdleUnitButtonType::Harvester)
	{
		GatherIdleHarvesters(OutIdleUnits);
		return;
	}
	GatherIdleNomadicTrucks(OutIdleUnits);
}

void UW_IdleUnitButton::GatherIdleHarvesters(TArray<AActor*>& OutIdleUnits) const
{
	const UWorld* const World = GetWorld();
	for (TObjectIterator<UHarvester> It; It; ++It)
	{
		if (It->GetWorld() != World)
		{
			continue;
		}
		AActor* const HarvesterActor = It->GetOwner();
		if (RTSFunctionLibrary::RTSIsValid(HarvesterActor) && GetIsOwnedByPlayer(*HarvesterActor)
			&& GetIsCommandIdle(*HarvesterActor))
		{
			OutIdleUnits.Add(HarvesterActor);
		}
	}
}

void UW_IdleUnitButton::GatherIdleNomadicTrucks(TArray<AActor*>& OutIdleUnits) const
{
	UWorld* const World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	for (TActorIterator<ANomadicVehicle> It(World); It; ++It)
	{
		ANomadicVehicle* const NomadicVehicle = *It;
		if (RTSFunctionLibrary::RTSIsValid(NomadicVehicle) && NomadicVehicle->GetIsInTruckMode()
			&& GetIsOwnedByPlayer(*NomadicVehicle) && GetIsCommandIdle(*NomadicVehicle))
		{
			OutIdleUnits.Add(NomadicVehicle);
		}
	}
}

bool UW_IdleUnitButton::GetIsOwnedByPlayer(const AActor& Actor)
{
	const URTSComponent* const RTSComponent = Actor.FindComponentByClass<URTSComponent>();
	return IsValid(RTSComponent) && RTSComponent->GetOwningPlayer() == IdleUnitButtonConstants::PlayerOwnerIndex;
}

bool UW_IdleUnitButton::GetIsCommandIdle(AActor& Actor)
{
	ICommands* const Commands = Cast<ICommands>(&Actor);
	return Commands != nullptr && Commands->GetIsUnitIdle();
}

FText UW_IdleUnitButton::GetTooltipText(const int32 IdleCount) const
{
	const TCHAR* const UnitName = M_IdleUnitType == EIdleUnitButtonType::Harvester
		                              ? TEXT("harvesters")
		                              : TEXT("nomadic trucks");
	return FText::FromString(FString::Printf(TEXT("Idle %s: %d (click to cycle)"), UnitName, IdleCount));
}

bool UW_IdleUnitButton::GetIsValidButton() const
{
	if (IsValid(M_Button))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, TEXT("M_Button"), TEXT("UW_IdleUnitButton::GetIsValidButton"), this);
	return false;
}

bool UW_IdleUnitButton::GetIsValidCountText() const
{
	if (IsValid(M_CountText))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, TEXT("M_CountText"), TEXT("UW_IdleUnitButton::GetIsValidCountText"), this);
	return false;
}
