// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#include "PanzerwerferReloadBonusSubsystem.h"

#include "BehaviourWeaponWurfrahmenShop.h"
#include "RTS_Survival/Behaviours/BehaviourComp.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

DEFINE_LOG_CATEGORY_STATIC(LogPanzerwerferReloadBonus, Log, All);

void UPanzerwerferReloadBonusSubsystem::RegisterBonusSource(const AActor* Source, const int32 OwningPlayer)
{
	if (not IsValid(Source))
	{
		RTSFunctionLibrary::ReportError("Tried to register a Panzerwerfer reload bonus without a source."
			"\n At function UPanzerwerferReloadBonusSubsystem::RegisterBonusSource");
		return;
	}
	RemoveStaleSources();
	const bool bAlreadyRegistered = M_Sources.ContainsByPredicate([Source](const FPanzerwerferReloadBonusSource& Each)
	{
		return Each.Source.Get() == Source;
	});
	if (bAlreadyRegistered)
	{
		return;
	}
	const bool bWasActive = GetIsBonusActiveForPlayer(OwningPlayer);
	FPanzerwerferReloadBonusSource NewSource;
	NewSource.Source = Source;
	NewSource.OwningPlayer = OwningPlayer;
	M_Sources.Add(NewSource);
	UE_LOG(LogPanzerwerferReloadBonus, Log, TEXT("RTS_PZW_RELOAD_BONUS register source=%s player=%d active_before=%d"),
	       *Source->GetName(), OwningPlayer, bWasActive ? 1 : 0);
	if (not bWasActive)
	{
		AddBonusToPlayerPanzerwerfers(OwningPlayer);
	}
}

void UPanzerwerferReloadBonusSubsystem::UnregisterBonusSource(const AActor* Source)
{
	const int32 SourceIndex = M_Sources.IndexOfByPredicate([Source](const FPanzerwerferReloadBonusSource& Each)
	{
		return Each.Source.Get() == Source;
	});
	// A source that is not registered (never built, or already taken back) is not an error.
	if (SourceIndex == INDEX_NONE)
	{
		RemoveStaleSources();
		return;
	}
	const int32 OwningPlayer = M_Sources[SourceIndex].OwningPlayer;
	M_Sources.RemoveAt(SourceIndex);
	RemoveStaleSources();
	const bool bStillActive = GetIsBonusActiveForPlayer(OwningPlayer);
	UE_LOG(LogPanzerwerferReloadBonus, Log, TEXT("RTS_PZW_RELOAD_BONUS unregister player=%d still_active=%d"),
	       OwningPlayer, bStillActive ? 1 : 0);
	if (not bStillActive)
	{
		RemoveBonusFromPlayerPanzerwerfers(OwningPlayer);
	}
}

void UPanzerwerferReloadBonusSubsystem::OnTankReady(ATankMaster* Tank)
{
	if (not IsValid(Tank) || not GetIsPanzerwerfer(*Tank))
	{
		return;
	}
	const URTSComponent* RTSComponent = Tank->GetRTSComponent();
	if (not IsValid(RTSComponent) || not GetIsBonusActiveForPlayer(RTSComponent->GetOwningPlayer()))
	{
		return;
	}
	AddBonusBehaviour(*Tank);
}

bool UPanzerwerferReloadBonusSubsystem::GetIsBonusActiveForPlayer(const int32 Player) const
{
	return M_Sources.ContainsByPredicate([Player](const FPanzerwerferReloadBonusSource& Each)
	{
		return Each.OwningPlayer == Player && Each.Source.IsValid();
	});
}

void UPanzerwerferReloadBonusSubsystem::RemoveStaleSources()
{
	M_Sources.RemoveAll([](const FPanzerwerferReloadBonusSource& Each)
	{
		return not Each.Source.IsValid();
	});
}

void UPanzerwerferReloadBonusSubsystem::AddBonusToPlayerPanzerwerfers(const int32 Player) const
{
	// A copy: adding a behaviour never changes the unit lists, but the copy keeps that true if it ever does.
	const TArray<ATankMaster*> Panzerwerfers = GetPlayerPanzerwerfers(Player);
	for (ATankMaster* Panzerwerfer : Panzerwerfers)
	{
		AddBonusBehaviour(*Panzerwerfer);
	}
}

