// Copyright (C) Bas Blokzijl - All rights reserved.

#include "TankMaster.h"

#include "AITankMaster.h"
#include "Engine/GameInstance.h"
#include "RTS_Survival/Audio/SpacialVoiceLinePlayer/SpatialVoiceLinePlayer.h"
#include "RTS_Survival/Behaviours/BehaviourComp.h"
#include "RTS_Survival/Behaviours/Derived/Damage/AmmoCookOff/AmmoCookOffBehaviour.h"
#include "RTS_Survival/Behaviours/Derived/Damage/TankEngineFire/TankEngineFireBehaviour.h"
#include "RTS_Survival/GameUI/Pooled_AnimatedVerticalText/Pooling/AnimatedTextWidgetPoolManager/AnimatedTextWidgetPoolManager.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/ArmorCalculation.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/VehicleModules/VehicleModuleSubsystem.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSBlueprintFunctionLibrary.h"
#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "TrackedTank/PathFollowingComponent/TrackPathFollowingComponent.h"
#include "WheeledTank/Components/ChaosTankMovementComponent.h"

using namespace VehicleModuleBalance;

namespace TankMasterModuleHelpers
{
	ERTSVoiceLine GetVoiceLineForDestroyedModule(const EVehicleModuleTypes Type)
	{
		switch (Type)
		{
		case EVehicleModuleTypes::Engine:
			return ERTSVoiceLine::LostEngine;
		case EVehicleModuleTypes::Tracks:
		case EVehicleModuleTypes::Wheels:
			return ERTSVoiceLine::LostTrack;
		case EVehicleModuleTypes::Turret:
		case EVehicleModuleTypes::Weapon:
			return ERTSVoiceLine::LostGun;
		default:
			return ERTSVoiceLine::None;
		}
	}
}

// ----------------------------------------------------------------------------------------------------
// Initialization
// ----------------------------------------------------------------------------------------------------

void ATankMaster::BeginPlay_OnModulesComplete_FinalizeVehicleModules()
{
	UArmorCalculation* ArmorCalculation = FindComponentByClass<UArmorCalculation>();
	if (not IsValid(ArmorCalculation))
	{
		return;
	}
	BeginVehicleModuleFinalization(ArmorCalculation);
}

void ATankMaster::BeginVehicleModuleFinalization(UArmorCalculation* ArmorCalculation)
{
	if (not IsValid(ArmorCalculation) || M_TurretModuleRegistration.bBlueprintModuleSetupComplete
		|| bM_AreVehicleModulesInitialized)
	{
		return;
	}
	M_ModuleArmor = ArmorCalculation;
	M_TurretModuleRegistration.bBlueprintModuleSetupComplete = true;
	// Child actors and their Blueprint weapons may still be in BeginPlay. Discover mounts next tick.
	ScheduleInitialTurretDiscovery();
}

void ATankMaster::FinalizeVehicleModulesAfterTurretRegistration()
{
	if (bM_AreVehicleModulesInitialized || not GetIsValidModuleArmor()
		|| M_ModuleArmor->GetInstalledModuleCount() <= 0)
	{
		return;
	}
	if (not GetIsValidHealthComponent())
	{
		return;
	}
	// Both tank and turret modules receive HP from the initialized hull MaxHealth at this boundary.
	if (not M_ModuleArmor->FinalizeVehicleModuleSetup(this, HealthComponent->GetMaxHealth()))
	{
		return;
	}

	bM_AreVehicleModulesInitialized = true;
	BeginPlay_InitVehicleModuleBindings();
	RefreshMountedModuleBehaviours();
	HealthComponent->InitializeTankRepairOwner(this);
	HealthComponent->InitializeTankModulePresentation(this);
	RefreshModuleRepairCounts();
	HealthComponent->SynchronizeModuleIconSnapshot(M_ModuleArmor->GetModuleIconStates());
	InitializeCrewRepairAbilitySlot();
}

