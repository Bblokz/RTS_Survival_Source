// Copyright (C) Bas Blokzijl - All rights reserved.

#include "BehVehicleStunned.h"

#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"

namespace BehVehicleStunnedConstants
{
	constexpr int32 MaximumStackCount = 1;
}

UBehVehicleStunned::UBehVehicleStunned()
{
	FRepeatedBehaviourTextSettings NewAnimatedTextSettings;
	NewAnimatedTextSettings.AmountRepeats = 1;
	NewAnimatedTextSettings.RepeatInterval =2.f;
	NewAnimatedTextSettings.RepeatStrategy = EBehaviourRepeatedVerticalTextStrategy::PerAmountRepeats;
	NewAnimatedTextSettings.TextSettings.bAutoWrap = false;
	NewAnimatedTextSettings.TextSettings.bUseText= true;
	
	NewAnimatedTextSettings.TextSettings.InSettings.DeltaZ = 75.f;
	NewAnimatedTextSettings.TextSettings.InSettings.VisibleDuration =2.f;
	NewAnimatedTextSettings.TextSettings.InSettings.FadeOutDuration = 1.f;
	
	NewAnimatedTextSettings.TextSettings.InJustification = ETextJustify::Center;
	NewAnimatedTextSettings.TextSettings.TextOffset = FVector(0,0,300);
	NewAnimatedTextSettings.TextSettings.InWrapAt = 0;
	NewAnimatedTextSettings.TextSettings.TextOnSubjects = FRTSRichTextConverter::MakeRTSRich("Crew Stunned!!", ERTSRichText::Text_Bad14);
	AnimatedTextSettings = NewAnimatedTextSettings;
	BehaviourLifeTime = EBehaviourLifeTime::Timed;
	M_MaxStackCount = BehVehicleStunnedConstants::MaximumStackCount;
	BehaviourStackRule = EBehaviourStackRule::Exclusive;
	
	BehaviourIcon = EBehaviourIcon::VehicleStunned;
	M_TitleText = FRTSRichTextConverter::MakeRTSRich("Crew Stunned", ERTSRichText::Text_BadTitle);
	M_DisplayText= "Weapons and movement are disabled.";
	M_LifeTimeDuration = 4.0f;
	bM_UsesTick = true;

	M_AbilitiesToRemove =
	{
		EAbilityID::IdMove,
		EAbilityID::IdReverseMove,
		EAbilityID::IdRotateTowards
	};
}

void UBehVehicleStunned::OnAdded(AActor* BehaviourOwner)
{
	CacheOwnerReferences(BehaviourOwner);

	SetOwnerToIdleAndStopMovement();
	DisableMountedWeapons();
	ApplyTurretRotationPenalty();
	CacheAndRemoveAbilities();

	// Make sure to call the bp event.
	Super::OnAdded(BehaviourOwner);
}

void UBehVehicleStunned::OnRemoved(AActor* BehaviourOwner)
{
	RestoreTurretRotation();
	EnableMountedWeapons();
	RestoreRemovedAbilities();
	ResetCachedState();

	// Make sure to call the bp event.
	Super::OnRemoved(BehaviourOwner);
}

void UBehVehicleStunned::CacheOwnerReferences(AActor* BehaviourOwner)
{
	M_TankMaster = Cast<ATankMaster>(BehaviourOwner);
	M_CommandsOwnerActor = BehaviourOwner;

	(void)GetIsValidTankMaster();
	(void)GetIsValidCommandsOwnerActor();
}

void UBehVehicleStunned::SetOwnerToIdleAndStopMovement() const
{
	if (not GetIsValidCommandsOwnerActor())
	{
		return;
	}

	ICommands* CommandsInterface = GetCommandsInterface();
	if (CommandsInterface == nullptr)
	{
		RTSFunctionLibrary::ReportFailedCastError(
			TEXT("M_CommandsOwnerActor"),
			TEXT("ICommands"),
			TEXT("UBehVehicleStunned::SetOwnerToIdleAndStopMovement")
		);
		return;
	}

	CommandsInterface->SetUnitToIdle();
	CommandsInterface->StopMovement();
}

