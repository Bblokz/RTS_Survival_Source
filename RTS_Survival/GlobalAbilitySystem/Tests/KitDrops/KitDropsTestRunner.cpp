// Copyright (C) Bas Blokzijl - All rights reserved.

// Kit drops test (-KitDropsValidate): fires, one after another through the player's click path, the commander kit
// abilities no other test fires: the Mammut glider drops (E-50, E-75, Maus, JagdTiger laser, Jagdpanther flamer), the
// landed Ju 390 drops (2x Radixite Pz II, Radixite Pz III + Wespe, 3x Panzerjäger laser, 2x Pz III Flamm), the
// paradrops (Light Bringers, Feuer Sturm, Rail Troopers) and the single Bf 109 strafing. Each case checks that the
// player paid, the ability ended once and its payload arrived: the promised tank classes or squad subtypes, owned by
// the player, on the ground and free of the aircraft; the Bf 109 has to fire at an enemy T-34. A screenshot per case
// goes to Saved/KitDropsTest, a summary file next to it; log lines start with RTS_KIT_DROPS_TEST.
//
// Shipping safety: this file has no header and no UCLASS. With RTS_WITH_MAP_TESTS at zero (every plain Shipping build,
// see RTSMapTests.h) the whole translation unit is empty, so not one symbol of it reaches the shipped executable. It
// runs in DebugGame and Development builds and in the RTS_SurvivalShippingMapTests target.
//
// Options: -KitDropsOnly=<GA_A+GA_B> (enum names, '+' separated), -KitDropsAnchorX= -KitDropsAnchorY= (default: an open
// spot inside UnitTests/RTS_ToCopySmall, away from its armed pits), -KitDropsTimeScale= (default 1).
// Run script: Source/Tools/KitDrops/run_kit_drops_test.ps1.

#include "CoreMinimal.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "EngineUtils.h"
#include "LandscapeProxy.h"
#include "Tickable.h"
#include "UnrealClient.h"
#include "Camera/CameraActor.h"
#include "Engine/World.h"
#include "HAL/CriticalSection.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/GlobalAbilitySetDataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/RTSGlobalAbilitySettings.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbility.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityCostState.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/AircraftDrop/GA_AircraftDrop.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/AircraftStrafing/GA_AircraftStrafing.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GliderDrop/GA_GliderTankDrop.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/Paratroopers/GA_Paratroopers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"
#include "RTS_Survival/MasterObjects/HealthBase/HpPawnMaster.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/PlayerAimAbilitiy/PlayerAimAbilityTypes/PlayerAimAbilityTypes.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "RTS_Survival/TechTree/Technologies/Technologies.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/Aircraft/GliderDelivery/GliderTankDelivery.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "Stats/Stats.h"
#include "UObject/GCObject.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSKitDropsTest, Log, All);

namespace RTS::KitDropsTest
{
	constexpr int32 TestPlayer = 1;
	constexpr uint8 EnemyPlayer = 2;
	constexpr float StartDelaySeconds = 3.f;
	// An open, flat spot inside UnitTests/RTS_ToCopySmall, clear of the armed enemy pits near its south-west edge.
	constexpr float DefaultAnchorX = -2642.f;
	constexpr float DefaultAnchorY = -4082.f;
	constexpr float LandscapeEdgeMargin = 2500.f;
	// The cases rotate over three spots across the run direction, so a late leftover never blocks the next landing.
	constexpr int32 SpotCount = 3;
	constexpr float SpotSpacing = 4000.f;

	constexpr float GliderTimeoutSeconds = 260.f;
	constexpr float LandedDropTimeoutSeconds = 220.f;
	constexpr float ParadropTimeoutSeconds = 180.f;
	constexpr float StrikeTimeoutSeconds = 220.f;
	// After the ability ends: the glider tank drives out of the nose, paratroopers may still hang under their
	// canopies when the transport leaves, landed payloads settle.
	constexpr float GliderSettleSeconds = 10.f;
	constexpr float ParadropSettleSeconds = 20.f;
	constexpr float DefaultSettleSeconds = 5.f;
	constexpr float ScreenshotToCleanupSeconds = 1.f;

	constexpr float TankGroundToleranceCm = 400.f;
	constexpr float SoldierGroundToleranceCm = 250.f;
	constexpr float TargetSpawnAboveGroundCm = 120.f;
	const TCHAR* const StrikeTargetClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_Medium/T34/Rus_Vehicle_T3476.Rus_Vehicle_T3476_C");

	constexpr float CameraDistance = 4200.f;
	constexpr float CameraPitch = -55.f;
	constexpr float CameraYawFromRun = 140.f;
	constexpr int32 MaxErrorSamples = 3;
	constexpr int32 MaxErrorSampleLength = 240;

	enum class EKitDropKind : uint8
	{
		Glider,
		LandedDrop,
		Paradrop,
		Strike
	};

	enum class ECaseStage : uint8
	{
		WaitingForEnd,
		Settling,
		WaitingForScreenshot
	};

	struct FKitDropCase
	{
		EGlobalAbility Ability;
		EKitDropKind Kind;
		// Commanders whose kit holds the ability, for the log only.
		const TCHAR* Kits;
	};

	const FKitDropCase AllCases[] = {
		{EGlobalAbility::GA_GerE50Drop, EKitDropKind::Glider, TEXT("Field Commander, Sky-Drop Quartermaster")},
		{EGlobalAbility::GA_GerE75Drop, EKitDropKind::Glider, TEXT("Sturmkommandant")},
		{EGlobalAbility::GA_GerMausDrop, EKitDropKind::Glider, TEXT("Stahllaeufer, Festungsbaumeister")},
		{EGlobalAbility::GA_GerJagdTigerLaserDrop, EKitDropKind::Glider, TEXT("Light Bringer")},
		{EGlobalAbility::GA_GerJagdPantherFlamerDrop, EKitDropKind::Glider, TEXT("Feuersturm")},
		{EGlobalAbility::GA_GerRadixitePzIIDrop, EKitDropKind::LandedDrop, TEXT("Radixite Herald")},
		{EGlobalAbility::GA_GerRadixitePzIIIWespeDrop, EKitDropKind::LandedDrop, TEXT("Radixite Herald")},
		{EGlobalAbility::GA_GerPzJagerLaserDrop, EKitDropKind::LandedDrop, TEXT("Light Bringer")},
		{EGlobalAbility::GA_GerPzIIIFlammDrop, EKitDropKind::LandedDrop, TEXT("Feuersturm")},
		{EGlobalAbility::GA_GerLightBringersParadrop, EKitDropKind::Paradrop, TEXT("Light Bringer")},
		{EGlobalAbility::GA_GerFeuerSturmParadrop, EKitDropKind::Paradrop, TEXT("Feuersturm")},
		{EGlobalAbility::GA_GerRailTroopersParadrop, EKitDropKind::Paradrop, TEXT("Rail Engineer")},
		{EGlobalAbility::GA_Bf109Strafing, EKitDropKind::Strike, TEXT("Luftwaffe Liaison")},
	};