void ATankMaster::BeginPlay_InitVehicleModuleBindings()
{
	const UGameInstance* GameInstance = GetGameInstance();
	M_VehicleModuleSubsystem = IsValid(GameInstance) ? GameInstance->GetSubsystem<UVehicleModuleSubsystem>() : nullptr;

	M_MaxHealthChangedHandle = HealthComponent->GetOnMaxHealthChanged().AddUObject(
		this, &ATankMaster::OnTankMaxHealthChanged);

	if (GetIsValidBehaviourComponent())
	{
		M_ModuleBehavioursAppliedHandle = BehaviourComponent->GetOnModuleBehavioursApplied().AddUObject(
			this, &ATankMaster::OnModuleBehavioursApplied);
	}
}

void ATankMaster::CleanupVehicleModuleBindings()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_TurretModuleRegistration.InitialTurretDiscoveryTimer);
	}
	if (IsValid(HealthComponent) && M_MaxHealthChangedHandle.IsValid())
	{
		HealthComponent->GetOnMaxHealthChanged().Remove(M_MaxHealthChangedHandle);
	}
	if (IsValid(BehaviourComponent) && M_ModuleBehavioursAppliedHandle.IsValid())
	{
		BehaviourComponent->GetOnModuleBehavioursApplied().Remove(M_ModuleBehavioursAppliedHandle);
	}
	M_MaxHealthChangedHandle.Reset();
	M_ModuleBehavioursAppliedHandle.Reset();
	M_DeferredModuleBlueprintEvents.Reset();
	// Turrets ending play after the tank then find nothing to cancel and trigger no module work.
	M_TurretModuleRegistration = FTankTurretModuleRegistrationState();
}

void ATankMaster::OnModuleEngineDamaged(
	const FModuleStateChange& Change, const float RemainingModuleHp, const bool bIsChangeToDamagedState)
{
	if (not bIsChangeToDamagedState)
	{
		return;
	}
	(void)StartEngineFireChance(Change.NewState, Change.DamageShellType);
	
	if (Change.NewState == EVehicleModuleState::Destroyed)
	{
		PlayVoiceLineForDamageOnRadioIfSelected(ERTSVoiceLine::LostEngine, true);
	}
	else
	{
		PlayVoiceLineForDamageOnRadioIfSelected(ERTSVoiceLine::EngineDamage, false);
	}
}

bool ATankMaster::StartEngineFireChance(const EVehicleModuleState EngineState, const EWeaponShellType ShellType)
{
	const float FireChance = EngineFire::GetChance(EngineState, ShellType);
	if (FireChance <= 0.f || FMath::FRand() >= FireChance)
	{
		return false;
	}
	const TSubclassOf<UTankEngineFireBehaviour> FireBehaviourClass = GetEngineFireBehaviourClass();
	if (FireBehaviourClass == nullptr || not GetIsValidBehaviourComponent())
	{
		return false;
	}
	BehaviourComponent->AddBehaviour(FireBehaviourClass);
	ShowModuleFirePopup(TEXT("ENGINE FIRE"));
	return true;
}

TSubclassOf<UTankEngineFireBehaviour> ATankMaster::GetEngineFireBehaviourClass() const
{
	if (not GetIsValidModuleArmor())
	{
		return nullptr;
	}
	if (not GetIsValidVehicleModuleSubsystem())
	{
		return nullptr;
	}
	return M_VehicleModuleSubsystem->GetEngineFireBehaviourClass(M_ModuleArmor->GetVehicleModuleProfile());
}

bool ATankMaster::StartAmmoCookOffChance(const EVehicleModuleState AmmoState)
{
	const float CookOffChance = AmmoCookOff::GetChance(AmmoState);
	if (CookOffChance <= 0.f || FMath::FRand() >= CookOffChance)
	{
		return false;
	}
	const TSubclassOf<UAmmoCookOffBehaviour> CookOffBehaviourClass = GetAmmoCookOffBehaviourClass();
	if (CookOffBehaviourClass == nullptr || not GetIsValidBehaviourComponent()
		|| BehaviourComponent->GetBehaviourByClass(CookOffBehaviourClass) != nullptr)
	{
		return false;
	}
	BehaviourComponent->AddBehaviour(CookOffBehaviourClass);
	ShowModuleFirePopup(TEXT("AMMO COOK OFF"));
	return true;
}

