// Copyright (C) Bas Blokzijl - All rights reserved.

#include "BulletFlyByAudioSubsystem.h"

#include "BulletFlyByDataAsset.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "RTS_Survival/Audio/Pooling/RTSPooledAudio.h"
#include "RTS_Survival/Audio/Settings/RTSAudioDeveloperSettings.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundConcurrency.h"

// -LogCmds="LogBulletFlyBy Verbose" logs every scheduled, dropped and played fly-by.
DEFINE_LOG_CATEGORY_STATIC(LogBulletFlyBy, Log, All);

namespace BulletFlyByAudio
{
	// Fly-bys waiting for their tracer; tracers live under a second, so a few cover any fight.
	constexpr int32 MaxPendingFlyBys = 8;
	// A tracer reported this much later than its pass (the trace result arrives a frame or so after the shot) still
	// plays at once; later ones have passed and are dropped.
	constexpr double MaxLateSeconds = 0.1;

	static TAutoConsoleVariable<int32> CVarBulletFlyByEnabled(
		TEXT("RTS.Audio.BulletFlyBy"),
		1,
		TEXT("1 plays bullet fly-by sounds near the camera focus, 0 turns them off (for A/B listening)."),
		ECVF_Default);
}

#if not UE_BUILD_SHIPPING
namespace BulletFlyByAudioDebug
{
	// RTS.Audio.TestBulletFlyBy [Count=5] [Offset=200]: fires Count fake tracers across the camera focus, Offset cm to
	// the side of it and 0.15 s apart in launch time, to hear the sounds and the budget. Pair with
	// -LogCmds="LogBulletFlyBy Verbose".
	static FAutoConsoleCommandWithWorldAndArgs GTestBulletFlyByCommand(
		TEXT("RTS.Audio.TestBulletFlyBy"),
		TEXT("Fires [Count=5] fake tracers [Offset=200] cm beside the camera focus to play bullet fly-bys."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UBulletFlyByAudioSubsystem* const Subsystem = IsValid(World)
				? World->GetSubsystem<UBulletFlyByAudioSubsystem>()
				: nullptr;
			const APlayerController* const PlayerController = IsValid(World) ? World->GetFirstPlayerController() : nullptr;
			const APawn* const Pawn = IsValid(PlayerController) ? PlayerController->GetPawn() : nullptr;
			if (not IsValid(Subsystem) || not IsValid(Pawn))
			{
				return;
			}
			const int32 Count = Args.Num() > 0 ? FMath::Max(1, FCString::Atoi(*Args[0])) : 5;
			const float Offset = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 200.f;
			const FVector Focus = Pawn->GetActorLocation();
			const float Now = World->GetTimeSeconds();
			for (int32 Index = 0; Index < Count; ++Index)
			{
				// Alternate the side the shot comes from so the panning is audible.
				const float Side = Index % 2 == 0 ? 1.f : -1.f;
				const FVector Start = Focus + FVector(-2000.f * Side, Offset, 100.f);
				const FVector End = Focus + FVector(2000.f * Side, Offset, 100.f);
				Subsystem->OnTracerFired(Start, End, Now + 0.15f * Index);
			}
		}));
}
#endif

bool UBulletFlyByAudioSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld();
}

void UBulletFlyByAudioSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	LoadData();
	if (not IsValid(M_Data))
	{
		return;
	}
	SpawnPool(InWorld);
	M_PendingFlyBys.Reserve(BulletFlyByAudio::MaxPendingFlyBys);
	M_FlyByTokens = static_cast<float>(M_Data->MaxSimultaneousFlyBys);
	M_LastTokenRefillTime = InWorld.GetTimeSeconds();
}

void UBulletFlyByAudioSubsystem::Deinitialize()
{
	for (UAudioComponent* Component : M_PoolComponents)
	{
		if (IsValid(Component))
		{
			Component->Stop();
		}
	}
	M_PoolComponents.Empty();
	M_PoolPlayOrders.Empty();
	M_PendingFlyBys.Empty();
	if (IsValid(M_PoolOwner))
	{
		M_PoolOwner->Destroy();
	}
	M_PoolOwner = nullptr;
	M_Data = nullptr;
	Super::Deinitialize();
}

bool UBulletFlyByAudioSubsystem::IsTickable() const
{
	// Ticks only while a fly-by waits for its tracer; a quiet battlefield costs nothing.
	return not M_PendingFlyBys.IsEmpty();
}

TStatId UBulletFlyByAudioSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UBulletFlyByAudioSubsystem, STATGROUP_Tickables);
}