	struct FExpectedTank
	{
		TSoftClassPtr<ATankMaster> ClassOverride;
		ETankSubtype Subtype = ETankSubtype::Tank_None;
	};

	struct FExpectedPayload
	{
		TArray<FExpectedTank> Tanks;
		TArray<ESquadSubtype> Squads;
		int32 StrikeAircraftCount = 0;
	};

	struct FTrackedWeapon
	{
		TWeakObjectPtr<UWeaponState> Weapon;
		int32 LastMagazine = 0;
	};

	bool GetWasRequestedOnCommandLine()
	{
		return FParse::Param(FCommandLine::Get(), TEXT("KitDropsValidate"));
	}

	FString GetAbilityName(const EGlobalAbility Ability)
	{
		return StaticEnum<EGlobalAbility>()->GetNameStringByValue(static_cast<int64>(Ability));
	}

	float GetTimeoutSeconds(const EKitDropKind Kind)
	{
		switch (Kind)
		{
		case EKitDropKind::Glider:
			return GliderTimeoutSeconds;
		case EKitDropKind::LandedDrop:
			return LandedDropTimeoutSeconds;
		case EKitDropKind::Paradrop:
			return ParadropTimeoutSeconds;
		case EKitDropKind::Strike:
			return StrikeTimeoutSeconds;
		}
		return LandedDropTimeoutSeconds;
	}

	float GetSettleSeconds(const EKitDropKind Kind)
	{
		switch (Kind)
		{
		case EKitDropKind::Glider:
			return GliderSettleSeconds;
		case EKitDropKind::Paradrop:
			return ParadropSettleSeconds;
		case EKitDropKind::LandedDrop:
		case EKitDropKind::Strike:
			return DefaultSettleSeconds;
		}
		return DefaultSettleSeconds;
	}

	const UClass* GetExpectedAbilityClass(const EKitDropKind Kind)
	{
		switch (Kind)
		{
		case EKitDropKind::Glider:
			return UGA_GliderTankDrop::StaticClass();
		case EKitDropKind::LandedDrop:
			return UGA_AircraftDrop::StaticClass();
		case EKitDropKind::Paradrop:
			return UGA_Paratroopers::StaticClass();
		case EKitDropKind::Strike:
			return UGA_AircraftStrafing::StaticClass();
		}
		return UGlobalAbility::StaticClass();
	}

	// The payload settings are private UPROPERTYs of each ability class; reflection reads them without a friend.
	template <typename TSettings>
	const TSettings* FindPrivateSettings(const UGlobalAbility* Ability, const UClass* OwnerClass,
	                                     const TCHAR* PropertyName)
	{
		if (not IsValid(Ability) || not Ability->IsA(OwnerClass))
		{
			return nullptr;
		}
		const FStructProperty* Property = CastField<FStructProperty>(OwnerClass->FindPropertyByName(PropertyName));
		if (Property == nullptr || Property->Struct != TSettings::StaticStruct())
		{
			return nullptr;
		}
		return Property->ContainerPtrToValuePtr<TSettings>(Ability);
	}

	FGlobalAbilityRequirements* FindMutableRequirements(UGlobalAbility* Ability)
	{
		const FStructProperty* Property = CastField<FStructProperty>(
			UGlobalAbility::StaticClass()->FindPropertyByName(TEXT("M_AbilityRequirements")));
		if (Property == nullptr || not IsValid(Ability))
		{
			return nullptr;
		}
		return Property->ContainerPtrToValuePtr<FGlobalAbilityRequirements>(Ability);
	}

	float GetHealth(const AActor* Actor)
	{
		const AHpPawnMaster* Pawn = Cast<AHpPawnMaster>(Actor);
		const UHealthComponent* Health = IsValid(Pawn) ? Pawn->GetHealthComponent() : nullptr;
		return IsValid(Health) ? Health->GetCurrentHealth() : 0.f;
	}

	FString DescribeTank(const FExpectedTank& Tank)
	{
		return Tank.ClassOverride.IsNull()
			       ? UEnum::GetValueAsString(Tank.Subtype)
			       : Tank.ClassOverride.ToSoftObjectPath().GetAssetName();
	}

	/** @brief Counts the error lines other code logs while a case runs, so a payload that "works" while spamming
	 * errors still shows up in the report. */
	class FKitDropsErrorCounter final : public FOutputDevice
	{
	public:
		virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override
		{
			if ((Verbosity & ELogVerbosity::VerbosityMask) != ELogVerbosity::Error
				|| Category == LogRTSKitDropsTest.GetCategoryName())
			{
				return;
			}
			FScopeLock Lock(&M_Lock);
			++M_ErrorCount;
			if (M_Samples.Num() < MaxErrorSamples)
			{
				M_Samples.Add(FString(Message).Left(MaxErrorSampleLength));
			}
		}

		virtual bool CanBeUsedOnAnyThread() const override { return true; }

		void Reset()
		{
			FScopeLock Lock(&M_Lock);
			M_ErrorCount = 0;
			M_Samples.Reset();
		}

		int32 GetErrorCount(TArray<FString>& OutSamples)
		{
			FScopeLock Lock(&M_Lock);
			OutSamples = M_Samples;
			return M_ErrorCount;
		}

	private:
		FCriticalSection M_Lock;
		int32 M_ErrorCount = 0;
		TArray<FString> M_Samples;
	};

	/**
	 * @brief Created by the world delegates below when -KitDropsValidate is on the command line; runs every case in
	 * AllCases (or the -KitDropsOnly subset) on the world it was made for, then quits the game.
	 */
	class FKitDropsTestRunner final : public FTickableGameObject, public FGCObject
	{
	public:
		explicit FKitDropsTestRunner(UWorld& InWorld);
		virtual ~FKitDropsTestRunner() override;

		virtual void Tick(float DeltaTime) override;
		virtual TStatId GetStatId() const override;
		virtual bool IsTickable() const override;
		virtual bool IsTickableWhenPaused() const override { return true; }

		virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
		virtual FString GetReferencerName() const override { return TEXT("FKitDropsTestRunner"); }

		bool GetIsForWorld(const UWorld* World) const { return M_World.Get() == World; }

	private:
		void Check(bool bCondition, const FString& Description, const FString& Details = FString());
		void Note(const FString& Text);
		float GetNow() const;
		float GetCaseSeconds() const;

		void TickWaitingForStart();
		void StartGameIfWaiting() const;
		void PrepareRun();
		void GatherCases();
		bool FindAnchor(FVector& OutAnchor) const;

		void StartCase(int32 CaseIndex);
		FVector GetCaseSpot(int32 CaseIndex) const;
		UGlobalAbility* MakeAbilityCopy(EGlobalAbility AbilityType) const;
		bool BuildExpectedPayload(const FKitDropCase& Case, const UGlobalAbility& Ability);
		void SpawnStrikeTarget();
		void BypassRequirementGateIfNeeded(UGlobalAbility& Ability, UGlobalAbilitiesManager& Manager);
		bool EnsureCanAfford(const UGlobalAbility& Ability, TMap<ERTSResourceType, int32>& OutAmountsBefore);
		void ClickAbility(UGlobalAbility& Ability);
		void CheckPayment(const UGlobalAbility& Ability, const TMap<ERTSResourceType, int32>& AmountsBefore);

