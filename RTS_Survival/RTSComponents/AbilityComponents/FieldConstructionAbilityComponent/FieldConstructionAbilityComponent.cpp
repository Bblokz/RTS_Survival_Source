// Copyright (C) Bas Blokzijl - All rights reserved.


#include "FieldConstructionAbilityComponent.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/KismetMathLibrary.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/Abilities.h"
#include "RTS_Survival/Player/ConstructionPreview/StaticMeshPreview/StaticPreviewMesh.h"
#include "RTS_Survival/GameUI/Pooled_TimedProgressBars/Pooling/Manager/RTSTimedProgressBarManager.h"
#include "RTS_Survival/GameUI/Pooled_TimedProgressBars/Pooling/WorldSubSystem/RTSTimedProgressBarWorldSubsystem.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/FieldConstructionAbilityComponent/FieldConstructionActor/FieldConstruction.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/Reinforcement/SquadReinforcementComponent.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Squads/SquadUnit/AnimSquadUnit/SquadUnitAnimInstance.h"
#include "RTS_Survival/Weapons/InfantryWeapon/InfantryWeaponMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"


UFieldConstructionAbilityComponent::UFieldConstructionAbilityComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UFieldConstructionAbilityComponent::AddStaticPreviewWaitingForConstruction(AStaticPreviewMesh* StaticPreview)
{
	M_StaticPreviewsWaitingForConstruction.Add(StaticPreview);
}


void UFieldConstructionAbilityComponent::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_SetOwningSquadController();
	BeginPlay_SetFieldTypeOnConstructionData();
	BeginPlay_ValidateSettings();
	BeginPlay_CacheTimedProgressBarManager();
	BeginPlay_AddAbility();
}

void UFieldConstructionAbilityComponent::BeginPlay_SetFieldTypeOnConstructionData()
{
	// This needs to be saved for the player preview mesh system so we can provide back what type of field construction is placed after a valid placement.
	FieldConstructionAbilitySettings.FieldConstructionData.FieldConstructionType = FieldConstructionAbilitySettings.
		FieldConstructionType;
}

void UFieldConstructionAbilityComponent::BeginPlay_SetOwningSquadController()
{
	if (not GetOwner())
	{
		RTSFunctionLibrary::ReportError("Owner missing for FieldConstructionAbilityComponent.");
		return;
	}
	M_OwningSquadController = Cast<ASquadController>(GetOwner());
	(void)GetIsValidSquadController();
}

void UFieldConstructionAbilityComponent::BeginPlay_ValidateSettings() const
{
	if (FieldConstructionAbilitySettings.Cooldown < 0)
	{
		RTSFunctionLibrary::ReportError("Field construction ability has negative cooldown configured.");
	}

	if (FieldConstructionAbilitySettings.ConstructionTime <= 0.f)
	{
		RTSFunctionLibrary::ReportError(
			"Field construction ability has non-positive base construction time configured.");
	}

	if (not FieldConstructionAbilitySettings.FieldConstructionClass)
	{
		RTSFunctionLibrary::ReportError("Field construction ability is missing FieldConstructionClass.");
	}

	if (not FieldConstructionAbilitySettings.PreviewMesh)
	{
		RTSFunctionLibrary::ReportError("Field construction ability is missing PreviewMesh.");
	}
}

void UFieldConstructionAbilityComponent::BeginPlay_CacheTimedProgressBarManager()
{
	UWorld* World = GetWorld();
	if (not World)
	{
		RTSFunctionLibrary::ReportError(
			"World missing while caching timed progress bar manager for field construction component.");
		return;
	}

	URTSTimedProgressBarWorldSubsystem* ProgressBarSubsystem = World->GetSubsystem<URTSTimedProgressBarWorldSubsystem>();
	if (not IsValid(ProgressBarSubsystem))
	{
		RTSFunctionLibrary::ReportError("Failed to access timed progress bar world subsystem.");
		return;
	}

	M_TimedProgressBarManager = ProgressBarSubsystem->GetTimedProgressBarManager();
	(void)GetIsValidTimedProgressBarManager();
}

