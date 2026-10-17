// Copyright (C) 2020-2026 Bas Blokzijl - All rights reserved.

#include "EmbInfantryTurretTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS
#include "Components/ChildActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/World.h"
#include "Kismet/KismetMathLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/Turret/EmbeddedInfantry/EmbeddedInfantryTurret.h"
#include "RTS_Survival/Weapons/Turret/EmbeddedInfantry/EmbeddedInfantryTurretAnimInstance.h"
#include "RTS_Survival/Weapons/Turret/EmbeddedInfantry/EmbeddedTurretMountComponent.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSEmbInfantryTurretTest, Log, All);

namespace EmbInfantryTurretTestPrivate
{
	constexpr float StartDelaySeconds = 2.f;
	constexpr float MountTimeoutSeconds = 5.f;
	constexpr float EngageTimeoutSeconds = 60.f;
	// Watch the gunners a little after their first shot: the aim pitch eases in over several frames.
	constexpr float MinimumEngageSeconds = 3.f;
	constexpr float GroundTargetDistance = 1500.f;
	constexpr float AttackGroundTimeoutSeconds = 30.f;
	// Far beyond any player unit's vision on the test map.
	constexpr float FogHostDistance = 15000.f;
	// Enemy units join the fog of war a few seconds after they spawn; then the next visibility update hides them.
	constexpr float FogSettleSeconds = 4.f;
	constexpr float FogTimeoutSeconds = 15.f;
	constexpr float StatusIntervalSeconds = 5.f;
	constexpr float GroundTraceHalfHeight = 100000.f;
	constexpr float UnitLift = 100.f;
	constexpr float TankSpacing = 900.f;
	// Fraction of the shortest infantry turret range the enemy stands at.
	constexpr float EnemyRangeFraction = 0.6f;
	constexpr float MinimumEnemyDistance = 1200.f;
	constexpr float MountLocationTolerance = 2.f;
	constexpr float AimedYawTolerance = 15.f;
	const FName MountSocketName = TEXT("Infantry");
	const FVector MountLocalOffset = FVector(0.f, 0.f, -80.f);
	const FVector2D DefaultAnchor = FVector2D(16796.f, -15808.f);

	const TCHAR* const EnemyClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Medium/T34/Rus_Vehicle_T3476.Rus_Vehicle_T3476_C");

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

	UEmbeddedInfantryTurretAnimInstance* GetInfantryAnimInstance(const AEmbeddedInfantryTurret* Turret)
	{
		const USkeletalMeshComponent* SoldierMesh = IsValid(Turret)
			? Turret->FindComponentByClass<USkeletalMeshComponent>()
			: nullptr;
		return IsValid(SoldierMesh) ? Cast<UEmbeddedInfantryTurretAnimInstance>(SoldierMesh->GetAnimInstance()) : nullptr;
	}
}

bool UEmbInfantryTurretTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), TEXT("EmbInfantryTurretValidate"));
}

TStatId UEmbInfantryTurretTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UEmbInfantryTurretTestSubsystem, STATGROUP_Tickables);
}

void UEmbInfantryTurretTestSubsystem::Tick(const float DeltaTime)
{
	const UWorld* World = GetWorld();
	if (not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	switch (M_Phase)
	{
	case EEmbInfantryTurretTestPhase::WaitingForStart:
		TickWaitingForStart();
		return;
	case EEmbInfantryTurretTestPhase::WaitingForMounts:
		TickWaitingForMounts();
		return;
	case EEmbInfantryTurretTestPhase::Engaging:
		TickEngaging();
		return;
	case EEmbInfantryTurretTestPhase::AttackingGround:
		TickAttackingGround();
		return;
	case EEmbInfantryTurretTestPhase::CheckingFogOfWar:
		TickCheckingFogOfWar();
		return;
	case EEmbInfantryTurretTestPhase::Finished:
		return;
	}
}

void UEmbInfantryTurretTestSubsystem::Check(const bool bCondition, const FString& Description, const FString& Details)
{
	if (bCondition)
	{
		++M_PassedCheckCount;
		UE_LOG(LogRTSEmbInfantryTurretTest, Display, TEXT("RTS_EIT_TEST CHECK PASS %s %s"), *Description, *Details);
		return;
	}
	++M_FailedCheckCount;
	UE_LOG(LogRTSEmbInfantryTurretTest, Error, TEXT("RTS_EIT_TEST CHECK FAIL %s %s"), *Description, *Details);
}

float UEmbInfantryTurretTestSubsystem::GetNow() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.f;
}

