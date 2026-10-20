// Copyright (C) Bas Blokzijl - All rights reserved.

// Map test for the flare aim ability, started with -FlareAbilityTest on a map with a navmesh (e.g.
// /Game/RTS_Survival/Maps/UnitTests/RTS_ToCopySmall). It spawns the Sd.Kfz. 231 for the player and follows the
// ability from the command card to the burnt-out flare. Docs/Abilities/FlareAbility.md has the run command.
// Everything here is compiled out with RTS_WITH_MAP_TESTS (every shipping build): no UObjects, no static
// registration, nothing reaches the shipping binary.

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Containers/Ticker.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/DelayedAutoRegister.h"
#include "Misc/Paths.h"
#include "UObject/StrongObjectPtr.h"
#include "Components/StaticMeshComponent.h"
#include "NiagaraSystem.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowComp.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowType.h"
#include "RTS_Survival/FOWSystem/FowManager/FowManager.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AimAbilityComponent/FlareAimAbility/FlareAimAbilityComponent.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AimAbilityComponent/FlareAimAbility/FlareProjectile.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/AimAbilityComponent/FlareAimAbility/Tests/FlareAbilityTestAccess.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/GrenadeComponent/Tests/GrenadeTestHelpers.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSFlareTest, Log, All);

namespace FlareAbilityTestPrivate
{
	const TCHAR* const StartSwitch = TEXT("FlareAbilityTest");
	const TCHAR* const SpotSwitch = TEXT("FlareTestAt=");
	const TCHAR* const NoQuitSwitch = TEXT("FlareTestNoQuit");
	const TCHAR* const UnitClassPath = TEXT(
		"/Game/RTS_Survival/Blueprints/GroundVehicles/TrackBasedVehicle/Ger_ArmoredCar/Sdkfz231/"
		"Ger_ArmoredCar_Sdkfz_231.Ger_ArmoredCar_Sdkfz_231_C");
	const TCHAR* const FlarePath = TEXT("/Game/RTS_Survival/SpecialEffects/Flare/");
	const TCHAR* const LauncherMeshPath = TEXT(
		"/Game/RTS_Survival/Blueprints/Turrets/German/Light/Sdkfz_231Turret/Meshes/SM_FlareDischarger_231");
	constexpr uint8 PlayerOne = 1;
	// The test keeps its moves short; the Blueprint's fire range is put back when the test ends.
	// The 10% move-to-range buffer must exceed the vehicle's move acceptance radius (~3 m), or the unit stops just
	// outside the range and waits there (the shared aim ability move logic).
	constexpr float TestFireRange = 3500.f;
	constexpr float OutOfRangeExtra = 1500.f;
	constexpr float SocketTolerance = 5.f;
	constexpr float ApexTolerance = 60.f;
	constexpr float HangToleranceSeconds = 0.5f;
	constexpr float SettleSeconds = 2.f;
	// Like the grenade tests: the HUD and the navmesh arrive a little after the game starts, and the map's start spot
	// may lie off the navmesh, so the spot is moved onto the nearest navmesh.
	constexpr double HudBuildSeconds = 5.0;
	constexpr double MaxStartSeconds = 60.0;
	const FVector SpotNavExtent(20000.f, 20000.f, 5000.f);
	const FVector TargetNavExtent(3000.f, 3000.f, 5000.f);
	constexpr float MoveTimeoutSeconds = 60.f;
	constexpr float PhaseTimeoutSeconds = 20.f;
	constexpr float SideCameraDistance = 3500.f;
	constexpr float SideCameraHeight = 400.f;
	constexpr int32 PictureFov = 90;
	// Short, so a cooldown left by the cancelled order (cooldowns start when a command is dispatched) cannot block the
	// real order; the cooldown after the launch is still checked.
	constexpr int32 TestCooldownSeconds = 3;
	constexpr float CooldownWaitSeconds = 6.f;
}

enum class EFlareTestPhase : uint8
{
	WaitingForGame,
	WaitingForUnit,
	CancelWaitingForMove,
	CancelWaitingForCard,
	OrderWaitingForLaunch,
	WaitingForIgnition,
	WaitingForBurnOut,
	WaitingForDestroy,
	Done
};

/**
 * @brief Follows one flare order on the Sd.Kfz. 231: the card entry, the drive into range, the launch from the
 * turret socket, the climb to ApexHeight, the vision bubble with VisionRadius in the fog of war manager for
 * HangTimeSeconds while the flare sinks, the burn-out and the cooldown; first it checks that cancelling the move
 * to range launches nothing.
 */
