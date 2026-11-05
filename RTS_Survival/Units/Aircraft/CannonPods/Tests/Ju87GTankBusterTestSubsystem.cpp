// Copyright (C) Bas Blokzijl - All rights reserved.

#include "Ju87GTankBusterTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "EngineUtils.h"
#include "UnrealClient.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/GlobalAbilitySetDataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/RTSGlobalAbilitySettings.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityTier.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/AircraftStrafing/GA_AircraftStrafing.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/MasterObjects/HealthBase/HpPawnMaster.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/Aircraft/CannonPods/AircraftCannonPodsComponent.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"
#include "RTS_Survival/Weapons/BombComponent/BombComponent.h"
#include "RTS_Survival/Weapons/BombComponent/BombActor/BombActor.h"
#include "RTS_Survival/Weapons/Projectile/Projectile.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSJu87GTest, Log, All);

namespace Ju87GTestPrivate
{
	constexpr int32 TestPlayer = 1;
	constexpr int32 EnemyPlayer = 2;
	constexpr float StartDelaySeconds = 3.f;
	constexpr float CameraTraceLength = 500000.f;
	constexpr float CloseUpSetupSeconds = 2.5f;
	constexpr float CloseUpShotIntervalSeconds = 1.f;
	constexpr int32 CloseUpShotCount = 3;
	constexpr float CloseUpHeightCm = 1500.f;
	const FVector CloseUpOffsetFromAnchor(-4000.f, -4000.f, 0.f);
	constexpr float StrikeTimeoutSeconds = 300.f;
	constexpr float AfterAbilityEndSeconds = 3.f;
	constexpr float PeriodicShotSeconds = 10.f;
	constexpr int32 MaxDiveShots = 5;
	constexpr float DiveShotIntervalSeconds = 0.6f;
	constexpr float StateLogIntervalSeconds = 2.f;
	constexpr float TargetSpawnAboveGroundCm = 120.f;
	constexpr float ProjectileSearchRadiusCm = 900.f;
	constexpr int32 ExpectedAircraft = 2;
	constexpr int32 ExpectedPods = 2;
	constexpr float ExpectedPodLateralCm = 245.f;
	constexpr float PodLateralToleranceCm = 40.f;
	constexpr float MinMuzzleFromPodCm = 200.f;
	constexpr float MaxMuzzleFromPodCm = 350.f;
	constexpr float ScaleTolerance = 0.02f;
	constexpr float MinCooldownSeconds = 180.f;
	constexpr float MaxCooldownSeconds = 240.f;
	const TCHAR* const Ju87GMarker = TEXT("Ju87G");
	const TCHAR* const ExpectedCannonWeapon = TEXT("Flak36_37MM");
	const TCHAR* const Ju87GClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/Aircraft/GerAircraft/JU_87VTO/Ju87G/Ger_Aircraft_StukaJu87G_Airborne."
		"Ger_Aircraft_StukaJu87G_Airborne_C");
	const TCHAR* const T34ClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Medium/T34/Rus_Vehicle_T3476.Rus_Vehicle_T3476_C");
	const TCHAR* const KV1ClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Heavy/KV1/Rus_Vehicle_KV-1E.Rus_Vehicle_KV-1E_C");

	struct FTargetSpawn
	{
		const TCHAR* ClassPath;
		const TCHAR* Label;
		FVector2D Offset;
	};

	const FTargetSpawn TargetSpawns[] = {
		{T34ClassPath, TEXT("T34_A"), FVector2D(600.f, 0.f)},
		{T34ClassPath, TEXT("T34_B"), FVector2D(-600.f, 300.f)},
		{KV1ClassPath, TEXT("KV1"), FVector2D(0.f, -700.f)},
	};

	float GetHealth(const AActor* Actor)
	{
		const AHpPawnMaster* Pawn = Cast<AHpPawnMaster>(Actor);
		const UHealthComponent* Health = IsValid(Pawn) ? Pawn->GetHealthComponent() : nullptr;
		return IsValid(Health) ? Health->GetCurrentHealth() : 0.f;
	}

	FString DescribeShotData(const UWeaponState& Weapon)
	{
		const FWeaponData Data = Weapon.GetWeaponDataAdjustedForShellType();
		return FString::Printf(
			TEXT("weapon=%s shell=%s calibre=%.0f damage=%.1f pen=%.1f pen_max_range=%.1f speed=%.0f range=%.0f mag=%d "
				"reload=%.2f cooldown=%.2f accuracy=%d tnt=%.1f"),
			*UEnum::GetValueAsString(Data.WeaponName), *UEnum::GetValueAsString(Data.ShellType), Data.WeaponCalibre,
			Data.BaseDamage, Data.ArmorPen, Data.ArmorPenMaxRange, Data.ProjectileMovementSpeed, Data.Range,
			Data.MagCapacity, Data.ReloadSpeed, Data.BaseCooldown, Data.Accuracy, Data.TNTExplosiveGrams);
	}
}

bool UJu87GTankBusterTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), TEXT("Ju87GTankBustersValidate"));
}

TStatId UJu87GTankBusterTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UJu87GTankBusterTestSubsystem, STATGROUP_Tickables);
}

