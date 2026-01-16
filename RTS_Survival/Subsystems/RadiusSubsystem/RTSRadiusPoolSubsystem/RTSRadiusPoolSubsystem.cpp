#include "RTSRadiusPoolSubsystem.h"

#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/Actor.h"
#include "RTS_Survival/Subsystems/RadiusSubsystem/PooledRadiusActor/PooledRadiusActor.h"
#include "RTS_Survival/Subsystems/RadiusSubsystem/RadiusPoolSettings/RadiusPoolSettings.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "UObject/ConstructorHelpers.h"

bool URTSRadiusPoolSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
        const UWorld* OuterWorld = Cast<UWorld>(Outer);
        if (not IsValid(OuterWorld))
        {
                return false;
        }

        return OuterWorld->IsGameWorld();
}

void URTSRadiusPoolSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
        Super::Initialize(Collection);

	if (not GetIsValidWorld())
	{
		return;
	}

	Initialize_LoadSettings();
	Initialize_SpawnPool();
}

void URTSRadiusPoolSubsystem::Deinitialize()
{
	// Clear timers
	if (UWorld* World = GetWorld())
	{
		for (auto& Pair : M_LifetimeTimers)
		{
			World->GetTimerManager().ClearTimer(Pair.Value);
		}
	}
	M_LifetimeTimers.Empty();

	// Destroy pooled actors
	for (APooledRadiusActor* Each : M_Pool)
	{
		if (IsValid(Each))
		{
			Each->Destroy();
		}
	}
	M_Pool.Empty();
	M_IdToActor.Empty();
	M_TypeToMaterial.Empty();
	M_BorderOnlyMeshSettings.M_RadiusMesh = nullptr;
	M_FullCircleMeshSettings.M_RadiusMesh = nullptr;
	M_RadiusMeshRadiusParameterName = NAME_None;

	Super::Deinitialize();
}

bool URTSRadiusPoolSubsystem::GetIsValidWorld() const
{
	if (IsValid(GetWorld()))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(TEXT("URTSRadiusPoolSubsystem:: World is invalid on Initialize/Use."));
	return false;
}

bool URTSRadiusPoolSubsystem::GetIsValidBorderOnlyMesh() const
{
	if (IsValid(M_BorderOnlyMeshSettings.M_RadiusMesh))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(TEXT("RTS Radius Pool: Border only radius mesh is not set/loaded in settings."));
	return false;
}

bool URTSRadiusPoolSubsystem::GetIsValidFullCircleMesh() const
{
	if (IsValid(M_FullCircleMeshSettings.M_RadiusMesh))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(TEXT("RTS Radius Pool: Full circle radius mesh is not set/loaded in settings."));
	return false;
}

bool URTSRadiusPoolSubsystem::GetIsFullCircleRadiusType(const ERTSRadiusType Type) const
{
	switch (Type)
	{
	case ERTSRadiusType::FUllCircle_Weaponrange:
	case ERTSRadiusType::FullCircle_RepairRange:
	case ERTSRadiusType::FullCircle_CommandAura:
	case ERTSRadiusType::FullCircle_ReinforcementAura:
	case ERTSRadiusType::FullCircle_ImprovedRangeArea:
	case ERTSRadiusType::FullCircle_RadiationAura:
	case ERTSRadiusType::Fullcircle_HealAura:
		return true;
	default:
		return false;
	}
}

const FRTSRadiusMeshSettings* URTSRadiusPoolSubsystem::GetMeshSettingsForType(const ERTSRadiusType Type) const
{
	return GetIsFullCircleRadiusType(Type) ? &M_FullCircleMeshSettings : &M_BorderOnlyMeshSettings;
}

void URTSRadiusPoolSubsystem::Initialize_LoadSettings()
{
	const URadiusPoolSettings* Settings = GetDefault<URadiusPoolSettings>();
	if (not IsValid(Settings))
	{
		RTSFunctionLibrary::ReportError(TEXT("URTSRadiusPoolSubsystem: Settings object is null."));
		return;
	}

	M_BorderOnlyMeshSettings.M_RadiusMesh = Settings->BorderOnlyRadiusMesh.LoadSynchronous();
	M_FullCircleMeshSettings.M_RadiusMesh = Settings->FullCircleRadiusMesh.LoadSynchronous();
	M_DefaultStartingRadius = Settings->StartingRadius;
	M_BorderOnlyMeshSettings.M_UnitsPerScale = Settings->BorderOnlyUnitsPerScale;
	M_BorderOnlyMeshSettings.M_ZScale = Settings->ZScale;
	M_BorderOnlyMeshSettings.M_RenderHeight = Settings->BorderOnlyRenderHeight;
	M_FullCircleMeshSettings.M_UnitsPerScale = Settings->FullCircleUnitsPerScale;
	M_FullCircleMeshSettings.M_ZScale = Settings->FullCircleZScale;
	M_FullCircleMeshSettings.M_RenderHeight = Settings->FullCircleRenderHeight;
	M_RadiusMeshRadiusParameterName = Settings->RadiusMeshRadiusParameterName;
	M_DefaultPoolSize = FMath::Max(1, Settings->DefaultPoolSize);

	Initialize_LoadMaterials(Settings);
}

