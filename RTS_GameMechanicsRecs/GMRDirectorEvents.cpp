// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

// Gameplay actions of AGMRDirector: every FGMREvent type and the per-frame upkeep of the units it films.

#include "GMRDirector.h"

// Before any game header: the MapTest_/ShippingTest_ hooks used here are declared under RTS_WITH_MAP_TESTS.
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"

#include "RTS_Survival/Audio/RTSVoiceLines/RTSVoicelines.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansionEnums.h"
#include "RTS_Survival/Buildings/BuildingExpansion/Interface/BuildingExpansionOwner.h"
#include "RTS_Survival/Buildings/FieldTrench/Anchors/FieldTrenchAnchorComponent.h"
#include "RTS_Survival/Buildings/FieldTrench/Anchors/FieldTrenchAnchorSubsystem.h"
#include "RTS_Survival/Buildings/FieldTrench/FieldTrenchLayout.h"
#include "RTS_Survival/Buildings/FieldTrench/RTSFieldTrench.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowComp.h"
#include "RTS_Survival/GameUI/BuildingExpansion/WidgetBxpOptionState/BxpOptionData.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/GlobalAbilitySetDataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/RTSGlobalAbilitySettings.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/AircraftStrafing/GA_AircraftStrafing.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderTypes.h"
#include "RTS_Survival/Navigation/CoverFinder/CoverFinderWorldSubsystem.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/ConstructionPreview/CPPConstructionPreview.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Player/RTSCheatManager/RTSCheatManager.h"
#include "RTS_Survival/Player/SquadMovePreview/SquadMovePreviewComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/RTSComponents/RTSTargetAcquisition/RTSEngagementStance/RTSAggroBehaviour.h"
#include "RTS_Survival/SupplyPoints/SupplyPoint.h"
#include "RTS_Survival/SupplyPoints/SupplyPointSubsystem.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeapon.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"

DEFINE_LOG_CATEGORY_STATIC(LogGMREvents, Log, All);

namespace GMRDirectorEventsPrivate
{
	constexpr float GroundTraceHalfHeight = 50000.f;
	constexpr float NavProjectExtent = 600.f;
	constexpr float SpawnHeightOffset = 100.f;
	constexpr float DefaultResourceAmount = 100000.f;
	constexpr double ExpansionTimeoutSeconds = 20.0;
	constexpr float DefaultClaimSeconds = 25.f;
	constexpr float DefaultDigSecondsPerMetre = 0.5f;
	constexpr float AnchorSearchRadius = 2500.f;
	constexpr float TrenchLengthSlack = 1.6f;
	const FName ClaimedActorTag(TEXT("GMR_Claimed"));

	FString NormaliseClassPath(const FString& ClassPath)
	{
		if (ClassPath.IsEmpty() || ClassPath.EndsWith(TEXT("_C")))
		{
			return ClassPath;
		}
		if (ClassPath.Contains(TEXT(".")))
		{
			return ClassPath + TEXT("_C");
		}
		return ClassPath + TEXT(".") + FPaths::GetBaseFilename(ClassPath) + TEXT("_C");
	}

	int32 GetOwningPlayerOf(const AActor* Actor)
	{
		const URTSComponent* const RTSComponent = IsValid(Actor) ? Actor->FindComponentByClass<URTSComponent>() : nullptr;
		return IsValid(RTSComponent) ? RTSComponent->GetOwningPlayer() : -1;
	}

	int64 GetEnumValue(const TCHAR* EnumPath, const FString& Name)
	{
		const UEnum* const Enum = FindObject<UEnum>(nullptr, EnumPath);
		if (Enum == nullptr || Name.IsEmpty())
		{
			return INDEX_NONE;
		}
		int64 Value = Enum->GetValueByNameString(Name);
		if (Value == INDEX_NONE)
		{
			Value = Enum->GetValueByNameString(Enum->GenerateEnumPrefix() + TEXT("_") + Name);
		}
		return Value;
	}

	void ForceAnimationTicking(AActor& Actor)
	{
		TInlineComponentArray<USkeletalMeshComponent*> Meshes(&Actor);
		for (USkeletalMeshComponent* const Mesh : Meshes)
		{
			Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		}
	}

	void HideWidgetComponents(AActor& Actor)
	{
		TInlineComponentArray<UWidgetComponent*> Widgets(&Actor);
		for (UWidgetComponent* const Widget : Widgets)
		{
			if (not Widget->bHiddenInGame)
			{
				Widget->SetHiddenInGame(true);
			}
		}
	}

