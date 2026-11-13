// Copyright (C) Bas Blokzijl - All rights reserved.

#include "SmallArmsMuzzleVfxBatcher.h"

#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "NiagaraComponent.h"
#include "NiagaraDataSetAccessor.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraSystemInstanceController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/RTSWeaponVFXSettings/RTSWeaponVFXSettings.h"
#include "RTS_Survival/Weapons/SmallArmsProjectileManager/SmallArmsProjectileManager.h"

namespace SmallArmsMuzzleVfxNames
{
	// User parameters of the small arms muzzle Niagara system.
	const FName PositionAndScaleArray = TEXT("ShotPositionAndScale");
	const FName DirectionArray = TEXT("ShotDirection");

	const FName TypeOffset[] = {TEXT("FlashOffset"), TEXT("SmokeOffset")};
	const FName TypeCount[] = {TEXT("FlashCount"), TEXT("SmokeCount")};
	// Emitters are named <Group>_<Layer>; these are the group prefixes in the order of ESmallArmsMuzzleVfxType.
	const TCHAR* TypePrefix[] = {TEXT("Flash_"), TEXT("Smoke_")};
	const TCHAR* PerShotSuffix = TEXT("_PerShot");
	const TCHAR* SpawnCountSuffix = TEXT("_SpawnCount");

	// Beyond this the float precision of the particle positions is no longer good enough to draw a muzzle.
	constexpr double MaxSafeWorldCoordinateCm = 10000000.0;
}

USmallArmsMuzzleVfxBatcher::USmallArmsMuzzleVfxBatcher()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	// Weapons fire from timers, which run after the tick groups of a frame: their shots are published at the start
	// of the next frame, before the Niagara component (which ticks after this one) spawns them.
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	for (int32 TypeIndex = 0; TypeIndex < MuzzleTypeCount; ++TypeIndex)
	{
		M_PublishedOffset[TypeIndex] = 0;
		M_PublishedCount[TypeIndex] = 0;
	}
}

USmallArmsMuzzleVfxBatcher* USmallArmsMuzzleVfxBatcher::FindReadyBatcher(const UWorld* World)
{
	if (World == nullptr)
	{
		return nullptr;
	}
	for (TActorIterator<ASmallArmsProjectileManager> ManagerIterator(World); ManagerIterator; ++ManagerIterator)
	{
		USmallArmsMuzzleVfxBatcher* Batcher = ManagerIterator->GetSmallArmsMuzzleVfxBatcher();
		if (IsValid(Batcher) && Batcher->GetIsReadyToDraw())
		{
			return Batcher;
		}
	}
	return nullptr;
}

#if RTS_WITH_MAP_TESTS
FString USmallArmsMuzzleVfxBatcher::DescribeEmitterParticleCounts() const
{
	if (not GetIsValidNiagaraComponent())
	{
		return TEXT("no component");
	}
	const FNiagaraSystemInstanceControllerConstPtr InstanceController =
		M_NiagaraComponent->GetSystemInstanceController();
	const FNiagaraSystemInstance* SystemInstance = InstanceController.IsValid()
		                                               ? InstanceController->GetSystemInstance_Unsafe()
		                                               : nullptr;
	if (SystemInstance == nullptr)
	{
		return TEXT("no system instance");
	}
	FString Description;
	for (const FNiagaraEmitterInstanceRef& EmitterInstance : SystemInstance->GetEmitters())
	{
		Description += FString::Printf(
			TEXT("%s=%d/%d "),
			*EmitterInstance->GetEmitterHandle().GetName().ToString(),
			EmitterInstance->GetNumParticles(),
			EmitterInstance->GetTotalSpawnedParticles());
	}
	return Description;
}

