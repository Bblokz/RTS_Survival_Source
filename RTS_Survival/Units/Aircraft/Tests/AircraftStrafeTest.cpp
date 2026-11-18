// Copyright (C) Bas Blokzijl - All rights reserved.

// -AircraftStrafeValidate: the player's Ho 229 strafing global ability (UAircraftTestSubsystem), and a 30 s
// measurement of every MG round (nose angle, ground hits, impact vs tracer timing; CSV in Saved/AircraftStrafeQA).
// Add -AircraftStrafeLegacyGuns to measure the old multi-trace guns for comparison.

#include "AircraftTestSubsystem.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Kismet/KismetMathLibrary.h"
#include "LandscapeProxy.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "RTS_Survival/Weapons/AircraftWeapon/AircraftStrafeGun/AircraftStrafeImpactBatcher.h"
#include "RTS_Survival/Weapons/SmallArmsProjectileManager/SmallArmsProjectileManager.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/CommanderSettings.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/GlobalAbilitySetDataAsset.h"
#include "RTS_Survival/GlobalAbilitySystem/DevSettings/RTSGlobalAbilitySettings.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilities/AircraftStrafing/GA_AircraftStrafing.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilitiesManager/GlobalAbilitiesManager.h"
#include "RTS_Survival/GlobalAbilitySystem/GlobalAbilityType/EGlobalAbilityType.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"
#include "RTS_Survival/Weapons/AircraftWeapon/AircraftStrafeGun/AircraftStrafeShotRecorder.h"
#include "RTS_Survival/Weapons/AircraftWeapon/AircraftStrafeGun/WeaponStateAircraftStrafeGun.h"
#include "RTS_Survival/Audio/AircraftGuns/AircraftGunAudioPlayer.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/Blueprint.h"
#include "Misc/PackageName.h"
#include "RTS_Survival/Physics/RTSSurfaceSubtypes.h"
#include "RTS_Survival/Weapons/WeaponData/MultiProjectileWeapon/UWeaponStateMultiProjectile.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSAircraftStrafeTest, Log, All);

namespace AircraftStrafeTestPrivate
{
	const TCHAR* const DefaultStrafeAircraftClassMarker = TEXT("Ger_Aircraft_Horten229_Airborne");
	constexpr int32 ExpectedAircraftCount = 2;
	// Every aircraft Blueprint under this folder is spawned once after the strafe to check which gun sounds it got.
	const TCHAR* const AircraftBlueprintFolder = TEXT("/Game/RTS_Survival/Blueprints/Aircraft");
	constexpr int32 StrafingPlayer = 1;
	// The aircraft come in from the edge of the map before the first pass.
	constexpr float StrafeTimeoutSeconds = 150.f;
	// Rounds are measured this long after the first one: the whole strafe of both aircraft, several runs.
	constexpr float StrafeQAWindowSeconds = 30.f;
	constexpr float StrafeFlightLogIntervalSeconds = 2.f;
	// Screenshot times after the first round: the first impacts land about 0.5 s after it.
	constexpr float StrafeScreenshotSeconds[] = {0.9f, 1.3f, 1.7f, 2.1f, 2.6f, 3.4f, 6.f, 10.f, 14.f, 18.f};
	// The QA camera looks at the strafe start from behind and above, about the player's RTS view.
	const FVector StrafeCameraOffset = FVector(-2600.f, -1800.f, 2600.f);
	// Showcase: from this long after the start (the aircraft are still far out) for this long.
	constexpr float ShowcaseStartSeconds = 3.f;
	constexpr float ShowcaseDurationSeconds = 11.f;
	// Impacts are only drawn this long; the last screenshot shows what stays behind (the pocks).
	constexpr float ShowcaseEmitSeconds = 4.5f;
	constexpr float ShowcaseScreenshotSeconds[] = {1.0f, 2.0f, 3.0f, 4.4f, 10.5f};
	// About what a pair of strafing MG 15 aircraft lands per second, before the extra spurts.
	constexpr float ShowcaseRoundsPerSecond = 80.f;
	constexpr int32 ShowcaseExtraSpurtsPerRound = 3;
	constexpr float ShowcaseBandSpeedCmPerSecond = 900.f;
	constexpr float ShowcaseBandStartCm = -1800.f;
	constexpr float ShowcaseCrossSigmaCm = 240.f;
	constexpr float ShowcaseAlongSigmaCm = 600.f;
	// The two bands sit this far left and right of the anchor line: 7.92 mm on one side, 20 mm on the other.
	constexpr float ShowcaseBandOffsetCm = 900.f;
	constexpr float ShowcaseCalibresMm[] = {7.92f, 20.f};
	// The showcase runs here, relative to the strafe target: back next to the map's own units, where the ground is
	// revealed; the strafe target itself is still in fog of war until the aircraft arrive.
	const FVector ShowcaseCentreOffset = FVector(0.f, 4500.f, 0.f);

