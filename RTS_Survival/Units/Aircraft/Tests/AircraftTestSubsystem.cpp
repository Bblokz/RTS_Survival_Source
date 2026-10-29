// Copyright (C) Bas Blokzijl - All rights reserved.

#include "AircraftTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS
#include "Components/ChildActorComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "NiagaraSystem.h"
#include "NiagaraUserRedirectionParameterStore.h"
#include "RTS_Survival/Buildings/BuildingExpansion/BuildingExpansion.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/TargetPreference/TargetPreference.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "RTS_Survival/Units/Aircraft/AircraftDeath/AircraftDeathComponent.h"
#include "RTS_Survival/Units/Aircraft/AircraftDeath/AircraftWreckActor.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/Aircraft/AirBase/AircraftOwnerComp/AircraftOwnerComp.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "PhysicsEngine/BodySetup.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Units/TeamWeapons/TeamWeapon.h"
#include "RTS_Survival/Weapons/Turret/CPPTurretsMaster.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSAircraftTest, Log, All);

namespace AircraftTestPrivate
{
	constexpr float StartDelaySeconds = 3.f;
	constexpr float StatusIntervalSeconds = 2.f;
	constexpr float CameraTraceLength = 200000.f;
	constexpr float GroundTraceHalfHeight = 50000.f;
	constexpr uint8 HumanPlayer = 1;
	constexpr uint8 EnemyPlayer = 2;

	// The anti-air units stand in a ring around the anchor; the aircraft circles above its centre.
	constexpr float AntiAirRingRadius = 1800.f;
	constexpr float TargetAircraftHeightAboveGround = 1800.f;
	constexpr float AntiAirTimeoutSeconds = 90.f;
	// Health the target aircraft is held at so it survives until every unit has fired.
	constexpr float TargetAircraftTestHealth = 1000000.f;

	struct FAntiAirUnitDefinition
	{
		const TCHAR* Label;
		const TCHAR* ClassPath;
		// Rounds the unit must fire at the aircraft; a gun its crew loads round by round fires slowly.
		int32 ShotsNeeded = 3;
	};

	const TCHAR* const BxpRoot = TEXT("/Game/RTS_Survival/Blueprints/Buildings/BuildingExpansions/");
	const TCHAR* const VehicleRoot = TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/");
	const TCHAR* const TeamWeaponRoot = TEXT("/Game/RTS_Survival/Blueprints/Squads/TeamWeapons/");
	// A squad's team weapon arrives next to the squad; the nearest one within this distance is the squad's.
	constexpr float TeamWeaponSearchRadius = 1200.f;

	TArray<FAntiAirUnitDefinition> GetAntiAirUnitDefinitions()
	{
		return {
			{TEXT("RusBofors"), TEXT("RusBunkers/Rus_BXP_Bofors.Rus_BXP_Bofors_C")},
			{TEXT("Ger37mmFlak"), TEXT("Childs/37mmFlak/RTS_BP_BXP_37mmFlak.RTS_BP_BXP_37mmFlak_C")},
			{TEXT("Ger88mmFlak"), TEXT("Childs/88mmFlak/RTS_BP_BXP_Flak88.RTS_BP_BXP_Flak88_C")},
			{TEXT("GerFlak37Pit"), TEXT("GerPits/Ger_BXP_GerPit_Flak37.Ger_BXP_GerPit_Flak37_C")},
			{TEXT("GerFlakBunker"), TEXT("GerBxps/Bunkers/FlakBunker/Ger_BXP_FlakBunker.Ger_BXP_FlakBunker_C")},
			{TEXT("GerPanzerIIIAA"), TEXT("@Ger_Medium/PanzerIII/Ger_Vehicle_PanzerIII_AA.Ger_Vehicle_PanzerIII_AA_C")},
			{TEXT("RusT34AA"), TEXT("@Rus_Medium/T34/Rus_Vehicle_T34AA.Rus_Vehicle_T34AA_C")},
			// Crewed guns traverse slowly; an aircraft circling close by outruns the barrel most of the time.
			{TEXT("GerFlak37TeamWeapon"), TEXT("#Flak37mm/Ger_SquadTW_Flak37MM.Ger_SquadTW_Flak37MM_C"), 1},
			{TEXT("GerFlak88TeamWeapon"), TEXT("#Flak88Wheels/Ger_SquadTW_Flak88.Ger_SquadTW_Flak88_C"), 1},
		};
	}

	FString ResolveAntiAirClassPath(const FString& RelativePath)
	{
		if (RelativePath.StartsWith(TEXT("@")))
		{
			return FString(VehicleRoot) + RelativePath.RightChop(1);
		}
		if (RelativePath.StartsWith(TEXT("#")))
		{
			return FString(TeamWeaponRoot) + RelativePath.RightChop(1);
		}
		return FString(BxpRoot) + RelativePath;
	}

	const TCHAR* const TargetAircraftClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/BF109/Ger_Aircraft_BF110Airborne.Ger_Aircraft_BF110Airborne_C");

	/**
	 * @brief Sets the owner on every RTS component template of a Blueprint class and its parents, so a unit spawned
	 * from it is built for that player; Blueprint-added components only exist after construction, too late to set.
	 * @param UnitClass The Blueprint class about to be spawned.
	 * @param OwningPlayer The owner to build the unit for.
	 * @param OutRestore Every template changed with its previous owner, to put back after the spawn.
	 */
	void OverrideRTSComponentTemplateOwners(UClass* UnitClass, const uint8 OwningPlayer,
	                                        TArray<TPair<TWeakObjectPtr<UObject>, uint8>>& OutRestore)
	{
		const FByteProperty* OwnerProperty = FindFProperty<FByteProperty>(URTSComponent::StaticClass(), TEXT("OwningPlayer"));
		if (OwnerProperty == nullptr)
		{
			return;
		}
		TArray<UActorComponent*> Templates;
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
					Templates.Add(Node != nullptr ? Node->ComponentTemplate : nullptr);
				}
			}
			if (UInheritableComponentHandler* Handler = BlueprintClass->GetInheritableComponentHandler(false))
			{
				for (auto RecordIterator = Handler->CreateRecordIterator(); RecordIterator; ++RecordIterator)
				{
					Templates.Add(RecordIterator->ComponentTemplate);
				}
			}
		}
		for (UActorComponent* Template : Templates)
		{
			if (not IsValid(Template) || not Template->IsA<URTSComponent>())
			{
				continue;
			}
			uint8* Value = OwnerProperty->ContainerPtrToValuePtr<uint8>(Template);
			OutRestore.Add({Template, *Value});
			*Value = OwningPlayer;
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

	// The ground target of the second anti-air phase: an enemy tank in the middle of the ring.
	const TCHAR* const GroundTargetClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Medium/T34/Rus_Vehicle_T3476.Rus_Vehicle_T3476_C");
	constexpr float GroundTargetLift = 100.f;
	constexpr float AntiAirGroundTimeoutSeconds = 60.f;

	bool GetIsTurretTargetingActor(const ACPPTurretsMaster* Turret, const AActor* Target)
	{
		return IsValid(Turret) && IsValid(Target) && Turret->GetCurrentTargetActor() == Target;
	}
}

bool UAircraftTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), TEXT("AntiAirValidate"))
		|| FParse::Param(FCommandLine::Get(), TEXT("AircraftDeathValidate"))
		|| FParse::Param(FCommandLine::Get(), TEXT("AircraftVtoValidate"))
		|| FParse::Param(FCommandLine::Get(), TEXT("AircraftStrafeValidate"));
}

TStatId UAircraftTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAircraftTestSubsystem, STATGROUP_Tickables);
}

void UAircraftTestSubsystem::Tick(const float DeltaTime)
{
	const UWorld* World = GetWorld();
	if (not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	switch (M_Phase)
	{
	case EAircraftTestPhase::WaitingForStart:
		TickWaitingForStart();
		return;
	case EAircraftTestPhase::AntiAirEngaging:
		TickAntiAir();
		return;
	case EAircraftTestPhase::AntiAirGroundEngaging:
		TickAntiAirGround();
		return;
	case EAircraftTestPhase::AircraftDeathRunning:
		TickAircraftDeath();
		return;
	case EAircraftTestPhase::AircraftVtoRunning:
		TickAircraftVto();
		return;
	case EAircraftTestPhase::AircraftStrafeRunning:
		TickAircraftStrafe();
		return;
	case EAircraftTestPhase::Inactive:
	case EAircraftTestPhase::Finished:
		return;
	}
}

void UAircraftTestSubsystem::Check(const bool bCondition, const FString& Description, const FString& Details)
{
	if (bCondition)
	{
		++M_PassedCheckCount;
		UE_LOG(LogRTSAircraftTest, Display, TEXT("RTS_AIRCRAFT_TEST CHECK PASS %s %s"), *Description, *Details);
		return;
	}
	++M_FailedCheckCount;
	UE_LOG(LogRTSAircraftTest, Error, TEXT("RTS_AIRCRAFT_TEST CHECK FAIL %s %s"), *Description, *Details);
}

float UAircraftTestSubsystem::GetNow() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.f;
}