void UJu87GTankBusterTestSubsystem::Tick(const float DeltaTime)
{
	const UWorld* World = GetWorld();
	if (not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	switch (M_Phase)
	{
	case EJu87GTestPhase::WaitingForStart:
		TickWaitingForStart();
		return;
	case EJu87GTestPhase::CloseUp:
		TickCloseUp();
		return;
	case EJu87GTestPhase::Strike:
		TickStrike();
		return;
	case EJu87GTestPhase::Finished:
		return;
	}
}

void UJu87GTankBusterTestSubsystem::Check(const bool bCondition, const FString& Description, const FString& Details)
{
	const TCHAR* Phase = M_Phase == EJu87GTestPhase::CloseUp ? TEXT("CloseUp") : M_Phase == EJu87GTestPhase::Strike
		                                                                             ? TEXT("Strike")
		                                                                             : TEXT("Setup");
	if (bCondition)
	{
		++M_PassedCheckCount;
		UE_LOG(LogRTSJu87GTest, Display, TEXT("RTS_JU87G_TEST CHECK PASS %s.%s %s"), Phase, *Description, *Details);
		return;
	}
	++M_FailedCheckCount;
	UE_LOG(LogRTSJu87GTest, Error, TEXT("RTS_JU87G_TEST CHECK FAIL %s.%s %s"), Phase, *Description, *Details);
}

float UJu87GTankBusterTestSubsystem::GetNow() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.f;
}

float UJu87GTankBusterTestSubsystem::GetPhaseSeconds() const
{
	return GetNow() - M_PhaseStartSeconds;
}

void UJu87GTankBusterTestSubsystem::Finish()
{
	const bool bPassed = M_FailedCheckCount == 0 && M_PassedCheckCount > 0;
	UE_LOG(LogRTSJu87GTest, Display, TEXT("RTS_JU87G_TEST RESULT %s passed=%d failed=%d"),
	       bPassed ? TEXT("PASS") : TEXT("FAIL"), M_PassedCheckCount, M_FailedCheckCount);
	M_Phase = EJu87GTestPhase::Finished;
	if (APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld()))
	{
		Controller->ConsoleCommand(TEXT("quit"));
	}
}

void UJu87GTankBusterTestSubsystem::TickWaitingForStart()
{
	using namespace Ju87GTestPrivate;
	ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	if (not IsValid(Controller))
	{
		return;
	}
	// The map boots paused behind the start-game widget; start it as the player's "Press To Start" does.
	UPlayerStartGameControl* StartGameControl = Controller->FindComponentByClass<UPlayerStartGameControl>();
	if (IsValid(StartGameControl) && StartGameControl->GetIsWaitingForPlayerToStart())
	{
		StartGameControl->PlayerStartedGame();
	}
	if (Controller->GetIsGameOnPause())
	{
		Controller->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
	if (M_StartSeconds < 0.f)
	{
		M_StartSeconds = GetNow();
		return;
	}
	if (GetNow() - M_StartSeconds < StartDelaySeconds)
	{
		return;
	}
	if (not FindAnchor(M_Anchor))
	{
		Check(false, TEXT("found_ground_anchor"));
		Finish();
		return;
	}
	if (UWorld* World = GetWorld())
	{
		M_Camera = World->SpawnActor<ACameraActor>();
	}
	UE_LOG(LogRTSJu87GTest, Display, TEXT("RTS_JU87G_TEST ANCHOR %s"), *M_Anchor.ToCompactString());
	StartCloseUp();
}

bool UJu87GTankBusterTestSubsystem::FindAnchor(FVector& OutAnchor) const
{
	using namespace Ju87GTestPrivate;
	// FParse::Value stops at commas, so the anchor comes as two values.
	float AnchorX = 0.f;
	float AnchorY = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("Ju87GAnchorX="), AnchorX)
		&& FParse::Value(FCommandLine::Get(), TEXT("Ju87GAnchorY="), AnchorY))
	{
		return GlobalAbilityHelpers::TryFindGroundLocation(this, FVector(AnchorX, AnchorY, 0.f), OutAnchor);
	}
	const UWorld* World = GetWorld();
	const APlayerController* Controller = FRTS_Statics::GetRTSController(World);
	if (not IsValid(Controller) || not IsValid(Controller->PlayerCameraManager))
	{
		return false;
	}
	const FVector CameraLocation = Controller->PlayerCameraManager->GetCameraLocation();
	const FVector CameraForward = Controller->PlayerCameraManager->GetCameraRotation().Vector();
	FHitResult GroundHit;
	if (not World->LineTraceSingleByChannel(GroundHit, CameraLocation, CameraLocation + CameraForward * CameraTraceLength,
	                                        ECC_Visibility))
	{
		return false;
	}
	return GlobalAbilityHelpers::TryFindGroundLocation(this, GroundHit.Location, OutAnchor);
}

// ---------------------------------------------------------------- Close-up

