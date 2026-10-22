// Copyright (C) 2020-2025 Bas Blokzijl - All rights reserved.

#include "TechBatchTwoTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "RTS_Survival/GameUI/GameDifficultyPicker/W_GameDifficultyPicker.h"
#include "RTS_Survival/GameUI/TrainingUI/TrainingOptions/TrainingOptions.h"
#include "RTS_Survival/Player/AsyncRTSAssetsSpawner/RTSAsyncSpawner.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/PlayerTechManager/PlayerTechManager.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/FieldConstructionAbilityComponent/FieldConstructionAbilityComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/ModeAbilityComponent/ModeAbilityComponent.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/ArmorCalculation.h"
#include "RTS_Survival/RTSComponents/CargoMechanic/Cargo/Cargo.h"
#include "RTS_Survival/RTSComponents/CargoMechanic/CargoSquad/CargoSquad.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/TechTree/Technologies/TechnologyEffect/TechnologyEffect.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/WeaponData/FRTSWeaponHelpers/FRTSWeaponHelpers.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSTechBatchTwoTest, Log, All);

namespace TechBatchTwoTestPrivate
{
	const TCHAR* const ValidateSwitch = TEXT("TechBatchTwoValidate");
	const FName DifficultyButtonName(TEXT("M_NormalDifficultyButton"));
	const TCHAR* const TechFolder = TEXT("/Game/RTS_Survival/Blueprints/TechTree/GerTechs/");

	// Labels of the spawned test units.
	const FString JagdPanther = TEXT("JagdPanther");
	const FString Brumbar = TEXT("Brumbar");
	const FString HalfTrack = TEXT("HalfTrack");
	const FString Flak88 = TEXT("Flak88");
	const FString Gebirgsjager = TEXT("Gebirgsjager");
	const FString Jager = TEXT("Jager");
	const FString Sniper = TEXT("Sniper");
	const FString Vultures = TEXT("Vultures");
	const FString Pioneers = TEXT("Pioneers");
	const FString Scavengers = TEXT("Scavengers");

	struct FTechUnderTest
	{
		ETechnology Technology;
		const TCHAR* EffectFolderAndName;
	};
	const FTechUnderTest TechsUnderTest[] = {
		{ETechnology::Tech_Tungsten88, TEXT("Tungsten88/TEBP_Ger_Tungsten88")},
		{ETechnology::Tech_StereoRangefinder, TEXT("StereoRangefinder/TEBP_Ger_StereoRangefinder")},
		{ETechnology::Tech_Assault_ShellHoist, TEXT("ShellHoist/TEBP_Ger_ShellHoist")},
		{ETechnology::Tech_HalfTrack_ArmoredBay, TEXT("ArmoredTroopBay/TEBP_Ger_ArmoredTroopBay")},
		{ETechnology::Tech_Flak88_GroundRole, TEXT("Flak88GroundRole/TEBP_Ger_Flak88GroundRole")},
		{ETechnology::Tech_Infantry_Kit, TEXT("InfantryKit/TEBP_Ger_InfantryKit")},
		{ETechnology::Tech_Sniper_Ghillie, TEXT("GhillieSuits/TEBP_Ger_GhillieSuits")},
		{ETechnology::Tech_Pioneer_Entrenching, TEXT("EntrenchingTools/TEBP_Ger_EntrenchingTools")},
	};

	// Expected effects (must match the TE classes and behaviour Blueprints).
	constexpr float TungstenPenetrationRatio = 1.15f;
	constexpr float RangefinderRangeRatio = 1.10f;
	constexpr float RangefinderAccuracyRatio = 1.15f;
	constexpr float ShellHoistReloadRatio = 0.70f;
	constexpr float TroopBayArmorRatio = 1.2f;
	constexpr float KitHealthRatio = 1.15f;
	constexpr float GhillieHealthRatio = 1.33f;
	constexpr float EntrenchingDurationRatio = 1.f / 1.35f;
	constexpr float FlakGroundRolePenetrationRatio = 1.2f;
	constexpr float BailOutHealthFraction = 0.5f;
	// Weapon values are rounded (accuracy is an int), so allow a few percent.
	constexpr float RatioTolerance = 0.03f;

	constexpr double HudBuildSeconds = 5.0;
	constexpr double MaxStartSeconds = 60.0;
	constexpr float MaxUnitWaitSeconds = 90.f;
	constexpr float UnitSettleSeconds = 4.f;
	constexpr float EffectSettleSeconds = 4.f;
	constexpr float MaxEffectWaitSeconds = 30.f;
	constexpr float ModeSettleSeconds = 2.f;
	constexpr float BailOutSettleSeconds = 3.f;
	constexpr float UnitSpacing = 900.f;
	constexpr float GroundTraceHalfHeight = 5000.f;
	constexpr float KillDamage = 1000000.f;
	constexpr float MainGunMinimumCalibre = 37.f;