void UAircraftTestSubsystem::EnterPhase(const EAircraftTestPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartSeconds = GetNow();
	M_NextStatusSeconds = M_PhaseStartSeconds;
}

void UAircraftTestSubsystem::Finish()
{
	const bool bPassed = M_FailedCheckCount == 0 && M_PassedCheckCount > 0;
	UE_LOG(LogRTSAircraftTest, Display, TEXT("RTS_AIRCRAFT_TEST RESULT %s passed=%d failed=%d"),
	       bPassed ? TEXT("PASS") : TEXT("FAIL"), M_PassedCheckCount, M_FailedCheckCount);
	M_Phase = EAircraftTestPhase::Finished;
	if (APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld()))
	{
		Controller->ConsoleCommand(TEXT("quit"));
	}
}

void UAircraftTestSubsystem::TickWaitingForStart()
{
	using namespace AircraftTestPrivate;
	ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	if (not IsValid(Controller))
	{
		return;
	}
	// The map boots paused behind the start-game widget.
	if (Controller->GetIsGameOnPause())
	{
		Controller->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
	if (M_PhaseStartSeconds < 0.f)
	{
		M_PhaseStartSeconds = GetNow();
		return;
	}
	if (GetNow() - M_PhaseStartSeconds < StartDelaySeconds)
	{
		return;
	}
	if (not FindAnchor(M_Anchor))
	{
		Check(false, TEXT("found_ground_anchor"));
		Finish();
		return;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("AntiAirValidate")))
	{
		StartAntiAir();
		return;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("AircraftVtoValidate")))
	{
		StartAircraftVto();
		return;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("AircraftStrafeValidate")))
	{
		StartAircraftStrafe();
		return;
	}
	StartAircraftDeath();
}

bool UAircraftTestSubsystem::FindAnchor(FVector& OutAnchor) const
{
	using namespace AircraftTestPrivate;
	const UWorld* World = GetWorld();
	const APlayerController* Controller = FRTS_Statics::GetRTSController(World);
	if (not IsValid(World) || not IsValid(Controller) || not IsValid(Controller->PlayerCameraManager))
	{
		return false;
	}
	// -AircraftTestAnchor=X,Y puts the test somewhere else than where the camera looks.
	FString AnchorText;
	if (FParse::Value(FCommandLine::Get(), TEXT("AircraftTestAnchor="), AnchorText))
	{
		TArray<FString> Parts;
		AnchorText.ParseIntoArray(Parts, TEXT(","));
		if (Parts.Num() >= 2)
		{
			return GetGroundAt(FVector2D(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1])), OutAnchor);
		}
	}
	const FVector CameraLocation = Controller->PlayerCameraManager->GetCameraLocation();
	const FVector CameraForward = Controller->PlayerCameraManager->GetCameraRotation().Vector();
	FHitResult GroundHit;
	if (not World->LineTraceSingleByChannel(GroundHit, CameraLocation, CameraLocation + CameraForward * CameraTraceLength,
	                                        ECC_Visibility))
	{
		return false;
	}
	return GetGroundAt(FVector2D(GroundHit.Location), OutAnchor);
}

bool UAircraftTestSubsystem::GetGroundAt(const FVector2D& Location2D, FVector& OutGround) const
{
	using namespace AircraftTestPrivate;
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	const FVector Start(Location2D.X, Location2D.Y, GroundTraceHalfHeight);
	const FVector End(Location2D.X, Location2D.Y, -GroundTraceHalfHeight);
	FHitResult GroundHit;
	FCollisionObjectQueryParams ObjectParams(ECC_WorldStatic);
	if (not World->LineTraceSingleByObjectType(GroundHit, Start, End, ObjectParams))
	{
		return false;
	}
	OutGround = GroundHit.Location;
	return true;
}

