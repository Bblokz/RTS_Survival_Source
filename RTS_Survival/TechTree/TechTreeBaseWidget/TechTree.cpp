// Copyright (C) Bas Blokzijl - All rights reserved.

#include "TechTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/GameUI/MainGameUI.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/GameUI/Resources/WPlayerResource/W_PlayerResource.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UTechTree::NativeConstruct()
{
	Super::NativeConstruct();

	bIsPanning = false;

	// Ensure that TechTreeCanvas is valid
	if (not TechTreeCanvas)
	{
		UE_LOG(LogTemp, Error, TEXT("TechTreeCanvas is null in UTechTree::NativeConstruct"));
		return;
	}

	// Set the size of the TechTreeCanvas
	FVector2D TechTreeSize(DeveloperSettings::UIUX::TechTree::TechTreeWidth,
	                       DeveloperSettings::UIUX::TechTree::TechTreeHeight);
	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(TechTreeCanvas->Slot);
	if (CanvasSlot)
	{
		CanvasSlot->SetSize(TechTreeSize);
		// Start at top-left 
		CanvasSlot->SetPosition(FVector2D(0.f, 0.f));
	}

	// Initialize panning bounds
	InitializePanBounds();
}

void UTechTree::InitializePanBounds()
{
	// Get viewport size (player's screen dimensions)
	FVector2D ViewportSize;
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}
	else
	{
		ViewportSize = FVector2D(1920.f, 1080.f); // Default to 1080p if viewport size can't be obtained
	}

	// Calculate panning limits
	// The maximum negative offset is the size of the TechTreeCanvas minus the viewport size
	// Since we start from (0,0), MinPanPosition is negative values (moving the canvas to the left/up)
	MinPanPosition = FVector2D(
		FMath::Min(0.f, ViewportSize.X - DeveloperSettings::UIUX::TechTree::TechTreeWidth),
		FMath::Min(0.f, ViewportSize.Y - DeveloperSettings::UIUX::TechTree::TechTreeHeight)
	);

	MaxPanPosition = FVector2D(0.f, 0.f);
}

FReply UTechTree::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bIsPanning = true;
		LastMousePosition = InMouseEvent.GetScreenSpacePosition();
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UTechTree::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bIsPanning)
	{
		FVector2D CurrentMousePosition = InMouseEvent.GetScreenSpacePosition();
		FVector2D Delta = CurrentMousePosition - LastMousePosition;

		// Apply speed multiplier
		Delta *= DeveloperSettings::UIUX::TechTree::PanSpeed;

		// Adjust the target position of the TechTreeCanvas
		TargetTranslation += Delta;

		// Clamp TargetTranslation to the allowed bounds
		TargetTranslation.X = FMath::Clamp(TargetTranslation.X, MinPanPosition.X, MaxPanPosition.X);
		TargetTranslation.Y = FMath::Clamp(TargetTranslation.Y, MinPanPosition.Y, MaxPanPosition.Y);

		LastMousePosition = CurrentMousePosition;

		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UTechTree::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bIsPanning && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bIsPanning = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void UTechTree::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (not IsValid(TechTreeCanvas))
	{
		return;
	}

	// Smoothly interpolate towards the target translation
	CurrentTranslation = FMath::Vector2DInterpTo(
		CurrentTranslation,
		TargetTranslation,
		InDeltaTime,
		DeveloperSettings::UIUX::TechTree::PanSmoothness
	);

	// Apply the new translation
	FWidgetTransform NewTransform = TechTreeCanvas->GetRenderTransform();
	NewTransform.Translation = CurrentTranslation;
	TechTreeCanvas->SetRenderTransform(NewTransform);

	if (IsValid(ResourcesCanvas))
	{
		// Set ResourcesCanvas's Translation to the negative of CurrentTranslation
		FWidgetTransform ResourceTransform = ResourcesCanvas->GetRenderTransform();
		ResourceTransform.Translation = -CurrentTranslation;
		ResourcesCanvas->SetRenderTransform(ResourceTransform);
	}
}

void UTechTree::SetupResources(TArray<UW_PlayerResource*> TResourceWidgets)
{
	TArray<ERTSResourceType> ResourcesLeftToRight = {
		ERTSResourceType::Resource_Radixite,
		ERTSResourceType::Resource_Metal,
		ERTSResourceType::Resource_VehicleParts,
		ERTSResourceType::Resource_Fuel,
		ERTSResourceType::Resource_Ammo,

		// second line; blueprints
		ERTSResourceType::Blueprint_Weapon,
		ERTSResourceType::Blueprint_Vehicle,
		ERTSResourceType::Blueprint_Energy,
		ERTSResourceType::Blueprint_Construction
	};

	const int32 NumResourceTypes = ResourcesLeftToRight.Num();
	const int32 NumResourceWidgets = TResourceWidgets.Num();
	if (NumResourceWidgets != NumResourceTypes)
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("UTechTree::SetupResources expects %d widgets but received %d."),
			NumResourceTypes,
			NumResourceWidgets));
	}

	if (not GetIsValidPlayerController())
	{
		return;
	}

	ACPPController* PlayerController = M_PlayerController.Get();
	if (not IsValid(PlayerController))
	{
		RTSFunctionLibrary::ReportError("Player controller invalid during UTechTree::SetupResources.");
		return;
	}

	UPlayerResourceManager* PlayerResourceManager = PlayerController->GetPlayerResourceManager();
	if (not IsValid(PlayerResourceManager))
	{
		RTSFunctionLibrary::ReportError("Could not access UPlayerResourceManager for UTechTree::SetupResources.");
		return;
	}

	const int32 NumToInitialize = FMath::Min(NumResourceWidgets, NumResourceTypes);
	for (int32 ResourceIndex = 0; ResourceIndex < NumToInitialize; ++ResourceIndex)
	{
		if (not IsValid(TResourceWidgets[ResourceIndex]))
		{
			RTSFunctionLibrary::ReportError(FString::Printf(
				TEXT("UTechTree::SetupResources widget at index %d is invalid."),
				ResourceIndex));
			continue;
		}
		TResourceWidgets[ResourceIndex]->InitUwPlayerResource(PlayerResourceManager,
		                                                     ResourcesLeftToRight[ResourceIndex]);
	}
}

bool UTechTree::GetIsValidPlayerController()
{
	if (M_PlayerController.IsValid())
	{
		return true;
	}

	M_PlayerController = Cast<ACPPController>(UGameplayStatics::GetPlayerController(this, 0));
	if (M_PlayerController.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_PlayerController",
	                                                             "GetIsValidPlayerController",
	                                                             this);
	return false;
}

bool UTechTree::GetIsValidMainGameUI() const
{
	if (M_MainGameUI.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_MainGameUI", "GetIsValidMainGameUI", this);
	return false;
}

void UTechTree::GoBackToMainGameUI()
{
	SetVisibility(ESlateVisibility::Collapsed);

	// Re-enable world rendering
	UGameViewportClient* ViewportClient = GetWorld()->GetGameViewport();
	if (ViewportClient)
	{
		ViewportClient->bDisableWorldRendering = false;
	}

	if (GetIsValidMainGameUI())
	{
		if (UMainGameUI* MainGameUI = M_MainGameUI.Get())
		{
			MainGameUI->OnCloseTechTree();
		}
	}
}

void UTechTree::BeginDestroy()
{
	Super::BeginDestroy();
	if constexpr (DeveloperSettings::Debugging::GTechTree_Compile_DebugSymbols)
	{
		RTSFunctionLibrary::PrintString("Tech tree destroyed!");
	}
}
