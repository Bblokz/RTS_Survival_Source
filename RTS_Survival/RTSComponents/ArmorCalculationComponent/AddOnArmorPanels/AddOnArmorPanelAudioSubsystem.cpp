// Copyright (C) Bas Blokzijl - All rights reserved.

#include "AddOnArmorPanelAudioSubsystem.h"

#include "Components/AudioComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "RTS_Survival/Audio/Pooling/RTSPooledAudio.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "Sound/SoundBase.h"

// -LogCmds="LogAddOnArmorAudio Verbose" logs every armour-off sound with its pool entry.
DEFINE_LOG_CATEGORY_STATIC(LogAddOnArmorAudio, Log, All);

#if not UE_BUILD_SHIPPING
namespace AddOnArmorPanelAudioDebug
{
	// RTS.AddOnArmor.TestArmorOffAudio [Count=20] [Weight=0|1|2]: plays Count armour-off sounds at once at the camera,
	// to check the pool hands out its entries and then reuses the oldest. Pair with -LogCmds="LogAddOnArmorAudio Verbose".
	static FAutoConsoleCommandWithWorldAndArgs GTestArmorOffAudioCommand(
		TEXT("RTS.AddOnArmor.TestArmorOffAudio"),
		TEXT("Plays [Count=20] armour-off sounds of [Weight=0 light, 1 medium, 2 heavy] at the camera at once."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UAddOnArmorPanelAudioSubsystem* const Subsystem = IsValid(World)
				? World->GetSubsystem<UAddOnArmorPanelAudioSubsystem>()
				: nullptr;
			if (not IsValid(Subsystem))
			{
				return;
			}
			const int32 Count = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 20;
			const int32 WeightIndex = Args.Num() > 1 ? FMath::Clamp(FCString::Atoi(*Args[1]), 0, 2) : 0;
			const APlayerController* const PlayerController = World->GetFirstPlayerController();
			const FVector Location = IsValid(PlayerController) && IsValid(PlayerController->PlayerCameraManager)
				? PlayerController->PlayerCameraManager->GetCameraLocation()
				: FVector::ZeroVector;
			for (int32 Index = 0; Index < Count; ++Index)
			{
				Subsystem->PlayArmorOffSound(static_cast<EAddOnArmorOffSoundWeight>(WeightIndex), Location);
			}
		}));
}
#endif

bool UAddOnArmorPanelAudioSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld();
}

void UAddOnArmorPanelAudioSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	LoadSounds();
	SpawnPool(InWorld);
}

void UAddOnArmorPanelAudioSubsystem::Deinitialize()
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
	if (IsValid(M_PoolOwner))
	{
		M_PoolOwner->Destroy();
	}
	M_PoolOwner = nullptr;
	M_Sounds.Empty();
	Super::Deinitialize();
}

void UAddOnArmorPanelAudioSubsystem::PlayArmorOffSound(const EAddOnArmorOffSoundWeight Weight,
                                                       const FVector& WorldLocation)
{
	const int32 SoundIndex = static_cast<int32>(Weight);
	USoundBase* const Sound = M_Sounds.IsValidIndex(SoundIndex) ? M_Sounds[SoundIndex].Get() : nullptr;
	// A missing cue was reported once when the sounds loaded.
	if (not IsValid(Sound))
	{
		return;
	}
	const int32 PoolIndex = AcquirePoolIndex();
	if (PoolIndex == INDEX_NONE)
	{
		return;
	}
	UAudioComponent* const Component = M_PoolComponents[PoolIndex];
	const bool bWasStolen = Component->IsPlaying();
	Component->Stop();
	Component->SetSound(Sound);
	Component->SetWorldLocation(WorldLocation);
	Component->Play();
	M_PoolPlayOrders[PoolIndex] = M_NextPlayOrder++;
	UE_LOG(LogAddOnArmorAudio, Verbose, TEXT("%s at %s on pool entry %d%s"), *GetNameSafe(Sound),
	       *WorldLocation.ToCompactString(), PoolIndex, bWasStolen ? TEXT(" (reused the oldest playing entry)") : TEXT(""));
}

EAddOnArmorOffSoundWeight UAddOnArmorPanelAudioSubsystem::GetSoundWeightForTank(const ETankSubtype TankSubtype)
{
	// Tank destroyers are listed under their weight class as well, so the weight checks cover them.
	if (Global_GetIsHeavyTank(TankSubtype))
	{
		return EAddOnArmorOffSoundWeight::Heavy;
	}
	if (Global_GetIsLightTank(TankSubtype) || Global_GetIsArmoredCar(TankSubtype))
	{
		return EAddOnArmorOffSoundWeight::Light;
	}
	return EAddOnArmorOffSoundWeight::Medium;
}

void UAddOnArmorPanelAudioSubsystem::LoadSounds()
{
	using namespace AddOnArmorPanelAudio;
	// Same order as EAddOnArmorOffSoundWeight.
	const TCHAR* const SoundPaths[] = {LightSoundPath, MediumSoundPath, HeavySoundPath};
	M_Sounds.Reset();
	for (const TCHAR* const SoundPath : SoundPaths)
	{
		USoundBase* const Sound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(SoundPath)).LoadSynchronous();
		if (not IsValid(Sound))
		{
			RTSFunctionLibrary::ReportError(TEXT("UAddOnArmorPanelAudioSubsystem: could not load the armour-off sound ")
				+ FString(SoundPath) + TEXT("; panels shot off that weight class make no sound."));
		}
		M_Sounds.Add(Sound);
	}
}

void UAddOnArmorPanelAudioSubsystem::SpawnPool(UWorld& World)
{
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Name = MakeUniqueObjectName(&World, ARTSPooledAudio::StaticClass(), TEXT("AddOnArmorOffAudioPool"));
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParameters.ObjectFlags |= RF_Transient;
	M_PoolOwner = World.SpawnActor<ARTSPooledAudio>(ARTSPooledAudio::StaticClass(), FTransform::Identity,
	                                                 SpawnParameters);
	if (not GetIsValidPoolOwner())
	{
		return;
	}
	const int32 PoolSize = DeveloperSettings::GamePlay::Audio::AddOnArmorOffAudioPoolSize;
	USceneComponent* const Root = M_PoolOwner->GetRootComponent();
	M_PoolComponents.Reset(PoolSize);
	M_PoolPlayOrders.Reset(PoolSize);
	for (int32 PoolIndex = 0; PoolIndex < PoolSize; ++PoolIndex)
	{
		UAudioComponent* const Component = NewObject<UAudioComponent>(M_PoolOwner);
		if (not IsValid(Component))
		{
			RTSFunctionLibrary::ReportError(FString::Printf(
				TEXT("UAddOnArmorPanelAudioSubsystem: failed to create the audio component for pool entry %d."),
				PoolIndex));
			continue;
		}
		Component->bAutoActivate = false;
		Component->bAutoDestroy = false;
		Component->bStopWhenOwnerDestroyed = true;
		Component->SetUISound(false);
		if (IsValid(Root))
		{
			Component->SetupAttachment(Root);
		}
		Component->RegisterComponent();
		M_PoolComponents.Add(Component);
		M_PoolPlayOrders.Add(0);
	}
}

bool UAddOnArmorPanelAudioSubsystem::GetIsValidPoolOwner() const
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

int32 UAddOnArmorPanelAudioSubsystem::AcquirePoolIndex() const
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
