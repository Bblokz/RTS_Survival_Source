// Copyright (C) Bas Blokzijl - All rights reserved.

#include "CarpetBombingTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/GlobalAbilitySetDataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/RTSGlobalAbilitySettings.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"
#include "RTS_Survival/Weapons/BombComponent/BombActor/BombActor.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSCarpetBombingTest, Log, All);

namespace CarpetBombingTestPrivate
{
	const TCHAR* const Switch = TEXT("CarpetBombingValidate");
	constexpr double StartDelaySeconds = 8.0;
	constexpr double BomberSpawnTimeoutSeconds = 30.0;
	constexpr double BombingTimeoutSeconds = 180.0;
	// After the ability ends the last bombs may still be falling (unused bombs stay visible on the leaving bomber).
	constexpr double AfterEndSeconds = 4.0;
	constexpr int32 HumanPlayer = 1;
	constexpr uint8 EnemyPlayer = 2;
	constexpr float GroundTraceHalfHeight = 100000.f;
	constexpr float TankSpawnLiftCm = 100.f;
	// Impacts this far behind the target (toward the spawn) are bombs of a run that turned back over the target.
	constexpr float BehindTargetToleranceCm = 1500.f;
	// Bombs fall ahead of the aircraft and the run may overshoot its end a little while turning.
	constexpr float CarpetEndToleranceCm = 2500.f;
	constexpr float MinShareOnCarpet = 0.7f;
	constexpr int32 MinBombImpacts = 4;
	const float TankRunFractions[] = {0.3f, 0.65f};
	const TCHAR* const TankClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Medium/T34/Rus_Vehicle_T3476."
		"Rus_Vehicle_T3476_C");
}

bool UCarpetBombingTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
#if RTS_WITH_MAP_TESTS
	const UWorld* const OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), CarpetBombingTestPrivate::Switch);
#else
	return false;
#endif
}

TStatId UCarpetBombingTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCarpetBombingTestSubsystem, STATGROUP_Tickables);
}