		void TickCase();
		void TickWaitingForEnd();
		void TickSettling();
		void TickWaitingForScreenshot();
		void TrackStrikeWeapons();
		void EvaluateCase();
		void EvaluateTanks();
		void CheckTank(const ATankMaster& Tank, const FString& Label);
		void EvaluateSquads();
		void CheckSquad(ASquadController& Squad, const FString& Label);
		void EvaluateStrike();
		void ReportCaseErrors();
		void CleanupCase();
		void FinishCaseAndContinue();
		void FinishRun();
		void WriteSummaryFile() const;

		void HandleActorSpawned(AActor* SpawnedActor);
		void HandleAbilityEnded(UGlobalAbility* EndedAbility);
		bool GetIsOnGround(const AActor& Actor, float ToleranceCm) const;
		uint8 GetOwner(const AActor& Actor) const;
		template <typename TActor>
		TArray<TActor*> GetSpawnedOfType() const;

		FBox FindLandscapeBounds() const;
		FVector KeepInsideLandscape(const FVector& Location) const;
		void ViewFrom(const FVector& Focus) const;
		void RequestScreenshot(const FString& Label);

		TWeakObjectPtr<UWorld> M_World;
		FDelegateHandle M_ActorSpawnedHandle;
		TSharedPtr<FKitDropsErrorCounter> M_ErrorCounter;
		bool bM_Started = false;
		bool bM_Finished = false;
		float M_StartSeconds = -1.f;
		FVector M_Anchor = FVector::ZeroVector;
		FBox M_LandscapeBounds = FBox(ForceInit);
		FVector M_TowardCentre = FVector(0.f, 1.f, 0.f);
		TArray<FKitDropCase> M_Cases;
		TArray<FString> M_SummaryLines;
		int32 M_PassedCheckCount = 0;
		int32 M_FailedCheckCount = 0;
		int32 M_ScreenshotCount = 0;
		TWeakObjectPtr<ACameraActor> M_Camera;

		// Every ability copy started by the run: their aircraft and load callbacks can outlive their case, and nothing
		// else holds the copies (the player's manager never loaded them).
		TArray<TObjectPtr<UGlobalAbility>> M_StartedAbilities;

		// The running case.
		int32 M_CaseIndex = INDEX_NONE;
		ECaseStage M_CaseStage = ECaseStage::WaitingForEnd;
		float M_CaseStartSeconds = 0.f;
		float M_StageStartSeconds = 0.f;
		FVector M_CaseSpot = FVector::ZeroVector;
		FVector M_CaseFocus = FVector::ZeroVector;
		TObjectPtr<UGlobalAbility> M_CaseAbility = nullptr;
		FExpectedPayload M_Expected;
		int32 M_AbilityEndedCount = 0;
		int32 M_CaseFailedChecksAtStart = 0;
		TArray<FString> M_CaseNotes;
		// Actors spawned while the case ran: its payload, aircraft and gliders, cleaned up when it ends.
		TArray<TWeakObjectPtr<AActor>> M_CaseSpawned;
		// The Bf 109's enemy: a strafing aircraft never dives on empty ground.
		TWeakObjectPtr<AActor> M_StrikeTarget;
		float M_StrikeTargetStartHealth = 0.f;
		TArray<FTrackedWeapon> M_StrikeWeapons;
		TArray<TWeakObjectPtr<AAircraftMaster>> M_StrikeAircraft;
		int32 M_StrikeShots = 0;
	};

	FKitDropsTestRunner::FKitDropsTestRunner(UWorld& InWorld)
		: M_World(&InWorld)
		, M_ErrorCounter(MakeShared<FKitDropsErrorCounter>())
	{
		M_ActorSpawnedHandle = InWorld.AddOnActorSpawnedHandler(
			FOnActorSpawned::FDelegate::CreateRaw(this, &FKitDropsTestRunner::HandleActorSpawned));
		GLog->AddOutputDevice(M_ErrorCounter.Get());
	}

	FKitDropsTestRunner::~FKitDropsTestRunner()
	{
		GLog->RemoveOutputDevice(M_ErrorCounter.Get());
		if (UWorld* World = M_World.Get())
		{
			World->RemoveOnActorSpawnedHandler(M_ActorSpawnedHandle);
		}
		for (UGlobalAbility* Ability : M_StartedAbilities)
		{
			if (IsValid(Ability))
			{
				Ability->OnGlobalAbilityEnded.RemoveAll(this);
			}
		}
	}