	const UWeaponState* GetMainGun(const TArray<UWeaponState*>& Weapons)
	{
		const UWeaponState* MainGun = nullptr;
		for (const UWeaponState* Weapon : Weapons)
		{
			if (Weapon && (not MainGun || Weapon->GetRawWeaponData().WeaponCalibre > MainGun->GetRawWeaponData().WeaponCalibre))
			{
				MainGun = Weapon;
			}
		}
		return MainGun;
	}

	TArray<UWeaponState*> GetUnitWeapons(AActor* Unit)
	{
		if (ATankMaster* Tank = Cast<ATankMaster>(Unit))
		{
			return FRTSWeaponHelpers::GetWeaponsMountedOnTank(Tank);
		}
		if (ASquadController* Squad = Cast<ASquadController>(Unit))
		{
			return Squad->GetWeaponsOfSquad();
		}
		return {};
	}

	float GetArmorSum(const AActor* Unit)
	{
		const UArmorCalculation* Armor = IsValid(Unit) ? Unit->FindComponentByClass<UArmorCalculation>() : nullptr;
		if (not IsValid(Armor))
		{
			return 0.f;
		}
		const FArmorSetup Setup = Armor->GetArmorSetup();
		float Sum = 0.f;
		for (int32 PlateIndex = 0; PlateIndex < Setup.NumArmorPlates0; ++PlateIndex)
		{
			Sum += Setup.ArmorSettings0[PlateIndex].ArmorValue;
		}
		for (int32 PlateIndex = 0; PlateIndex < Setup.NumArmorPlates1; ++PlateIndex)
		{
			Sum += Setup.ArmorSettings1[PlateIndex].ArmorValue;
		}
		for (int32 PlateIndex = 0; PlateIndex < Setup.NumArmorPlates2; ++PlateIndex)
		{
			Sum += Setup.ArmorSettings2[PlateIndex].ArmorValue;
		}
		return Sum;
	}

	float GetFirstSoldierMaxHealth(ASquadController* Squad)
	{
		if (not IsValid(Squad))
		{
			return 0.f;
		}
		for (ASquadUnit* Soldier : Squad->GetSquadUnitsChecked())
		{
			if (const UHealthComponent* Health = IsValid(Soldier) ? Soldier->FindComponentByClass<UHealthComponent>() : nullptr)
			{
				return Health->GetMaxHealth();
			}
		}
		return 0.f;
	}

	float GetConstructionDuration(const ASquadController* Squad)
	{
		const UFieldConstructionAbilityComponent* Construction = IsValid(Squad)
			                                                        ? Squad->FindComponentByClass<UFieldConstructionAbilityComponent>()
			                                                        : nullptr;
		return IsValid(Construction) ? Construction->GetFullSquadConstructionDurationSeconds() : 0.f;
	}

	UModeAbilityComponent* FindGroundRoleMode(const AActor* Unit)
	{
		if (not IsValid(Unit))
		{
			return nullptr;
		}
		TArray<UModeAbilityComponent*> Modes;
		Unit->GetComponents(Modes);
		for (UModeAbilityComponent* Mode : Modes)
		{
			if (Mode->GetModeAbilityType() == EModeAbilityType::Flak88GroundRole)
			{
				return Mode;
			}
		}
		return nullptr;
	}
}

bool UTechBatchTwoTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), TechBatchTwoTestPrivate::ValidateSwitch);
}

void UTechBatchTwoTestSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// Error dialogs stall a frame each in unattended runs; log them instead.
	RTSFunctionLibrary::PushModalDialogSuppression();
	bM_IsSuppressingErrorDialogs = true;
	EnterPhase(ETechBatchTwoTestPhase::WaitingForGameStart);
	UE_LOG(LogRTSTechBatchTwoTest, Display, TEXT("RTS_TECH_BATCH2 started"));
}

void UTechBatchTwoTestSubsystem::Deinitialize()
{
	if (bM_IsSuppressingErrorDialogs)
	{
		RTSFunctionLibrary::PopModalDialogSuppression();
		bM_IsSuppressingErrorDialogs = false;
	}
	Super::Deinitialize();
}