void UFieldConstructionAbilityComponent::BeginPlay_AddAbility()
{
	if (not GetOwner())
	{
		return;
	}
	ASquadController* SquadController = Cast<ASquadController>(GetOwner());
	if (SquadController)
	{
		AddAbilityToSquad(SquadController);
		return;
	}
	if (UWorld* World = GetWorld())
	{
		TWeakObjectPtr<UFieldConstructionAbilityComponent> WeakThis(this);
		FTimerDelegate Del;
		Del.BindLambda([WeakThis]()
		{
			if (not WeakThis.IsValid())
			{
				return;
			}
			WeakThis->AddAbilityToCommands();
		});
		World->GetTimerManager().SetTimerForNextTick(Del);
	}
}

void UFieldConstructionAbilityComponent::AddAbilityToCommands()
{
	if (not GetIsValidCommandsInterface())
	{
		return;
	}

	FUnitAbilityEntry NewAbility;
	NewAbility.AbilityId = EAbilityID::IdFieldConstruction;
	NewAbility.CooldownDuration = FieldConstructionAbilitySettings.Cooldown;
	NewAbility.CustomType = static_cast<int32>(FieldConstructionAbilitySettings.FieldConstructionType);

	M_OwningSquadController->AddAbility(NewAbility, FieldConstructionAbilitySettings.PreferredAbilityIndex);
}

void UFieldConstructionAbilityComponent::AddAbilityToSquad(ASquadController* Squad)
{
	TWeakObjectPtr<UFieldConstructionAbilityComponent> WeakThis(this);
	auto ApplyLambda = [WeakThis]() -> void
	{
		if (not WeakThis.IsValid())
		{
			return;
		}
		WeakThis->AddAbilityToCommands();
	};
	Squad->SquadDataCallbacks.CallbackOnSquadDataLoaded(ApplyLambda, WeakThis);
}

bool UFieldConstructionAbilityComponent::GetIsValidCommandsInterface() const
{
	if (not GetIsValidSquadController())
	{
		return false;
	}

	return true;
}

bool UFieldConstructionAbilityComponent::GetIsValidSquadController() const
{
	if (IsValid(M_OwningSquadController))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_OwningSquadController",
		"UFieldConstructionAbilityComponent::GetIsValidSquadController");
	return false;
}

bool UFieldConstructionAbilityComponent::GetIsValidTimedProgressBarManager() const
{
	if (M_TimedProgressBarManager.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_TimedProgressBarManager",
		"UFieldConstructionAbilityComponent::GetIsValidTimedProgressBarManager");
	return false;
}

void UFieldConstructionAbilityComponent::ExecuteFieldConstruction(const FVector& ConstructionLocation,
                                                                  const FRotator& ConstructionRotation,
                                                                  AStaticPreviewMesh* StaticPreviewActor)
{
	if (not GetIsValidSquadController())
	{
		RTSFunctionLibrary::ReportError("Cannot execute field construction without a valid squad controller.");
		return;
	}

	ResetConstructionState();

	M_ActiveConstructionState.M_TargetLocation = ConstructionLocation;
	M_ActiveConstructionState.M_TargetRotation = ConstructionRotation;
	M_ActiveConstructionState.M_PreviewActor = StaticPreviewActor;
	M_ActiveConstructionState.M_ConstructionType = FieldConstructionAbilitySettings.FieldConstructionType;
	M_ActiveConstructionState.M_CurrentPhase = EFieldConstructionAbilityPhase::MovingToLocation;

	StartMovementToConstruction();
}