class FFlareAbilityTestScenario
{
public:
	explicit FFlareAbilityTestScenario(UWorld& World) : M_World(&World) {}

	// Returns false once the test is done and may be deleted.
	bool Tick();

private:
	TWeakObjectPtr<UWorld> M_World;
	TWeakObjectPtr<ACPPController> M_PlayerController;
	TWeakObjectPtr<AActor> M_Unit;
	TWeakObjectPtr<UFlareAimAbilityComponent> M_Ability;
	TWeakObjectPtr<AFlareProjectile> M_Flare;
	TWeakObjectPtr<ASceneCapture2D> M_Capture;
	TStrongObjectPtr<UTextureRenderTarget2D> M_RenderTarget;
	FString M_PictureDirectory;

	EFlareTestPhase M_Phase = EFlareTestPhase::WaitingForGame;
	float M_PhaseStartSeconds = 0.f;
	double M_StartRealSeconds = FPlatformTime::Seconds();
	FVector M_Spot = FVector::ZeroVector;
	FVector M_Target = FVector::ZeroVector;
	float M_StartDistanceToTarget = 0.f;
	float M_OriginalFireRange = 0.f;
	float M_IgnitionSeconds = 0.f;
	float M_LastFlareZ = 0.f;
	bool bM_FlareOnlySank = true;
	bool bM_TookHalfwayPicture = false;
	bool bM_TookClimbPicture = false;
	bool bM_CheckedCancel = false;
	int32 M_Passed = 0;
	int32 M_Failed = 0;

	UWorld* GetWorld() const { return M_World.Get(); }
	float GetSeconds() const { return M_World.IsValid() ? M_World->GetTimeSeconds() : 0.f; }
	float GetSecondsInPhase() const { return GetSeconds() - M_PhaseStartSeconds; }
	void EnterPhase(EFlareTestPhase NewPhase);
	void Check(bool bCondition, const TCHAR* Description, const FString& Details = FString());
	void Finish(const TCHAR* Reason);

	ICommands* GetCommands() const { return Cast<ICommands>(M_Unit.Get()); }
	const FUnitAbilityEntry* FindCardEntry(EAbilityID AbilityId, TArray<FUnitAbilityEntry>& Entries) const;
	bool GetHasCardEntry(EAbilityID AbilityId) const;
	int32 GetAimCooldownRemaining() const;
	FVector GetLaunchSocketLocation() const;
	float GetGroundZ(const FVector& Location) const;
	AFlareProjectile* FindFlare() const;
	bool FindOutOfRangeTarget(FVector& OutTarget) const;
	void EnsureFlareComponent();
	void TakePicture(const TCHAR* Name, const FVector& LookAt, bool bSideView);

	void TickWaitingForGame();
	void TickWaitingForUnit();
	void TickCancelWaitingForMove();
	void TickCancelWaitingForCard();
	void TickOrderWaitingForLaunch();
	void TickWaitingForIgnition();
	void TickWaitingForBurnOut();
	void TickWaitingForDestroy();
	bool OrderFlare(const TCHAR* Label);
};

void FFlareAbilityTestScenario::EnterPhase(const EFlareTestPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartSeconds = GetSeconds();
	UE_LOG(LogRTSFlareTest, Display, TEXT("RTS_FLARE_TEST phase %d at %.2f s"), static_cast<int32>(NewPhase),
	       M_PhaseStartSeconds);
}

void FFlareAbilityTestScenario::Check(const bool bCondition, const TCHAR* Description, const FString& Details)
{
	bCondition ? ++M_Passed : ++M_Failed;
	UE_LOG(LogRTSFlareTest, Display, TEXT("RTS_FLARE_TEST check=%s %s %s"), bCondition ? TEXT("PASS") : TEXT("FAIL"),
	       Description, *Details);
}

void FFlareAbilityTestScenario::Finish(const TCHAR* Reason)
{
	if (M_Ability.IsValid() && M_OriginalFireRange > 0.f)
	{
		FFlareAbilityTestAccess::GetFlareSettings(*M_Ability).FireRange = M_OriginalFireRange;
	}
	UE_LOG(LogRTSFlareTest, Display, TEXT("RTS_FLARE_TEST RESULT %s passed=%d failed=%d (%s) pictures=%s"),
	       M_Failed == 0 ? TEXT("PASS") : TEXT("FAIL"), M_Passed, M_Failed, Reason, *M_PictureDirectory);
	EnterPhase(EFlareTestPhase::Done);
	if (not FParse::Param(FCommandLine::Get(), FlareAbilityTestPrivate::NoQuitSwitch))
	{
		FPlatformMisc::RequestExit(false, TEXT("FlareAbilityTest"));
	}
}

