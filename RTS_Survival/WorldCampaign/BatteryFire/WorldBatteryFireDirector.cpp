// Copyright (C) Bas Blokzijl - All rights reserved.

#include "RTS_Survival/WorldCampaign/BatteryFire/WorldBatteryFireDirector.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/App.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "RTS_Survival/Subsystems/RadiusSubsystem/RTSRadiusPoolSubsystem/RTSRadiusPoolSubsystem.h"
#include "RTS_Survival/Types/MovePlayerCameraTypes.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/ImpactVfxAuthoring/ImpactVfxSpriteScale.h"
#include "RTS_Survival/WorldCampaign/BatteryFire/WorldBatteryFireSettings.h"
#include "RTS_Survival/WorldCampaign/CampaignProgress/WorldCampaignProgressManager.h"
#include "RTS_Survival/WorldCampaign/SaveAndState/WorldStateAndSaveManager/WorldStateAndSaveManager.h"
#include "RTS_Survival/WorldCampaign/WorldBuildNodes/WorldBuildNode.h"
#include "RTS_Survival/WorldCampaign/WorldBuildNodes/WorldBuildNodeManager.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionBase.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionManager.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldDivisionTankFormation.h"
#include "RTS_Survival/WorldCampaign/WorldDivisions/WorldTankDivision.h"
#include "RTS_Survival/WorldCampaign/WorldFow/WorldFowManager.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/Controller/WorldCameraController/WorldCameraController.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/Controller/WorldPlayerAudioController/WorldPlayerAudioController.h"
#include "RTS_Survival/WorldCampaign/WorldPlayer/Controller/WorldPlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSWorldBatteryFire, Log, All);

namespace WorldBatteryFireDirectorConstants
{
	// User parameters of the game's shell trail (see AProjectile).
	const FName TrailShellColorParameterName = TEXT("ShellColor");
	const FName TrailShellWidthParameterName = TEXT("ShellWidth");
	// The falling whistle starts when the first shell is past the top of its arc.
	constexpr float DescentSoundAlpha = 0.5f;
	// A small step along the arc, to read the shell's direction of flight.
	constexpr float DirectionSampleAlpha = 0.01f;
	// Impacts sit a little above the map so they are not buried in the country fill.
	constexpr float ImpactHeightAboveMap = 2.f;
	// The shell owner outlives the show by this long, so the trails left in the air can fade out.
	constexpr float ShellOwnerLifeSpanAfterShow = 5.f;
}

UWorldBatteryFireDirector::UWorldBatteryFireDirector()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UWorldBatteryFireDirector::BeginPlay()
{
	Super::BeginPlay();
	M_WorldPlayerController = Cast<AWorldPlayerController>(GetOwner());
}

void UWorldBatteryFireDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelTargetPicking();
	if (M_Show.bIsPlaying)
	{
		// The mission itself was applied and saved before the show started; only the visuals are cut short.
		FinishShow(false);
	}
	Super::EndPlay(EndPlayReason);
}

void UWorldBatteryFireDirector::TickComponent(const float DeltaTime, const ELevelTick TickType,
                                              FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const UWorldBatteryFireSettings* const Settings = UWorldBatteryFireSettings::Get();
	if (not M_Show.bIsPlaying || Settings == nullptr)
	{
		SetComponentTickEnabled(false);
		return;
	}

	M_Show.ElapsedSeconds += DeltaTime;
	for (FWorldBatteryFireShell& Shell : M_Show.Shells)
	{
		UpdateShell(Shell, *Settings);
	}

	if (M_Show.ElapsedSeconds >= M_Show.DurationSeconds)
	{
		FinishShow(Settings->bReturnCameraAfterShow);
	}
}

// ------------------------------------------------------------------ Rules against the live map

EWorldBatteryFireResult UWorldBatteryFireDirector::GetBatteryReadiness(const FName BuildNodeId) const
{
	const UWorldBatteryFireSettings* const Settings = UWorldBatteryFireSettings::Get();
	if (Settings == nullptr)
	{
		return EWorldBatteryFireResult::UnknownBattery;
	}

	return WorldBatteryFireRules::GetBatteryReadiness(MakeBatteryInput(BuildNodeId), MakeAllTargetInputs(),
	                                                  Settings->GetNumbers());
}

EWorldBatteryFireResult UWorldBatteryFireDirector::GetFireMissionResult(const FName BuildNodeId,
                                                                       const FGuid& DivisionKey) const
{
	const UWorldBatteryFireSettings* const Settings = UWorldBatteryFireSettings::Get();
	const UWorldDivisionManager* const DivisionManager = GetDivisionManager();
	if (Settings == nullptr || not IsValid(DivisionManager))
	{
		return EWorldBatteryFireResult::UnknownTarget;
	}

	return WorldBatteryFireRules::GetFireMissionResult(
		MakeBatteryInput(BuildNodeId), MakeTargetInput(DivisionManager->FindDivisionByKey(DivisionKey)),
		Settings->GetNumbers());
}