void UBulletFlyByAudioSubsystem::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
	const UWorld* const World = GetWorld();
	if (not IsValid(World))
	{
		M_PendingFlyBys.Reset();
		return;
	}
	const double Now = World->GetTimeSeconds();
	for (int32 Index = M_PendingFlyBys.Num() - 1; Index >= 0; --Index)
	{
		if (M_PendingFlyBys[Index].PlayTime > Now)
		{
			continue;
		}
		PlayFlyBy(M_PendingFlyBys[Index]);
		M_PendingFlyBys.RemoveAtSwap(Index, EAllowShrinking::No);
	}
}

void UBulletFlyByAudioSubsystem::OnTracerFired(const FVector& LaunchLocation, const FVector& EndLocation,
                                               const float LaunchTime)
{
	if (not IsValid(M_Data) || M_PoolComponents.IsEmpty()
		|| BulletFlyByAudio::CVarBulletFlyByEnabled.GetValueOnGameThread() == 0)
	{
		return;
	}
	if (not UpdateEar() || M_CameraDistance > M_Data->MaxCameraDistance)
	{
		return;
	}
	// Closest pass measured flat: the camera pawn keeps one height over the whole map, the bullets fly near the ground.
	const FVector2D Start(LaunchLocation);
	const FVector2D Path = FVector2D(EndLocation) - Start;
	const double PathLengthSquared = Path.SizeSquared();
	if (PathLengthSquared < 1.0)
	{
		return;
	}
	const double PathAlpha = FMath::Clamp(
		FVector2D::DotProduct(FVector2D(M_EarLocation) - Start, Path) / PathLengthSquared, 0.0, 1.0);
	const double PassDistanceSquared = FVector2D::DistSquared(Start + Path * PathAlpha, FVector2D(M_EarLocation));
	if (PassDistanceSquared > FMath::Square(M_Data->PassRadius))
	{
		return;
	}
	const double PathLength = FMath::Sqrt(PathLengthSquared);
	if (PathAlpha * PathLength < M_Data->MinDistanceFromMuzzle
		|| (1.0 - PathAlpha) * PathLength < M_Data->MinDistanceFromImpact)
	{
		return;
	}
	const UWorld* const World = GetWorld();
	const double Now = World->GetTimeSeconds();
	FPendingFlyBy FlyBy;
	FlyBy.Location = FMath::Lerp(LaunchLocation, EndLocation, PathAlpha);
	// When the tracer drawn by the projectile manager gets to the pass, so the whiz lines up with what is seen.
	FlyBy.PlayTime = LaunchTime + FVector::Dist(LaunchLocation, FlyBy.Location)
		/ DeveloperSettings::GamePlay::Projectile::SmallArmsProjectileSpeed;
	if (FlyBy.PlayTime < Now - BulletFlyByAudio::MaxLateSeconds)
	{
		return;
	}
	const float Closeness = 1.f - FMath::Sqrt(static_cast<float>(PassDistanceSquared)) / M_Data->PassRadius;
	FlyBy.Volume = FMath::Lerp(M_Data->VolumeAtPassRadius, M_Data->VolumeAtClosestPass, Closeness)
		* GetZoomVolumeFactor();
	if (FlyBy.Volume <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	SchedulePendingFlyBy(FlyBy);
}

void UBulletFlyByAudioSubsystem::LoadData()
{
	const URTSAudioDeveloperSettings* const Settings = GetDefault<URTSAudioDeveloperSettings>();
	if (Settings == nullptr || Settings->BulletFlyByData.IsNull())
	{
		RTSFunctionLibrary::ReportError(TEXT("UBulletFlyByAudioSubsystem: no Bullet Fly By Data set in Project "
			"Settings > Game > RTS Audio Settings; bullets make no fly-by sound."));
		return;
	}
	M_Data = Settings->BulletFlyByData.LoadSynchronous();
	if (not IsValid(M_Data))
	{
		RTSFunctionLibrary::ReportError(TEXT("UBulletFlyByAudioSubsystem: could not load the Bullet Fly By Data ")
			+ Settings->BulletFlyByData.ToString() + TEXT("; bullets make no fly-by sound."));
		return;
	}
	const bool bHasSound = M_Data->FlyBySounds.ContainsByPredicate([](const TObjectPtr<USoundBase>& Sound)
	{
		return IsValid(Sound);
	});
	if (not bHasSound)
	{
		RTSFunctionLibrary::ReportError(TEXT("UBulletFlyByAudioSubsystem: ") + M_Data->GetName()
			+ TEXT(" has no fly-by sounds; bullets make no fly-by sound."));
		M_Data = nullptr;
	}
}

void UBulletFlyByAudioSubsystem::SpawnPool(UWorld& World)
{
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Name = MakeUniqueObjectName(&World, ARTSPooledAudio::StaticClass(), TEXT("BulletFlyByAudioPool"));
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParameters.ObjectFlags |= RF_Transient;
	M_PoolOwner = World.SpawnActor<ARTSPooledAudio>(ARTSPooledAudio::StaticClass(), FTransform::Identity,
	                                                 SpawnParameters);
	if (not GetIsValidPoolOwner())
	{
		return;
	}
	const int32 PoolSize = M_Data->MaxSimultaneousFlyBys;
	USceneComponent* const Root = M_PoolOwner->GetRootComponent();
	M_PoolComponents.Reset(PoolSize);
	M_PoolPlayOrders.Reset(PoolSize);
	for (int32 PoolIndex = 0; PoolIndex < PoolSize; ++PoolIndex)
	{
		UAudioComponent* const Component = NewObject<UAudioComponent>(M_PoolOwner);
		if (not IsValid(Component))
		{
			RTSFunctionLibrary::ReportError(FString::Printf(
				TEXT("UBulletFlyByAudioSubsystem: failed to create the audio component for pool entry %d."), PoolIndex));
			continue;
		}
		Component->bAutoActivate = false;
		Component->bAutoDestroy = false;
		Component->bStopWhenOwnerDestroyed = true;
		Component->bAllowSpatialization = true;
		Component->SetUISound(false);
		// Set once here: the sounds are swapped per fly-by, the class, attenuation and concurrency stay.
		Component->SoundClassOverride = M_Data->SoundClass;
		Component->AttenuationSettings = M_Data->Attenuation;
		if (IsValid(M_Data->Concurrency))
		{
			Component->ConcurrencySet.Add(M_Data->Concurrency);
		}
		Component->bOverridePriority = true;
		Component->Priority = M_Data->Priority;
		if (IsValid(Root))
		{
			Component->SetupAttachment(Root);
		}
		Component->RegisterComponent();
		M_PoolComponents.Add(Component);
		M_PoolPlayOrders.Add(0);
	}
}

bool UBulletFlyByAudioSubsystem::GetIsValidPoolOwner() const
{
	if (IsValid(M_PoolOwner))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_PoolOwner",
		"GetIsValidPoolOwner",
		this);
	return false;
}