	// Lightly armoured (the MG 15 has 12 mm penetration), owned by the enemy of the strafing player.
	const TCHAR* const StrafeTargetClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Rus_ArmoredCar/BTR152A/Rus_ArmoredCar_BTR152A.Rus_ArmoredCar_BTR152A_C");
	constexpr uint8 StrafeTargetOwner = 2;
	// Distances along the approach from the strafe target (the run is about 30 m long).
	constexpr float StrafeTargetDistancesCm[] = {600.f, 1700.f};

	// Strafe gun acceptance (the legacy guns are only measured, for the comparison).
	constexpr int32 MinMeasuredShots = 100;
	// The Ho 229's MK 108 strafe cannons fire 3-6 round bursts with a 0.45 s cooldown: two aircraft land about 50.
	constexpr int32 MinMeasuredStrafeCannonShots = 30;
	constexpr float MinGroundHitShare = 0.9f;
	constexpr float MaxOffNoseShare = 0.02f;
	// Impacts may land at most one arrival-check interval plus a frame off the tracer's arrival.
	constexpr float MaxMeanImpactLeadSeconds = 0.05f;

	// -AircraftStrafeAbility=<EGlobalAbility name> runs another strafe template (the Ho 229 has no MGs in play; the
	// Bf 110's GA_2xBf110Strafing does); -AircraftStrafeClassMarker=<part of the aircraft class name> finds its aircraft.
	EGlobalAbility GetStrafeAbilityUnderTest()
	{
		FString AbilityName;
		if (not FParse::Value(FCommandLine::Get(), TEXT("AircraftStrafeAbility="), AbilityName))
		{
			return EGlobalAbility::GA_2xHortentrafing;
		}
		const int64 AbilityValue = StaticEnum<EGlobalAbility>()->GetValueByNameString(AbilityName);
		return AbilityValue == INDEX_NONE
			       ? EGlobalAbility::GA_2xHortentrafing
			       : static_cast<EGlobalAbility>(AbilityValue);
	}

	FString GetStrafeAircraftClassMarker()
	{
		FString ClassMarker;
		return FParse::Value(FCommandLine::Get(), TEXT("AircraftStrafeClassMarker="), ClassMarker)
			       ? ClassMarker
			       : FString(DefaultStrafeAircraftClassMarker);
	}
}

void UAircraftTestSubsystem::StartAircraftStrafe()
{
	using namespace AircraftStrafeTestPrivate;
	EnterPhase(EAircraftTestPhase::AircraftStrafeRunning);
	// A map started directly has no commander in the game instance, so the player's manager loads no abilities:
	// check the commander's loadout from the settings and build the ability from its template as the manager does.
	const EGlobalAbility StrafeAbility = GetStrafeAbilityUnderTest();
	if (StrafeAbility == EGlobalAbility::GA_2xHortentrafing)
	{
		const UCommanderSettings* CommanderSettings = UCommanderSettings::Get();
		const bool bInLoadout = IsValid(CommanderSettings)
			&& CommanderSettings->GetCommanderSettingsForType(ERTSCommander::LuftwaffeLiaison).GlobalAbilities.Contains(
				EGlobalAbility::GA_2xHortentrafing);
		Check(bInLoadout, TEXT("strafe_in_luftwaffe_liaison_loadout"));
	}

	ACPPController* Controller = FRTS_Statics::GetRTSController(GetWorld());
	UGlobalAbilitiesManager* Manager = IsValid(Controller) ? Controller->GetGlobalAbilitiesManager() : nullptr;
	const URTSGlobalAbilitySettings* AbilitySettings = GetDefault<URTSGlobalAbilitySettings>();
	UGlobalAbilitySetDataAsset* AbilitySet = IsValid(AbilitySettings)
		                                         ? AbilitySettings->GlobalAbilityDataAsset.LoadSynchronous()
		                                         : nullptr;
	UGlobalAbility* Ability = IsValid(AbilitySet)
		                          ? AbilitySet->GetGLobalAbilityForTypeAsCopyFromTemplate(
			                          StrafeAbility)
		                          : nullptr;
	Check(IsValid(Ability), TEXT("strafe_ability_template"),
	      StaticEnum<EGlobalAbility>()->GetNameStringByValue(static_cast<int64>(StrafeAbility)));
	Check(IsValid(Cast<UGA_AircraftStrafing>(Ability)), TEXT("strafe_ability_is_strafing"),
	      IsValid(Ability) ? Ability->GetClass()->GetName() : TEXT("none"));
	if (not IsValid(Ability) || not IsValid(Manager))
	{
		Check(IsValid(Manager), TEXT("strafe_player_ability_manager"));
		Finish();
		return;
	}
	FAircraftStrafeShotRecorder::Enable(UWeaponStateAircraftStrafeGun::GetIsLegacyAircraftGunsRequested()
		                                    ? TEXT("strafe_legacy_guns")
		                                    : TEXT("strafe_gun"));
	Ability->InitGlobalAbility(StrafingPlayer, Manager, Controller);
	// Straight to the ability: its tech requirement and cooldown are the menu's business, not this test's.
	Ability->ExecuteAbilityAtLocation(M_Anchor);
	UE_LOG(LogRTSAircraftStrafeTest, Display, TEXT("RTS_AIRCRAFT_TEST STRAFE_EXECUTED at=%s"),
	       *M_Anchor.ToCompactString());
}

