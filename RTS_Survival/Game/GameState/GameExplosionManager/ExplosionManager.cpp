#include "ExplosionManager.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"  // Required for SetTimer
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundCue.h"

UGameExplosionsManager::UGameExplosionsManager()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UGameExplosionsManager::InitExplosionManager(
	TArray<FExplosionTypeSystems> ExplSystemsData,
	USoundAttenuation* ExplSoundAtt,
	USoundConcurrency* ExplSoundConc)
{
	// Clear any existing data in the internal TMap
	M_ExplosionSystemsMap.Empty();
	M_ExplSoundsMap.Empty();


	M_SoundAttenuation = ExplSoundAtt;
	M_SoundConcurrency = ExplSoundConc;
	// Build the TMap from ExplosionSystemsData array
	for (const FExplosionTypeSystems& Entry : ExplSystemsData)
	{
		TArray<UNiagaraSystem*>& SystemsForType = M_ExplosionSystemsMap.FindOrAdd(Entry.ExplosionType);
		for (UNiagaraSystem* NiagaraSys : Entry.Systems)
		{
			if (NiagaraSys)
			{
				SystemsForType.AddUnique(NiagaraSys);
			}
		}
		TArray<USoundCue*>& SoundsForType = M_ExplSoundsMap.FindOrAdd(Entry.ExplosionType);
		for (USoundCue* SoundCue : Entry.SoundCues)
		{
			if (SoundCue)
			{
				SoundsForType.AddUnique(SoundCue);
			}
		}
	}
}

void UGameExplosionsManager::SpawnRTSExplosionAtLocation(ERTS_ExplosionType ExplosionType, const FVector& SpawnLocation,
                                                         const bool bPlaySound, float Delay /*= 0.f*/)
{
	// If the delay is nearly zero, spawn immediately.
	if (FMath::IsNearlyZero(Delay))
	{
		DelayedExplosionSpawn(ExplosionType, SpawnLocation, bPlaySound);
		return;
	}

	// Otherwise, schedule a timer to spawn later.
	if (UWorld* World = GetWorld())
	{
		TWeakObjectPtr<UGameExplosionsManager> WeakThis(this);
		// We'll bind a delegate to call DelayedExplosionSpawn with the correct parameters
		FTimerDelegate TimerDel;
		TimerDel.BindLambda([WeakThis, ExplosionType, SpawnLocation, bPlaySound]()
		{
			if (WeakThis.IsValid())
			{
				UGameExplosionsManager* StrongThis = WeakThis.Get();
				StrongThis->DelayedExplosionSpawn(ExplosionType, SpawnLocation, bPlaySound);
			}
		});

		FTimerHandle TimerHandle;
		// Schedule a one-shot timer that calls DelayedExplosionSpawn after 'Delay' seconds
		World->GetTimerManager().SetTimer(
			TimerHandle,
			TimerDel,
			Delay,
			/*bLoop=*/ false
		);
	}
	else
	{
		RTSFunctionLibrary::ReportError(
			TEXT("[UGameExplosionsManager] SpawnRTSExplosionAtLocation failed, no valid World."));
	}
}