bool FFlareAbilityTestScenario::Tick()
{
	if (not M_World.IsValid())
	{
		return false;
	}
	if (M_Phase != EFlareTestPhase::WaitingForGame && M_Phase != EFlareTestPhase::Done)
	{
		FGrenadeTestHelpers::KeepGameRunning(*GetWorld(), M_PlayerController.Get());
		if (not M_Unit.IsValid())
		{
			Check(false, TEXT("The armored car stays alive during the test"));
			Finish(TEXT("unit lost"));
		}
	}
	switch (M_Phase)
	{
	case EFlareTestPhase::WaitingForGame: TickWaitingForGame(); break;
	case EFlareTestPhase::WaitingForUnit: TickWaitingForUnit(); break;
	case EFlareTestPhase::CancelWaitingForMove: TickCancelWaitingForMove(); break;
	case EFlareTestPhase::CancelWaitingForCard: TickCancelWaitingForCard(); break;
	case EFlareTestPhase::OrderWaitingForLaunch: TickOrderWaitingForLaunch(); break;
	case EFlareTestPhase::WaitingForIgnition: TickWaitingForIgnition(); break;
	case EFlareTestPhase::WaitingForBurnOut: TickWaitingForBurnOut(); break;
	case EFlareTestPhase::WaitingForDestroy: TickWaitingForDestroy(); break;
	case EFlareTestPhase::Done: return false;
	}
	return M_Phase != EFlareTestPhase::Done;
}

void FFlareAbilityTestScenario::TickWaitingForGame()
{
	using namespace FlareAbilityTestPrivate;
	UWorld* World = GetWorld();
	const double RealSeconds = FPlatformTime::Seconds() - M_StartRealSeconds;
	if (RealSeconds > MaxStartSeconds)
	{
		Check(false, TEXT("The game starts on a spot with navmesh"));
		Finish(TEXT("no spot"));
		return;
	}
	if (RealSeconds < HudBuildSeconds || not World->HasBegunPlay() ||
		not FGrenadeTestHelpers::TryStartGame(*World, M_PlayerController))
	{
		return;
	}
	if (not FGrenadeTestHelpers::FindTestSpot(*World, SpotSwitch, M_Spot) ||
		not FGrenadeTestHelpers::ProjectOntoNavMesh(*World, M_Spot, SpotNavExtent))
	{
		return;
	}
	AActor* Unit = FGrenadeTestHelpers::SpawnUnitForPlayer(*World, UnitClassPath, FTransform(M_Spot), PlayerOne);
	Check(IsValid(Unit), TEXT("Sd.Kfz. 231 spawns for the player"), UnitClassPath);
	if (not IsValid(Unit))
	{
		Finish(TEXT("no unit"));
		return;
	}
	M_Unit = Unit;
	EnsureFlareComponent();
	if (FApp::CanEverRender())
	{
		ASceneCapture2D* Capture = nullptr;
		UTextureRenderTarget2D* RenderTarget = nullptr;
		if (FGrenadeTestHelpers::CreateCaptureCamera(*World, PictureFov, Capture, RenderTarget))
		{
			M_Capture = Capture;
			M_RenderTarget.Reset(RenderTarget);
			M_PictureDirectory = FPaths::ProjectSavedDir() / TEXT("FlareAbilityTest");
		}
	}
	EnterPhase(EFlareTestPhase::WaitingForUnit);
}