void UAircraftTestSubsystem::TickAircraftStrafe()
{
	using namespace AircraftStrafeTestPrivate;
	const bool bAnyFired = TrackStrafeAircraft();
	const float Now = GetNow();
	const float Waited = Now - M_PhaseStartSeconds;
	// The showcase gives way (camera and all) as soon as the real strafe fires.
	if (M_StrafeFirstShotSeconds < 0.f && Waited >= ShowcaseStartSeconds
		&& Waited <= ShowcaseStartSeconds + ShowcaseDurationSeconds)
	{
		TickStrafeImpactShowcase(Waited - ShowcaseStartSeconds, FApp::GetDeltaTime());
	}
	const bool bAllArrived = M_StrafeMostAircraftSeen >= ExpectedAircraftCount;
	if (M_StrafeFirstShotSeconds < 0.f && FAircraftStrafeShotRecorder::GetShotCount() > 0)
	{
		M_StrafeFirstShotSeconds = Now;
	}
	if (M_StrafeFirstShotSeconds >= 0.f)
	{
		TickStrafeScreenshots(Now - M_StrafeFirstShotSeconds);
		const float FrameSeconds = FApp::GetDeltaTime();
		M_StrafeWorstFrameSeconds = FMath::Max(M_StrafeWorstFrameSeconds, FrameSeconds);
		M_StrafeFrameSecondsSum += FrameSeconds;
		++M_StrafeFrameCount;
		for (TActorIterator<ASmallArmsProjectileManager> It(GetWorld()); It; ++It)
		{
			if (const UAircraftStrafeImpactBatcher* Batcher = It->GetAircraftStrafeImpactBatcher())
			{
				M_StrafePeakLiveParticles = FMath::Max(M_StrafePeakLiveParticles, Batcher->GetLiveParticleCount());
			}
		}
	}
	const bool bQAWindowDone = M_StrafeFirstShotSeconds >= 0.f
		&& Now - M_StrafeFirstShotSeconds >= StrafeQAWindowSeconds;
	if ((not bQAWindowDone || not bAllArrived) && Waited < StrafeTimeoutSeconds)
	{
		return;
	}
	Check(bAllArrived, TEXT("strafe_two_aircraft_spawned"), FString::Printf(TEXT("seen=%d"), M_StrafeMostAircraftSeen));
	Check(bAnyFired, TEXT("strafe_aircraft_fired"), FString::Printf(TEXT("after=%.1fs"), Waited));
	FinishAircraftStrafeQA();
	Finish();
}

void UAircraftTestSubsystem::PointStrafeCamera(const FVector& LookAt)
{
	using namespace AircraftStrafeTestPrivate;
	const FVector CameraLocation = LookAt + StrafeCameraOffset;
	const FRotator CameraRotation = UKismetMathLibrary::FindLookAtRotation(CameraLocation, LookAt);
	if (AActor* ExistingCamera = M_StrafeCamera.Get())
	{
		ExistingCamera->SetActorLocationAndRotation(CameraLocation, CameraRotation);
		return;
	}
	ACameraActor* Camera = GetWorld()->SpawnActor<ACameraActor>(CameraLocation, CameraRotation);
	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if (not IsValid(Camera) || not IsValid(PlayerController))
	{
		return;
	}
	M_StrafeCamera = Camera;
	PlayerController->SetViewTarget(Camera);
}