void UTechBatchTwoTestSubsystem::Tick(float DeltaTime)
{
	if (M_Phase == ETechBatchTwoTestPhase::Inactive || M_Phase == ETechBatchTwoTestPhase::Done)
	{
		return;
	}
	if (M_Phase != ETechBatchTwoTestPhase::WaitingForGameStart)
	{
		KeepGameRunning();
	}
	switch (M_Phase)
	{
	case ETechBatchTwoTestPhase::WaitingForGameStart:
		TickWaitingForGameStart();
		break;
	case ETechBatchTwoTestPhase::WaitingForUnits:
		TickWaitingForUnits();
		break;
	case ETechBatchTwoTestPhase::WaitingForEffects:
		TickWaitingForEffects();
		break;
	case ETechBatchTwoTestPhase::FlakModeOn:
		TickFlakModeOn();
		break;
	case ETechBatchTwoTestPhase::FlakModeOff:
		TickFlakModeOff();
		break;
	case ETechBatchTwoTestPhase::BailingOut:
		TickBailingOut();
		break;
	default:
		break;
	}
}

TStatId UTechBatchTwoTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTechBatchTwoTestSubsystem, STATGROUP_Tickables);
}

void UTechBatchTwoTestSubsystem::TickWaitingForGameStart()
{
	using namespace TechBatchTwoTestPrivate;
	if (GetRealSecondsInPhase() > MaxStartSeconds)
	{
		Check(false, TEXT("game_started"), TEXT("timed out"));
		Finish();
		return;
	}
	if (GetRealSecondsInPhase() < HudBuildSeconds || not TryStartGame())
	{
		return;
	}
	RequestTestUnits();
	EnterPhase(ETechBatchTwoTestPhase::WaitingForUnits);
}

void UTechBatchTwoTestSubsystem::TickWaitingForUnits()
{
	using namespace TechBatchTwoTestPrivate;
	if (GetGameSecondsInPhase() > MaxUnitWaitSeconds)
	{
		Check(false, TEXT("units_spawned"), FString::Printf(TEXT("only %d of 10 ready"), M_Units.Num()));
		Finish();
		return;
	}
	if (not GetAreAllUnitsReady())
	{
		return;
	}
	// Squads finish loading their data and weapons a little after they report ready.
	if (M_UnitsReadyAtPhaseSeconds < 0.f)
	{
		M_UnitsReadyAtPhaseSeconds = GetGameSecondsInPhase();
	}
	if (GetGameSecondsInPhase() - M_UnitsReadyAtPhaseSeconds < UnitSettleSeconds)
	{
		return;
	}
	RecordBaselines();
	ResearchTechnologies();
	EnterPhase(ETechBatchTwoTestPhase::WaitingForEffects);
}

void UTechBatchTwoTestSubsystem::TickWaitingForEffects()
{
	using namespace TechBatchTwoTestPrivate;
	if (GetGameSecondsInPhase() < EffectSettleSeconds)
	{
		return;
	}
	CheckWeaponTechs();
	CheckArmoredTroopBay();
	CheckSquadHealthTechs();
	CheckReinforcementKeepsHealth();
	CheckEntrenchingTools();
	if (not StartFlakGroundRole())
	{
		if (not StartBailOut())
		{
			Finish();
			return;
		}
		EnterPhase(ETechBatchTwoTestPhase::BailingOut);
		return;
	}
	EnterPhase(ETechBatchTwoTestPhase::FlakModeOn);
}

void UTechBatchTwoTestSubsystem::TickFlakModeOn()
{
	if (GetGameSecondsInPhase() < TechBatchTwoTestPrivate::ModeSettleSeconds)
	{
		return;
	}
	CheckFlakGroundRole(true);
	if (UModeAbilityComponent* Mode = TechBatchTwoTestPrivate::FindGroundRoleMode(GetSquad(TechBatchTwoTestPrivate::Flak88)))
	{
		Mode->DeactivateMode();
	}
	EnterPhase(ETechBatchTwoTestPhase::FlakModeOff);
}

void UTechBatchTwoTestSubsystem::TickFlakModeOff()
{
	if (GetGameSecondsInPhase() < TechBatchTwoTestPrivate::ModeSettleSeconds)
	{
		return;
	}
	CheckFlakGroundRole(false);
	if (not StartBailOut())
	{
		Finish();
		return;
	}
	EnterPhase(ETechBatchTwoTestPhase::BailingOut);
}

void UTechBatchTwoTestSubsystem::TickBailingOut()
{
	if (GetGameSecondsInPhase() < TechBatchTwoTestPrivate::BailOutSettleSeconds)
	{
		return;
	}
	CheckBailOut();
	Finish();
}

void UTechBatchTwoTestSubsystem::EnterPhase(const ETechBatchTwoTestPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartRealSeconds = FPlatformTime::Seconds();
	M_PhaseStartGameSeconds = IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.f;
}

double UTechBatchTwoTestSubsystem::GetRealSecondsInPhase() const
{
	return FPlatformTime::Seconds() - M_PhaseStartRealSeconds;
}

float UTechBatchTwoTestSubsystem::GetGameSecondsInPhase() const
{
	return (IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.f) - M_PhaseStartGameSeconds;
}