AActor* UAircraftTestSubsystem::SpawnUnitForPlayer(const FString& ClassPath, const FTransform& Transform,
                                                   const uint8 OwningPlayer) const
{
	UWorld* World = GetWorld();
	UClass* UnitClass = LoadClass<AActor>(nullptr, *ClassPath);
	if (not IsValid(World) || not IsValid(UnitClass))
	{
		return nullptr;
	}
	// The owner must be in place before BeginPlay, so the unit registers, sees and shoots as that player from the start.
	TArray<TPair<TWeakObjectPtr<UObject>, uint8>> TemplateOwnersToRestore;
	AircraftTestPrivate::OverrideRTSComponentTemplateOwners(UnitClass, OwningPlayer, TemplateOwnersToRestore);
	AActor* Unit = World->SpawnActorDeferred<AActor>(UnitClass, Transform, nullptr, nullptr,
	                                                 ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (not IsValid(Unit))
	{
		AircraftTestPrivate::RestoreRTSComponentTemplateOwners(TemplateOwnersToRestore);
		return nullptr;
	}
	// Components the C++ class creates exist already; Blueprint-added ones take the template value above.
	if (URTSComponent* RTSComponent = Unit->FindComponentByClass<URTSComponent>())
	{
		RTSComponent->SetOwningPlayerRuntime(OwningPlayer);
	}
	Unit->FinishSpawning(Transform);
	AircraftTestPrivate::RestoreRTSComponentTemplateOwners(TemplateOwnersToRestore);
	const URTSComponent* SpawnedRTSComponent = Unit->FindComponentByClass<URTSComponent>();
	const FBoolProperty* AddToGameStateProperty = FindFProperty<FBoolProperty>(URTSComponent::StaticClass(),
	                                                                          TEXT("bAddToGameState"));
	const bool bAddsToGameState = IsValid(SpawnedRTSComponent) && AddToGameStateProperty != nullptr
		&& AddToGameStateProperty->GetPropertyValue_InContainer(SpawnedRTSComponent);
	UE_LOG(LogRTSAircraftTest, Display, TEXT("RTS_AIRCRAFT_TEST SPAWNED %s owner=%d wanted=%d templates=%d add_to_game_state=%d unit_type=%d"),
	       *Unit->GetName(), IsValid(SpawnedRTSComponent) ? SpawnedRTSComponent->GetOwningPlayer() : -1, OwningPlayer,
	       TemplateOwnersToRestore.Num(), bAddsToGameState,
	       IsValid(SpawnedRTSComponent) ? static_cast<int32>(SpawnedRTSComponent->GetUnitType()) : -1);
	if (APawn* Pawn = Cast<APawn>(Unit); IsValid(Pawn) && not Pawn->GetController())
	{
		Pawn->SpawnDefaultController();
	}
	return Unit;
}

// ---------------- Anti-air ----------------

void UAircraftTestSubsystem::StartAntiAir()
{
	UE_LOG(LogRTSAircraftTest, Display, TEXT("RTS_AIRCRAFT_TEST START anti_air anchor=%s"), *M_Anchor.ToCompactString());
	SpawnAntiAirUnits();
	SpawnTargetAircraft();
	if (not M_TargetAircraft.IsValid())
	{
		Finish();
		return;
	}
	EnterPhase(EAircraftTestPhase::AntiAirEngaging);
}

void UAircraftTestSubsystem::SpawnAntiAirUnits()
{
	using namespace AircraftTestPrivate;
	FString OnlyLabel;
	FParse::Value(FCommandLine::Get(), TEXT("AntiAirUnit="), OnlyLabel);
	const TArray<FAntiAirUnitDefinition> Definitions = GetAntiAirUnitDefinitions();
	for (int32 DefinitionIndex = 0; DefinitionIndex < Definitions.Num(); ++DefinitionIndex)
	{
		const FAntiAirUnitDefinition& Definition = Definitions[DefinitionIndex];
		if (not OnlyLabel.IsEmpty() && OnlyLabel != Definition.Label)
		{
			continue;
		}
		const float AngleDegrees = 360.f * DefinitionIndex / Definitions.Num();
		const FVector2D Offset = FVector2D(FRotator(0.f, AngleDegrees, 0.f).Vector()) * AntiAirRingRadius;
		FVector Ground;
		if (not GetGroundAt(FVector2D(M_Anchor) + Offset, Ground))
		{
			Check(false, FString(Definition.Label) + TEXT("_ground_found"));
			continue;
		}
		FAntiAirUnitUnderTest Unit;
		Unit.Label = Definition.Label;
		Unit.ClassPath = ResolveAntiAirClassPath(Definition.ClassPath);
		Unit.ShotsNeeded = Definition.ShotsNeeded;
		// Facing away from the centre, so a turret must traverse to find the aircraft.
		Unit.Unit = SpawnUnitForPlayer(Unit.ClassPath, FTransform(FRotator(0.f, AngleDegrees, 0.f), Ground), HumanPlayer);
		Check(Unit.Unit.IsValid(), Unit.Label + TEXT("_spawned"), Unit.ClassPath);
		if (Unit.Unit.IsValid())
		{
			M_AntiAirUnits.Add(Unit);
		}
	}
}

void UAircraftTestSubsystem::SpawnTargetAircraft()
{
	using namespace AircraftTestPrivate;
	const FVector SpawnLocation = M_Anchor + FVector(0.f, 0.f, TargetAircraftHeightAboveGround);
	AActor* Aircraft = SpawnUnitForPlayer(TargetAircraftClassPath, FTransform(SpawnLocation), EnemyPlayer);
	M_TargetAircraft = Cast<AAircraftMaster>(Aircraft);
	Check(M_TargetAircraft.IsValid(), TEXT("target_aircraft_spawned"), TargetAircraftClassPath);
	if (not M_TargetAircraft.IsValid())
	{
		return;
	}
	KeepTargetAircraftAlive();
	M_TargetAircraftDamageTaken = 0.f;
}

void UAircraftTestSubsystem::CollectTurrets(FAntiAirUnitUnderTest& Unit) const
{
	if (not Unit.Unit.IsValid() || Unit.Turrets.Num() > 0)
	{
		return;
	}
	TArray<UChildActorComponent*> ChildActors;
	Unit.Unit->GetComponents<UChildActorComponent>(ChildActors);
	for (const UChildActorComponent* ChildActor : ChildActors)
	{
		ACPPTurretsMaster* Turret = IsValid(ChildActor) ? Cast<ACPPTurretsMaster>(ChildActor->GetChildActor()) : nullptr;
		if (IsValid(Turret))
		{
			Unit.Turrets.Add(Turret);
		}
	}
	if (ACPPTurretsMaster* UnitAsTurret = Cast<ACPPTurretsMaster>(Unit.Unit.Get()))
	{
		Unit.Turrets.AddUnique(UnitAsTurret);
	}
	if (Unit.Turrets.IsEmpty())
	{
		// A team weapon squad: its gun is an actor of its own next to the crew; the nearest one is the squad's.
		ATeamWeapon* NearestTeamWeapon = nullptr;
		float NearestDistance = AircraftTestPrivate::TeamWeaponSearchRadius;
		for (TActorIterator<ATeamWeapon> TeamWeaponIterator(GetWorld()); TeamWeaponIterator; ++TeamWeaponIterator)
		{
			const float Distance = FVector::Dist2D(TeamWeaponIterator->GetActorLocation(), Unit.Unit->GetActorLocation());
			if (Distance < NearestDistance)
			{
				NearestDistance = Distance;
				NearestTeamWeapon = *TeamWeaponIterator;
			}
		}
		if (IsValid(NearestTeamWeapon))
		{
			Unit.Turrets.Add(NearestTeamWeapon);
		}
	}
	if (not Unit.Unit->IsA<ABuildingExpansion>())
	{
		return;
	}
	// A building expansion placed on its own is never constructed; finishing construction starts its turrets.
	for (const TWeakObjectPtr<ACPPTurretsMaster>& Turret : Unit.Turrets)
	{
		if (Turret.IsValid())
		{
			Turret->SetAutoEngageTargets(false);
		}
	}
}

int32 UAircraftTestSubsystem::CountShots(FAntiAirUnitUnderTest& Unit) const
{
	int32 FiredSinceLastCount = 0;
	int32 WeaponSlot = 0;
	for (const TWeakObjectPtr<ACPPTurretsMaster>& WeakTurret : Unit.Turrets)
	{
		ACPPTurretsMaster* Turret = WeakTurret.Get();
		if (not IsValid(Turret))
		{
			continue;
		}
		const TArray<UWeaponState*> Weapons = Turret->GetWeapons();
		for (const UWeaponState* Weapon : Weapons)
		{
			const int32 Magazine = IsValid(Weapon) ? Weapon->GetCurrentMagCapacity() : 0;
			if (not Unit.LastMagazineCounts.IsValidIndex(WeaponSlot))
			{
				Unit.LastMagazineCounts.Add(Magazine);
				++WeaponSlot;
				continue;
			}
			const int32 Fired = Unit.LastMagazineCounts[WeaponSlot] - Magazine;
			FiredSinceLastCount += Fired > 0 ? Fired : 0;
			Unit.LastMagazineCounts[WeaponSlot] = Magazine;
			++WeaponSlot;
		}
	}
	return FiredSinceLastCount;
}

void UAircraftTestSubsystem::KeepTargetAircraftAlive()
{
	using namespace AircraftTestPrivate;
	if (not M_TargetAircraft.IsValid())
	{
		return;
	}
	UHealthComponent* Health = M_TargetAircraft->GetHealthComponent();
	if (not IsValid(Health))
	{
		return;
	}
	if (Health->GetMaxHealth() < TargetAircraftTestHealth)
	{
		Health->SetMaxHealth(TargetAircraftTestHealth);
		Health->SetCurrentHealth(TargetAircraftTestHealth);
		return;
	}
	M_TargetAircraftDamageTaken += Health->GetMaxHealth() - Health->GetCurrentHealth();
	Health->SetCurrentHealth(Health->GetMaxHealth());
}

void UAircraftTestSubsystem::TickAntiAir()
{
	using namespace AircraftTestPrivate;
	AAircraftMaster* Aircraft = M_TargetAircraft.Get();
	if (not IsValid(Aircraft))
	{
		Check(false, TEXT("target_aircraft_survived_test"));
		FinishAntiAir();
		return;
	}
	KeepTargetAircraftAlive();
	bool bAllEngaged = M_AntiAirUnits.Num() > 0;
	for (FAntiAirUnitUnderTest& Unit : M_AntiAirUnits)
	{
		CollectTurrets(Unit);
		bool bIsTargetingAircraft = false;
		for (const TWeakObjectPtr<ACPPTurretsMaster>& Turret : Unit.Turrets)
		{
			bIsTargetingAircraft |= GetIsTurretTargetingActor(Turret.Get(), Aircraft);
			if (Turret.IsValid() && IsValid(Turret->GetCurrentTargetActor()))
			{
				Unit.LastTargetName = Turret->GetCurrentTargetActor()->GetName();
			}
		}
		if (bIsTargetingAircraft && not Unit.bHasTargetedAircraft)
		{
			Unit.bHasTargetedAircraft = true;
			Unit.FirstTargetedSeconds = GetNow() - M_PhaseStartSeconds;
		}
		const int32 Fired = CountShots(Unit);
		Unit.ShotsWhileTargetingAircraft += bIsTargetingAircraft ? Fired : 0;
		bAllEngaged &= Unit.bHasTargetedAircraft && Unit.ShotsWhileTargetingAircraft >= Unit.ShotsNeeded;
	}
	if (GetNow() >= M_NextStatusSeconds)
	{
		LogAntiAirStatus();
		M_NextStatusSeconds = GetNow() + StatusIntervalSeconds;
	}
	if (bAllEngaged || GetNow() - M_PhaseStartSeconds > AntiAirTimeoutSeconds)
	{
		FinishAntiAir();
	}
}

void UAircraftTestSubsystem::LogAntiAirStatus() const
{
	using namespace AircraftTestPrivate;
	const AAircraftMaster* Aircraft = M_TargetAircraft.Get();
	if (not IsValid(Aircraft))
	{
		return;
	}
	const UGameUnitManager* UnitManager = FRTS_Statics::GetGameUnitManager(GetWorld());
	const bool bRegistered = IsValid(UnitManager)
		&& UnitManager->ShippingTest_GetAircraftOfPlayer(EnemyPlayer).Contains(Aircraft);
	const bool bRegisteredAsPlayer = IsValid(UnitManager)
		&& UnitManager->ShippingTest_GetAircraftOfPlayer(HumanPlayer).Contains(Aircraft);
	const bool bVisible = RTSFunctionLibrary::RTSIsVisibleTarget(const_cast<AAircraftMaster*>(Aircraft), HumanPlayer);
	UE_LOG(LogRTSAircraftTest, Display,
	       TEXT("RTS_AIRCRAFT_TEST STATUS t=%.1f aircraft=%s loc=%s registered=%d registered_as_player=%d visible=%d hidden=%d landed_state=%d damage=%.0f"),
	       GetNow() - M_PhaseStartSeconds, *Aircraft->GetName(), *Aircraft->GetActorLocation().ToCompactString(),
	       bRegistered, bRegisteredAsPlayer, bVisible, Aircraft->IsHidden(), static_cast<int32>(Aircraft->GetLandedState()),
	       M_TargetAircraftDamageTaken);
	for (const FAntiAirUnitUnderTest& Unit : M_AntiAirUnits)
	{
		for (const TWeakObjectPtr<ACPPTurretsMaster>& WeakTurret : Unit.Turrets)
		{
			const ACPPTurretsMaster* Turret = WeakTurret.Get();
			if (not IsValid(Turret))
			{
				continue;
			}
			const float Distance = FVector::Dist(Turret->GetActorLocation(), Aircraft->GetActorLocation());
			UE_LOG(LogRTSAircraftTest, Display,
			       TEXT("RTS_AIRCRAFT_TEST UNIT %s turret=%s pref=%s range=%.0f dist=%.0f target=%s on_aircraft=%d shots=%d"),
			       *Unit.Label, *Turret->GetName(), *Global_GetTargetPreferenceAsString(Turret->GetTargetPreference()),
			       Turret->GetMaxWeaponRange(), Distance,
			       IsValid(Turret->GetCurrentTargetActor()) ? *Turret->GetCurrentTargetActor()->GetName() : TEXT("none"),
			       GetIsTurretTargetingActor(Turret, Aircraft), Unit.ShotsWhileTargetingAircraft);
		}
		if (Unit.Turrets.IsEmpty())
		{
			UE_LOG(LogRTSAircraftTest, Display, TEXT("RTS_AIRCRAFT_TEST UNIT %s has no turrets yet"), *Unit.Label);
		}
	}
}

void UAircraftTestSubsystem::FinishAntiAir()
{
	using namespace AircraftTestPrivate;
	LogAntiAirStatus();
	for (const FAntiAirUnitUnderTest& Unit : M_AntiAirUnits)
	{
		Check(Unit.Turrets.Num() > 0, Unit.Label + TEXT("_has_turret"));
		Check(Unit.bHasTargetedAircraft, Unit.Label + TEXT("_targeted_aircraft"),
		      FString::Printf(TEXT("after=%.1fs last_target=%s"), Unit.FirstTargetedSeconds, *Unit.LastTargetName));
		Check(Unit.ShotsWhileTargetingAircraft >= Unit.ShotsNeeded, Unit.Label + TEXT("_fired_at_aircraft"),
		      FString::Printf(TEXT("shots=%d"), Unit.ShotsWhileTargetingAircraft));
	}
	Check(M_TargetAircraftDamageTaken > 0.f, TEXT("aircraft_took_damage"),
	      FString::Printf(TEXT("damage=%.0f"), M_TargetAircraftDamageTaken));
	StartAntiAirGround();
}

void UAircraftTestSubsystem::StartAntiAirGround()
{
	using namespace AircraftTestPrivate;
	if (M_TargetAircraft.IsValid())
	{
		M_TargetAircraft->Destroy();
	}
	FVector Ground;
	if (not GetGroundAt(FVector2D(M_Anchor), Ground))
	{
		Check(false, TEXT("ground_target_spot_found"));
		Finish();
		return;
	}
	M_GroundTarget = SpawnUnitForPlayer(GroundTargetClassPath, FTransform(Ground + FVector(0.f, 0.f, GroundTargetLift)),
	                                    EnemyPlayer);
	Check(M_GroundTarget.IsValid(), TEXT("ground_target_spawned"), GroundTargetClassPath);
	if (not M_GroundTarget.IsValid())
	{
		Finish();
		return;
	}
	UE_LOG(LogRTSAircraftTest, Display, TEXT("RTS_AIRCRAFT_TEST START anti_air_ground target=%s"),
	       *M_GroundTarget->GetName());
	EnterPhase(EAircraftTestPhase::AntiAirGroundEngaging);
}

void UAircraftTestSubsystem::TickAntiAirGround()
{
	using namespace AircraftTestPrivate;
	AActor* GroundTarget = M_GroundTarget.Get();
	if (not IsValid(GroundTarget))
	{
		Check(false, TEXT("ground_target_survived_test"));
		FinishAntiAirGround();
		return;
	}
	if (UHealthComponent* Health = GroundTarget->FindComponentByClass<UHealthComponent>())
	{
		if (Health->GetMaxHealth() < TargetAircraftTestHealth)
		{
			Health->SetMaxHealth(TargetAircraftTestHealth);
		}
		Health->SetCurrentHealth(Health->GetMaxHealth());
	}
	bool bAllEngaged = M_AntiAirUnits.Num() > 0;
	for (FAntiAirUnitUnderTest& Unit : M_AntiAirUnits)
	{
		bool bIsTargetingGround = false;
		for (const TWeakObjectPtr<ACPPTurretsMaster>& Turret : Unit.Turrets)
		{
			bIsTargetingGround |= GetIsTurretTargetingActor(Turret.Get(), GroundTarget);
		}
		Unit.bHasTargetedGround |= bIsTargetingGround;
		const int32 Fired = CountShots(Unit);
		Unit.ShotsWhileTargetingGround += bIsTargetingGround ? Fired : 0;
		bAllEngaged &= Unit.bHasTargetedGround && Unit.ShotsWhileTargetingGround > 0;
	}
	if (bAllEngaged || GetNow() - M_PhaseStartSeconds > AntiAirGroundTimeoutSeconds)
	{
		FinishAntiAirGround();
	}
}

void UAircraftTestSubsystem::FinishAntiAirGround()
{
	for (const FAntiAirUnitUnderTest& Unit : M_AntiAirUnits)
	{
		Check(Unit.bHasTargetedGround, Unit.Label + TEXT("_targeted_ground_unit"));
		Check(Unit.ShotsWhileTargetingGround > 0, Unit.Label + TEXT("_fired_at_ground_unit"),
		      FString::Printf(TEXT("shots=%d"), Unit.ShotsWhileTargetingGround));
	}
	Finish();
}

// ---------------- Aircraft death ----------------

namespace AircraftDeathTestPrivate
{
	const TCHAR* const AircraftRoot = TEXT("/Game/RTS_Survival/Blueprints/Aircraft/");
	constexpr float FlightHeightAboveGround = 2200.f;
	constexpr float DeathAircraftSpacing = 4500.f;
	constexpr int32 DeathAircraftPerRow = 6;
	constexpr float ParkedHeight = 600.f;
	constexpr float ParkedSpreadCheckSeconds = 3.f;
	// Furthest a piece may lie from where the aircraft stood: about the aircraft's own size.
	constexpr float ParkedMaxPieceSpread = 1100.f;
	// Long enough for the aircraft to pick up its circling speed before it is shot down.
	constexpr float FlyBeforeKillSeconds = 3.f;
	constexpr float FallTimeoutSeconds = 25.f;
	// The aircraft actor lingers hidden while its smoke fades; it must be gone after this long.
	constexpr float CleanupTimeoutSeconds = 12.f;
	// A wreck must come down near the ground, not in the air or under the map.
	constexpr float ImpactHeightTolerance = 400.f;
	// Pieces the smallest crash collection breaks into, less some that Chaos merges.
	constexpr int32 MinimumPieces = 15;
	// The fall carries the wreck on: it lands at least this share of the way its speed at death covers in a second.
	constexpr float MinimumTravelShareOfOneSecond = 0.5f;
	const TArray<float> CaptureSecondsAfterKill = {0.5f, 1.2f};
	const TArray<float> CaptureSecondsAfterImpact = {0.15f, 1.f, 3.f};
	const FName NoseEngineBone(TEXT("B_Rotor1"));
	const TCHAR* const AirbaseTruckClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerT2/AircraftBuilding/"
		"Ger_Airbase_Truck.Ger_Airbase_Truck_C");
	constexpr float AirbaseSpawnLift = 100.f;
	// Headless runs never draw the truck, so its unpack montage is fast-forwarded after this long.
	constexpr float AirbaseFastForwardSeconds = 5.f;
	constexpr float AirbaseTimeoutSeconds = 30.f;
	// Piece centres lie at least about this far above the bay floor; lower means they fell through it.
	constexpr float BayFloorTolerance = 60.f;

	struct FDeathAircraftDefinition
	{
		const TCHAR* Label;
		const TCHAR* RelativeClassPath;
		bool bExpectWingFire;
		bool bSpawnLanded;
		bool bParkInAirbase = false;
	};

	TArray<FDeathAircraftDefinition> GetDeathAircraftDefinitions()
	{
		return {
			{TEXT("Bf109"), TEXT("GerAircraft/BF109/Ger_Aircraft_BF110Airborne.Ger_Aircraft_BF110Airborne_C"), false, false},
			{TEXT("Ju87"), TEXT("GerAircraft/JU_87VTO/Ger_Aircraft_StukaJu87_Airborne.Ger_Aircraft_StukaJu87_Airborne_C"), false, false},
			{TEXT("Sturmovik"), TEXT("RusAircraft/Sturmovic/Rus_Aircraft_Sturmovic.Rus_Aircraft_Sturmovic_C"), false, false},
			{TEXT("Me410"), TEXT("GerAircraft/ME410/Ger_Aircraft_ME5_Airbrone.Ger_Aircraft_ME5_Airbrone_C"), true, false},
			{TEXT("Pe2"), TEXT("RusAircraft/PE2/Rus_PE2_Aircraft_3x250.Rus_PE2_Aircraft_3x250_C"), true, false},
			{TEXT("Pe8"), TEXT("RusAircraft/PE8Bomber/Rus_PE8_Aircraft_8x500.Rus_PE8_Aircraft_8x500_C"), true, false},
			{TEXT("Ju390"), TEXT("GerAircraft/JU390B/Ger_Aircraft_JU390B_Airbrone.Ger_Aircraft_JU390B_Airbrone_C"), true, false},
			// Two jets (B_Rotor1/2 are the intake compressors): burns at a wing engine.
			{TEXT("Ho229"), TEXT("GerAircraft/HO229/Ger_Aircraft_Horten229_Airborne.Ger_Aircraft_Horten229_Airborne_C"), true, false},
			{TEXT("Ho229Parked"), TEXT("GerAircraft/HO229/Ger_Aircraft_Horten229.Ger_Aircraft_Horten229_C"), true, true},
			{TEXT("Bf109Parked"), TEXT("GerAircraft/BF109/Ger_Aircraft_BF109.Ger_Aircraft_BF109_C"), false, true},
			{TEXT("Bf109InAirbase"), TEXT("GerAircraft/BF109/Ger_Aircraft_BF109.Ger_Aircraft_BF109_C"), false, true, true},
			{TEXT("Ju87DropShip"), TEXT("DropShips/BP_DropShip_Ger_Ju87.BP_DropShip_Ger_Ju87_C"), false, false},
			{TEXT("Pe8DropShip"), TEXT("DropShips/BP_DropShip_Rus_PE8.BP_DropShip_Rus_PE8_C"), true, false},
			{TEXT("VtolDropship"), TEXT("BP_VTOL_Dropship.BP_VTOL_Dropship_C"), false, false},
		};
	}
}