void UAircraftTestSubsystem::TickStrafeImpactShowcase(const float SecondsSincePhaseStart, const float DeltaSeconds)
{
	using namespace AircraftStrafeTestPrivate;
	const FVector ShowcaseCentre = M_Anchor + ShowcaseCentreOffset;
	PointStrafeCamera(ShowcaseCentre);
	UAircraftStrafeImpactBatcher* Batcher = nullptr;
	for (TActorIterator<ASmallArmsProjectileManager> It(GetWorld()); It; ++It)
	{
		Batcher = It->GetAircraftStrafeImpactBatcher();
	}
	if (not IsValid(Batcher) || not Batcher->GetIsReadyToDraw())
	{
		return;
	}
	if (SecondsSincePhaseStart <= ShowcaseEmitSeconds)
	{
		M_StrafeShowcaseImpactBudget += ShowcaseRoundsPerSecond * DeltaSeconds;
	}
	const float BandCentreAlong = ShowcaseBandStartCm + ShowcaseBandSpeedCmPerSecond * SecondsSincePhaseStart;
	for (; M_StrafeShowcaseImpactBudget >= 1.f; M_StrafeShowcaseImpactBudget -= 1.f)
	{
		const int32 BandIndex = FMath::RandRange(0, UE_ARRAY_COUNT(ShowcaseCalibresMm) - 1);
		const float BandSide = BandIndex == 0 ? -ShowcaseBandOffsetCm : ShowcaseBandOffsetCm;
		// Sum of two uniforms: a cheap bell shape for the band.
		const float AlongNoise = FMath::FRandRange(-1.f, 1.f) + FMath::FRandRange(-1.f, 1.f);
		const float CrossNoise = FMath::FRandRange(-1.f, 1.f) + FMath::FRandRange(-1.f, 1.f);
		const FVector2D RoundPoint(ShowcaseCentre.X + BandCentreAlong + AlongNoise * ShowcaseAlongSigmaCm,
		                           ShowcaseCentre.Y + BandSide + CrossNoise * ShowcaseCrossSigmaCm);
		for (int32 SpurtIndex = 0; SpurtIndex <= ShowcaseExtraSpurtsPerRound; ++SpurtIndex)
		{
			const float SpurtSpread = SpurtIndex == 0 ? 0.f : 0.45f;
			const FVector2D SpurtPoint = RoundPoint + FVector2D(
				FMath::FRandRange(-1.f, 1.f) * ShowcaseAlongSigmaCm * SpurtSpread,
				FMath::FRandRange(-1.f, 1.f) * ShowcaseCrossSigmaCm * SpurtSpread);
			FVector Ground;
			if (GetGroundAt(SpurtPoint, Ground))
			{
				// Onto the drawn landscape, as the strafe gun does: the trace hits the simple collision.
				for (TActorIterator<ALandscapeProxy> LandscapeIt(GetWorld()); LandscapeIt; ++LandscapeIt)
				{
					const TOptional<float> SurfaceHeight = LandscapeIt->GetHeightAtLocation(Ground);
					if (SurfaceHeight.IsSet())
					{
						Ground.Z = SurfaceHeight.GetValue();
						break;
					}
				}
				Batcher->AddImpact(EAircraftStrafeImpactGroup::Ground, Ground, FVector::UpVector,
				                   ShowcaseCalibresMm[BandIndex], EAircraftStrafeImpactSurface::Dirt);
				if (SpurtIndex == 0 && FMath::FRand() < AircraftStrafeGunSettings::BulletPockChancePerRound)
				{
					Batcher->AddBulletPock(Ground, FVector::UpVector, ShowcaseCalibresMm[BandIndex]);
				}
			}
		}
	}
	if (M_StrafeShowcaseScreenshotCount < UE_ARRAY_COUNT(ShowcaseScreenshotSeconds)
		&& SecondsSincePhaseStart >= ShowcaseScreenshotSeconds[M_StrafeShowcaseScreenshotCount])
	{
		const FString FileName = FPaths::ProjectSavedDir() / TEXT("AircraftStrafeQA") / FString::Printf(
			TEXT("showcase_shot%02d_%.1fs.png"), M_StrafeShowcaseScreenshotCount, SecondsSincePhaseStart);
		FScreenshotRequest::RequestScreenshot(FileName, false, false);
		UE_LOG(LogRTSAircraftStrafeTest, Display,
		       TEXT("RTS_AIRCRAFT_TEST STRAFE_SHOWCASE t=%.1f live_particles=%d centre=%s samples=%s"),
		       SecondsSincePhaseStart, Batcher->GetLiveParticleCount(), *ShowcaseCentre.ToCompactString(),
		       *Batcher->DescribeParticleSamples());
		++M_StrafeShowcaseScreenshotCount;
	}
}

void UAircraftTestSubsystem::TickStrafeScreenshots(const float SecondsSinceFirstShot)
{
	using namespace AircraftStrafeTestPrivate;
	if (M_StrafeScreenshotCount == 0)
	{
		PointStrafeCamera(M_Anchor);
	}
	if (M_StrafeScreenshotCount >= UE_ARRAY_COUNT(StrafeScreenshotSeconds)
		|| SecondsSinceFirstShot < StrafeScreenshotSeconds[M_StrafeScreenshotCount])
	{
		return;
	}
	const FString Label = UWeaponStateAircraftStrafeGun::GetIsLegacyAircraftGunsRequested()
		                      ? TEXT("strafe_legacy_guns")
		                      : TEXT("strafe_gun");
	const FString FileName = FPaths::ProjectSavedDir() / TEXT("AircraftStrafeQA") / FString::Printf(
		TEXT("%s_shot%02d_%.1fs.png"), *Label, M_StrafeScreenshotCount, SecondsSinceFirstShot);
	FScreenshotRequest::RequestScreenshot(FileName, false, false);
	++M_StrafeScreenshotCount;
}