TSubclassOf<UAmmoCookOffBehaviour> ATankMaster::GetAmmoCookOffBehaviourClass() const
{
	if (not GetIsValidModuleArmor() || not GetIsValidVehicleModuleSubsystem())
	{
		return nullptr;
	}
	return M_VehicleModuleSubsystem->GetAmmoCookOffBehaviourClass(M_ModuleArmor->GetVehicleModuleProfile());
}

void ATankMaster::ShowModuleFirePopup(const FString& PopupText) const
{
	UAnimatedTextWidgetPoolManager* PoolManager = FRTS_Statics::GetVerticalAnimatedTextWidgetPoolManager(this);
	if (not IsValid(PoolManager))
	{
		return;
	}
	constexpr float PopupHeight = 350.f;
	constexpr float PopupDelta = 100.f;
	constexpr float PopupVisibleDuration = 2.33f;
	constexpr float PopupFadeOutDuration = 1.f;
	constexpr float PopupWrapWidth = 350.f;
	FRTSVerticalAnimTextSettings TextSettings;
	TextSettings.DeltaZ = PopupDelta;
	TextSettings.VisibleDuration = PopupVisibleDuration;
	TextSettings.FadeOutDuration = PopupFadeOutDuration;
	PoolManager->ShowAnimatedText(
		FRTSRichTextConverter::MakeRTSRich(PopupText, ERTSRichText::Text_Bad14),
		GetActorLocation() + FVector(0.f, 0.f, PopupHeight), false,
		PopupWrapWidth, ETextJustify::Center, TextSettings);
}

void ATankMaster::OnModuleAddOnArmorDamaged(
	const FModuleStateChange& Change, const float RemainingModuleHp, const bool bIsChangeToDamagedState)
{
	if (not bIsChangeToDamagedState)
	{
		return;
	}
	PlayVoiceLineForDamageOnRadioIfSelected(ERTSVoiceLine::GeneralDamage,
	                                        Change.NewState == EVehicleModuleState::Destroyed);
}

void ATankMaster::PlayVoiceLineForDamageOnRadioIfSelected(const ERTSVoiceLine VlType,
                                                          const bool bIsDestroyedModule) const
{
	if (not GetIsValidSpatialVoiceLinePlayer())
	{
		return;
	}
	if (GetIsSelected() || bIsDestroyedModule)
	{
		M_SpatialVoiceLinePlayer->PlayVoiceLineOverRadio(VlType, true, true);
		return;
	}
	M_SpatialVoiceLinePlayer->PlaySpatialVoiceLine(VlType, GetActorLocation(), true);
}


void ATankMaster::OnModuleTracksDamaged(
	const FModuleStateChange& Change, const float RemainingModuleHp, const bool bIsChangeToDamagedState)
{
	if (not bIsChangeToDamagedState)
	{
		return;
	}
	if (Change.NewState == EVehicleModuleState::Destroyed)
	{
		PlayVoiceLineForDamageOnRadioIfSelected(ERTSVoiceLine::LostTrack, true);
	}
}

void ATankMaster::OnModuleAmmoDamaged(
	const FModuleStateChange& Change, const float RemainingModuleHp, const bool bIsChangeToDamagedState)
{
	if (not bIsChangeToDamagedState)
	{
		return;
	}
	PlayVoiceLineForDamageOnRadioIfSelected(ERTSVoiceLine::GeneralDamage,
	                                        Change.NewState == EVehicleModuleState::Destroyed);

	(void)StartAmmoCookOffChance(Change.NewState);
}

void ATankMaster::OnModuleTurretDamaged(
	const FModuleStateChange& Change, const float RemainingModuleHp, const bool bIsChangeToDamagedState)
{
	if (not bIsChangeToDamagedState)
	{
		return;
	}
	PlayVoiceLineForDamageOnRadioIfSelected(ERTSVoiceLine::GeneralDamage,
	                                        Change.NewState == EVehicleModuleState::Destroyed);
}

