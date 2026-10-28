// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTSUnitOwnerSpawning.h"

#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"
#include "RTS_Survival/Weapons/InfantryWeapon/InfantryWeaponMaster.h"

namespace RTSUnitOwnerSpawningPrivate
{
	void CollectComponentTemplates(UClass* UnitClass, TArray<UActorComponent*>& OutTemplates)
	{
		for (UClass* Class = UnitClass; Class != nullptr; Class = Class->GetSuperClass())
		{
			UBlueprintGeneratedClass* BlueprintClass = Cast<UBlueprintGeneratedClass>(Class);
			if (BlueprintClass == nullptr)
			{
				continue;
			}
			if (BlueprintClass->SimpleConstructionScript != nullptr)
			{
				for (const USCS_Node* Node : BlueprintClass->SimpleConstructionScript->GetAllNodes())
				{
					OutTemplates.Add(Node != nullptr ? Node->ComponentTemplate : nullptr);
				}
			}
			if (UInheritableComponentHandler* Handler = BlueprintClass->GetInheritableComponentHandler(false))
			{
				for (auto RecordIterator = Handler->CreateRecordIterator(); RecordIterator; ++RecordIterator)
				{
					OutTemplates.Add(RecordIterator->ComponentTemplate);
				}
			}
		}
	}

	// Blueprint-added RTS components copy their template at spawn; the template carries the owner until restored.
	void OverrideRTSComponentTemplateOwners(UClass* UnitClass, const uint8 OwningPlayer,
	                                        TArray<TPair<TWeakObjectPtr<UObject>, uint8>>& OutRestore)
	{
		const FByteProperty* OwnerProperty = FindFProperty<FByteProperty>(URTSComponent::StaticClass(), TEXT("OwningPlayer"));
		if (OwnerProperty == nullptr)
		{
			return;
		}
		TArray<UActorComponent*> Templates;
		CollectComponentTemplates(UnitClass, Templates);
		for (UActorComponent* Template : Templates)
		{
			if (not IsValid(Template) || not Template->IsA<URTSComponent>())
			{
				continue;
			}
			uint8* OwnerValue = OwnerProperty->ContainerPtrToValuePtr<uint8>(Template);
			OutRestore.Emplace(Template, *OwnerValue);
			*OwnerValue = OwningPlayer;
		}
	}

	void RestoreRTSComponentTemplateOwners(const TArray<TPair<TWeakObjectPtr<UObject>, uint8>>& Restore)
	{
		const FByteProperty* OwnerProperty = FindFProperty<FByteProperty>(URTSComponent::StaticClass(), TEXT("OwningPlayer"));
		for (const TPair<TWeakObjectPtr<UObject>, uint8>& Entry : Restore)
		{
			if (OwnerProperty != nullptr && Entry.Key.IsValid())
			{
				*OwnerProperty->ContainerPtrToValuePtr<uint8>(Entry.Key.Get()) = Entry.Value;
			}
		}
	}
}

AActor* FRTSUnitOwnerSpawning::SpawnUnitForPlayer(UWorld& World, UClass* UnitClass, const FTransform& Transform,
                                                  const uint8 OwningPlayer)
{
	using namespace RTSUnitOwnerSpawningPrivate;
	if (not IsValid(UnitClass))
	{
		return nullptr;
	}
	TArray<TPair<TWeakObjectPtr<UObject>, uint8>> TemplateOwnersToRestore;
	OverrideRTSComponentTemplateOwners(UnitClass, OwningPlayer, TemplateOwnersToRestore);
	AActor* Unit = World.SpawnActorDeferred<AActor>(UnitClass, Transform, nullptr, nullptr,
	                                                ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (IsValid(Unit))
	{
		// Components the C++ class creates exist already; Blueprint-added ones took the template value above.
		if (URTSComponent* RTSComponent = Unit->FindComponentByClass<URTSComponent>())
		{
			RTSComponent->SetOwningPlayerRuntime(OwningPlayer);
		}
		Unit->FinishSpawning(Transform);
	}
	RestoreRTSComponentTemplateOwners(TemplateOwnersToRestore);
	APawn* Pawn = Cast<APawn>(Unit);
	if (IsValid(Pawn) && not IsValid(Pawn->GetController()))
	{
		Pawn->SpawnDefaultController();
	}
	return Unit;
}

void FRTSUnitOwnerSpawning::MatchSoldierToSquadOwner(ASquadUnit* Soldier, const uint8 SquadOwner)
{
	URTSComponent* SoldierRTS = IsValid(Soldier) ? Soldier->FindComponentByClass<URTSComponent>() : nullptr;
	if (SoldierRTS == nullptr || SoldierRTS->GetOwningPlayer() == SquadOwner)
	{
		return;
	}
	SoldierRTS->SetOwningPlayerRuntime(SquadOwner);
	FRTS_CollisionSetup::SetupInfantryCapsuleCollision(Soldier->GetCapsuleComponent(), SquadOwner);
	TArray<AActor*> Attached;
	Soldier->GetAttachedActors(Attached, true, true);
	for (AActor* Child : Attached)
	{
		if (AInfantryWeaponMaster* Weapon = Cast<AInfantryWeaponMaster>(Child))
		{
			Weapon->SetOwningPlayer(SquadOwner);
		}
	}
	if (AInfantryWeaponMaster* Weapon = Soldier->GetInfantryWeapon())
	{
		Weapon->SetOwningPlayer(SquadOwner);
	}
}