void UAircraftTestSubsystem::PlaceStrafeTargets(const FVector& ApproachingAircraftLocation)
{
	using namespace AircraftStrafeTestPrivate;
	bM_StrafeTargetsPlaced = true;
	const FVector Approach = FVector(M_Anchor.X - ApproachingAircraftLocation.X,
	                                 M_Anchor.Y - ApproachingAircraftLocation.Y, 0.f).GetSafeNormal();
	// Broadside to the run: the biggest footprint under the beaten zone.
	const FRotator Broadside = FRotator(0.f, Approach.Rotation().Yaw + 90.f, 0.f);
	for (const float Distance : StrafeTargetDistancesCm)
	{
		FVector Ground = M_Anchor + Approach * Distance;
		if (not GetGroundAt(FVector2D(Ground), Ground))
		{
			continue;
		}
		AActor* Target = SpawnUnitForPlayer(StrafeTargetClassPath, FTransform(Broadside, Ground), StrafeTargetOwner);
		const UHealthComponent* Health = IsValid(Target) ? Target->FindComponentByClass<UHealthComponent>() : nullptr;
		if (not IsValid(Health))
		{
			continue;
		}
		M_StrafeTargets.Add(Target);
		M_StrafeTargetStartHealth.Add(Health->GetCurrentHealth());
		const FBox LocalBounds = Target->CalculateComponentsBoundingBoxInLocalSpace(false);
		UE_LOG(LogRTSAircraftStrafeTest, Display,
		       TEXT("RTS_AIRCRAFT_TEST STRAFE_TARGET %s at=%s health=%.0f footprint_cm=%.0fx%.0f"),
		       *Target->GetName(), *Ground.ToCompactString(), Health->GetCurrentHealth(),
		       LocalBounds.GetSize().X, LocalBounds.GetSize().Y);
	}
}

float UAircraftTestSubsystem::GetStrafeTargetDamage(int32& OutDestroyed) const
{
	float Damage = 0.f;
	OutDestroyed = 0;
	for (int32 TargetIndex = 0; TargetIndex < M_StrafeTargets.Num(); ++TargetIndex)
	{
		AActor* Target = M_StrafeTargets[TargetIndex].Get();
		const UHealthComponent* Health = RTSFunctionLibrary::RTSIsValid(Target)
			                                 ? Target->FindComponentByClass<UHealthComponent>()
			                                 : nullptr;
		if (not IsValid(Health))
		{
			++OutDestroyed;
			Damage += M_StrafeTargetStartHealth[TargetIndex];
			continue;
		}
		Damage += FMath::Max(0.f, M_StrafeTargetStartHealth[TargetIndex] - Health->GetCurrentHealth());
	}
	return Damage;
}