void UAircraftTestSubsystem::StartAircraftDeath()
{
	using namespace AircraftDeathTestPrivate;
	FString OnlyLabel;
	FParse::Value(FCommandLine::Get(), TEXT("AircraftDeathOnly="), OnlyLabel);
	for (const FDeathAircraftDefinition& Definition : GetDeathAircraftDefinitions())
	{
		if (not OnlyLabel.IsEmpty() && OnlyLabel != Definition.Label)
		{
			continue;
		}
		FAircraftDeathUnderTest& Entry = M_DeathAircraft.AddDefaulted_GetRef();
		Entry.Label = Definition.Label;
		Entry.ClassPath = FString(AircraftRoot) + Definition.RelativeClassPath;
		Entry.bExpectWingFire = Definition.bExpectWingFire;
		Entry.bSpawnLanded = Definition.bSpawnLanded;
		Entry.bParkInAirbase = Definition.bParkInAirbase;
		// The player's own airbase: the aircraft in its bay is the player's too.
		Entry.OwningPlayer = Definition.bParkInAirbase ? AircraftTestPrivate::HumanPlayer : AircraftTestPrivate::EnemyPlayer;
	}
	FString CaptureDirectory;
	if (FParse::Value(FCommandLine::Get(), TEXT("AircraftDeathCapture="), CaptureDirectory) && IsValid(GetWorld()))
	{
		M_CaptureRig.Init(*GetWorld(), CaptureDirectory);
	}
	UE_LOG(LogRTSAircraftTest, Display, TEXT("RTS_AIRCRAFT_TEST START aircraft_death count=%d anchor=%s"),
	       M_DeathAircraft.Num(), *M_Anchor.ToCompactString());
	M_DeathAircraftIndex = 0;
	EnterPhase(EAircraftTestPhase::AircraftDeathRunning);
	if (M_DeathAircraft.IsEmpty() || not SpawnDeathAircraft(M_DeathAircraft[0]))
	{
		Finish();
	}
}