void UFieldConstructionAbilityComponent::TerminateFieldConstructionCommand(AActor* StaticPreviewActor)
{
	StopConstructionRangeCheckTimer();
	StopConstructionDurationTimer();

	if (M_ActiveConstructionState.M_CurrentPhase == EFieldConstructionAbilityPhase::MovingToLocation)
	{
		DestroyPreviewActor(StaticPreviewActor);
		DestroyPreviewActor(M_ActiveConstructionState.M_PreviewActor.IsValid()
			                    ? M_ActiveConstructionState.M_PreviewActor.Get()
			                    : nullptr);
		if (GetIsValidSquadController())
		{
			M_OwningSquadController->TerminateMoveCommand();
		}
		ResetConstructionState();
		return;
	}

	if (M_ActiveConstructionState.M_CurrentPhase == EFieldConstructionAbilityPhase::Constructing)
	{
		if (M_ActiveConstructionState.M_SpawnedConstruction.IsValid())
		{
			M_ActiveConstructionState.M_SpawnedConstruction->StopConstructionMaterialTimer();
		}
		DestroyPreviewActor(StaticPreviewActor);
		DisableSquadWeapons(false);
		RemoveEquipmentFromSquad();
		StopConstructionAnimation();
		StopConstructionProgressBar();
		ResetConstructionState();
		return;
	}

	DestroyPreviewActor(StaticPreviewActor);
	ResetConstructionState();
}

void UFieldConstructionAbilityComponent::StartMovementToConstruction()
{
	if (not GetIsValidSquadController())
	{
		ReportError_InvalidConstructionState("StartMovementToConstruction");
		return;
	}

	M_OwningSquadController->StartFieldConstructionMove(M_ActiveConstructionState.M_TargetLocation);

	if (UWorld* World = GetWorld())
	{
		constexpr float RangeCheckIntervalSeconds = 0.15f;
		World->GetTimerManager().SetTimer(
			M_FieldConstructionRangeCheckHandle,
			this,
			&UFieldConstructionAbilityComponent::OnUnitCloseEnoughForConstruction,
			RangeCheckIntervalSeconds,
			true);
	}
}

void UFieldConstructionAbilityComponent::OnUnitCloseEnoughForConstruction()
{
	if (GetAnySquadUnitWithinConstructionRange())
	{
		StopConstructionRangeCheckTimer();
		StartConstructionPhase();
	}
}

bool UFieldConstructionAbilityComponent::GetAnySquadUnitWithinConstructionRange() const
{
	if (not GetIsValidSquadController())
	{
		return false;
	}

	using DeveloperSettings::GamePlay::Navigation::SquadUnitFieldConstructionDistance;
	const TArray<ASquadUnit*> SquadUnits = M_OwningSquadController->GetSquadUnitsChecked();

	for (ASquadUnit* SquadUnit : SquadUnits)
	{
		if (not IsValid(SquadUnit))
		{
			continue;
		}

		const float DistanceToConstruction = FVector::Dist(M_ActiveConstructionState.M_TargetLocation,
		                                                   SquadUnit->GetActorLocation());
		if (DistanceToConstruction <= SquadUnitFieldConstructionDistance)
		{
			return true;
		}
	}

	return false;
}

void UFieldConstructionAbilityComponent::StartConstructionPhase()
{
	M_ActiveConstructionState.M_CurrentPhase = EFieldConstructionAbilityPhase::Constructing;
	M_ActiveConstructionState.M_ConstructionDuration = CalculateConstructionDurationSeconds();

	if (GetIsValidSquadController())
	{
		M_OwningSquadController->TerminateMoveCommand();
	}

	AFieldConstruction* SpawnedConstruction = SpawnFieldConstructionActor();
	if (not IsValid(SpawnedConstruction))
	{
		DisableSquadWeapons(false);
		RemoveEquipmentFromSquad();
		StopConstructionAnimation();
		DestroyPreviewActor(M_ActiveConstructionState.M_PreviewActor.IsValid()
			                    ? M_ActiveConstructionState.M_PreviewActor.Get()
			                    : nullptr);
		if (GetIsValidSquadController())
		{
			M_OwningSquadController->DoneExecutingCommand(EAbilityID::IdFieldConstruction);
		}
		ResetConstructionState();
		return;
	}

	UStaticMeshComponent* PreviewMeshComponent =
		GetPreviewMeshComponent(M_ActiveConstructionState.M_PreviewActor.Get());
	SetupSpawnedConstruction(SpawnedConstruction, PreviewMeshComponent);
	DestroyPreviewActor(M_ActiveConstructionState.M_PreviewActor.IsValid()
		                    ? M_ActiveConstructionState.M_PreviewActor.Get()
		                    : nullptr);

	DisableSquadWeapons(true);
	AddEquipmentToSquad();
	PlayConstructionAnimation();

	StartConstructionProgressBar(SpawnedConstruction);
	StartConstructionTimer();
}