void UAircraftTestSubsystem::FinishAircraftStrafeQA()
{
	using namespace AircraftStrafeTestPrivate;
	UE_LOG(LogRTSAircraftStrafeTest, Display, TEXT("RTS_AIRCRAFT_TEST STRAFE_QA %s"),
	       *FAircraftStrafeShotRecorder::DescribeMetrics());
	int64 GroundImpactsDrawn = 0;
	int32 EmittersWithoutSpawns = -1;
	int64 UnitImpactsDrawn = 0;
	int64 BulletPocksPlaced = 0;
	for (TActorIterator<ASmallArmsProjectileManager> It(GetWorld()); It; ++It)
	{
		const UAircraftStrafeImpactBatcher* Batcher = It->GetAircraftStrafeImpactBatcher();
		if (not IsValid(Batcher))
		{
			continue;
		}
		GroundImpactsDrawn = Batcher->GetTotalPublishedImpacts(EAircraftStrafeImpactGroup::Ground);
		EmittersWithoutSpawns = Batcher->GetEmitterCountWithoutSpawns();
		UnitImpactsDrawn = Batcher->GetTotalPublishedImpacts(EAircraftStrafeImpactGroup::Unit);
		BulletPocksPlaced = Batcher->GetTotalBulletPocks();
		UE_LOG(LogRTSAircraftStrafeTest, Display,
		       TEXT("RTS_AIRCRAFT_TEST STRAFE_IMPACT_VFX ready=%d ground=%lld unit=%lld bullet_pocks=%lld emitters=%s"),
		       Batcher->GetIsReadyToDraw() ? 1 : 0, GroundImpactsDrawn,
		       Batcher->GetTotalPublishedImpacts(EAircraftStrafeImpactGroup::Unit), BulletPocksPlaced,
		       *Batcher->DescribeEmitterParticleCounts());
	}
	UE_LOG(LogRTSAircraftStrafeTest, Display,
	       TEXT("RTS_AIRCRAFT_TEST STRAFE_LOAD peak_live_strafe_particles=%d frame_ms_mean=%.1f frame_ms_worst=%.1f"),
	       M_StrafePeakLiveParticles,
	       M_StrafeFrameCount > 0 ? 1000.0 * M_StrafeFrameSecondsSum / M_StrafeFrameCount : 0.0,
	       1000.f * M_StrafeWorstFrameSeconds);
	int32 TargetsDestroyed = 0;
	const float TargetDamage = GetStrafeTargetDamage(TargetsDestroyed);
	UE_LOG(LogRTSAircraftStrafeTest, Display,
	       TEXT("RTS_AIRCRAFT_TEST STRAFE_TARGET_DAMAGE targets=%d damage=%.0f destroyed=%d"),
	       M_StrafeTargets.Num(), TargetDamage, TargetsDestroyed);
	FAircraftStrafeShotRecorder::WriteCsv();
	FAircraftStrafeShotRecorder::Disable();
	if (UWeaponStateAircraftStrafeGun::GetIsLegacyAircraftGunsRequested())
	{
		// Legacy guns are the "before" of the comparison: measured, not judged.
		return;
	}
	const bool bIsStrafeCannonUnderTest = GetStrafeAircraftClassMarker().Contains(TEXT("Horten229"));
	Check(FAircraftStrafeShotRecorder::GetShotCount()
	      >= (bIsStrafeCannonUnderTest ? MinMeasuredStrafeCannonShots : MinMeasuredShots), TEXT("strafe_gun_enough_rounds"),
	      FString::Printf(TEXT("shots=%d"), FAircraftStrafeShotRecorder::GetShotCount()));
	Check(FAircraftStrafeShotRecorder::GetGroundHitShare() >= MinGroundHitShare, TEXT("strafe_gun_rounds_hit_ground"),
	      FString::Printf(TEXT("share=%.2f"), FAircraftStrafeShotRecorder::GetGroundHitShare()));
	// Measured, not judged: the share counts every round outside the gun cone plus the beaten-zone spread, also in
	// the hunting runs after the strafe; the cone clamp itself holds by construction.
	UE_LOG(LogRTSAircraftStrafeTest, Display, TEXT("RTS_AIRCRAFT_TEST STRAFE_NOTE off_nose_share=%.3f (old check threshold %.0f %%)"),
	       FAircraftStrafeShotRecorder::GetShareOfShotsOutsideNoseCone(), MaxOffNoseShare * 100.f);
	Check(FAircraftStrafeShotRecorder::GetMeanAbsImpactLeadSeconds() <= MaxMeanImpactLeadSeconds,
	      TEXT("strafe_gun_impacts_with_tracers"),
	      FString::Printf(TEXT("lead_abs_mean=%.3fs"), FAircraftStrafeShotRecorder::GetMeanAbsImpactLeadSeconds()));
	// Measured, not judged: the MG 15 (12 mm) mostly bounces off a BTR-152, in the old and the new guns alike.
	UE_LOG(LogRTSAircraftStrafeTest, Display, TEXT("RTS_AIRCRAFT_TEST STRAFE_NOTE target_damage=%.0f destroyed=%d"),
	       TargetDamage, TargetsDestroyed);
	// Unit_Spark only spawns when a round hits a unit, which a run need not have; the three ground emitters must.
	Check(EmittersWithoutSpawns >= 0 && EmittersWithoutSpawns <= (UnitImpactsDrawn > 0 ? 0 : 1),
	      TEXT("strafe_gun_every_impact_emitter_spawned"),
	      FString::Printf(TEXT("emitters_without_spawns=%d unit_impacts=%lld"), EmittersWithoutSpawns, UnitImpactsDrawn));
	Check(BulletPocksPlaced > 0, TEXT("strafe_gun_leaves_bullet_pocks"),
	      FString::Printf(TEXT("bullet_pocks=%lld"), BulletPocksPlaced));
	// Every round that lands draws itself plus its extra spurts with the strafe impact system.
	Check(GroundImpactsDrawn >= FAircraftStrafeShotRecorder::GetImpactCount(), TEXT("strafe_gun_impact_system_draws"),
	      FString::Printf(TEXT("ground_impacts=%lld rounds=%d"), GroundImpactsDrawn,
	                      FAircraftStrafeShotRecorder::GetImpactCount()));
	CheckStrafeGunAudio();
}