TArray<FGuid> UWorldBatteryFireDirector::GetValidTargetKeys(const FName BuildNodeId) const
{
	const UWorldBatteryFireSettings* const Settings = UWorldBatteryFireSettings::Get();
	if (Settings == nullptr)
	{
		return TArray<FGuid>();
	}

	return WorldBatteryFireRules::GetValidTargetKeys(MakeBatteryInput(BuildNodeId), MakeAllTargetInputs(),
	                                                 Settings->GetNumbers());
}

FWorldBatteryFireBattery UWorldBatteryFireDirector::MakeBatteryInput(const FName BuildNodeId) const
{
	FWorldBatteryFireBattery Battery;
	const UWorldBuildNodeManager* const BuildNodeManager = GetBuildNodeManager();
	const AWorldBuildNode* const BuildNode = IsValid(BuildNodeManager)
		                                         ? BuildNodeManager->GetBuildNode(BuildNodeId)
		                                         : nullptr;
	const UWorldCampaignProgressManager* const ProgressManager = GetProgressManager();
	if (not IsValid(BuildNode) || not IsValid(ProgressManager))
	{
		return Battery;
	}

	Battery.bIsKnownNode = true;
	Battery.NodeState = BuildNode->GetNodeState();
	Battery.Building = BuildNode->GetBuilding();
	Battery.Location = BuildNode->GetActorLocation();
	Battery.MissionsFiredThisTurn = WorldBatteryFireRules::GetMissionsFiredThisTurn(
		ProgressManager->GetProgressState().BatteryFire, BuildNodeId, GetCurrentTurn());
	return Battery;
}

FWorldBatteryFireTarget UWorldBatteryFireDirector::MakeTargetInput(const AWorldDivisionBase* Division) const
{
	FWorldBatteryFireTarget Target;
	if (not IsValid(Division))
	{
		return Target;
	}

	Target.bExists = true;
	Target.DivisionKey = Division->GetDivisionKey();
	Target.OwningPlayer = Division->GetOwningPlayer();
	Target.Location = Division->GetActorLocation();
	// Divisions are no fog participants; the fog mask says whether the place they stand on is in sight.
	const AWorldFowManager* const FowManager = M_WorldPlayerController.IsValid()
		                                           ? M_WorldPlayerController->GetWorldFowManager()
		                                           : nullptr;
	Target.bIsVisibleThroughFog = not IsValid(FowManager) || FowManager->GetIsWorldLocationRevealed(Target.Location);
	return Target;
}

TArray<FWorldBatteryFireTarget> UWorldBatteryFireDirector::MakeAllTargetInputs() const
{
	TArray<FWorldBatteryFireTarget> Targets;
	const UWorldDivisionManager* const DivisionManager = GetDivisionManager();
	if (not IsValid(DivisionManager))
	{
		return Targets;
	}

	for (const TObjectPtr<AWorldDivisionBase>& Division : DivisionManager->GetWorldDivisions())
	{
		if (IsValid(Division))
		{
			Targets.Add(MakeTargetInput(Division.Get()));
		}
	}
	return Targets;
}

int32 UWorldBatteryFireDirector::GetCurrentTurn() const
{
	const UWorldStateAndSaveManager* const SaveManager = M_WorldPlayerController.IsValid()
		                                                     ? M_WorldPlayerController->GetWorldStateAndSaveManager()
		                                                     : nullptr;
	return IsValid(SaveManager) ? SaveManager->GetCurrentTurn() : 0;
}

// ------------------------------------------------------------------ Mission

bool UWorldBatteryFireDirector::RequestFireMission(const FName BuildNodeId, const FGuid& DivisionKey,
                                                   FText& OutFailReason)
{
	OutFailReason = FText::GetEmpty();
	if (M_Show.bIsPlaying || not GetIsValidWorldPlayerController())
	{
		return false;
	}

	const EWorldBatteryFireResult RuleResult = GetFireMissionResult(BuildNodeId, DivisionKey);
	if (RuleResult != EWorldBatteryFireResult::Success)
	{
		OutFailReason = WorldBatteryFireRules::GetFireResultText(RuleResult);
		return false;
	}

	const UWorldBuildNodeManager* const BuildNodeManager = GetBuildNodeManager();
	const UWorldDivisionManager* const DivisionManager = GetDivisionManager();
	UWorldCampaignProgressManager* const ProgressManager = GetProgressManager();
	const AWorldBuildNode* const BuildNode = IsValid(BuildNodeManager)
		                                         ? BuildNodeManager->GetBuildNode(BuildNodeId)
		                                         : nullptr;
	AWorldDivisionBase* const Division = IsValid(DivisionManager)
		                                     ? DivisionManager->FindDivisionByKey(DivisionKey)
		                                     : nullptr;
	if (not IsValid(BuildNode) || not IsValid(Division) || not IsValid(ProgressManager))
	{
		return false;
	}

	CancelTargetPicking();
	const FWorldBatteryFireMissionResult Result = BuildMissionResult(*BuildNode, *Division);
	const bool bWillPlayShow = GetCanPlayShow();
	/*
	 * Order matters: the battery is marked first, because applying the damage already saves the campaign (the
	 * division manager does on every strength change). The show starts only after everything is saved, so cutting
	 * it short loses nothing and repeats nothing.
	 */
	WorldBatteryFireRules::RecordMissionFired(ProgressManager->GetMutableProgressState().BatteryFire, BuildNodeId,
	                                          GetCurrentTurn());
	ApplyMissionDamage(Result, *Division, bWillPlayShow);
	SaveCampaign();
	UE_LOG(LogRTSWorldBatteryFire, Display,
	       TEXT("BATTERYFIRE mission node=%s division=%s destroyed=%d emptied=%d show=%d"),
	       *BuildNodeId.ToString(), *DivisionKey.ToString(), Result.EntriesDestroyed,
	       Result.bIsDivisionEmptied ? 1 : 0, bWillPlayShow ? 1 : 0);
	OnMissionFired.Broadcast(Result);

	if (bWillPlayShow)
	{
		StartShow(Result, Division);
		return true;
	}

	FinishMissionVisuals(Division, Result.bIsDivisionEmptied);
	OnShowFinished.Broadcast();
	return true;
}

