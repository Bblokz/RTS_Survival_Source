// Copyright (C) Bas Blokzijl - All rights reserved.

#include "ResourceDropTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "EngineUtils.h"
#include "UnrealClient.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/GlobalAbilitySetDataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/RTSGlobalAbilitySettings.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/GlobalAbilityHelpers.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/ResourceDrop/GA_ResourceDrop.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityTier.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/Resources/Resource.h"
#include "RTS_Survival/Resources/Harvester/Harvester.h"
#include "RTS_Survival/Resources/ResourceDropOff/ResourceDropOff.h"
#include "RTS_Survival/Resources/ResourceComponent/ResourceComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Scavenging/ScavengeObject/ScavengableObject.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Tanks/WheeledTank/BaseTruck/NomadicVehicle.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSResourceDropTest, Log, All);

namespace ResourceDropTestPrivate
{
	constexpr int32 TestPlayer = 1;
	constexpr float StartDelaySeconds = 3.f;
	constexpr float LandingTimeoutSeconds = 120.f;
	constexpr float CollectTimeoutSeconds = 420.f;
	constexpr float SettleSeconds = 10.f;
	constexpr float CameraTraceLength = 500000.f;
	// Generous storage so the cap of a map without an HQ never hides a gain.
	constexpr int32 TestStorage = 200000;
	// A landed payload may sit this far above or below the landscape under it (mesh pivots, slopes).
	constexpr float GroundTolerance = 80.f;
	// And this far from the spot the aim preview showed.
	constexpr float DropPointTolerance = 300.f;
	constexpr float CollectorSideOffset = 1500.f;
	constexpr float RefinerySideOffset = 3200.f;
	constexpr float OrderIntervalSeconds = 1.f;
	constexpr float MidCollectShotSeconds = 20.f;
	constexpr float CameraPitch = -55.f;
	constexpr float CameraYaw = 135.f;
	constexpr float CameraDistance = 5200.f;
	constexpr int32 RewardRoundMultiple = 5;
	constexpr float MinCooldown = 60.f;
	constexpr float MaxCooldown = 90.f;
	// Scavenger infantry (Ger_Inf_Scavenger*, Rail Troopers) triple scavenged rewards; the templates are tuned for them.
	constexpr float ReferenceScavengeMultiplier = 3.f;

	const TCHAR* const ScavengerSquadPath =
		TEXT("/Game/RTS_Survival/Blueprints/Squads/Ger/Barracks_Infantry/Scavengers/Squad_Ger_Scavengers.Squad_Ger_Scavengers_C");
	const TCHAR* const HarvesterPath =
		TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_Medium/PanzerIV/Harvester/Ger_Harvester_PanzerIV.Ger_Harvester_PanzerIV_C");
	const TCHAR* const RefineryPath =
		TEXT("/Game/RTS_Survival/Blueprints/GroundVehicles/BaseAndNomadic/ChildBlueprints/GerEco/OpelBlitz_Ref/Ger_Refinery_Truck.Ger_Refinery_Truck_C");

	FString FormatResources(const TMap<ERTSResourceType, int32>& Resources)
	{
		FString Text;
		for (const TPair<ERTSResourceType, int32>& Resource : Resources)
		{
			Text += FString::Printf(TEXT("%s:%d "), *Global_GetResourceTypeAsString(Resource.Key), Resource.Value);
		}
		return Text.TrimEnd();
	}

	int32 SumResources(const TMap<ERTSResourceType, int32>& Resources)
	{
		int32 Sum = 0;
		for (const TPair<ERTSResourceType, int32>& Resource : Resources)
		{
			Sum += Resource.Value;
		}
		return Sum;
	}
}

bool UResourceDropTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), TEXT("ResourceDropValidate"));
}

TStatId UResourceDropTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UResourceDropTestSubsystem, STATGROUP_Tickables);
}

void UResourceDropTestSubsystem::Tick(const float DeltaTime)
{
	const UWorld* World = GetWorld();
	if (not IsValid(World) || not World->HasBegunPlay())
	{
		return;
	}
	switch (M_Phase)
	{
	case EResourceDropTestPhase::WaitingForStart:
		TickWaitingForStart();
		return;
	case EResourceDropTestPhase::WaitingForLanding:
		TickWaitingForLanding();
		break;
	case EResourceDropTestPhase::Collecting:
		TickCollecting();
		break;
	case EResourceDropTestPhase::Settling:
		TickSettling();
		break;
	case EResourceDropTestPhase::Finished:
		return;
	}
	UpdateCamera();
}

void UResourceDropTestSubsystem::Check(const bool bCondition, const FString& Description, const FString& Details)
{
	const FString CaseName = M_Cases.IsValidIndex(M_CaseIndex) ? M_Cases[M_CaseIndex].Name : TEXT("setup");
	if (bCondition)
	{
		++M_PassedCheckCount;
		UE_LOG(LogRTSResourceDropTest, Display, TEXT("RTS_RESOURCE_DROP_TEST CHECK PASS [%s] %s %s"), *CaseName,
		       *Description, *Details);
		return;
	}
	++M_FailedCheckCount;
	UE_LOG(LogRTSResourceDropTest, Error, TEXT("RTS_RESOURCE_DROP_TEST CHECK FAIL [%s] %s %s"), *CaseName,
	       *Description, *Details);
}

float UResourceDropTestSubsystem::GetNow() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() : 0.f;
}

float UResourceDropTestSubsystem::GetPhaseSeconds() const
{
	return GetNow() - M_PhaseStartSeconds;
}

void UResourceDropTestSubsystem::SetPhase(const EResourceDropTestPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartSeconds = GetNow();
}