	bool GetIsAnyAircraft(const AActor* Actor)
	{
		static const UClass* AircraftClass = FindObject<UClass>(nullptr, TEXT("/Script/RTS_Survival.AircraftMaster"));
		return AircraftClass != nullptr && IsValid(Actor) && Actor->IsA(AircraftClass);
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Helpers

TArray<AActor*> AGMRDirector::GetEventUnits(const FGMREvent& Event) const
{
	if (Event.Index < 0)
	{
		return GetLiveTaggedUnits(Event.Tag);
	}
	TArray<AActor*> Units;
	const FGMRTaggedUnits* Tagged = M_TaggedUnits.Find(Event.Tag);
	if (Tagged != nullptr && Tagged->Units.IsValidIndex(Event.Index) && Tagged->Units[Event.Index].IsValid())
	{
		AActor* Unit = Tagged->Units[Event.Index].Get();
		if (Unit->IsUnitAlive())
		{
			Units.Add(Unit);
		}
	}
	return Units;
}

FVector AGMRDirector::GetGroundLocation(const FVector& Location, const bool bProjectToNav) const
{
	using namespace GMRDirectorEventsPrivate;
	FVector Result = Location;
	FHitResult Hit;
	FCollisionObjectQueryParams ObjectParams(ECC_WorldStatic);
	const FVector Up(0.f, 0.f, GroundTraceHalfHeight);
	if (GetWorld()->LineTraceSingleByObjectType(Hit, Location + Up, Location - Up, ObjectParams))
	{
		Result = Hit.ImpactPoint;
	}
	if (not bProjectToNav)
	{
		return Result;
	}
	UNavigationSystemV1* NavigationSystem = UNavigationSystemV1::GetCurrent(GetWorld());
	FNavLocation NavLocation;
	if (IsValid(NavigationSystem) && NavigationSystem->ProjectPointToNavigation(Result, NavLocation, FVector(NavProjectExtent)))
	{
		Result = NavLocation.Location;
	}
	return Result;
}

void AGMRDirector::PlayUnitVoiceLine(AActor* Unit, const FString& VoiceLineName, const bool bForce) const
{
	using namespace GMRDirectorEventsPrivate;
	if (VoiceLineName.IsEmpty() || not IsValid(Unit) || not GetIsValidPlayerController())
	{
		return;
	}
	const int64 Value = GetEnumValue(TEXT("/Script/RTS_Survival.ERTSVoiceLine"), VoiceLineName);
	if (Value == INDEX_NONE)
	{
		UE_LOG(LogGMREvents, Warning, TEXT("GMR_VOICE_UNKNOWN %s"), *VoiceLineName);
		return;
	}
	M_PlayerController->PlayVoiceLine(Unit, static_cast<ERTSVoiceLine>(Value), bForce, false);
}

void AGMRDirector::DisableUnitOptimisation() const
{
	// Units off screen tick slowly and freeze their deploy animations; the camera visits them later.
	if (not GetIsValidPlayerController())
	{
		return;
	}
	if (not IsValid(M_PlayerController->CheatManager))
	{
		M_PlayerController->EnableCheats();
	}
	if (const URTSCheatManager* const Cheats = Cast<URTSCheatManager>(M_PlayerController->CheatManager))
	{
		Cheats->RTSOptimizeUnits(false);
	}
}

int32 AGMRDirector::ClearPreplacedUnits() const
{
	using namespace GMRDirectorEventsPrivate;
	if (M_Job.ClearUnitsRadius <= 0.f && M_Job.ClearPlayers.IsEmpty())
	{
		return 0;
	}
	TArray<AActor*> ToDestroy;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		const bool bIsUnit = Actor->IsA<ASelectablePawnMaster>() || Actor->IsA<ASquadController>();
		if (not bIsUnit || M_PreparedUnits.Contains(Actor) || Actor->Tags.Contains(FName(TEXT("GMR_Recorded"))))
		{
			continue;
		}
		const bool bInRadius = M_Job.ClearUnitsRadius > 0.f
			&& FVector::Dist2D(Actor->GetActorLocation(), M_AnchorLocation) < M_Job.ClearUnitsRadius;
		if (bInRadius || M_Job.ClearPlayers.Contains(GetOwningPlayerOf(Actor)))
		{
			ToDestroy.Add(Actor);
		}
	}
	for (AActor* Actor : ToDestroy)
	{
		if (ASquadController* Squad = Cast<ASquadController>(Actor))
		{
			for (ASquadUnit* Soldier : Squad->GetSquadUnitsChecked())
			{
				if (IsValid(Soldier))
				{
					Soldier->Destroy();
				}
			}
		}
		Actor->Destroy();
	}
	return ToDestroy.Num();
}

// ---------------------------------------------------------------------------------------------------------------
// Per-frame upkeep

void AGMRDirector::PrepareNewUnit(AActor* Unit)
{
	using namespace GMRDirectorEventsPrivate;
	if (not IsValid(Unit) || M_PreparedUnits.Contains(Unit))
	{
		return;
	}
	M_PreparedUnits.Add(Unit);
	// Out of the fog system: always visible and targetable, whatever sees it.
	if (UFowComp* const Fow = Unit->FindComponentByClass<UFowComp>())
	{
		Fow->StopFowParticipation();
	}
	ForceAnimationTicking(*Unit);
	TArray<AActor*> Attached;
	Unit->GetAttachedActors(Attached, true, true);
	for (AActor* const Child : Attached)
	{
		if (IsValid(Child))
		{
			ForceAnimationTicking(*Child);
		}
	}
}

void AGMRDirector::KeepUnitsReady()
{
	using namespace GMRDirectorEventsPrivate;
	const FName InEnemyVisionTag = DeveloperSettings::GamePlay::FoW::TagUnitInEnemyVision;
	for (TPair<FString, FGMRTaggedUnits>& Pair : M_TaggedUnits)
	{
		for (const TWeakObjectPtr<AActor>& Weak : Pair.Value.Units)
		{
			AActor* const Unit = Weak.Get();
			if (not IsValid(Unit) || Unit->Tags.Contains(ClaimedActorTag))
			{
				continue;
			}
			PrepareNewUnit(Unit);
			const int32 OwningPlayer = GetOwningPlayerOf(Unit);
			if (ASquadController* const Squad = Cast<ASquadController>(Unit))
			{
				// Soldiers spawn later with their own Blueprint's owner; give them the squad's.
				for (ASquadUnit* const Soldier : Squad->GetSquadUnitsChecked())
				{
					if (not IsValid(Soldier))
					{
						continue;
					}
					FRTSUnitOwnerSpawning::MatchSoldierToSquadOwner(Soldier, static_cast<uint8>(OwningPlayer));
					PrepareNewUnit(Soldier);
					HideWidgetComponents(*Soldier);
					if (OwningPlayer == 1)
					{
						Soldier->Tags.AddUnique(InEnemyVisionTag);
					}
					else if (Soldier->IsHidden())
					{
						Soldier->SetActorHiddenInGame(false);
					}
				}
			}
			HideWidgetComponents(*Unit);
			if (OwningPlayer == 1)
			{
				Unit->Tags.AddUnique(InEnemyVisionTag);
			}
			else if (Unit->IsHidden())
			{
				Unit->SetActorHiddenInGame(false);
			}
		}
	}
}

void AGMRDirector::SuppressUnwantedActors()
{
	// The map's own systems keep spawning things the take must not show (survival drop ships): remove them.
	constexpr uint64 SuppressEveryFrames = 15;
	if (M_Job.SuppressClassPrefixes.IsEmpty() || GFrameCounter % SuppressEveryFrames != 0)
	{
		return;
	}
	TArray<AActor*> ToDestroy;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		const FString ClassName = It->GetClass()->GetName();
		const bool bMatch = M_Job.SuppressClassPrefixes.ContainsByPredicate([&ClassName](const FString& Prefix)
		{
			return ClassName.StartsWith(Prefix);
		});
		if (bMatch && not M_PreparedUnits.Contains(*It) && not It->Tags.Contains(FName(TEXT("GMR_Recorded"))))
		{
			ToDestroy.Add(*It);
		}
	}
	for (AActor* const Actor : ToDestroy)
	{
		UE_LOG(LogGMREvents, Display, TEXT("GMR_SUPPRESSED %s"), *Actor->GetName());
		Actor->Destroy();
	}
}