bool UTechBatchTwoTestSubsystem::TryStartGame()
{
	UWorld* World = GetWorld();
	M_PlayerController = IsValid(World) ? Cast<ACPPController>(World->GetFirstPlayerController()) : nullptr;
	if (not M_PlayerController.IsValid())
	{
		return false;
	}
	UPlayerStartGameControl* StartGameControl = M_PlayerController->FindComponentByClass<UPlayerStartGameControl>();
	if (IsValid(StartGameControl) && StartGameControl->GetIsWaitingForPlayerToStart())
	{
		StartGameControl->PlayerStartedGame();
	}
	KeepGameRunning();
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		M_TestOrigin = It->GetActorLocation();
		break;
	}
	UE_LOG(LogRTSTechBatchTwoTest, Display, TEXT("RTS_TECH_BATCH2 game started, origin %s"),
	       *M_TestOrigin.ToCompactString());
	return true;
}

void UTechBatchTwoTestSubsystem::KeepGameRunning() const
{
	TArray<UUserWidget*> Pickers;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), Pickers, UW_GameDifficultyPicker::StaticClass(), false);
	for (UUserWidget* const Picker : Pickers)
	{
		if (not IsValid(Picker) || not IsValid(Picker->WidgetTree) || not Picker->IsInViewport())
		{
			continue;
		}
		if (UButton* const NormalButton = Cast<UButton>(
			Picker->WidgetTree->FindWidget(TechBatchTwoTestPrivate::DifficultyButtonName)))
		{
			NormalButton->OnClicked.Broadcast();
		}
	}
	if (M_PlayerController.IsValid() && M_PlayerController->GetIsGameOnPause())
	{
		M_PlayerController->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
}

void UTechBatchTwoTestSubsystem::RequestTestUnits()
{
	using namespace TechBatchTwoTestPrivate;
	RequestUnit(JagdPanther, true, static_cast<uint8>(ETankSubtype::Tank_JagdPanther), FVector(UnitSpacing, 0.f, 0.f));
	RequestUnit(Brumbar, true, static_cast<uint8>(ETankSubtype::Tank_Brumbar), FVector(UnitSpacing, UnitSpacing, 0.f));
	RequestUnit(HalfTrack, true, static_cast<uint8>(ETankSubtype::Tank_Sdkfz250),
	            FVector(UnitSpacing, -UnitSpacing, 0.f));
	RequestUnit(Flak88, false, static_cast<uint8>(ESquadSubtype::Squad_Ger_88mmFlak), FVector(2 * UnitSpacing, 0.f, 0.f));
	RequestUnit(Gebirgsjager, false, static_cast<uint8>(ESquadSubtype::Squad_Ger_Gebirgsjagerin),
	            FVector(0.f, UnitSpacing, 0.f));
	RequestUnit(Jager, false, static_cast<uint8>(ESquadSubtype::Squad_Ger_JagerTruppKar98k),
	            FVector(0.f, -UnitSpacing, 0.f));
	RequestUnit(Sniper, false, static_cast<uint8>(ESquadSubtype::Squad_Ger_SniperTeam),
	            FVector(-UnitSpacing, UnitSpacing, 0.f));
	RequestUnit(Vultures, false, static_cast<uint8>(ESquadSubtype::Squad_Ger_Vultures),
	            FVector(-UnitSpacing, -UnitSpacing, 0.f));
	RequestUnit(Pioneers, false, static_cast<uint8>(ESquadSubtype::Squad_Ger_SturmPionieren),
	            FVector(-UnitSpacing, 0.f, 0.f));
	RequestUnit(Scavengers, false, static_cast<uint8>(ESquadSubtype::Squad_Ger_Scavengers),
	            FVector(-2 * UnitSpacing, 0.f, 0.f));
}

void UTechBatchTwoTestSubsystem::RequestUnit(const FString& Label, const bool bIsTank, const uint8 Subtype,
                                             const FVector& Offset)
{
	using namespace TechBatchTwoTestPrivate;
	ARTSAsyncSpawner* Spawner = M_PlayerController.IsValid() ? M_PlayerController->GetRTSAsyncSpawner() : nullptr;
	if (not IsValid(Spawner))
	{
		Check(false, TEXT("spawner_valid"), Label);
		return;
	}
	FVector Location = M_TestOrigin + Offset;
	FHitResult GroundHit;
	const FVector TraceOffset(0.f, 0.f, GroundTraceHalfHeight);
	if (GetWorld()->LineTraceSingleByChannel(GroundHit, Location + TraceOffset, Location - TraceOffset, ECC_Visibility))
	{
		Location = GroundHit.ImpactPoint;
	}
	const FTrainingOption Option(bIsTank ? EAllUnitType::UNType_Tank : EAllUnitType::UNType_Squad, Subtype);
	TWeakObjectPtr<UTechBatchTwoTestSubsystem> WeakThis(this);
	const bool bRequested = Spawner->AsyncSpawnOptionAtLocation(
		Option, Location, this, M_Units.Num() + 1,
		[WeakThis, Label](const FTrainingOption&, AActor* SpawnedActor, const int32)
		{
			if (WeakThis.IsValid() && IsValid(SpawnedActor))
			{
				WeakThis->M_Units.Add(Label, SpawnedActor);
			}
		},
		FRotator::ZeroRotator);
	Check(bRequested, TEXT("spawn_requested_") + Label, Option.GetTrainingName());
}