void UCarpetBombingTestSubsystem::Tick(const float DeltaTime)
{
	const UWorld* const World = GetWorld();
	if (not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	ACPPController* const Controller = FRTS_Statics::GetRTSController(GetWorld());
	if (IsValid(Controller) && Controller->GetIsGameOnPause())
	{
		Controller->PauseGame(ERTSPauseGameOptions::ForceUnpause);
		return;
	}
	if (M_PhaseStartSeconds < 0.0)
	{
		M_PhaseStartSeconds = World->GetTimeSeconds();
		return;
	}
	switch (M_Phase)
	{
	case EPhase::WaitingForStart:
		if (GetPhaseSeconds() >= CarpetBombingTestPrivate::StartDelaySeconds)
		{
			StartBombing();
		}
		break;
	case EPhase::WaitingForBomber:
		TickWaitingForBomber();
		break;
	case EPhase::Bombing:
		TickBombing();
		break;
	case EPhase::Finished:
		break;
	}
}

void UCarpetBombingTestSubsystem::SetPhase(const EPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartSeconds = GetWorld()->GetTimeSeconds();
}

double UCarpetBombingTestSubsystem::GetPhaseSeconds() const
{
	return GetWorld()->GetTimeSeconds() - M_PhaseStartSeconds;
}

void UCarpetBombingTestSubsystem::StartBombing()
{
	using namespace CarpetBombingTestPrivate;
	ACPPController* const Controller = FRTS_Statics::GetRTSController(GetWorld());
	UGlobalAbilitiesManager* const Manager = IsValid(Controller) ? Controller->GetGlobalAbilitiesManager() : nullptr;
	const URTSGlobalAbilitySettings* const Settings = GetDefault<URTSGlobalAbilitySettings>();
	UGlobalAbilitySetDataAsset* const DataAsset = IsValid(Settings)
		                                              ? Settings->GlobalAbilityDataAsset.LoadSynchronous()
		                                              : nullptr;
	M_Ability = IsValid(DataAsset)
		            ? DataAsset->GetGLobalAbilityForTypeAsCopyFromTemplate(EGlobalAbility::GA_HeavyCarpet_Ju390B_250Gr)
		            : nullptr;
	const FVector CameraLocation = IsValid(Controller) && IsValid(Controller->PlayerCameraManager)
		                               ? Controller->PlayerCameraManager->GetCameraLocation()
		                               : FVector::ZeroVector;
	const bool bHasGround = FindGround(FVector2D(CameraLocation), M_Target);
	Check(IsValid(Manager) && IsValid(M_Ability) && bHasGround, TEXT("setup"),
	      FString::Printf(TEXT("manager=%d ability=%d ground=%d"), IsValid(Manager), IsValid(M_Ability), bHasGround));
	if (not IsValid(Manager) || not IsValid(M_Ability) || not bHasGround)
	{
		Evaluate();
		return;
	}
	// The private carpet length, read like the preview reads it.
	const FStructProperty* const SettingsProperty = FindFProperty<FStructProperty>(
		M_Ability->GetClass(), TEXT("M_AircraftBombingSettings"));
	const FFloatProperty* const LengthProperty = SettingsProperty != nullptr
		                                             ? FindFProperty<FFloatProperty>(
			                                             SettingsProperty->Struct, TEXT("CarpetBombingLength"))
		                                             : nullptr;
	M_CarpetLength = LengthProperty != nullptr
		                 ? *LengthProperty->ContainerPtrToValuePtr<float>(
			                 SettingsProperty->ContainerPtrToValuePtr<void>(M_Ability.Get()))
		                 : 0.f;
	for (TActorIterator<AAircraftMaster> It(GetWorld()); It; ++It)
	{
		M_AircraftOfTheMap.Add(*It);
	}
	// Executed directly like the ability preview does: no panel, so no costs or requirements are checked.
	M_Ability->InitGlobalAbility(HumanPlayer, Manager, Controller);
	TWeakObjectPtr<UCarpetBombingTestSubsystem> WeakThis(this);
	M_Ability->OnGlobalAbilityEnded.AddWeakLambda(this, [WeakThis](UGlobalAbility*)
	{
		if (WeakThis.IsValid())
		{
			WeakThis->bM_AbilityEnded = true;
			WeakThis->M_AbilityEndedSeconds = WeakThis->GetWorld()->GetTimeSeconds();
		}
	});
	M_Ability->ExecuteAbilityAtLocation(M_Target);
	UE_LOG(LogRTSCarpetBombingTest, Display, TEXT("RTS_CARPET_TEST executed at %s carpet_length=%.0f"),
	       *M_Target.ToCompactString(), M_CarpetLength);
	SetPhase(EPhase::WaitingForBomber);
}

void UCarpetBombingTestSubsystem::TickWaitingForBomber()
{
	using namespace CarpetBombingTestPrivate;
	for (TActorIterator<AAircraftMaster> It(GetWorld()); It; ++It)
	{
		if (M_AircraftOfTheMap.Contains(*It))
		{
			continue;
		}
		M_Bomber = *It;
		M_BomberSpawn = It->GetActorLocation();
		// The bomber is far out; the tanks stand on the run before it arrives.
		SpawnTanksOnRun();
		SetPhase(EPhase::Bombing);
		return;
	}
	if (GetPhaseSeconds() > BomberSpawnTimeoutSeconds)
	{
		Check(false, TEXT("bomber_spawned"), TEXT("no new aircraft"));
		Evaluate();
	}
}

void UCarpetBombingTestSubsystem::SpawnTanksOnRun()
{
	using namespace CarpetBombingTestPrivate;
	UClass* const TankClass = TSoftClassPtr<AActor>(FSoftObjectPath(TankClassPath)).LoadSynchronous();
	const FVector RunDirection = (M_Target - M_BomberSpawn).GetSafeNormal2D();
	for (const float RunFraction : TankRunFractions)
	{
		FVector Ground;
		const FVector OnRun = M_Target + RunDirection * (RunFraction * M_CarpetLength);
		if (not IsValid(TankClass) || not FindGround(FVector2D(OnRun), Ground))
		{
			continue;
		}
		const FTransform Transform(RunDirection.Rotation(), Ground + FVector::UpVector * TankSpawnLiftCm);
		if (AActor* const Tank = FRTSUnitOwnerSpawning::SpawnUnitForPlayer(*GetWorld(), TankClass, Transform,
		                                                                   EnemyPlayer))
		{
			M_Tanks.Add(Tank);
		}
	}
	Check(M_Tanks.Num() == UE_ARRAY_COUNT(TankRunFractions), TEXT("tanks_on_run"), FString::FromInt(M_Tanks.Num()));
}

void UCarpetBombingTestSubsystem::TickBombing()
{
	using namespace CarpetBombingTestPrivate;
	TrackBombImpacts();
	if (bM_AbilityEnded && GetWorld()->GetTimeSeconds() - M_AbilityEndedSeconds > AfterEndSeconds)
	{
		Evaluate();
		return;
	}
	if (GetPhaseSeconds() > BombingTimeoutSeconds)
	{
		Check(false, TEXT("ability_ended_in_time"), FString::Printf(TEXT("ended=%d"), bM_AbilityEnded ? 1 : 0));
		Evaluate();
	}
}

void UCarpetBombingTestSubsystem::TrackBombImpacts()
{
	for (TActorIterator<ABombActor> It(GetWorld()); It; ++It)
	{
		const TWeakObjectPtr<AActor> Bomb(*It);
		if (not It->IsHidden())
		{
			M_BombLastSeen.FindOrAdd(Bomb) = It->GetActorLocation();
			continue;
		}
		// Hidden again after a fall: that fall ended where it was last seen.
		if (const FVector* const LastSeen = M_BombLastSeen.Find(Bomb))
		{
			M_BombImpacts.Add(*LastSeen);
			M_BombLastSeen.Remove(Bomb);
		}
	}
}

void UCarpetBombingTestSubsystem::Evaluate()
{
	using namespace CarpetBombingTestPrivate;
	if (M_Phase == EPhase::Finished)
	{
		return;
	}
	const FVector RunDirection = (M_Target - M_BomberSpawn).GetSafeNormal2D();
	int32 OnCarpet = 0;
	int32 BehindTarget = 0;
	float FarthestBehind = 0.f;
	for (const FVector& Impact : M_BombImpacts)
	{
		const float AlongRun = FVector::DotProduct(Impact - M_Target, RunDirection);
		OnCarpet += AlongRun >= -BehindTargetToleranceCm && AlongRun <= M_CarpetLength + CarpetEndToleranceCm ? 1 : 0;
		if (AlongRun < -BehindTargetToleranceCm)
		{
			++BehindTarget;
			FarthestBehind = FMath::Max(FarthestBehind, -AlongRun);
		}
	}
	int32 TanksHit = 0;
	for (const TWeakObjectPtr<AActor>& Tank : M_Tanks)
	{
		const UHealthComponent* const Health = Tank.IsValid() ? Tank->FindComponentByClass<UHealthComponent>() : nullptr;
		TanksHit += not Tank.IsValid() || (IsValid(Health) && Health->GetCurrentHealth() < Health->GetMaxHealth())
			            ? 1
			            : 0;
	}
	const int32 Impacts = M_BombImpacts.Num();
	Check(Impacts >= MinBombImpacts, TEXT("bombs_dropped"), FString::FromInt(Impacts));
	Check(Impacts > 0 && OnCarpet >= FMath::CeilToInt(MinShareOnCarpet * Impacts), TEXT("bombs_on_carpet_past_target"),
	      FString::Printf(TEXT("%d of %d"), OnCarpet, Impacts));
	Check(BehindTarget == 0, TEXT("no_bombs_back_toward_spawn"),
	      FString::Printf(TEXT("%d, farthest %.0f cm behind"), BehindTarget, FarthestBehind));
	Check(TanksHit >= 1, TEXT("carpet_hit_tanks"), FString::Printf(TEXT("%d of %d"), TanksHit, M_Tanks.Num()));
	M_Lines.Add(FString::Printf(TEXT("SUMMARY %s impacts=%d on_carpet=%d behind_target=%d tanks_hit=%d carpet_length=%.0f"),
	                            M_Failed == 0 ? TEXT("PASS") : TEXT("FAIL"), Impacts, OnCarpet, BehindTarget, TanksHit,
	                            M_CarpetLength));
	UE_LOG(LogRTSCarpetBombingTest, Display, TEXT("RTS_CARPET_TEST %s"), *M_Lines.Last());
	const FString ResultsPath = FPaths::ProjectSavedDir() / TEXT("CarpetBombingTest") / TEXT("results.txt");
	FFileHelper::SaveStringToFile(FString::Join(M_Lines, TEXT("\n")), *ResultsPath);
	SetPhase(EPhase::Finished);
	if (APlayerController* const Controller = GetWorld()->GetFirstPlayerController())
	{
		Controller->ConsoleCommand(TEXT("quit"));
	}
}

void UCarpetBombingTestSubsystem::Check(const bool bCondition, const FString& Name, const FString& Detail)
{
	M_Lines.Add(FString::Printf(TEXT("%s %s %s"), bCondition ? TEXT("PASS") : TEXT("FAIL"), *Name, *Detail));
	UE_LOG(LogRTSCarpetBombingTest, Display, TEXT("RTS_CARPET_TEST %s"), *M_Lines.Last());
	M_Failed += bCondition ? 0 : 1;
}

bool UCarpetBombingTestSubsystem::FindGround(const FVector2D& XY, FVector& OutGround) const
{
	using namespace CarpetBombingTestPrivate;
	const FVector Top(XY.X, XY.Y, GroundTraceHalfHeight);
	const FVector Bottom(XY.X, XY.Y, -GroundTraceHalfHeight);
	FCollisionObjectQueryParams ObjectQuery;
	ObjectQuery.AddObjectTypesToQuery(ECC_WorldStatic);
	FHitResult Hit;
	if (not GetWorld()->LineTraceSingleByObjectType(Hit, Top, Bottom, ObjectQuery))
	{
		return false;
	}
	OutGround = Hit.ImpactPoint;
	return true;
}