FWorldBatteryFireMissionResult UWorldBatteryFireDirector::BuildMissionResult(
	const AWorldBuildNode& BuildNode, const AWorldDivisionBase& Division) const
{
	FWorldBatteryFireMissionResult Result;
	const UWorldBatteryFireSettings* const Settings = UWorldBatteryFireSettings::Get();
	Result.BuildNodeId = BuildNode.GetBuildNodeId();
	Result.DivisionKey = Division.GetDivisionKey();
	Result.BatteryLocation = BuildNode.GetActorLocation();
	Result.MuzzleLocation = GetMuzzleLocation(BuildNode);
	Result.TargetLocation = Division.GetActorLocation();
	const int32 EntriesPerMission = Settings != nullptr ? Settings->EntriesDestroyedPerMission : 0;
	const FWorldBatteryFireCasualties Casualties = WorldBatteryFireRules::GetCasualties(
		Division.GetRemainingEntryCount(), EntriesPerMission);
	Result.EntriesDestroyed = Casualties.DestroyedCount;
	Result.bIsDivisionEmptied = Casualties.bIsDivisionEmptied;

	const AWorldTankDivision* const TankDivision = Cast<AWorldTankDivision>(&Division);
	const UWorldDivisionTankFormation* const Formation = IsValid(TankDivision)
		                                                     ? TankDivision->GetTankFormation()
		                                                     : nullptr;
	if (not IsValid(Formation) || not Formation->GetShowsTanks())
	{
		return Result;
	}

	// The last slot dies last, so the division visibly shrinks from the back of its roster.
	for (int32 CasualtyIndex = 0; CasualtyIndex < Casualties.DestroyedCount; ++CasualtyIndex)
	{
		const int32 SlotIndex = Casualties.FirstDestroyedSlot + CasualtyIndex;
		FVector SlotLocation = Result.TargetLocation;
		if (Formation->TryGetSlotWorldLocation(SlotIndex, SlotLocation))
		{
			Result.DestroyedSlots.Add(SlotIndex);
			Result.DestroyedSlotLocations.Add(SlotLocation);
		}
	}
	return Result;
}

FVector UWorldBatteryFireDirector::GetMuzzleLocation(const AWorldBuildNode& BuildNode) const
{
	const UWorldBatteryFireSettings* const Settings = UWorldBatteryFireSettings::Get();
	if (Settings == nullptr)
	{
		return BuildNode.GetActorLocation();
	}

	FVector MuzzleLocation = FVector::ZeroVector;
	if (BuildNode.TryGetBuildingPieceSocketLocation(Settings->MuzzleSocketName, MuzzleLocation))
	{
		return MuzzleLocation;
	}

	return BuildNode.GetActorLocation() + FVector(0.f, 0.f, Settings->MuzzleHeightOffset);
}

void UWorldBatteryFireDirector::ApplyMissionDamage(const FWorldBatteryFireMissionResult& Result,
                                                   AWorldDivisionBase& Division, const bool bWillPlayShow)
{
	UWorldDivisionTankFormation* const Formation = GetTankFormation(&Division);
	if (IsValid(Formation))
	{
		Formation->SetDefersCasualtyVisuals(bWillPlayShow);
	}

	if (not Result.bIsDivisionEmptied)
	{
		// The division manager hears the strength change, refreshes the targets in range and saves.
		Division.ApplyWorldDivisionDamage(Result.EntriesDestroyed);
		return;
	}

	UWorldDivisionManager* const DivisionManager = GetDivisionManager();
	UWorldCampaignProgressManager* const ProgressManager = GetProgressManager();
	if (not IsValid(DivisionManager) || not IsValid(ProgressManager))
	{
		return;
	}

	// Same bookkeeping as a division beaten in an operation: the enemy reacts toward where it lost a piece.
	ProgressManager->GetMutableProgressState().EnemyTurn.RecentlyStruckLocations.Add(Result.TargetLocation);
	/*
	 * Damaging the last entries through the division would destroy the actor inside that call. Releasing it takes
	 * it out of the manager and the save right now and leaves the actor standing for the show.
	 */
	(void)DivisionManager->ReleaseDivisionForDeferredDestroy(Result.DivisionKey);
	if (IsValid(Formation))
	{
		Formation->HandleRosterShrunk(0);
	}
}