void UJu87GTankBusterTestSubsystem::StartCloseUp()
{
	using namespace Ju87GTestPrivate;
	M_Phase = EJu87GTestPhase::CloseUp;
	M_PhaseStartSeconds = GetNow();
	UClass* AircraftClass = LoadClass<AAircraftMaster>(nullptr, Ju87GClassPath);
	Check(IsValid(AircraftClass), TEXT("ju87g_class_loads"), Ju87GClassPath);
	if (not IsValid(AircraftClass))
	{
		StartStrike();
		return;
	}
	FVector Ground = M_Anchor + CloseUpOffsetFromAnchor;
	GlobalAbilityHelpers::TryFindGroundLocation(this, M_Anchor + CloseUpOffsetFromAnchor, Ground);
	AAircraftMaster* Aircraft = GlobalAbilityHelpers::SpawnAircraftAtLocation(
		this, AircraftClass, Ground + FVector(0.f, 0.f, CloseUpHeightCm));
	Check(IsValid(Aircraft), TEXT("closeup_aircraft_spawned"));
	if (not IsValid(Aircraft))
	{
		StartStrike();
		return;
	}
	Aircraft->SetUnitSelectable(false);
	M_CloseUpAircraft = Aircraft;
}

void UJu87GTankBusterTestSubsystem::TickCloseUp()
{
	using namespace Ju87GTestPrivate;
	if (not M_CloseUpAircraft.IsValid())
	{
		Check(false, TEXT("closeup_aircraft_alive"));
		StartStrike();
		return;
	}
	AAircraftMaster& Aircraft = *M_CloseUpAircraft.Get();
	PlaceCloseUpCamera(Aircraft, M_CloseUpShotIndex);
	if (GetPhaseSeconds() < CloseUpSetupSeconds)
	{
		return;
	}
	if (not bM_CloseUpChecked)
	{
		bM_CloseUpChecked = true;
		CheckCloseUpAircraft(Aircraft);
		M_NextCloseUpShotSeconds = GetPhaseSeconds();
	}
	if (GetPhaseSeconds() < M_NextCloseUpShotSeconds)
	{
		return;
	}
	if (M_CloseUpShotIndex < CloseUpShotCount)
	{
		// The camera moved to this shot's angle last frame.
		RequestScreenshot(FString::Printf(TEXT("CloseUp_%d"), M_CloseUpShotIndex));
		++M_CloseUpShotIndex;
		M_NextCloseUpShotSeconds = GetPhaseSeconds() + CloseUpShotIntervalSeconds;
		return;
	}
	Aircraft.Destroy();
	StartStrike();
}

void UJu87GTankBusterTestSubsystem::CheckCloseUpAircraft(AAircraftMaster& Aircraft)
{
	using namespace Ju87GTestPrivate;
	const UAircraftCannonPodsComponent* PodsComponent = Aircraft.FindComponentByClass<UAircraftCannonPodsComponent>();
	Check(IsValid(PodsComponent), TEXT("has_cannon_pods_component"), Aircraft.GetClass()->GetName());
	const USkeletalMeshComponent* AircraftMesh = Aircraft.FindComponentByClass<USkeletalMeshComponent>();
	if (not IsValid(PodsComponent) || not IsValid(AircraftMesh))
	{
		return;
	}
	UE_LOG(LogRTSJu87GTest, Display, TEXT("RTS_JU87G_TEST AIRCRAFT_MESH %s"),
	       IsValid(AircraftMesh->GetSkeletalMeshAsset()) ? *AircraftMesh->GetSkeletalMeshAsset()->GetPathName() : TEXT("none"));
	const TArray<TObjectPtr<UStaticMeshComponent>>& PodComponents = PodsComponent->GetPodComponents();
	Check(PodComponents.Num() == ExpectedPods, TEXT("two_pods"), FString::Printf(TEXT("pods=%d"), PodComponents.Num()));
	const FTransform AircraftTransform = Aircraft.GetActorTransform();
	for (int32 PodIndex = 0; PodIndex < PodComponents.Num(); ++PodIndex)
	{
		const UStaticMeshComponent* Pod = PodComponents[PodIndex];
		if (not IsValid(Pod))
		{
			Check(false, FString::Printf(TEXT("pod_%d_valid"), PodIndex));
			continue;
		}
		const FVector Local = AircraftTransform.InverseTransformPosition(Pod->GetComponentLocation());
		const FVector Scale = Pod->GetComponentScale();
		Check(Pod->IsVisible() && Pod->GetAttachParent() == AircraftMesh
		      && FMath::Abs(FMath::Abs(Local.Y) - ExpectedPodLateralCm) <= PodLateralToleranceCm
		      && Scale.Equals(FVector::OneVector, ScaleTolerance),
		      FString::Printf(TEXT("pod_%d_under_wing"), PodIndex),
		      FString::Printf(TEXT("mesh=%s socket=%s local=%s scale=%s visible=%d"),
		                      IsValid(Pod->GetStaticMesh()) ? *Pod->GetStaticMesh()->GetName() : TEXT("none"),
		                      *Pod->GetAttachSocketName().ToString(), *Local.ToCompactString(),
		                      *Scale.ToCompactString(), Pod->IsVisible()));
		if (not PodsComponent->GetPods().IsValidIndex(PodIndex))
		{
			continue;
		}
		const FName MuzzleSocket = PodsComponent->GetPods()[PodIndex].MuzzleSocketName;
		const FVector Muzzle = AircraftMesh->GetSocketLocation(MuzzleSocket);
		const FVector MuzzleLocal = AircraftTransform.InverseTransformPosition(Muzzle);
		const float MuzzleFromPod = FVector::Dist(Muzzle, Pod->GetComponentLocation());
		Check(AircraftMesh->DoesSocketExist(MuzzleSocket) && MuzzleFromPod >= MinMuzzleFromPodCm
		      && MuzzleFromPod <= MaxMuzzleFromPodCm && MuzzleLocal.X > Local.X,
		      FString::Printf(TEXT("pod_%d_muzzle_at_barrel_tip"), PodIndex),
		      FString::Printf(TEXT("socket=%s local=%s from_pod=%.0f"), *MuzzleSocket.ToString(),
		                      *MuzzleLocal.ToCompactString(), MuzzleFromPod));
	}

	const UBombComponent* BombBay = Aircraft.GetBombComponent();
	Check(not IsValid(BombBay) && not IsValid(Aircraft.FindComponentByClass<UBombComponent>()), TEXT("no_bomb_bay"));
	Check(Aircraft.GetAircraftTargetPreference() == ETargetPreference::Tank, TEXT("prefers_tanks"),
	      Global_GetTargetPreferenceAsString(Aircraft.GetAircraftTargetPreference()));

	const TArray<UWeaponState*> Cannons = PodsComponent->GetCannons();
	Check(Cannons.Num() == ExpectedPods, TEXT("two_cannons"), FString::Printf(TEXT("cannons=%d"), Cannons.Num()));
	for (const UWeaponState* Cannon : Cannons)
	{
		const FWeaponData& Raw = Cannon->GetRawWeaponData();
		const FString WeaponName = UEnum::GetValueAsString(Raw.WeaponName);
		Check(WeaponName.Contains(ExpectedCannonWeapon) && Raw.ShellType == EWeaponShellType::Shell_APCR
		      && Raw.Range >= DeveloperSettings::GameBalance::Ranges::AircraftSmallArmsRange - 1.f,
		      TEXT("cannon_is_37mm_apcr"), DescribeShotData(*Cannon) + TEXT(" socket=") +
		      Cannon->GetWeaponSocket().ToString());
	}
	FString AllWeapons;
	for (const UWeaponState* Weapon : Aircraft.GetAllAircraftWeapons())
	{
		AllWeapons += UEnum::GetValueAsString(Weapon->GetRawWeaponData().WeaponName) + TEXT("@") +
			Weapon->GetWeaponSocket().ToString() + TEXT(" ");
	}
	UE_LOG(LogRTSJu87GTest, Display, TEXT("RTS_JU87G_TEST WEAPONS %s"), *AllWeapons);
}