	TStatId FKitDropsTestRunner::GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FKitDropsTestRunner, STATGROUP_Tickables);
	}

	bool FKitDropsTestRunner::IsTickable() const
	{
		return not bM_Finished && M_World.IsValid();
	}

	void FKitDropsTestRunner::AddReferencedObjects(FReferenceCollector& Collector)
	{
		Collector.AddReferencedObjects(M_StartedAbilities);
		Collector.AddReferencedObject(M_CaseAbility);
	}

	void FKitDropsTestRunner::Tick(float DeltaTime)
	{
		const UWorld* World = M_World.Get();
		if (not IsValid(World) || not World->HasBegunPlay())
		{
			return;
		}
		if (not bM_Started)
		{
			TickWaitingForStart();
			return;
		}
		TickCase();
	}

	void FKitDropsTestRunner::Check(const bool bCondition, const FString& Description, const FString& Details)
	{
		const FString CaseName = M_Cases.IsValidIndex(M_CaseIndex) ? GetAbilityName(M_Cases[M_CaseIndex].Ability) : TEXT("run");
		if (bCondition)
		{
			++M_PassedCheckCount;
			UE_LOG(LogRTSKitDropsTest, Display, TEXT("RTS_KIT_DROPS_TEST CHECK PASS %s %s %s"), *CaseName, *Description,
			       *Details);
			return;
		}
		++M_FailedCheckCount;
		M_CaseNotes.Add(TEXT("FAIL ") + Description + TEXT(" ") + Details);
		UE_LOG(LogRTSKitDropsTest, Warning, TEXT("RTS_KIT_DROPS_TEST CHECK FAIL %s %s %s"), *CaseName, *Description,
		       *Details);
	}

	void FKitDropsTestRunner::Note(const FString& Text)
	{
		M_CaseNotes.Add(Text);
		UE_LOG(LogRTSKitDropsTest, Display, TEXT("RTS_KIT_DROPS_TEST NOTE %s"), *Text);
	}

	float FKitDropsTestRunner::GetNow() const
	{
		const UWorld* World = M_World.Get();
		return IsValid(World) ? World->GetTimeSeconds() : 0.f;
	}

	float FKitDropsTestRunner::GetCaseSeconds() const
	{
		return GetNow() - M_CaseStartSeconds;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Start
	// ---------------------------------------------------------------------------------------------------------------

	void FKitDropsTestRunner::TickWaitingForStart()
	{
		StartGameIfWaiting();
		if (M_StartSeconds < 0.f)
		{
			M_StartSeconds = GetNow();
			return;
		}
		if (GetNow() - M_StartSeconds < StartDelaySeconds)
		{
			return;
		}
		bM_Started = true;
		PrepareRun();
	}

	void FKitDropsTestRunner::StartGameIfWaiting() const
	{
		ACPPController* Controller = FRTS_Statics::GetRTSController(M_World.Get());
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
	}

	void FKitDropsTestRunner::PrepareRun()
	{
		UWorld* World = M_World.Get();
		GatherCases();
		M_LandscapeBounds = FindLandscapeBounds();
		if (not FindAnchor(M_Anchor) || M_Cases.Num() == 0)
		{
			Check(M_Cases.Num() > 0, TEXT("cases_selected"));
			Check(false, TEXT("found_ground_anchor"));
			FinishRun();
			return;
		}
		M_Anchor = KeepInsideLandscape(M_Anchor);
		const FVector ToCentre = M_LandscapeBounds.IsValid
			                         ? (M_LandscapeBounds.GetCenter() - M_Anchor).GetSafeNormal2D()
			                         : FVector(0.f, 1.f, 0.f);
		M_TowardCentre = ToCentre.IsNearlyZero() ? FVector(0.f, 1.f, 0.f) : ToCentre;
		float TimeScale = 1.f;
		if (FParse::Value(FCommandLine::Get(), TEXT("KitDropsTimeScale="), TimeScale) && TimeScale > 0.f)
		{
			UGameplayStatics::SetGlobalTimeDilation(World, TimeScale);
		}
		M_Camera = World->SpawnActor<ACameraActor>();
		UE_LOG(LogRTSKitDropsTest, Display, TEXT("RTS_KIT_DROPS_TEST START map=%s cases=%d anchor=%s to_centre=%s time_scale=%.2f"),
		       *World->GetMapName(), M_Cases.Num(), *M_Anchor.ToCompactString(), *M_TowardCentre.ToCompactString(),
		       TimeScale);
		StartCase(0);
	}

	void FKitDropsTestRunner::GatherCases()
	{
		FString OnlyList;
		TArray<FString> OnlyNames;
		if (FParse::Value(FCommandLine::Get(), TEXT("KitDropsOnly="), OnlyList))
		{
			OnlyList.ParseIntoArray(OnlyNames, TEXT("+"), true);
		}
		for (const FKitDropCase& Case : AllCases)
		{
			if (OnlyNames.Num() == 0 || OnlyNames.Contains(GetAbilityName(Case.Ability)))
			{
				M_Cases.Add(Case);
			}
		}
	}

	bool FKitDropsTestRunner::FindAnchor(FVector& OutAnchor) const
	{
		// FParse::Value stops at commas, so the anchor comes as two values.
		float AnchorX = DefaultAnchorX;
		float AnchorY = DefaultAnchorY;
		FParse::Value(FCommandLine::Get(), TEXT("KitDropsAnchorX="), AnchorX);
		FParse::Value(FCommandLine::Get(), TEXT("KitDropsAnchorY="), AnchorY);
		return GlobalAbilityHelpers::TryFindGroundLocation(M_World.Get(), FVector(AnchorX, AnchorY, 0.f), OutAnchor);
	}

	// ---------------------------------------------------------------------------------------------------------------
	// One case
	// ---------------------------------------------------------------------------------------------------------------

	void FKitDropsTestRunner::StartCase(const int32 CaseIndex)
	{
		M_CaseIndex = CaseIndex;
		const FKitDropCase& Case = M_Cases[CaseIndex];
		M_CaseStage = ECaseStage::WaitingForEnd;
		M_CaseStartSeconds = GetNow();
		M_StageStartSeconds = GetNow();
		M_CaseSpot = GetCaseSpot(CaseIndex);
		M_CaseFocus = M_CaseSpot;
		M_Expected = FExpectedPayload();
		M_AbilityEndedCount = 0;
		M_CaseFailedChecksAtStart = M_FailedCheckCount;
		M_CaseNotes.Reset();
		M_CaseSpawned.Reset();
		M_StrikeWeapons.Reset();
		M_StrikeAircraft.Reset();
		M_StrikeShots = 0;
		M_ErrorCounter->Reset();
		UE_LOG(LogRTSKitDropsTest, Display, TEXT("RTS_KIT_DROPS_TEST CASE_START %s kits=\"%s\" spot=%s"),
		       *GetAbilityName(Case.Ability), Case.Kits, *M_CaseSpot.ToCompactString());

		ACPPController* Controller = FRTS_Statics::GetRTSController(M_World.Get());
		UGlobalAbilitiesManager* Manager = IsValid(Controller) ? Controller->GetGlobalAbilitiesManager() : nullptr;
		M_CaseAbility = MakeAbilityCopy(Case.Ability);
		const bool bRightClass = IsValid(M_CaseAbility) && M_CaseAbility->IsA(GetExpectedAbilityClass(Case.Kind));
		Check(IsValid(Manager) && bRightClass, TEXT("ability_copy_has_expected_class"),
		      IsValid(M_CaseAbility) ? M_CaseAbility->GetClass()->GetName() : TEXT("none"));
		if (not IsValid(Manager) || not bRightClass || not BuildExpectedPayload(Case, *M_CaseAbility))
		{
			FinishCaseAndContinue();
			return;
		}
		M_StartedAbilities.Add(M_CaseAbility);
		M_CaseAbility->OnGlobalAbilityEnded.AddRaw(this, &FKitDropsTestRunner::HandleAbilityEnded);
		M_CaseAbility->InitGlobalAbility(TestPlayer, Manager, Controller);
		if (Case.Kind == EKitDropKind::Strike)
		{
			SpawnStrikeTarget();
		}
		BypassRequirementGateIfNeeded(*M_CaseAbility, *Manager);
		TMap<ERTSResourceType, int32> AmountsBefore;
		Check(EnsureCanAfford(*M_CaseAbility, AmountsBefore), TEXT("player_can_afford"));
		ClickAbility(*M_CaseAbility);
		CheckPayment(*M_CaseAbility, AmountsBefore);
		ViewFrom(M_CaseFocus);
	}

	FVector FKitDropsTestRunner::GetCaseSpot(const int32 CaseIndex) const
	{
		const FVector Side(-M_TowardCentre.Y, M_TowardCentre.X, 0.f);
		const int32 SpotIndex = CaseIndex % SpotCount - SpotCount / 2;
		return KeepInsideLandscape(M_Anchor + Side * (SpotIndex * SpotSpacing));
	}

	UGlobalAbility* FKitDropsTestRunner::MakeAbilityCopy(const EGlobalAbility AbilityType) const
	{
		const URTSGlobalAbilitySettings* AbilitySettings = GetDefault<URTSGlobalAbilitySettings>();
		UGlobalAbilitySetDataAsset* AbilitySet = IsValid(AbilitySettings)
			                                         ? AbilitySettings->GlobalAbilityDataAsset.LoadSynchronous()
			                                         : nullptr;
		// A map started directly has no commander, so the ability is built from its template as the manager does.
		return IsValid(AbilitySet) ? AbilitySet->GetGLobalAbilityForTypeAsCopyFromTemplate(AbilityType) : nullptr;
	}

	bool FKitDropsTestRunner::BuildExpectedPayload(const FKitDropCase& Case, const UGlobalAbility& Ability)
	{
		if (const FGliderTankDropSettings* Glider = FindPrivateSettings<FGliderTankDropSettings>(
			&Ability, UGA_GliderTankDrop::StaticClass(), TEXT("M_GliderDropSettings")))
		{
			M_Expected.Tanks.Add({Glider->TankClassOverride, Glider->TankSubtype});
		}
		if (const FAircraftDropGlobalAbilitySettings* Drop = FindPrivateSettings<FAircraftDropGlobalAbilitySettings>(
			&Ability, UGA_AircraftDrop::StaticClass(), TEXT("M_AircraftDropSettings")))
		{
			for (const FAircraftDropTankWithOffset& Tank : Drop->TanksWithOffsets)
			{
				if (Tank.TankSubtype != ETankSubtype::Tank_None)
				{
					M_Expected.Tanks.Add({Tank.TankClassOverride, Tank.TankSubtype});
				}
			}
			M_Expected.Squads.Append(Drop->SquadSubtypes);
		}
		if (const FParatroopersGlobalAbilitySettings* Para = FindPrivateSettings<FParatroopersGlobalAbilitySettings>(
			&Ability, UGA_Paratroopers::StaticClass(), TEXT("M_ParatroopersSettings")))
		{
			M_Expected.Squads.Append(Para->SquadSubtypes);
		}
		if (const FAircraftStrafingGlobalAbilitySettings* Strike = FindPrivateSettings<
			FAircraftStrafingGlobalAbilitySettings>(&Ability, UGA_AircraftStrafing::StaticClass(),
			                                        TEXT("M_AircraftStrafingSettings")))
		{
			M_Expected.StrikeAircraftCount = Strike->AircraftClasses.Num();
		}
		FString Tanks;
		for (const FExpectedTank& Tank : M_Expected.Tanks)
		{
			Tanks += DescribeTank(Tank) + TEXT(" ");
		}
		FString Squads;
		for (const ESquadSubtype Squad : M_Expected.Squads)
		{
			Squads += UEnum::GetValueAsString(Squad) + TEXT(" ");
		}
		const bool bHasPayload = M_Expected.Tanks.Num() + M_Expected.Squads.Num() + M_Expected.StrikeAircraftCount > 0;
		Check(bHasPayload, TEXT("template_has_payload"), FString::Printf(TEXT("tanks=[%s] squads=[%s] strike_aircraft=%d"),
		                                                                  *Tanks, *Squads, M_Expected.StrikeAircraftCount));
		return bHasPayload;
	}

	void FKitDropsTestRunner::SpawnStrikeTarget()
	{
		UWorld* World = M_World.Get();
		UClass* TargetClass = LoadClass<AActor>(nullptr, StrikeTargetClassPath);
		if (not IsValid(World) || not IsValid(TargetClass))
		{
			Check(false, TEXT("strike_target_class"), StrikeTargetClassPath);
			return;
		}
		AActor* Target = FRTSUnitOwnerSpawning::SpawnUnitForPlayer(
			*World, TargetClass, FTransform(M_CaseSpot + FVector(0.f, 0.f, TargetSpawnAboveGroundCm)), EnemyPlayer);
		Check(IsValid(Target), TEXT("strike_target_spawned"));
		// The spawn handler took it for the case payload; it is the target, not something the ability brought.
		M_CaseSpawned.RemoveAll([Target](const TWeakObjectPtr<AActor>& Spawned)
		{
			return Spawned.Get() == Target;
		});
		M_StrikeTarget = Target;
		M_StrikeTargetStartHealth = GetHealth(Target);
	}

	void FKitDropsTestRunner::BypassRequirementGateIfNeeded(UGlobalAbility& Ability, UGlobalAbilitiesManager& Manager)
	{
		if (Manager.QueryRequirementForAbility(&Ability))
		{
			return;
		}
		FGlobalAbilityRequirements* Requirements = FindMutableRequirements(&Ability);
		const FString Gate = Requirements != nullptr
			                     ? FString::Printf(TEXT("tech=%s unit=%s"),
			                                       *UEnum::GetValueAsString(Requirements->RequiredTechnology),
			                                       *Requirements->RequiredUnit.GetTrainingName())
			                     : TEXT("unknown");
		// A player without the requirement could not fire it; -Round5FixesValidate owns that check. This test is
		// about the payload, so it clears the gate on its own copy and says so in the report.
		Note(TEXT("REQUIREMENT_GATE_BYPASSED ") + Gate);
		if (Requirements != nullptr)
		{
			*Requirements = FGlobalAbilityRequirements();
		}
		Check(Manager.QueryRequirementForAbility(&Ability), TEXT("requirements_met_after_bypass"), Gate);
	}

	bool FKitDropsTestRunner::EnsureCanAfford(const UGlobalAbility& Ability, TMap<ERTSResourceType, int32>& OutAmountsBefore)
	{
		ACPPController* Controller = FRTS_Statics::GetRTSController(M_World.Get());
		UPlayerResourceManager* Resources = IsValid(Controller) ? Controller->GetPlayerResourceManager() : nullptr;
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
			OutAmountsBefore.Add(Cost.Key, Resources->GetResourceAmount(Cost.Key));
			bCanAfford &= Resources->GetResourceAmount(Cost.Key) >= Cost.Value;
		}
		return bCanAfford;
	}

	void FKitDropsTestRunner::ClickAbility(UGlobalAbility& Ability)
	{
		const FGlobalAbilityAreaAimSettings& AreaAim = Ability.GetAimSettings().AreaAim;
		if (AreaAim.Shape != EGlobalAbilityAimShape::Strip)
		{
			Ability.OnClickedAbilityLocation(M_CaseSpot);
			return;
		}
		// The player's two clicks: touchdown at the spot, the end of the run toward the middle of the map.
		const FVector LineEnd = KeepInsideLandscape(M_CaseSpot + M_TowardCentre * AreaAim.MaxStripLength);
		M_CaseFocus = LineEnd;
		UE_LOG(LogRTSKitDropsTest, Display, TEXT("RTS_KIT_DROPS_TEST STRIP start=%s end=%s length=%.0f"),
		       *M_CaseSpot.ToCompactString(), *LineEnd.ToCompactString(), FVector::Dist2D(M_CaseSpot, LineEnd));
		Ability.OnClickedAbilityLine(M_CaseSpot, LineEnd);
	}

	void FKitDropsTestRunner::CheckPayment(const UGlobalAbility& Ability, const TMap<ERTSResourceType, int32>& AmountsBefore)
	{
		ACPPController* Controller = FRTS_Statics::GetRTSController(M_World.Get());
		const UPlayerResourceManager* Resources = IsValid(Controller) ? Controller->GetPlayerResourceManager() : nullptr;
		if (not IsValid(Resources))
		{
			Check(false, TEXT("player_resource_manager"));
			return;
		}
		bool bPaid = true;
		FString Costs;
		for (const TPair<ERTSResourceType, int32>& Cost : Ability.GetAbilityCosts().Costs.ResourceCosts)
		{
			const int32 Before = AmountsBefore.FindRef(Cost.Key);
			const int32 After = Resources->GetResourceAmount(Cost.Key);
			bPaid &= Before - After >= Cost.Value;
			Costs += FString::Printf(TEXT("%s=%d(%d->%d) "), *UEnum::GetValueAsString(Cost.Key), Cost.Value, Before, After);
		}
		// Paying happens only in the click path once nothing blocks the ability, so this also proves it executed.
		Check(bPaid, TEXT("click_path_paid_and_executed"), Costs);
	}

	void FKitDropsTestRunner::TickCase()
	{
		if (bM_Finished || not M_Cases.IsValidIndex(M_CaseIndex))
		{
			return;
		}
		switch (M_CaseStage)
		{
		case ECaseStage::WaitingForEnd:
			TickWaitingForEnd();
			return;
		case ECaseStage::Settling:
			TickSettling();
			return;
		case ECaseStage::WaitingForScreenshot:
			TickWaitingForScreenshot();
			return;
		}
	}

	void FKitDropsTestRunner::TickWaitingForEnd()
	{
		const FKitDropCase& Case = M_Cases[M_CaseIndex];
		if (Case.Kind == EKitDropKind::Strike)
		{
			TrackStrikeWeapons();
		}
		const bool bTimedOut = GetCaseSeconds() >= GetTimeoutSeconds(Case.Kind);
		if (M_AbilityEndedCount == 0 && not bTimedOut)
		{
			return;
		}
		Check(M_AbilityEndedCount > 0, TEXT("ability_ended"),
		      FString::Printf(TEXT("after=%.1fs timeout=%.0fs"), GetCaseSeconds(), GetTimeoutSeconds(Case.Kind)));
		UE_LOG(LogRTSKitDropsTest, Display, TEXT("RTS_KIT_DROPS_TEST CASE_ABILITY_ENDED %s at=%.1fs"),
		       *GetAbilityName(Case.Ability), GetCaseSeconds());
		M_CaseStage = ECaseStage::Settling;
		M_StageStartSeconds = GetNow();
	}

	void FKitDropsTestRunner::TickSettling()
	{
		const FKitDropCase& Case = M_Cases[M_CaseIndex];
		if (GetNow() - M_StageStartSeconds < GetSettleSeconds(Case.Kind))
		{
			return;
		}
		EvaluateCase();
		ViewFrom(M_CaseFocus);
		RequestScreenshot(GetAbilityName(Case.Ability));
		M_CaseStage = ECaseStage::WaitingForScreenshot;
		M_StageStartSeconds = GetNow();
	}

	void FKitDropsTestRunner::TickWaitingForScreenshot()
	{
		if (GetNow() - M_StageStartSeconds < ScreenshotToCleanupSeconds)
		{
			return;
		}
		FinishCaseAndContinue();
	}

	void FKitDropsTestRunner::TrackStrikeWeapons()
	{
		for (AAircraftMaster* Aircraft : GetSpawnedOfType<AAircraftMaster>())
		{
			M_StrikeAircraft.AddUnique(Aircraft);
			for (UWeaponState* Weapon : Aircraft->GetAllAircraftWeapons())
			{
				if (not IsValid(Weapon))
				{
					continue;
				}
				FTrackedWeapon* Tracked = M_StrikeWeapons.FindByPredicate([Weapon](const FTrackedWeapon& Candidate)
				{
					return Candidate.Weapon.Get() == Weapon;
				});
				if (Tracked == nullptr)
				{
					M_StrikeWeapons.Add({Weapon, Weapon->GetCurrentMagCapacity()});
					continue;
				}
				// A drop in the magazine is a shot; a reload refills it without counting.
				M_StrikeShots += FMath::Max(0, Tracked->LastMagazine - Weapon->GetCurrentMagCapacity());
				Tracked->LastMagazine = Weapon->GetCurrentMagCapacity();
			}
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Checks
	// ---------------------------------------------------------------------------------------------------------------

	void FKitDropsTestRunner::EvaluateCase()
	{
		Check(M_AbilityEndedCount == 1, TEXT("ability_ended_once"), FString::Printf(TEXT("count=%d"), M_AbilityEndedCount));
		EvaluateTanks();
		EvaluateSquads();
		if (M_Cases[M_CaseIndex].Kind == EKitDropKind::Strike)
		{
			EvaluateStrike();
		}
		ReportCaseErrors();
	}

	void FKitDropsTestRunner::EvaluateTanks()
	{
		TArray<ATankMaster*> Unmatched = GetSpawnedOfType<ATankMaster>();
		for (int32 Index = 0; Index < M_Expected.Tanks.Num(); ++Index)
		{
			const FExpectedTank& Expected = M_Expected.Tanks[Index];
			const FString ExpectedPath = Expected.ClassOverride.ToSoftObjectPath().ToString();
			const int32 MatchIndex = Unmatched.IndexOfByPredicate([&Expected, &ExpectedPath](const ATankMaster* Tank)
			{
				if (not Expected.ClassOverride.IsNull())
				{
					return Tank->GetClass()->GetPathName() == ExpectedPath;
				}
				const URTSComponent* RTSComponent = Tank->FindComponentByClass<URTSComponent>();
				return IsValid(RTSComponent) && RTSComponent->GetSubtypeAsTankSubtype() == Expected.Subtype;
			});
			const FString Label = FString::Printf(TEXT("tank_%d_%s"), Index, *DescribeTank(Expected));
			Check(MatchIndex != INDEX_NONE, Label + TEXT("_arrived"));
			if (MatchIndex == INDEX_NONE)
			{
				continue;
			}
			CheckTank(*Unmatched[MatchIndex], Label);
			Unmatched.RemoveAt(MatchIndex);
		}
		FString Extra;
		for (const ATankMaster* Tank : Unmatched)
		{
			Extra += Tank->GetClass()->GetName() + TEXT(" ");
		}
		Check(Unmatched.Num() == 0, TEXT("no_unexpected_tanks"), Extra);
	}

	void FKitDropsTestRunner::CheckTank(const ATankMaster& Tank, const FString& Label)
	{
		const FVector Location = Tank.GetActorLocation();
		const AActor* AttachParent = Tank.GetAttachParentActor();
		UE_LOG(LogRTSKitDropsTest, Display, TEXT("RTS_KIT_DROPS_TEST TANK %s class=%s at=%s owner=%d health=%.0f attached=%s"),
		       *Label, *Tank.GetClass()->GetName(), *Location.ToCompactString(), GetOwner(Tank), GetHealth(&Tank),
		       IsValid(AttachParent) ? *AttachParent->GetName() : TEXT("none"));
		Check(GetOwner(Tank) == TestPlayer, Label + TEXT("_owned_by_player"), FString::Printf(TEXT("owner=%d"), GetOwner(Tank)));
		Check(not IsValid(AttachParent), Label + TEXT("_released_from_aircraft"));
		Check(GetIsOnGround(Tank, TankGroundToleranceCm), Label + TEXT("_on_ground"), Location.ToCompactString());
		Check(GetHealth(&Tank) > 0.f, Label + TEXT("_alive"));
	}

	void FKitDropsTestRunner::EvaluateSquads()
	{
		TArray<ASquadController*> Unmatched = GetSpawnedOfType<ASquadController>();
		for (int32 Index = 0; Index < M_Expected.Squads.Num(); ++Index)
		{
			const ESquadSubtype Expected = M_Expected.Squads[Index];
			const int32 MatchIndex = Unmatched.IndexOfByPredicate([Expected](const ASquadController* Squad)
			{
				const URTSComponent* RTSComponent = Squad->GetRTSComponent();
				return IsValid(RTSComponent) && RTSComponent->GetSubtypeAsSquadSubtype() == Expected;
			});
			const FString Label = FString::Printf(TEXT("squad_%d_%s"), Index, *UEnum::GetValueAsString(Expected));
			Check(MatchIndex != INDEX_NONE, Label + TEXT("_arrived"));
			if (MatchIndex == INDEX_NONE)
			{
				continue;
			}
			CheckSquad(*Unmatched[MatchIndex], Label);
			Unmatched.RemoveAt(MatchIndex);
		}
		FString Extra;
		for (const ASquadController* Squad : Unmatched)
		{
			const URTSComponent* RTSComponent = Squad->GetRTSComponent();
			Extra += IsValid(RTSComponent) ? UEnum::GetValueAsString(RTSComponent->GetSubtypeAsSquadSubtype()) + TEXT(" ") : TEXT("? ");
		}
		Check(Unmatched.Num() == 0, TEXT("no_unexpected_squads"), Extra);
	}

	void FKitDropsTestRunner::CheckSquad(ASquadController& Squad, const FString& Label)
	{
		const TArray<ASquadUnit*> Units = Squad.GetSquadUnitsChecked();
		int32 UnitsOnGround = 0;
		FString UnitLocations;
		for (const ASquadUnit* Unit : Units)
		{
			UnitsOnGround += GetIsOnGround(*Unit, SoldierGroundToleranceCm) ? 1 : 0;
			UnitLocations += Unit->GetActorLocation().ToCompactString() + TEXT(" ");
		}
		UE_LOG(LogRTSKitDropsTest, Display, TEXT("RTS_KIT_DROPS_TEST SQUAD %s owner=%d loaded=%d units=%d on_ground=%d at=%s"),
		       *Label, GetOwner(Squad), Squad.GetIsSquadFullyLoadedAndInitialized(), Units.Num(), UnitsOnGround,
		       *UnitLocations);
		Check(GetOwner(Squad) == TestPlayer, Label + TEXT("_owned_by_player"), FString::Printf(TEXT("owner=%d"), GetOwner(Squad)));
		Check(Squad.GetIsSquadFullyLoadedAndInitialized(), Label + TEXT("_fully_loaded"));
		Check(Units.Num() > 0, Label + TEXT("_has_soldiers"), FString::Printf(TEXT("units=%d"), Units.Num()));
		Check(Units.Num() > 0 && UnitsOnGround == Units.Num(), Label + TEXT("_all_soldiers_landed"),
		      FString::Printf(TEXT("%d/%d"), UnitsOnGround, Units.Num()));
	}

	void FKitDropsTestRunner::EvaluateStrike()
	{
		// Strike aircraft leave and are destroyed before the ability ends, so they are counted while they fly.
		Check(M_StrikeAircraft.Num() >= M_Expected.StrikeAircraftCount, TEXT("strike_aircraft_seen"),
		      FString::Printf(TEXT("seen=%d expected=%d weapons=%d"), M_StrikeAircraft.Num(),
		                      M_Expected.StrikeAircraftCount, M_StrikeWeapons.Num()));
		Check(M_StrikeShots > 0, TEXT("strike_aircraft_fired"), FString::Printf(TEXT("shots=%d"), M_StrikeShots));
		const float TargetHealth = M_StrikeTarget.IsValid() ? GetHealth(M_StrikeTarget.Get()) : 0.f;
		Note(FString::Printf(TEXT("STRIKE_TARGET health %.0f -> %.0f%s"), M_StrikeTargetStartHealth, TargetHealth,
		                     M_StrikeTarget.IsValid() ? TEXT("") : TEXT(" (destroyed)")));
	}

	void FKitDropsTestRunner::ReportCaseErrors()
	{
		TArray<FString> Samples;
		const int32 ErrorCount = M_ErrorCounter->GetErrorCount(Samples);
		if (ErrorCount == 0)
		{
			return;
		}
		// Reported, not failed: a bare test map logs errors that have nothing to do with the payload.
		Note(FString::Printf(TEXT("ERRORS_LOGGED %d first=\"%s\""), ErrorCount,
		                     Samples.Num() > 0 ? *Samples[0] : TEXT("")));
	}

	void FKitDropsTestRunner::CleanupCase()
	{
		// Soldiers first: a destroyed controller would otherwise leave them standing.
		for (const TWeakObjectPtr<AActor>& Spawned : M_CaseSpawned)
		{
			if (ASquadUnit* Unit = Cast<ASquadUnit>(Spawned.Get()))
			{
				Unit->Destroy();
			}
		}
		for (const TWeakObjectPtr<AActor>& Spawned : M_CaseSpawned)
		{
			const bool bIsPayloadOrCarrier = Spawned.IsValid()
				&& (Spawned->IsA<ASquadController>() || Spawned->IsA<ATankMaster>()
					|| Spawned->IsA<AGliderTankDelivery>() || Spawned->IsA<AAircraftMaster>());
			if (bIsPayloadOrCarrier)
			{
				Spawned->Destroy();
			}
		}
		if (AActor* Target = M_StrikeTarget.Get())
		{
			Target->Destroy();
		}
		M_StrikeTarget.Reset();
		M_CaseSpawned.Reset();
	}

	void FKitDropsTestRunner::FinishCaseAndContinue()
	{
		const FKitDropCase& Case = M_Cases[M_CaseIndex];
		const int32 CaseFailures = M_FailedCheckCount - M_CaseFailedChecksAtStart;
		const FString Line = FString::Printf(TEXT("%s %s kits=\"%s\" failed_checks=%d %s"),
		                                     CaseFailures == 0 ? TEXT("PASS") : TEXT("FAIL"),
		                                     *GetAbilityName(Case.Ability), Case.Kits, CaseFailures,
		                                     *FString::Join(M_CaseNotes, TEXT(" | ")));
		M_SummaryLines.Add(Line);
		UE_LOG(LogRTSKitDropsTest, Display, TEXT("RTS_KIT_DROPS_TEST CASE_RESULT %s"), *Line);
		CleanupCase();
		M_CaseAbility = nullptr;
		if (M_Cases.IsValidIndex(M_CaseIndex + 1))
		{
			StartCase(M_CaseIndex + 1);
			return;
		}
		FinishRun();
	}

	void FKitDropsTestRunner::FinishRun()
	{
		const bool bPassed = M_FailedCheckCount == 0 && M_PassedCheckCount > 0;
		UE_LOG(LogRTSKitDropsTest, Display, TEXT("RTS_KIT_DROPS_TEST RESULT %s passed=%d failed=%d cases=%d"),
		       bPassed ? TEXT("PASS") : TEXT("FAIL"), M_PassedCheckCount, M_FailedCheckCount, M_Cases.Num());
		WriteSummaryFile();
		bM_Finished = true;
		if (APlayerController* Controller = FRTS_Statics::GetRTSController(M_World.Get()))
		{
			Controller->ConsoleCommand(TEXT("quit"));
		}
	}

	void FKitDropsTestRunner::WriteSummaryFile() const
	{
		TArray<FString> Lines = M_SummaryLines;
		Lines.Insert(FString::Printf(TEXT("RESULT passed=%d failed=%d cases=%d"), M_PassedCheckCount, M_FailedCheckCount,
		                             M_Cases.Num()), 0);
		const FString FileName = FPaths::ProjectSavedDir() / TEXT("KitDropsTest") /
			FString::Printf(TEXT("result_%s.txt"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
		FFileHelper::SaveStringArrayToFile(Lines, *FileName);
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Helpers
	// ---------------------------------------------------------------------------------------------------------------

	void FKitDropsTestRunner::HandleActorSpawned(AActor* SpawnedActor)
	{
		if (not bM_Started || bM_Finished || not IsValid(SpawnedActor))
		{
			return;
		}
		const bool bIsTracked = SpawnedActor->IsA<ATankMaster>() || SpawnedActor->IsA<ASquadController>()
			|| SpawnedActor->IsA<ASquadUnit>() || SpawnedActor->IsA<AAircraftMaster>()
			|| SpawnedActor->IsA<AGliderTankDelivery>();
		if (bIsTracked)
		{
			M_CaseSpawned.Add(SpawnedActor);
		}
	}

	void FKitDropsTestRunner::HandleAbilityEnded(UGlobalAbility* EndedAbility)
	{
		if (EndedAbility == M_CaseAbility)
		{
			++M_AbilityEndedCount;
		}
	}

	bool FKitDropsTestRunner::GetIsOnGround(const AActor& Actor, const float ToleranceCm) const
	{
		const FVector Location = Actor.GetActorLocation();
		FVector Ground = Location;
		if (not GlobalAbilityHelpers::TryFindGroundLocation(M_World.Get(), Location, Ground))
		{
			return false;
		}
		return FMath::Abs(Location.Z - Ground.Z) <= ToleranceCm;
	}

	uint8 FKitDropsTestRunner::GetOwner(const AActor& Actor) const
	{
		const URTSComponent* RTSComponent = Actor.FindComponentByClass<URTSComponent>();
		return IsValid(RTSComponent) ? RTSComponent->GetOwningPlayer() : 0;
	}

	template <typename TActor>
	TArray<TActor*> FKitDropsTestRunner::GetSpawnedOfType() const
	{
		TArray<TActor*> Found;
		for (const TWeakObjectPtr<AActor>& Spawned : M_CaseSpawned)
		{
			if (TActor* Typed = Cast<TActor>(Spawned.Get()))
			{
				Found.AddUnique(Typed);
			}
		}
		return Found;
	}

	FBox FKitDropsTestRunner::FindLandscapeBounds() const
	{
		FBox Bounds(ForceInit);
		for (TActorIterator<ALandscapeProxy> It(M_World.Get()); It; ++It)
		{
			Bounds += It->GetComponentsBoundingBox(true);
		}
		return Bounds;
	}

	FVector FKitDropsTestRunner::KeepInsideLandscape(const FVector& Location) const
	{
		FVector Clamped = Location;
		if (M_LandscapeBounds.IsValid)
		{
			Clamped.X = FMath::Clamp(Location.X, M_LandscapeBounds.Min.X + LandscapeEdgeMargin,
			                         M_LandscapeBounds.Max.X - LandscapeEdgeMargin);
			Clamped.Y = FMath::Clamp(Location.Y, M_LandscapeBounds.Min.Y + LandscapeEdgeMargin,
			                         M_LandscapeBounds.Max.Y - LandscapeEdgeMargin);
		}
		FVector OnGround = Clamped;
		GlobalAbilityHelpers::TryFindGroundLocation(M_World.Get(), Clamped, OnGround);
		return OnGround;
	}

	void FKitDropsTestRunner::ViewFrom(const FVector& Focus) const
	{
		APlayerController* Controller = FRTS_Statics::GetRTSController(M_World.Get());
		ACameraActor* Camera = M_Camera.Get();
		if (not IsValid(Controller) || not IsValid(Camera))
		{
			return;
		}
		const FRotator ViewRotation(CameraPitch, M_TowardCentre.Rotation().Yaw + CameraYawFromRun, 0.f);
		Camera->SetActorLocationAndRotation(Focus - ViewRotation.Vector() * CameraDistance, ViewRotation);
		Controller->SetViewTarget(Camera);
	}

	void FKitDropsTestRunner::RequestScreenshot(const FString& Label)
	{
		const FString FileName = FPaths::ProjectSavedDir() / TEXT("KitDropsTest") /
			FString::Printf(TEXT("%02d_%s.png"), M_ScreenshotCount++, *Label);
		FScreenshotRequest::RequestScreenshot(FileName, false, false);
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Registration
	// ---------------------------------------------------------------------------------------------------------------

	TArray<TUniquePtr<FKitDropsTestRunner>> GActiveRunners;

	void HandlePostWorldInitialization(UWorld* World, const UWorld::InitializationValues)
	{
		if (not IsValid(World) || not World->IsGameWorld() || not GetWasRequestedOnCommandLine())
		{
			return;
		}
		GActiveRunners.Add(MakeUnique<FKitDropsTestRunner>(*World));
	}

	void HandleWorldCleanup(UWorld* World, bool, bool)
	{
		GActiveRunners.RemoveAll([World](const TUniquePtr<FKitDropsTestRunner>& Runner)
		{
			return not Runner.IsValid() || Runner->GetIsForWorld(World);
		});
	}

	struct FKitDropsTestRegistration
	{
		FDelegateHandle PostWorldInitializationHandle;
		FDelegateHandle WorldCleanupHandle;

		FKitDropsTestRegistration()
		{
			PostWorldInitializationHandle =
				FWorldDelegates::OnPostWorldInitialization.AddStatic(&HandlePostWorldInitialization);
			WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddStatic(&HandleWorldCleanup);
		}

		~FKitDropsTestRegistration()
		{
			FWorldDelegates::OnPostWorldInitialization.Remove(PostWorldInitializationHandle);
			FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
		}
	};

	FKitDropsTestRegistration GRegistration;
}

#endif
