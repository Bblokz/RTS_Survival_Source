// Copyright (C) Bas Blokzijl

#include "W_FormationPicker.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void UW_FormationPicker::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (!FormationImage)
    {
        RTSFunctionLibrary::ReportError(TEXT("FormationImage is not bound."));
        return;
    }
    if (!RadialMaterial)
    {
        RTSFunctionLibrary::ReportError(TEXT("RadialMaterial is not assigned."));
        return;
    }

    RadialMaterialInstance = UMaterialInstanceDynamic::Create(RadialMaterial, this);
    if (!RadialMaterialInstance)
    {
        RTSFunctionLibrary::ReportError(TEXT("Failed to create dynamic material instance."));
        return;
    }
    FormationImage->SetBrushFromMaterial(RadialMaterialInstance);

    AddToViewport(30);
    bIsActive = false;
    SetVisibility(ESlateVisibility::Collapsed);
}

bool UW_FormationPicker::Activate(const FVector2D& /*InMousePosition*/)
{
    if (FormationTypes.Num() == 0)
    {
        RTSFunctionLibrary::ReportError(TEXT("FormationTypes array is empty."));
        return false;
    }
    if (!RadialMaterialInstance)
    {
        RTSFunctionLibrary::ReportError(TEXT("RadialMaterialInstance is null."));
        return false;
    }

    SegmentCount = FormationTypes.Num();
    RadialMaterialInstance->SetScalarParameterValue(TEXT("Segments"), SegmentCount);

    FVector2D ViewportSize(0, 0);
    if (GetWorld() && GetWorld()->GetGameViewport())
    {
        GetWorld()->GetGameViewport()->GetViewportSize(ViewportSize);
    }
    OriginPosition = ViewportSize * 0.5f;

    FocusedIndex = 0;
    RadialMaterialInstance->SetScalarParameterValue(TEXT("Index"), FocusedIndex);

    bIsActive = true;
    SetVisibility(ESlateVisibility::Visible);
    return true;
}

void UW_FormationPicker::Deactivate()
{
    bIsActive = false;
    SetVisibility(ESlateVisibility::Collapsed);
}

void UW_FormationPicker::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!bIsActive || SegmentCount <= 0)
    {
        return;
    }

    APlayerController* PC = GetOwningPlayer();
    if (!PC) return;

    FVector2D MousePos;
    if (!PC->GetMousePosition(MousePos.X, MousePos.Y))
    {
        return;
    }

    FVector2D Delta = MousePos - OriginPosition;
    float Distance = Delta.Size();
    if (Distance < SphereInnerRadius)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Red,
                TEXT("FormationPicker: In deadzone"));
        }
        return;
    }

    // **Invert Y here** so that screen +Y (down) becomes game +Y (up)
    float RawAngle = FMath::Atan2(Delta.Y, Delta.X);  // <— changed

    // Rotate by +180° (π), then rotate so zero at top, wrap into [0..2π)
    float Angle = RawAngle + (PI / 2);
    Angle = FMath::Fmod(Angle + 2 * PI, 2 * PI);

    const float SegmentAngle = 2.f * PI / SegmentCount;
    int32 NewIndex = FMath::Clamp(int32(Angle / SegmentAngle), 0, SegmentCount - 1);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Green,
            FString::Printf(TEXT("Raw=%.2f  Adj=%.2f  Idx=%d"),
                RawAngle, Angle, NewIndex));
    }

    if (NewIndex != FocusedIndex)
    {
        FocusedIndex = NewIndex;
        RadialMaterialInstance->SetScalarParameterValue(TEXT("Index"), FocusedIndex);
    }
}

EFormation UW_FormationPicker::GetFormationFromPrimaryClick(const FVector2D& /*ClickedPosition*/)
{
    if (!bIsActive || SegmentCount <= 0)
    {
        RTSFunctionLibrary::ReportError(TEXT("Select called when widget was inactive or invalid."));
        return EFormation::RectangleFormation;
    }

    EFormation Chosen = FormationTypes.IsValidIndex(FocusedIndex)
        ? FormationTypes[FocusedIndex]
        : EFormation::RectangleFormation;

    if (!FormationTypes.IsValidIndex(FocusedIndex))
    {
        RTSFunctionLibrary::ReportError(
            FString::Printf(TEXT("Invalid formation index %d"), FocusedIndex));
    }

    Deactivate();
    return Chosen;
}