bool UBulletFlyByAudioSubsystem::UpdateEar()
{
	if (M_EarFrame == GFrameCounter)
	{
		return bM_HasEar;
	}
	M_EarFrame = GFrameCounter;
	bM_HasEar = false;
	const UWorld* const World = GetWorld();
	const APlayerController* const PlayerController = IsValid(World) ? World->GetFirstPlayerController() : nullptr;
	if (not IsValid(PlayerController) || not IsValid(PlayerController->PlayerCameraManager))
	{
		return false;
	}
	const APawn* const Pawn = PlayerController->GetPawn();
	// Only while the player looks through the RTS camera pawn; cinematic view targets get no fly-bys.
	if (not IsValid(Pawn) || PlayerController->GetViewTarget() != Pawn)
	{
		return false;
	}
	M_EarLocation = Pawn->GetActorLocation();
	M_CameraDistance = FVector::Dist(PlayerController->PlayerCameraManager->GetCameraLocation(), M_EarLocation);
	bM_HasEar = true;
	return true;
}

float UBulletFlyByAudioSubsystem::GetZoomVolumeFactor() const
{
	if (M_CameraDistance <= M_Data->FullVolumeCameraDistance)
	{
		return 1.f;
	}
	const float FadeRange = M_Data->MaxCameraDistance - M_Data->FullVolumeCameraDistance;
	if (FadeRange <= KINDA_SMALL_NUMBER)
	{
		return 0.f;
	}
	return FMath::Clamp(1.f - (M_CameraDistance - M_Data->FullVolumeCameraDistance) / FadeRange, 0.f, 1.f);
}

void UBulletFlyByAudioSubsystem::RefillTokens(const double Now)
{
	const double Elapsed = FMath::Max(0.0, Now - M_LastTokenRefillTime);
	M_LastTokenRefillTime = Now;
	M_FlyByTokens = FMath::Min(static_cast<float>(M_Data->MaxSimultaneousFlyBys),
	                           M_FlyByTokens + static_cast<float>(Elapsed) * M_Data->MaxFlyBysPerSecond);
}

