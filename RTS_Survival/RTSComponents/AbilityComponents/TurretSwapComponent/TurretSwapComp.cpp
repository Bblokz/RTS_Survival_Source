#include "TurretSwapComp.h"

#include "Components/ChildActorComponent.h"
#include "RTS_Survival/Behaviours/BehaviourComp.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/GameUI/Pooled_TimedProgressBars/Pooling/Manager/RTSTimedProgressBarManager.h"
#include "RTS_Survival/GameUI/Pooled_TimedProgressBars/Pooling/WorldSubSystem/RTSTimedProgressBarWorldSubsystem.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/Abilities.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"

UTurretSwapComp::UTurretSwapComp()
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UTurretSwapComp::InitTurretSwapComponent(UChildActorComponent* ChildActorComponent)
{
	M_TurretChildActorComponent = ChildActorComponent;
	if (not GetIsValidChildActorComponent())
	{
		return;
	}

	ACPPTurretsMaster* CurrentTurret = Cast<ACPPTurretsMaster>(M_TurretChildActorComponent->GetChildActor());
	if (not IsValid(CurrentTurret))
	{
		RTSFunctionLibrary::ReportError("UTurretSwapComp::InitTurretSwapComponent expects a turret child actor.");
	}
}

void UTurretSwapComp::ExecuteTurretSwap()
{
	if (not GetIsValidChildActorComponent() || not GetIsValidSwapTurretClass())
	{
		if (GetIsValidOwnerCommandsInterface())
		{
			M_OwnerCommandsInterface->DoneExecutingCommand(EAbilityID::IdSwapTurret);
		}
		return;
	}

	ACPPTurretsMaster* CurrentTurret = Cast<ACPPTurretsMaster>(M_TurretChildActorComponent->GetChildActor());
	if (not IsValid(CurrentTurret))
	{
		RTSFunctionLibrary::ReportError(
			"UTurretSwapComp::ExecuteTurretSwap failed, current child actor is not a turret.");
		if (GetIsValidOwnerCommandsInterface())
		{
			M_OwnerCommandsInterface->DoneExecutingCommand(EAbilityID::IdSwapTurret);
		}
		return;
	}

	CurrentTurret->DisableTurret();
	ApplyMaterialOverrideToTurret(CurrentTurret);
	StartSwapProgressAndTimer();
}

void UTurretSwapComp::TerminateTurretSwap()
{
	StopProgressBar();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_SwapTimerHandle);
	}
}

ETurretSwapAbility UTurretSwapComp::GetCurrentActiveSwapAbilityType() const
{
	return M_TurretSwapAbilitySettings.ActiveSwapAbilityType;
}

void UTurretSwapComp::BeginPlay()
{
	Super::BeginPlay();

	AActor* OwnerActor = GetOwner();
	if (not IsValid(OwnerActor))
	{
		return;
	}

	if (OwnerActor->GetClass()->ImplementsInterface(UCommands::StaticClass()))
	{
		M_OwnerCommandsInterface.SetObject(OwnerActor);
		M_OwnerCommandsInterface.SetInterface(Cast<ICommands>(OwnerActor));
	}

	UWorld* const World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	URTSTimedProgressBarWorldSubsystem* const TimedProgressBarSubsystem =
		World->GetSubsystem<URTSTimedProgressBarWorldSubsystem>();

	if (not IsValid(TimedProgressBarSubsystem))
	{
		// RTSFunctionLibrary::ReportError(TEXT("TimedProgressBarSubsystem missing"));
		return;
	}

	URTSTimedProgressBarManager* const TimedProgressBarManager = TimedProgressBarSubsystem->
		GetTimedProgressBarManager();
	if (not IsValid(TimedProgressBarManager))
	{
		// RTSFunctionLibrary::ReportError(TEXT("TimedProgressBarManager invalid"));
		return;
	}


	BeginPlay_AddAbility();
}

void UTurretSwapComp::BeginPlay_AddAbility()
{
	UWorld* World = GetWorld();
	if (not World)
	{
		return;
	}

	FTimerDelegate NextTickDelegate;
	TWeakObjectPtr<UTurretSwapComp> WeakThis(this);
	NextTickDelegate.BindLambda([WeakThis]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}

		WeakThis->AddAbilityToCommands();
	});
	World->GetTimerManager().SetTimerForNextTick(NextTickDelegate);
}

void UTurretSwapComp::AddAbilityToCommands()
{
	if (not GetIsValidOwnerCommandsInterface())
	{
		return;
	}

	FUnitAbilityEntry AbilityEntry;
	AbilityEntry.AbilityId = EAbilityID::IdSwapTurret;
	AbilityEntry.CustomType = static_cast<int32>(M_TurretSwapAbilitySettings.ActiveSwapAbilityType);
	AbilityEntry.CooldownDuration = M_TurretSwapAbilitySettings.Cooldown;
	M_OwnerCommandsInterface->AddAbility(AbilityEntry, M_TurretSwapAbilitySettings.PreferredAbilityIndex);
}

