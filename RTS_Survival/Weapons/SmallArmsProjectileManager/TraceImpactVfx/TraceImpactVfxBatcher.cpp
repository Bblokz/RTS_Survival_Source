// Copyright (C) Bas Blokzijl - All rights reserved.

#include "TraceImpactVfxBatcher.h"

#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraSystemInstanceController.h"
#include "NiagaraSystem.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/RTSWeaponVFXSettings/RTSWeaponVFXSettings.h"
#include "TraceImpactVfxPerfCounters.h"

namespace TraceImpactVfxNames
{
	// User parameters of the trace impact Niagara system.
	const FName PositionAndScaleArray = TEXT("ImpactPositionAndScale");
	const FName NormalArray = TEXT("ImpactNormal");

	const FName TypeOffset[] = {
		TEXT("SandOffset"), TEXT("MetalOffset"), TEXT("FleshOffset"),
		TEXT("AirOffset"), TEXT("StoneOffset"), TEXT("BounceOffset")
	};
	const FName TypeCount[] = {
		TEXT("SandCount"), TEXT("MetalCount"), TEXT("FleshCount"),
		TEXT("AirCount"), TEXT("StoneCount"), TEXT("BounceCount")
	};
	// Emitters are named <Type>_<Layer>; these are the type prefixes in the order of ETraceImpactVfxType.
	const TCHAR* TypePrefix[] = {
		TEXT("Sand_"), TEXT("Metal_"), TEXT("Flesh_"), TEXT("Air_"), TEXT("Stone_"), TEXT("Bounce_")
	};
	const TCHAR* PerImpactSuffix = TEXT("_PerImpact");
	const TCHAR* SpawnCountSuffix = TEXT("_SpawnCount");

	// Beyond this the float precision of the particle positions is no longer good enough to draw an impact.
	constexpr double MaxSafeWorldCoordinateCm = 10000000.0;
}

#if RTS_WITH_MAP_TESTS
namespace TraceImpactVfxDebug
{
	TAutoConsoleVariable<int32> CVarLogTotals(
		TEXT("RTS.TraceImpactVfx.LogTotals"),
		0,
		TEXT("1: the trace impact batcher logs how many impacts of every type it has drawn, every few seconds."));
	constexpr double SecondsBetweenTotalsLogs = 10.0;
}

void UTraceImpactVfxBatcher::LogPublishedTotalsWhenAsked()
{
	const UWorld* World = GetWorld();
	if (TraceImpactVfxDebug::CVarLogTotals.GetValueOnGameThread() == 0 || World == nullptr)
	{
		return;
	}
	const double NowSeconds = World->GetTimeSeconds();
	if (NowSeconds - M_LastTotalsLogSeconds < TraceImpactVfxDebug::SecondsBetweenTotalsLogs)
	{
		return;
	}
	M_LastTotalsLogSeconds = NowSeconds;
	UE_LOG(
		LogTemp,
		Display,
		TEXT(
			"RTS_TRACE_IMPACT_VFX totals game_seconds=%.0f niagara_active=%d sand=%lld metal=%lld flesh=%lld air=%lld stone=%lld bounce=%lld"
		),
		NowSeconds,
		M_NiagaraComponent->IsActive() ? 1 : 0,
		M_TotalPublishedByType[0], M_TotalPublishedByType[1], M_TotalPublishedByType[2],
		M_TotalPublishedByType[3], M_TotalPublishedByType[4], M_TotalPublishedByType[5]);
}

bool FTraceImpactVfxPerfCounters::bIsGathering = false;
uint64 FTraceImpactVfxPerfCounters::DispatchCycles = 0;
int32 FTraceImpactVfxPerfCounters::ImpactCount = 0;
TMap<FName, int32> FTraceImpactVfxPerfCounters::ImpactCountByOwnerClass;

void FTraceImpactVfxPerfCounters::Reset()
{
	DispatchCycles = 0;
	ImpactCount = 0;
	ImpactCountByOwnerClass.Reset();
}