void UEmbInfantryTurretTestSubsystem::EnterPhase(const EEmbInfantryTurretTestPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartSeconds = GetNow();
	M_NextStatusSeconds = M_PhaseStartSeconds;
}

void UEmbInfantryTurretTestSubsystem::Finish()
{
	const bool bPassed = M_FailedCheckCount == 0 && M_PassedCheckCount > 0;
	UE_LOG(LogRTSEmbInfantryTurretTest, Display, TEXT("RTS_EIT_TEST RESULT %s passed=%d failed=%d"),
	       bPassed ? TEXT("PASS") : TEXT("FAIL"), M_PassedCheckCount, M_FailedCheckCount);
	M_Phase = EEmbInfantryTurretTestPhase::Finished;
	if (APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld()))
	{
		Controller->ConsoleCommand(TEXT("quit"));
	}
}

void UEmbInfantryTurretTestSubsystem::TickWaitingForStart()
{
	using namespace EmbInfantryTurretTestPrivate;
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

	FVector2D Anchor2D = DefaultAnchor;
	FString AnchorText;
	if (FParse::Value(FCommandLine::Get(), TEXT("EITAt="), AnchorText))
	{
		FString XText;
		FString YText;
		if (AnchorText.Split(TEXT(","), &XText, &YText))
		{
			Anchor2D = FVector2D(FCString::Atof(*XText), FCString::Atof(*YText));
		}
	}
	if (not GetGroundAt(Anchor2D, M_Anchor))
	{
		Check(false, TEXT("found_ground_anchor"), Anchor2D.ToString());
		Finish();
		return;
	}
	FString CaptureDirectory;
	if (FParse::Value(FCommandLine::Get(), TEXT("EITCapture="), CaptureDirectory))
	{
		M_CaptureRig.Init(*GetWorld(), CaptureDirectory);
	}

	const FString Root = TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/");
	M_Units.SetNum(2);
	M_Units[0].Label = TEXT("T34E_Soviet");
	M_Units[0].TankClassPath = Root + TEXT("Rus_Medium/T34/Rus_Vehicle_T34E_Soviet.Rus_Vehicle_T34E_Soviet_C");
	M_Units[0].ExpectedTurretClassName = TEXT("Rus_EIT_BasicSoviet_PTRS_C");
	M_Units[1].Label = TEXT("KV1_L11_Soviet");
	M_Units[1].TankClassPath = Root + TEXT("Rus_Heavy/KV1/Rus_Vehicle_KV-1_L11_Soviet.Rus_Vehicle_KV-1_L11_Soviet_C");
	M_Units[1].ExpectedTurretClassName = TEXT("Rus_EIT_Hazmat_Bazooka_C");

	for (int32 UnitIndex = 0; UnitIndex < M_Units.Num(); ++UnitIndex)
	{
		FEmbInfantryTurretUnderTest& Unit = M_Units[UnitIndex];
		FVector Ground;
		const FVector2D Spot = Anchor2D + FVector2D(0.f, UnitIndex * TankSpacing);
		if (not GetGroundAt(Spot, Ground))
		{
			Ground = M_Anchor;
		}
		Unit.Tank = SpawnUnitForPlayer(Unit.TankClassPath, FTransform(Ground + FVector(0.f, 0.f, UnitLift)), 1);
		Check(Unit.Tank.IsValid(), TEXT("spawned_host"), Unit.Label);
	}
	UE_LOG(LogRTSEmbInfantryTurretTest, Display, TEXT("RTS_EIT_TEST START anchor=%s"), *M_Anchor.ToCompactString());
	EnterPhase(EEmbInfantryTurretTestPhase::WaitingForMounts);
}