void UAircraftTestSubsystem::EnterDeathStep(FAircraftDeathUnderTest& Entry, const EAircraftDeathTestStep NewStep)
{
	Entry.Step = NewStep;
	Entry.StepStartSeconds = GetNow();
}

bool UAircraftTestSubsystem::SpawnDeathAircraft(FAircraftDeathUnderTest& Entry)
{
	using namespace AircraftDeathTestPrivate;
	using AircraftTestPrivate::EnemyPlayer;
	FVector Ground;
	// Each aircraft comes down on its own patch of ground, clear of the fire and smoke of the one before.
	// In rows: twelve aircraft in one line run off the test map.
	const FVector2D SpawnSpot = FVector2D(M_Anchor) + FVector2D(
		(M_DeathAircraftIndex % DeathAircraftPerRow) * DeathAircraftSpacing,
		(M_DeathAircraftIndex / DeathAircraftPerRow) * DeathAircraftSpacing);
	if (not GetGroundAt(SpawnSpot, Ground))
	{
		Check(false, Entry.Label + TEXT("_ground_found"));
		return false;
	}
	Entry.GroundZ = Ground.Z;
	if (Entry.bParkInAirbase)
	{
		return SpawnAirbaseForDeathAircraft(Entry, Ground);
	}
	const FVector SpawnLocation = Entry.bSpawnLanded ? Ground : Ground + FVector(0.f, 0.f, FlightHeightAboveGround);
	Entry.Aircraft = Cast<AAircraftMaster>(SpawnUnitForPlayer(Entry.ClassPath, FTransform(SpawnLocation),
	                                                          Entry.OwningPlayer));
	Check(Entry.Aircraft.IsValid(), Entry.Label + TEXT("_spawned"), Entry.ClassPath);
	EnterDeathStep(Entry, EAircraftDeathTestStep::Flying);
	if (Entry.Aircraft.IsValid())
	{
		ApplyDeathEffectOverrides(*Entry.Aircraft);
	}
	return Entry.Aircraft.IsValid();
}

bool UAircraftTestSubsystem::SpawnAirbaseForDeathAircraft(FAircraftDeathUnderTest& Entry, const FVector& Ground)
{
	using namespace AircraftDeathTestPrivate;
	Entry.Airbase = Cast<ANomadicVehicle>(SpawnUnitForPlayer(
		AirbaseTruckClassPath, FTransform(Ground + FVector(0.f, 0.f, AirbaseSpawnLift)), Entry.OwningPlayer));
	Check(Entry.Airbase.IsValid(), Entry.Label + TEXT("_airbase_spawned"), AirbaseTruckClassPath);
	if (not Entry.Airbase.IsValid())
	{
		return false;
	}
	ANomadicVehicle* Airbase = Entry.Airbase.Get();
	const ECommandQueueError Error = Airbase->CreateBuildingAtLocation(Airbase->GetActorLocation(),
	                                                                   Airbase->GetActorRotation(), true);
	Check(Error == ECommandQueueError::NoError, Entry.Label + TEXT("_airbase_unpack_ordered"));
	EnterDeathStep(Entry, EAircraftDeathTestStep::WaitingForAirbase);
	return true;
}

void UAircraftTestSubsystem::TickDeathWaitingForAirbase(FAircraftDeathUnderTest& Entry)
{
	using namespace AircraftDeathTestPrivate;
	ANomadicVehicle* Airbase = Entry.Airbase.Get();
	const float Waited = GetNow() - Entry.StepStartSeconds;
	if (not IsValid(Airbase) || Waited > AirbaseTimeoutSeconds)
	{
		Check(false, Entry.Label + TEXT("_airbase_unpacked"),
		      IsValid(Airbase) ? FString::Printf(TEXT("status=%d"), static_cast<int32>(Airbase->GetNomadicStatus()))
		                       : FString(TEXT("airbase gone")));
		EnterDeathStep(Entry, EAircraftDeathTestStep::Done);
		return;
	}
	if (Waited >= AirbaseFastForwardSeconds)
	{
		(void)Airbase->ShippingTest_ForceFinishConversionToBuilding();
	}
	const UAircraftOwnerComp* Bays = Airbase->GetAircraftOwnerComp();
	if (Airbase->GetNomadicStatus() != ENomadStatus::Building || not IsValid(Bays)
		|| not Bays->IsAirbaseUnPackedAsBuilding())
	{
		return;
	}
	Check(true, Entry.Label + TEXT("_airbase_unpacked"), FString::Printf(TEXT("after=%.1fs"), Waited));
	if (not ParkDeathAircraftInAirbase(Entry))
	{
		EnterDeathStep(Entry, EAircraftDeathTestStep::Done);
		return;
	}
	EnterDeathStep(Entry, EAircraftDeathTestStep::Flying);
}

bool UAircraftTestSubsystem::ParkDeathAircraftInAirbase(FAircraftDeathUnderTest& Entry)
{
	ANomadicVehicle* Airbase = Entry.Airbase.Get();
	UAircraftOwnerComp* Bays = Airbase->GetAircraftOwnerComp();
	Entry.Aircraft = Cast<AAircraftMaster>(SpawnUnitForPlayer(
		Entry.ClassPath, FTransform(Airbase->GetActorRotation(), Airbase->GetActorLocation()), Entry.OwningPlayer));
	Check(Entry.Aircraft.IsValid(), Entry.Label + TEXT("_spawned"), Entry.ClassPath);
	if (not Entry.Aircraft.IsValid())
	{
		return false;
	}
	ApplyDeathEffectOverrides(*Entry.Aircraft);
	// The way a trained aircraft appears: assigned to a free bay and put down on its landing socket.
	const bool bParked = Bays->OnAircraftTrained(Entry.Aircraft.Get(), EAircraftSubtype::Aircraft_Bf109);
	const int32 BayIndex = Bays->GetAssignedSocketIndex(Entry.Aircraft.Get());
	const bool bHasBay = bParked && Bays->GetSocketWorldLocation(BayIndex, Entry.BayLocation);
	Check(bHasBay, Entry.Label + TEXT("_parked_in_a_bay"), FString::Printf(TEXT("bay=%d"), BayIndex));
	if (bHasBay)
	{
		// The bay floor, not the open ground under the building, is where it stands.
		Entry.GroundZ = Entry.BayLocation.Z;
	}
	const UStaticMeshComponent* Building = Airbase->MapTest_GetBuildingMeshComponent();
	const UBodySetup* BuildingBody = IsValid(Building) && IsValid(Building->GetStaticMesh())
		                                ? Building->GetStaticMesh()->GetBodySetup()
		                                : nullptr;
	UE_LOG(LogRTSAircraftTest, Display,
	       TEXT("RTS_AIRCRAFT_TEST AIRBASE %s bay=%d bay_at=%s building_bounds=%s object_type=%d collision=%d trace_flag=%d boxes=%d convex=%d"),
	       *Entry.Label, BayIndex, *Entry.BayLocation.ToCompactString(),
	       IsValid(Building) ? *Building->Bounds.GetBox().ToString() : TEXT("none"),
	       IsValid(Building) ? static_cast<int32>(Building->GetCollisionObjectType()) : -1,
	       IsValid(Building) ? static_cast<int32>(Building->GetCollisionEnabled()) : -1,
	       IsValid(BuildingBody) ? static_cast<int32>(BuildingBody->CollisionTraceFlag.GetValue()) : -1,
	       IsValid(BuildingBody) ? BuildingBody->AggGeom.BoxElems.Num() : -1,
	       IsValid(BuildingBody) ? BuildingBody->AggGeom.ConvexElems.Num() : -1);
	return bHasBay;
}