void UTurretSwapComp::StartSwapProgressAndTimer()
{
	const float SwapTime = M_TurretSwapAbilitySettings.TimingProgress.SwapTime;
	if (SwapTime <= 0.0f)
	{
		ExecuteSwapNow();
		return;
	}

	if (GetIsValidTimedProgressBarManager())
	{
		URTSTimedProgressBarManager* ProgressBarManager = M_TimedProgressBarManager.Get();
		if (IsValid(ProgressBarManager))
		{
			constexpr float RatioStart = 0.0f;
			constexpr bool bUsePercentageText = false;
			M_ProgressBarActivationId = ProgressBarManager->ActivateTimedProgressBarAnchored(
				M_TurretChildActorComponent,
				M_TurretSwapAbilitySettings.TimingProgress.AttachedAnimProgressBarOffset,
				RatioStart,
				SwapTime,
				bUsePercentageText,
				M_TurretSwapAbilitySettings.TimingProgress.BarType,
				M_TurretSwapAbilitySettings.TimingProgress.bUseDescriptionText,
				M_TurretSwapAbilitySettings.TimingProgress.BarText,
				M_TurretSwapAbilitySettings.TimingProgress.BarScaleMlt);
		}
	}

	UWorld* World = GetWorld();
	if (not World)
	{
		return;
	}

	FTimerDelegate SwapDelegate;
	TWeakObjectPtr<UTurretSwapComp> WeakThis(this);
	SwapDelegate.BindLambda([WeakThis]()
	{
		if (not WeakThis.IsValid())
		{
			return;
		}

		WeakThis->ExecuteSwapNow();
	});
	World->GetTimerManager().SetTimer(M_SwapTimerHandle, SwapDelegate, SwapTime, false);
}

void UTurretSwapComp::ExecuteSwapNow()
{
	StopProgressBar();

	if (not GetIsValidChildActorComponent() || not GetIsValidSwapTurretClass())
	{
		if (GetIsValidOwnerCommandsInterface())
		{
			M_OwnerCommandsInterface->DoneExecutingCommand(EAbilityID::IdSwapTurret);
		}
		return;
	}

	ACPPTurretsMaster* OldTurret = Cast<ACPPTurretsMaster>(M_TurretChildActorComponent->GetChildActor());
	if (not IsValid(OldTurret))
	{
		RTSFunctionLibrary::ReportError("UTurretSwapComp::ExecuteSwapNow expected old turret before swap.");
		if (GetIsValidOwnerCommandsInterface())
		{
			M_OwnerCommandsInterface->DoneExecutingCommand(EAbilityID::IdSwapTurret);
		}
		return;
	}

	UpdateOwnerTurretArrays(OldTurret, nullptr);
	M_TurretChildActorComponent->SetChildActorClass(M_TurretSwapAbilitySettings.SwapTurretClass);

	ACPPTurretsMaster* NewTurret = Cast<ACPPTurretsMaster>(M_TurretChildActorComponent->GetChildActor());
	if (not IsValid(NewTurret))
	{
		RTSFunctionLibrary::ReportError("UTurretSwapComp::ExecuteSwapNow failed to spawn new turret.");
		if (GetIsValidOwnerCommandsInterface())
		{
			M_OwnerCommandsInterface->DoneExecutingCommand(EAbilityID::IdSwapTurret);
		}
		return;
	}

	CompleteSwapWithNewTurret(NewTurret);
}

void UTurretSwapComp::CompleteSwapWithNewTurret(ACPPTurretsMaster* NewTurret)
{
	if (not IsValid(NewTurret))
	{
		return;
	}

	UpdateOwnerTurretArrays(nullptr, NewTurret);
	SwapCommandCardSubtypeAndStartCooldown();
	SwapActiveAndSwapBackAbilityTypes();
	SetOwnerTurretsAutoEngage();
	RefreshOwnerBehaviours();

	if (GetIsValidOwnerCommandsInterface())
	{
		M_OwnerCommandsInterface->DoneExecutingCommand(EAbilityID::IdSwapTurret);
	}
}

void UTurretSwapComp::UpdateOwnerTurretArrays(ACPPTurretsMaster* OldTurret, ACPPTurretsMaster* NewTurret) const
{
	AActor* OwnerActor = GetOwner();
	if (not IsValid(OwnerActor))
	{
		return;
	}

	ATankMaster* TankOwner = Cast<ATankMaster>(OwnerActor);
	if (IsValid(TankOwner))
	{
		if (IsValid(OldTurret))
		{
			TankOwner->RemoveTurret(OldTurret);
		}

		if (IsValid(NewTurret))
		{
			TankOwner->SetupTurret(NewTurret);
		}
		return;
	}

	ABuildingExpansion* BxpOwner = Cast<ABuildingExpansion>(OwnerActor);
	if (not IsValid(BxpOwner))
	{
		return;
	}

	if (IsValid(OldTurret))
	{
		BxpOwner->RemoveTurret(OldTurret);
	}

	if (IsValid(NewTurret))
	{
		BxpOwner->SetupTurret(NewTurret);
	}
}