void UEmbInfantryTurretTestSubsystem::TickWaitingForMounts()
{
	using namespace EmbInfantryTurretTestPrivate;
	KeepUnitsAlive();
	bool bAllMounted = true;
	for (FEmbInfantryTurretUnderTest& Unit : M_Units)
	{
		const UEmbeddedTurretMountComponent* Mount = Unit.Tank.IsValid()
			? Unit.Tank->FindComponentByClass<UEmbeddedTurretMountComponent>()
			: nullptr;
		Unit.Turret = IsValid(Mount) ? Cast<AEmbeddedInfantryTurret>(Mount->GetMountedTurret()) : nullptr;
		bAllMounted &= Unit.Turret.IsValid();
	}
	// Let the soldiers settle into their pose before the mount is measured.
	constexpr float SettleSeconds = 1.f;
	const bool bTimedOut = GetNow() - M_PhaseStartSeconds > MountTimeoutSeconds;
	if ((not bAllMounted || GetNow() - M_PhaseStartSeconds < SettleSeconds) && not bTimedOut)
	{
		return;
	}
	for (FEmbInfantryTurretUnderTest& Unit : M_Units)
	{
		CheckMountedTurret(Unit);
	}
	CaptureUnits(TEXT("mounted"));
	SpawnEnemyInRange();
	EnterPhase(EEmbInfantryTurretTestPhase::Engaging);
}

void UEmbInfantryTurretTestSubsystem::TickEngaging()
{
	using namespace EmbInfantryTurretTestPrivate;
	KeepUnitsAlive();
	bool bAllEngaged = true;
	for (FEmbInfantryTurretUnderTest& Unit : M_Units)
	{
		CountShotsAndAim(Unit);
		bAllEngaged &= Unit.Shots > 0 && Unit.bSawFireMontage;
	}
	const float Elapsed = GetNow() - M_PhaseStartSeconds;
	if (GetNow() >= M_NextStatusSeconds)
	{
		M_NextStatusSeconds = GetNow() + StatusIntervalSeconds;
		for (const FEmbInfantryTurretUnderTest& Unit : M_Units)
		{
			UE_LOG(LogRTSEmbInfantryTurretTest, Display,
			       TEXT("RTS_EIT_TEST STATUS %s t=%.1f shots=%d targeted=%d montage=%d max_pitch=%.1f target_pitch=%.1f yaw_error=%.1f"),
			       *Unit.Label, Elapsed, Unit.Shots, Unit.bHasTargetedEnemy, Unit.bSawFireMontage,
			       Unit.MaxAbsAimPitch, Unit.MaxAbsTargetPitch, Unit.SmallestYawErrorToEnemy);
		}
	}
	if ((not bAllEngaged || Elapsed < MinimumEngageSeconds) && Elapsed < EngageTimeoutSeconds)
	{
		return;
	}
	CaptureUnits(TEXT("engaging"));
	for (const FEmbInfantryTurretUnderTest& Unit : M_Units)
	{
		const FString Details = FString::Printf(TEXT("%s shots=%d max_pitch=%.1f target_pitch=%.1f yaw_error=%.1f"),
		                                        *Unit.Label, Unit.Shots, Unit.MaxAbsAimPitch, Unit.MaxAbsTargetPitch,
		                                        Unit.SmallestYawErrorToEnemy);
		Check(Unit.bHasTargetedEnemy, TEXT("targeted_enemy"), Details);
		Check(Unit.Shots > 0, TEXT("fired_at_enemy"), Details);
		Check(Unit.bSawFireMontage, TEXT("played_fire_montage"), Details);
		Check(Unit.SmallestYawErrorToEnemy <= AimedYawTolerance, TEXT("turned_to_enemy"), Details);
		// The gunner stands well above the enemy tank: he must aim down at it, and his pose must follow.
		Check(Unit.MaxAbsTargetPitch > 0.f && Unit.MaxAbsAimPitch > 0.f, TEXT("aimed_pitch_at_enemy"), Details);
	}
	StartAttackGround();
}