void UPanzerwerferReloadBonusSubsystem::RemoveBonusFromPlayerPanzerwerfers(const int32 Player) const
{
	const TArray<ATankMaster*> Panzerwerfers = GetPlayerPanzerwerfers(Player);
	for (ATankMaster* Panzerwerfer : Panzerwerfers)
	{
		RemoveBonusBehaviour(*Panzerwerfer);
	}
}

TArray<ATankMaster*> UPanzerwerferReloadBonusSubsystem::GetPlayerPanzerwerfers(const int32 Player) const
{
	TArray<ATankMaster*> Panzerwerfers;
	const UGameUnitManager* GameUnitManager = FRTS_Statics::GetGameUnitManager(this);
	if (not IsValid(GameUnitManager))
	{
		// The match is starting or ending; new Panzerwerfers still get the bonus through OnTankReady.
		return Panzerwerfers;
	}
	for (ATankMaster* Tank : GameUnitManager->GetPlayerTanks(static_cast<uint8>(Player)))
	{
		if (not IsValid(Tank) || not GetIsPanzerwerfer(*Tank))
		{
			continue;
		}
		const URTSComponent* RTSComponent = Tank->GetRTSComponent();
		// The enemy list holds every non-player owner.
		if (IsValid(RTSComponent) && RTSComponent->GetOwningPlayer() == Player)
		{
			Panzerwerfers.Add(Tank);
		}
	}
	return Panzerwerfers;
}

bool UPanzerwerferReloadBonusSubsystem::GetIsPanzerwerfer(const ATankMaster& Tank)
{
	const URTSComponent* RTSComponent = Tank.GetRTSComponent();
	if (not IsValid(RTSComponent) || RTSComponent->GetUnitType() != EAllUnitType::UNType_Tank)
	{
		return false;
	}
	// Tank_Panzerwerfer is the Mechanized Depot's Panzerwerfer 42 (Maultier); the Pz 38(t) R carries the smaller
	// Panzerwerfer rocket turret, so it counts as one too.
	const ETankSubtype Subtype = RTSComponent->GetSubtypeAsTankSubtype();
	return Subtype == ETankSubtype::Tank_Panzerwerfer || Subtype == ETankSubtype::Tank_Pz38t_R;
}

void UPanzerwerferReloadBonusSubsystem::AddBonusBehaviour(ATankMaster& Tank)
{
	UBehaviourComp* BehaviourComponent = Tank.FindComponentByClass<UBehaviourComp>();
	if (not IsValid(BehaviourComponent))
	{
		RTSFunctionLibrary::ReportError("Panzerwerfer has no behaviour component; the Wurfrahmen shop's reload bonus"
			" cannot be applied.\n Tank: " + Tank.GetName() +
			"\n At function UPanzerwerferReloadBonusSubsystem::AddBonusBehaviour");
		return;
	}
	// Exclusive stack rule, but checked here too: a second instance would take two removals.
	if (IsValid(BehaviourComponent->GetBehaviourByClass(UBehaviourWeaponWurfrahmenShop::StaticClass())))
	{
		return;
	}
	BehaviourComponent->AddBehaviour(UBehaviourWeaponWurfrahmenShop::StaticClass());
	UE_LOG(LogPanzerwerferReloadBonus, Log, TEXT("RTS_PZW_RELOAD_BONUS added tank=%s"), *Tank.GetName());
}

void UPanzerwerferReloadBonusSubsystem::RemoveBonusBehaviour(ATankMaster& Tank)
{
	UBehaviourComp* BehaviourComponent = Tank.FindComponentByClass<UBehaviourComp>();
	if (not IsValid(BehaviourComponent)
		|| not IsValid(BehaviourComponent->GetBehaviourByClass(UBehaviourWeaponWurfrahmenShop::StaticClass())))
	{
		return;
	}
	BehaviourComponent->RemoveBehaviour(UBehaviourWeaponWurfrahmenShop::StaticClass());
	UE_LOG(LogPanzerwerferReloadBonus, Log, TEXT("RTS_PZW_RELOAD_BONUS removed tank=%s"), *Tank.GetName());
}