void UTurretSwapComp::ApplyMaterialOverrideToTurret(ACPPTurretsMaster* TurretToUpdate) const
{
	if (not IsValid(TurretToUpdate))
	{
		return;
	}

	UMaterialInterface* OverrideMaterial = M_TurretSwapAbilitySettings.TimingProgress.OldTurretMaterialOverride;
	if (not IsValid(OverrideMaterial))
	{
		return;
	}

	TurretToUpdate->ApplyMaterialToAllMeshComponents(OverrideMaterial);
}

void UTurretSwapComp::SwapCommandCardSubtypeAndStartCooldown()
{
	if (not GetIsValidOwnerCommandsInterface())
	{
		return;
	}

	FUnitAbilityEntry SwapAbilityEntry;
	SwapAbilityEntry.AbilityId = EAbilityID::IdSwapTurret;
	SwapAbilityEntry.CustomType = static_cast<int32>(M_TurretSwapAbilitySettings.SwapBackAbilityType);
	SwapAbilityEntry.CooldownDuration = M_TurretSwapAbilitySettings.Cooldown;
	M_OwnerCommandsInterface->SwapAbility(EAbilityID::IdSwapTurret, SwapAbilityEntry);
	M_OwnerCommandsInterface->StartCooldownOnAbility(
		EAbilityID::IdSwapTurret,
		static_cast<int32>(M_TurretSwapAbilitySettings.SwapBackAbilityType));
}

void UTurretSwapComp::StopProgressBar()
{
	if (M_ProgressBarActivationId == 0)
	{
		return;
	}

	if (not GetIsValidTimedProgressBarManager())
	{
		M_ProgressBarActivationId = 0;
		return;
	}

	URTSTimedProgressBarManager* ProgressBarManager = M_TimedProgressBarManager.Get();
	if (not IsValid(ProgressBarManager))
	{
		M_ProgressBarActivationId = 0;
		return;
	}

	ProgressBarManager->ForceProgressBarDormant(M_ProgressBarActivationId);
	M_ProgressBarActivationId = 0;
}

void UTurretSwapComp::RefreshOwnerBehaviours() const
{
	AActor* OwnerActor = GetOwner();
	if (not IsValid(OwnerActor))
	{
		return;
	}

	UBehaviourComp* BehaviourComp = OwnerActor->FindComponentByClass<UBehaviourComp>();
	if (not IsValid(BehaviourComp))
	{
		return;
	}

	BehaviourComp->RefreshAllBehaviours();
}

void UTurretSwapComp::SetOwnerTurretsAutoEngage() const
{
	AActor* OwnerActor = GetOwner();
	if (not IsValid(OwnerActor))
	{
		return;
	}

	ATankMaster* TankOwner = Cast<ATankMaster>(OwnerActor);
	if (IsValid(TankOwner))
	{
		TankOwner->SetTurretsToAutoEngage(true);
		return;
	}

	ABuildingExpansion* BxpOwner = Cast<ABuildingExpansion>(OwnerActor);
	if (not IsValid(BxpOwner))
	{
		return;
	}

	BxpOwner->SetTurretsToAutoEngage();
}

void UTurretSwapComp::SwapActiveAndSwapBackAbilityTypes()
{
	const ETurretSwapAbility OldActiveType = M_TurretSwapAbilitySettings.ActiveSwapAbilityType;
	M_TurretSwapAbilitySettings.ActiveSwapAbilityType = M_TurretSwapAbilitySettings.SwapBackAbilityType;
	M_TurretSwapAbilitySettings.SwapBackAbilityType = OldActiveType;
}

bool UTurretSwapComp::GetIsValidChildActorComponent() const
{
	if (IsValid(M_TurretChildActorComponent))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_TurretChildActorComponent",
		"UTurretSwapComp::GetIsValidChildActorComponent",
		this);
	return false;
}

bool UTurretSwapComp::GetIsValidOwnerCommandsInterface() const
{
	if (IsValid(M_OwnerCommandsInterface.GetObject()))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_OwnerCommandsInterface",
		"UTurretSwapComp::GetIsValidOwnerCommandsInterface",
		this);
	return false;
}

bool UTurretSwapComp::GetIsValidTimedProgressBarManager() const
{
	if (M_TimedProgressBarManager.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_TimedProgressBarManager",
		"UTurretSwapComp::GetIsValidTimedProgressBarManager",
		this);
	return false;
}

bool UTurretSwapComp::GetIsValidSwapTurretClass() const
{
	if (M_TurretSwapAbilitySettings.SwapTurretClass != nullptr)
	{
		return true;
	}

	RTSFunctionLibrary::ReportError("UTurretSwapComp::GetIsValidSwapTurretClass swap turret class is null.");
	return false;
}
