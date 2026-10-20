#include "HoverCommandIconComponent.h"

#include "HoverCommandPrediction.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalIcons/AnimatedIconDataAsset.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalIcons/AnimatedIconWidgetPoolManager.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalIcons/AnimatedIconWorldSubsystem.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalIcons/RTSVerticalAnimatedIcon.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace HoverCommandIconConstants
{
	// Target health and cargo vacancy change without the hovered actor changing.
	constexpr double PredictionRefreshIntervalSeconds = 0.2;
	// Slate units from the cursor hotspot; the icon sits above-right so it does not cover the hover description,
	// which opens below-right of the cursor.
	const FVector2D CursorOffset(18.0, -4.0);
	// Anchors the icon's bottom-left corner on the offset point.
	const FVector2D IconAlignment(0.0, 1.0);
	// Above the regular HUD so the icon is never drawn behind panels near the cursor.
	constexpr int32 IconViewportZOrder = 100;
}

UHoverCommandIconComponent::UHoverCommandIconComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHoverCommandIconComponent::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_CreateCommandIconWidget();
}

void UHoverCommandIconComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(M_CommandIconWidget))
	{
		M_CommandIconWidget->RemoveFromParent();
	}
	M_CommandIconWidget = nullptr;
	Super::EndPlay(EndPlayReason);
}

void UHoverCommandIconComponent::BeginPlay_CreateCommandIconWidget()
{
	APlayerController* OwningPlayerController = Cast<APlayerController>(GetOwner());
	if (not IsValid(OwningPlayerController) || not OwningPlayerController->IsLocalController())
	{
		return;
	}
	M_CommandIconWidget = CreateWidget<UW_RTSVerticalAnimatedIcon>(
		OwningPlayerController, UW_RTSVerticalAnimatedIcon::StaticClass());
	if (not GetIsValidCommandIconWidget())
	{
		return;
	}
	M_CommandIconWidget->AddToViewport(HoverCommandIconConstants::IconViewportZOrder);
	M_CommandIconWidget->SetAlignmentInViewport(HoverCommandIconConstants::IconAlignment);
	M_CommandIconWidget->SetDormant();
}

void UHoverCommandIconComponent::UpdateCommandIcon(
	const UPlayerCommandTypeDecoder& CommandTypeDecoder,
	AActor* HoveredActor,
	AActor* PrimarySelectedUnit,
	const FVector2D& MouseViewportPosition)
{
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	const double NowRealTimeSeconds = World->GetRealTimeSeconds();
	if (GetShouldRefreshPrediction(HoveredActor, PrimarySelectedUnit, NowRealTimeSeconds))
	{
		RefreshPrediction(CommandTypeDecoder, HoveredActor, PrimarySelectedUnit, NowRealTimeSeconds);
		DisplayPredictedIcon();
	}
	if (M_DisplayedIcon == ERTSVerticalAnimatedIcon::None)
	{
		return;
	}
	PositionIconAtCursor(MouseViewportPosition);
}

void UHoverCommandIconComponent::HideCommandIcon()
{
	// Forget the prediction so the next hover evaluates at once instead of waiting for the refresh interval.
	M_PredictionCache = FHoverCommandIconPredictionCache();
	if (M_DisplayedIcon == ERTSVerticalAnimatedIcon::None)
	{
		return;
	}
	M_DisplayedIcon = ERTSVerticalAnimatedIcon::None;
	if (GetIsValidCommandIconWidget())
	{
		M_CommandIconWidget->SetDormant();
	}
}

bool UHoverCommandIconComponent::GetShouldRefreshPrediction(const AActor* HoveredActor,
                                                            const AActor* PrimarySelectedUnit,
                                                            const double NowRealTimeSeconds) const
{
	if (not M_PredictionCache.bHasPrediction)
	{
		return true;
	}
	if (M_PredictionCache.HoveredActor.Get() != HoveredActor
		|| M_PredictionCache.PrimarySelectedUnit.Get() != PrimarySelectedUnit)
	{
		return true;
	}
	const double SecondsSinceEvaluation = NowRealTimeSeconds - M_PredictionCache.EvaluatedAtRealTimeSeconds;
	return SecondsSinceEvaluation >= HoverCommandIconConstants::PredictionRefreshIntervalSeconds;
}

void UHoverCommandIconComponent::RefreshPrediction(const UPlayerCommandTypeDecoder& CommandTypeDecoder,
                                                   AActor* HoveredActor, AActor* PrimarySelectedUnit,
                                                   const double NowRealTimeSeconds)
{
	M_PredictionCache.HoveredActor = HoveredActor;
	M_PredictionCache.PrimarySelectedUnit = PrimarySelectedUnit;
	M_PredictionCache.EvaluatedAtRealTimeSeconds = NowRealTimeSeconds;
	M_PredictionCache.PredictedIcon = FHoverCommandPrediction::PredictCommandIcon(
		CommandTypeDecoder, HoveredActor, PrimarySelectedUnit);
	M_PredictionCache.bHasPrediction = true;
}

void UHoverCommandIconComponent::DisplayPredictedIcon()
{
	const ERTSVerticalAnimatedIcon PredictedIcon = M_PredictionCache.PredictedIcon;
	if (PredictedIcon == M_DisplayedIcon || not GetIsValidCommandIconWidget())
	{
		return;
	}
	M_DisplayedIcon = ERTSVerticalAnimatedIcon::None;
	M_CommandIconWidget->SetDormant();
	if (PredictedIcon == ERTSVerticalAnimatedIcon::None)
	{
		return;
	}

	FRTSVerticalAnimatedIconDefinition IconDefinition;
	const UAnimatedIconWidgetPoolManager* IconCatalog = GetIconCatalog();
	if (not IsValid(IconCatalog) || not IconCatalog->TryGetCatalogIconDefinition(PredictedIcon, IconDefinition))
	{
		// No artwork in the catalog: this command is deliberately not previewed.
		return;
	}
	if (not M_CommandIconWidget->ActivateIcon(IconDefinition, IconDefinition.Texture.Get()))
	{
		return;
	}
	M_DisplayedIcon = PredictedIcon;
}

void UHoverCommandIconComponent::PositionIconAtCursor(const FVector2D& MouseViewportPosition) const
{
	if (not GetIsValidCommandIconWidget())
	{
		return;
	}
	// The mouse position is in viewport pixels while the offset is in Slate units.
	const float ViewportScale = UWidgetLayoutLibrary::GetViewportScale(M_CommandIconWidget);
	const FVector2D IconPixelPosition = MouseViewportPosition + HoverCommandIconConstants::CursorOffset * ViewportScale;
	constexpr bool bRemoveDPIScale = true;
	M_CommandIconWidget->SetPositionInViewport(IconPixelPosition, bRemoveDPIScale);
}

UAnimatedIconWidgetPoolManager* UHoverCommandIconComponent::GetIconCatalog() const
{
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return nullptr;
	}
	const UAnimatedIconWorldSubsystem* IconSubsystem = World->GetSubsystem<UAnimatedIconWorldSubsystem>();
	if (not IsValid(IconSubsystem))
	{
		return nullptr;
	}
	return IconSubsystem->GetAnimatedIconWidgetPoolManager();
}

bool UHoverCommandIconComponent::GetIsValidCommandIconWidget() const
{
	if (IsValid(M_CommandIconWidget))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_CommandIconWidget"),
		TEXT("GetIsValidCommandIconWidget"),
		this);
	return false;
}