void AGMRDirector::TickPendingClaims()
{
	using namespace GMRDirectorEventsPrivate;
	const double Now = GetWorld()->GetTimeSeconds();
	for (int32 Index = M_PendingClaims.Num() - 1; Index >= 0; --Index)
	{
		const FGMRPendingClaim& Claim = M_PendingClaims[Index];
		if (Now > Claim.UntilWorldSeconds || not IsValid(Claim.ClaimClass))
		{
			M_PendingClaims.RemoveAt(Index);
			continue;
		}
		for (TActorIterator<AActor> It(GetWorld(), Claim.ClaimClass); It; ++It)
		{
			// Only actors born after the claim opened (not the map's own aircraft or earlier drops).
			const bool bOlderThanClaim = It->GetGameTimeSinceCreation() > Now - Claim.SinceWorldSeconds + KINDA_SMALL_NUMBER;
			const bool bIsChildClass = Claim.bExactClass && It->GetClass() != Claim.ClaimClass;
			if (bOlderThanClaim || bIsChildClass || M_PreparedUnits.Contains(*It)
				|| It->Tags.Contains(FName(TEXT("GMR_Recorded"))))
			{
				continue;
			}
			RegisterUnit(Claim.Tag, *It);
			M_SpawnedActors.Add(*It);
			PrepareNewUnit(*It);
			UE_LOG(LogGMREvents, Display, TEXT("GMR_CLAIMED tag=%s actor=%s"), *Claim.Tag, *It->GetName());
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Spawning and orders

FString AGMRDirector::Event_Spawn(const FGMREvent& Event)
{
	using namespace GMRDirectorEventsPrivate;
	UClass* const UnitClass = LoadClass<AActor>(nullptr, *NormaliseClassPath(Event.ClassPath));
	if (not IsValid(UnitClass))
	{
		AddManifestWarning(FString::Printf(TEXT("spawn class not found %s"), *Event.ClassPath));
		return TEXT("class not found");
	}
	const int32 Columns = Event.Columns > 0 ? Event.Columns : FMath::Max(1, Event.Count);
	const FRotator EventRotation(0.f, Event.Yaw, 0.f);
	const float FlightHeight = Event.GetSetting(TEXT("FlightHeight"), 0.f);
	int32 Spawned = 0;
	for (int32 UnitIndex = 0; UnitIndex < Event.Count; ++UnitIndex)
	{
		const int32 Row = UnitIndex / Columns;
		const int32 Column = UnitIndex % Columns;
		const int32 InRow = FMath::Min(Columns, Event.Count - Row * Columns);
		const FVector Local(-Row * Event.Spacing, (Column - (InRow - 1) * 0.5f) * Event.Spacing, 0.f);
		FVector Location = AnchorToWorld(Event.Offset + EventRotation.RotateVector(Local));
		if (FlightHeight > 0.f)
		{
			Location = GetGroundLocation(Location, false) + FVector(0.f, 0.f, FlightHeight);
		}
		else
		{
			Location = GetGroundLocation(Location, true) + FVector(0.f, 0.f, SpawnHeightOffset);
		}
		TSet<ATeamWeapon*> TeamWeaponsBefore;
		for (TActorIterator<ATeamWeapon> It(GetWorld()); It; ++It)
		{
			TeamWeaponsBefore.Add(*It);
		}
		const FTransform Transform(FRotator(0.f, AnchorYawToWorld(Event.Yaw), 0.f), Location);
		AActor* const Unit = FRTSUnitOwnerSpawning::SpawnUnitForPlayer(*GetWorld(), UnitClass, Transform,
		                                                               static_cast<uint8>(Event.Player));
		if (not IsValid(Unit))
		{
			continue;
		}
		if (Unit->IsA<ABuildingExpansion>())
		{
			// Pits stand on the ground, not 1 m above it like a vehicle that drops onto its wheels.
			Unit->SetActorLocation(Location - FVector(0.f, 0.f, SpawnHeightOffset));
		}
		RegisterUnit(Event.Tag, Unit);
		M_SpawnedActors.Add(Unit);
		PrepareNewUnit(Unit);
		for (TActorIterator<ATeamWeapon> It(GetWorld()); It; ++It)
		{
			if (not TeamWeaponsBefore.Contains(*It))
			{
				RegisterUnit(Event.Tag + TEXT("_Gun"), *It);
				M_SpawnedActors.Add(*It);
				PrepareNewUnit(*It);
			}
		}
		++Spawned;
	}
	HideGameUI();
	return FString::Printf(TEXT("spawned %d/%d %s"), Spawned, Event.Count, *UnitClass->GetName());
}

FString AGMRDirector::Event_Order(const FGMREvent& Event)
{
	const TArray<AActor*> Units = GetEventUnits(Event);
	if (Units.IsEmpty())
	{
		return TEXT("no live units");
	}
	FVector Centroid = FVector::ZeroVector;
	for (const AActor* Unit : Units)
	{
		Centroid += Unit->GetActorLocation();
	}
	Centroid /= Units.Num();
	const bool bQueue = Event.GetSetting(TEXT("Queue"), 0.f) > 0.f;
	const float FormationKeep = Event.GetSetting(TEXT("KeepFormation"), 1.f);
	const FRotator FinalRotation(0.f, AnchorYawToWorld(Event.TargetYaw), 0.f);
	const TArray<AActor*> Targets = Event.TargetTag.IsEmpty() ? TArray<AActor*>() : GetLiveTaggedUnits(Event.TargetTag);
	int32 Ordered = 0;
	for (int32 UnitIndex = 0; UnitIndex < Units.Num(); ++UnitIndex)
	{
		AActor* const Unit = Units[UnitIndex];
		ICommands* const Commands = Cast<ICommands>(Unit);
		if (Commands == nullptr)
		{
			continue;
		}
		const FVector Destination = GetGroundLocation(
			AnchorToWorld(Event.TargetOffset) + (Unit->GetActorLocation() - Centroid) * FormationKeep, true);
		ECommandQueueError Error = ECommandQueueError::NoError;
		if (Event.Type.Equals(TEXT("Move"), ESearchCase::IgnoreCase))
		{
			Error = Commands->MoveToLocation(Destination, not bQueue, FinalRotation, true);
		}
		else if (Event.Type.Equals(TEXT("AttackMove"), ESearchCase::IgnoreCase))
		{
			Error = Commands->AttackMoveToLocation(Destination, not bQueue, FinalRotation, true);
		}
		else if (Event.Type.Equals(TEXT("AttackGround"), ESearchCase::IgnoreCase))
		{
			Error = Commands->AttackGround(GetGroundLocation(AnchorToWorld(Event.TargetOffset), false), not bQueue);
		}
		else if (Event.Type.Equals(TEXT("Attack"), ESearchCase::IgnoreCase))
		{
			if (Targets.IsEmpty())
			{
				continue;
			}
			// Spread the attackers over the targets so the fight is not one tank against a whole line.
			Error = Commands->AttackActor(Targets[UnitIndex % Targets.Num()], not bQueue);
		}
		else if (Event.Type.Equals(TEXT("Stop"), ESearchCase::IgnoreCase))
		{
			Error = Commands->StopCommand();
		}
		else if (Event.Type.Equals(TEXT("Rotate"), ESearchCase::IgnoreCase))
		{
			Error = Commands->RotateTowards(FinalRotation, not bQueue);
		}
		Ordered += Error == ECommandQueueError::NoError ? 1 : 0;
	}
	PlayUnitVoiceLine(Units[0], Event.Text, Event.GetSetting(TEXT("ForceVoice"), 0.f) > 0.f);
	return FString::Printf(TEXT("ordered %d/%d"), Ordered, Units.Num());
}

FString AGMRDirector::Event_Stance(const FGMREvent& Event)
{
	using namespace GMRDirectorEventsPrivate;
	const int64 Value = GetEnumValue(TEXT("/Script/RTS_Survival.ERTSAggroBehaviour"), Event.Text);
	if (Value == INDEX_NONE)
	{
		return TEXT("unknown stance");
	}
	const ERTSAggroBehaviour Stance = static_cast<ERTSAggroBehaviour>(Value);
	int32 Changed = 0;
	for (AActor* const Unit : GetEventUnits(Event))
	{
		if (const ASquadController* const Squad = Cast<ASquadController>(Unit))
		{
			Squad->SetAggroStance(Stance);
			++Changed;
		}
		else if (ATankMaster* const Tank = Cast<ATankMaster>(Unit))
		{
			Tank->SetAggroStance(Stance);
			++Changed;
		}
	}
	return FString::Printf(TEXT("stance %s on %d"), *Event.Text, Changed);
}

FString AGMRDirector::Event_Voice(const FGMREvent& Event)
{
	const TArray<AActor*> Units = GetEventUnits(Event);
	if (Units.IsEmpty())
	{
		return TEXT("no live units");
	}
	PlayUnitVoiceLine(Units[0], Event.Text, Event.GetSetting(TEXT("ForceVoice"), 1.f) > 0.f);
	return TEXT("played ") + Event.Text;
}

FString AGMRDirector::Event_Destroy(const FGMREvent& Event)
{
	int32 Destroyed = 0;
	for (AActor* const Unit : GetEventUnits(Event))
	{
		if (ASquadController* Squad = Cast<ASquadController>(Unit))
		{
			for (ASquadUnit* Soldier : Squad->GetSquadUnitsChecked())
			{
				if (IsValid(Soldier))
				{
					Soldier->Destroy();
				}
			}
		}
		Unit->Destroy();
		++Destroyed;
	}
	return FString::Printf(TEXT("destroyed %d"), Destroyed);
}

FString AGMRDirector::Event_Marker(const FGMREvent& Event)
{
	// Location scouting only: a coloured column where a job will spawn or deploy something.
	static const TMap<FString, FLinearColor> Colours = {
		{TEXT("red"), FLinearColor(1.f, 0.05f, 0.02f)}, {TEXT("blue"), FLinearColor(0.05f, 0.2f, 1.f)},
		{TEXT("green"), FLinearColor(0.1f, 1.f, 0.1f)}, {TEXT("yellow"), FLinearColor(1.f, 0.9f, 0.05f)},
		{TEXT("white"), FLinearColor::White}, {TEXT("magenta"), FLinearColor(1.f, 0.1f, 1.f)}};
	UStaticMesh* const Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UMaterialInterface* const BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (not IsValid(Cylinder) || not IsValid(BaseMaterial))
	{
		return TEXT("marker assets missing");
	}
	constexpr float CylinderSize = 100.f;
	const float Radius = Event.Value > 0.f ? Event.Value : 300.f;
	const float Height = Event.GetSetting(TEXT("Height"), 1500.f);
	const FVector Location = GetGroundLocation(AnchorToWorld(Event.Offset), false) + FVector(0.f, 0.f, Height * 0.5f);
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStaticMeshActor* const Marker = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location,
	                                                                          FRotator::ZeroRotator, SpawnParameters);
	if (not IsValid(Marker))
	{
		return TEXT("marker spawn failed");
	}
	UStaticMeshComponent* const Mesh = Marker->GetStaticMeshComponent();
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetStaticMesh(Cylinder);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	UMaterialInstanceDynamic* const Material = UMaterialInstanceDynamic::Create(BaseMaterial, Marker);
	const FLinearColor* Colour = Colours.Find(Event.Text.ToLower());
	Material->SetVectorParameterValue(TEXT("Color"), Colour != nullptr ? *Colour : FLinearColor::White);
	Mesh->SetMaterial(0, Material);
	Marker->SetActorScale3D(FVector(Radius * 2.f / CylinderSize, Radius * 2.f / CylinderSize, Height / CylinderSize));
	M_SpawnedActors.Add(Marker);
	return FString::Printf(TEXT("marker %s at %s"), *Event.Text, *Location.ToCompactString());
}

FString AGMRDirector::Event_Hide(const FGMREvent& Event)
{
	// Runtime only: a tree or wall that blocks a shot is hidden in this render, never in the map.
	const FVector Centre = AnchorToWorld(Event.Offset);
	int32 Hidden = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		const bool bNameMatch = Event.Text.IsEmpty() || It->GetActorNameOrLabel().StartsWith(Event.Text)
			|| It->GetClass()->GetName().StartsWith(Event.Text);
		const bool bInRadius = Event.Radius <= 0.f || FVector::Dist2D(It->GetActorLocation(), Centre) < Event.Radius;
		if (not bNameMatch || not bInRadius || *It == this || It->Tags.Contains(FName(TEXT("GMR_Recorded"))))
		{
			continue;
		}
		It->SetActorHiddenInGame(true);
		It->SetActorEnableCollision(false);
		++Hidden;
	}
	return FString::Printf(TEXT("hidden %d"), Hidden);
}

FString AGMRDirector::Event_Clear(const FGMREvent& Event)
{
	using namespace GMRDirectorEventsPrivate;
	const FVector Centre = AnchorToWorld(Event.Offset);
	int32 Cleared = ClearPreplacedUnits();
	if (Event.Radius <= 0.f)
	{
		return FString::Printf(TEXT("cleared %d"), Cleared);
	}
	TArray<AActor*> ToDestroy;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		const bool bIsUnit = It->IsA<ASelectablePawnMaster>() || It->IsA<ASquadController>();
		if (bIsUnit && not M_PreparedUnits.Contains(*It) && not It->Tags.Contains(FName(TEXT("GMR_Recorded")))
			&& FVector::Dist2D(It->GetActorLocation(), Centre) < Event.Radius)
		{
			ToDestroy.Add(*It);
		}
	}
	for (AActor* Actor : ToDestroy)
	{
		Actor->Destroy();
		++Cleared;
	}
	return FString::Printf(TEXT("cleared %d"), Cleared);
}