void UWorldBatteryFireDirector::SaveCampaign() const
{
	UWorldStateAndSaveManager* const SaveManager = M_WorldPlayerController.IsValid()
		                                               ? M_WorldPlayerController->GetWorldStateAndSaveManager()
		                                               : nullptr;
	if (IsValid(SaveManager))
	{
		SaveManager->SaveCampaignState();
	}
}

void UWorldBatteryFireDirector::HandleNewPlayerTurn(const int32 CurrentTurn)
{
	if (UWorldCampaignProgressManager* const ProgressManager = GetProgressManager())
	{
		WorldBatteryFireRules::ResetForNewTurn(ProgressManager->GetMutableProgressState().BatteryFire, CurrentTurn);
	}

	const UWorldDivisionManager* const DivisionManager = GetDivisionManager();
	if (not IsValid(DivisionManager))
	{
		return;
	}

	// Wrecks are scenery of the turn they were made in: every formation is laid out again from its roster.
	for (const TObjectPtr<AWorldDivisionBase>& Division : DivisionManager->GetWorldDivisions())
	{
		const AWorldTankDivision* const TankDivision = Cast<AWorldTankDivision>(Division.Get());
		UWorldDivisionTankFormation* const Formation = IsValid(TankDivision) ? TankDivision->GetTankFormation() : nullptr;
		if (IsValid(Formation))
		{
			Formation->RebuildFromRoster(TankDivision->GetTankSubtypes());
		}
	}
}

// ------------------------------------------------------------------ Show

bool UWorldBatteryFireDirector::GetCanPlayShow() const
{
	const UWorld* const World = GetWorld();
	// Without a renderer (headless runs) nobody sees the show, and input must not stay blocked waiting for it.
	const bool bHasAudience = FApp::CanEverRender() || bM_PlaysShowWithoutRenderer;
	return IsValid(World) && World->IsGameWorld() && bHasAudience && UWorldBatteryFireSettings::Get() != nullptr;
}

void UWorldBatteryFireDirector::StartShow(const FWorldBatteryFireMissionResult& Result, AWorldDivisionBase* Division)
{
	const UWorldBatteryFireSettings* const Settings = UWorldBatteryFireSettings::Get();
	if (Settings == nullptr || not GetIsValidWorldPlayerController())
	{
		FinishMissionVisuals(Division, Result.bIsDivisionEmptied);
		OnShowFinished.Broadcast();
		return;
	}

	M_Show = FWorldBatteryFireShowState();
	M_Show.Result = Result;
	M_Show.Division = Division;
	M_Show.bIsPlaying = true;
	M_Show.DurationSeconds = WorldBatteryFireRules::GetShowDurationSeconds(
		Settings->ShellCount, Settings->LeadInSeconds, Settings->ShellStaggerSeconds, Settings->FlightSeconds,
		Settings->LingerSeconds);
	if (UWorld* const World = GetWorld())
	{
		M_Show.ShellOwner = World->SpawnActor<AActor>();
	}
	M_WorldPlayerController->SetIsInputBlockedForMapShow(true);
	StartShow_PreloadAssets(*Settings);
	StartShow_FrameCamera(*Settings);
	StartShow_BuildShells(*Settings);
	SetComponentTickEnabled(true);
}

void UWorldBatteryFireDirector::StartShow_PreloadAssets(const UWorldBatteryFireSettings& Settings) const
{
	UWorldBatteryFireSettings::LoadOptional(Settings.ShellMesh, TEXT("ShellMesh"));
	UWorldBatteryFireSettings::LoadOptional(Settings.MuzzleEffect, TEXT("MuzzleEffect"));
	UWorldBatteryFireSettings::LoadOptional(Settings.TrailEffect, TEXT("TrailEffect"));
	UWorldBatteryFireSettings::LoadOptional(Settings.ImpactEffect, TEXT("ImpactEffect"));
	UWorldBatteryFireSettings::LoadOptional(Settings.TankExplosionEffect, TEXT("TankExplosionEffect"));
	UWorldBatteryFireSettings::LoadOptional(Settings.WreckFireEffect, TEXT("WreckFireEffect"));
	UWorldBatteryFireSettings::LoadOptional(Settings.LaunchSound, TEXT("LaunchSound"));
	UWorldBatteryFireSettings::LoadOptional(Settings.DescentSound, TEXT("DescentSound"));
	UWorldBatteryFireSettings::LoadOptional(Settings.ImpactSound, TEXT("ImpactSound"));
	UWorldBatteryFireSettings::LoadOptional(Settings.TankExplosionSound, TEXT("TankExplosionSound"));
}