void UJu87GTankBusterTestSubsystem::PlaceCloseUpCamera(const AAircraftMaster& Aircraft, const int32 ShotIndex) const
{
	APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	if (not IsValid(Controller) || not IsValid(M_Camera))
	{
		return;
	}
	// Front-low three-quarter, side and from below-behind: every angle shows both pods.
	static const FVector LocalOffsets[] = {
		FVector(1100.f, 650.f, -250.f), FVector(150.f, 1300.f, -120.f), FVector(-700.f, -900.f, -450.f)
	};
	const FVector Offset = LocalOffsets[FMath::Clamp(ShotIndex, 0, static_cast<int32>(UE_ARRAY_COUNT(LocalOffsets)) - 1)];
	const FTransform AircraftTransform = Aircraft.GetActorTransform();
	const FVector Focus = AircraftTransform.TransformPosition(FVector(80.f, 0.f, 120.f));
	const FVector CameraLocation = AircraftTransform.TransformPosition(Offset);
	M_Camera->SetActorLocationAndRotation(CameraLocation, (Focus - CameraLocation).Rotation());
	Controller->SetViewTarget(M_Camera);
}

// ---------------------------------------------------------------- Strike

void UJu87GTankBusterTestSubsystem::StartStrike()
{
	using namespace Ju87GTestPrivate;
	M_Phase = EJu87GTestPhase::Strike;
	M_PhaseStartSeconds = GetNow();
	ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	UGlobalAbilitiesManager* Manager = IsValid(Controller) ? Controller->GetGlobalAbilitiesManager() : nullptr;
	const URTSGlobalAbilitySettings* AbilitySettings = GetDefault<URTSGlobalAbilitySettings>();
	UGlobalAbilitySetDataAsset* AbilitySet = IsValid(AbilitySettings)
		                                         ? AbilitySettings->GlobalAbilityDataAsset.LoadSynchronous()
		                                         : nullptr;
	Check(IsValid(Manager) && IsValid(AbilitySet), TEXT("ability_manager_and_set"));
	if (not IsValid(Manager) || not IsValid(AbilitySet))
	{
		Finish();
		return;
	}
	// A map started directly has no commander, so the ability is built from its template as the manager does.
	M_Ability = AbilitySet->GetGLobalAbilityForTypeAsCopyFromTemplate(EGlobalAbility::GA_GerJu87GTankBusters);
	Check(IsValid(Cast<UGA_AircraftStrafing>(M_Ability)), TEXT("template_is_aircraft_strafing"),
	      IsValid(M_Ability) ? M_Ability->GetClass()->GetName() : TEXT("none"));
	if (not IsValid(M_Ability))
	{
		Finish();
		return;
	}
	const FGLobalAbilityCostState& Costs = M_Ability->GetAbilityCosts();
	bool bRadixiteOnly = not Costs.Costs.ResourceCosts.IsEmpty();
	FString CostText;
	for (const TPair<ERTSResourceType, int32>& Cost : Costs.Costs.ResourceCosts)
	{
		bRadixiteOnly &= Cost.Key == ERTSResourceType::Resource_Radixite && Cost.Value > 0;
		CostText += FString::Printf(TEXT("%s=%d "), *UEnum::GetValueAsString(Cost.Key), Cost.Value);
	}
	Check(M_Ability->GetAbilityTier() == EGlobalAbilityTier::TierIII, TEXT("tier_iii"),
	      UEnum::GetValueAsString(M_Ability->GetAbilityTier()));
	Check(bRadixiteOnly, TEXT("radixite_only_cost"), CostText);
	Check(Costs.CoolDownTime >= MinCooldownSeconds && Costs.CoolDownTime <= MaxCooldownSeconds,
	      TEXT("tier_iii_cooldown"), FString::Printf(TEXT("cooldown=%d"), Costs.CoolDownTime));

	M_Ability->InitGlobalAbility(TestPlayer, Manager, Controller);
	if (not EnsureCanAfford(*M_Ability))
	{
		Finish();
		return;
	}
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->IsA<AAircraftMaster>() || It->IsA<ATankMaster>())
		{
			M_PreExistingActors.Add(*It);
		}
	}
	SpawnTargets();
	M_Ability->OnGlobalAbilityEnded.AddUObject(this, &UJu87GTankBusterTestSubsystem::OnAbilityEnded);

	// The player's path: activate from the panel, then click the target.
	TMap<ERTSResourceType, int32> Before;
	const UPlayerResourceManager* Resources = Controller->GetPlayerResourceManager();
	for (const TPair<ERTSResourceType, int32>& Cost : Costs.Costs.ResourceCosts)
	{
		Before.Add(Cost.Key, IsValid(Resources) ? Resources->GetResourceAmount(Cost.Key) : 0);
	}
	Controller->OnGlobaAbilityActivated(M_Ability->GetAimSettings(), FGlobalAbilitySoundSettings(), M_Ability);
	Check(Controller->TryHandleActiveGlobalAbilityPrimaryClick(M_Anchor), TEXT("click_handled"));
	bool bPaid = IsValid(Resources);
	FString PaidText;
	for (const TPair<ERTSResourceType, int32>& Cost : Costs.Costs.ResourceCosts)
	{
		const int32 Paid = Before.FindRef(Cost.Key) - (IsValid(Resources) ? Resources->GetResourceAmount(Cost.Key) : 0);
		bPaid &= Paid == Cost.Value;
		PaidText += FString::Printf(TEXT("%s paid=%d "), *UEnum::GetValueAsString(Cost.Key), Paid);
	}
	Check(bPaid, TEXT("cost_paid"), PaidText);
	Check(M_Ability->GetAbilityCosts().CoolDownRemaining == Costs.CoolDownTime, TEXT("cooldown_started"),
	      FString::Printf(TEXT("remaining=%d"), M_Ability->GetAbilityCosts().CoolDownRemaining));
	RequestScreenshot(TEXT("Strike_Clicked"));
}

