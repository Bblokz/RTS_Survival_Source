#include "MissionMarker.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
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
    RTSFunctionLibrary::ReportError(
        "MarkerWidget is not valid for MissionMarker '" + GetNameSafe(this) + "'.");
    return false;
}

void AMissionMarker::BeginPlay_SetViewPortSize()
{
    // Query the player's viewport size so we know our screen bounds
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        int32 X, Y;
        PC->GetViewportSize(X, Y);
        M_ViewportSize = FVector2D(X, Y);
    }
    else
    {
        RTSFunctionLibrary::ReportError(
            "Failed to get PlayerController in AMissionMarker for '" + GetNameSafe(this) + "'");
    }
}

void AMissionMarker::BeginPlay_CreateMarkerWidget()
{
    if (!EnsureMarkerWidgetClassIsValid())
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World) return;

    M_MarkerWidget = CreateWidget<UW_MissionMarker>(World, MarkerWidgetClass);
    if (!GetIsValidMarkerWidget())
    {
        return;
    }

    // Start it at 0,0; we will reposition in Tick()
    M_MarkerWidget->AddToViewport();
}

void AMissionMarker::UpdateScreenMarker() const
{
    if (!GetIsValidMarkerWidget())
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World) return;

    APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
    if (!PC) return;

    // 1) Project world → screen
    FVector2D ScreenPos;
    const bool bOnScreen = PC->ProjectWorldLocationToScreen(GetActorLocation(), ScreenPos, /*bPlayerViewportRelative=*/true);

    // 2) If off-screen, clamp it to the edge along the direction from screen-center
    FVector2D ClampedPos = ScreenPos;
    const FVector2D Center = M_ViewportSize * 0.5f;

    if (!bOnScreen
        || ScreenPos.X < 0.f
        || ScreenPos.X > M_ViewportSize.X
        || ScreenPos.Y < 0.f
        || ScreenPos.Y > M_ViewportSize.Y)
    {
        // build a unit-vector toward the projected point
        FVector2D Dir = (ScreenPos - Center).GetSafeNormal();

        // collect all intersection candidates with the viewport rectangle
        TArray<TPair<float, FVector2D>> Hits;
        auto TryEdge = [&](float EdgeX, float EdgeY, bool bClampX)
        {
            float t = bClampX
                ? (EdgeX - Center.X) / Dir.X
                : (EdgeY - Center.Y) / Dir.Y;
            if (t <= 0.f) return;
            FVector2D P = Center + Dir * t;
            if (bClampX)
            {
                if (P.Y >= 0.f && P.Y <= M_ViewportSize.Y)
                {
                    Hits.Emplace(t, P);
                }
            }
            else
            {
                if (P.X >= 0.f && P.X <= M_ViewportSize.X)
                {
                    Hits.Emplace(t, P);
                }
            }
        };

        // edges: X=0, X=Max, Y=0, Y=Max
        if (!FMath::IsNearlyZero(Dir.X))
        {
            TryEdge(0.f,   0.f,                 /*clampX=*/true);
            TryEdge(M_ViewportSize.X, 0.f,      /*clampX=*/true);
        }
        if (!FMath::IsNearlyZero(Dir.Y))
        {
            TryEdge(0.f,   0.f,                 /*clampX=*/false);
            TryEdge(0.f,   M_ViewportSize.Y,   /*clampX=*/false);
        }

        if (Hits.Num())
        {
            Hits.Sort([](auto& A, auto& B){ return A.Key < B.Key; });
            ClampedPos = Hits[0].Value;
        }
        else
        {
            // extremely unlikely fall-back
            ClampedPos.X = FMath::Clamp(ScreenPos.X, 0.f, M_ViewportSize.X);
            ClampedPos.Y = FMath::Clamp(ScreenPos.Y, 0.f, M_ViewportSize.Y);
        }
    }

    // 3) Finally hand it off to the widget
    M_MarkerWidget->UpdateMarkerScreenPosition(ClampedPos);
}