void URTSRadiusPoolSubsystem::Initialize_LoadMaterials(const URadiusPoolSettings* Settings)
{
	M_TypeToMaterial.Empty();

	for (const auto& Pair : Settings->TypeToMaterial)
	{
		UMaterialInterface* Mat = Pair.Value.LoadSynchronous();
		if (not IsValid(Mat))
		{
			// Not fatal; can fall back to mesh default. Still log for visibility.
			RTSFunctionLibrary::ReportError(
				FString::Printf(TEXT("RTS Radius Pool: Material for type %d failed to load."), static_cast<int32>(Pair.Key)));
			continue;
		}
		M_TypeToMaterial.Add(Pair.Key, Mat);
	}
}

void URTSRadiusPoolSubsystem::Initialize_SpawnPool()
{
	if (not GetIsValidWorld() || not GetIsValidBorderOnlyMesh() || not GetIsValidFullCircleMesh())
	{
		return;
	}

	M_Pool.Reserve(M_DefaultPoolSize);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const int32 BorderOnlyCount = M_DefaultPoolSize / 2;

	for (int32 i = 0; i < M_DefaultPoolSize; ++i)
	{
		APooledRadiusActor* Actor = GetWorld()->SpawnActor<APooledRadiusActor>(APooledRadiusActor::StaticClass(), FTransform::Identity, Params);
		if (not IsValid(Actor))
		{
			RTSFunctionLibrary::ReportError(TEXT("RTS Radius Pool: Failed to spawn APooledRadiusActor."));
			continue;
		}
		const bool bUseFullCircleMesh = i >= BorderOnlyCount;
		const FRTSRadiusMeshSettings& MeshSettings = bUseFullCircleMesh ? M_FullCircleMeshSettings : M_BorderOnlyMeshSettings;
		Actor->SetPoolId(INDEX_NONE);
		Actor->InitRadiusActor(MeshSettings.M_RadiusMesh, M_DefaultStartingRadius, MeshSettings.M_UnitsPerScale,
		                       MeshSettings.M_ZScale, MeshSettings.M_RenderHeight, bUseFullCircleMesh);
		M_Pool.Add(Actor);
	}
}

APooledRadiusActor* URTSRadiusPoolSubsystem::AcquireFreeOrLRU()
{
	APooledRadiusActor* Free = nullptr;

	// First pass: free one
	for (APooledRadiusActor* Each : M_Pool)
	{
		if (not IsValid(Each))
		{
			continue;
		}
		if (not Each->GetIsInUse())
		{
			Free = Each;
			break;
		}
	}

	if (Free)
	{
		return Free;
	}

	// Second pass: pick LRU
	float BestTime = TNumericLimits<float>::Max();
	APooledRadiusActor* Lru = nullptr;

	for (APooledRadiusActor* Each : M_Pool)
	{
		if (not IsValid(Each))
		{
			continue;
		}
		if (Each->GetLastUsedWorldSeconds() < BestTime)
		{
			BestTime = Each->GetLastUsedWorldSeconds();
			Lru = Each;
		}
	}

	// If we are stealing an in-use entry, reclaim its id mapping.
	if (IsValid(Lru))
	{
		int32 FoundId = INDEX_NONE;
		for (const auto& Pair : M_IdToActor)
		{
			if (Pair.Value.Get() == Lru)
			{
				FoundId = Pair.Key;
				break;
			}
		}

		if (FoundId != INDEX_NONE)
		{
			ReleaseById_Internal(FoundId, /*bSilentIfMissing*/ true);
		}
	}
	return Lru;
}