void UAircraftTestSubsystem::ApplyDeathEffectOverrides(AAircraftMaster& Aircraft) const
{
	UAircraftDeathComponent* Death = Aircraft.GetDeathComponent();
	if (not IsValid(Death))
	{
		return;
	}
	FAircraftDeathSettings& Settings = Death->MapTest_GetMutableDeathSettings();
	// -AircraftDeath<Effect>Vfx=<object path|none> swaps one effect while the look of a crash is judged.
	const TArray<TPair<const TCHAR*, TObjectPtr<UNiagaraSystem>*>> Overrides = {
		{TEXT("AircraftDeathFireVfx="), &Settings.EngineFire.System},
		{TEXT("AircraftDeathSmokeVfx="), &Settings.SmokeTrail.System},
		{TEXT("AircraftDeathExplosionVfx="), &Settings.ImpactExplosion.System},
		{TEXT("AircraftDeathGroundFireVfx="), &Settings.GroundFire.System},
	};
	for (const TPair<const TCHAR*, TObjectPtr<UNiagaraSystem>*>& Override : Overrides)
	{
		FString Value;
		if (not FParse::Value(FCommandLine::Get(), Override.Key, Value))
		{
			continue;
		}
		*Override.Value = Value == TEXT("none") ? nullptr : LoadObject<UNiagaraSystem>(nullptr, *Value);
	}
	FParse::Value(FCommandLine::Get(), TEXT("AircraftDeathEffectScale="), Settings.EffectScale);
	// -AircraftDeath<Effect>Params="Name:Value|Name:X,Y,Z" replaces the user parameters of one effect.
	const TArray<TPair<const TCHAR*, FAircraftDeathEffect*>> ParameterOverrides = {
		{TEXT("AircraftDeathFireParams="), &Settings.EngineFire},
		{TEXT("AircraftDeathSmokeParams="), &Settings.SmokeTrail},
		{TEXT("AircraftDeathExplosionParams="), &Settings.ImpactExplosion},
		{TEXT("AircraftDeathGroundFireParams="), &Settings.GroundFire},
	};
	for (const TPair<const TCHAR*, FAircraftDeathEffect*>& Override : ParameterOverrides)
	{
		FString Value;
		if (not FParse::Value(FCommandLine::Get(), Override.Key, Value, false))
		{
			continue;
		}
		Override.Value->FloatParameters.Reset();
		Override.Value->VectorParameters.Reset();
		Override.Value->ColorParameters.Reset();
		TArray<FString> Entries;
		Value.ParseIntoArray(Entries, TEXT("|"));
		for (const FString& Entry : Entries)
		{
			FString Name;
			FString Number;
			if (not Entry.Split(TEXT(":"), &Name, &Number))
			{
				continue;
			}
			TArray<FString> Components;
			Number.ParseIntoArray(Components, TEXT(","));
			// "@Scale:2" scales the whole effect; a few systems have a user parameter called Scale of their own.
			if (Name == TEXT("@Scale"))
			{
				Override.Value->Scale = FCString::Atof(*Number);
				continue;
			}
			if (Components.Num() == 4)
			{
				Override.Value->ColorParameters.Add(FName(*Name), FLinearColor(
					FCString::Atof(*Components[0]), FCString::Atof(*Components[1]), FCString::Atof(*Components[2]),
					FCString::Atof(*Components[3])));
				continue;
			}
			if (Components.Num() == 3)
			{
				Override.Value->VectorParameters.Add(FName(*Name), FVector(FCString::Atof(*Components[0]),
				                                                           FCString::Atof(*Components[1]),
				                                                           FCString::Atof(*Components[2])));
				continue;
			}
			Override.Value->FloatParameters.Add(FName(*Name), FCString::Atof(*Number));
		}
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("AircraftDeathDumpVfxParams")))
	{
		for (const UNiagaraSystem* System : {Settings.EngineFire.System.Get(), Settings.SmokeTrail.System.Get(),
		                                     Settings.ImpactExplosion.System.Get(), Settings.GroundFire.System.Get()})
		{
			LogNiagaraUserParameters(System);
		}
	}
}

void UAircraftTestSubsystem::LogNiagaraUserParameters(const UNiagaraSystem* System) const
{
	if (not IsValid(System))
	{
		return;
	}
	const FNiagaraUserRedirectionParameterStore& Store = System->GetExposedParameters();
	for (const FNiagaraVariableWithOffset& Variable : Store.ReadParameterVariables())
	{
		const uint8* Data = Store.GetParameterData(Variable.Offset);
		FString ValueText;
		const int32 Size = Variable.GetSizeInBytes();
		const int32 FloatCount = Size / static_cast<int32>(sizeof(float));
		if (Data != nullptr && Size % sizeof(float) == 0 && FloatCount <= 4)
		{
			for (int32 FloatIndex = 0; FloatIndex < FloatCount; ++FloatIndex)
			{
				float Component = 0.f;
				FMemory::Memcpy(&Component, Data + FloatIndex * sizeof(float), sizeof(float));
				ValueText += FString::Printf(TEXT("%s%g"), FloatIndex > 0 ? TEXT(",") : TEXT(""), Component);
			}
		}
		UE_LOG(LogRTSAircraftTest, Display, TEXT("RTS_AIRCRAFT_TEST VFX_PARAM %s %s (%s) = %s"), *System->GetName(),
		       *Variable.GetName().ToString(), *Variable.GetType().GetName(), *ValueText);
	}
}

void UAircraftTestSubsystem::TickAircraftDeath()
{
	if (not M_DeathAircraft.IsValidIndex(M_DeathAircraftIndex))
	{
		Finish();
		return;
	}
	FAircraftDeathUnderTest& Entry = M_DeathAircraft[M_DeathAircraftIndex];
	switch (Entry.Step)
	{
	case EAircraftDeathTestStep::WaitingForAirbase:
		TickDeathWaitingForAirbase(Entry);
		return;
	case EAircraftDeathTestStep::Flying:
		TickDeathFlying(Entry);
		return;
	case EAircraftDeathTestStep::Falling:
		TickDeathFalling(Entry);
		return;
	case EAircraftDeathTestStep::WaitingForCleanup:
		TickDeathWaitingForCleanup(Entry);
		return;
	case EAircraftDeathTestStep::Done:
		++M_DeathAircraftIndex;
		if (not M_DeathAircraft.IsValidIndex(M_DeathAircraftIndex)
			|| not SpawnDeathAircraft(M_DeathAircraft[M_DeathAircraftIndex]))
		{
			Finish();
		}
		return;
	}
}

void UAircraftTestSubsystem::TickDeathFlying(FAircraftDeathUnderTest& Entry)
{
	using namespace AircraftDeathTestPrivate;
	if (not Entry.Aircraft.IsValid())
	{
		Check(false, Entry.Label + TEXT("_alive_until_shot"));
		EnterDeathStep(Entry, EAircraftDeathTestStep::Done);
		return;
	}
	if (GetNow() - Entry.StepStartSeconds < FlyBeforeKillSeconds)
	{
		return;
	}
	KillDeathAircraft(Entry);
}

void UAircraftTestSubsystem::KillDeathAircraft(FAircraftDeathUnderTest& Entry)
{
	AAircraftMaster* Aircraft = Entry.Aircraft.Get();
	Entry.LocationAtDeath = Aircraft->GetActorLocation();
	Entry.HorizontalSpeedAtDeath = Aircraft->GetVelocity().Size2D();
	Entry.VelocityAtDeath = Aircraft->GetVelocity();
	Entry.KilledSeconds = GetNow();
	// Some Blueprints start landed on the ground; those must break up where they stand. A landed aircraft waiting in
	// the air for its drop order still falls.
	Entry.bSpawnLanded |= Aircraft->GetLandedState() == EAircraftLandingState::Landed
		&& Entry.LocationAtDeath.Z - Entry.GroundZ < AircraftDeathTestPrivate::ParkedHeight;
	UE_LOG(LogRTSAircraftTest, Display, TEXT("RTS_AIRCRAFT_TEST KILL %s at=%s speed=%.0f landed_state=%d"),
	       *Entry.Label, *Entry.LocationAtDeath.ToCompactString(), Entry.HorizontalSpeedAtDeath,
	       static_cast<int32>(Aircraft->GetLandedState()));
	Aircraft->TakeFatalDamage();
	CheckRightAfterKill(Entry);
	EnterDeathStep(Entry, EAircraftDeathTestStep::Falling);
}