void UJu87GTankBusterTestSubsystem::SpawnTargets()
{
	using namespace Ju87GTestPrivate;
	UWorld* World = GetWorld();
	for (const FTargetSpawn& Spawn : TargetSpawns)
	{
		UClass* TankClass = LoadClass<AActor>(nullptr, Spawn.ClassPath);
		if (not IsValid(World) || not IsValid(TankClass))
		{
			Check(false, FString::Printf(TEXT("target_class_%s"), Spawn.Label), Spawn.ClassPath);
			continue;
		}
		FVector Ground = M_Anchor;
		GlobalAbilityHelpers::TryFindGroundLocation(this, M_Anchor + FVector(Spawn.Offset.X, Spawn.Offset.Y, 0.f),
		                                            Ground);
		AActor* Tank = FRTSUnitOwnerSpawning::SpawnUnitForPlayer(
			*World, TankClass, FTransform(Ground + FVector(0.f, 0.f, TargetSpawnAboveGroundCm)), EnemyPlayer);
		Check(IsValid(Tank), FString::Printf(TEXT("target_%s_spawned"), Spawn.Label));
		if (not IsValid(Tank))
		{
			continue;
		}
		FTargetTrack Track;
		Track.Tank = Tank;
		Track.Label = Spawn.Label;
		M_Targets.Add(Track);
		UE_LOG(LogRTSJu87GTest, Display, TEXT("RTS_JU87G_TEST TARGET %s %s at=%s"), Spawn.Label, *Tank->GetName(),
		       *Ground.ToCompactString());
	}
}

bool UJu87GTankBusterTestSubsystem::EnsureCanAfford(const UGlobalAbility& Ability)
{
	ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	UPlayerResourceManager* Resources = IsValid(Controller) ? Controller->GetPlayerResourceManager() : nullptr;
	Check(IsValid(Resources), TEXT("player_resource_manager"));
	if (not IsValid(Resources))
	{
		return false;
	}
	bool bCanAfford = true;
	for (const TPair<ERTSResourceType, int32>& Cost : Ability.GetAbilityCosts().Costs.ResourceCosts)
	{
		const int32 Missing = Cost.Value - Resources->GetResourceAmount(Cost.Key);
		if (Missing > 0)
		{
			// A bare test map has no HQ, so no storage: add both, as the cheat does.
			Resources->CheatAddStorage(Cost.Key, Missing);
		}
		bCanAfford &= Resources->GetResourceAmount(Cost.Key) >= Cost.Value;
	}
	Check(bCanAfford, TEXT("player_can_afford"));
	return bCanAfford;
}