void FTraceImpactVfxPerfCounters::RecordWeaponOwner(const UObject* WeaponOwner)
{
	if (not bIsGathering || WeaponOwner == nullptr)
	{
		return;
	}
	const UClass* OwnerClass = WeaponOwner->GetClass();
	while (OwnerClass != nullptr && not OwnerClass->IsNative())
	{
		OwnerClass = OwnerClass->GetSuperClass();
	}
	ImpactCountByOwnerClass.FindOrAdd(OwnerClass != nullptr ? OwnerClass->GetFName() : NAME_None)++;
}

void FTraceImpactVfxPerfCounters::RecordConfiguredSystem(
	const TCHAR* ImpactKind,
	const UNiagaraSystem* System,
	const FVector& Scale)
{
	if (not bIsGathering)
	{
		return;
	}
	static TSet<FString> LoggedSystems;
	const FString Key = FString::Printf(
		TEXT("%s|%s|%s"), ImpactKind, *GetNameSafe(System), *Scale.ToCompactString());
	if (LoggedSystems.Contains(Key))
	{
		return;
	}
	LoggedSystems.Add(Key);
	UE_LOG(LogTemp, Display, TEXT("RTS_TRACE_IMPACT_VFX configured kind=%s system=%s scale=%s"),
	       ImpactKind, *GetNameSafe(System), *Scale.ToCompactString());
}
#endif

#if RTS_WITH_MAP_TESTS
FString UTraceImpactVfxBatcher::DescribeEmitterParticleCounts() const
{
	if (not GetIsValidNiagaraComponent())
	{
		return TEXT("no component");
	}
	const FNiagaraSystemInstanceControllerConstPtr InstanceController = M_NiagaraComponent->
		GetSystemInstanceController();
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
#endif

UTraceImpactVfxBatcher::UTraceImpactVfxBatcher()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	// The traces of a frame complete before this group, so their impacts are drawn in the same frame.
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	for (int32 TypeIndex = 0; TypeIndex < ImpactTypeCount; ++TypeIndex)
	{
		M_PublishedOffset[TypeIndex] = 0;
		M_PublishedCount[TypeIndex] = 0;
#if RTS_WITH_MAP_TESTS
		M_TotalPublishedByType[TypeIndex] = 0;
#endif
	}
}

ETraceImpactVfxType UTraceImpactVfxBatcher::GetImpactTypeForSurface(const ERTSSurfaceType SurfaceType)
{
	switch (SurfaceType)
	{
	case ERTSSurfaceType::Sand: return ETraceImpactVfxType::Sand;
	case ERTSSurfaceType::Metal: return ETraceImpactVfxType::Metal;
	case ERTSSurfaceType::Flesh: return ETraceImpactVfxType::Flesh;
	case ERTSSurfaceType::Air: return ETraceImpactVfxType::Air;
	case ERTSSurfaceType::Stone: return ETraceImpactVfxType::Stone;
	}
	return ETraceImpactVfxType::Sand;
}

void UTraceImpactVfxBatcher::BeginPlay()
{
	Super::BeginPlay();

	const URTSWeaponVFXSettings* VfxSettings = URTSWeaponVFXSettings::Get();
	if (VfxSettings == nullptr || not VfxSettings->bUseCentralisedTraceImpactVfx)
	{
		return;
	}
#if RTS_WITH_MAP_TESTS
	// Lets a measuring run draw the impacts the old way, one Niagara component per impact, to compare against.
	if (FParse::Param(FCommandLine::Get(), TEXT("TraceImpactVfxLegacy")))
	{
		return;
	}
#endif
	UNiagaraSystem* TraceImpactSystem = VfxSettings->TraceImpactNiagaraSystem.LoadSynchronous();
	if (not IsValid(TraceImpactSystem))
	{
		RTSFunctionLibrary::ReportError(
			"The trace impact Niagara system set in the RTS Weapon VFX project settings could not be loaded: "
			+ VfxSettings->TraceImpactNiagaraSystem.ToString()
			+ "\n Trace weapons fall back to the impact systems of each weapon.");
		return;
	}
	M_MaxImpactsPerFrame = FMath::Max(1, VfxSettings->MaxTraceImpactsPerFrame);
	BeginPlay_ReadEmitterSpawns(*TraceImpactSystem);
	BeginPlay_CreateNiagaraComponent(TraceImpactSystem);
}