void UGameExplosionsManager::SpawnExplosionAtLocationCustomSound(ERTS_ExplosionType ExplosionType,
                                                                 const FVector& SpawnLocation, USoundCue* CustomSound,
                                                                 float Delay)
{
	// If the delay is nearly zero, spawn immediately.
	if (FMath::IsNearlyZero(Delay))
	{
		DelayedExplosionSpawnCustomSound(ExplosionType, SpawnLocation, CustomSound);
		return;
	}

	// Otherwise, schedule a timer to spawn later.
	if (UWorld* World = GetWorld())
	{
		TWeakObjectPtr<UGameExplosionsManager> WeakThis(this);
		// We'll bind a delegate to call DelayedExplosionSpawn with the correct parameters
		FTimerDelegate TimerDel;
		TimerDel.BindLambda([WeakThis, ExplosionType, SpawnLocation, CustomSound]()
		{
			if (WeakThis.IsValid())
			{
				UGameExplosionsManager* StrongThis = WeakThis.Get();
				StrongThis->DelayedExplosionSpawnCustomSound(ExplosionType, SpawnLocation, CustomSound);
			}
		});

		FTimerHandle TimerHandle;
		// Schedule a one-shot timer that calls DelayedExplosionSpawn after 'Delay' seconds
		World->GetTimerManager().SetTimer(
			TimerHandle,
			TimerDel,
			Delay,
			/*bLoop=*/ false
		);
	}
	else
	{
		RTSFunctionLibrary::ReportError(
			TEXT("[UGameExplosionsManager] SpawnRTSExplosionAtLocation failed, no valid World."));
	}
}

void UGameExplosionsManager::DelayedExplosionSpawn(ERTS_ExplosionType ExplosionType, const FVector& SpawnLocation,
                                                   const bool bPlaySound)
{
	UNiagaraSystem* ChosenSystem = PickRandomExplForType(ExplosionType);
	if (not ChosenSystem)
	{
		// Return silently as the pick function already handles any errors.
		return;
	}
	// Play sound if requested
	USoundCue* SoundToPlay = PickRandomSoundForType(ExplosionType, bPlaySound);

	// Spawn the Niagara system at the specified location
	if (UWorld* World = GetWorld())
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World,
			ChosenSystem,
			SpawnLocation,
			FRotator::ZeroRotator,
			FVector::OneVector,
			/*bAutoDestroy=*/ true,
			/*bAutoActivate=*/ true,
			/*PoolMethod=*/ ENCPoolMethod::AutoRelease
		);

		// Debug visualization if desired
		if (DeveloperSettings::Debugging::ExplosionsManager_Compile_DebugSymbols)
		{
			DrawDebugSphere(World, SpawnLocation, 50.f, 12, FColor::Green, false, 5.f);
		}

		if (not SoundToPlay)
		{
			return;
		}
		UGameplayStatics::PlaySoundAtLocation(
			World,
			SoundToPlay,
			SpawnLocation,
			FRotator::ZeroRotator,
			1.0f, // Volume multiplier
			1.0f, // Pitch multiplier
			0.0f, // Start time
			M_SoundAttenuation, // Attenuation settings
			M_SoundConcurrency, // Concurrency settings
			nullptr);
	}
	else
	{
		RTSFunctionLibrary::ReportError(TEXT("[UGameExplosionsManager] DelayedExplosionSpawn failed, no valid World."));
	}
}

void UGameExplosionsManager::DelayedExplosionSpawnCustomSound(ERTS_ExplosionType ExplosionType,
                                                              const FVector& SpawnLocation,
                                                              USoundCue* CustomSound)
{
	UNiagaraSystem* ChosenSystem = PickRandomExplForType(ExplosionType);
	if (not ChosenSystem)
	{
		// Return silently as the pick function already handles any errors.
		return;
	}

	if (UWorld* World = GetWorld())
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			World,
			ChosenSystem,
			SpawnLocation,
			FRotator::ZeroRotator,
			FVector::OneVector,
			/*bAutoDestroy=*/ true,
			/*bAutoActivate=*/ true,
			/*PoolMethod=*/ ENCPoolMethod::AutoRelease
		);
		if (not CustomSound)
		{
			return;
		}
		UGameplayStatics::PlaySoundAtLocation(
			World,
			CustomSound,
			SpawnLocation,
			FRotator::ZeroRotator,
			1.0f, // Volume multiplier
			1.0f, // Pitch multiplier
			0.0f, // Start time
			M_SoundAttenuation, // Attenuation settings
			M_SoundConcurrency, // Concurrency settings
			nullptr);
	}
}