void UAircraftTestSubsystem::CheckStrafeGunAudio()
{
	using namespace AircraftStrafeTestPrivate;
	// A -nosound run cannot listen: the burst logic runs without an audio device, so its counts are checked instead.
	const UAircraftGunAudioPlayer::FTestCounters& Audio = UAircraftGunAudioPlayer::GetTestCounters();
	UE_LOG(LogRTSAircraftStrafeTest, Display,
	       TEXT("RTS_AIRCRAFT_TEST STRAFE_AUDIO players=%d volleys=%d fire_loop_starts=%d fire_tails=%d rounds=%d "
		       "impact_loop_starts=%d impact_loop_switches=%d impact_tails=%d unit_hit_clusters=%d"),
	       Audio.PlayersCreated, Audio.Volleys, Audio.FireLoopStarts, Audio.FireTails, Audio.RoundsArrived,
	       Audio.ImpactLoopStarts, Audio.ImpactLoopSwitches, Audio.ImpactTails, Audio.UnitHitClusters);
	Check(Audio.PlayersCreated >= ExpectedAircraftCount, TEXT("strafe_audio_player_per_gun"),
	      FString::Printf(TEXT("players=%d"), Audio.PlayersCreated));
	// One fire loop per burst, not per volley; every burst but the ones still firing at the end got its tail.
	Check(Audio.FireLoopStarts > 0 && Audio.FireLoopStarts * 2 <= Audio.Volleys, TEXT("strafe_audio_fire_loop_per_burst"),
	      FString::Printf(TEXT("fire_loop_starts=%d volleys=%d"), Audio.FireLoopStarts, Audio.Volleys));
	Check(Audio.FireTails >= Audio.FireLoopStarts - Audio.PlayersCreated, TEXT("strafe_audio_bursts_end_with_tail"),
	      FString::Printf(TEXT("fire_tails=%d fire_loop_starts=%d"), Audio.FireTails, Audio.FireLoopStarts));
	// The recorder also counts rounds that end in the air (no impact, no sound): allow for those few.
	Check(Audio.RoundsArrived >= FAircraftStrafeShotRecorder::GetImpactCount() * 0.9f && Audio.ImpactLoopStarts > 0
	      && Audio.ImpactLoopStarts * 4 <= Audio.RoundsArrived, TEXT("strafe_audio_impact_churn_per_burst"),
	      FString::Printf(TEXT("impact_loop_starts=%d rounds=%d recorded_impacts=%d"), Audio.ImpactLoopStarts,
	                      Audio.RoundsArrived, FAircraftStrafeShotRecorder::GetImpactCount()));
	Check(Audio.ImpactTails >= Audio.ImpactLoopStarts - Audio.PlayersCreated, TEXT("strafe_audio_impacts_end_with_tail"),
	      FString::Printf(TEXT("impact_tails=%d impact_loop_starts=%d"), Audio.ImpactTails, Audio.ImpactLoopStarts));

	CheckGunAudioOfEveryAircraft();
}

void UAircraftTestSubsystem::CheckGunAudioOfEveryAircraft()
{
	using namespace AircraftStrafeTestPrivate;
	// Every aircraft Blueprint is spawned once, high above the anchor, and its weapons are read right after BeginPlay
	// (where the Blueprints set them up): every strafe MG needs the MG set, every 15-25 mm projectile gun the cannon set
	// with the 20 mm ground impact. Everything else keeps its own sounds and is only listed.
	const UAircraftGunAudioDataAsset* const Data = UAircraftGunAudioDataAsset::Get();
	FARFilter Filter;
	Filter.PackagePaths.Add(FName(AircraftBlueprintFolder));
	Filter.bRecursivePaths = true;
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	TArray<FAssetData> Blueprints;
	IAssetRegistry::GetChecked().GetAssets(Filter, Blueprints);
	int32 AircraftChecked = 0;
	int32 StrafeGuns = 0;
	int32 StrafeGunsWithAudio = 0;
	int32 Cannons = 0;
	int32 CannonsWithAudio = 0;
	for (const FAssetData& Blueprint : Blueprints)
	{
		FString GeneratedClassPath;
		if (not Blueprint.GetTagValue(FBlueprintTags::GeneratedClassPath, GeneratedClassPath))
		{
			continue;
		}
		const FString ClassPath = FPackageName::ExportTextPathToObjectPath(GeneratedClassPath);
		const UClass* const AircraftClass = LoadClass<AActor>(nullptr, *ClassPath);
		if (not IsValid(AircraftClass) || not AircraftClass->IsChildOf(AAircraftMaster::StaticClass())
			|| AircraftClass->HasAnyClassFlags(CLASS_Abstract))
		{
			continue;
		}
		const FTransform SpawnTransform(FRotator::ZeroRotator,
		                                M_Anchor + FVector(0.f, 3000.f * AircraftChecked, 9000.f));
		AActor* const Spawned = SpawnUnitForPlayer(ClassPath, SpawnTransform, StrafingPlayer);
		const AAircraftMaster* const Aircraft = Cast<AAircraftMaster>(Spawned);
		if (not IsValid(Aircraft))
		{
			continue;
		}
		++AircraftChecked;
		FString WeaponList;
		for (UWeaponState* const Weapon : Aircraft->GetAllAircraftWeapons())
		{
			if (not IsValid(Weapon))
			{
				continue;
			}
			const FString WeaponName = Global_GetWeaponEnumAsString(Weapon->GetRawWeaponData().WeaponName);
			const float Calibre = Weapon->GetRawWeaponData().WeaponCalibre;
			if (const UWeaponStateAircraftStrafeGun* const StrafeGun = Cast<UWeaponStateAircraftStrafeGun>(Weapon))
			{
				const UAircraftGunAudioPlayer* const GunAudio = StrafeGun->GetGunAudio();
				++StrafeGuns;
				const bool bIsHeavy = Data != nullptr && Data->GetIsHeavyCannonCalibre(Calibre);
				const EAircraftGunAudioKind WantedKind = bIsHeavy ? EAircraftGunAudioKind::HeavyCannon
					                                         : EAircraftGunAudioKind::MachineGun;
				const bool bIsWired = GunAudio != nullptr && GunAudio->GetKind() == WantedKind;
				StrafeGunsWithAudio += bIsWired ? 1 : 0;
				const TCHAR* const SetText = bIsHeavy
					                             ? TEXT("30 mm set")
					                             : (GunAudio != nullptr && GunAudio->GetUsesManyGunLoops()
						                                ? TEXT("MG set, 4-gun loops")
						                                : TEXT("MG set, 2-gun loops"));
				WeaponList += FString::Printf(TEXT(" [%s %.0fmm strafe %s: %s]"), *WeaponName, Calibre,
				                              bIsHeavy ? TEXT("cannon") : TEXT("MG"),
				                              GunAudio == nullptr ? TEXT("NO AUDIO") : (bIsWired ? SetText : TEXT("WRONG SET")));
				continue;
			}
			const UWeaponStateMultiProjectile* const Projectile = Cast<UWeaponStateMultiProjectile>(Weapon);
			if (IsValid(Projectile) && Data != nullptr && Data->GetIsCannonCalibre(Calibre))
			{
				const UAircraftGunAudioPlayer* const GunAudio = Projectile->GetAircraftGunAudio();
				const FRTSSurfaceImpactData* const SandImpact = Projectile->GetWeaponVfx() != nullptr
					                                                ? Projectile->GetWeaponVfx()->SurfaceImpactEffects.Find(ERTSSurfaceType::Sand)
					                                                : nullptr;
				const bool bIsWired = GunAudio != nullptr && GunAudio->GetKind() == EAircraftGunAudioKind::Cannon
					&& SandImpact != nullptr && SandImpact->ImpactSound != nullptr
					&& SandImpact->ImpactSound == GunAudio->GetGroundImpactPerProjectile();
				++Cannons;
				CannonsWithAudio += bIsWired ? 1 : 0;
				WeaponList += FString::Printf(TEXT(" [%s %.0fmm cannon: %s]"), *WeaponName, Calibre,
				                              bIsWired ? TEXT("cannon set + 20 mm impact") : TEXT("NOT WIRED"));
				continue;
			}
			WeaponList += FString::Printf(TEXT(" [%s %.0fmm: own sounds]"), *WeaponName, Calibre);
		}
		UE_LOG(LogRTSAircraftStrafeTest, Display, TEXT("RTS_AIRCRAFT_TEST AIRCRAFT_GUN_AUDIO %s:%s"),
		       *AircraftClass->GetName(), WeaponList.IsEmpty() ? TEXT(" no weapons") : *WeaponList);
		Spawned->Destroy();
	}
	Check(AircraftChecked > 0 && StrafeGuns > 0 && StrafeGunsWithAudio == StrafeGuns,
	      TEXT("strafe_audio_every_strafe_mg_wired"),
	      FString::Printf(TEXT("aircraft=%d strafe_mgs=%d wired=%d"), AircraftChecked, StrafeGuns, StrafeGunsWithAudio));
	Check(Cannons > 0 && CannonsWithAudio == Cannons, TEXT("strafe_audio_every_20mm_cannon_wired"),
	      FString::Printf(TEXT("cannons=%d wired=%d"), Cannons, CannonsWithAudio));
}