void UBehVehicleStunned::ApplyTurretRotationPenalty()
{
	if (not GetIsValidTankMaster())
	{
		return;
	}

	const TArray<ACPPTurretsMaster*> Turrets = M_TankMaster->GetTurrets();
	for (ACPPTurretsMaster* Turret : Turrets)
	{
		if (Turret == nullptr)
		{
			continue;
		}

		const TWeakObjectPtr<ACPPTurretsMaster> TurretPtr = Turret;
		if (M_CachedTurretRotationSpeeds.Contains(TurretPtr))
		{
			continue;
		}

		const float BaseRotationSpeed = Turret->GetTurretRotationSpeed();
		M_CachedTurretRotationSpeeds.Add(TurretPtr, BaseRotationSpeed);
		Turret->SetTurretRotationSpeed(BaseRotationSpeed * M_TurretRotationSpeedMultiplier);
	}
}

void UBehVehicleStunned::RestoreTurretRotation()
{
	for (const TPair<TWeakObjectPtr<ACPPTurretsMaster>, float>& CachedTurretSpeed : M_CachedTurretRotationSpeeds)
	{
		if (not CachedTurretSpeed.Key.IsValid())
		{
			continue;
		}

		CachedTurretSpeed.Key->SetTurretRotationSpeed(CachedTurretSpeed.Value);
	}

	M_CachedTurretRotationSpeeds.Reset();
}

void UBehVehicleStunned::DisableMountedWeapons()
{
	if (not GetIsValidTankMaster())
	{
		return;
	}

	// Shared weapon lock: overlapping crew repair or module behaviours keep weapons disabled after the stun ends.
	M_TankMaster->AcquireMountedWeaponLock(this);
}

void UBehVehicleStunned::EnableMountedWeapons()
{
	if (not GetIsValidTankMaster())
	{
		return;
	}

	constexpr bool bUseLastTargetOnRestore = true;
	M_TankMaster->ReleaseMountedWeaponLock(this, bUseLastTargetOnRestore);
}

void UBehVehicleStunned::CacheAndRemoveAbilities()
{
	UCommandData* CommandData = GetCommandData();
	if (not IsValid(CommandData))
	{
		return;
	}

	// Suppression keeps full entries, indices and cooldowns and shares ownership with other sources.
	CommandData->EndAbilitySuppression(M_AbilitySuppressionHandle);
	M_AbilitySuppressionHandle = CommandData->BeginAbilitySuppression(this, M_AbilitiesToRemove);
	CommandData->UpdateActionUI();
}

void UBehVehicleStunned::RestoreRemovedAbilities()
{
	UCommandData* CommandData = GetCommandData();
	if (not IsValid(CommandData))
	{
		M_AbilitySuppressionHandle.Reset();
		return;
	}

	CommandData->EndAbilitySuppression(M_AbilitySuppressionHandle);
	CommandData->UpdateActionUI();
}

UCommandData* UBehVehicleStunned::GetCommandData() const
{
	ICommands* CommandsInterface = GetCommandsInterface();
	if (CommandsInterface == nullptr)
	{
		RTSFunctionLibrary::ReportFailedCastError(
			TEXT("M_CommandsOwnerActor"),
			TEXT("ICommands"),
			TEXT("UBehVehicleStunned::GetCommandData")
		);
		return nullptr;
	}

	return CommandsInterface->GetIsValidCommandData();
}

void UBehVehicleStunned::ResetCachedState()
{
	M_CachedTurretRotationSpeeds.Reset();
	M_TankMaster.Reset();
	M_CommandsOwnerActor.Reset();
}

ICommands* UBehVehicleStunned::GetCommandsInterface() const
{
	if (not GetIsValidCommandsOwnerActor())
	{
		return nullptr;
	}

	return Cast<ICommands>(M_CommandsOwnerActor.Get());
}

bool UBehVehicleStunned::GetIsValidTankMaster() const
{
	if (M_TankMaster.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_TankMaster"),
		TEXT("GetIsValidTankMaster"),
		this
	);

	return false;
}

bool UBehVehicleStunned::GetIsValidCommandsOwnerActor() const
{
	if (M_CommandsOwnerActor.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_CommandsOwnerActor"),
		TEXT("GetIsValidCommandsOwnerActor"),
		this
	);

	return false;
}