AFieldConstruction* UFieldConstructionAbilityComponent::SpawnFieldConstructionActor()
{
	if (not GetIsValidSquadController())
	{
		ReportError_InvalidConstructionState("SpawnFieldConstructionActor");
		return nullptr;
	}

	if (not FieldConstructionAbilitySettings.FieldConstructionClass)
	{
		RTSFunctionLibrary::ReportError("Cannot spawn field construction without FieldConstructionClass.");
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (not World)
	{
		RTSFunctionLibrary::ReportError("World missing while spawning field construction actor.");
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = M_OwningSquadController;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	SpawnParams.bNoFail = true;

	AFieldConstruction* SpawnedConstruction = World->SpawnActor<AFieldConstruction>(
		FieldConstructionAbilitySettings.FieldConstructionClass,
		M_ActiveConstructionState.M_TargetLocation,
		M_ActiveConstructionState.M_TargetRotation,
		SpawnParams);

	if (not IsValid(SpawnedConstruction))
	{
		RTSFunctionLibrary::ReportError("Failed to spawn field construction actor.");
		return nullptr;
	}

	SpawnedConstruction->OnDestroyed.AddUniqueDynamic(
		this, &UFieldConstructionAbilityComponent::OnConstructionActorDestroyed);
	M_ActiveConstructionState.M_SpawnedConstruction = SpawnedConstruction;
	return SpawnedConstruction;
}

UStaticMeshComponent* UFieldConstructionAbilityComponent::GetPreviewMeshComponent(AActor* StaticPreviewActor) const
{
	if (not IsValid(StaticPreviewActor))
	{
		return nullptr;
	}

	AStaticPreviewMesh* PreviewMeshActor = Cast<AStaticPreviewMesh>(StaticPreviewActor);
	if (not IsValid(PreviewMeshActor))
	{
		return nullptr;
	}

	return PreviewMeshActor->FindComponentByClass<UStaticMeshComponent>();
}

void UFieldConstructionAbilityComponent::SetupSpawnedConstruction(AFieldConstruction* SpawnedConstruction,
                                                                  UStaticMeshComponent* PreviewMeshComponent)
{
	if (not IsValid(SpawnedConstruction))
	{
		return;
	}
	SpawnedConstruction->InitialiseFromPreview(PreviewMeshComponent, M_ActiveConstructionState.M_ConstructionDuration);
}

void UFieldConstructionAbilityComponent::StartConstructionTimer()
{
	if (M_ActiveConstructionState.M_ConstructionDuration <= 0.f)
	{
		FinishConstruction();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			M_FieldConstructionDurationHandle,
			this,
			&UFieldConstructionAbilityComponent::FinishConstruction,
			M_ActiveConstructionState.M_ConstructionDuration,
			false);
	}
}

void UFieldConstructionAbilityComponent::StartConstructionProgressBar(AFieldConstruction* SpawnedConstruction)
{
	if (M_ActiveConstructionState.M_ProgressBarActivationID != 0)
	{
		StopConstructionProgressBar();
	}

	if (not GetIsValidTimedProgressBarManager())
	{
		return;
	}

	if (not IsValid(SpawnedConstruction))
	{
		return;
	}

	USceneComponent* AnchorComponent = SpawnedConstruction->GetRootComponent();
	if (not IsValid(AnchorComponent))
	{
		return;
	}

	URTSTimedProgressBarManager* ProgressBarManager = M_TimedProgressBarManager.Get();
	if (not IsValid(ProgressBarManager))
	{
		return;
	}

	const FVector AttachOffset = FVector::ZeroVector;
	constexpr float RatioStart = 0.f;
	constexpr bool bUsePercentageText = true;
	constexpr bool bUseDescriptionText = false;
	constexpr float ProgressBarScale = 1.f;

	M_ActiveConstructionState.M_ProgressBarActivationID = ProgressBarManager->ActivateTimedProgressBarAnchored(
		AnchorComponent,
		AttachOffset,
		RatioStart,
		M_ActiveConstructionState.M_ConstructionDuration,
		bUsePercentageText,
		ERTSProgressBarType::Default,
		bUseDescriptionText,
		TEXT(""),
		ProgressBarScale);
}

void UFieldConstructionAbilityComponent::StopConstructionProgressBar()
{
	if (M_ActiveConstructionState.M_ProgressBarActivationID == 0)
	{
		return;
	}

	if (not GetIsValidTimedProgressBarManager())
	{
		M_ActiveConstructionState.M_ProgressBarActivationID = 0;
		return;
	}

	URTSTimedProgressBarManager* ProgressBarManager = M_TimedProgressBarManager.Get();
	if (not IsValid(ProgressBarManager))
	{
		M_ActiveConstructionState.M_ProgressBarActivationID = 0;
		return;
	}

	ProgressBarManager->ForceProgressBarDormant(M_ActiveConstructionState.M_ProgressBarActivationID);
	M_ActiveConstructionState.M_ProgressBarActivationID = 0;
}

float UFieldConstructionAbilityComponent::CalculateConstructionDurationSeconds() const
{
	const float BaseTime = FieldConstructionAbilitySettings.ConstructionTime;
	const int32 AliveUnits = GetAliveSquadUnitCount();
	if (AliveUnits <= 0)
	{
		RTSFunctionLibrary::ReportError("No squad units alive to construct field construction.");
		return BaseTime;
	}

	const int32 MaxUnits = GetMaxSquadUnitCount();
	if (MaxUnits <= 0)
	{
		RTSFunctionLibrary::ReportError("Invalid max squad unit count while calculating construction duration.");
		return BaseTime;
	}

	const float ConstructionScale = static_cast<float>(MaxUnits) / static_cast<float>(AliveUnits);
	return BaseTime * ConstructionScale;
}

int32 UFieldConstructionAbilityComponent::GetAliveSquadUnitCount() const
{
	if (not GetIsValidSquadController())
	{
		return 0;
	}
	return M_OwningSquadController->GetSquadUnitsChecked().Num();
}

int32 UFieldConstructionAbilityComponent::GetMaxSquadUnitCount() const
{
	if (not GetIsValidSquadController())
	{
		return 0;
	}
	TObjectPtr<USquadReinforcementComponent> ReinforcementComponent = M_OwningSquadController->
		GetSquadReinforcementComponent();
	if (not IsValid(ReinforcementComponent))
	{
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(
			this,
			"SquadReinforcement",
			"UFieldConstructionAbilityComponent::GetMaxSquadUnitCount",
			M_OwningSquadController);
		return M_OwningSquadController->GetSquadUnitsChecked().Num();
	}
	return ReinforcementComponent->GetMaxSquadUnits();
}

void UFieldConstructionAbilityComponent::FinishConstruction()
{
	StopConstructionDurationTimer();
	StopConstructionProgressBar();
	SpawnCompletionEffect();
	DisableSquadWeapons(false);
	RemoveEquipmentFromSquad();
	StopConstructionAnimation();
	M_ActiveConstructionState.M_CurrentPhase = EFieldConstructionAbilityPhase::Completed;

	if (GetIsValidSquadController())
	{
		M_OwningSquadController->DoneExecutingCommand(EAbilityID::IdFieldConstruction);
	}
	else
	{
		ReportError_InvalidConstructionState("FinishConstruction");
	}
}

void UFieldConstructionAbilityComponent::SpawnCompletionEffect() const
{
	if (not IsValid(FieldConstructionAbilitySettings.OnCompletionEffect))
	{
		return;
	}

	if (not M_ActiveConstructionState.M_SpawnedConstruction.IsValid())
	{
		return;
	}

	const FVector CompletionLocation = M_ActiveConstructionState.M_SpawnedConstruction->GetActorLocation();
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		this,
		FieldConstructionAbilitySettings.OnCompletionEffect,
		CompletionLocation,
		M_ActiveConstructionState.M_TargetRotation);
}