void UAircraftTestSubsystem::CheckRightAfterKill(FAircraftDeathUnderTest& Entry)
{
	AAircraftMaster* Aircraft = Entry.Aircraft.Get();
	Check(IsValid(Aircraft), Entry.Label + TEXT("_not_destroyed_at_once"));
	if (not IsValid(Aircraft))
	{
		return;
	}
	Check(not Aircraft->IsUnitAlive(), Entry.Label + TEXT("_counts_as_dead"));
	const UGameUnitManager* UnitManager = FRTS_Statics::GetGameUnitManager(GetWorld());
	Check(IsValid(UnitManager) && not UnitManager->ShippingTest_GetAircraftOfPlayer(Entry.OwningPlayer).Contains(Aircraft),
	      Entry.Label + TEXT("_left_the_target_lists"));
	const UAircraftDeathComponent* Death = Aircraft->GetDeathComponent();
	Check(IsValid(Death), Entry.Label + TEXT("_has_death_component"));
	if (not IsValid(Death))
	{
		return;
	}
	Check(Death->GetHasCrashCollection(), Entry.Label + TEXT("_has_crash_collection"));
	if (Entry.bSpawnLanded)
	{
		Check(Death->GetDeathPhase() == EAircraftDeathPhase::Impacted, Entry.Label + TEXT("_parked_breaks_up_on_the_spot"));
		return;
	}
	Check(Death->GetDeathPhase() == EAircraftDeathPhase::Crashing, Entry.Label + TEXT("_starts_falling"));
	const float SpeedAtDeath = Entry.VelocityAtDeath.Size();
	const float ExpectedSpeed = FMath::Max(SpeedAtDeath, Death->GetDeathSettings().MinCrashSpeed);
	Check(FMath::IsNearlyEqual(Death->GetInitialCrashSpeed(), ExpectedSpeed, ExpectedSpeed * 0.05f),
	      Entry.Label + TEXT("_falls_with_the_speed_it_had"),
	      FString::Printf(TEXT("crash_speed=%.0f speed_at_death=%.0f"), Death->GetInitialCrashSpeed(), SpeedAtDeath));
	Check(Death->GetIsFireBurning(), Entry.Label + TEXT("_engine_burns"),
	      FString::Printf(TEXT("attach=%s"), *Death->GetFireAttachName().ToString()));
	if (Entry.bExpectWingFire)
	{
		Check(Death->GetFireOrigin() == EAircraftFireOrigin::WingEngine
		      && Death->GetDamagedSide() != EAircraftDamagedSide::None,
		      Entry.Label + TEXT("_burns_on_a_wing"),
		      FString::Printf(TEXT("attach=%s side=%d"), *Death->GetFireAttachName().ToString(),
		                      static_cast<int32>(Death->GetDamagedSide())));
		return;
	}
	Check(Death->GetFireOrigin() == EAircraftFireOrigin::NoseEngine
	      && Death->GetFireAttachName() == AircraftDeathTestPrivate::NoseEngineBone,
	      Entry.Label + TEXT("_burns_at_the_nose_engine"),
	      FString::Printf(TEXT("attach=%s"), *Death->GetFireAttachName().ToString()));
}

void UAircraftTestSubsystem::TickDeathFalling(FAircraftDeathUnderTest& Entry)
{
	using namespace AircraftDeathTestPrivate;
	const AAircraftMaster* Aircraft = Entry.Aircraft.Get();
	const UAircraftDeathComponent* Death = IsValid(Aircraft) ? Aircraft->GetDeathComponent() : nullptr;
	if (not IsValid(Death))
	{
		Check(false, Entry.Label + TEXT("_exists_until_impact"));
		EnterDeathStep(Entry, EAircraftDeathTestStep::Done);
		return;
	}
	const float SinceKill = GetNow() - Entry.KilledSeconds;
	if (CaptureSecondsAfterKill.IsValidIndex(Entry.NextCaptureIndex)
		&& SinceKill >= CaptureSecondsAfterKill[Entry.NextCaptureIndex])
	{
		CaptureDeathMoment(Entry, Aircraft->GetActorLocation());
	}
	if (Death->GetDeathPhase() == EAircraftDeathPhase::Impacted)
	{
		Entry.ImpactSeconds = GetNow();
		Entry.ImpactLocation = Death->GetImpactLocation();
		Entry.Wreck = Death->GetWreck();
		Entry.NextCaptureIndex = CaptureSecondsAfterKill.Num();
		CheckImpact(Entry);
		EnterDeathStep(Entry, EAircraftDeathTestStep::WaitingForCleanup);
		return;
	}
	if (SinceKill > FallTimeoutSeconds)
	{
		Check(false, Entry.Label + TEXT("_hit_the_ground"),
		      FString::Printf(TEXT("still at %s"), *Aircraft->GetActorLocation().ToCompactString()));
		EnterDeathStep(Entry, EAircraftDeathTestStep::Done);
	}
}

void UAircraftTestSubsystem::CheckImpact(FAircraftDeathUnderTest& Entry)
{
	using namespace AircraftDeathTestPrivate;
	const AAircraftMaster* Aircraft = Entry.Aircraft.Get();
	const UAircraftDeathComponent* Death = Aircraft->GetDeathComponent();
	const float FallSeconds = Entry.ImpactSeconds - Entry.KilledSeconds;
	const float Travel = FVector::Dist2D(Entry.LocationAtDeath, Entry.ImpactLocation);
	UE_LOG(LogRTSAircraftTest, Display,
	       TEXT("RTS_AIRCRAFT_TEST IMPACT %s fall=%.2fs travel=%.0f impact=%s ground_z=%.0f pieces=%d wreck=%d fire=%s side=%d size_scale=%.2f puffs=%d"),
	       *Entry.Label, FallSeconds, Travel, *Entry.ImpactLocation.ToCompactString(), Entry.GroundZ,
	       Death->GetBreakupPieceCount(), IsValid(Death->GetWreck()), *Death->GetFireAttachName().ToString(),
	       static_cast<int32>(Death->GetDamagedSide()), Death->GetSizeEffectScaleOfAircraft(), Death->GetSmokePuffCount());
	Check(FMath::Abs(Entry.ImpactLocation.Z - Entry.GroundZ) < ImpactHeightTolerance,
	      Entry.Label + TEXT("_impact_on_the_ground"),
	      FString::Printf(TEXT("impact_z=%.0f ground_z=%.0f"), Entry.ImpactLocation.Z, Entry.GroundZ));
	Check(IsValid(Death->GetWreck()), Entry.Label + TEXT("_left_a_wreck"));
	Check(Death->GetBreakupPieceCount() >= MinimumPieces, Entry.Label + TEXT("_broke_into_pieces"),
	      FString::Printf(TEXT("pieces=%d"), Death->GetBreakupPieceCount()));
	Check(not Death->GetIsFireBurning(), Entry.Label + TEXT("_engine_fire_ends_at_impact"));
	if (Entry.bSpawnLanded)
	{
		return;
	}
	Check(FallSeconds > 0.5f, Entry.Label + TEXT("_fell_for_a_while"), FString::Printf(TEXT("fall=%.2fs"), FallSeconds));
	Check(Travel >= Entry.HorizontalSpeedAtDeath * MinimumTravelShareOfOneSecond,
	      Entry.Label + TEXT("_carried_on_with_its_speed"),
	      FString::Printf(TEXT("travel=%.0f speed=%.0f"), Travel, Entry.HorizontalSpeedAtDeath));
}

void UAircraftTestSubsystem::TickDeathWaitingForCleanup(FAircraftDeathUnderTest& Entry)
{
	using namespace AircraftDeathTestPrivate;
	const float SinceImpact = GetNow() - Entry.ImpactSeconds;
	const int32 CaptureIndex = Entry.NextCaptureIndex - CaptureSecondsAfterKill.Num();
	if (CaptureSecondsAfterImpact.IsValidIndex(CaptureIndex) && SinceImpact >= CaptureSecondsAfterImpact[CaptureIndex])
	{
		CaptureDeathMoment(Entry, Entry.ImpactLocation);
		LogWreck(Entry);
	}
	if (Entry.bSpawnLanded && not Entry.bCheckedParkedSpread && SinceImpact >= ParkedSpreadCheckSeconds)
	{
		CheckParkedPieces(Entry);
	}
	if (not Entry.Aircraft.IsValid() && Entry.ActorClearedSeconds < 0.f)
	{
		Entry.ActorClearedSeconds = GetNow();
		Check(true, Entry.Label + TEXT("_aircraft_actor_cleared"), FString::Printf(TEXT("after=%.1fs"), SinceImpact));
	}
	const bool bCapturesDone = not CaptureSecondsAfterImpact.IsValidIndex(CaptureIndex);
	if (Entry.ActorClearedSeconds >= 0.f && bCapturesDone)
	{
		EnterDeathStep(Entry, EAircraftDeathTestStep::Done);
		return;
	}
	if (SinceImpact > CleanupTimeoutSeconds)
	{
		Check(Entry.ActorClearedSeconds >= 0.f, Entry.Label + TEXT("_aircraft_actor_cleared"));
		EnterDeathStep(Entry, EAircraftDeathTestStep::Done);
	}
}