bool UTechBatchTwoTestSubsystem::GetAreAllUnitsReady() const
{
	constexpr int32 ExpectedUnits = 10;
	if (M_Units.Num() < ExpectedUnits)
	{
		return false;
	}
	for (const TPair<FString, TWeakObjectPtr<AActor>>& Unit : M_Units)
	{
		const ASquadController* Squad = Cast<ASquadController>(Unit.Value.Get());
		if (Squad && not Squad->GetIsSquadFullyLoadedAndInitialized())
		{
			return false;
		}
	}
	return true;
}

ATankMaster* UTechBatchTwoTestSubsystem::GetTank(const FString& Label) const
{
	const TWeakObjectPtr<AActor>* Unit = M_Units.Find(Label);
	return Unit ? Cast<ATankMaster>(Unit->Get()) : nullptr;
}

ASquadController* UTechBatchTwoTestSubsystem::GetSquad(const FString& Label) const
{
	const TWeakObjectPtr<AActor>* Unit = M_Units.Find(Label);
	return Unit ? Cast<ASquadController>(Unit->Get()) : nullptr;
}

FTechBatchTwoBaseline UTechBatchTwoTestSubsystem::MeasureUnit(const FString& Label) const
{
	using namespace TechBatchTwoTestPrivate;
	FTechBatchTwoBaseline Baseline;
	const TWeakObjectPtr<AActor>* UnitPtr = M_Units.Find(Label);
	AActor* Unit = UnitPtr ? UnitPtr->Get() : nullptr;
	if (not IsValid(Unit))
	{
		return Baseline;
	}
	const TArray<UWeaponState*> Weapons = GetUnitWeapons(Unit);
	if (const UWeaponState* MainGun = GetMainGun(Weapons))
	{
		const FWeaponData& Data = MainGun->GetRawWeaponData();
		Baseline.MainGunPenetration = Data.ArmorPen;
		Baseline.MainGunPenetrationMaxRange = Data.ArmorPenMaxRange;
		Baseline.MainGunRange = Data.Range;
		Baseline.MainGunAccuracy = Data.Accuracy;
		Baseline.MainGunReload = Data.ReloadSpeed;
		for (const UWeaponState* Weapon : Weapons)
		{
			const FWeaponData& Other = Weapon->GetRawWeaponData();
			if (Weapon != MainGun && Other.WeaponCalibre < MainGunMinimumCalibre)
			{
				Baseline.SecondaryWeaponsSignature += Other.ArmorPen + Other.Range + Other.Accuracy + Other.ReloadSpeed;
			}
		}
	}
	Baseline.ArmorSum = GetArmorSum(Unit);
	if (ASquadController* Squad = Cast<ASquadController>(Unit))
	{
		Baseline.SoldierMaxHealth = GetFirstSoldierMaxHealth(Squad);
		Baseline.ConstructionDuration = GetConstructionDuration(Squad);
	}
	return Baseline;
}

void UTechBatchTwoTestSubsystem::RecordBaselines()
{
	for (const TPair<FString, TWeakObjectPtr<AActor>>& Unit : M_Units)
	{
		const FTechBatchTwoBaseline Baseline = MeasureUnit(Unit.Key);
		M_Baselines.Add(Unit.Key, Baseline);
		UE_LOG(LogRTSTechBatchTwoTest, Display,
		       TEXT("RTS_TECH_BATCH2 baseline %s pen=%.1f range=%.0f acc=%d reload=%.2f armor=%.1f hp=%.1f build=%.2f"),
		       *Unit.Key, Baseline.MainGunPenetration, Baseline.MainGunRange, Baseline.MainGunAccuracy,
		       Baseline.MainGunReload, Baseline.ArmorSum, Baseline.SoldierMaxHealth, Baseline.ConstructionDuration);
	}
}