void ATankMaster::OnModuleWeaponDamaged(
	const FModuleStateChange& Change, const float RemainingModuleHp, const bool bIsChangeToDamagedState)
{
	if (not bIsChangeToDamagedState)
	{
		return;
	}
	
	if (Change.NewState == EVehicleModuleState::Destroyed)
	{
		PlayVoiceLineForDamageOnRadioIfSelected(ERTSVoiceLine::LostGun, true);
	}
	PlayVoiceLineForDamageOnRadioIfSelected(ERTSVoiceLine::GeneralDamage,
	                                        false);
}

void ATankMaster::OnModuleWheelsDamaged(
	const FModuleStateChange& Change, const float RemainingModuleHp, const bool bIsChangeToDamagedState)
{
	if (not bIsChangeToDamagedState)
	{
		return;
	}
	PlayVoiceLineForDamageOnRadioIfSelected(ERTSVoiceLine::GeneralDamage,
	                                        Change.NewState == EVehicleModuleState::Destroyed);
}

bool ATankMaster::GetIsValidModuleArmor() const
{
	if (M_ModuleArmor.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this,
		"M_ModuleArmor",
		"GetIsValidModuleArmor",
		this
	);
	return false;
}

bool ATankMaster::GetIsValidVehicleModuleSubsystem() const
{
	if (M_VehicleModuleSubsystem.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(
		this, "M_VehicleModuleSubsystem", "GetIsValidVehicleModuleSubsystem", this);
	return false;
}

void ATankMaster::OnTankMaxHealthChanged(const float OldMaxHealth, const float NewMaxHealth)
{
	if (not bM_AreVehicleModulesInitialized || not GetIsValidModuleArmor())
	{
		return;
	}
	// Upgrades preserve every module's health percentage and state; no repair work is granted.
	M_ModuleArmor->RecalculateModuleMaxHealth(NewMaxHealth);
}

// ----------------------------------------------------------------------------------------------------
// Committed module batches
// ----------------------------------------------------------------------------------------------------

void ATankMaster::OnModuleStateBatchCommitted(const FModuleChangeBatch& Batch)
{
	RefreshModuleRepairCounts();
	// Death beats every other reaction; late batches never recreate behaviours or icons on a dead tank.
	if (not IsUnitAlive())
	{
		return;
	}

	for (const FModuleStateChange& Change : Batch.GetChanges())
	{
		SyncModuleBehaviour(Change);
	}
	const bool bBehavioursCommitted = GetIsValidBehaviourComponent()
		&& BehaviourComponent->CommitModuleBehaviourChanges();

	UpdateCrewRepairAfterModuleBatch();
	RefreshCrewRepairAbilityFromModuleState();
	PublishModuleIconChanges(Batch);
	AnnounceModuleTransitions(Batch);

	if (bBehavioursCommitted)
	{
		PublishModuleBlueprintEvents(Batch);
		return;
	}
	// Behaviour work was deferred to its safe boundary; publish once it has been applied.
	for (const FModuleStateChange& Change : Batch.GetChanges())
	{
		M_DeferredModuleBlueprintEvents.AddOrMerge(Change);
	}
}

void ATankMaster::OnModuleBindingChanged(const int32 SlotIndex)
{
	if (not bM_AreVehicleModulesInitialized || not IsUnitAlive())
	{
		return;
	}
	SyncModuleBehaviourForSlot(SlotIndex);
	if (GetIsValidBehaviourComponent())
	{
		(void)BehaviourComponent->CommitModuleBehaviourChanges();
	}
}

void ATankMaster::RefreshMountedModuleBehaviours()
{
	if (not bM_AreVehicleModulesInitialized || not IsUnitAlive() || not GetIsValidBehaviourComponent())
	{
		return;
	}
	for (const EVehicleModuleTypes Type : {
		     EVehicleModuleTypes::Ammo, EVehicleModuleTypes::Turret,
		     EVehicleModuleTypes::Weapon
	     })
	{
		const int32 FirstSlot = VehicleModuleBalance::GetFirstSlotForType(Type);
		const int32 SlotCount = VehicleModuleBalance::GetSlotCountForType(Type);
		for (int32 SlotOffset = 0; SlotOffset < SlotCount; ++SlotOffset)
		{
			SyncModuleBehaviourForSlot(FirstSlot + SlotOffset);
		}
	}
	(void)BehaviourComponent->CommitModuleBehaviourChanges();
}

void ATankMaster::RefreshModuleRepairCounts()
{
	if (not GetIsValidModuleArmor())
	{
		return;
	}
	M_ModuleRepairState.DestroyedModuleCount = M_ModuleArmor->GetDestroyedModuleCount();
	M_ModuleRepairState.NonHealthyModuleCount = M_ModuleArmor->GetNonHealthyModuleCount();
	// Mirrored before the mutation returns so repair eligibility is current for the next healer query.
	if (GetIsValidHealthComponent())
	{
		HealthComponent->SetTankPendingModuleRepairCount(M_ModuleRepairState.NonHealthyModuleCount);
	}
}

void ATankMaster::SyncModuleBehaviour(const FModuleStateChange& Change)
{
	SyncModuleBehaviourForSlot(Change.SlotIndex);
}

void ATankMaster::SyncModuleBehaviourForSlot(const int32 SlotIndex)
{
	if (not GetIsValidBehaviourComponent() || not GetIsValidModuleArmor())
	{
		return;
	}
	const FVehicleModuleSnapshot Snapshot = M_ModuleArmor->GetModuleSnapshotForSlot(SlotIndex);
	// A missing asset or unassigned class means no behaviour for this state: no error and no fallback.
	TSubclassOf<UVehicleModuleBehaviour> DesiredClass = nullptr;
	if (GetIsValidVehicleModuleSubsystem())
	{
		DesiredClass = M_VehicleModuleSubsystem->GetModuleBehaviourClass(Snapshot.Type, Snapshot.State);
	}

	FVehicleModuleBehaviourContext Context;
	Context.ModuleId = Snapshot.ModuleId;
	Context.SlotIndex = SlotIndex;
	Context.Type = Snapshot.Type;
	Context.State = Snapshot.State;
	Context.CurrentHp = Snapshot.CurrentHp;
	Context.MaxHp = Snapshot.MaxHp;
	Context.BoundMesh = M_ModuleArmor->GetBoundMeshForSlot(SlotIndex);
	Context.BoundWeapon = M_ModuleArmor->GetBoundWeaponForSlot(SlotIndex);
	Context.ArmorCalculation = M_ModuleArmor;
	BehaviourComponent->SetModuleBehaviour(SlotIndex, DesiredClass, Context);
}

void ATankMaster::OnModuleBehavioursApplied()
{
	if (M_DeferredModuleBlueprintEvents.IsEmpty() || not IsUnitAlive())
	{
		M_DeferredModuleBlueprintEvents.Reset();
		return;
	}
	// Copy first: Blueprint handlers may cause new module batches.
	const FModuleChangeBatch DeferredEvents = M_DeferredModuleBlueprintEvents;
	M_DeferredModuleBlueprintEvents.Reset();
	PublishModuleBlueprintEvents(DeferredEvents);
}

void ATankMaster::PublishModuleIconChanges(const FModuleChangeBatch& Batch) const
{
	if (not IsValid(HealthComponent) || not GetIsValidModuleArmor())
	{
		return;
	}
	FModuleIconDeltaBatch IconDelta;
	IconDelta.IconStates = M_ModuleArmor->GetModuleIconStates();
	for (const FModuleStateChange& Change : Batch.GetChanges())
	{
		IconDelta.ChangedTypeMask |= 1u << GetModuleTypeIndex(Change.Type);
	}
	// The health component suppresses unchanged aggregate states before touching the widget.
	HealthComponent->ApplyModuleIconStateChanges(IconDelta);
}

void ATankMaster::PublishModuleBlueprintEvents(const FModuleChangeBatch& Batch)
{
	for (const FModuleStateChange& Change : Batch.GetChanges())
	{
		// Stop dispatch on death; load reconciliation never replays transition events.
		if (not IsUnitAlive())
		{
			return;
		}
		if (Change.Cause == EModuleChangeCause::Load)
		{
			continue;
		}
		OnVehicleModuleStateChanged(Change);
	}
}

void ATankMaster::AnnounceModuleTransitions(const FModuleChangeBatch& Batch)
{
	const UWorld* World = GetWorld();
	if (not IsValid(M_SpatialVoiceLinePlayer) || not IsValid(World))
	{
		return;
	}
	const double NowSeconds = World->GetTimeSeconds();
	const bool bIsOnCooldown = M_LastModuleAnnouncementTime >= 0.0
		&& NowSeconds - M_LastModuleAnnouncementTime < ModuleAnnouncementCooldownSeconds;
	if (bIsOnCooldown)
	{
		return;
	}

	for (const FModuleStateChange& Change : Batch.GetChanges())
	{
		const bool bIsNewFailure = Change.Cause == EModuleChangeCause::Damage
			&& Change.NewState == EVehicleModuleState::Destroyed;
		const ERTSVoiceLine VoiceLine = bIsNewFailure
			                                ? TankMasterModuleHelpers::GetVoiceLineForDestroyedModule(Change.Type)
			                                : ERTSVoiceLine::None;
		if (VoiceLine == ERTSVoiceLine::None)
		{
			continue;
		}
		M_SpatialVoiceLinePlayer->PlaySpatialVoiceLine(VoiceLine, GetActorLocation(), false);
		M_LastModuleAnnouncementTime = NowSeconds;
		return;
	}
}

// ----------------------------------------------------------------------------------------------------
// Ordinary healing milestones
// ----------------------------------------------------------------------------------------------------

void ATankMaster::OnHealthHealingApplied(const FHealthHealingReceipt& Receipt)
{
	if (not IsUnitAlive() || not bM_AreVehicleModulesInitialized || Receipt.AcceptedHealingWork <= 0.f)
	{
		return;
	}
	if (M_ModuleRepairState.NonHealthyModuleCount <= 0 || not GetIsValidModuleArmor())
	{
		return;
	}

	// Choose the final milestone before mutating modules: full service skips the intermediate yellow state.
	const float SurplusWork = FMath::Max(0.f, Receipt.AcceptedHealingWork - Receipt.AppliedHullHealing);
	const bool bIsHullFull = Receipt.MaxHealth > 0.f
		&& Receipt.HealthAfter >= Receipt.MaxHealth * (CompletedHealth01 - HealthCompletionTolerance01);
	if (bIsHullFull && AccumulateFullModuleService(SurplusWork))
	{
		M_ModuleRepairState.FullServiceAccumulatedWork = 0.f;
		M_ModuleArmor->RestoreAllModulesToHealthy();
		return;
	}
	(void)TryRecoverDestroyedModulesAfterHealing();
}

bool ATankMaster::TryRecoverDestroyedModulesAfterHealing()
{
	if (M_ModuleRepairState.DestroyedModuleCount <= 0 || not GetIsValidHealthComponent())
	{
		return false;
	}
	// The single ordinary-healing gate; no new upward crossing is required.
	if (HealthComponent->GetHealthPercentage() < TankHealthRequiredForModuleRecovery01)
	{
		return false;
	}
	M_ModuleArmor->RestoreDestroyedModulesToDamaged(EModuleChangeCause::Recovery);
	return true;
}

bool ATankMaster::AccumulateFullModuleService(const float Work)
{
	if (Work <= 0.f || M_ModuleRepairState.NonHealthyModuleCount <= 0)
	{
		return false;
	}
	// New module damage since the work started resets unfinished finishing progress.
	const uint32 ArmorDamageRevision = M_ModuleArmor->GetModuleDamageRevision();
	if (ArmorDamageRevision != M_ModuleRepairState.ModuleDamageRevision)
	{
		M_ModuleRepairState.ModuleDamageRevision = ArmorDamageRevision;
		M_ModuleRepairState.FullServiceAccumulatedWork = 0.f;
	}
	// Never rate-limited; only capped to the remaining requirement.
	const float RemainingWork = FMath::Max(0.f, FullModuleServiceWork - M_ModuleRepairState.FullServiceAccumulatedWork);
	M_ModuleRepairState.FullServiceAccumulatedWork += FMath::Min(Work, RemainingWork);
	return M_ModuleRepairState.FullServiceAccumulatedWork >= FullModuleServiceWork;
}

float ATankMaster::GetCurrentFinishingWork() const
{
	if (not M_ModuleArmor.IsValid()
		|| M_ModuleArmor->GetModuleDamageRevision() != M_ModuleRepairState.ModuleDamageRevision)
	{
		return 0.f;
	}
	return M_ModuleRepairState.FullServiceAccumulatedWork;
}

bool ATankMaster::GetIsVehicleFullyRepaired() const
{
	if (not IsValid(HealthComponent))
	{
		return false;
	}
	return HealthComponent->GetHealthPercentage() >= CompletedHealth01 - HealthCompletionTolerance01
		&& M_ModuleRepairState.NonHealthyModuleCount <= 0;
}

// ----------------------------------------------------------------------------------------------------
// Persistence
// ----------------------------------------------------------------------------------------------------

FVehicleModuleSaveData ATankMaster::ExportVehicleModuleSaveData() const
{
	if (not bM_AreVehicleModulesInitialized || not GetIsValidModuleArmor())
	{
		return FVehicleModuleSaveData();
	}
	FVehicleModuleSaveData SaveData = M_ModuleArmor->ExportModuleState();
	SaveData.FinishingWork = GetCurrentFinishingWork();
	return SaveData;
}

bool ATankMaster::ImportVehicleModuleSaveData(const FVehicleModuleSaveData& SaveData)
{
	if (not IsUnitAlive())
	{
		return false;
	}
	if (M_TurretModuleRegistration.bBlueprintModuleSetupComplete && not bM_AreVehicleModulesInitialized)
	{
		M_TurretModuleRegistration.DeferredSaveData = SaveData;
		return true;
	}
	if (not bM_AreVehicleModulesInitialized || not GetIsValidModuleArmor())
	{
		return false;
	}
	if (GetHasUnprocessedTurretModuleRegistrations())
	{
		// Turret modules install the tick after BeginPlay; importing earlier would mismatch the module count.
		M_TurretModuleRegistration.DeferredSaveData = SaveData;
		return true;
	}
	// Transient crew work is never deserialized; a live timer is not resumed after load.
	if (GetCurrentActiveCommand() == EAbilityID::IdCrewRepair)
	{
		SetUnitToIdle();
	}
	FinishCrewRepair(ECrewRepairStopReason::QueueTermination);
	if (not M_ModuleArmor->ImportModuleState(SaveData))
	{
		return false;
	}
	M_ModuleRepairState.ModuleDamageRevision = M_ModuleArmor->GetModuleDamageRevision();
	M_ModuleRepairState.FullServiceAccumulatedWork = FMath::Clamp(SaveData.FinishingWork, 0.f, FullModuleServiceWork);
	RefreshModuleRepairCounts();
	if (GetIsValidHealthComponent())
	{
		HealthComponent->SynchronizeModuleIconSnapshot(M_ModuleArmor->GetModuleIconStates());
	}
	// Exposes EnableRepair if damaged or destroyed modules remain, never DisableRepair without an active session.
	RefreshCrewRepairAbilityFromModuleState();
	return true;
}

// ----------------------------------------------------------------------------------------------------
// Mounted weapon lock
// ----------------------------------------------------------------------------------------------------

void ATankMaster::AcquireMountedWeaponLock(UObject* Source)
{
	if (not IsValid(Source))
	{
		return;
	}
	M_MountedWeaponLockSources.RemoveAll([](const TWeakObjectPtr<UObject>& LockSource)
	{
		return not LockSource.IsValid();
	});
	M_MountedWeaponLockSources.AddUnique(Source);
	SetTurretsDisabled();
}

void ATankMaster::ReleaseMountedWeaponLock(UObject* Source, const bool bUseLastTargetOnRestore)
{
	M_MountedWeaponLockSources.RemoveAll([Source](const TWeakObjectPtr<UObject>& LockSource)
	{
		return not LockSource.IsValid() || LockSource.Get() == Source;
	});
	// Never enable weapons still disabled by another source or on a dead tank.
	if (GetHasMountedWeaponLock() || not IsUnitAlive())
	{
		return;
	}
	SetTurretsToAutoEngage(bUseLastTargetOnRestore);
}

bool ATankMaster::GetHasMountedWeaponLock() const
{
	for (const TWeakObjectPtr<UObject>& LockSource : M_MountedWeaponLockSources)
	{
		if (LockSource.IsValid())
		{
			return true;
		}
	}
	return false;
}

// ----------------------------------------------------------------------------------------------------
// Mobility restrictions
// ----------------------------------------------------------------------------------------------------

void ATankMaster::SetMobilityRestriction(UObject* Source, const float TravelSpeedMultiplier,
                                         const float TurnRateMultiplier, const float AccelerationMultiplier)
{
	if (not IsValid(Source))
	{
		return;
	}
	FTankMobilityRestriction* ExistingRestriction = M_MobilityRestrictions.FindByPredicate(
		[Source](const FTankMobilityRestriction& Restriction)
		{
			return Restriction.Source.Get() == Source;
		});
	if (ExistingRestriction == nullptr)
	{
		ExistingRestriction = &M_MobilityRestrictions.AddDefaulted_GetRef();
		ExistingRestriction->Source = Source;
	}
	ExistingRestriction->TravelSpeedMultiplier = FMath::Clamp(TravelSpeedMultiplier, 0.f, 1.f);
	ExistingRestriction->TurnRateMultiplier = FMath::Clamp(TurnRateMultiplier, 0.f, 1.f);
	ExistingRestriction->AccelerationMultiplier = FMath::Clamp(AccelerationMultiplier, 0.f, 1.f);
	RebuildVehicleMobility();
}

void ATankMaster::ClearMobilityRestriction(const UObject* Source)
{
	const int32 RemovedCount = M_MobilityRestrictions.RemoveAll([Source](const FTankMobilityRestriction& Restriction)
	{
		return not Restriction.Source.IsValid() || Restriction.Source.Get() == Source;
	});
	if (RemovedCount > 0)
	{
		RebuildVehicleMobility();
	}
}

void ATankMaster::RebuildVehicleMobility()
{
	// Minimum, not product: an engine and a running-gear limit never stack multiplicatively.
	float TravelSpeedMultiplier = 1.f;
	float TurnRateMultiplier = 1.f;
	float AccelerationMultiplier = 1.f;
	for (const FTankMobilityRestriction& Restriction : M_MobilityRestrictions)
	{
		if (not Restriction.Source.IsValid())
		{
			continue;
		}
		TravelSpeedMultiplier = FMath::Min(TravelSpeedMultiplier, Restriction.TravelSpeedMultiplier);
		TurnRateMultiplier = FMath::Min(TurnRateMultiplier, Restriction.TurnRateMultiplier);
		AccelerationMultiplier = FMath::Min(AccelerationMultiplier, Restriction.AccelerationMultiplier);
	}

	if (UChaosTankMovementComponent* WheeledMovement = FindComponentByClass<UChaosTankMovementComponent>())
	{
		float UnrestrictedMaxSpeedKmh = 0.f;
		if (URTSComponent* VehicleRtsComponent = GetRTSComponent())
		{
			const FTankData TankData = URTSBlueprintFunctionLibrary::BP_GetTankDataOfPlayer(
				VehicleRtsComponent->GetOwningPlayer(), VehicleRtsComponent->GetSubtypeAsTankSubtype(), this);
			UnrestrictedMaxSpeedKmh = TankData.VehicleMaxSpeedKmh;
		}
		WheeledMovement->SetModuleMobilityLimits(TravelSpeedMultiplier, TurnRateMultiplier,
		                                         AccelerationMultiplier, UnrestrictedMaxSpeedKmh);
	}

	if (not IsValid(AITankController))
	{
		return;
	}
	// Tracked path following applies the scalars on top of its desired speeds; physics reads the limited outputs.
	UTrackPathFollowingComponent* TrackPathFollowing = Cast<UTrackPathFollowingComponent>(
		AITankController->GetPathFollowingComponent());
	if (IsValid(TrackPathFollowing))
	{
		TrackPathFollowing->SetMobilityLimits(TravelSpeedMultiplier, TurnRateMultiplier, AccelerationMultiplier);
	}
}