void UWorldBatteryFireDirector::StartShow_BuildShells(const UWorldBatteryFireSettings& Settings)
{
	const FWorldBatteryFireMissionResult& Result = M_Show.Result;
	// Seeded by the target so a mission looks the same every time it is replayed for a capture.
	FRandomStream ScatterStream(static_cast<int32>(GetTypeHash(Result.DivisionKey)));
	M_Show.Shells.SetNum(FMath::Max(1, Settings.ShellCount));
	for (int32 ShellIndex = 0; ShellIndex < M_Show.Shells.Num(); ++ShellIndex)
	{
		FWorldBatteryFireShell& Shell = M_Show.Shells[ShellIndex];
		Shell.LaunchSeconds = WorldBatteryFireRules::GetShellLaunchSeconds(
			ShellIndex, Settings.LeadInSeconds, Settings.ShellStaggerSeconds);
		const FVector2D Scatter = FVector2D(ScatterStream.GetUnitVector()).GetSafeNormal()
			* ScatterStream.FRandRange(0.f, Settings.ImpactScatterRadius);
		Shell.ImpactLocation = Result.TargetLocation + FVector(Scatter.X, Scatter.Y, 0.f);
	}

	const int32 CasualtyCount = Result.DestroyedSlots.Num();
	for (int32 CasualtyIndex = 0; CasualtyIndex < CasualtyCount; ++CasualtyIndex)
	{
		const int32 ShellIndex = WorldBatteryFireRules::GetShellIndexForCasualty(
			CasualtyIndex, CasualtyCount, M_Show.Shells.Num());
		if (not M_Show.Shells.IsValidIndex(ShellIndex) || not Result.DestroyedSlotLocations.IsValidIndex(CasualtyIndex))
		{
			continue;
		}

		// With more casualties than shells several share a shell; the shell is aimed at the last of them.
		M_Show.Shells[ShellIndex].CasualtySlot = Result.DestroyedSlots[CasualtyIndex];
		M_Show.Shells[ShellIndex].ImpactLocation = Result.DestroyedSlotLocations[CasualtyIndex];
	}
}

void UWorldBatteryFireDirector::StartShow_FrameCamera(const UWorldBatteryFireSettings& Settings)
{
	UWorldCameraController* const CameraController = M_WorldPlayerController.IsValid()
		                                                 ? M_WorldPlayerController->FindComponentByClass<
			                                                 UWorldCameraController>()
		                                                 : nullptr;
	if (not Settings.bFrameGunAndTarget || not IsValid(CameraController))
	{
		return;
	}

	const FWorldBatteryFireMissionResult& Result = M_Show.Result;
	M_Show.CameraLocationBeforeShow = CameraController->GetCurrentCameraLocation();
	M_Show.ZoomDistanceBeforeShow = CameraController->GetZoomDistance();
	M_Show.bHasCameraToRestore = true;

	const FVector MidPoint = (Result.BatteryLocation + Result.TargetLocation) * 0.5f;
	FMovePlayerCamera MoveRequest;
	// The camera keeps its own height; only its place over the map changes.
	MoveRequest.MoveToLocation = FVector(MidPoint.X, MidPoint.Y, M_Show.CameraLocationBeforeShow.Z);
	MoveRequest.TimeToMove = Settings.CameraMoveSeconds;
	CameraController->MoveCameraTo(MoveRequest);
	const float WantedZoom = FVector::Dist2D(Result.BatteryLocation, Result.TargetLocation)
		* Settings.CameraZoomPerDistance + Settings.CameraZoomPadding;
	// Never zooms in: a player who already sees both keeps the view.
	CameraController->SetZoomDistance(FMath::Max(M_Show.ZoomDistanceBeforeShow, WantedZoom));
}

void UWorldBatteryFireDirector::UpdateShell(FWorldBatteryFireShell& Shell, const UWorldBatteryFireSettings& Settings)
{
	if (Shell.bHasLanded || M_Show.ElapsedSeconds < Shell.LaunchSeconds)
	{
		return;
	}

	if (not Shell.bHasLaunched)
	{
		LaunchShell(Shell, Settings);
	}

	const float Alpha = (M_Show.ElapsedSeconds - Shell.LaunchSeconds) / FMath::Max(Settings.FlightSeconds,
	                                                                               KINDA_SMALL_NUMBER);
	if (Alpha >= 1.f)
	{
		LandShell(Shell, Settings);
		return;
	}

	if (Alpha >= WorldBatteryFireDirectorConstants::DescentSoundAlpha && not M_Show.bHasPlayedDescentSound)
	{
		M_Show.bHasPlayedDescentSound = true;
		PlaySound(UWorldBatteryFireSettings::LoadOptional(Settings.DescentSound, TEXT("DescentSound")));
	}

	if (not IsValid(Shell.Carrier))
	{
		return;
	}

	const FVector& Muzzle = M_Show.Result.MuzzleLocation;
	const FVector ShellLocation = WorldBatteryFireRules::GetShellLocation(
		Muzzle, Shell.ImpactLocation, Alpha, Settings.ArcHeightFactor);
	const FVector AheadLocation = WorldBatteryFireRules::GetShellLocation(
		Muzzle, Shell.ImpactLocation, Alpha + WorldBatteryFireDirectorConstants::DirectionSampleAlpha,
		Settings.ArcHeightFactor);
	Shell.Carrier->SetWorldLocationAndRotation(ShellLocation, (AheadLocation - ShellLocation).Rotation());
}