void UTraceImpactVfxBatcher::BeginPlay_ReadEmitterSpawns(const UNiagaraSystem& TraceImpactSystem)
{
	using namespace TraceImpactVfxNames;
	const FNiagaraUserRedirectionParameterStore& UserParameterStore = TraceImpactSystem.GetExposedParameters();
	TArray<FNiagaraVariable> UserParameters;
	UserParameterStore.GetUserParameters(UserParameters);
	for (const FNiagaraVariable& UserParameter : UserParameters)
	{
		const FString ParameterName = UserParameter.GetName().ToString();
		if (UserParameter.GetType() != FNiagaraTypeDefinition::GetIntDef() || not ParameterName.EndsWith(
			PerImpactSuffix))
		{
			continue;
		}
		const FString EmitterName = ParameterName.LeftChop(FCString::Strlen(PerImpactSuffix));
		for (int32 TypeIndex = 0; TypeIndex < ImpactTypeCount; ++TypeIndex)
		{
			if (not EmitterName.StartsWith(TypePrefix[TypeIndex]))
			{
				continue;
			}
			const FNiagaraVariable StoredParameter(
				FNiagaraTypeDefinition::GetIntDef(), FName(*(TEXT("User.") + ParameterName)));
			FTraceImpactVfxEmitterSpawn EmitterSpawn;
			EmitterSpawn.TypeIndex = TypeIndex;
			EmitterSpawn.SpawnCountParameter = FName(*(EmitterName + SpawnCountSuffix));
			EmitterSpawn.ParticlesPerImpact = FMath::Max(
				1, UserParameterStore.GetParameterValue<int32>(StoredParameter));
			M_EmitterSpawns.Add(EmitterSpawn);
			break;
		}
	}
}

void UTraceImpactVfxBatcher::BeginPlay_CreateNiagaraComponent(UNiagaraSystem* TraceImpactSystem)
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
	M_NiagaraComponent->SetAsset(TraceImpactSystem);
	M_NiagaraComponent->RegisterComponent();
	M_NiagaraComponent->AddTickPrerequisiteComponent(this);
	for (int32 TypeIndex = 0; TypeIndex < ImpactTypeCount; ++TypeIndex)
	{
		M_NiagaraComponent->SetVariableInt(TraceImpactVfxNames::TypeOffset[TypeIndex], 0);
		M_NiagaraComponent->SetVariableInt(TraceImpactVfxNames::TypeCount[TypeIndex], 0);
	}
	for (const FTraceImpactVfxEmitterSpawn& EmitterSpawn : M_EmitterSpawns)
	{
		M_NiagaraComponent->SetVariableInt(EmitterSpawn.SpawnCountParameter, 0);
	}
	M_NiagaraComponent->Activate(true);
	bM_IsReadyToDraw = true;
}

bool UTraceImpactVfxBatcher::GetIsValidNiagaraComponent() const
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

void UTraceImpactVfxBatcher::AddImpact(
	const ETraceImpactVfxType ImpactType,
	const FVector& Location,
	const FVector& Normal,
	const float Scale)
{
	if (not bM_IsReadyToDraw || M_PendingImpactCount >= M_MaxImpactsPerFrame)
	{
		return;
	}
	const bool bIsSafeLocation = not Location.ContainsNaN() && not Normal.ContainsNaN()
		&& Location.GetAbsMax() < TraceImpactVfxNames::MaxSafeWorldCoordinateCm;
	if (not bIsSafeLocation)
	{
		return;
	}
	/**
	 * @brief What one impact stores in the batch: two float4 entries in the pending lists of its impact type.
	 * - M_PendingPositionAndScale: xyz = world location of the hit, w = scale of the effect.
	 * - M_PendingNormal: xyz = surface normal the effect is oriented along, w = unused.
	 * The type itself is not stored per impact; it is the list the entries are in. On the next tick the lists
	 * of all types are put behind each other and each emitter is told where its type starts and how many it has.
	 */
	const int32 TypeIndex = FMath::Clamp(static_cast<int32>(ImpactType), 0, ImpactTypeCount - 1);
	M_PendingPositionAndScale[TypeIndex].Emplace(
		static_cast<float>(Location.X), static_cast<float>(Location.Y), static_cast<float>(Location.Z), Scale);
	M_PendingNormal[TypeIndex].Emplace(
		static_cast<float>(Normal.X), static_cast<float>(Normal.Y), static_cast<float>(Normal.Z), 0.0f);
	if (M_PendingImpactCount == 0)
	{
		SetComponentTickEnabled(true);
	}
	++M_PendingImpactCount;
}