void UResourceDropTestSubsystem::Finish()
{
	const bool bPassed = M_FailedCheckCount == 0 && M_PassedCheckCount > 0;
	UE_LOG(LogRTSResourceDropTest, Display, TEXT("RTS_RESOURCE_DROP_TEST RESULT %s passed=%d failed=%d"),
	       bPassed ? TEXT("PASS") : TEXT("FAIL"), M_PassedCheckCount, M_FailedCheckCount);
	M_Phase = EResourceDropTestPhase::Finished;
	if (APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld()))
	{
		Controller->ConsoleCommand(TEXT("quit"));
	}
}

// ---------------------------------------------------------------------------------------------------------------
// Start

void UResourceDropTestSubsystem::TickWaitingForStart()
{
	using namespace ResourceDropTestPrivate;
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
	float RunYawDegrees = 0.f;
	FParse::Value(FCommandLine::Get(), TEXT("ResourceDropYaw="), RunYawDegrees);
	M_RunDirection = FRotator(0.f, RunYawDegrees, 0.f).Vector();
	float TimeScale = 1.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("ResourceDropTimeScale="), TimeScale) && TimeScale > 0.f)
	{
		Controller->ConsoleCommand(FString::Printf(TEXT("slomo %.2f"), TimeScale));
	}
	if (UWorld* World = GetWorld())
	{
		M_Camera = World->SpawnActor<ACameraActor>();
	}
	UE_LOG(LogRTSResourceDropTest, Display, TEXT("RTS_RESOURCE_DROP_TEST START anchor=%s yaw=%.0f timescale=%.2f"),
	       *M_Anchor.ToCompactString(), RunYawDegrees, TimeScale);
	GrantTestStorage();
	RemoveHostilesNearAnchor();
	BuildCases();
	StartNextCase();
}

bool UResourceDropTestSubsystem::FindAnchor(FVector& OutAnchor) const
{
	using namespace ResourceDropTestPrivate;
	// FParse::Value stops at commas, so the anchor comes as two values.
	float AnchorX = 0.f;
	float AnchorY = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("ResourceDropAnchorX="), AnchorX)
		&& FParse::Value(FCommandLine::Get(), TEXT("ResourceDropAnchorY="), AnchorY))
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

void UResourceDropTestSubsystem::RemoveHostilesNearAnchor() const
{
	// The economy is measured, not a fight: the small map's Soviet pits shell anything near the anchor (the refinery
	// and the scavengers died to them). -ResourceDropKeepHostiles leaves them in place.
	constexpr float HostileRadius = 30000.f;
	const bool bKeepHostiles = FParse::Param(FCommandLine::Get(), TEXT("ResourceDropKeepHostiles"));
	TArray<AActor*> Hostiles;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		const URTSComponent* RTSComponent = It->FindComponentByClass<URTSComponent>();
		const float Distance = FVector::Dist2D(It->GetActorLocation(), M_Anchor);
		if (IsValid(RTSComponent) && RTSComponent->GetOwningPlayer() != ResourceDropTestPrivate::TestPlayer
			&& Distance <= HostileRadius)
		{
			Hostiles.Add(*It);
		}
	}
	for (AActor* Hostile : Hostiles)
	{
		UE_LOG(LogRTSResourceDropTest, Display, TEXT("RTS_RESOURCE_DROP_TEST HOSTILE %s distance_m=%.0f %s"),
		       *Hostile->GetName(), FVector::Dist2D(Hostile->GetActorLocation(), M_Anchor) / 100.f,
		       bKeepHostiles ? TEXT("kept") : TEXT("removed"));
		if (not bKeepHostiles)
		{
			Hostile->Destroy();
		}
	}
}

void UResourceDropTestSubsystem::BuildCases()
{
	const TArray<FResourceDropTestCase> AllCases = {
		{EGlobalAbility::GA_GerRadixiteMetalSupplyDrop, TEXT("Supply")},
		{EGlobalAbility::GA_GerConstructionSupplyDrop, TEXT("Construction")},
		{EGlobalAbility::GA_GerRadixiteCacheDrop, TEXT("Cache")},
	};
	FString OnlyCase;
	FParse::Value(FCommandLine::Get(), TEXT("ResourceDropOnly="), OnlyCase);
	for (const FResourceDropTestCase& TestCase : AllCases)
	{
		if (OnlyCase.IsEmpty() || TestCase.Name.Equals(OnlyCase, ESearchCase::IgnoreCase))
		{
			M_Cases.Add(TestCase);
		}
	}
}

void UResourceDropTestSubsystem::GrantTestStorage() const
{
	using namespace ResourceDropTestPrivate;
	const ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	UPlayerResourceManager* ResourceManager = IsValid(Controller) ? Controller->GetPlayerResourceManager() : nullptr;
	if (not IsValid(ResourceManager))
	{
		return;
	}
	for (const ERTSResourceType Resource : {ERTSResourceType::Resource_Radixite, ERTSResourceType::Resource_Metal})
	{
		// The cheat fills the storage it adds; empty half of it so gains are never capped.
		ResourceManager->CheatAddStorage(Resource, TestStorage);
		ResourceManager->SubtractResource(Resource, TestStorage / 2);
	}
}