void UFieldConstructionAbilityComponent::AddEquipmentToSquad()
{
	if (M_FieldConstructionEquipment.Num() > 0)
	{
		return;
	}

	if (not IsValid(FieldConstructionEquipmentSettings.FieldConstructEquipment))
	{
		return;
	}

	if (not GetIsValidSquadController())
	{
		ReportError_InvalidConstructionState("AddEquipmentToSquad");
		return;
	}

	const TArray<ASquadUnit*> SquadUnits = M_OwningSquadController->GetSquadUnitsChecked();
	for (ASquadUnit* SquadUnit : SquadUnits)
	{
		if (not IsValid(SquadUnit) || not IsValid(SquadUnit->GetMesh()))
		{
			continue;
		}

		UStaticMeshComponent* EquipmentMesh = NewObject<UStaticMeshComponent>(SquadUnit);
		if (not IsValid(EquipmentMesh))
		{
			RTSFunctionLibrary::ReportError("Failed to create field construction equipment mesh.");
			continue;
		}

		EquipmentMesh->SetStaticMesh(FieldConstructionEquipmentSettings.FieldConstructEquipment);
		EquipmentMesh->AttachToComponent(
			SquadUnit->GetMesh(),
			FAttachmentTransformRules::SnapToTargetIncludingScale,
			FieldConstructionEquipmentSettings.FieldConstructSocketName);
		EquipmentMesh->RegisterComponent();
		EquipmentMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		EquipmentMesh->SetCanEverAffectNavigation(false);

		UNiagaraComponent* EquipmentEffect = nullptr;
		if (IsValid(FieldConstructionEquipmentSettings.FieldConstructEffect))
		{
			EquipmentEffect = UNiagaraFunctionLibrary::SpawnSystemAttached(
				FieldConstructionEquipmentSettings.FieldConstructEffect,
				EquipmentMesh,
				FieldConstructionEquipmentSettings.FieldConstructEffectSocketName,
				FVector::ZeroVector,
				FRotator::ZeroRotator,
				EAttachLocation::KeepRelativeOffset,
				true);
			if (IsValid(EquipmentEffect))
			{
				EquipmentEffect->SetAutoDestroy(true);
			}
		}

		FFieldConstructionUnitEquipmentState EquipmentState;
		EquipmentState.M_SquadUnit = SquadUnit;
		EquipmentState.M_EquipmentMesh = EquipmentMesh;
		EquipmentState.M_EquipmentEffect = EquipmentEffect;
		M_FieldConstructionEquipment.Add(EquipmentState);
	}
}