int32 URTSRadiusPoolSubsystem::CreateRTSRadius(const FVector Location, const float Radius, const ERTSRadiusType Type, const float LifeTime)
{
	if (not GetIsValidWorld())
	{
		return -1;
	}

	const bool bRequestFullCircleMesh = GetIsFullCircleRadiusType(Type);
	if (bRequestFullCircleMesh && not GetIsValidFullCircleMesh())
	{
		return -1;
	}
	if (not bRequestFullCircleMesh && not GetIsValidBorderOnlyMesh())
	{
		return -1;
	}

	const FRTSRadiusMeshSettings* MeshSettings = GetMeshSettingsForType(Type);
	if (not MeshSettings)
	{
		RTSFunctionLibrary::ReportError(TEXT("RTS Radius Pool: Missing mesh settings for radius type."));
		return -1;
	}

	APooledRadiusActor* Entry = AcquireFreeOrLRU();
	if (not IsValid(Entry))
	{
		RTSFunctionLibrary::ReportError(TEXT("RTS Radius Pool: Could not acquire a pooled radius actor."));
		return -1;
	}

	// Assign new id and bind
	const int32 NewId = M_NextId++;
	Entry->SetPoolId(NewId);
	M_IdToActor.Add(NewId, Entry);

	const TObjectPtr<UMaterialInterface>* MatPtr = M_TypeToMaterial.Find(Type);
	UMaterialInterface* Mat = MatPtr ? *MatPtr : nullptr;

	const bool bMeshTypeMatches = Entry->GetUsesFullCircleMesh() == bRequestFullCircleMesh;
	const bool bRadiusTypeMatches = Entry->GetRadiusType() == Type;
	if (not bMeshTypeMatches)
	{
		Entry->InitRadiusActor(MeshSettings->M_RadiusMesh, M_DefaultStartingRadius, MeshSettings->M_UnitsPerScale,
		                       MeshSettings->M_ZScale, MeshSettings->M_RenderHeight, bRequestFullCircleMesh);
	}

	UMaterialInterface* MaterialToApply = nullptr;
	if (not bRadiusTypeMatches || not bMeshTypeMatches)
	{
		MaterialToApply = Mat;
	}

	Entry->ActivateRadiusAt(Location, Radius, MaterialToApply, Type, bRequestFullCircleMesh, M_RadiusMeshRadiusParameterName);

	// Lifetime handling
	if (LifeTime > 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			FTimerHandle Handle;
			FTimerDelegate Del;
			Del.BindUFunction(this, FName(TEXT("OnLifetimeExpired")), NewId);
			World->GetTimerManager().SetTimer(Handle, Del, LifeTime, false);
			M_LifetimeTimers.Add(NewId, Handle);
		}
	}

	return NewId;
}

void URTSRadiusPoolSubsystem::HideRTSRadiusById(const int32 ID)
{
	ReleaseById_Internal(ID, /*bSilentIfMissing*/ false);
}

void URTSRadiusPoolSubsystem::AttachRTSRadiusToActor(const int32 ID, AActor* TargetActor, const FVector RelativeOffset)
{
	if (not GetIsValidWorld())
	{
		return;
	}

	if (not IsValid(TargetActor))
	{
		RTSFunctionLibrary::ReportError(TEXT("RTS Radius Pool: Attach target actor is invalid."));
		return;
	}

	APooledRadiusActor* RadiusActor = FindPooledActorById(ID);
	if (not IsValid(RadiusActor))
	{
		RTSFunctionLibrary::ReportError(FString::Printf(TEXT("RTS Radius Pool: Unknown id %d in AttachRTSRadiusToActor."), ID));
		return;
	}

	// Attach and apply offset
	RadiusActor->AttachToActor(TargetActor, FAttachmentTransformRules::KeepWorldTransform);
	RadiusActor->SetActorRelativeLocation(RelativeOffset);
	RadiusActor->SetActorRelativeRotation(FRotator::ZeroRotator);
}

void URTSRadiusPoolSubsystem::ReleaseById_Internal(const int32 Id, const bool bSilentIfMissing)
{
	TWeakObjectPtr<APooledRadiusActor>* Found = M_IdToActor.Find(Id);
	if (not Found)
	{
		if (not bSilentIfMissing)
		{
			RTSFunctionLibrary::ReportError(FString::Printf(TEXT("RTS Radius Pool: Unknown id %d in HideRTSRadiusById."), Id));
		}
		return;
	}

	// Clear timer if any
	if (UWorld* World = GetWorld())
	{
		if (FTimerHandle* H = M_LifetimeTimers.Find(Id))
		{
			World->GetTimerManager().ClearTimer(*H);
			M_LifetimeTimers.Remove(Id);
		}
	}

	if (APooledRadiusActor* Actor = Found->Get())
	{
		// Detach (keep world so we can safely hide).
		Actor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		Actor->DeactivateRadius();
		Actor->SetPoolId(INDEX_NONE);
	}

	M_IdToActor.Remove(Id);
}

APooledRadiusActor* URTSRadiusPoolSubsystem::FindPooledActorById(const int32 Id) const
{
	if (const TWeakObjectPtr<APooledRadiusActor>* Found = M_IdToActor.Find(Id))
	{
		return Found->Get();
	}
	return nullptr;
}

void URTSRadiusPoolSubsystem::OnLifetimeExpired(const int32 ExpiredId)
{
	ReleaseById_Internal(ExpiredId, /*bSilentIfMissing*/ true);
}