FString AGMRDirector::Event_Resources(const FGMREvent& Event)
{
	using namespace GMRDirectorEventsPrivate;
	if (not GetIsValidPlayerController())
	{
		return TEXT("no controller");
	}
	UPlayerResourceManager* const Resources = M_PlayerController->GetPlayerResourceManager();
	const UEnum* const ResourceEnum = FindObject<UEnum>(nullptr, TEXT("/Script/RTS_Survival.ERTSResourceType"));
	if (not IsValid(Resources) || ResourceEnum == nullptr)
	{
		return TEXT("no resource manager");
	}
	const int32 Amount = FMath::RoundToInt(Event.Value > 0.f ? Event.Value : DefaultResourceAmount);
	for (int32 Index = 0; Index < ResourceEnum->NumEnums() - 1; ++Index)
	{
		const ERTSResourceType Type = static_cast<ERTSResourceType>(ResourceEnum->GetValueByIndex(Index));
		Resources->CheatAddStorage(Type, Amount);
		Resources->AddResource(Type, Amount);
	}
	return FString::Printf(TEXT("added %d of each"), Amount);
}

FString AGMRDirector::Event_Claim(const FGMREvent& Event)
{
	using namespace GMRDirectorEventsPrivate;
	if (not Event.ClassPath.IsEmpty())
	{
		// Actors of this class the game spawns from now on (a supply truck, a drop) join the tag.
		UClass* const ClaimClass = LoadClass<AActor>(nullptr, *NormaliseClassPath(Event.ClassPath));
		if (not IsValid(ClaimClass))
		{
			return TEXT("claim class not found");
		}
		FGMRPendingClaim& Claim = M_PendingClaims.AddDefaulted_GetRef();
		Claim.Tag = Event.Tag;
		Claim.ClaimClass = ClaimClass;
		Claim.SinceWorldSeconds = GetWorld()->GetTimeSeconds();
		Claim.UntilWorldSeconds = GetWorld()->GetTimeSeconds() + Event.GetSetting(TEXT("ClaimSeconds"), 60.f);
		return TEXT("claiming new ") + ClaimClass->GetName();
	}
	const FVector Centre = AnchorToWorld(Event.Offset);
	int32 Claimed = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		const bool bMatch = It->GetActorNameOrLabel().StartsWith(Event.Text) || It->GetClass()->GetName().StartsWith(Event.Text);
		const bool bInRadius = Event.Radius <= 0.f || FVector::Dist2D(It->GetActorLocation(), Centre) < Event.Radius;
		if (not bMatch || not bInRadius || *It == this)
		{
			continue;
		}
		// Claimed level actors are framed and ordered but never destroyed at cleanup.
		It->Tags.AddUnique(ClaimedActorTag);
		RegisterUnit(Event.Tag, *It);
		++Claimed;
		if (Claimed >= Event.Count && Event.Count > 0)
		{
			break;
		}
	}
	return FString::Printf(TEXT("claimed %d"), Claimed);
}