void UEmbInfantryTurretTestSubsystem::StartAttackGround()
{
	using namespace EmbInfantryTurretTestPrivate;
	// Behind the hosts, opposite the enemy: a gunner that missed the order keeps shooting the enemy instead.
	const FVector2D Spot(M_Anchor.X - GroundTargetDistance, M_Anchor.Y + TankSpacing * 0.5f);
	if (not GetGroundAt(Spot, M_GroundTarget))
	{
		M_GroundTarget = M_Anchor - FVector(GroundTargetDistance, 0.f, 0.f);
	}
	for (FEmbInfantryTurretUnderTest& Unit : M_Units)
	{
		Unit.ShotsBeforeGroundOrder = Unit.Shots;
		ICommands* Commands = Cast<ICommands>(Unit.Tank.Get());
		const ECommandQueueError Error = Commands != nullptr
			? Commands->AttackGround(M_GroundTarget, true)
			: ECommandQueueError::QueueFull;
		Check(Error == ECommandQueueError::NoError, TEXT("attack_ground_ordered"), Unit.Label);
	}
	UE_LOG(LogRTSEmbInfantryTurretTest, Display, TEXT("RTS_EIT_TEST START attack_ground at=%s"),
	       *M_GroundTarget.ToCompactString());
	EnterPhase(EEmbInfantryTurretTestPhase::AttackingGround);
}

void UEmbInfantryTurretTestSubsystem::TickAttackingGround()
{
	using namespace EmbInfantryTurretTestPrivate;
	KeepUnitsAlive();
	bool bAllFiredAtGround = true;
	for (FEmbInfantryTurretUnderTest& Unit : M_Units)
	{
		CountShotsAndAim(Unit);
		if (Unit.Turret.IsValid())
		{
			const float YawToGround = UKismetMathLibrary::FindLookAtRotation(
				Unit.Turret->GetActorLocation(), M_GroundTarget).Yaw;
			const float YawError = FMath::Abs(FMath::FindDeltaAngleDegrees(
				Unit.Turret->GetActorRotation().Yaw, YawToGround));
			Unit.SmallestYawErrorToGround = FMath::Min(Unit.SmallestYawErrorToGround, YawError);
		}
		bAllFiredAtGround &= Unit.Shots > Unit.ShotsBeforeGroundOrder && Unit.SmallestYawErrorToGround <= AimedYawTolerance;
	}
	const float Elapsed = GetNow() - M_PhaseStartSeconds;
	if (not bAllFiredAtGround && Elapsed < AttackGroundTimeoutSeconds)
	{
		return;
	}
	CaptureUnits(TEXT("attack_ground"));
	for (const FEmbInfantryTurretUnderTest& Unit : M_Units)
	{
		const FString Details = FString::Printf(TEXT("%s shots_after_order=%d yaw_error=%.1f t=%.1f"), *Unit.Label,
		                                        Unit.Shots - Unit.ShotsBeforeGroundOrder,
		                                        Unit.SmallestYawErrorToGround, Elapsed);
		Check(Unit.SmallestYawErrorToGround <= AimedYawTolerance, TEXT("turned_to_ground_target"), Details);
		Check(Unit.Shots > Unit.ShotsBeforeGroundOrder, TEXT("fired_at_ground_target"), Details);
	}
	StartFogOfWarCheck();
}