void UTraceImpactVfxBatcher::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
#if RTS_WITH_MAP_TESTS
	const FTraceImpactVfxScopedDispatchTimer DispatchTimer(false);
#endif
	if (not GetIsValidNiagaraComponent())
	{
		bM_IsReadyToDraw = false;
		SetComponentTickEnabled(false);
		return;
	}
	if (M_PendingImpactCount > 0)
	{
		PublishPendingImpacts();
		return;
	}
	// Nothing new: the system has spawned last frame's impacts, stop it from spawning them again and go idle.
	ClearPublishedImpacts();
	SetComponentTickEnabled(false);
}

void UTraceImpactVfxBatcher::PublishPendingImpacts()
{
	M_PublishedPositionAndScale.Reset();
	M_PublishedNormal.Reset();
	for (int32 TypeIndex = 0; TypeIndex < ImpactTypeCount; ++TypeIndex)
	{
		const int32 Offset = M_PublishedPositionAndScale.Num();
		const int32 Count = M_PendingPositionAndScale[TypeIndex].Num();
#if RTS_WITH_MAP_TESTS
		M_TotalPublishedByType[TypeIndex] += Count;
#endif
		M_PublishedPositionAndScale.Append(M_PendingPositionAndScale[TypeIndex]);
		M_PublishedNormal.Append(M_PendingNormal[TypeIndex]);
		// Reset keeps the memory, so a fight does not allocate every frame.
		M_PendingPositionAndScale[TypeIndex].Reset();
		M_PendingNormal[TypeIndex].Reset();
		PublishTypeRange(TypeIndex, Offset, Count);
	}
	M_PendingImpactCount = 0;
	PublishEmitterSpawnCounts();
#if RTS_WITH_MAP_TESTS
	LogPublishedTotalsWhenAsked();
#endif

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector4(
		M_NiagaraComponent,
		TraceImpactVfxNames::PositionAndScaleArray,
		TConstArrayView<FVector4f>(M_PublishedPositionAndScale));
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector4(
		M_NiagaraComponent,
		TraceImpactVfxNames::NormalArray,
		TConstArrayView<FVector4f>(M_PublishedNormal));
}

void UTraceImpactVfxBatcher::ClearPublishedImpacts()
{
	for (int32 TypeIndex = 0; TypeIndex < ImpactTypeCount; ++TypeIndex)
	{
		PublishTypeRange(TypeIndex, 0, 0);
	}
	PublishEmitterSpawnCounts();
}

void UTraceImpactVfxBatcher::PublishEmitterSpawnCounts()
{
	for (FTraceImpactVfxEmitterSpawn& EmitterSpawn : M_EmitterSpawns)
	{
		const int32 SpawnCount = M_PublishedCount[EmitterSpawn.TypeIndex] * EmitterSpawn.ParticlesPerImpact;
		if (EmitterSpawn.PublishedSpawnCount == SpawnCount)
		{
			continue;
		}
		EmitterSpawn.PublishedSpawnCount = SpawnCount;
		M_NiagaraComponent->SetVariableInt(EmitterSpawn.SpawnCountParameter, SpawnCount);
	}
}

void UTraceImpactVfxBatcher::PublishTypeRange(const int32 TypeIndex, const int32 Offset, const int32 Count)
{
	// Only what changed is handed over; most frames only one or two types have impacts.
	if (Count > 0 && M_PublishedOffset[TypeIndex] != Offset)
	{
		M_PublishedOffset[TypeIndex] = Offset;
		M_NiagaraComponent->SetVariableInt(TraceImpactVfxNames::TypeOffset[TypeIndex], Offset);
	}
	if (M_PublishedCount[TypeIndex] != Count)
	{
		M_PublishedCount[TypeIndex] = Count;
		M_NiagaraComponent->SetVariableInt(TraceImpactVfxNames::TypeCount[TypeIndex], Count);
	}
}
