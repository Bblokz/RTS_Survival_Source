#include "ScavRewardComponent.h"
#include "RTS_Survival/Scavenging/ScavengeRewardWidget/ScavengeRewardWidget.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Player/Camera/CameraPawn.h"

UScavRewardComponent::UScavRewardComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	TickWhenOffscreen = true;

	SetDrawSize(FVector2D(500.f, 200.f)); // Adjust as needed
	SetPivot(FVector2D(0.5f, 0.5f));
	SetTwoSided(true);


	// Initialize durations
	M_VisibleDuration = 3.0f; // First 3 seconds fully visible
	M_FadeDuration = 2.0f; // Last 2 seconds fade out
	M_TotalLifetime = M_VisibleDuration + M_FadeDuration;
}

void UScavRewardComponent::StartDisplay()
{
	if (IsValid(M_RewardWidget))
	{
		// Start the fade out timer
		GetWorld()->GetTimerManager().SetTimer(FadeOutTimerHandle, this, &UScavRewardComponent::StartFadeOut,
		                                       M_VisibleDuration, false);
		GetWorld()->GetTimerManager().SetTimer(DestroyTimerHandle, this, &UScavRewardComponent::DestroySelf,
		                                       M_TotalLifetime, false);
	}
	else
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "M_RewardWidget", "StartDisplay");
	}
}

void UScavRewardComponent::SetRewardText(const FText& InText)
{
	if (IsValid(M_RewardWidget))
	{
		M_RewardWidget->SetRewardText(InText);
	}
}

void UScavRewardComponent::SetFadeDuration(const float InFadeDuration)
{
	M_FadeDuration = InFadeDuration;
	M_TotalLifetime = M_VisibleDuration + M_FadeDuration;
}

void UScavRewardComponent::SetVisibleDuration(const float InVisibleDuration)
{
	M_VisibleDuration = InVisibleDuration;
	M_TotalLifetime = M_VisibleDuration + M_FadeDuration;
}

void UScavRewardComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!WidgetClass)
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "WidgetClass", "BeginPlay");
	}
	else
	{
		if (DeveloperSettings::Debugging::GScavenging_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString("This is the widget class: " + WidgetClass->GetName(), FColor::Purple);
		}
	}

	M_RewardWidget = Cast<UScavengeRewardWidget>(GetUserWidgetObject());
	if (!IsValid(M_RewardWidget))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "M_RewardWidget", "BeginPlay");
		return;
	}
	// Set widget in screen space:
	SetWidgetSpace(EWidgetSpace::Screen);
	
	// if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
	// {
	// 	if (const ACameraPawn* PlayerCameraPawn = Cast<ACameraPawn>(Pawn))
	// 	{
	// 		if (UCameraComponent* CameraComponent = PlayerCameraPawn->GetCameraComponent())
	// 		{
	// 			M_PlayerCameraPawn = CameraComponent;
	// 			return;
	// 		}
	// 	}
	// 	
	// }
	// RTSFunctionLibrary::ReportNullErrorInitialisation(this, "M_PlayerCameraPawn", "BeginPlay");
}

// void UScavRewardComponent::TickComponent(float DeltaTime, ELevelTick TickType,
//                                          FActorComponentTickFunction* ThisTickFunction)
// {
// 	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
// 	if (IsValid(M_PlayerCameraPawn) )
// 	{
// 		const FRotator Rotation = UKismetMathLibrary::FindLookAtRotation(GetComponentLocation(),
// 																			  M_PlayerCameraPawn->
// 																			  GetComponentLocation());
// 		SetWorldRotation(Rotation);
// 	}
// }

void UScavRewardComponent::StartFadeOut()
{
	if (IsValid(M_RewardWidget))
	{
		M_RewardWidget->StartFadeOut(M_FadeDuration);
	}
}

void UScavRewardComponent::DestroySelf()
{
	// Destroy the component
	this->DestroyComponent();
}