void UEmbInfantryTurretTestSubsystem::StartFogOfWarCheck()
{
	using namespace EmbInfantryTurretTestPrivate;
	const TArray<FVector2D> Offsets = {
		FVector2D(0.f, FogHostDistance), FVector2D(0.f, -FogHostDistance),
		FVector2D(FogHostDistance, 0.f), FVector2D(-FogHostDistance, 0.f)
	};
	FVector Ground;
	bool bFoundGround = false;
	for (const FVector2D& Offset : Offsets)
	{
		bFoundGround = GetGroundAt(FVector2D(M_Anchor) + Offset, Ground);
		if (bFoundGround)
		{
			break;
		}
	}
	if (bFoundGround)
	{
		M_HiddenEnemyHost = SpawnUnitForPlayer(M_Units[1].TankClassPath,
		                                       FTransform(Ground + FVector(0.f, 0.f, UnitLift)), 2);
	}
	Check(M_HiddenEnemyHost.IsValid(), TEXT("spawned_far_enemy_host"),
	      bFoundGround ? Ground.ToCompactString() : FString(TEXT("no ground")));
	EnterPhase(EEmbInfantryTurretTestPhase::CheckingFogOfWar);
}

void UEmbInfantryTurretTestSubsystem::TickCheckingFogOfWar()
{
	using namespace EmbInfantryTurretTestPrivate;
	KeepUnitsAlive();
	AActor* Host = M_HiddenEnemyHost.Get();
	const float Elapsed = GetNow() - M_PhaseStartSeconds;
	const bool bHostHidden = IsValid(Host) && Host->IsHidden();
	if (Elapsed < FogSettleSeconds || (not bHostHidden && Elapsed < FogTimeoutSeconds))
	{
		return;
	}
	const UEmbeddedTurretMountComponent* Mount = IsValid(Host)
		? Host->FindComponentByClass<UEmbeddedTurretMountComponent>()
		: nullptr;
	const AActor* EnemyGunner = IsValid(Mount) ? Mount->GetMountedTurret() : nullptr;
	Check(bHostHidden, TEXT("fog_hides_enemy_host"), FString::Printf(TEXT("%s t=%.1f"), *GetNameSafe(Host), Elapsed));
	Check(IsValid(EnemyGunner), TEXT("enemy_host_has_gunner"), GetNameSafe(Host));

	// ShouldRender follows the hidden host through the child actor component the gunner hangs in.
	auto CountRenderedMeshes = [](const AActor* Actor, int32& OutMeshes) -> int32
	{
		int32 Rendered = 0;
		OutMeshes = 0;
		TArray<UMeshComponent*> Meshes;
		if (IsValid(Actor))
		{
			Actor->GetComponents<UMeshComponent>(Meshes);
		}
		for (const UMeshComponent* Mesh : Meshes)
		{
			++OutMeshes;
			Rendered += Mesh->ShouldRender() ? 1 : 0;
		}
		return Rendered;
	};
	int32 EnemyGunnerMeshes = 0;
	const int32 EnemyGunnerRendered = CountRenderedMeshes(EnemyGunner, EnemyGunnerMeshes);
	Check(EnemyGunnerMeshes > 0 && EnemyGunnerRendered == 0, TEXT("fog_hides_enemy_gunner"),
	      FString::Printf(TEXT("meshes=%d rendered=%d"), EnemyGunnerMeshes, EnemyGunnerRendered));
	for (const FEmbInfantryTurretUnderTest& Unit : M_Units)
	{
		int32 PlayerGunnerMeshes = 0;
		const int32 PlayerGunnerRendered = CountRenderedMeshes(Unit.Turret.Get(), PlayerGunnerMeshes);
		Check(PlayerGunnerMeshes > 0 && PlayerGunnerRendered == PlayerGunnerMeshes, TEXT("player_gunner_rendered"),
		      FString::Printf(TEXT("%s meshes=%d rendered=%d"), *Unit.Label, PlayerGunnerMeshes, PlayerGunnerRendered));
	}
	Finish();
}