void UJu87GTankBusterTestSubsystem::OnAbilityEnded(UGlobalAbility* Ability)
{
	++M_AbilityEndedCount;
	M_AbilityEndedSeconds = GetPhaseSeconds();
	UE_LOG(LogRTSJu87GTest, Display, TEXT("RTS_JU87G_TEST ABILITY_ENDED at=%.1fs count=%d"), M_AbilityEndedSeconds,
	       M_AbilityEndedCount);
}

void UJu87GTankBusterTestSubsystem::TickStrike()
{
	using namespace Ju87GTestPrivate;
	TrackStrikeAircraft();
	TrackTargets();
	TrackBombActors();
	UpdateStrikeCamera();
	if (M_FirstCannonShotSeconds >= 0.f && M_DiveShotsTaken < MaxDiveShots && GetPhaseSeconds() >= M_NextDiveShotSeconds)
	{
		RequestScreenshot(FString::Printf(TEXT("Strike_DiveFiring_%d"), M_DiveShotsTaken++));
		M_NextDiveShotSeconds = GetPhaseSeconds() + DiveShotIntervalSeconds;
	}
	if (M_LastPeriodicShotSeconds < 0.f || GetPhaseSeconds() - M_LastPeriodicShotSeconds >= PeriodicShotSeconds)
	{
		M_LastPeriodicShotSeconds = GetPhaseSeconds();
		RequestScreenshot(FString::Printf(TEXT("Strike_t%03.0f"), GetPhaseSeconds()));
	}
	const bool bEnded = M_AbilityEndedCount > 0 && GetPhaseSeconds() - M_AbilityEndedSeconds >= AfterAbilityEndSeconds;
	if (bEnded || GetPhaseSeconds() >= StrikeTimeoutSeconds)
	{
		FinishStrike();
	}
}

void UJu87GTankBusterTestSubsystem::TrackStrikeAircraft()
{
	using namespace Ju87GTestPrivate;
	for (TActorIterator<AAircraftMaster> It(GetWorld()); It; ++It)
	{
		AAircraftMaster* Aircraft = *It;
		if (not IsValid(Aircraft) || M_PreExistingActors.Contains(Aircraft))
		{
			continue;
		}
		FStrikeAircraftTrack* Track = M_StrikeAircraft.FindByPredicate([Aircraft](const FStrikeAircraftTrack& Candidate)
		{
			return Candidate.Aircraft.Get() == Aircraft;
		});
		if (Track == nullptr)
		{
			FStrikeAircraftTrack NewTrack;
			NewTrack.Aircraft = Aircraft;
			NewTrack.ClassName = Aircraft->GetClass()->GetName();
			M_StrikeAircraft.Add(NewTrack);
			Track = &M_StrikeAircraft.Last();
			UE_LOG(LogRTSJu87GTest, Display, TEXT("RTS_JU87G_TEST AIRCRAFT %s at=%.1fs location=%s"),
			       *Track->ClassName, GetPhaseSeconds(), *Aircraft->GetActorLocation().ToCompactString());
			if (not M_CameraFollowAircraft.IsValid())
			{
				M_CameraFollowAircraft = Aircraft;
			}
		}
		TrackWeapons(*Aircraft, *Track);
	}
}

void UJu87GTankBusterTestSubsystem::TrackWeapons(AAircraftMaster& Aircraft, FStrikeAircraftTrack& Track)
{
	using namespace Ju87GTestPrivate;
	const TArray<UWeaponState*> Weapons = Aircraft.GetAllAircraftWeapons();
	if (Track.Weapons.Num() != Weapons.Num())
	{
		// The cannons join a frame after spawn: restart the magazine baseline.
		Track.Weapons.Reset();
		Track.LastMagazines.Reset();
		for (UWeaponState* Weapon : Weapons)
		{
			Track.Weapons.Add(Weapon);
			Track.LastMagazines.Add(IsValid(Weapon) ? Weapon->GetCurrentMagCapacity() : 0);
		}
	}
	const UAircraftCannonPodsComponent* PodsComponent = Aircraft.FindComponentByClass<UAircraftCannonPodsComponent>();
	const TArray<UWeaponState*> Cannons = IsValid(PodsComponent) ? PodsComponent->GetCannons() : TArray<UWeaponState*>();
	for (int32 WeaponIndex = 0; WeaponIndex < Weapons.Num(); ++WeaponIndex)
	{
		UWeaponState* Weapon = Weapons[WeaponIndex];
		if (not IsValid(Weapon))
		{
			continue;
		}
		const int32 Magazine = Weapon->GetCurrentMagCapacity();
		const int32 Fired = Track.LastMagazines[WeaponIndex] - Magazine;
		Track.LastMagazines[WeaponIndex] = Magazine;
		if (Fired <= 0)
		{
			continue;
		}
		if (not Cannons.Contains(Weapon))
		{
			Track.OtherShots += Fired;
			continue;
		}
		for (int32 Shot = 0; Shot < Fired; ++Shot)
		{
			OnCannonShot(Aircraft, Track, *Weapon);
		}
	}
	if (GetPhaseSeconds() >= Track.NextStateLogSeconds)
	{
		Track.NextStateLogSeconds = GetPhaseSeconds() + StateLogIntervalSeconds;
		UE_LOG(LogRTSJu87GTest, Display,
		       TEXT("RTS_JU87G_TEST AIRCRAFT_STATE %s at=%.1fs dist=%.0f z=%.0f pitch=%.0f cannon_shots=%d other_shots=%d"),
		       *Aircraft.GetName(), GetPhaseSeconds(), FVector::Dist2D(Aircraft.GetActorLocation(), M_Anchor),
		       Aircraft.GetActorLocation().Z - M_Anchor.Z, Aircraft.GetActorRotation().Pitch, Track.CannonShots,
		       Track.OtherShots);
	}
}

