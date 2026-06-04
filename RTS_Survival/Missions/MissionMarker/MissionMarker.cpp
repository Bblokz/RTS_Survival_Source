#include "MissionMarker.h"

#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "W_MissionMarker/W_MissionMarker.h"

AMissionMarker::AMissionMarker(): M_ViewportSize()
{
	PrimaryActorTick.bCanEverTick = true;
	// You can tune this in your DeveloperSettings if you like
	PrimaryActorTick.TickInterval = DeveloperSettings::Optimization::UpdateMarkerWidgetInterval;
}

void AMissionMarker::BeginPlay()
{
	Super::BeginPlay();

	BeginPlay_SetViewPortSize();
	BeginPlay_CreateMarkerWidget();
}

void AMissionMarker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateScreenMarker();
}

bool AMissionMarker::EnsureMarkerWidgetClassIsValid() const
{
	if (IsValid(MarkerWidgetClass))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(
		"No valid MarkerWidgetClass set for MissionMarker '" + GetNameSafe(this) + "'.");
	return false;
}

bool AMissionMarker::GetIsValidMarkerWidget() const
{
	if (IsValid(M_MarkerWidget))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		TEXT("M_MarkerWidget"),
		TEXT("AMissionMarker::GetIsValidMarkerWidget"),
		this);
	return false;
}

void AMissionMarker::BeginPlay_SetViewPortSize()
{
	// Query the player's viewport size so we know our screen bounds
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (not IsValid(PlayerController))
	{
		RTSFunctionLibrary::ReportError(
			"Failed to get PlayerController in AMissionMarker for '" + GetNameSafe(this) + "'");
		return;
	}

	int32 ViewportSizeX = 0;
	int32 ViewportSizeY = 0;
	PlayerController->GetViewportSize(ViewportSizeX, ViewportSizeY);
	M_ViewportSize = FVector2D(ViewportSizeX, ViewportSizeY);
}

void AMissionMarker::BeginPlay_CreateMarkerWidget()
{
	if (not EnsureMarkerWidgetClassIsValid())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not World)
	{
		RTSFunctionLibrary::ReportError(
			"Failed to get world in AMissionMarker for '" + GetNameSafe(this) + "'");
		return;
	}

	M_MarkerWidget = CreateWidget<UW_MissionMarker>(World, MarkerWidgetClass);
	if (not GetIsValidMarkerWidget())
	{
		return;
	}

	// Start it at 0,0; we will reposition in Tick()
	M_MarkerWidget->AddToViewport();
}

void AMissionMarker::UpdateScreenMarker() const
{
	if (not GetIsValidMarkerWidget())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (not World)
	{
		return;
	}

	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(World, 0);
	if (not IsValid(PlayerController))
	{
		return;
	}

	if (M_ViewportSize.X <= 0.0f || M_ViewportSize.Y <= 0.0f)
	{
		return;
	}

	// 1) Project world → screen
	FVector2D ScreenPosition;
	const bool bOnScreen = PlayerController->ProjectWorldLocationToScreen(
		GetActorLocation(),
		ScreenPosition,
		/*bPlayerViewportRelative=*/true);

	// 2) If off-screen, clamp it to the edge along the direction from screen-center
	FVector2D ClampedPosition = ScreenPosition;
	const FVector2D ViewportCenter = M_ViewportSize * 0.5f;

	if (not bOnScreen
		|| ScreenPosition.X < 0.f
		|| ScreenPosition.X > M_ViewportSize.X
		|| ScreenPosition.Y < 0.f
		|| ScreenPosition.Y > M_ViewportSize.Y)
	{
		ClampedPosition = GetClampedOffscreenPosition(ScreenPosition, ViewportCenter);
	}

	// 3) Finally hand it off to the widget
	M_MarkerWidget->UpdateMarkerScreenPosition(ClampedPosition);
}

FVector2D AMissionMarker::GetClampedOffscreenPosition(const FVector2D& ScreenPosition,
                                                      const FVector2D& ViewportCenter) const
{
	// Build a unit-vector toward the projected point.
	const FVector2D DirectionToMarker = (ScreenPosition - ViewportCenter).GetSafeNormal();

	// Collect all intersection candidates with the viewport rectangle.
	TArray<TPair<float, FVector2D>> EdgeHits;
	AddMarkerEdgeHit(EdgeHits, DirectionToMarker, ViewportCenter, 0.f, true);
	AddMarkerEdgeHit(EdgeHits, DirectionToMarker, ViewportCenter, M_ViewportSize.X, true);
	AddMarkerEdgeHit(EdgeHits, DirectionToMarker, ViewportCenter, 0.f, false);
	AddMarkerEdgeHit(EdgeHits, DirectionToMarker, ViewportCenter, M_ViewportSize.Y, false);

	if (EdgeHits.Num() > 0)
	{
		EdgeHits.Sort([](const TPair<float, FVector2D>& FirstHit, const TPair<float, FVector2D>& SecondHit)
		{
			return FirstHit.Key < SecondHit.Key;
		});
		return EdgeHits[0].Value;
	}

	// Extremely unlikely fall-back.
	return FVector2D(
		FMath::Clamp(ScreenPosition.X, 0.f, M_ViewportSize.X),
		FMath::Clamp(ScreenPosition.Y, 0.f, M_ViewportSize.Y));
}

void AMissionMarker::AddMarkerEdgeHit(TArray<TPair<float, FVector2D>>& EdgeHits,
                                      const FVector2D& DirectionToMarker,
                                      const FVector2D& ViewportCenter,
                                      const float EdgeValue,
                                      const bool bClampX) const
{
	const float DirectionAxis = bClampX ? DirectionToMarker.X : DirectionToMarker.Y;
	if (FMath::IsNearlyZero(DirectionAxis))
	{
		return;
	}

	const float CenterAxis = bClampX ? ViewportCenter.X : ViewportCenter.Y;
	const float TimeToEdge = (EdgeValue - CenterAxis) / DirectionAxis;
	if (TimeToEdge <= 0.f)
	{
		return;
	}

	const FVector2D CandidatePosition = ViewportCenter + DirectionToMarker * TimeToEdge;
	const bool bIsInsideViewport = bClampX
		? CandidatePosition.Y >= 0.f && CandidatePosition.Y <= M_ViewportSize.Y
		: CandidatePosition.X >= 0.f && CandidatePosition.X <= M_ViewportSize.X;
	if (not bIsInsideViewport)
	{
		return;
	}

	EdgeHits.Emplace(TimeToEdge, CandidatePosition);
}