USoundCue* UGameExplosionsManager::PickRandomSoundForType(const ERTS_ExplosionType ExplosionType, const bool bPlaySound)
{
	if (not bPlaySound)
	{
		return nullptr;
	}
	const TArray<USoundCue*>* SoundArrayPtr = M_ExplSoundsMap.Find(ExplosionType);
	if (not SoundArrayPtr || SoundArrayPtr->Num() == 0)
	{
		return nullptr;
	}
	const TArray<USoundCue*>& SoundArray = *SoundArrayPtr;
	const int32 SoundIndex = FMath::RandRange(0, SoundArray.Num() - 1);
	return SoundArray[SoundIndex];
}

UNiagaraSystem* UGameExplosionsManager::PickRandomExplForType(const ERTS_ExplosionType ExplosionType)
{
	// Ensure we have systems for the requested type
	const TArray<UNiagaraSystem*>* SystemsArrayPtr = M_ExplosionSystemsMap.Find(ExplosionType);
	if (not SystemsArrayPtr || SystemsArrayPtr->Num() == 0)
	{
		RTSFunctionLibrary::ReportError(
			Global_GetExplosionTypeString(ExplosionType) + FString::Printf(
				TEXT("[UGameExplosionsManager] No NiagaraSystems available for ExplosionType %d."),
				static_cast<int32>(ExplosionType)
			)
		);
		return nullptr;
	}
	// Randomly pick a system from the array
	const TArray<UNiagaraSystem*>& SystemsArray = *SystemsArrayPtr;
	const int32 RandomIndex = FMath::RandRange(0, SystemsArray.Num() - 1);
	return SystemsArray[RandomIndex];
}

void UGameExplosionsManager::SpawnMultipleExplosionsInArea(
	ERTS_ExplosionType ExplosionType,
	const FVector& CenterLocation,
	int32 ExplosionCount,
	float Range,
	float PreferredDistance
)
{
	if (ExplosionCount <= 0)
	{
		return;
	}

	TArray<FVector> PlacedLocations;
	PlacedLocations.Reserve(ExplosionCount);

	constexpr int32 MaxPlacementAttempts = 15;

	for (int32 i = 0; i < ExplosionCount; i++)
	{
		bool bPlaced = false;
		FVector Candidate;

		for (int32 Attempt = 0; Attempt < MaxPlacementAttempts; Attempt++)
		{
			float RandRadius = FMath::FRandRange(0.f, Range);
			float RandAngle = FMath::FRandRange(0.f, 2.f * PI);

			Candidate.X = CenterLocation.X + RandRadius * FMath::Cos(RandAngle);
			Candidate.Y = CenterLocation.Y + RandRadius * FMath::Sin(RandAngle);
			Candidate.Z = CenterLocation.Z; // same plane

			bool bValid = true;
			for (const FVector& Existing : PlacedLocations)
			{
				if (FVector::Dist2D(Candidate, Existing) < PreferredDistance)
				{
					bValid = false;
					break;
				}
			}

			if (bValid)
			{
				PlacedLocations.Add(Candidate);
				bPlaced = true;
				break;
			}
		}

		if (not bPlaced)
		{
			// Couldn't find a strictly valid spot, place randomly ignoring the distance
			float RandRadius = FMath::FRandRange(0.f, Range);
			float RandAngle = FMath::FRandRange(0.f, 2.f * PI);

			Candidate.X = CenterLocation.X + RandRadius * FMath::Cos(RandAngle);
			Candidate.Y = CenterLocation.Y + RandRadius * FMath::Sin(RandAngle);
			Candidate.Z = CenterLocation.Z;
			PlacedLocations.Add(Candidate);
		}
	}

	// Now spawn the requested explosion type at each location
	for (const FVector& FinalLocation : PlacedLocations)
	{
		// No delay in this example, but you could add a random delay if desired
		SpawnRTSExplosionAtLocation(ExplosionType, FinalLocation, false, 0.f);
	}
}