void FFlareAbilityTestScenario::EnsureFlareComponent()
{
	using namespace FlareAbilityTestPrivate;
	UFlareAimAbilityComponent* Ability = M_Unit->FindComponentByClass<UFlareAimAbilityComponent>();
	Check(IsValid(Ability), TEXT("The Blueprint carries a flare aim ability component"));
	if (not IsValid(Ability))
	{
		// Lets the logic be tested before the Blueprint is wired; the result still fails on the check above.
		Ability = NewObject<UFlareAimAbilityComponent>(M_Unit.Get(), TEXT("FlareAbilityTestComponent"));
		FFlareAimAbilitySettings& Settings = FFlareAbilityTestAccess::GetFlareSettings(*Ability);
		const FString Flare(FlarePath);
		Settings.Visuals.LauncherMesh = LoadObject<UStaticMesh>(nullptr, LauncherMeshPath);
		Settings.Visuals.FlareMesh = LoadObject<UStaticMesh>(nullptr, *(Flare + TEXT("SM_Flare_Cartridge")));
		Settings.Visuals.ParachuteMesh = LoadObject<UStaticMesh>(nullptr, *(Flare + TEXT("SM_Flare_Parachute")));
		Settings.Visuals.LaunchEffect = LoadObject<UNiagaraSystem>(nullptr, *(Flare + TEXT("NS_Flare_Launch")));
		Settings.Visuals.TrailEffect = LoadObject<UNiagaraSystem>(nullptr, *(Flare + TEXT("NS_Flare_Trail")));
		Settings.Visuals.BurnEffect = LoadObject<UNiagaraSystem>(nullptr, *(Flare + TEXT("NS_Flare_Burn")));
		M_Unit->AddInstanceComponent(Ability);
		Ability->RegisterComponent();
		UE_LOG(LogRTSFlareTest, Warning, TEXT("RTS_FLARE_TEST added a flare component at runtime"));
	}
	M_Ability = Ability;
	// Still in the spawn tick: the card entry is made next tick and takes this cooldown.
	FFlareAbilityTestAccess::GetAimSettings(*Ability).Cooldown = TestCooldownSeconds;
	FFlareAimAbilitySettings& Settings = FFlareAbilityTestAccess::GetFlareSettings(*Ability);
	M_OriginalFireRange = Settings.FireRange;
	Settings.FireRange = FMath::Min(Settings.FireRange, TestFireRange);
	UE_LOG(LogRTSFlareTest, Display,
	       TEXT("RTS_FLARE_TEST settings FireRange=%.0f (Blueprint %.0f) ApexHeight=%.0f HangTimeSeconds=%.1f "
		       "VisionRadius=%.0f Socket=%s"), Settings.FireRange, M_OriginalFireRange, Settings.ApexHeight,
	       Settings.HangTimeSeconds, Settings.VisionRadius, *Settings.LaunchSocketName.ToString());
}

const FUnitAbilityEntry* FFlareAbilityTestScenario::FindCardEntry(const EAbilityID AbilityId,
                                                                  TArray<FUnitAbilityEntry>& Entries) const
{
	ICommands* Commands = GetCommands();
	if (Commands == nullptr)
	{
		return nullptr;
	}
	Entries = Commands->GetUnitAbilityEntries();
	return Entries.FindByPredicate([AbilityId](const FUnitAbilityEntry& Entry)
	{
		return Entry.AbilityId == AbilityId && Entry.CustomType == static_cast<int32>(EAimAbilityType::FlareLaunch);
	});
}

bool FFlareAbilityTestScenario::GetHasCardEntry(const EAbilityID AbilityId) const
{
	TArray<FUnitAbilityEntry> Entries;
	return FindCardEntry(AbilityId, Entries) != nullptr;
}

int32 FFlareAbilityTestScenario::GetAimCooldownRemaining() const
{
	TArray<FUnitAbilityEntry> Entries;
	const FUnitAbilityEntry* Entry = FindCardEntry(EAbilityID::IdAimAbility, Entries);
	return Entry != nullptr ? Entry->CooldownRemaining : -1;
}

FVector FFlareAbilityTestScenario::GetLaunchSocketLocation() const
{
	USceneComponent* SocketComponent = FFlareAbilityTestAccess::FindLaunchSocketComponent(*M_Ability);
	if (not IsValid(SocketComponent))
	{
		return FVector(BIG_NUMBER);
	}
	return SocketComponent->GetSocketLocation(M_Ability->GetFlareSettings().LaunchSocketName);
}

float FFlareAbilityTestScenario::GetGroundZ(const FVector& Location) const
{
	FVector Ground = Location;
	FGrenadeTestHelpers::DropToGround(*GetWorld(), Ground);
	return Ground.Z;
}

AFlareProjectile* FFlareAbilityTestScenario::FindFlare() const
{
	for (TActorIterator<AFlareProjectile> It(GetWorld()); It; ++It)
	{
		if (It->GetOwner() == M_Unit.Get())
		{
			return *It;
		}
	}
	return nullptr;
}

bool FFlareAbilityTestScenario::FindOutOfRangeTarget(FVector& OutTarget) const
{
	using namespace FlareAbilityTestPrivate;
	const float Distance = M_Ability->GetFlareSettings().FireRange + OutOfRangeExtra;
	for (int32 Attempt = 0; Attempt < 24; ++Attempt)
	{
		const FVector Direction = FRotator(0.f, 15.f * Attempt, 0.f).Vector();
		FVector Candidate = M_Unit->GetActorLocation() + Direction * Distance;
		FGrenadeTestHelpers::DropToGround(*GetWorld(), Candidate);
		FVector OnNavMesh = Candidate;
		if (FGrenadeTestHelpers::ProjectOntoNavMesh(*GetWorld(), OnNavMesh, TargetNavExtent) &&
			FVector::Dist2D(OnNavMesh, M_Unit->GetActorLocation()) > M_Ability->GetFlareSettings().FireRange + 500.f)
		{
			OutTarget = OnNavMesh;
			return true;
		}
	}
	return false;
}