void UTechBatchTwoTestSubsystem::ResearchTechnologies()
{
	using namespace TechBatchTwoTestPrivate;
	for (const FTechUnderTest& Tech : TechsUnderTest)
	{
		if (ResearchThroughTechManager(Tech.Technology))
		{
			continue;
		}
		bM_UsedDirectApplication = true;
		const FString EffectName = FPackageName::GetShortName(Tech.EffectFolderAndName);
		ApplyTechnologyDirectly(Tech.Technology,
		                        FString(TechFolder) + Tech.EffectFolderAndName + TEXT(".") + EffectName + TEXT("_C"));
	}
	UE_LOG(LogRTSTechBatchTwoTest, Display, TEXT("RTS_TECH_BATCH2 research path: %s"),
	       bM_UsedDirectApplication
		       ? TEXT("DIRECT for at least one tech (controller GetPlayerTechs map is missing entries)")
		       : TEXT("PlayerTechManager for every tech"));
}

bool UTechBatchTwoTestSubsystem::ResearchThroughTechManager(const ETechnology Technology)
{
	if (not M_PlayerController.IsValid()
		|| not M_PlayerController->GetTechnologyEffectsMap().Contains(Technology))
	{
		return false;
	}
	UPlayerTechManager* TechManager = M_PlayerController->FindComponentByClass<UPlayerTechManager>();
	if (not IsValid(TechManager))
	{
		return false;
	}
	TechManager->OnTechResearched(Technology);
	return true;
}

void UTechBatchTwoTestSubsystem::ApplyTechnologyDirectly(const ETechnology Technology, const FString& EffectClassPath)
{
	UClass* EffectClass = LoadClass<UTechnologyEffect>(nullptr, *EffectClassPath);
	Check(IsValid(EffectClass), TEXT("effect_class_loads_") + Global_GetTechAsString(Technology), EffectClassPath);
	if (not IsValid(EffectClass))
	{
		return;
	}
	UTechnologyEffect* Effect = NewObject<UTechnologyEffect>(this, EffectClass);
	M_DirectEffects.Add(Effect);
	for (const TPair<FString, TWeakObjectPtr<AActor>>& Unit : M_Units)
	{
		if (ATankMaster* Tank = Cast<ATankMaster>(Unit.Value.Get()))
		{
			const URTSComponent* RTS = Tank->GetRTSComponent();
			if (IsValid(RTS) && Effect->GetTanksToApplyTo().Contains(RTS->GetSubtypeAsTankSubtype()))
			{
				Effect->ApplyOnTank(Tank);
			}
			continue;
		}
		ASquadController* Squad = Cast<ASquadController>(Unit.Value.Get());
		const URTSComponent* RTS = IsValid(Squad) ? Squad->FindComponentByClass<URTSComponent>() : nullptr;
		if (IsValid(RTS) && Effect->GetSquadsToApplyTo().Contains(RTS->GetSubtypeAsSquadSubtype()))
		{
			Effect->ApplyOnSquad(Squad);
		}
	}
}

void UTechBatchTwoTestSubsystem::CheckWeaponTechs()
{
	using namespace TechBatchTwoTestPrivate;
	const FTechBatchTwoBaseline JagdBefore = M_Baselines.FindRef(JagdPanther);
	const FTechBatchTwoBaseline JagdAfter = MeasureUnit(JagdPanther);
	// The JagdPanther gets both Tungsten-Core 8.8 and the Stereoscopic Rangefinder.
	CheckRatio(TEXT("tungsten_penetration"), JagdBefore.MainGunPenetration, JagdAfter.MainGunPenetration,
	           TungstenPenetrationRatio);
	CheckRatio(TEXT("tungsten_penetration_max_range"), JagdBefore.MainGunPenetrationMaxRange,
	           JagdAfter.MainGunPenetrationMaxRange, TungstenPenetrationRatio);
	CheckRatio(TEXT("rangefinder_range"), JagdBefore.MainGunRange, JagdAfter.MainGunRange, RangefinderRangeRatio);
	CheckRatio(TEXT("rangefinder_accuracy"), JagdBefore.MainGunAccuracy, JagdAfter.MainGunAccuracy,
	           RangefinderAccuracyRatio);
	Check(FMath::IsNearlyEqual(JagdBefore.SecondaryWeaponsSignature, JagdAfter.SecondaryWeaponsSignature),
	      TEXT("jagdpanther_machine_guns_untouched"),
	      FString::Printf(TEXT("%.1f -> %.1f"), JagdBefore.SecondaryWeaponsSignature,
	                      JagdAfter.SecondaryWeaponsSignature));

	const FTechBatchTwoBaseline BrumbarBefore = M_Baselines.FindRef(Brumbar);
	const FTechBatchTwoBaseline BrumbarAfter = MeasureUnit(Brumbar);
	CheckRatio(TEXT("shell_hoist_reload"), BrumbarBefore.MainGunReload, BrumbarAfter.MainGunReload, ShellHoistReloadRatio);
	Check(FMath::IsNearlyEqual(BrumbarBefore.SecondaryWeaponsSignature, BrumbarAfter.SecondaryWeaponsSignature),
	      TEXT("brumbar_machine_guns_untouched"),
	      FString::Printf(TEXT("%.1f -> %.1f"), BrumbarBefore.SecondaryWeaponsSignature,
	                      BrumbarAfter.SecondaryWeaponsSignature));
}