void UAircraftTestSubsystem::CheckParkedPieces(FAircraftDeathUnderTest& Entry)
{
	using namespace AircraftDeathTestPrivate;
	Entry.bCheckedParkedSpread = true;
	const TArray<FVector> PieceLocations = GetWreckPieceLocations(Entry);
	float Spread = PieceLocations.IsEmpty() ? -1.f : 0.f;
	for (const FVector& PieceLocation : PieceLocations)
	{
		Spread = FMath::Max(Spread, static_cast<float>(FVector::Dist2D(PieceLocation, Entry.LocationAtDeath)));
	}
	// Pieces of a plane killed in its bay must stay about where it stood, not fly through the walls.
	Check(Spread >= 0.f && Spread < ParkedMaxPieceSpread, Entry.Label + TEXT("_parked_pieces_stay_in_place"),
	      FString::Printf(TEXT("spread=%.0f"), Spread));
	if (Entry.bParkInAirbase)
	{
		CheckPiecesStayInTheBay(Entry, PieceLocations);
	}
}

void UAircraftTestSubsystem::CheckPiecesStayInTheBay(const FAircraftDeathUnderTest& Entry,
                                                     const TArray<FVector>& PieceLocations)
{
	using namespace AircraftDeathTestPrivate;
	const ANomadicVehicle* Airbase = Entry.Airbase.Get();
	const UStaticMeshComponent* Building = IsValid(Airbase) ? Airbase->MapTest_GetBuildingMeshComponent() : nullptr;
	Check(IsValid(Building) && not PieceLocations.IsEmpty(), Entry.Label + TEXT("_bay_pieces_found"));
	if (not IsValid(Building) || PieceLocations.IsEmpty())
	{
		return;
	}
	const FBox BuildingBox = Building->Bounds.GetBox();
	int32 PiecesOutsideTheBuilding = 0;
	int32 PiecesUnderTheFloor = 0;
	for (const FVector& PieceLocation : PieceLocations)
	{
		const bool bInsideFootprint = PieceLocation.X >= BuildingBox.Min.X && PieceLocation.X <= BuildingBox.Max.X
			&& PieceLocation.Y >= BuildingBox.Min.Y && PieceLocation.Y <= BuildingBox.Max.Y;
		PiecesOutsideTheBuilding += bInsideFootprint ? 0 : 1;
		// The bay floor is the ground under the building; where the ground dips, a piece lies lower and is fine.
		FVector GroundUnderPiece = Entry.BayLocation;
		(void)GetGroundAt(FVector2D(PieceLocation), GroundUnderPiece);
		if (PieceLocation.Z < GroundUnderPiece.Z - BayFloorTolerance)
		{
			++PiecesUnderTheFloor;
			UE_LOG(LogRTSAircraftTest, Display, TEXT("RTS_AIRCRAFT_TEST SUNK_PIECE %s at=%s ground_z=%.0f"),
			       *Entry.Label, *PieceLocation.ToCompactString(), GroundUnderPiece.Z);
		}
	}
	const FString Details = FString::Printf(TEXT("pieces=%d outside=%d under_floor=%d building=%s bay=%s"),
	                                        PieceLocations.Num(), PiecesOutsideTheBuilding, PiecesUnderTheFloor,
	                                        *BuildingBox.ToString(), *Entry.BayLocation.ToCompactString());
	Check(PiecesOutsideTheBuilding == 0, Entry.Label + TEXT("_pieces_stay_inside_the_airbase"), Details);
	Check(PiecesUnderTheFloor == 0, Entry.Label + TEXT("_pieces_stay_on_the_bay_floor"), Details);
}

TArray<FVector> UAircraftTestSubsystem::GetWreckPieceLocations(const FAircraftDeathUnderTest& Entry) const
{
	TArray<FVector> PieceLocations;
	const AAircraftWreckActor* Wreck = Cast<AAircraftWreckActor>(Entry.Wreck.Get());
	UGeometryCollectionComponent* Pieces = IsValid(Wreck) ? Wreck->GetGeometryCollectionComponent() : nullptr;
	if (not IsValid(Pieces) || not IsValid(Pieces->GetRestCollection()))
	{
		return PieceLocations;
	}
	const TArray<FTransform3f>& PieceTransforms = Pieces->GetComponentSpaceTransforms3f();
	const FGeometryCollection* Collection = Pieces->GetRestCollection()->GetGeometryCollection().Get();
	const int32 TransformCount = Collection != nullptr
		                             ? FMath::Min(PieceTransforms.Num(), Collection->Children.Num())
		                             : 0;
	for (int32 Index = 0; Index < TransformCount; ++Index)
	{
		// Leaves with geometry are the pieces; clusters and empty transforms are not drawn.
		const int32 GeometryIndex = Collection->TransformToGeometryIndex[Index];
		if (not Collection->Children[Index].IsEmpty() || not Collection->BoundingBox.IsValidIndex(GeometryIndex))
		{
			continue;
		}
		// The crash collections are baked into collection space: every piece's transform origin is the aircraft's
		// centre, so the piece itself is where the centre of its geometry is.
		const FVector LocalCentre = FVector(Collection->BoundingBox[GeometryIndex].GetCenter());
		const FVector ComponentCentre = FVector(PieceTransforms[Index].TransformPosition(FVector3f(LocalCentre)));
		PieceLocations.Add(Pieces->GetComponentTransform().TransformPosition(ComponentCentre));
	}
	return PieceLocations;
}

void UAircraftTestSubsystem::LogWreck(const FAircraftDeathUnderTest& Entry) const
{
	const AAircraftWreckActor* Wreck = Cast<AAircraftWreckActor>(Entry.Wreck.Get());
	const UGeometryCollectionComponent* Pieces = IsValid(Wreck) ? Wreck->GetGeometryCollectionComponent() : nullptr;
	if (not IsValid(Pieces))
	{
		UE_LOG(LogRTSAircraftTest, Display, TEXT("RTS_AIRCRAFT_TEST WRECK %s no pieces component"), *Entry.Label);
		return;
	}
	// Where the pieces with geometry are: lowest, highest and furthest from the impact.
	float LowestZ = TNumericLimits<float>::Max();
	float HighestZ = TNumericLimits<float>::Lowest();
	float FurthestFromImpact = 0.f;
	const TArray<FVector> PieceLocations = GetWreckPieceLocations(Entry);
	for (const FVector& PieceLocation : PieceLocations)
	{
		LowestZ = FMath::Min(LowestZ, static_cast<float>(PieceLocation.Z));
		HighestZ = FMath::Max(HighestZ, static_cast<float>(PieceLocation.Z));
		FurthestFromImpact = FMath::Max(FurthestFromImpact,
		                                static_cast<float>(FVector::Dist(PieceLocation, Entry.ImpactLocation)));
	}
	UE_LOG(LogRTSAircraftTest, Display, TEXT("RTS_AIRCRAFT_TEST WRECK_PIECES %s seen=%d lowest_z=%.0f highest_z=%.0f furthest=%.0f"),
	       *Entry.Label, PieceLocations.Num(), LowestZ, HighestZ, FurthestFromImpact);
	UE_LOG(LogRTSAircraftTest, Display,
	       TEXT("RTS_AIRCRAFT_TEST WRECK %s registered=%d visible=%d render_state=%d physics_state=%d bounds=%s extent=%s wreck_at=%s"),
	       *Entry.Label, Pieces->IsRegistered(), Pieces->IsVisible(), Pieces->IsRenderStateCreated(),
	       Pieces->IsPhysicsStateCreated(), *Pieces->Bounds.Origin.ToCompactString(),
	       *Pieces->Bounds.BoxExtent.ToCompactString(), *Wreck->GetActorLocation().ToCompactString());
}

void UAircraftTestSubsystem::CaptureDeathMoment(FAircraftDeathUnderTest& Entry, const FVector& Focus)
{
	const int32 MomentIndex = Entry.NextCaptureIndex++;
	if (not M_CaptureRig.GetIsEnabled() || not IsValid(GetWorld()))
	{
		return;
	}
	// The player's view (high, looking down) and a low side view of the same moment.
	const TArray<FB4HowitzerCameraView> Views = {
		{TEXT("Game"), FVector(-2000.f, -1200.f, 2400.f), FVector::ZeroVector, 60.f},
		{TEXT("Side"), FVector(0.f, -2400.f, 300.f), FVector::ZeroVector, 60.f},
	};
	M_CaptureRig.CaptureViews(*GetWorld(), FTransform(Focus), Views,
	                          FString::Printf(TEXT("%s_%d"), *Entry.Label, MomentIndex));
}

#else

bool UAircraftTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UAircraftTestSubsystem::Tick(const float DeltaTime)
{
}

TStatId UAircraftTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAircraftTestSubsystem, STATGROUP_Tickables);
}

#endif
