// Copyright (C) Bas Blokzijl - All rights reserved.

#include "DiginProgressBar.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/DigInComponent/DigInProgress/W_DigInProgress.h"

ADiginProgressBar::ADiginProgressBar()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// Create a simple SceneComponent as the root
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// Create the WidgetComponent, attach to SceneRoot
	ProgressWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("ProgressWidgetComponent"));
	ProgressWidgetComponent->SetupAttachment(SceneRoot);

	// Default settings for a world-space widget:
	ProgressWidgetComponent->SetWidgetSpace(EWidgetSpace::World);
	ProgressWidgetComponent->SetDrawAtDesiredSize(false);
	ProgressWidgetComponent->SetTranslucentSortPriority(0);
	ProgressWidgetComponent->SetRelativeLocation(FVector::ZeroVector);
}

void ADiginProgressBar::BeginPlay()
{
	Super::BeginPlay();
	// Nothing else to do until InitializeProgressBar is called
}

void ADiginProgressBar::BeginDestroy()
{
	Super::BeginDestroy();
}

void ADiginProgressBar::InitializeProgressBar(
	TSubclassOf<UW_DigInProgress> InWidgetClass,
	float InTotalTime,
	float InStartPercent,
	const FVector2D& InDrawSize
)
{
	if (!IsValid(InWidgetClass))
	{
		UE_LOG(LogTemp, Error, TEXT("ADiginProgressBar::InitializeProgressBar: Invalid WidgetClass"));
		return;
	}

	WidgetClass = InWidgetClass;
	TotalTime = InTotalTime;
	StartPercent = FMath::Clamp(InStartPercent, 0.0f, 1.0f);
	DrawSize = InDrawSize;
	bIsInitialized = true;

	ProgressWidgetComponent->SetDrawSize(DrawSize);

	ProgressWidgetComponent->SetWidgetClass(WidgetClass);
	ProgressWidgetComponent->InitWidget();

	GetWorld()->GetTimerManager().SetTimerForNextTick(
		[this]()
		{
			FinishInitializeWidget();
		}
	);
}

void ADiginProgressBar::FinishInitializeWidget()
{
	if (!bIsInitialized)
	{
		return;
	}

	UW_DigInProgress* ProgressWidget = Cast<UW_DigInProgress>(
		ProgressWidgetComponent->GetUserWidgetObject()
	);
	if (!ProgressWidget)
	{
		UE_LOG(LogTemp, Error, TEXT("ADiginProgressBar: Failed to cast to UW_DigInProgress"));
		// If the cast fails, destroy ourselves to avoid a stray actor
		Destroy();
		return;
	}

	// Finally call FillProgressBar so the UW_DigInProgress starts ticking
	ProgressWidget->FillProgressBar(StartPercent, TotalTime, this, DrawSize);
}

void ADiginProgressBar::StopProgressBar()
{
	if (ProgressWidgetComponent)
	{
		UW_DigInProgress* ProgressWidget = Cast<UW_DigInProgress>(
			ProgressWidgetComponent->GetUserWidgetObject()
		);
		if (ProgressWidget)
		{
			ProgressWidget->StopProgressBar();
		}
	}
	SetActorTickEnabled(false);
}

void ADiginProgressBar::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Only rotate if we've initialized the widget and we still have a valid component
	if (bIsInitialized && IsValid(ProgressWidgetComponent))
	{
		// 1) Get the local player camera location
		APlayerController* PC = GetWorld()->GetFirstPlayerController();
		if (!PC || !PC->PlayerCameraManager)
		{
			return;
		}
		const FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();

		// 2) Get the widget component’s world location
		const FVector WidgetLocation = ProgressWidgetComponent->GetComponentLocation();

		// 3) Compute a look-at rotation so that the +X axis of the widget quad faces the camera
		const FRotator LookAtRotation = (CameraLocation - WidgetLocation).Rotation();

		// 4) Optionally zero out pitch & roll so the bar stays upright
		FRotator NewRotation = LookAtRotation;
		NewRotation.Pitch = 0.0f;
		NewRotation.Roll = 0.0f;

		// 5) Apply rotation to the widget component
		ProgressWidgetComponent->SetWorldRotation(NewRotation);
	}
}