void UTechBatchTwoTestSubsystem::CheckArmoredTroopBay()
{
	using namespace TechBatchTwoTestPrivate;
	const FTechBatchTwoBaseline Before = M_Baselines.FindRef(HalfTrack);
	const FTechBatchTwoBaseline After = MeasureUnit(HalfTrack);
	Check(Before.ArmorSum > 0.f, TEXT("troop_bay_has_armor_plates"), FString::SanitizeFloat(Before.ArmorSum));
	CheckRatio(TEXT("troop_bay_hull_armor"), Before.ArmorSum, After.ArmorSum, TroopBayArmorRatio);
	const ATankMaster* Tank = GetTank(HalfTrack);
	const UCargo* Cargo = IsValid(Tank) ? Tank->FindComponentByClass<UCargo>() : nullptr;
	Check(IsValid(Cargo) && Cargo->GetDoPassengersBailOutOnOwnerDeath(), TEXT("troop_bay_bail_out_enabled"),
	      IsValid(Cargo) ? TEXT("cargo found") : TEXT("no cargo component"));
}

void UTechBatchTwoTestSubsystem::CheckSquadHealthTechs()
{
	using namespace TechBatchTwoTestPrivate;
	const auto CheckHealth = [this](const FString& Label, const float ExpectedRatio)
	{
		CheckRatio(TEXT("health_") + Label, M_Baselines.FindRef(Label).SoldierMaxHealth,
		           GetFirstSoldierMaxHealth(GetSquad(Label)), ExpectedRatio);
	};
	CheckHealth(Jager, KitHealthRatio);
	CheckHealth(Sniper, GhillieHealthRatio);
	// Gebirgsjager are in both technologies; they stack by multiplying.
	CheckHealth(Gebirgsjager, KitHealthRatio * GhillieHealthRatio);
	CheckHealth(Vultures, 1.f);
}

void UTechBatchTwoTestSubsystem::CheckReinforcementKeepsHealth()
{
	using namespace TechBatchTwoTestPrivate;
	ASquadController* Squad = GetSquad(Gebirgsjager);
	if (not IsValid(Squad))
	{
		Check(false, TEXT("reinforcement_keeps_health"), TEXT("no squad"));
		return;
	}
	// The same re-initialisation a squad runs when it is fully reinforced: max health is set from the data again.
	Squad->OnSquadFullyReinforced();
	CheckRatio(TEXT("reinforcement_keeps_health"), M_Baselines.FindRef(Gebirgsjager).SoldierMaxHealth,
	           GetFirstSoldierMaxHealth(Squad), KitHealthRatio * GhillieHealthRatio);
}

void UTechBatchTwoTestSubsystem::CheckEntrenchingTools()
{
	using namespace TechBatchTwoTestPrivate;
	for (const FString& Label : {Pioneers, Scavengers})
	{
		const float Before = M_Baselines.FindRef(Label).ConstructionDuration;
		Check(Before > 0.f, TEXT("has_field_construction_") + Label, FString::SanitizeFloat(Before));
		CheckRatio(TEXT("entrenching_duration_") + Label, Before, GetConstructionDuration(GetSquad(Label)),
		           EntrenchingDurationRatio);
	}
}

bool UTechBatchTwoTestSubsystem::StartFlakGroundRole()
{
	using namespace TechBatchTwoTestPrivate;
	UModeAbilityComponent* Mode = FindGroundRoleMode(GetSquad(Flak88));
	Check(IsValid(Mode), TEXT("flak_ground_role_mode_added"), TEXT("mode component on the 8.8 Flak squad"));
	if (not IsValid(Mode))
	{
		return false;
	}
	Mode->ActivateMode();
	return true;
}

void UTechBatchTwoTestSubsystem::CheckFlakGroundRole(const bool bExpectActive)
{
	using namespace TechBatchTwoTestPrivate;
	const FTechBatchTwoBaseline Before = M_Baselines.FindRef(Flak88);
	const FTechBatchTwoBaseline Now = MeasureUnit(Flak88);
	CheckRatio(bExpectActive ? TEXT("flak_ground_role_penetration_on") : TEXT("flak_ground_role_penetration_off"),
	           Before.MainGunPenetration, Now.MainGunPenetration,
	           bExpectActive ? FlakGroundRolePenetrationRatio : 1.f);
	Check(FMath::IsNearlyEqual(Before.SecondaryWeaponsSignature, Now.SecondaryWeaponsSignature),
	      TEXT("flak_crew_rifles_untouched"), FString::Printf(TEXT("%.1f -> %.1f"), Before.SecondaryWeaponsSignature,
	                                                          Now.SecondaryWeaponsSignature));
}