void UWorldBatteryFireDirector::LaunchShell(FWorldBatteryFireShell& Shell, const UWorldBatteryFireSettings& Settings)
{
	Shell.bHasLaunched = true;
	const FVector& Muzzle = M_Show.Result.MuzzleLocation;
	const FVector FirstStep = WorldBatteryFireRules::GetShellLocation(
		Muzzle, Shell.ImpactLocation, WorldBatteryFireDirectorConstants::DirectionSampleAlpha, Settings.ArcHeightFactor);
	const FRotator LaunchRotation = (FirstStep - Muzzle).Rotation();
	SpawnEffect(UWorldBatteryFireSettings::LoadOptional(Settings.MuzzleEffect, TEXT("MuzzleEffect")), Muzzle,
	            LaunchRotation, Settings.MuzzleEffectScale);
	PlaySound(UWorldBatteryFireSettings::LoadOptional(Settings.LaunchSound, TEXT("LaunchSound")));

	AActor* const ShellOwner = M_Show.ShellOwner;
	if (not IsValid(ShellOwner))
	{
		return;
	}

	// Not attached to anything: the carrier flies in world space.
	Shell.Carrier = NewObject<UStaticMeshComponent>(ShellOwner);
	Shell.Carrier->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Shell.Carrier->SetGenerateOverlapEvents(false);
	Shell.Carrier->SetCastShadow(false);
	Shell.Carrier->SetStaticMesh(UWorldBatteryFireSettings::LoadOptional(Settings.ShellMesh, TEXT("ShellMesh")));
	Shell.Carrier->RegisterComponent();
	Shell.Carrier->SetWorldScale3D(FVector(Settings.ShellMeshScale));
	Shell.Carrier->SetWorldLocationAndRotation(Muzzle, LaunchRotation);

	UNiagaraSystem* const TrailEffect = UWorldBatteryFireSettings::LoadOptional(Settings.TrailEffect, TEXT("TrailEffect"));
	if (not IsValid(TrailEffect))
	{
		return;
	}

	Shell.Trail = UNiagaraFunctionLibrary::SpawnSystemAttached(
		TrailEffect, Shell.Carrier, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
		EAttachLocation::SnapToTarget, true);
	if (not IsValid(Shell.Trail))
	{
		return;
	}

	Shell.Trail->SetWorldScale3D(FVector(Settings.TrailEffectScale));
	Shell.Trail->SetColorParameter(WorldBatteryFireDirectorConstants::TrailShellColorParameterName,
	                               Settings.TrailShellColor);
	Shell.Trail->SetFloatParameter(WorldBatteryFireDirectorConstants::TrailShellWidthParameterName,
	                               Settings.TrailShellWidth);
}

void UWorldBatteryFireDirector::LandShell(FWorldBatteryFireShell& Shell, const UWorldBatteryFireSettings& Settings)
{
	Shell.bHasLanded = true;
	RemoveShellVisuals(Shell);
	const FVector ImpactLocation = Shell.ImpactLocation
		+ FVector(0.f, 0.f, WorldBatteryFireDirectorConstants::ImpactHeightAboveMap);
	SpawnEffect(UWorldBatteryFireSettings::LoadOptional(Settings.ImpactEffect, TEXT("ImpactEffect")), ImpactLocation,
	            FRotator::ZeroRotator, Settings.ImpactEffectScale);
	PlaySound(UWorldBatteryFireSettings::LoadOptional(Settings.ImpactSound, TEXT("ImpactSound")));

	UWorldDivisionTankFormation* const Formation = GetTankFormation(M_Show.Division.Get());
	if (Shell.CasualtySlot == INDEX_NONE || not IsValid(Formation))
	{
		return;
	}

	// Slots that share this shell (more casualties than shells) all go with it.
	bool bDidExplodeTank = false;
	for (int32 CasualtyIndex = 0; CasualtyIndex < M_Show.Result.DestroyedSlots.Num(); ++CasualtyIndex)
	{
		const int32 ShellIndexOfCasualty = WorldBatteryFireRules::GetShellIndexForCasualty(
			CasualtyIndex, M_Show.Result.DestroyedSlots.Num(), M_Show.Shells.Num());
		if (M_Show.Shells.IsValidIndex(ShellIndexOfCasualty) && &M_Show.Shells[ShellIndexOfCasualty] == &Shell)
		{
			bDidExplodeTank = Formation->ExplodeSlot(M_Show.Result.DestroyedSlots[CasualtyIndex]) || bDidExplodeTank;
		}
	}

	if (bDidExplodeTank)
	{
		PlaySound(UWorldBatteryFireSettings::LoadOptional(Settings.TankExplosionSound, TEXT("TankExplosionSound")));
	}
}