void UFieldConstructionAbilityComponent::RemoveEquipmentFromSquad()
{
	for (FFieldConstructionUnitEquipmentState& EquipmentState : M_FieldConstructionEquipment)
	{
		if (IsValid(EquipmentState.M_EquipmentEffect))
		{
			EquipmentState.M_EquipmentEffect->DestroyComponent();
			EquipmentState.M_EquipmentEffect = nullptr;
		}

		if (IsValid(EquipmentState.M_EquipmentMesh))
		{
			EquipmentState.M_EquipmentMesh->DestroyComponent();
			EquipmentState.M_EquipmentMesh = nullptr;
		}
	}

	M_FieldConstructionEquipment.Reset();
}

void UFieldConstructionAbilityComponent::DisableSquadWeapons(const bool bDisable) const
{
	if (not GetIsValidSquadController())
	{
		ReportError_InvalidConstructionState("DisableSquadWeapons");
		return;
	}

	const TArray<ASquadUnit*> SquadUnits = M_OwningSquadController->GetSquadUnitsChecked();
	for (ASquadUnit* SquadUnit : SquadUnits)
	{
		if (not IsValid(SquadUnit))
		{
			continue;
		}

		AInfantryWeaponMaster* InfantryWeapon = SquadUnit->GetInfantryWeapon();
		if (not IsValid(InfantryWeapon))
		{
			continue;
		}

		if (bDisable)
		{
			InfantryWeapon->DisableWeaponSearch(true, true);
		}
		else
		{
			InfantryWeapon->SetAutoEngageTargets(true);
		}
	}
}