void UBulletFlyByAudioSubsystem::SchedulePendingFlyBy(const FPendingFlyBy& FlyBy)
{
	const double MinGap = M_Data->MinSecondsBetweenFlyBys;
	if (FlyBy.PlayTime - M_LastFlyByStartTime < MinGap)
	{
		UE_LOG(LogBulletFlyBy, Verbose, TEXT("Dropped: within %.2f s of the last fly-by."), MinGap);
		return;
	}
	// Two fly-bys close together blur into one; keep the louder.
	for (FPendingFlyBy& Pending : M_PendingFlyBys)
	{
		if (FMath::Abs(Pending.PlayTime - FlyBy.PlayTime) >= MinGap)
		{
			continue;
		}
		if (FlyBy.Volume > Pending.Volume)
		{
			Pending = FlyBy;
			UE_LOG(LogBulletFlyBy, Verbose, TEXT("Replaced a quieter pending fly-by (volume %.2f)."), FlyBy.Volume);
		}
		return;
	}
	RefillTokens(GetWorld()->GetTimeSeconds());
	if (M_FlyByTokens < 1.f || M_PendingFlyBys.Num() >= BulletFlyByAudio::MaxPendingFlyBys)
	{
		UE_LOG(LogBulletFlyBy, Verbose, TEXT("Dropped: over budget (%.2f tokens, %d pending)."), M_FlyByTokens,
		       M_PendingFlyBys.Num());
		return;
	}
	M_FlyByTokens -= 1.f;
	M_PendingFlyBys.Add(FlyBy);
	UE_LOG(LogBulletFlyBy, Verbose, TEXT("Scheduled at %s in %.2f s, volume %.2f, camera distance %.0f."),
	       *FlyBy.Location.ToCompactString(), FlyBy.PlayTime - GetWorld()->GetTimeSeconds(), FlyBy.Volume,
	       M_CameraDistance);
}

void UBulletFlyByAudioSubsystem::PlayFlyBy(const FPendingFlyBy& FlyBy)
{
	const int32 SoundIndex = PickSoundIndex();
	const int32 PoolIndex = AcquirePoolIndex();
	if (SoundIndex == INDEX_NONE || PoolIndex == INDEX_NONE)
	{
		return;
	}
	UAudioComponent* const Component = M_PoolComponents[PoolIndex];
	Component->Stop();
	Component->SetSound(M_Data->FlyBySounds[SoundIndex]);
	Component->SetWorldLocation(FlyBy.Location);
	Component->SetVolumeMultiplier(FlyBy.Volume);
	Component->SetPitchMultiplier(FMath::FRandRange(M_Data->PitchRange.X, M_Data->PitchRange.Y));
	Component->Play();
	M_PoolPlayOrders[PoolIndex] = M_NextPlayOrder++;
	M_LastFlyByStartTime = GetWorld()->GetTimeSeconds();
	UE_LOG(LogBulletFlyBy, Verbose, TEXT("Played %s at %s, volume %.2f, pool entry %d."),
	       *GetNameSafe(M_Data->FlyBySounds[SoundIndex]), *FlyBy.Location.ToCompactString(), FlyBy.Volume, PoolIndex);
}

int32 UBulletFlyByAudioSubsystem::PickSoundIndex()
{
	const int32 SoundCount = M_Data->FlyBySounds.Num();
	if (SoundCount == 0)
	{
		return INDEX_NONE;
	}
	// A few tries skip empty entries and the sound played last; with one sound that sound repeats.
	int32 Picked = INDEX_NONE;
	for (int32 Attempt = 0; Attempt < 4; ++Attempt)
	{
		const int32 Candidate = FMath::RandRange(0, SoundCount - 1);
		if (not IsValid(M_Data->FlyBySounds[Candidate]))
		{
			continue;
		}
		Picked = Candidate;
		if (Candidate != M_LastSoundIndex)
		{
			break;
		}
	}
	if (Picked == INDEX_NONE)
	{
		Picked = M_Data->FlyBySounds.IndexOfByPredicate([](const TObjectPtr<USoundBase>& Sound)
		{
			return IsValid(Sound);
		});
	}
	M_LastSoundIndex = Picked;
	return Picked;
}

int32 UBulletFlyByAudioSubsystem::AcquirePoolIndex() const
{
	int32 OldestIndex = INDEX_NONE;
	uint64 OldestPlayOrder = TNumericLimits<uint64>::Max();
	for (int32 PoolIndex = 0; PoolIndex < M_PoolComponents.Num(); ++PoolIndex)
	{
		const UAudioComponent* const Component = M_PoolComponents[PoolIndex];
		if (not IsValid(Component))
		{
			continue;
		}
		if (not Component->IsPlaying())
		{
			return PoolIndex;
		}
		if (M_PoolPlayOrders[PoolIndex] < OldestPlayOrder)
		{
			OldestIndex = PoolIndex;
			OldestPlayOrder = M_PoolPlayOrders[PoolIndex];
		}
	}
	return OldestIndex;
}
