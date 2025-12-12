// Copyright (C) Bas Blokzijl - All rights reserved.


#include "DigInWall.h"

#include "Engine/AssetManager.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/DigInComponent/DigInComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/DigInComponent/DigInProgress/W_DigInProgress.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/DigInComponent/DigInProgress/DigInProgressBarActor/DiginProgressBar.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"

ADigInWall::ADigInWall(const FObjectInitializer& ObjectInitializer):
	AHPActorObjectsMaster(ObjectInitializer),
	ConstructionMaterial(nullptr), WallMeshComponent(nullptr), M_WallMaterial(nullptr)
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
	// No errors when no RTSComponent is found.
	bDoNotUseRTSComponent = true;
}

void ADigInWall::StartBuildingWall(const float NewMaxHealth,
                                   const float BuildTime,
                                   const uint8 OwningPlayer,
                                   UDigInComponent* OwningDigInComponent, const FVector& ScalingFactor)
{
	using namespace DeveloperSettings::GameBalance::DigInWalls;

	SetupDigInComp(OwningDigInComponent);
	if (!GetIsValidHealthComponent() || !GetIsValidWallMeshComponent() || !EnsureConstructionMaterialIsValid())
	{
		RTSFunctionLibrary::ReportError("Cannot start building wall due to null references; destroying actor.");
		UnitDies(ERTSDeathType::Kinetic);
		return;
	}
	FRTS_CollisionSetup::SetupWallActorMeshCollision(WallMeshComponent, true, OwningPlayer);
	SetupWallMlt(ScalingFactor);
	// stash the real max and set starting HP
	MaxHealth = NewMaxHealth * M_WallMultiplier;
	const float StartHP = MaxHealth * DigInWallStartHPMlt;
	HealthComponent->SetMaxHealth(StartHP);

	// pick and apply mesh/material
	SetupRandomWallMesh();

	// kick off the build‐timer
	StartBuildTimer(BuildTime, DigInWallUpdateHealthDuringConstructionInterval, DigInWallStartHPMlt);
}

void ADigInWall::DestroyWall()
{
	UnitDies(ERTSDeathType::Kinetic);
}

void ADigInWall::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// Find static mesh component.
	WallMeshComponent = FindComponentByClass<UStaticMeshComponent>();
	// error check
	(void)GetIsValidWallMeshComponent();
}

void ADigInWall::BeginPlay()
{
	Super::BeginPlay();
}

void ADigInWall::BeginDestroy()
{
	Super::BeginDestroy();
	DestroyProgressBar();
}

void ADigInWall::UnitDies(const ERTSDeathType DeathType)
{
	if (not IsUnitAlive())
	{
		return;
	}
	SetUnitDying();
	OnUnitDies.Broadcast();
	if (EnsureIsValidDigInOwner())
	{
		M_OwningDigInComponent->OnWallDestroyed(this);
	}
	DestroyProgressBar();
	BP_OnUnitDies();
}


int32 ADigInWall::InterpolateDependingOnHealth(const int32 From, const int32 To) const
{
	// 0 to 1 float to interpoolate the ints with.
	const float HealthPercentage = GetIsValidHealthComponent() ? HealthComponent->GetHealthPercentage() : 1.0f;
	return FMath::RoundToInt(FMath::Lerp(static_cast<float>(From), static_cast<float>(To), HealthPercentage));
}

void ADigInWall::SetupRandomWallMesh()
{
	if (WallMeshOptions.IsEmpty())
	{
		RTSFunctionLibrary::ReportError(
			"cannot pick wall mesh as no options available!\n Actor: " + GetName()
		);
		Destroy();
		return;
	}

	// pick a random mesh soft reference
	const int32 Index = FMath::RandRange(0, WallMeshOptions.Num() - 1);
	const TSoftObjectPtr<UStaticMesh> SoftMesh = WallMeshOptions[Index];

	// If already loaded, set it immediately
	if (SoftMesh.IsValid())
	{
		OnLoadedWallMesh(SoftMesh.Get());
	}
	else
	{
		// otherwise request an async load, then invoke OnLoadedWallMesh once ready
		const FSoftObjectPath AssetPath = SoftMesh.ToSoftObjectPath();
		TWeakObjectPtr<ADigInWall> WeakThis(this);
		UAssetManager::GetStreamableManager().RequestAsyncLoad(
			AssetPath,
			FStreamableDelegate::CreateLambda([WeakThis, SoftMesh]()
			{
				if (not WeakThis.IsValid())
				{
					return;
				}
				if (UStaticMesh* Mesh = SoftMesh.Get())
				{
					WeakThis.Get()->OnLoadedWallMesh(Mesh);
					return;
				}
				RTSFunctionLibrary::ReportError(
					"Failed to load wall mesh asset. "
					"ADigInWall::SetupRandomWallMesh"
				);
			})
		);
	}
}


bool ADigInWall::GetIsValidWallMeshComponent() const
{
	if (IsValid(WallMeshComponent))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("No valid wall mesh component for actor: " + GetName() +
		"ADigInWall::GetIsValidWallMeshComponent");
	return false;
}

void ADigInWall::SetupDigInComp(UDigInComponent* OwningDigInComponent)
{
	M_OwningDigInComponent = OwningDigInComponent;
	(void)EnsureIsValidDigInOwner();
}