void UFieldConstructionAbilityComponent::PlayConstructionAnimation() const
{
	if (not GetIsValidSquadController())
	{
		ReportError_InvalidConstructionState("PlayConstructionAnimation");
		return;
	}

	for (ASquadUnit* SquadUnit : M_OwningSquadController->GetSquadUnitsChecked())
	{
		if (not IsValid(SquadUnit))
		{
			continue;
		}

		FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(
			SquadUnit->GetActorLocation(),
			M_ActiveConstructionState.M_TargetLocation);
		LookAtRotation.Pitch = 0.0f;
		LookAtRotation.Roll = 0.0f;
		SquadUnit->SetActorRotation(LookAtRotation);

		USquadUnitAnimInstance* AnimInstance = SquadUnit->GetAnimBP_SquadUnit();
		if (AnimInstance)
		{
			AnimInstance->PlayWeldingMontage();
		}
	}
}

void UFieldConstructionAbilityComponent::StopConstructionAnimation() const
{
	if (not GetIsValidSquadController())
	{
		return;
	}

	for (ASquadUnit* SquadUnit : M_OwningSquadController->GetSquadUnitsChecked())
	{
		if (not IsValid(SquadUnit))
		{
			continue;
		}

		USquadUnitAnimInstance* AnimInstance = SquadUnit->GetAnimBP_SquadUnit();
		if (AnimInstance)
		{
			AnimInstance->StopAllMontages();
		}
	}
}

void UFieldConstructionAbilityComponent::DestroyPreviewActor(AActor* StaticPreviewActor) const
{
	if (IsValid(StaticPreviewActor))
	{
		StaticPreviewActor->Destroy();
	}
}

void UFieldConstructionAbilityComponent::OnConstructionActorDestroyed(AActor* DestroyedActor)
{
	if (M_ActiveConstructionState.M_CurrentPhase != EFieldConstructionAbilityPhase::Constructing)
	{
		return;
	}

	StopConstructionDurationTimer();
	DisableSquadWeapons(false);
	RemoveEquipmentFromSquad();
	StopConstructionAnimation();
	StopConstructionProgressBar();
	M_ActiveConstructionState.M_CurrentPhase = EFieldConstructionAbilityPhase::Completed;

	if (GetIsValidSquadController())
	{
		M_OwningSquadController->DoneExecutingCommand(EAbilityID::IdFieldConstruction);
	}

	ResetConstructionState();
}

void UFieldConstructionAbilityComponent::ResetConstructionState()
{
	StopConstructionProgressBar();
	StopConstructionRangeCheckTimer();
	StopConstructionDurationTimer();
	M_FieldConstructionEquipment.Reset();
	M_ActiveConstructionState.Reset();
}

void UFieldConstructionAbilityComponent::StopConstructionRangeCheckTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_FieldConstructionRangeCheckHandle);
	}
}

void UFieldConstructionAbilityComponent::StopConstructionDurationTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_FieldConstructionDurationHandle);
	}
}

void UFieldConstructionAbilityComponent::ReportError_InvalidConstructionState(const FString& ErrorContext) const
{
	const FString ContextMessage = "Field construction component in invalid state at: " + ErrorContext;
	RTSFunctionLibrary::ReportError(ContextMessage);
}