void FFlareAbilityTestScenario::TakePicture(const TCHAR* Name, const FVector& LookAt, const bool bSideView)
{
	using namespace FlareAbilityTestPrivate;
	if (not M_Capture.IsValid() || not M_RenderTarget.IsValid())
	{
		return;
	}
	if (bSideView)
	{
		const FVector Away = (M_Unit->GetActorLocation() - LookAt).GetSafeNormal2D();
		const FVector From = LookAt + Away * SideCameraDistance + FVector(0.f, 0.f, SideCameraHeight);
		M_Capture->SetActorLocationAndRotation(From, (LookAt - From).Rotation());
	}
	else
	{
		FGrenadeTestHelpers::AimLikeGameCamera(*M_Capture, LookAt);
	}
	// The capture renders every frame; the picture is taken from the frame rendered with the new view.
	M_Capture->GetCaptureComponent2D()->CaptureScene();
	FGrenadeTestHelpers::SaveCapture(*GetWorld(), *M_RenderTarget, M_PictureDirectory, FString(Name) + TEXT(".png"));
}

void FFlareAbilityTestScenario::TickWaitingForUnit()
{
	using namespace FlareAbilityTestPrivate;
	if (GetSecondsInPhase() < SettleSeconds)
	{
		return;
	}
	Check(GetHasCardEntry(EAbilityID::IdAimAbility), TEXT("The command card shows the flare (IdAimAbility, FlareLaunch)"));
	const FFlareAimAbilitySettings& Settings = M_Ability->GetFlareSettings();
	Check(FMath::IsNearlyEqual(M_Ability->GetAimAbilityRange(), Settings.FireRange),
	      TEXT("Aim range is FireRange"), FString::SanitizeFloat(M_Ability->GetAimAbilityRange()));
	Check(FMath::IsNearlyEqual(M_Ability->GetAimAbilityRadius(), Settings.VisionRadius),
	      TEXT("Aim preview radius is VisionRadius"), FString::SanitizeFloat(M_Ability->GetAimAbilityRadius()));
	Check(M_Ability->GetAimAbilityType() == EAimAbilityType::FlareLaunch, TEXT("Subtype is FlareLaunch"));
	Check(IsValid(FFlareAbilityTestAccess::FindLaunchSocketComponent(*M_Ability)),
	      TEXT("The launch socket exists on the turret"), Settings.LaunchSocketName.ToString());
	if (IsValid(Settings.Visuals.LauncherMesh))
	{
		const UStaticMeshComponent* Launcher = FFlareAbilityTestAccess::GetLauncherMesh(*M_Ability);
		const float FromSocket = IsValid(Launcher)
			                         ? FVector::Dist(Launcher->GetComponentLocation(), GetLaunchSocketLocation())
			                         : BIG_NUMBER;
		Check(FromSocket < SocketTolerance, TEXT("The discharger rack sits on the launch socket"),
		      FString::Printf(TEXT("%.1f cm"), FromSocket));
		Check(IsValid(Launcher) && Launcher->GetComponentScale().Equals(FVector::OneVector, 0.01f),
		      TEXT("The discharger rack has its modelled size"),
		      IsValid(Launcher) ? Launcher->GetComponentScale().ToString() : FString());
	}
	TakePicture(TEXT("00_unit"), M_Unit->GetActorLocation(), false);

	// First the cancel: order out of range, then cancel the move. No flare may leave.
	if (not FindOutOfRangeTarget(M_Target))
	{
		Check(false, TEXT("Found an out-of-range target on the navmesh"),
		      FString::Printf(TEXT("from %s, %.0f away"), *M_Unit->GetActorLocation().ToString(),
		                      M_Ability->GetFlareSettings().FireRange + OutOfRangeExtra));
		Finish(TEXT("no target"));
		return;
	}
	if (not OrderFlare(TEXT("cancel order")))
	{
		Finish(TEXT("order refused"));
		return;
	}
	EnterPhase(EFlareTestPhase::CancelWaitingForMove);
}