void UJu87GTankBusterTestSubsystem::OnCannonShot(AAircraftMaster& Aircraft, FStrikeAircraftTrack& Track,
                                                 const UWeaponState& Weapon)
{
	using namespace Ju87GTestPrivate;
	++Track.CannonShots;
	const bool bApcr = Weapon.GetRawWeaponData().ShellType == EWeaponShellType::Shell_APCR;
	Track.ApcrShots += bApcr ? 1 : 0;
	const USkeletalMeshComponent* AircraftMesh = Aircraft.FindComponentByClass<USkeletalMeshComponent>();
	const FName Socket = Weapon.GetWeaponSocket();
	const FVector Muzzle = IsValid(AircraftMesh) ? AircraftMesh->GetSocketLocation(Socket) : FVector::ZeroVector;
	const FVector MuzzleLocal = Aircraft.GetActorTransform().InverseTransformPosition(Muzzle);
	UE_LOG(LogRTSJu87GTest, Display,
	       TEXT("RTS_JU87G_TEST CANNON_SHOT %s at=%.1fs socket=%s origin=%s origin_local=%s aircraft_z=%.0f pitch=%.0f "
		       "projectiles_near_muzzle=%d %s"),
	       *Aircraft.GetName(), GetPhaseSeconds(), *Socket.ToString(), *Muzzle.ToCompactString(),
	       *MuzzleLocal.ToCompactString(), Aircraft.GetActorLocation().Z - M_Anchor.Z,
	       Aircraft.GetActorRotation().Pitch, CountProjectilesNear(Muzzle, ProjectileSearchRadiusCm),
	       *DescribeShotData(Weapon));
	if (M_FirstCannonShotSeconds < 0.f)
	{
		M_FirstCannonShotSeconds = GetPhaseSeconds();
		M_CameraFollowAircraft = &Aircraft;
		M_NextDiveShotSeconds = GetPhaseSeconds();
	}
}

int32 UJu87GTankBusterTestSubsystem::CountProjectilesNear(const FVector& Location, const float Radius) const
{
	int32 Count = 0;
	for (TActorIterator<AProjectile> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && not It->IsHidden() && FVector::Dist(It->GetActorLocation(), Location) <= Radius)
		{
			++Count;
		}
	}
	return Count;
}

void UJu87GTankBusterTestSubsystem::TrackTargets()
{
	for (FTargetTrack& Target : M_Targets)
	{
		if (Target.bM_Died)
		{
			continue;
		}
		AActor* Tank = Target.Tank.Get();
		const float Health = Ju87GTestPrivate::GetHealth(Tank);
		if (Target.StartHealth <= 0.f)
		{
			Target.StartHealth = Health;
			Target.LastHealth = Health;
		}
		ATankMaster* TankMaster = Cast<ATankMaster>(Tank);
		const bool bDead = not IsValid(Tank) || Health <= 0.f || (IsValid(TankMaster) && not TankMaster->IsUnitAlive());
		if (Health < Target.LastHealth || bDead)
		{
			UE_LOG(LogRTSJu87GTest, Display, TEXT("RTS_JU87G_TEST TARGET_HIT %s at=%.1fs health=%.0f/%.0f dead=%d"),
			       *Target.Label, GetPhaseSeconds(), bDead ? 0.f : Health, Target.StartHealth, bDead);
		}
		Target.LastHealth = bDead ? 0.f : Health;
		Target.bM_Died = bDead;
	}
}

void UJu87GTankBusterTestSubsystem::TrackBombActors()
{
	int32 Count = 0;
	for (TActorIterator<ABombActor> It(GetWorld()); It; ++It)
	{
		Count += IsValid(*It) ? 1 : 0;
	}
	M_BombActorsSeen = FMath::Max(M_BombActorsSeen, Count);
}