// ---------------------------------------------------------------------------------------------------------------
// Base building

FString AGMRDirector::Event_Deploy(const FGMREvent& Event)
{
	int32 Ordered = 0;
	const bool bHasYaw = Event.Settings.Contains(TEXT("Yaw"));
	for (AActor* const Unit : GetEventUnits(Event))
	{
		ANomadicVehicle* const Truck = Cast<ANomadicVehicle>(Unit);
		if (not IsValid(Truck) || not Truck->GetIsInTruckMode())
		{
			continue;
		}
		// Deploys where the truck stands, Value cm ahead, or at TargetOffset for a single truck (Index).
		FVector Location = Truck->GetActorLocation() + Truck->GetActorForwardVector() * Event.Value;
		if (Event.Index >= 0 && not Event.TargetOffset.IsNearlyZero())
		{
			Location = AnchorToWorld(Event.TargetOffset);
		}
		Location = GetGroundLocation(Location, false);
		const float Yaw = bHasYaw ? AnchorYawToWorld(Event.GetSetting(TEXT("Yaw"), 0.f)) : Truck->GetActorRotation().Yaw;
		const ECommandQueueError Error = Truck->CreateBuildingAtLocation(Location, FRotator(0.f, Yaw, 0.f), true);
		Ordered += Error == ECommandQueueError::NoError ? 1 : 0;
		if (Event.GetSetting(TEXT("Instant"), 0.f) > 0.f)
		{
			M_InstantDeploys.Add(Truck);
		}
		UE_LOG(LogGMREvents, Display, TEXT("GMR_DEPLOY truck=%s at=%s yaw=%.0f construction=%.1fs error=%d"),
		       *Truck->GetName(), *Location.ToCompactString(), Yaw, Truck->GetTotalConstructionTime(),
		       static_cast<int32>(Error));
	}
	const TArray<AActor*> Units = GetEventUnits(Event);
	if (Units.Num() > 0)
	{
		PlayUnitVoiceLine(Units[0], Event.Text, false);
	}
	return FString::Printf(TEXT("deploying %d"), Ordered);
}

void AGMRDirector::TickInstantDeploys()
{
	for (int32 Index = M_InstantDeploys.Num() - 1; Index >= 0; --Index)
	{
		ANomadicVehicle* const Truck = Cast<ANomadicVehicle>(M_InstantDeploys[Index].Get());
		if (not IsValid(Truck))
		{
			M_InstantDeploys.RemoveAt(Index);
			continue;
		}
		// False while the order has not started the conversion yet; true once it stands as a building.
		if (Truck->ShippingTest_ForceFinishConversionToBuilding())
		{
			UE_LOG(LogGMREvents, Display, TEXT("GMR_DEPLOY_INSTANT %s"), *Truck->GetName());
			M_InstantDeploys.RemoveAt(Index);
		}
	}
}

FString AGMRDirector::Event_Expand(const FGMREvent& Event)
{
	FGMRPendingExpansion& Pending = M_PendingExpansions.AddDefaulted_GetRef();
	Pending.Event = Event;
	Pending.QueuedWorldSeconds = GetWorld()->GetTimeSeconds();
	return TEXT("queued ") + Event.Text;
}

void AGMRDirector::TickPendingExpansions()
{
	using namespace GMRDirectorEventsPrivate;
	if (M_PendingExpansions.IsEmpty())
	{
		return;
	}
	FGMRPendingExpansion& Pending = M_PendingExpansions[0];
	const bool bDone = Pending.bPreviewStarted ? TryPlaceExpansion(Pending) : (TryStartExpansion(Pending), false);
	const bool bTimedOut = GetWorld()->GetTimeSeconds() - Pending.QueuedWorldSeconds > ExpansionTimeoutSeconds;
	if (bTimedOut && not bDone)
	{
		AddManifestWarning(FString::Printf(TEXT("expansion %s on %s timed out"), *Pending.Event.Text, *Pending.Event.Tag));
	}
	if (bDone || bTimedOut)
	{
		M_PendingExpansions.RemoveAt(0);
	}
}