bool FFlareAbilityTestScenario::OrderFlare(const TCHAR* Label)
{
	M_StartDistanceToTarget = FVector::Dist2D(M_Unit->GetActorLocation(), M_Target);
	const ECommandQueueError Error = GetCommands()->AimAbility(M_Target, true, EAimAbilityType::FlareLaunch);
	Check(Error == ECommandQueueError::NoError, TEXT("The flare order is accepted"),
	      FString::Printf(TEXT("%s error=%d distance=%.0f"), Label, static_cast<int32>(Error), M_StartDistanceToTarget));
	return Error == ECommandQueueError::NoError;
}

void FFlareAbilityTestScenario::TickCancelWaitingForMove()
{
	using namespace FlareAbilityTestPrivate;
	const bool bShowsCancel = GetHasCardEntry(EAbilityID::IdCancelAimAbility);
	const float Distance = FVector::Dist2D(M_Unit->GetActorLocation(), M_Target);
	if (not bShowsCancel || Distance > M_StartDistanceToTarget - 150.f)
	{
		if (GetSecondsInPhase() > PhaseTimeoutSeconds)
		{
			Check(false, TEXT("Out of range the unit drives towards the target with the cancel button up"),
			      FString::Printf(TEXT("cancel=%d moved=%.0f"), bShowsCancel, M_StartDistanceToTarget - Distance));
			Finish(TEXT("no move to range"));
		}
		return;
	}
	Check(true, TEXT("Out of range the unit drives towards the target with the cancel button up"));
	// As the cancel button does without Shift.
	const ECommandQueueError Error = GetCommands()->CancelAimAbility(true, EAimAbilityType::FlareLaunch);
	Check(Error == ECommandQueueError::NoError, TEXT("The cancel button is accepted"),
	      FString::Printf(TEXT("error=%d"), static_cast<int32>(Error)));
	EnterPhase(EFlareTestPhase::CancelWaitingForCard);
}

void FFlareAbilityTestScenario::TickCancelWaitingForCard()
{
	using namespace FlareAbilityTestPrivate;
	if (GetSecondsInPhase() < 1.f)
	{
		return;
	}
	if (not bM_CheckedCancel)
	{
		bM_CheckedCancel = true;
		Check(GetHasCardEntry(EAbilityID::IdAimAbility) && not GetHasCardEntry(EAbilityID::IdCancelAimAbility),
		      TEXT("After the cancel the card shows the flare again"));
		Check(FindFlare() == nullptr, TEXT("A cancelled order launches no flare"));
		const float StoppedDistance = FVector::Dist2D(M_Unit->GetActorLocation(), M_Target);
		Check(StoppedDistance > M_Ability->GetFlareSettings().FireRange, TEXT("A cancelled order stops the drive"),
		      FString::Printf(TEXT("%.0f from the target"), StoppedDistance));
		UE_LOG(LogRTSFlareTest, Display, TEXT("RTS_FLARE_TEST cooldown after the cancel: %d s"),
		       GetAimCooldownRemaining());
	}
	if (GetAimCooldownRemaining() > 0 && GetSecondsInPhase() < CooldownWaitSeconds)
	{
		return;
	}

	if (OrderFlare(TEXT("real order")))
	{
		EnterPhase(EFlareTestPhase::OrderWaitingForLaunch);
		return;
	}
	Finish(TEXT("order refused"));
}

void FFlareAbilityTestScenario::TickOrderWaitingForLaunch()
{
	using namespace FlareAbilityTestPrivate;
	AFlareProjectile* Flare = FindFlare();
	if (not IsValid(Flare))
	{
		if (GetSecondsInPhase() > MoveTimeoutSeconds)
		{
			Check(false, TEXT("The flare is launched once in range"),
			      FString::Printf(TEXT("distance=%.0f"), FVector::Dist2D(M_Unit->GetActorLocation(), M_Target)));
			Finish(TEXT("no launch"));
		}
		return;
	}
	M_Flare = Flare;
	const FFlareAimAbilitySettings& Settings = M_Ability->GetFlareSettings();
	const float Distance = FVector::Dist(M_Unit->GetActorLocation(), M_Target);
	Check(Distance <= Settings.FireRange + 1.f, TEXT("Launched from within FireRange"),
	      FString::Printf(TEXT("distance=%.0f range=%.0f"), Distance, Settings.FireRange));
	Check(FFlareAbilityTestAccess::GetLastLaunchedFlare(*M_Ability) == Flare, TEXT("The component launched it"));
	const FVector Socket = GetLaunchSocketLocation();
	const float FromSocket = FVector::Dist(Flare->GetFlightPath().Start, Socket);
	Check(FromSocket < SocketTolerance, TEXT("The flare leaves the turret's launch socket"),
	      FString::Printf(TEXT("%.1f cm from %s"), FromSocket, *Socket.ToString()));
	Check(Flare->GetPhase() == EFlareProjectilePhase::Ascending, TEXT("The flare climbs"));
	Check(not FFlareAbilityTestAccess::GetIsGivingVision(*Flare), TEXT("No vision while climbing"));
	Check(GetHasCardEntry(EAbilityID::IdAimAbility) && not GetHasCardEntry(EAbilityID::IdCancelAimAbility),
	      TEXT("The card shows the flare again after the launch"));
	Check(GetAimCooldownRemaining() > 0, TEXT("The flare is on cooldown after the launch"),
	      FString::FromInt(GetAimCooldownRemaining()));
	ICommands* Commands = GetCommands();
	Check(Commands != nullptr && Commands->GetActiveCommandID() != EAbilityID::IdAimAbility,
	      TEXT("The command completed at the launch; the unit is free"),
	      Commands != nullptr ? Global_GetAbilityIDAsString(Commands->GetActiveCommandID()) : FString());
	TakePicture(TEXT("01_launch"), M_Unit->GetActorLocation(), false);
	EnterPhase(EFlareTestPhase::WaitingForIgnition);
}