void UWorldBatteryFireDirector::RemoveShellVisuals(FWorldBatteryFireShell& Shell) const
{
	if (IsValid(Shell.Trail))
	{
		// Left behind in the air so the trail fades out; it destroys itself when it is done.
		Shell.Trail->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		Shell.Trail->Deactivate();
	}
	Shell.Trail = nullptr;

	if (IsValid(Shell.Carrier))
	{
		Shell.Carrier->DestroyComponent();
	}
	Shell.Carrier = nullptr;
}

void UWorldBatteryFireDirector::FinishShow(const bool bRestoreCamera)
{
	if (not M_Show.bIsPlaying)
	{
		return;
	}

	M_Show.bIsPlaying = false;
	SetComponentTickEnabled(false);
	for (FWorldBatteryFireShell& Shell : M_Show.Shells)
	{
		RemoveShellVisuals(Shell);
	}
	M_Show.Shells.Reset();
	if (IsValid(M_Show.ShellOwner))
	{
		M_Show.ShellOwner->SetLifeSpan(WorldBatteryFireDirectorConstants::ShellOwnerLifeSpanAfterShow);
	}
	M_Show.ShellOwner = nullptr;
	const UWorldDivisionTankFormation* const Formation = GetTankFormation(M_Show.Division.Get());
	FinishMissionVisuals(M_Show.Division.Get(), M_Show.Result.bIsDivisionEmptied);
	const int32 WreckCount = IsValid(Formation) && not M_Show.Result.bIsDivisionEmptied ? Formation->GetWreckCount() : 0;

	if (M_WorldPlayerController.IsValid())
	{
		UWorldCameraController* const CameraController =
			M_WorldPlayerController->FindComponentByClass<UWorldCameraController>();
		if (bRestoreCamera && M_Show.bHasCameraToRestore && IsValid(CameraController))
		{
			const UWorldBatteryFireSettings* const Settings = UWorldBatteryFireSettings::Get();
			FMovePlayerCamera MoveRequest;
			MoveRequest.MoveToLocation = M_Show.CameraLocationBeforeShow;
			MoveRequest.TimeToMove = Settings != nullptr ? Settings->CameraMoveSeconds : 0.f;
			CameraController->MoveCameraTo(MoveRequest);
			CameraController->SetZoomDistance(M_Show.ZoomDistanceBeforeShow);
		}
		M_WorldPlayerController->SetIsInputBlockedForMapShow(false);
	}

	const bool bIsInputStillBlocked = M_WorldPlayerController.IsValid()
		&& M_WorldPlayerController->GetIsWorldPlayerInputDisabled();
	UE_LOG(LogRTSWorldBatteryFire, Display,
	       TEXT("BATTERYFIRE show finished after %.2f s wrecks=%d emptied=%d input_blocked=%d"), M_Show.ElapsedSeconds,
	       WreckCount, M_Show.Result.bIsDivisionEmptied ? 1 : 0, bIsInputStillBlocked ? 1 : 0);
	OnShowFinished.Broadcast();
}

void UWorldBatteryFireDirector::FinishMissionVisuals(AWorldDivisionBase* Division, const bool bIsDivisionEmptied) const
{
	if (not IsValid(Division))
	{
		return;
	}

	if (bIsDivisionEmptied)
	{
		// Already out of the manager and the save; only the actor was kept for the show.
		Division->Destroy();
		return;
	}

	if (UWorldDivisionTankFormation* const Formation = GetTankFormation(Division))
	{
		Formation->FlushDeferredCasualties();
	}
}

void UWorldBatteryFireDirector::SpawnEffect(UNiagaraSystem* Effect, const FVector& Location, const FRotator& Rotation,
                                            const float Scale) const
{
	if (not IsValid(Effect))
	{
		return;
	}

	UNiagaraComponent* const EffectComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		this, Effect, Location, Rotation, FVector(Scale));
	if (IsValid(EffectComponent))
	{
		// Component scale does not reach sprite sizes; the game's impact effects read this parameter for them.
		ImpactVfxSpriteScale::ApplyImpactScale(*EffectComponent, FVector(Scale));
	}
}

void UWorldBatteryFireDirector::PlaySound(USoundBase* Sound) const
{
	const AActor* const Owner = GetOwner();
	const UWorldPlayerAudioController* const AudioController = IsValid(Owner)
		                                                           ? Owner->FindComponentByClass<
			                                                           UWorldPlayerAudioController>()
		                                                           : nullptr;
	if (IsValid(Sound) && IsValid(AudioController))
	{
		AudioController->PlayUISound(Sound);
	}
}