bool UTechBatchTwoTestSubsystem::StartBailOut()
{
	using namespace TechBatchTwoTestPrivate;
	ATankMaster* Tank = GetTank(HalfTrack);
	ASquadController* Squad = GetSquad(Jager);
	UCargoSquad* CargoSquad = IsValid(Squad) ? Squad->FindComponentByClass<UCargoSquad>() : nullptr;
	const bool bEntered = IsValid(Tank) && IsValid(CargoSquad) && CargoSquad->EnterCargoImmediateInternal(Tank, true);
	Check(bEntered, TEXT("bail_out_squad_entered_half_track"), TEXT("immediate cargo entry"));
	if (not bEntered)
	{
		return false;
	}
	UGameplayStatics::ApplyDamage(Tank, KillDamage, nullptr, nullptr, nullptr);
	return true;
}

void UTechBatchTwoTestSubsystem::CheckBailOut()
{
	using namespace TechBatchTwoTestPrivate;
	ASquadController* Squad = GetSquad(Jager);
	int32 Survivors = 0;
	float MinFraction = 1.f;
	float MaxFraction = 0.f;
	if (IsValid(Squad))
	{
		for (ASquadUnit* Soldier : Squad->GetSquadUnitsChecked())
		{
			const UHealthComponent* Health = IsValid(Soldier) ? Soldier->FindComponentByClass<UHealthComponent>() : nullptr;
			if (not IsValid(Health) || Health->GetCurrentHealth() <= 0.f || Health->GetMaxHealth() <= 0.f)
			{
				continue;
			}
			++Survivors;
			const float Fraction = Health->GetCurrentHealth() / Health->GetMaxHealth();
			MinFraction = FMath::Min(MinFraction, Fraction);
			MaxFraction = FMath::Max(MaxFraction, Fraction);
		}
	}
	Check(Survivors > 0, TEXT("bail_out_passengers_survive"), FString::Printf(TEXT("%d alive"), Survivors));
	Check(Survivors > 0 && FMath::Abs(MinFraction - BailOutHealthFraction) < 0.1f
	      && FMath::Abs(MaxFraction - BailOutHealthFraction) < 0.1f,
	      TEXT("bail_out_health_halved"), FString::Printf(TEXT("fractions %.2f..%.2f"), MinFraction, MaxFraction));
}

void UTechBatchTwoTestSubsystem::Check(const bool bCondition, const FString& Name, const FString& Detail)
{
	bCondition ? ++M_PassedChecks : ++M_FailedChecks;
	UE_LOG(LogRTSTechBatchTwoTest, Display, TEXT("RTS_TECH_BATCH2 CHECK %s %s (%s)"), bCondition ? TEXT("PASS") : TEXT("FAIL"),
	       *Name, *Detail);
}

void UTechBatchTwoTestSubsystem::CheckRatio(const FString& Name, const float Before, const float After,
                                            const float ExpectedRatio)
{
	const float Ratio = Before > 0.f ? After / Before : 0.f;
	Check(Before > 0.f && FMath::Abs(Ratio - ExpectedRatio) <= TechBatchTwoTestPrivate::RatioTolerance * ExpectedRatio,
	      Name, FString::Printf(TEXT("%.3f -> %.3f, ratio %.3f expected %.3f"), Before, After, Ratio, ExpectedRatio));
}

void UTechBatchTwoTestSubsystem::Finish()
{
	M_Phase = ETechBatchTwoTestPhase::Done;
	UE_LOG(LogRTSTechBatchTwoTest, Display, TEXT("RTS_TECH_BATCH2 RESULT %s passed=%d failed=%d path=%s"),
	       M_FailedChecks == 0 && M_PassedChecks > 0 ? TEXT("PASS") : TEXT("FAIL"), M_PassedChecks, M_FailedChecks,
	       bM_UsedDirectApplication ? TEXT("direct") : TEXT("tech_manager"));
	FPlatformMisc::RequestExit(false, TEXT("TechBatchTwoValidate"));
}

#else

bool UTechBatchTwoTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UTechBatchTwoTestSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

void UTechBatchTwoTestSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UTechBatchTwoTestSubsystem::Tick(float DeltaTime)
{
}

TStatId UTechBatchTwoTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTechBatchTwoTestSubsystem, STATGROUP_Tickables);
}

#endif