void FFlareAbilityTestScenario::TickWaitingForIgnition()
{
	using namespace FlareAbilityTestPrivate;
	AFlareProjectile* Flare = M_Flare.Get();
	if (not IsValid(Flare))
	{
		Check(false, TEXT("The flare lives until it burns out"));
		Finish(TEXT("flare lost while climbing"));
		return;
	}
	if (not bM_TookClimbPicture && GetSecondsInPhase() > Flare->GetFlightPath().AscentSeconds * 0.5f)
	{
		bM_TookClimbPicture = true;
		TakePicture(TEXT("02_climb"), Flare->GetActorLocation(), true);
	}
	if (Flare->GetPhase() == EFlareProjectilePhase::Ascending)
	{
		if (GetSecondsInPhase() > PhaseTimeoutSeconds)
		{
			Check(false, TEXT("The flare ignites at the end of its climb"));
			Finish(TEXT("no ignition"));
		}
		return;
	}
	const FFlareAimAbilitySettings& Settings = M_Ability->GetFlareSettings();
	const float Height = Flare->GetActorLocation().Z - GetGroundZ(M_Target);
	Check(FMath::Abs(Height - Settings.ApexHeight) < ApexTolerance, TEXT("Ignites at ApexHeight above the target"),
	      FString::Printf(TEXT("height=%.0f wanted=%.0f"), Height, Settings.ApexHeight));
	Check(FVector::Dist2D(Flare->GetActorLocation(), M_Target) < 50.f, TEXT("Ignites above the target"),
	      FString::Printf(TEXT("%.0f cm off"), FVector::Dist2D(Flare->GetActorLocation(), M_Target)));
	Check(GetSecondsInPhase() <= Flare->GetFlightPath().AscentSeconds + 0.5f, TEXT("Climbs in its climb time"),
	      FString::Printf(TEXT("%.2f s, path %.2f s"), GetSecondsInPhase(), Flare->GetFlightPath().AscentSeconds));

	UFowComp* FowComponent = FFlareAbilityTestAccess::GetFowComponent(*Flare);
	Check(FowComponent->GetFowBehaviour() == EFowBehaviour::Fow_Active, TEXT("Gives the player a vision bubble"));
	Check(FMath::IsNearlyEqual(FowComponent->GetVisionRadius(), Settings.VisionRadius),
	      TEXT("Vision bubble has VisionRadius"), FString::SanitizeFloat(FowComponent->GetVisionRadius()));
	AFowManager* FowManager = FRTS_Statics::GetFowManager(GetWorld());
	Check(IsValid(FowManager) && FowManager->GetIsFowParticipant(FowComponent),
	      TEXT("The fog of war manager has the vision bubble"));
	M_IgnitionSeconds = GetSeconds();
	M_LastFlareZ = Flare->GetActorLocation().Z;
	EnterPhase(EFlareTestPhase::WaitingForBurnOut);
}