bool ADigInWall::EnsureIsValidDigInOwner() const
{
	if (M_OwningDigInComponent.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("No valid DigInComponent owner for actor: " + GetName() +
		"ADigInWall::EnsureIsValidDigInOwner");
	return false;
}

void ADigInWall::OnLoadedWallMesh(UStaticMesh* LoadedMesh)
{
	if (!LoadedMesh)
	{
		RTSFunctionLibrary::ReportError("DigInWall failed to load mesh. destroying actor."
			"\n actor: " + GetName());
		Destroy();
		return;
	}
	if (not GetIsValidWallMeshComponent() || not EnsureConstructionMaterialIsValid())
	{
		Destroy();
		return;
	}
	WallMeshComponent->SetStaticMesh(LoadedMesh);
	M_ChosenMesh = LoadedMesh;
	M_WallMaterial = WallMeshComponent->GetMaterial(0);
	WallMeshComponent->SetMaterial(0, ConstructionMaterial);
}

bool ADigInWall::EnsureConstructionMaterialIsValid() const
{
	if (IsValid(ConstructionMaterial))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("No valid construction material set in DEFAULTS of ADigInWall"
		"\n Actor: " + GetName() + "\n");
	return false;
}

bool ADigInWall::EnsureWallMaterialIsValid() const
{
	if (not IsValid(M_WallMaterial))
	{
		RTSFunctionLibrary::ReportError("No valid wall material set in ADigInWall"
			"\n Actor: " + GetName() + "\n");
		return false;
	}
	return true;
}

void ADigInWall::StartBuildTimer(const float BuildTime, const float Interval, const float StartHpMlt)
{
	M_TimeRemaining = BuildTime;

	// schedule recurring health updates
	GetWorld()->GetTimerManager().ClearTimer(M_WallBuildTimerHandle);
	GetWorld()->GetTimerManager().SetTimer(
		M_WallBuildTimerHandle,
		this,
		&ADigInWall::OnBuildInterval,
		Interval,
		true
	);
	CreateProgressBar(BuildTime, StartHpMlt);
}

void ADigInWall::OnBuildInterval()
{
	using namespace DeveloperSettings::GameBalance::DigInWalls;

	M_TimeRemaining -= DigInWallUpdateHealthDuringConstructionInterval;

	// compute completion ratio (clamped 0→1)
	const float RatioComplete = FMath::Clamp(
		1.0f - (M_TimeRemaining / (M_TimeRemaining + DigInWallUpdateHealthDuringConstructionInterval)),
		0.0f, 1.0f
	);

	// linearly interpolate HP from start→MaxHealth
	const float NewHP = FMath::Lerp(
		MaxHealth * DigInWallStartHPMlt,
		MaxHealth,
		RatioComplete
	);
	HealthComponent->SetMaxHealth(NewHP);

	if (M_TimeRemaining <= 0.0f)
	{
		GetWorld()->GetTimerManager().ClearTimer(M_WallBuildTimerHandle);
		HealthComponent->SetMaxHealth(MaxHealth);
		OnBuildingComplete();
	}
}

void ADigInWall::OnBuildingComplete()
{
	if (EnsureWallMaterialIsValid() && GetIsValidWallMeshComponent())
	{
		WallMeshComponent->SetMaterial(0, M_WallMaterial);
	}
	if (EnsureIsValidDigInOwner())
	{
		M_OwningDigInComponent->OnWallCompletedBuilding(this);
	}
	DestroyProgressBar();
}

void ADigInWall::CreateProgressBar(const float TotalTime, const float StartPercentage)
{
	if (!EnsureProgressBarClassIsValid())
	{
		return;
	}

	// If we already have a spawned progress-bar actor, just reuse it:
	if (M_SpawnedProgressBarActor.IsValid())
	{
		// Optionally, you could destroy the old one and respawn, but here
		// we assume only one build cycle at a time.
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		RTSFunctionLibrary::ReportError(TEXT("ADigInWall::CreateProgressBar: GetWorld() returned nullptr"));
		return;
	}

	// 1) Compute spawn location: this actor's location + the RelativeWidgetOffset
	const FVector SpawnLocation = GetActorLocation() + RelativeWidgetOffset;

	// 2) Spawn the ADiginProgressBar actor with no rotation (we'll rotate in Tick)
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = GetInstigator();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ADiginProgressBar* NewBarActor = World->SpawnActor<ADiginProgressBar>(
		ADiginProgressBar::StaticClass(),
		SpawnLocation,
		/*Rotation=*/ FRotator::ZeroRotator,
		SpawnParams
	);

	if (!IsValid(NewBarActor))
	{
		RTSFunctionLibrary::ReportError(TEXT("Failed to spawn ADiginProgressBar in CreateProgressBar"));
		return;
	}

	// 3) Keep a weak pointer so we can destroy it later when building completes
	M_SpawnedProgressBarActor = NewBarActor;

	// 4) Initialize the widget on that actor:
	NewBarActor->InitializeProgressBar(
		ProgressBarWidgetClass, // TSubclassOf<UW_DigInProgress>
		TotalTime, // total seconds to fill
		StartPercentage, // [0..1] initial fill
		ProgressBarSize // draw size in pixels
	);
}

void ADigInWall::DestroyProgressBar() const
{
	if (M_SpawnedProgressBarActor.IsValid())
	{
		M_SpawnedProgressBarActor->StopProgressBar();
		M_SpawnedProgressBarActor->Destroy();
	}
}


bool ADigInWall::EnsureProgressBarClassIsValid() const
{
	if (IsValid(ProgressBarWidgetClass))
	{
		return true;
	}
	const FString OwnerName = GetOwner() ? GetOwner()->GetName() : "Unknown";
	const FString DigInCompName = GetName();
	RTSFunctionLibrary::ReportError(
		FString::Printf(TEXT("No valid progress widget CLASS for DigInWall '%s' owned by '%s'."),
		                *DigInCompName, *OwnerName)
	);
	return false;
}

void ADigInWall::SetupWallMlt(const FVector& ScalingFactor)
{
	M_WallMultiplier = FMath::Max(ScalingFactor.X, FMath::Max(ScalingFactor.Y, ScalingFactor.Z));
}