FString USmallArmsMuzzleVfxBatcher::DescribeFirstParticles() const
{
	if (not GetIsValidNiagaraComponent())
	{
		return TEXT("no component");
	}
	const FNiagaraSystemInstanceControllerConstPtr InstanceController =
		M_NiagaraComponent->GetSystemInstanceController();
	const FNiagaraSystemInstance* SystemInstance = InstanceController.IsValid()
		                                               ? InstanceController->GetSystemInstance_Unsafe()
		                                               : nullptr;
	if (SystemInstance == nullptr)
	{
		return TEXT("no system instance");
	}
	FString Description;
	for (const FNiagaraEmitterInstanceRef& EmitterInstance : SystemInstance->GetEmitters())
	{
		const FNiagaraDataSet& DataSet = EmitterInstance->GetParticleData();
		const int32 ParticleCount = EmitterInstance->GetNumParticles();
		Description += FString::Printf(TEXT("\n  %s n=%d"), *EmitterInstance->GetEmitterHandle().GetName().ToString(),
		                               ParticleCount);
		if (ParticleCount <= 0)
		{
			continue;
		}
		const auto Position = FNiagaraDataSetAccessor<FVector3f>(DataSet, TEXT("Position")).GetReader(DataSet);
		const auto Age = FNiagaraDataSetAccessor<float>(DataSet, TEXT("Age")).GetReader(DataSet);
		const auto Lifetime = FNiagaraDataSetAccessor<float>(DataSet, TEXT("Lifetime")).GetReader(DataSet);
		const auto Color = FNiagaraDataSetAccessor<FLinearColor>(DataSet, TEXT("Color")).GetReader(DataSet);
		const auto SpriteSize = FNiagaraDataSetAccessor<FVector2f>(DataSet, TEXT("SpriteSize")).GetReader(DataSet);
		const auto Scale = FNiagaraDataSetAccessor<FVector3f>(DataSet, TEXT("Scale")).GetReader(DataSet);
		Description += FString::Printf(
			TEXT(" pos=%s age=%s life=%s color=%s sprite=%s scale=%s"),
			Position.IsValid() ? *FVector(Position.Get(0)).ToCompactString() : TEXT("-"),
			Age.IsValid() ? *FString::SanitizeFloat(Age.Get(0)) : TEXT("-"),
			Lifetime.IsValid() ? *FString::SanitizeFloat(Lifetime.Get(0)) : TEXT("-"),
			Color.IsValid() ? *Color.Get(0).ToString() : TEXT("-"),
			SpriteSize.IsValid() ? *FVector2D(SpriteSize.Get(0)).ToString() : TEXT("-"),
			Scale.IsValid() ? *FVector(Scale.Get(0)).ToCompactString() : TEXT("-"));
	}
	return Description;
}
#endif

void USmallArmsMuzzleVfxBatcher::BeginPlay()
{
	Super::BeginPlay();

	const URTSWeaponVFXSettings* VfxSettings = URTSWeaponVFXSettings::Get();
	if (VfxSettings == nullptr || not VfxSettings->bUseBatchedSmallArmsMuzzleVfx)
	{
		return;
	}
#if RTS_WITH_MAP_TESTS
	// Lets a measuring run draw the muzzles the old way, one Niagara component per weapon socket, to compare against.
	if (FParse::Param(FCommandLine::Get(), TEXT("SmallArmsMuzzleVfxLegacy")))
	{
		return;
	}
#endif
	UNiagaraSystem* MuzzleSystem = VfxSettings->SmallArmsMuzzleNiagaraSystem.LoadSynchronous();
	if (not IsValid(MuzzleSystem))
	{
		RTSFunctionLibrary::ReportError(
			"The small arms muzzle Niagara system set in the RTS Weapon VFX project settings could not be loaded: "
			+ VfxSettings->SmallArmsMuzzleNiagaraSystem.ToString()
			+ "\n Infantry weapons fall back to the launch effect of each weapon.");
		return;
	}
	M_MaxShotsPerFrame = FMath::Max(1, VfxSettings->MaxSmallArmsMuzzleShotsPerFrame);
	BeginPlay_ReadEmitterSpawns(*MuzzleSystem);
	BeginPlay_CreateNiagaraComponent(MuzzleSystem);
}

void USmallArmsMuzzleVfxBatcher::BeginPlay_ReadEmitterSpawns(const UNiagaraSystem& MuzzleSystem)
{
	using namespace SmallArmsMuzzleVfxNames;
	const FNiagaraUserRedirectionParameterStore& UserParameterStore = MuzzleSystem.GetExposedParameters();
	TArray<FNiagaraVariable> UserParameters;
	UserParameterStore.GetUserParameters(UserParameters);
	for (const FNiagaraVariable& UserParameter : UserParameters)
	{
		const FString ParameterName = UserParameter.GetName().ToString();
		if (UserParameter.GetType() != FNiagaraTypeDefinition::GetIntDef() || not ParameterName.EndsWith(PerShotSuffix))
		{
			continue;
		}
		const FString EmitterName = ParameterName.LeftChop(FCString::Strlen(PerShotSuffix));
		for (int32 TypeIndex = 0; TypeIndex < MuzzleTypeCount; ++TypeIndex)
		{
			if (not EmitterName.StartsWith(TypePrefix[TypeIndex]))
			{
				continue;
			}
			const FNiagaraVariable StoredParameter(
				FNiagaraTypeDefinition::GetIntDef(), FName(*(TEXT("User.") + ParameterName)));
			FSmallArmsMuzzleEmitterSpawn EmitterSpawn;
			EmitterSpawn.TypeIndex = TypeIndex;
			EmitterSpawn.SpawnCountParameter = FName(*(EmitterName + SpawnCountSuffix));
			EmitterSpawn.ParticlesPerShot = FMath::Max(1, UserParameterStore.GetParameterValue<int32>(StoredParameter));
			M_EmitterSpawns.Add(EmitterSpawn);
			break;
		}
	}
}