void FFlareAbilityTestScenario::TickWaitingForBurnOut()
{
	using namespace FlareAbilityTestPrivate;
	AFlareProjectile* Flare = M_Flare.Get();
	if (not IsValid(Flare))
	{
		Check(false, TEXT("The flare lives until it burns out"));
		Finish(TEXT("flare lost while burning"));
		return;
	}
	const FFlareAimAbilitySettings& Settings = M_Ability->GetFlareSettings();
	const float BurnSeconds = GetSeconds() - M_IgnitionSeconds;
	if (Flare->GetPhase() == EFlareProjectilePhase::Burning)
	{
		bM_FlareOnlySank &= Flare->GetActorLocation().Z <= M_LastFlareZ + 0.1f;
		M_LastFlareZ = Flare->GetActorLocation().Z;
		if (BurnSeconds > 0.7f && GetSecondsInPhase() < 1.f)
		{
			TakePicture(TEXT("03_burning_game_camera"), M_Target, false);
			TakePicture(TEXT("04_burning_side"), Flare->GetActorLocation(), true);
		}
		if (not bM_TookHalfwayPicture && BurnSeconds > Settings.HangTimeSeconds * 0.5f)
		{
			bM_TookHalfwayPicture = true;
			AFowManager* FowManager = FRTS_Statics::GetFowManager(GetWorld());
			Check(IsValid(FowManager) && FowManager->GetIsFowParticipant(FFlareAbilityTestAccess::GetFowComponent(*Flare)),
			      TEXT("Vision lasts while the flare sinks"));
			const float Height = Flare->GetActorLocation().Z - GetGroundZ(M_Target);
			Check(FMath::Abs(Height - Settings.ApexHeight * 0.5f) < ApexTolerance + 40.f,
			      TEXT("Halfway through its time it is halfway down"),
			      FString::Printf(TEXT("height=%.0f"), Height));
			TakePicture(TEXT("05_halfway_side"), Flare->GetActorLocation(), true);
		}
		if (BurnSeconds > Settings.HangTimeSeconds + PhaseTimeoutSeconds)
		{
			Check(false, TEXT("The flare burns out after HangTimeSeconds"));
			Finish(TEXT("no burn-out"));
		}
		return;
	}
	Check(bM_FlareOnlySank, TEXT("The flare only sinks under its parachute"));
	Check(FMath::Abs(BurnSeconds - Settings.HangTimeSeconds) < HangToleranceSeconds,
	      TEXT("Gives vision for HangTimeSeconds"),
	      FString::Printf(TEXT("%.2f s, wanted %.2f s"), BurnSeconds, Settings.HangTimeSeconds));
	const float Height = Flare->GetActorLocation().Z - GetGroundZ(M_Target);
	Check(Height < 60.f, TEXT("Burns out on the ground"), FString::Printf(TEXT("height=%.0f"), Height));
	AFowManager* FowManager = FRTS_Statics::GetFowManager(GetWorld());
	Check(IsValid(FowManager) && not FowManager->GetIsFowParticipant(FFlareAbilityTestAccess::GetFowComponent(*Flare)),
	      TEXT("The vision bubble is gone after the burn-out"));
	TakePicture(TEXT("06_burnt_out"), M_Target, false);
	EnterPhase(EFlareTestPhase::WaitingForDestroy);
}

void FFlareAbilityTestScenario::TickWaitingForDestroy()
{
	if (M_Flare.IsValid() && GetSecondsInPhase() < 10.f)
	{
		return;
	}
	Check(not M_Flare.IsValid(), TEXT("The burnt-out flare removes itself"));
	Finish(TEXT("done"));
}

namespace FlareAbilityTestPrivate
{
	TUniquePtr<FFlareAbilityTestScenario> GScenario;
	bool bGHasStarted = false;
	constexpr float PollSeconds = 0.f;

	UWorld* FindGameWorld()
	{
		if (not GEngine)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (IsValid(World) && World->IsGameWorld())
			{
				return World;
			}
		}
		return nullptr;
	}

	// The startup map is loaded during engine init, before this registers, so the game world is polled for.
	bool TickScenario(float)
	{
		if (not GScenario.IsValid())
		{
			UWorld* World = bGHasStarted ? nullptr : FindGameWorld();
			if (not IsValid(World))
			{
				return not bGHasStarted;
			}
			bGHasStarted = true;
			UE_LOG(LogRTSFlareTest, Display, TEXT("RTS_FLARE_TEST started on %s"), *World->GetMapName());
			GScenario = MakeUnique<FFlareAbilityTestScenario>(*World);
		}
		if (not GScenario->Tick())
		{
			GScenario.Reset();
			return false;
		}
		return true;
	}

	FDelayedAutoRegisterHelper GRegisterFlareAbilityTest(EDelayedRegisterRunPhase::EndOfEngineInit, []()
	{
		if (FParse::Param(FCommandLine::Get(), StartSwitch))
		{
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&TickScenario), PollSeconds);
		}
	});
}

#endif