// ------------------------------------------------------------------ Picking

bool UWorldBatteryFireDirector::BeginTargetPicking(const FName BuildNodeId, FText& OutFailReason)
{
	OutFailReason = FText::GetEmpty();
	if (M_Show.bIsPlaying)
	{
		return false;
	}

	const EWorldBatteryFireResult Readiness = GetBatteryReadiness(BuildNodeId);
	if (Readiness != EWorldBatteryFireResult::Success)
	{
		OutFailReason = WorldBatteryFireRules::GetFireResultText(Readiness);
		return false;
	}

	CancelTargetPicking();
	bM_IsPickingTarget = true;
	M_PickingBuildNodeId = BuildNodeId;
	ShowRangeRing(MakeBatteryInput(BuildNodeId).Location);
	OnPickingChanged.Broadcast();
	return true;
}

void UWorldBatteryFireDirector::CancelTargetPicking()
{
	if (not bM_IsPickingTarget)
	{
		return;
	}

	bM_IsPickingTarget = false;
	M_PickingBuildNodeId = NAME_None;
	HideRangeRing();
	OnPickingChanged.Broadcast();
}

bool UWorldBatteryFireDirector::TryFireOnPickedActor(AActor* ClickedActor, FText& OutFailReason)
{
	OutFailReason = FText::GetEmpty();
	if (not bM_IsPickingTarget)
	{
		return false;
	}

	const AWorldDivisionBase* const ClickedDivision = Cast<AWorldDivisionBase>(ClickedActor);
	if (not IsValid(ClickedDivision))
	{
		OutFailReason = WorldBatteryFireRules::GetFireResultText(EWorldBatteryFireResult::UnknownTarget);
		OnPickRefused.Broadcast(OutFailReason);
		return false;
	}

	// RequestFireMission ends the pick itself once the rules accept the target.
	const FName PickingBuildNodeId = M_PickingBuildNodeId;
	if (RequestFireMission(PickingBuildNodeId, ClickedDivision->GetDivisionKey(), OutFailReason))
	{
		return true;
	}

	OnPickRefused.Broadcast(OutFailReason);
	return false;
}

void UWorldBatteryFireDirector::ShowRangeRing(const FVector& BatteryLocation)
{
	HideRangeRing();
	const UWorld* const World = GetWorld();
	URTSRadiusPoolSubsystem* const RadiusPool = IsValid(World) ? World->GetSubsystem<URTSRadiusPoolSubsystem>() : nullptr;
	const UWorldBatteryFireSettings* const Settings = UWorldBatteryFireSettings::Get();
	if (not IsValid(RadiusPool) || Settings == nullptr)
	{
		return;
	}

	M_RangeRingId = RadiusPool->CreateRTSRadius(BatteryLocation, Settings->Range, ERTSRadiusType::RangeBorderOnly);
}

void UWorldBatteryFireDirector::HideRangeRing()
{
	const UWorld* const World = GetWorld();
	URTSRadiusPoolSubsystem* const RadiusPool = IsValid(World) ? World->GetSubsystem<URTSRadiusPoolSubsystem>() : nullptr;
	if (M_RangeRingId > 0 && IsValid(RadiusPool) && RadiusPool->GetIsRTSRadiusIdActive(M_RangeRingId))
	{
		RadiusPool->HideRTSRadiusById(M_RangeRingId);
	}
	M_RangeRingId = INDEX_NONE;
}

// ------------------------------------------------------------------ Access

UWorldDivisionTankFormation* UWorldBatteryFireDirector::GetTankFormation(AWorldDivisionBase* Division) const
{
	const AWorldTankDivision* const TankDivision = Cast<AWorldTankDivision>(Division);
	return IsValid(TankDivision) ? TankDivision->GetTankFormation() : nullptr;
}

bool UWorldBatteryFireDirector::GetIsValidWorldPlayerController() const
{
	if (M_WorldPlayerController.IsValid())
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		TEXT("M_WorldPlayerController"),
		TEXT("UWorldBatteryFireDirector::GetIsValidWorldPlayerController"),
		this);
	return false;
}

UWorldBuildNodeManager* UWorldBatteryFireDirector::GetBuildNodeManager() const
{
	return M_WorldPlayerController.IsValid() ? M_WorldPlayerController->GetWorldBuildNodeManager() : nullptr;
}

UWorldDivisionManager* UWorldBatteryFireDirector::GetDivisionManager() const
{
	return M_WorldPlayerController.IsValid() ? M_WorldPlayerController->GetWorldDivisionManager() : nullptr;
}

UWorldCampaignProgressManager* UWorldBatteryFireDirector::GetProgressManager() const
{
	return M_WorldPlayerController.IsValid() ? M_WorldPlayerController->GetWorldCampaignProgressManager() : nullptr;
}