void UResourceDropTestSubsystem::StartNextCase()
{
	++M_CaseIndex;
	if (not M_Cases.IsValidIndex(M_CaseIndex))
	{
		Finish();
		return;
	}
	M_Measurement = FResourceDropTestMeasurement();
	M_ExpectedDropPoints.Reset();
	// Leftovers of an earlier case would block the next drop's spots and harvest positions.
	for (const TWeakObjectPtr<AActor>& Landed : M_LandedResources)
	{
		if (Landed.IsValid())
		{
			Landed->Destroy();
		}
	}
	M_LandedResources.Reset();
	M_ScavengeCompletions.Reset();
	bM_TookFallingShot = false;
	bM_TookMidCollectShot = false;
	M_FirstFallingSeconds = -1.f;
	M_NextOrderSeconds = 0.f;
	UE_LOG(LogRTSResourceDropTest, Display, TEXT("RTS_RESOURCE_DROP_TEST CASE %s"), *M_Cases[M_CaseIndex].Name);

	UGA_ResourceDrop* Ability = CreateAbility(M_Cases[M_CaseIndex].AbilityType);
	if (not IsValid(Ability))
	{
		StartNextCase();
		return;
	}
	CheckTemplate(*Ability);
	SpawnCollectors(*Ability);
	CastAbility(*Ability);
	SetPhase(EResourceDropTestPhase::WaitingForLanding);
}

UGA_ResourceDrop* UResourceDropTestSubsystem::CreateAbility(const EGlobalAbility AbilityType)
{
	ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	UGlobalAbilitiesManager* Manager = IsValid(Controller) ? Controller->GetGlobalAbilitiesManager() : nullptr;
	const URTSGlobalAbilitySettings* AbilitySettings = GetDefault<URTSGlobalAbilitySettings>();
	UGlobalAbilitySetDataAsset* AbilitySet = IsValid(AbilitySettings)
		                                         ? AbilitySettings->GlobalAbilityDataAsset.LoadSynchronous()
		                                         : nullptr;
	if (not IsValid(Manager) || not IsValid(AbilitySet))
	{
		Check(false, TEXT("ability_manager_and_set"));
		return nullptr;
	}
	// A map started directly has no commander, so the ability is built from its template as the manager does.
	UGlobalAbility* Ability = AbilitySet->GetGLobalAbilityForTypeAsCopyFromTemplate(AbilityType);
	UGA_ResourceDrop* ResourceDrop = Cast<UGA_ResourceDrop>(Ability);
	Check(IsValid(ResourceDrop), TEXT("template_is_resource_drop"),
	      IsValid(Ability) ? Ability->GetClass()->GetName() : TEXT("no template"));
	if (not IsValid(ResourceDrop))
	{
		return nullptr;
	}
	ResourceDrop->InitGlobalAbility(ResourceDropTestPrivate::TestPlayer, Manager, Controller);
	M_Abilities.Add(ResourceDrop);
	return ResourceDrop;
}

void UResourceDropTestSubsystem::CheckTemplate(const UGA_ResourceDrop& Ability)
{
	using namespace ResourceDropTestPrivate;
	const FGLobalAbilityCostState& Costs = Ability.GetAbilityCosts();
	const FGlobalAbilityUISettings& UISettings = Ability.GetUISettings();
	M_Measurement.CostTotal = SumResources(Costs.Costs.ResourceCosts);
	const TMap<ERTSResourceType, int32> Delivered = Ability.GetTotalDeliveredResources();
	const bool bIsContainerDrop = Ability.GetResourceDropSettings().PayloadKind
		== EResourceDropPayloadKind::ScavengeContainer;
	const int32 DeliveredTotal = FMath::RoundToInt(SumResources(Delivered)
		* (bIsContainerDrop ? ReferenceScavengeMultiplier : 1.f));
	UE_LOG(LogRTSResourceDropTest, Display,
	       TEXT("RTS_RESOURCE_DROP_TEST TEMPLATE title=\"%s\" tier=%s cooldown=%d cost=[%s] base_delivery=[%s] "
		       "delivered_to_reference_collector=%d ratio=%.2f"),
	       *UISettings.Title.ToString(), *UEnum::GetValueAsString(Ability.GetAbilityTier()), Costs.CoolDownTime,
	       *FormatResources(Costs.Costs.ResourceCosts), *FormatResources(Delivered), DeliveredTotal,
	       M_Measurement.CostTotal > 0 ? static_cast<float>(DeliveredTotal) / M_Measurement.CostTotal : 0.f);
	Check(Ability.GetAbilityTier() == EGlobalAbilityTier::TierI, TEXT("template_tier_I"));
	Check(Costs.CoolDownTime >= MinCooldown && Costs.CoolDownTime <= MaxCooldown, TEXT("template_cooldown_60_90"),
	      FString::Printf(TEXT("cooldown=%d"), Costs.CoolDownTime));
	Check(M_Measurement.CostTotal > 0, TEXT("template_has_cost"));
	Check(DeliveredTotal > M_Measurement.CostTotal, TEXT("template_delivers_more_than_cost"),
	      FString::Printf(TEXT("delivers=%d cost=%d"), DeliveredTotal, M_Measurement.CostTotal));
	Check(IsValid(UISettings.AbilityIcon) && not UISettings.Title.ToString().Contains(TEXT("Ability Title")),
	      TEXT("template_has_icon_and_title"));
}

void UResourceDropTestSubsystem::SpawnCollectors(const UGA_ResourceDrop& Ability)
{
	using namespace ResourceDropTestPrivate;
	const FVector Sideways(-M_RunDirection.Y, M_RunDirection.X, 0.f);
	if (Ability.GetResourceDropSettings().PayloadKind == EResourceDropPayloadKind::ScavengeContainer)
	{
		// A fresh squad per case: a squad that finished the previous case did not take new scavenge orders.
		M_Scavengers = Cast<ASquadController>(SpawnTestUnit(ScavengerSquadPath, M_Anchor + Sideways *
			                                                    CollectorSideOffset, false));
		Check(M_Scavengers.IsValid(), TEXT("scavenger_squad_spawned"));
		return;
	}
	if (not M_Refinery.IsValid())
	{
		M_Refinery = SpawnTestUnit(RefineryPath, M_Anchor + Sideways * RefinerySideOffset, true);
	}
	if (not M_Harvester.IsValid())
	{
		M_Harvester = SpawnTestUnit(HarvesterPath, M_Anchor + Sideways * CollectorSideOffset, false);
	}
	const URTSComponent* RefineryRTS = M_Refinery.IsValid()
		                                   ? M_Refinery->FindComponentByClass<URTSComponent>()
		                                   : nullptr;
	Check(M_Refinery.IsValid() && IsValid(RefineryRTS) && RefineryRTS->GetOwningPlayer() == TestPlayer,
	      TEXT("player_refinery_spawned"), M_Refinery.IsValid() ? M_Refinery->GetClass()->GetName() : TEXT("none"));
	Check(M_Harvester.IsValid(), TEXT("harvester_spawned"));
}