void USmallArmsMuzzleVfxBatcher::BeginPlay_CreateNiagaraComponent(UNiagaraSystem* MuzzleSystem)
{
	AActor* OwningActor = GetOwner();
	if (not IsValid(OwningActor))
	{
		return;
	}
	// Not attached to anything: stays at the world origin, the emitters place their particles in world space.
	M_NiagaraComponent = NewObject<UNiagaraComponent>(OwningActor);
	if (not GetIsValidNiagaraComponent())
	{
		return;
	}
	M_NiagaraComponent->bAutoActivate = false;
	M_NiagaraComponent->SetAutoDestroy(false);
	M_NiagaraComponent->SetCastShadow(false);
	M_NiagaraComponent->SetAsset(MuzzleSystem);
	M_NiagaraComponent->RegisterComponent();
	M_NiagaraComponent->AddTickPrerequisiteComponent(this);
	for (int32 TypeIndex = 0; TypeIndex < MuzzleTypeCount; ++TypeIndex)
	{
		M_NiagaraComponent->SetVariableInt(SmallArmsMuzzleVfxNames::TypeOffset[TypeIndex], 0);
		M_NiagaraComponent->SetVariableInt(SmallArmsMuzzleVfxNames::TypeCount[TypeIndex], 0);
	}
	for (const FSmallArmsMuzzleEmitterSpawn& EmitterSpawn : M_EmitterSpawns)
	{
		M_NiagaraComponent->SetVariableInt(EmitterSpawn.SpawnCountParameter, 0);
	}
	M_NiagaraComponent->Activate(true);
	bM_IsReadyToDraw = not M_EmitterSpawns.IsEmpty();
	if (not bM_IsReadyToDraw)
	{
		RTSFunctionLibrary::ReportError(
			"The small arms muzzle Niagara system has no <Group>_<Layer>_PerShot user parameters; run the console"
			" command RTS.MuzzleVfx.PatchNiagaraSystem. Infantry weapons fall back to their own launch effects.");
	}
}

bool USmallArmsMuzzleVfxBatcher::GetIsValidNiagaraComponent() const
{
	if (IsValid(M_NiagaraComponent))
	{
		return true;
	}

	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this,
		"M_NiagaraComponent",
		"GetIsValidNiagaraComponent",
		this
	);

	return false;
}

void USmallArmsMuzzleVfxBatcher::AddShot(
	const FVector& MuzzleLocation,
	const FVector& FireDirection,
	const float FlashScale,
	const float SmokeScale,
	const float SmokeLifetimeMlt)
{
	if (not bM_IsReadyToDraw || M_PendingShotCount >= M_MaxShotsPerFrame)
	{
		return;
	}
	const bool bIsSafeLocation = not MuzzleLocation.ContainsNaN() && not FireDirection.ContainsNaN()
		&& MuzzleLocation.GetAbsMax() < SmallArmsMuzzleVfxNames::MaxSafeWorldCoordinateCm;
	if (not bIsSafeLocation)
	{
		return;
	}
	const FVector3f Direction = FVector3f(FireDirection.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector));
	AddPendingEntry(ESmallArmsMuzzleVfxType::Flash, MuzzleLocation, Direction, FlashScale, 1.0f);
	if (SmokeScale > 0.0f)
	{
		AddPendingEntry(ESmallArmsMuzzleVfxType::Smoke, MuzzleLocation, Direction, SmokeScale, SmokeLifetimeMlt);
	}
	if (M_PendingShotCount == 0)
	{
		SetComponentTickEnabled(true);
	}
	++M_PendingShotCount;
}