bool AGMRDirector::TryStartExpansion(FGMRPendingExpansion& Pending)
{
	const FGMREvent& Event = Pending.Event;
	const TArray<AActor*> Buildings = GetLiveTaggedUnits(Event.Tag);
	const int32 BuildingIndex = FMath::RoundToInt(Event.GetSetting(TEXT("Building"), 0.f));
	ANomadicVehicle* const Building = Buildings.IsValidIndex(BuildingIndex) ? Cast<ANomadicVehicle>(Buildings[BuildingIndex]) : nullptr;
	if (not IsValid(Building) || not Building->IsBuildingAbleToExpand() || not GetIsValidPlayerController())
	{
		return false;
	}
	const TArray<FBxpOptionData> Options = Building->GetUnlockedBuildingExpansionTypes();
	const FBxpOptionData* Option = Options.FindByPredicate([&Event](const FBxpOptionData& Candidate)
	{
		return UEnum::GetValueAsString(Candidate.ExpansionType).EndsWith(Event.Text);
	});
	if (Option == nullptr)
	{
		FString Offered;
		for (const FBxpOptionData& Candidate : Options)
		{
			Offered += UEnum::GetValueAsString(Candidate.ExpansionType) + TEXT(" ");
		}
		AddManifestWarning(FString::Printf(TEXT("%s does not offer %s (offers: %s)"), *Building->GetName(), *Event.Text, *Offered));
		Pending.QueuedWorldSeconds = -1000.0;
		return false;
	}
	int32 Slot = Event.Index;
	const TArray<FBuildingExpansionItem>& Slots = Building->GetBuildingExpansions();
	for (int32 Index = 0; Slot < 0 && Index < Slots.Num(); ++Index)
	{
		Slot = Slots[Index].Expansion == nullptr ? Index : Slot;
	}
	if (Slot < 0)
	{
		AddManifestWarning(FString::Printf(TEXT("%s has no free expansion slot"), *Building->GetName()));
		Pending.QueuedWorldSeconds = -1000.0;
		return false;
	}
	TScriptInterface<IBuildingExpansionOwner> BxpOwner;
	BxpOwner.SetObject(Building);
	BxpOwner.SetInterface(static_cast<IBuildingExpansionOwner*>(Building));
	M_PlayerController->ExpandBuildingWithBxpAndStartPreview(*Option, BxpOwner, Slot, false);
	Pending.bPreviewStarted = true;
	Pending.SlotIndex = Slot;
	Pending.bSocket = Option->BxpConstructionRules.ConstructionType == EBxpConstructionType::Socket;
	Pending.Building = Building;
	return true;
}

bool AGMRDirector::TryPlaceExpansion(FGMRPendingExpansion& Pending)
{
	ANomadicVehicle* const Building = Cast<ANomadicVehicle>(Pending.Building.Get());
	if (not IsValid(Building) || not GetIsValidPlayerController() || not M_PlayerController->MapTest_GetIsBxpWaitingForPlacement())
	{
		return false;
	}
	const UStaticMeshComponent* const AttachMesh = Building->GetAttachToMeshComponent();
	if (not IsValid(AttachMesh) || not IsValid(M_PlayerController->CPPConstructionPreviewRef))
	{
		return false;
	}
	FVector Click = AttachMesh->GetComponentLocation();
	if (Pending.bSocket)
	{
		const TArray<UStaticMeshSocket*> FreeSockets = Building->GetFreeSocketList(Pending.SlotIndex);
		if (FreeSockets.IsEmpty() || FreeSockets[0] == nullptr)
		{
			AddManifestWarning(FString::Printf(TEXT("no free socket for %s"), *Pending.Event.Text));
			return true;
		}
		Click = AttachMesh->GetSocketLocation(FreeSockets[0]->SocketName);
	}
	// Headless there is no cursor: prime the preview at the click, then click.
	M_PlayerController->CPPConstructionPreviewRef->ShippingTest_PrimeBxpPlacementAtLocation(Click);
	const bool bPlaced = M_PlayerController->MapTest_PlaceLoadedBxpAtLocation(Click);
	UE_LOG(LogGMREvents, Display, TEXT("GMR_EXPANSION %s on %s slot=%d placed=%s at=%s"), *Pending.Event.Text,
	       *Building->GetName(), Pending.SlotIndex, bPlaced ? TEXT("true") : TEXT("false"), *Click.ToCompactString());
	if (not bPlaced)
	{
		AddManifestWarning(FString::Printf(TEXT("expansion %s was not placed"), *Pending.Event.Text));
	}
	HideGameUI();
	return true;
}

// ---------------------------------------------------------------------------------------------------------------
// Air support

FString AGMRDirector::Event_Ability(const FGMREvent& Event)
{
	using namespace GMRDirectorEventsPrivate;
	if (not GetIsValidPlayerController())
	{
		return TEXT("no controller");
	}
	const int64 AbilityValue = GetEnumValue(TEXT("/Script/RTS_Survival.EGlobalAbility"), Event.Text);
	UGlobalAbilitySetDataAsset* const AbilitySet = GetDefault<URTSGlobalAbilitySettings>()->GlobalAbilityDataAsset.LoadSynchronous();
	if (AbilityValue == INDEX_NONE || not IsValid(AbilitySet))
	{
		return TEXT("unknown ability ") + Event.Text;
	}
	UGlobalAbility* const Ability = AbilitySet->GetGLobalAbilityForTypeAsCopyFromTemplate(static_cast<EGlobalAbility>(AbilityValue)).Get();
	if (not IsValid(Ability))
	{
		return TEXT("no template for ") + Event.Text;
	}
	if (UGA_AircraftStrafing* const Strafing = Cast<UGA_AircraftStrafing>(Ability))
	{
		Strafing->SetMissionStrafeOverrides(FMath::RoundToInt(Event.GetSetting(TEXT("Aircraft"), 0.f)),
		                                    Event.GetSetting(TEXT("StrafeTime"), 0.f),
		                                    Event.GetSetting(TEXT("Repeat"), 1.f) > 0.f);
	}
	Ability->InitGlobalAbility(Event.Player, M_PlayerController->GetGlobalAbilitiesManager(), M_PlayerController.Get());
	const FVector Target = GetGroundLocation(AnchorToWorld(Event.Offset), false);
	if (Event.TargetOffset.IsNearlyZero())
	{
		Ability->ExecuteAbilityAtLocation(Target);
	}
	else
	{
		Ability->ExecuteAbilityAlongLine(Target, GetGroundLocation(AnchorToWorld(Event.TargetOffset), false));
	}
	M_ActiveAbilities.Add(Ability);
	// Claim the strike aircraft only: survival maps also fly drop ships, so a specific class is preferred.
	UClass* AircraftClass = Event.ClassPath.IsEmpty() ? nullptr : LoadClass<AActor>(nullptr, *NormaliseClassPath(Event.ClassPath));
	const bool bHasStrikeClass = IsValid(AircraftClass);
	if (not bHasStrikeClass)
	{
		AircraftClass = FindObject<UClass>(nullptr, TEXT("/Script/RTS_Survival.AircraftMaster"));
	}
	if (IsValid(AircraftClass) && not Event.Tag.IsEmpty())
	{
		FGMRPendingClaim& Claim = M_PendingClaims.AddDefaulted_GetRef();
		Claim.Tag = Event.Tag;
		Claim.ClaimClass = AircraftClass;
		Claim.bExactClass = bHasStrikeClass;
		Claim.SinceWorldSeconds = GetWorld()->GetTimeSeconds();
		Claim.UntilWorldSeconds = GetWorld()->GetTimeSeconds() + Event.GetSetting(TEXT("ClaimSeconds"), DefaultClaimSeconds);
	}
	return TEXT("executed ") + Event.Text;
}