AActor* UResourceDropTestSubsystem::SpawnTestUnit(const TCHAR* ClassPath, const FVector& NearLocation,
                                                  const bool bStartAsBuilding) const
{
	UWorld* World = GetWorld();
	UClass* UnitClass = LoadClass<AActor>(nullptr, ClassPath);
	FVector GroundLocation;
	if (not IsValid(World) || not IsValid(UnitClass)
		|| not GlobalAbilityHelpers::TryFindGroundLocation(this, NearLocation, GroundLocation))
	{
		UE_LOG(LogRTSResourceDropTest, Error, TEXT("RTS_RESOURCE_DROP_TEST could not spawn %s"), ClassPath);
		return nullptr;
	}
	constexpr float SpawnHeight = 150.f;
	const FTransform SpawnTransform(M_RunDirection.Rotation(), GroundLocation + FVector(0.f, 0.f, SpawnHeight));
	if (not bStartAsBuilding)
	{
		return FRTSUnitOwnerSpawning::SpawnUnitForPlayer(*World, UnitClass, SpawnTransform,
		                                                 ResourceDropTestPrivate::TestPlayer);
	}
	// A refinery that is already unpacked: its drop-off is what turns harvested cargo into player resources.
	ANomadicVehicle* Nomadic = World->SpawnActorDeferred<ANomadicVehicle>(
		UnitClass, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (not IsValid(Nomadic))
	{
		return nullptr;
	}
	// Protected Blueprint setting; set through reflection like a level-placed refinery would have it.
	const FBoolProperty* StartAsBuildingProperty = FindFProperty<FBoolProperty>(
		ANomadicVehicle::StaticClass(), TEXT("bStartAsConvertedToBuilding"));
	if (StartAsBuildingProperty != nullptr)
	{
		StartAsBuildingProperty->SetPropertyValue_InContainer(Nomadic, true);
	}
	Nomadic->FinishSpawning(SpawnTransform);
	return Nomadic;
}

void UResourceDropTestSubsystem::CastAbility(UGA_ResourceDrop& Ability)
{
	const float StripLength =Ability.GetAimSettings().AreaAim.MaxStripLength;
	const FVector HalfLine = M_RunDirection * StripLength * 0.5f;
	const FVector LineStart = M_Anchor - HalfLine;
	const FVector LineEnd = M_Anchor + HalfLine;
	Ability.GetStripDropPoints(LineStart, LineEnd, M_ExpectedDropPoints);
	Check(M_ExpectedDropPoints.Num() == Ability.GetResourceDropSettings().PayloadCount, TEXT("preview_shows_every_drop"),
	      FString::Printf(TEXT("preview=%d payloads=%d strip=%.0f"), M_ExpectedDropPoints.Num(),
	                      Ability.GetResourceDropSettings().PayloadCount, StripLength));
	Ability.OnResourceLanded.AddUObject(this, &UResourceDropTestSubsystem::OnResourceLanded);
	M_Measurement.BaselineRadixite = GetPlayerResource(ERTSResourceType::Resource_Radixite);
	M_Measurement.BaselineMetal = GetPlayerResource(ERTSResourceType::Resource_Metal);
	M_LastSeenRadixite = M_Measurement.BaselineRadixite;
	M_LastSeenMetal = M_Measurement.BaselineMetal;
	M_Measurement.CastSeconds = GetNow();
	Ability.ExecuteAbilityAlongLine(LineStart, LineEnd);
	UE_LOG(LogRTSResourceDropTest, Display, TEXT("RTS_RESOURCE_DROP_TEST EXECUTED strip=%.0f baseline R=%d M=%d"),
	       StripLength, M_Measurement.BaselineRadixite, M_Measurement.BaselineMetal);
}

void UResourceDropTestSubsystem::OnResourceLanded(UGA_ResourceDrop* Ability, AActor* SpawnedResource)
{
	if (Ability != GetCurrentAbility())
	{
		return;
	}
	M_LandedResources.Add(SpawnedResource);
	UE_LOG(LogRTSResourceDropTest, Display, TEXT("RTS_RESOURCE_DROP_TEST LANDED %s at=%.1fs location=%s"),
	       IsValid(SpawnedResource) ? *SpawnedResource->GetClass()->GetName() : TEXT("none"),
	       GetNow() - M_Measurement.CastSeconds,
	       IsValid(SpawnedResource) ? *SpawnedResource->GetActorLocation().ToCompactString() : TEXT("-"));
	AScavengeableObject* Container = Cast<AScavengeableObject>(SpawnedResource);
	if (not IsValid(Container))
	{
		return;
	}
	TWeakObjectPtr<AScavengeableObject> WeakContainer(Container);
	M_ScavengeCompletions.Add(WeakContainer, 0);
	Container->OnScavenged.AddWeakLambda(this, [this, WeakContainer]()
	{
		++M_ScavengeCompletions.FindOrAdd(WeakContainer);
		UE_LOG(LogRTSResourceDropTest, Display, TEXT("RTS_RESOURCE_DROP_TEST SCAVENGED container at=%.1fs"),
		       GetNow() - M_Measurement.CastSeconds);
	});
}

// ---------------------------------------------------------------------------------------------------------------
// Landing

void UResourceDropTestSubsystem::TickWaitingForLanding()
{
	using namespace ResourceDropTestPrivate;
	if (not bM_TookFallingShot)
	{
		// Half way down: the canopy is open and the payload is clear of the aircraft.
		constexpr float FallingShotDelaySeconds = 3.f;
		const bool bIsFalling = TActorIterator<AParachutedResourcePayload>(GetWorld()) ? true : false;
		if (bIsFalling && M_FirstFallingSeconds < 0.f)
		{
			M_FirstFallingSeconds = GetNow();
		}
		if (bIsFalling && GetNow() - M_FirstFallingSeconds >= FallingShotDelaySeconds)
		{
			bM_TookFallingShot = true;
			RequestScreenshot(M_Cases[M_CaseIndex].Name + TEXT("_falling"));
		}
	}
	const bool bAllLanded = M_LandedResources.Num() >= M_ExpectedDropPoints.Num() && not M_ExpectedDropPoints.IsEmpty();
	if (not bAllLanded && GetPhaseSeconds() < LandingTimeoutSeconds)
	{
		return;
	}
	M_Measurement.LandedSeconds = GetNow() - M_Measurement.CastSeconds;
	Check(bAllLanded, TEXT("all_payloads_landed"), FString::Printf(TEXT("landed=%d expected=%d after=%.1fs"),
		      M_LandedResources.Num(), M_ExpectedDropPoints.Num(), M_Measurement.LandedSeconds));
	CheckLandedResources();
	RequestScreenshot(M_Cases[M_CaseIndex].Name + TEXT("_landed"));
	SetPhase(EResourceDropTestPhase::Collecting);
}

void UResourceDropTestSubsystem::CheckLandedResources()
{
	using namespace ResourceDropTestPrivate;
	int32 OnGroundCount = 0;
	int32 AtPreviewCount = 0;
	for (const TWeakObjectPtr<AActor>& Landed : M_LandedResources)
	{
		if (not Landed.IsValid())
		{
			continue;
		}
		const FVector Location = Landed->GetActorLocation();
		FVector Ground;
		if (GlobalAbilityHelpers::TryFindGroundLocation(this, Location, Ground)
			&& FMath::Abs(Location.Z - Ground.Z) <= GroundTolerance)
		{
			++OnGroundCount;
		}
		const bool bAtPreview = M_ExpectedDropPoints.ContainsByPredicate([&Location](const FVector& Point)
		{
			return FVector::Dist2D(Point, Location) <= DropPointTolerance;
		});
		AtPreviewCount += bAtPreview ? 1 : 0;
		if (AScavengeableObject* Container = Cast<AScavengeableObject>(Landed.Get()))
		{
			CheckLandedContainer(*Container);
		}
		else if (ACPPResourceMaster* Deposit = Cast<ACPPResourceMaster>(Landed.Get()))
		{
			CheckLandedDeposit(*Deposit);
		}
	}
	Check(OnGroundCount == M_LandedResources.Num() && OnGroundCount > 0, TEXT("payloads_on_ground"),
	      FString::Printf(TEXT("on_ground=%d of %d"), OnGroundCount, M_LandedResources.Num()));
	Check(AtPreviewCount == M_LandedResources.Num(), TEXT("payloads_at_preview_points"),
	      FString::Printf(TEXT("at_preview=%d of %d"), AtPreviewCount, M_LandedResources.Num()));
}

void UResourceDropTestSubsystem::CheckLandedContainer(const AScavengeableObject& Container)
{
	const UGA_ResourceDrop* Ability = GetCurrentAbility();
	if (not IsValid(Ability))
	{
		return;
	}
	const TMap<ERTSResourceType, int32>& Configured = Ability->GetResourceDropSettings().ContainerSettings.
	                                                           RewardsPerContainer;
	const TMap<ERTSResourceType, float> Expected = Container.GetExpectedScavengeRewards();
	bool bMatches = Expected.Num() == Configured.Num();
	for (const TPair<ERTSResourceType, int32>& Reward : Configured)
	{
		const float* ExpectedAmount = Expected.Find(Reward.Key);
		bMatches = bMatches && ExpectedAmount && FMath::IsNearlyEqual(*ExpectedAmount, Reward.Value);
	}
	Check(bMatches && Container.GetCanScavenge(), TEXT("container_has_fixed_rewards"),
	      FString::Printf(TEXT("%s scavenge_time=%.0f"), *ResourceDropTestPrivate::FormatResources(Configured),
	                      Container.GetScavengeTime()));
}

void UResourceDropTestSubsystem::CheckLandedDeposit(ACPPResourceMaster& Deposit)
{
	const UGA_ResourceDrop* Ability = GetCurrentAbility();
	const UResourceComponent* ResourceComponent = Deposit.GetResourceComponent();
	if (not IsValid(Ability) || not IsValid(ResourceComponent))
	{
		Check(false, TEXT("deposit_has_resource_component"));
		return;
	}
	const int32 Configured = Ability->GetResourceDropSettings().DepositSettings.DepositAmount;
	M_Measurement.DepositStartAmount = ResourceComponent->GetTotalAmount();
	Check(ResourceComponent->GetTotalAmount() == Configured
	      && ResourceComponent->GetResourceType() == ERTSResourceType::Resource_Radixite,
	      TEXT("deposit_holds_configured_radixite"),
	      FString::Printf(TEXT("amount=%d configured=%d type=%s max_harvesters=%d"), ResourceComponent->GetTotalAmount(),
	                      Configured, *Global_GetResourceTypeAsString(ResourceComponent->GetResourceType()),
	                      ResourceComponent->GetMaxHarvesters()));
	ICommands* HarvesterCommands = Cast<ICommands>(M_Harvester.Get());
	if (HarvesterCommands == nullptr)
	{
		Check(false, TEXT("harvester_ordered"));
		return;
	}
	const ECommandQueueError OrderError = HarvesterCommands->HarvestResource(&Deposit, true);
	Check(OrderError == ECommandQueueError::NoError, TEXT("harvester_ordered"),
	      FString::Printf(TEXT("error=%d"), static_cast<int32>(OrderError)));
}

// ---------------------------------------------------------------------------------------------------------------
// Collecting

void UResourceDropTestSubsystem::TickCollecting()
{
	using namespace ResourceDropTestPrivate;
	TrackResourceGain();
	if (not bM_TookMidCollectShot && GetPhaseSeconds() >= MidCollectShotSeconds)
	{
		bM_TookMidCollectShot = true;
		RequestScreenshot(M_Cases[M_CaseIndex].Name + TEXT("_collecting"));
	}
	const bool bTimedOut = GetPhaseSeconds() >= CollectTimeoutSeconds;
	if (GetIsCurrentCaseDeposit())
	{
		LogHarvestDiagnostics();
		if (M_Measurement.PaybackSeconds >= 0.f || bTimedOut)
		{
			M_Measurement.CollectedSeconds = GetNow() - M_Measurement.CastSeconds;
			SetPhase(EResourceDropTestPhase::Settling);
		}
		return;
	}
	if (M_Scavengers.IsValid())
	{
		M_Measurement.ScavengeMultiplier = M_Scavengers->GetScavengeRewardMultiplier();
	}
	OrderScavengersToNextContainer();
	if (GetAreAllContainersEmptied() || bTimedOut)
	{
		M_Measurement.CollectedSeconds = GetNow() - M_Measurement.CastSeconds;
		Check(not bTimedOut, TEXT("all_containers_emptied_in_time"),
		      FString::Printf(TEXT("after=%.1fs"), M_Measurement.CollectedSeconds));
		RequestScreenshot(M_Cases[M_CaseIndex].Name + TEXT("_emptied"));
		// An emptied container must refuse a second scavenge (the panel greys it out, the squad cannot start one).
		int32 OpenEmptiedContainers = 0;
		for (const TPair<TWeakObjectPtr<AScavengeableObject>, int32>& Completion : M_ScavengeCompletions)
		{
			const bool bIsStillOpen = Completion.Key.IsValid() && Completion.Key->GetCanScavenge();
			OpenEmptiedContainers += Completion.Value > 0 && bIsStillOpen ? 1 : 0;
		}
		Check(OpenEmptiedContainers == 0, TEXT("emptied_containers_closed"),
		      FString::Printf(TEXT("still_open=%d"), OpenEmptiedContainers));
		M_Measurement.GainAtEmptied = M_Measurement.GainedRadixite + M_Measurement.GainedMetal;
		SetPhase(EResourceDropTestPhase::Settling);
	}
}

void UResourceDropTestSubsystem::TrackResourceGain()
{
	const int32 Radixite = GetPlayerResource(ERTSResourceType::Resource_Radixite);
	const int32 Metal = GetPlayerResource(ERTSResourceType::Resource_Metal);
	if (Radixite == M_LastSeenRadixite && Metal == M_LastSeenMetal)
	{
		return;
	}
	const float SinceCast = GetNow() - M_Measurement.CastSeconds;
	UE_LOG(LogRTSResourceDropTest, Display, TEXT("RTS_RESOURCE_DROP_TEST GAIN at=%.1fs radixite=%+d metal=%+d"),
	       SinceCast, Radixite - M_LastSeenRadixite, Metal - M_LastSeenMetal);
	M_LastSeenRadixite = Radixite;
	M_LastSeenMetal = Metal;
	++M_Measurement.GainEvents;
	M_Measurement.GainedRadixite = Radixite - M_Measurement.BaselineRadixite;
	M_Measurement.GainedMetal = Metal - M_Measurement.BaselineMetal;
	if (M_Measurement.FirstGainSeconds < 0.f)
	{
		M_Measurement.FirstGainSeconds = SinceCast;
	}
	if (M_Measurement.PaybackSeconds < 0.f
		&& M_Measurement.GainedRadixite + M_Measurement.GainedMetal >= M_Measurement.CostTotal)
	{
		M_Measurement.PaybackSeconds = SinceCast;
	}
}

void UResourceDropTestSubsystem::OrderScavengersToNextContainer()
{
	using namespace ResourceDropTestPrivate;
	ASquadController* Squad = M_Scavengers.Get();
	if (not IsValid(Squad) || not Squad->GetIsSquadFullyLoaded() || GetNow() < M_NextOrderSeconds)
	{
		return;
	}
	M_NextOrderSeconds = GetNow() + OrderIntervalSeconds;
	// Only an idle squad gets a new container: a new order would pause the scavenge in progress.
	if (not Squad->GetIsUnitIdle())
	{
		return;
	}
	AScavengeableObject* Nearest = nullptr;
	float NearestDistance = TNumericLimits<float>::Max();
	for (const TPair<TWeakObjectPtr<AScavengeableObject>, int32>& Completion : M_ScavengeCompletions)
	{
		AScavengeableObject* Container = Completion.Key.Get();
		if (not IsValid(Container) || not Container->GetCanScavenge() || Completion.Value > 0)
		{
			continue;
		}
		const float Distance = FVector::Dist2D(Container->GetActorLocation(), Squad->GetActorLocation());
		if (Distance < NearestDistance)
		{
			NearestDistance = Distance;
			Nearest = Container;
		}
	}
	if (IsValid(Nearest))
	{
		(void)Cast<ICommands>(Squad)->ScavengeObject(Nearest, true);
	}
}

bool UResourceDropTestSubsystem::GetAreAllContainersEmptied() const
{
	if (M_ScavengeCompletions.IsEmpty())
	{
		return true;
	}
	for (const TPair<TWeakObjectPtr<AScavengeableObject>, int32>& Completion : M_ScavengeCompletions)
	{
		if (Completion.Value == 0)
		{
			return false;
		}
	}
	return true;
}

void UResourceDropTestSubsystem::LogHarvestDiagnostics()
{
	constexpr float DiagnosticIntervalSeconds = 15.f;
	if (GetNow() < M_NextDiagnosticSeconds)
	{
		return;
	}
	M_NextDiagnosticSeconds = GetNow() + DiagnosticIntervalSeconds;
	const UResourceDropOff* DropOff = M_Refinery.IsValid() ? M_Refinery->FindComponentByClass<UResourceDropOff>() : nullptr;
	const UHarvester* Harvester = M_Harvester.IsValid() ? M_Harvester->FindComponentByClass<UHarvester>() : nullptr;
	int32 Cargo = 0;
	ERTSResourceType CargoType = ERTSResourceType::Resource_None;
	const int32 CargoCapacity = IsValid(Harvester) ? Harvester->GetMaxCapacityForTargetResource(Cargo, CargoType) : 0;
	ICommands* HarvesterCommands = Cast<ICommands>(M_Harvester.Get());
	UE_LOG(LogRTSResourceDropTest, Display,
	       TEXT("RTS_RESOURCE_DROP_TEST HARVEST at=%.0fs dropoff_active=%d capacity=%d stored=%d cargo=%d/%d "
		       "command=%s harvester_to_refinery_m=%.0f"),
	       GetNow() - M_Measurement.CastSeconds, IsValid(DropOff) ? DropOff->GetIsDropOffActive() : -1,
	       IsValid(DropOff) ? DropOff->GetDropOffCapacity(ERTSResourceType::Resource_Radixite) : -1,
	       IsValid(DropOff) ? DropOff->GetDropOffAmountStored(ERTSResourceType::Resource_Radixite) : -1, Cargo,
	       CargoCapacity, HarvesterCommands ? *UEnum::GetValueAsString(HarvesterCommands->GetActiveCommandID()) : TEXT("-"),
	       M_Harvester.IsValid() && M_Refinery.IsValid()
		       ? FVector::Dist2D(M_Harvester->GetActorLocation(), M_Refinery->GetActorLocation()) / 100.f
		       : -1.f);
}

// ---------------------------------------------------------------------------------------------------------------
// Settling and results

void UResourceDropTestSubsystem::TickSettling()
{
	TrackResourceGain();
	if (GetPhaseSeconds() < ResourceDropTestPrivate::SettleSeconds)
	{
		return;
	}
	if (GetIsCurrentCaseDeposit())
	{
		FinishDepositCase();
	}
	else
	{
		FinishContainerCase();
	}
	LogMeasurement();
	RequestScreenshot(M_Cases[M_CaseIndex].Name + TEXT("_final"));
	StartNextCase();
}

void UResourceDropTestSubsystem::FinishContainerCase()
{
	using namespace ResourceDropTestPrivate;
	const UGA_ResourceDrop* Ability = GetCurrentAbility();
	if (not IsValid(Ability))
	{
		return;
	}
	const float Multiplier = M_Measurement.ScavengeMultiplier;
	const int32 ContainerCount = M_ScavengeCompletions.Num();
	TMap<ERTSResourceType, int32> ExpectedGain;
	for (const TPair<ERTSResourceType, int32>& Reward : Ability->GetResourceDropSettings().ContainerSettings.
	                                                             RewardsPerContainer)
	{
		const int32 PerContainer = FMath::RoundToInt(Reward.Value * Multiplier / RewardRoundMultiple)
			* RewardRoundMultiple;
		ExpectedGain.Add(Reward.Key, PerContainer * ContainerCount);
	}
	const int32 ExpectedRadixite = ExpectedGain.FindRef(ERTSResourceType::Resource_Radixite);
	const int32 ExpectedMetal = ExpectedGain.FindRef(ERTSResourceType::Resource_Metal);
	Check(M_Measurement.GainedRadixite == ExpectedRadixite && M_Measurement.GainedMetal == ExpectedMetal,
	      TEXT("gain_equals_containers_times_reward"),
	      FString::Printf(TEXT("gained R=%d M=%d expected R=%d M=%d containers=%d multiplier=%.2f"),
	                      M_Measurement.GainedRadixite, M_Measurement.GainedMetal, ExpectedRadixite, ExpectedMetal,
	                      ContainerCount, Multiplier));
	int32 SingleCompletions = 0;
	int32 RemovedContainers = 0;
	for (const TPair<TWeakObjectPtr<AScavengeableObject>, int32>& Completion : M_ScavengeCompletions)
	{
		SingleCompletions += Completion.Value == 1 ? 1 : 0;
		RemovedContainers += Completion.Key.IsValid() ? 0 : 1;
	}
	const int32 GainNow = M_Measurement.GainedRadixite + M_Measurement.GainedMetal;
	Check(GainNow == M_Measurement.GainAtEmptied, TEXT("no_gain_after_emptied"),
	      FString::Printf(TEXT("at_emptied=%d after_settle=%d"), M_Measurement.GainAtEmptied, GainNow));
	Check(SingleCompletions == ContainerCount, TEXT("each_container_paid_once"),
	      FString::Printf(TEXT("once=%d of %d"), SingleCompletions, ContainerCount));
	Check(RemovedContainers == ContainerCount, TEXT("emptied_containers_removed"),
	      FString::Printf(TEXT("removed=%d of %d"), RemovedContainers, ContainerCount));
	Check(M_Measurement.PaybackSeconds >= 0.f, TEXT("gain_covers_cost"),
	      FString::Printf(TEXT("gained=%d cost=%d"), M_Measurement.GainedRadixite + M_Measurement.GainedMetal,
	                      M_Measurement.CostTotal));
}

void UResourceDropTestSubsystem::FinishDepositCase()
{
	const ACPPResourceMaster* Deposit = nullptr;
	for (const TWeakObjectPtr<AActor>& Landed : M_LandedResources)
	{
		Deposit = Deposit ? Deposit : Cast<ACPPResourceMaster>(Landed.Get());
	}
	const UResourceComponent* ResourceComponent = IsValid(Deposit) ? Deposit->GetResourceComponent() : nullptr;
	const int32 Remaining = IsValid(ResourceComponent) ? ResourceComponent->GetTotalAmount() : 0;
	const int32 Mined = M_Measurement.DepositStartAmount - Remaining;
	Check(M_Measurement.GainedRadixite > 0, TEXT("harvested_radixite_reached_player"),
	      FString::Printf(TEXT("gained=%d deliveries=%d"), M_Measurement.GainedRadixite, M_Measurement.GainEvents));
	// Cargo still in the harvester explains mined > gained; gained > mined would be radixite from nowhere.
	Check(M_Measurement.GainedRadixite <= Mined && M_Measurement.GainedMetal == 0, TEXT("gain_never_exceeds_mined"),
	      FString::Printf(TEXT("gained=%d mined=%d remaining=%d"), M_Measurement.GainedRadixite, Mined, Remaining));
	Check(M_Measurement.PaybackSeconds >= 0.f, TEXT("gain_covers_cost"),
	      FString::Printf(TEXT("gained=%d cost=%d"), M_Measurement.GainedRadixite, M_Measurement.CostTotal));
	if (M_Refinery.IsValid() && IsValid(Deposit))
	{
		UE_LOG(LogRTSResourceDropTest, Display, TEXT("RTS_RESOURCE_DROP_TEST refinery_to_deposit_m=%.1f"),
		       FVector::Dist2D(M_Refinery->GetActorLocation(), Deposit->GetActorLocation()) / 100.f);
	}
}

void UResourceDropTestSubsystem::LogMeasurement() const
{
	const FResourceDropTestMeasurement& Measured = M_Measurement;
	const float CollectWindow = Measured.CollectedSeconds - Measured.FirstGainSeconds;
	const int32 Gained = Measured.GainedRadixite + Measured.GainedMetal;
	const float PerMinute = CollectWindow > KINDA_SMALL_NUMBER ? Gained / CollectWindow * 60.f : 0.f;
	UE_LOG(LogRTSResourceDropTest, Display,
	       TEXT("RTS_RESOURCE_DROP_TEST MEASURED %s cost=%d gained R=%d M=%d net=%+d landed=%.1fs first_gain=%.1fs "
		       "payback=%.1fs collected=%.1fs gain_events=%d rate_after_first_gain=%.0f/min"),
	       *M_Cases[M_CaseIndex].Name, Measured.CostTotal, Measured.GainedRadixite, Measured.GainedMetal,
	       Gained - Measured.CostTotal, Measured.LandedSeconds, Measured.FirstGainSeconds, Measured.PaybackSeconds,
	       Measured.CollectedSeconds, Measured.GainEvents, PerMinute);
}

// ---------------------------------------------------------------------------------------------------------------
// Helpers

UGA_ResourceDrop* UResourceDropTestSubsystem::GetCurrentAbility() const
{
	return M_Abilities.IsEmpty() ? nullptr : M_Abilities.Last().Get();
}

bool UResourceDropTestSubsystem::GetIsCurrentCaseDeposit() const
{
	const UGA_ResourceDrop* Ability = GetCurrentAbility();
	return IsValid(Ability)
		&& Ability->GetResourceDropSettings().PayloadKind == EResourceDropPayloadKind::HarvestableDeposit;
}

int32 UResourceDropTestSubsystem::GetPlayerResource(const ERTSResourceType ResourceType) const
{
	const ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	const UPlayerResourceManager* ResourceManager = IsValid(Controller)
		                                                ? Controller->GetPlayerResourceManager()
		                                                : nullptr;
	return IsValid(ResourceManager) ? ResourceManager->GetResourceAmount(ResourceType) : 0;
}

void UResourceDropTestSubsystem::UpdateCamera() const
{
	using namespace ResourceDropTestPrivate;
	APlayerController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	if (not IsValid(Controller) || not IsValid(M_Camera))
	{
		return;
	}
	FVector Focus = M_Anchor;
	if (M_Phase == EResourceDropTestPhase::WaitingForLanding)
	{
		for (TActorIterator<AParachutedResourcePayload> It(GetWorld()); It; ++It)
		{
			Focus = It->GetActorLocation();
			break;
		}
	}
	const FRotator ViewRotation(CameraPitch, CameraYaw, 0.f);
	M_Camera->SetActorLocationAndRotation(Focus - ViewRotation.Vector() * CameraDistance, ViewRotation);
	// The game's own camera takes the view back; it is handed to the test camera again every frame.
	Controller->SetViewTarget(M_Camera);
}

void UResourceDropTestSubsystem::RequestScreenshot(const FString& Label)
{
	const FString FileName = FPaths::ProjectSavedDir() / TEXT("ResourceDropTest") /
		FString::Printf(TEXT("%02d_%s.png"), M_ScreenshotCount++, *Label);
	FScreenshotRequest::RequestScreenshot(FileName, false, false);
}

#else

bool UResourceDropTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const { return false; }
void UResourceDropTestSubsystem::Tick(float DeltaTime) {}
TStatId UResourceDropTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UResourceDropTestSubsystem, STATGROUP_Tickables);
}

#endif