void USmallArmsMuzzleVfxBatcher::AddPendingEntry(
	const ESmallArmsMuzzleVfxType Type,
	const FVector& MuzzleLocation,
	const FVector3f& Direction,
	const float Scale,
	const float LifetimeMlt)
{
	/**
	 * @brief What one shot stores per type: two float4 entries in the pending lists of that type.
	 * - M_PendingPositionAndScale: xyz = muzzle location, w = size of the flash or smoke.
	 * - M_PendingDirection: xyz = normalised fire direction, w = lifetime multiplier (1 for the flash).
	 * The type is the list the entries are in; on the next tick the lists are put behind each other and each
	 * emitter is told where its type starts and how many shots it has.
	 */
	const int32 TypeIndex = FMath::Clamp(static_cast<int32>(Type), 0, MuzzleTypeCount - 1);
	M_PendingPositionAndScale[TypeIndex].Emplace(
		static_cast<float>(MuzzleLocation.X), static_cast<float>(MuzzleLocation.Y),
		static_cast<float>(MuzzleLocation.Z), Scale);
	M_PendingDirection[TypeIndex].Emplace(Direction.X, Direction.Y, Direction.Z, LifetimeMlt);
}

void USmallArmsMuzzleVfxBatcher::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (not GetIsValidNiagaraComponent())
	{
		bM_IsReadyToDraw = false;
		SetComponentTickEnabled(false);
		return;
	}
	if (M_PendingShotCount > 0)
	{
		PublishPendingShots();
		return;
	}
	// Nothing new: the system has spawned last frame's shots, stop it from spawning them again and go idle.
	ClearPublishedShots();
	SetComponentTickEnabled(false);
}

void USmallArmsMuzzleVfxBatcher::PublishPendingShots()
{
	M_PublishedPositionAndScale.Reset();
	M_PublishedDirection.Reset();
	for (int32 TypeIndex = 0; TypeIndex < MuzzleTypeCount; ++TypeIndex)
	{
		const int32 Offset = M_PublishedPositionAndScale.Num();
		const int32 Count = M_PendingPositionAndScale[TypeIndex].Num();
		M_PublishedPositionAndScale.Append(M_PendingPositionAndScale[TypeIndex]);
		M_PublishedDirection.Append(M_PendingDirection[TypeIndex]);
		// Reset keeps the memory, so a fight does not allocate every frame.
		M_PendingPositionAndScale[TypeIndex].Reset();
		M_PendingDirection[TypeIndex].Reset();
		PublishTypeRange(TypeIndex, Offset, Count);
	}
#if RTS_WITH_MAP_TESTS
	M_TotalPublishedShots += M_PendingShotCount;
#endif
	M_PendingShotCount = 0;
	PublishEmitterSpawnCounts();

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector4(
		M_NiagaraComponent,
		SmallArmsMuzzleVfxNames::PositionAndScaleArray,
		TConstArrayView<FVector4f>(M_PublishedPositionAndScale));
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector4(
		M_NiagaraComponent,
		SmallArmsMuzzleVfxNames::DirectionArray,
		TConstArrayView<FVector4f>(M_PublishedDirection));
}

void USmallArmsMuzzleVfxBatcher::ClearPublishedShots()
{
	for (int32 TypeIndex = 0; TypeIndex < MuzzleTypeCount; ++TypeIndex)
	{
		PublishTypeRange(TypeIndex, 0, 0);
	}
	PublishEmitterSpawnCounts();
}

void USmallArmsMuzzleVfxBatcher::PublishEmitterSpawnCounts()
{
	for (FSmallArmsMuzzleEmitterSpawn& EmitterSpawn : M_EmitterSpawns)
	{
		const int32 SpawnCount = M_PublishedCount[EmitterSpawn.TypeIndex] * EmitterSpawn.ParticlesPerShot;
		if (EmitterSpawn.PublishedSpawnCount == SpawnCount)
		{
			continue;
		}
		EmitterSpawn.PublishedSpawnCount = SpawnCount;
		M_NiagaraComponent->SetVariableInt(EmitterSpawn.SpawnCountParameter, SpawnCount);
	}
}

void USmallArmsMuzzleVfxBatcher::PublishTypeRange(const int32 TypeIndex, const int32 Offset, const int32 Count)
{
	// Only what changed is handed over; on most frames only a few shots come in.
	if (Count > 0 && M_PublishedOffset[TypeIndex] != Offset)
	{
		M_PublishedOffset[TypeIndex] = Offset;
		M_NiagaraComponent->SetVariableInt(SmallArmsMuzzleVfxNames::TypeOffset[TypeIndex], Offset);
	}
	if (M_PublishedCount[TypeIndex] != Count)
	{
		M_PublishedCount[TypeIndex] = Count;
		M_NiagaraComponent->SetVariableInt(SmallArmsMuzzleVfxNames::TypeCount[TypeIndex], Count);
	}
}