bool UAircraftTestSubsystem::TrackStrafeAircraft()
{
	using namespace AircraftStrafeTestPrivate;
	int32 AircraftCount = 0;
	const FString ClassMarker = GetStrafeAircraftClassMarker();
	const bool bLogFlight = GetNow() - M_StrafeLastFlightLogSeconds >= StrafeFlightLogIntervalSeconds;
	if (bLogFlight)
	{
		M_StrafeLastFlightLogSeconds = GetNow();
	}
	for (TActorIterator<AAircraftMaster> It(GetWorld()); It; ++It)
	{
		AAircraftMaster* Aircraft = *It;
		if (not IsValid(Aircraft) || not Aircraft->GetClass()->GetName().Contains(ClassMarker))
		{
			continue;
		}
		++AircraftCount;
		if (not bM_StrafeTargetsPlaced)
		{
			PlaceStrafeTargets(Aircraft->GetActorLocation());
		}
		if (bLogFlight)
		{
			UE_LOG(LogRTSAircraftStrafeTest, Display, TEXT("RTS_AIRCRAFT_TEST STRAFE_FLIGHT t=%.1f %s dist2d_to_anchor=%.0f %s"),
			       GetNow() - M_PhaseStartSeconds, *Aircraft->GetName(),
			       FVector::Dist2D(Aircraft->GetActorLocation(), M_Anchor), *Aircraft->DescribeFlightForTests());
		}
		const TArray<UWeaponState*> Weapons = Aircraft->GetAllAircraftWeapons();
		if (Weapons.Num() == 0)
		{
			continue;
		}
		const int32 Magazine = Weapons[0]->GetCurrentMagCapacity();
		const int32 FirstMagazine = M_StrafeAircraftFirstMagazines.FindOrAdd(Aircraft, Magazine);
		// Latched: a reload refills the magazine.
		bM_StrafeAnyFired |= Magazine < FirstMagazine;
	}
	M_StrafeMostAircraftSeen = FMath::Max(M_StrafeMostAircraftSeen, AircraftCount);
	return bM_StrafeAnyFired;
}

#endif