bool UEmbInfantryTurretTestSubsystem::GetGroundAt(const FVector2D& Location2D, FVector& OutGround) const
{
	using namespace EmbInfantryTurretTestPrivate;
	const UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	const FVector Start(Location2D.X, Location2D.Y, GroundTraceHalfHeight);
	const FVector End(Location2D.X, Location2D.Y, -GroundTraceHalfHeight);
	FHitResult GroundHit;
	const FCollisionObjectQueryParams ObjectParams(ECC_WorldStatic);
	if (not World->LineTraceSingleByObjectType(GroundHit, Start, End, ObjectParams))
	{
		return false;
	}
	OutGround = GroundHit.Location;
	return true;
}

AActor* UEmbInfantryTurretTestSubsystem::SpawnUnitForPlayer(const FString& ClassPath, const FTransform& Transform,
                                                            const uint8 OwningPlayer) const
{
	using namespace EmbInfantryTurretTestPrivate;
	UWorld* World = GetWorld();
	UClass* UnitClass = LoadClass<AActor>(nullptr, *ClassPath);
	if (not IsValid(World) || not IsValid(UnitClass))
	{
		return nullptr;
	}
	// The owner must be in place before BeginPlay, so the unit registers, sees and shoots as that player.
	TArray<TPair<TWeakObjectPtr<UObject>, uint8>> TemplateOwnersToRestore;
	OverrideRTSComponentTemplateOwners(UnitClass, OwningPlayer, TemplateOwnersToRestore);
	AActor* Unit = World->SpawnActorDeferred<AActor>(UnitClass, Transform, nullptr, nullptr,
	                                                 ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (not IsValid(Unit))
	{
		RestoreRTSComponentTemplateOwners(TemplateOwnersToRestore);
		return nullptr;
	}
	if (URTSComponent* RTSComponent = Unit->FindComponentByClass<URTSComponent>())
	{
		RTSComponent->SetOwningPlayerRuntime(OwningPlayer);
	}
	Unit->FinishSpawning(Transform);
	RestoreRTSComponentTemplateOwners(TemplateOwnersToRestore);
	if (APawn* Pawn = Cast<APawn>(Unit); IsValid(Pawn) && not Pawn->GetController())
	{
		Pawn->SpawnDefaultController();
	}
	return Unit;
}

void UEmbInfantryTurretTestSubsystem::CheckMountedTurret(FEmbInfantryTurretUnderTest& Unit)
{
	using namespace EmbInfantryTurretTestPrivate;
	AEmbeddedInfantryTurret* Turret = Unit.Turret.Get();
	Check(IsValid(Turret), TEXT("mounted_turret"), Unit.Label);
	if (not IsValid(Turret) || not Unit.Tank.IsValid())
	{
		return;
	}
	Check(Turret->GetClass()->GetName() == Unit.ExpectedTurretClassName, TEXT("turret_class"),
	      Unit.Label + TEXT(" ") + Turret->GetClass()->GetName());
	Check(Turret->GetParentActor() == Unit.Tank.Get(), TEXT("turret_parent_is_host"), Unit.Label);

	const USceneComponent* MountComponent = Turret->GetParentComponent();
	const USceneComponent* MountMesh = IsValid(MountComponent) ? MountComponent->GetAttachParent() : nullptr;
	const bool bOnTurretStaticMesh = IsValid(MountMesh) && MountMesh->IsA<UStaticMeshComponent>()
		&& MountMesh->GetOwner() != Unit.Tank.Get() && Cast<ACPPTurretsMaster>(MountMesh->GetOwner()) != nullptr;
	Check(bOnTurretStaticMesh && MountComponent->GetAttachSocketName() == MountSocketName,
	      TEXT("mounted_on_turret_static_mesh_socket"),
	      Unit.Label + TEXT(" ") + GetNameSafe(MountMesh) + TEXT(" of ")
	      + GetNameSafe(IsValid(MountMesh) ? MountMesh->GetOwner() : nullptr));
	if (IsValid(MountMesh))
	{
		const FVector Expected = MountMesh->GetSocketTransform(MountSocketName).TransformPosition(MountLocalOffset);
		const float Distance = FVector::Dist(Expected, Turret->GetActorLocation());
		Check(Distance <= MountLocationTolerance, TEXT("mounted_at_socket_offset"),
		      FString::Printf(TEXT("%s distance=%.2f socket=%s"), *Unit.Label, Distance,
		                      *MountMesh->GetSocketLocation(MountSocketName).ToCompactString()));
	}

	const ATankMaster* Tank = Cast<ATankMaster>(Unit.Tank.Get());
	Check(IsValid(Tank) && Tank->GetTurrets().Contains(Turret), TEXT("registered_with_host_turrets"), Unit.Label);
	Check(Turret->GetWeapons().Num() == 1, TEXT("one_weapon"),
	      FString::Printf(TEXT("%s weapons=%d range=%.0f"), *Unit.Label, Turret->GetWeapons().Num(),
	                      Turret->GetMaxWeaponRange()));
	Check(GetInfantryAnimInstance(Turret) != nullptr, TEXT("infantry_anim_instance"), Unit.Label);
	CheckTurretHasNoCollisionNavigationOrDecals(Unit);
}

void UEmbInfantryTurretTestSubsystem::CheckTurretHasNoCollisionNavigationOrDecals(const FEmbInfantryTurretUnderTest& Unit)
{
	TArray<UPrimitiveComponent*> PrimitiveComponents;
	Unit.Turret->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	FString Offenders;
	for (const UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		const bool bClean = PrimitiveComponent->GetCollisionEnabled() == ECollisionEnabled::NoCollision
			&& not PrimitiveComponent->CanEverAffectNavigation()
			&& not PrimitiveComponent->bReceivesDecals;
		if (not bClean)
		{
			Offenders += PrimitiveComponent->GetName() + TEXT(" ");
		}
	}
	Check(PrimitiveComponents.Num() > 0 && Offenders.IsEmpty(), TEXT("no_collision_navigation_or_decals"),
	      FString::Printf(TEXT("%s components=%d offenders=%s"), *Unit.Label, PrimitiveComponents.Num(), *Offenders));
}

void UEmbInfantryTurretTestSubsystem::SpawnEnemyInRange()
{
	using namespace EmbInfantryTurretTestPrivate;
	float ShortestRange = TNumericLimits<float>::Max();
	for (const FEmbInfantryTurretUnderTest& Unit : M_Units)
	{
		if (Unit.Turret.IsValid())
		{
			ShortestRange = FMath::Min(ShortestRange, Unit.Turret->GetMaxWeaponRange());
		}
	}
	if (ShortestRange == TNumericLimits<float>::Max())
	{
		ShortestRange = MinimumEnemyDistance;
	}
	const float Distance = FMath::Max(MinimumEnemyDistance, ShortestRange * EnemyRangeFraction);
	const FVector2D Spot(M_Anchor.X + Distance, M_Anchor.Y + TankSpacing * 0.5f);
	FVector Ground;
	if (not GetGroundAt(Spot, Ground))
	{
		Ground = M_Anchor + FVector(Distance, 0.f, 0.f);
	}
	const FRotator FacingHosts(0.f, 180.f, 0.f);
	M_Enemy = SpawnUnitForPlayer(EnemyClassPath, FTransform(FacingHosts, Ground + FVector(0.f, 0.f, UnitLift)), 2);
	Check(M_Enemy.IsValid(), TEXT("spawned_enemy"), FString::Printf(TEXT("distance=%.0f range=%.0f"), Distance,
	                                                                ShortestRange));
}

void UEmbInfantryTurretTestSubsystem::CountShotsAndAim(FEmbInfantryTurretUnderTest& Unit)
{
	AEmbeddedInfantryTurret* Turret = Unit.Turret.Get();
	if (not IsValid(Turret) || not M_Enemy.IsValid())
	{
		return;
	}
	Unit.bHasTargetedEnemy |= Turret->GetCurrentTargetActor() == M_Enemy.Get();
	// Unrendered soldiers only tick montages; the pose (and the pictures) need the aim pitch applied too.
	if (USkeletalMeshComponent* SoldierMesh = Turret->FindComponentByClass<USkeletalMeshComponent>())
	{
		SoldierMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}

	TArray<UWeaponState*> Weapons = Turret->GetWeapons();
	for (int32 WeaponSlot = 0; WeaponSlot < Weapons.Num(); ++WeaponSlot)
	{
		const int32 Magazine = IsValid(Weapons[WeaponSlot]) ? Weapons[WeaponSlot]->GetCurrentMagCapacity() : 0;
		if (not Unit.LastMagazineCounts.IsValidIndex(WeaponSlot))
		{
			Unit.LastMagazineCounts.Add(Magazine);
			continue;
		}
		Unit.Shots += FMath::Max(0, Unit.LastMagazineCounts[WeaponSlot] - Magazine);
		Unit.LastMagazineCounts[WeaponSlot] = Magazine;
	}

	if (const UEmbeddedInfantryTurretAnimInstance* AnimInstance =
		EmbInfantryTurretTestPrivate::GetInfantryAnimInstance(Turret))
	{
		Unit.bSawFireMontage |= AnimInstance->IsAnyMontagePlaying();
		Unit.MaxAbsAimPitch = FMath::Max(Unit.MaxAbsAimPitch, FMath::Abs(AnimInstance->GetCurrentPitchAngle()));
		Unit.MaxAbsTargetPitch = FMath::Max(Unit.MaxAbsTargetPitch, FMath::Abs(AnimInstance->GetTargetAimPitch()));
	}
	const float YawToEnemy = UKismetMathLibrary::FindLookAtRotation(
		Turret->GetActorLocation(), M_Enemy->GetActorLocation()).Yaw;
	const float YawError = FMath::Abs(FMath::FindDeltaAngleDegrees(Turret->GetActorRotation().Yaw, YawToEnemy));
	Unit.SmallestYawErrorToEnemy = FMath::Min(Unit.SmallestYawErrorToEnemy, YawError);
}

void UEmbInfantryTurretTestSubsystem::KeepUnitsAlive() const
{
	TArray<AActor*> Actors;
	for (const FEmbInfantryTurretUnderTest& Unit : M_Units)
	{
		Actors.Add(Unit.Tank.Get());
	}
	Actors.Add(M_Enemy.Get());
	for (AActor* Actor : Actors)
	{
		UHealthComponent* Health = IsValid(Actor) ? Actor->FindComponentByClass<UHealthComponent>() : nullptr;
		if (IsValid(Health))
		{
			Health->AddHealth(Health->GetMaxHealth());
		}
	}
}

void UEmbInfantryTurretTestSubsystem::CaptureUnits(const FString& Moment)
{
	if (not M_CaptureRig.GetIsEnabled())
	{
		return;
	}
	for (const FEmbInfantryTurretUnderTest& Unit : M_Units)
	{
		if (not Unit.Turret.IsValid())
		{
			continue;
		}
		const TArray<FB4HowitzerCameraView> Views = {
			{TEXT("Front"), FVector(420.f, 160.f, 140.f), FVector(0.f, 0.f, 120.f), 50.f},
			{TEXT("Side"), FVector(0.f, -520.f, 220.f), FVector(0.f, 0.f, 80.f), 50.f},
			{TEXT("Top"), FVector(-250.f, -60.f, 600.f), FVector(0.f, 0.f, 60.f), 55.f},
		};
		M_CaptureRig.CaptureViews(*GetWorld(), Unit.Turret->GetActorTransform(), Views,
		                          FString::Printf(TEXT("%02d_%s_%s"), M_CaptureCount, *Unit.Label, *Moment));
	}
	++M_CaptureCount;
}

#else

bool UEmbInfantryTurretTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UEmbInfantryTurretTestSubsystem::Tick(const float DeltaTime)
{
}

TStatId UEmbInfantryTurretTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UEmbInfantryTurretTestSubsystem, STATGROUP_Tickables);
}

#endif