void AGMRDirector::LogTrenchProgress()
{
	constexpr double TrenchLogIntervalSeconds = 0.5;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < M_NextTrenchLogSeconds)
	{
		return;
	}
	M_NextTrenchLogSeconds = Now + TrenchLogIntervalSeconds;
	for (const TWeakObjectPtr<AActor>& Spawned : M_SpawnedActors)
	{
		const ARTSFieldTrench* const Trench = Cast<ARTSFieldTrench>(Spawned.Get());
		if (Trench == nullptr || Trench->GetBuiltPieceCount() >= Trench->GetPieceCount())
		{
			continue;
		}
		UE_LOG(LogGMREvents, Display, TEXT("GMR_TRENCH %s built=%d/%d frame=%d"), *Trench->GetName(),
		       Trench->GetBuiltPieceCount(), Trench->GetPieceCount(), GetTimelineFrame());
	}
}

void AGMRDirector::ForceTrenchPiecesLodZero(const ARTSFieldTrench& Trench) const
{
	// The field trench kit meshes carry LODs 1-3 with no triangles (screen size 0.75 and below), so a trench vanishes a
	// few metres from the camera. This render pins LOD0 on the pieces it spawned; the assets are left as they are.
	// (The German kit pieces do not draw at all in these renders even so; jobs film the Soviet kit, Settings Kit=1.)
	for (const FFieldTrenchPieceRuntime& Piece : Trench.GetPieces())
	{
		if (IsValid(Piece.Component))
		{
			Piece.Component->bOverrideMinLOD = true;
			Piece.Component->MinLOD = 0;
			Piece.Component->SetForcedLodModel(1);
			Piece.Component->MarkRenderStateDirty();
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Trenches

FString AGMRDirector::Event_Trench(const FGMREvent& Event)
{
	using namespace GMRDirectorEventsPrivate;
	const TArray<AActor*> Pits = GetLiveTaggedUnits(Event.TargetTag);
	const int32 FromIndex = FMath::Max(0, FMath::RoundToInt(Event.GetSetting(TEXT("From"), 0.f)));
	const int32 ToIndex = FMath::RoundToInt(Event.GetSetting(TEXT("To"), 1.f));
	UFieldTrenchAnchorSubsystem* const Anchors = GetWorld()->GetSubsystem<UFieldTrenchAnchorSubsystem>();
	if (not Pits.IsValidIndex(FromIndex) || not Pits.IsValidIndex(ToIndex) || not IsValid(Anchors))
	{
		return TEXT("pits or anchor subsystem missing");
	}
	const AActor* const FromPit = Pits[FromIndex];
	const AActor* const ToPit = Pits[ToIndex];
	TArray<FFieldTrenchAnchorHandle> Free;
	Anchors->GetFreeAnchorsOfPlayer(Event.Player, Free);
	// A trench only claims its start port; its end port stays "free", and a trench started on it loops in place. So
	// starts skip every port an earlier trench used, while ends may share one (two trenches meeting at a pit's port).
	TArray<FFieldTrenchAnchorHandle> FreeStarts = Free;
	FreeStarts.RemoveAll([this](const FFieldTrenchAnchorHandle& Handle) { return M_UsedTrenchAnchors.Contains(Handle); });
	// The start anchor: the From pit's free port nearest the To pit; the end: the To pit's port nearest that.
	auto PickAnchor = [](const TArray<FFieldTrenchAnchorHandle>& Candidates, const AActor* Pit, const FVector& Towards,
	                     FFieldTrenchAnchorHandle& Out, FVector& OutLocation)
	{
		float Best = TNumericLimits<float>::Max();
		for (const FFieldTrenchAnchorHandle& Handle : Candidates)
		{
			const UFieldTrenchAnchorComponent* const Component = Handle.Get();
			if (Component == nullptr || Component->GetOwner() != Pit)
			{
				continue;
			}
			const FVector Location = Component->GetAnchorLocation(Handle.AnchorIndex);
			const float Distance = FVector::Dist2D(Location, Towards);
			if (Distance < Best)
			{
				Best = Distance;
				Out = Handle;
				OutLocation = Location;
			}
		}
		return Out.IsSet();
	};
	FFieldTrenchAnchorHandle StartAnchor;
	FFieldTrenchAnchorHandle EndAnchor;
	FVector StartLocation = FromPit->GetActorLocation();
	FVector EndLocation = ToPit->GetActorLocation();
	const bool bHasStart = PickAnchor(FreeStarts, FromPit, ToPit->GetActorLocation(), StartAnchor, StartLocation);
	PickAnchor(Free, ToPit, StartLocation, EndAnchor, EndLocation);
	for (const FFieldTrenchAnchorHandle& Used : {StartAnchor, EndAnchor})
	{
		if (Used.IsSet())
		{
			M_UsedTrenchAnchors.Add(Used);
		}
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARTSFieldTrench* const Trench = GetWorld()->SpawnActor<ARTSFieldTrench>(ARTSFieldTrench::StaticClass(),
	                                                                       FTransform(StartLocation), SpawnParameters);
	if (not IsValid(Trench))
	{
		return TEXT("trench spawn failed");
	}
	Trench->SetKit(Event.GetSetting(TEXT("Kit"), 0.f) > 0.5f ? EFieldTrenchKit::Soviet : EFieldTrenchKit::German);
	Trench->SetDigSecondsPerMetre(Event.Value > 0.f ? Event.Value : DefaultDigSecondsPerMetre);
	FFieldTrenchPlanOptions Options;
	Options.bAddEndCap = Event.GetSetting(TEXT("EndCap"), 0.f) > 0.f;
	if (bHasStart)
	{
		Trench->SetStartAnchor(StartAnchor);
		if (const UFieldTrenchAnchorComponent* const Component = StartAnchor.Get())
		{
			Options.StartPort = Component->GetAnchorPort(StartAnchor.AnchorIndex);
		}
	}
	// Optional bend: the drag passes a point pushed sideways from the middle (Settings Bend, cm).
	TArray<FVector2D> Path;
	Path.Add(FVector2D(StartLocation));
	const float Bend = Event.GetSetting(TEXT("Bend"), 0.f);
	if (Bend != 0.f)
	{
		const FVector Middle = (StartLocation + EndLocation) * 0.5f;
		const FVector Side = FVector::CrossProduct((EndLocation - StartLocation).GetSafeNormal2D(), FVector::UpVector);
		Path.Add(FVector2D(Middle + Side * Bend));
	}
	Path.Add(FVector2D(EndLocation));
	const float MaxLength = FVector::Dist2D(StartLocation, EndLocation) * TrenchLengthSlack + FMath::Abs(Bend) * 2.f;
	const TArray<FFieldTrenchPiecePlacement> Pieces = Trench->PlanOnGround(Path, MaxLength, StartLocation.Z, Options);
	for (int32 PieceIndex = 0; PieceIndex < Pieces.Num(); ++PieceIndex)
	{
		UE_LOG(LogGMREvents, Display, TEXT("GMR_TRENCH_PIECE %s %d %s at=%s yaw=%.0f"), *Event.Tag, PieceIndex,
		       *UEnum::GetValueAsString(Pieces[PieceIndex].Piece), *Pieces[PieceIndex].Location.ToCompactString(),
		       Pieces[PieceIndex].Yaw);
	}
	UE_LOG(LogGMREvents, Display, TEXT("GMR_TRENCH_PATH %s start=%s end=%s"), *Event.Tag, *StartLocation.ToCompactString(),
	       *EndLocation.ToCompactString());
	if (Event.Text.Equals(TEXT("Instant"), ESearchCase::IgnoreCase))
	{
		for (const FFieldTrenchPiecePlacement& Piece : Pieces)
		{
			Trench->AddBuiltPiece(Piece);
		}
	}
	else
	{
		Trench->StartConstruction(Pieces, Event.Player, {});
	}
	ForceTrenchPiecesLodZero(*Trench);
	RegisterUnit(Event.Tag, Trench);
	M_SpawnedActors.Add(Trench);
	return FString::Printf(TEXT("trench %d pieces, %.0f m, start anchor %s"), Pieces.Num(),
	                       FVector::Dist2D(StartLocation, EndLocation) / 100.f, bHasStart ? TEXT("yes") : TEXT("no"));
}

FString AGMRDirector::Event_Garrison(const FGMREvent& Event)
{
	// Squads take the stand-up cover points the built trench pieces publish (like the player's planned move).
	URTSCoverFinderWorldSubsystem* const Cover = GetWorld()->GetSubsystem<URTSCoverFinderWorldSubsystem>();
	const TArray<AActor*> Trenches = GetLiveTaggedUnits(Event.TargetTag);
	if (not IsValid(Cover) || not GetIsValidPlayerController())
	{
		return TEXT("no cover subsystem");
	}
	TArray<FVector> Points;
	for (const FRTSCoverPoint& Point : Cover->GetCoverPointsView())
	{
		if (Point.CoverType != ERTSCoverType::TrenchStandUp)
		{
			continue;
		}
		const AActor* const Provider = Cover->ResolveBlockingProvider(Point);
		if (Trenches.Contains(Provider))
		{
			Points.Add(Point.Location);
		}
	}
	USquadMovePreviewComponent* const Preview = M_PlayerController->FindComponentByClass<USquadMovePreviewComponent>();
	const TArray<AActor*> Units = GetEventUnits(Event);
	int32 Ordered = 0;
	for (int32 UnitIndex = 0; UnitIndex < Units.Num(); ++UnitIndex)
	{
		ASquadController* const Squad = Cast<ASquadController>(Units[UnitIndex]);
		if (not IsValid(Squad) || Points.IsEmpty())
		{
			continue;
		}
		// Spread the squads along the trench.
		const int32 PointIndex = FMath::Clamp((UnitIndex * 2 + 1) * Points.Num() / FMath::Max(1, Units.Num() * 2), 0, Points.Num() - 1);
		const FVector Target = Points[PointIndex];
		uint32 Issued = 0;
		const bool bPlanned = IsValid(Preview) && Preview->TryIssuePlannedMove({Squad}, Target, false, FRotator::ZeroRotator, false, Issued);
		if (not bPlanned)
		{
			Squad->MoveToLocation(Target, true, FRotator(0.f, AnchorYawToWorld(Event.TargetYaw), 0.f), false);
		}
		++Ordered;
	}
	if (Units.Num() > 0)
	{
		PlayUnitVoiceLine(Units[0], Event.Text, false);
	}
	return FString::Printf(TEXT("garrisoned %d squads on %d trench points (scan %s)"), Ordered, Points.Num(),
	                       Cover->GetHasCompletedFullScan() ? TEXT("done") : TEXT("pending"));
}

// ---------------------------------------------------------------------------------------------------------------
// Supply points

FString AGMRDirector::Event_Capture(const FGMREvent& Event)
{
	const FVector Centre = AnchorToWorld(Event.Offset);
	ASupplyPoint* Nearest = nullptr;
	float NearestDistance = TNumericLimits<float>::Max();
	for (TActorIterator<ASupplyPoint> It(GetWorld()); It; ++It)
	{
		const float Distance = FVector::Dist2D(It->GetActorLocation(), Centre);
		if (Distance < NearestDistance)
		{
			NearestDistance = Distance;
			Nearest = *It;
		}
	}
	if (not IsValid(Nearest))
	{
		return TEXT("no supply point");
	}
	FSupplyPointSettings Settings = Nearest->GetSupplyPointSettings();
	Settings.SuppliesReadyTime = Event.GetSetting(TEXT("SuppliesReadyTime"), Settings.SuppliesReadyTime);
	Settings.TruckLoadingTime = Event.GetSetting(TEXT("TruckLoadingTime"), Settings.TruckLoadingTime);
	Settings.CaptureDuration = Event.GetSetting(TEXT("CaptureDuration"), Settings.CaptureDuration);
	Settings.CaptureRadius = Event.GetSetting(TEXT("CaptureRadius"), Settings.CaptureRadius);
	Nearest->SetSupplyPointSettings(Settings);
	if (not Event.TargetOffset.IsNearlyZero())
	{
		if (USupplyPointSubsystem* const Subsystem = USupplyPointSubsystem::Get(this))
		{
			Subsystem->SetSupplyBaseOverride(static_cast<uint8>(Event.Player),
			                                 FTransform(GetGroundLocation(AnchorToWorld(Event.TargetOffset), true)));
		}
	}
	if (Event.Text.Equals(TEXT("Force"), ESearchCase::IgnoreCase))
	{
		Nearest->ForceCaptureByPlayer(static_cast<uint8>(Event.Player));
	}
	if (not Event.Tag.IsEmpty())
	{
		Nearest->Tags.AddUnique(GMRDirectorEventsPrivate::ClaimedActorTag);
		RegisterUnit(Event.Tag, Nearest);
	}
	return FString::Printf(TEXT("supply point %s owner=%d at %.0f m"), *Nearest->GetName(), Nearest->GetOwningPlayer(),
	                       NearestDistance / 100.f);
}