void UJu87GTankBusterTestSubsystem::UpdateStrikeCamera()
{
	APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	if (not IsValid(Controller) || not IsValid(M_Camera))
	{
		return;
	}
	const AAircraftMaster* Aircraft = M_CameraFollowAircraft.Get();
	if (not IsValid(Aircraft))
	{
		// Overview of the target area.
		const FRotator ViewRotation(-45.f, 35.f, 0.f);
		M_Camera->SetActorLocationAndRotation(M_Anchor - ViewRotation.Vector() * 6000.f, ViewRotation);
		Controller->SetViewTarget(M_Camera);
		return;
	}
	// Behind and beside the aircraft, looking past it along its flight path: pods, muzzle flashes and the target.
	const FTransform AircraftTransform = Aircraft->GetActorTransform();
	const FVector CameraLocation = AircraftTransform.TransformPosition(FVector(-1800.f, 900.f, 350.f));
	const FVector Focus = AircraftTransform.TransformPosition(FVector(1500.f, 0.f, -200.f));
	M_Camera->SetActorLocationAndRotation(CameraLocation, (Focus - CameraLocation).Rotation());
	Controller->SetViewTarget(M_Camera);
}

void UJu87GTankBusterTestSubsystem::FinishStrike()
{
	using namespace Ju87GTestPrivate;
	RequestScreenshot(TEXT("Strike_Final"));
	int32 Ju87GCount = 0;
	int32 AircraftWithApcr = 0;
	int32 TotalCannonShots = 0;
	int32 TotalApcrShots = 0;
	int32 TotalOtherShots = 0;
	int32 AliveAircraft = 0;
	for (const FStrikeAircraftTrack& Track : M_StrikeAircraft)
	{
		Ju87GCount += Track.ClassName.Contains(Ju87GMarker) ? 1 : 0;
		AircraftWithApcr += Track.ApcrShots > 0 ? 1 : 0;
		TotalCannonShots += Track.CannonShots;
		TotalApcrShots += Track.ApcrShots;
		TotalOtherShots += Track.OtherShots;
		AliveAircraft += Track.Aircraft.IsValid() ? 1 : 0;
		UE_LOG(LogRTSJu87GTest, Display, TEXT("RTS_JU87G_TEST AIRCRAFT_SUMMARY %s cannon=%d apcr=%d other=%d"),
		       *Track.ClassName, Track.CannonShots, Track.ApcrShots, Track.OtherShots);
	}
	Check(Ju87GCount == ExpectedAircraft && M_StrikeAircraft.Num() == ExpectedAircraft, TEXT("two_ju87g"),
	      FString::Printf(TEXT("ju87g=%d aircraft=%d"), Ju87GCount, M_StrikeAircraft.Num()));
	Check(TotalCannonShots > 0, TEXT("cannons_fired"), FString::Printf(TEXT("shots=%d"), TotalCannonShots));
	Check(TotalApcrShots == TotalCannonShots, TEXT("all_cannon_shots_apcr"),
	      FString::Printf(TEXT("apcr=%d of %d"), TotalApcrShots, TotalCannonShots));
	Check(AircraftWithApcr == ExpectedAircraft, TEXT("both_aircraft_fired_apcr"),
	      FString::Printf(TEXT("aircraft=%d"), AircraftWithApcr));
	Check(M_BombActorsSeen == 0, TEXT("no_bombs_dropped"), FString::Printf(TEXT("bomb_actors=%d other_shots=%d"),
		                                                                    M_BombActorsSeen, TotalOtherShots));

	int32 DamagedT34 = 0;
	int32 Kills = 0;
	bool bHeavyDamaged = false;
	for (const FTargetTrack& Target : M_Targets)
	{
		const bool bDamaged = Target.bM_Died || Target.LastHealth < Target.StartHealth;
		DamagedT34 += Target.Label.StartsWith(TEXT("T34")) && bDamaged ? 1 : 0;
		bHeavyDamaged |= Target.Label == TEXT("KV1") && bDamaged;
		Kills += Target.bM_Died ? 1 : 0;
		UE_LOG(LogRTSJu87GTest, Display, TEXT("RTS_JU87G_TEST TARGET_SUMMARY %s health=%.0f/%.0f dead=%d"),
		       *Target.Label, Target.LastHealth, Target.StartHealth, Target.bM_Died);
	}
	Check(DamagedT34 > 0, TEXT("t34_damaged"), FString::Printf(TEXT("damaged=%d"), DamagedT34));
	Check(bHeavyDamaged, TEXT("kv1_damaged"));
	Check(Kills > 0, TEXT("tank_killed"), FString::Printf(TEXT("kills=%d of %d"), Kills, M_Targets.Num()));
	Check(M_AbilityEndedCount == 1, TEXT("ability_ended_once"), FString::Printf(TEXT("ended=%d at=%.1fs"),
		      M_AbilityEndedCount, M_AbilityEndedSeconds));
	Check(AliveAircraft == 0, TEXT("aircraft_gone_after_end"), FString::Printf(TEXT("alive=%d"), AliveAircraft));
	Finish();
}

void UJu87GTankBusterTestSubsystem::RequestScreenshot(const FString& Label)
{
	const FString FileName = FPaths::ProjectSavedDir() / TEXT("Ju87GTest") /
		FString::Printf(TEXT("%02d_%s.png"), M_ScreenshotCount++, *Label);
	FScreenshotRequest::RequestScreenshot(FileName, false, false);
}

#else

bool UJu87GTankBusterTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const { return false; }
void UJu87GTankBusterTestSubsystem::Tick(float DeltaTime) {}
TStatId UJu87GTankBusterTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UJu87GTankBusterTestSubsystem, STATGROUP_Tickables);
}

#endif
