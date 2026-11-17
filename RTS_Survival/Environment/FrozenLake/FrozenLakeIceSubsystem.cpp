// Copyright (C) Bas Blokzijl - All rights reserved.

#include "FrozenLakeIceSubsystem.h"

#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NavAreas/NavArea_Null.h"
#include "NavAreas/NavAreaMeta_SwitchByAgent.h"
#include "NavigationSystem.h"
#include "RTS_Survival/Interfaces/Commands.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSFrozenLakeIce, Log, All);

namespace FrozenLakeIceConstants
{
	// Material parameters of the ice master (Docs/Campaign/FrozenLakeIce.md, "State mask format").
	const FName StateMaskParameter = TEXT("StateMask");
	const FName LakeOriginParameter = TEXT("LakeOrigin");
	const FName LakeSizeParameter = TEXT("LakeSize");

	const TCHAR* const HoleWaterMaterialPath =
		TEXT("/Game/RTS_Survival/Blueprints/Environment/FrozenLake/Ice/MI_FrozenLakeWater.MI_FrozenLakeWater");
	const TCHAR* const PlaneMeshPath = TEXT("/Engine/BasicShapes/Plane.Plane");
	// The engine plane is 100 cm across at scale 1.
	constexpr float PlaneMeshSizeCm = 100.f;

	constexpr float CheckIntervalSeconds = 0.25f;
	// On ice too weak for it this long before the ice starts to give.
	constexpr float StressSeconds = 0.75f;
	// The glowing, cracking warning: a vehicle that drives off in time is safe.
	constexpr float WarningSeconds = 2.5f;
	constexpr float SinkSeconds = 6.f;
	// Nose down and a little sideways while going under; stays well under ATankMaster's 45 degree upright snap.
	constexpr float SinkPitchDegrees = 16.f;
	constexpr float SinkRollDegrees = 7.f;
	// Hull buried this far into the lake bed when it comes to rest.
	constexpr float BuryIntoBedCm = 80.f;
	constexpr float MaxSinkDepthCm = 600.f;
	constexpr float BedTraceStartBelowIceCm = 30.f;
	constexpr float BedTraceLengthCm = 1200.f;
	// A vehicle counts as on the ice when its origin is this close to the ice surface height.
	constexpr float OnIceHeightToleranceCm = 250.f;

	constexpr float WarningRadiusPerHullRadius = 1.9f;
	constexpr float HoleRadiusPerHullRadius = 1.25f;
	constexpr float MinHullRadiusCm = 200.f;
	// Disc edges: full strength inside InnerFraction of the radius, fading to zero at OuterFraction, so the material's
	// 0.5 cut sits at the radius.
	constexpr float DiscInnerFraction = 0.75f;
	constexpr float DiscOuterFraction = 1.25f;
	constexpr int32 DiscSegments = 40;

	// Water surface in a hole, under the ice; the plane is a little bigger than the hole so the ragged edge has water.
	constexpr float HoleWaterBelowIceCm = 25.f;
	constexpr float HoleWaterOversize = 1.3f;
	// The hole's Null box reaches from the lake bed to over the ice (as the lead boxes do).
	constexpr float HoleNavBottomBelowIceCm = 800.f;
	constexpr float HoleNavTopAboveIceCm = 300.f;
	constexpr float HoleNavOversize = 1.1f;
}

bool UFrozenLakeIceSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld();
}

void UFrozenLakeIceSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (not BeginPlay_FindIceSurface(InWorld))
	{
		return;
	}
	BeginPlay_ReadNavAreas(InWorld);
	if (M_Areas.IsEmpty())
	{
		UE_LOG(LogRTSFrozenLakeIce, Display, TEXT("Frozen lake ice: ice surface found but no per-agent nav areas; "
			       "the ice does not break on this map."));
		return;
	}
	if (not BeginPlay_SetupStateTarget())
	{
		return;
	}
	BeginPlay_RegisterVehicles(InWorld);
	bM_IsActive = true;
	UE_LOG(LogRTSFrozenLakeIce, Display,
	       TEXT("Frozen lake ice active: surface %s at z %.0f, %d nav areas, %d vehicles, state mask %s %.0f cm from %s"),
	       M_IceSurface.IsValid() ? *M_IceSurface->GetOwner()->GetName() : TEXT("?"), M_IceSurfaceZ, M_Areas.Num(),
	       M_Vehicles.Num(), IsValid(M_AuthoredStateMask) ? *M_AuthoredStateMask->GetName() : TEXT("?"), M_LakeSize,
	       *M_LakeOrigin.ToString());
}

void UFrozenLakeIceSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		if (M_ActorSpawnedHandle.IsValid())
		{
			World->RemoveOnActorSpawnedHandler(M_ActorSpawnedHandle);
		}
	}
	M_ActorSpawnedHandle.Reset();
	M_Vehicles.Reset();
	bM_IsActive = false;
	Super::Deinitialize();
}

void UFrozenLakeIceSubsystem::Tick(const float DeltaTime)
{
	if (not bM_IsActive)
	{
		return;
	}
	for (FFrozenLakeIceVehicle& Vehicle : M_Vehicles)
	{
		if (Vehicle.State == EFrozenLakeIceVehicleState::Sinking)
		{
			UpdateSinking(Vehicle, DeltaTime);
		}
	}
	M_SecondsSinceCheck += DeltaTime;
	if (M_SecondsSinceCheck >= FrozenLakeIceConstants::CheckIntervalSeconds)
	{
		CheckVehicles(M_SecondsSinceCheck);
		M_SecondsSinceCheck = 0.f;
	}
	if (bM_IsStateMaskDirty)
	{
		RedrawStateMask();
	}
}

TStatId UFrozenLakeIceSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UFrozenLakeIceSubsystem, STATGROUP_Tickables);
}

bool UFrozenLakeIceSubsystem::GetIsIceTooWeakFor(const ERTSNavAgents Agent, const FVector& Location) const
{
	if (FMath::Abs(Location.Z - M_IceSurfaceZ) > FrozenLakeIceConstants::OnIceHeightToleranceCm)
	{
		return false;
	}
	const uint8 AgentBit = GetAgentBit(Agent);
	for (const FFrozenLakeIceArea& Area : M_Areas)
	{
		if ((Area.BlockedAgentsMask & AgentBit) != 0 && Area.Bounds.IsInsideXY(Location))
		{
			return true;
		}
	}
	return false;
}

EFrozenLakeIceVehicleState UFrozenLakeIceSubsystem::GetVehicleState(const ATankMaster* Tank) const
{
	for (const FFrozenLakeIceVehicle& Vehicle : M_Vehicles)
	{
		if (Vehicle.Tank.Get() == Tank)
		{
			return Vehicle.State;
		}
	}
	return EFrozenLakeIceVehicleState::Safe;
}

void UFrozenLakeIceSubsystem::SetShowAuthoredStateMask(const bool bShowAuthored) const
{
	if (not GetIsValidIceMaterial() || not GetIsValidStateTarget())
	{
		return;
	}
	UTexture* const Shown = bShowAuthored ? M_AuthoredStateMask.Get() : static_cast<UTexture*>(M_StateTarget.Get());
	M_IceMaterial->SetTextureParameterValue(FrozenLakeIceConstants::StateMaskParameter, Shown);
}

bool UFrozenLakeIceSubsystem::GetIsValidStateTarget() const
{
	if (IsValid(M_StateTarget))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_StateTarget", "GetIsValidStateTarget", this);
	return false;
}

bool UFrozenLakeIceSubsystem::GetIsValidIceMaterial() const
{
	if (IsValid(M_IceMaterial))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, "M_IceMaterial", "GetIsValidIceMaterial", this);
	return false;
}

bool UFrozenLakeIceSubsystem::BeginPlay_FindIceSurface(UWorld& InWorld)
{
	using namespace FrozenLakeIceConstants;
	for (TActorIterator<AActor> It(&InWorld); It; ++It)
	{
		TInlineComponentArray<UStaticMeshComponent*> MeshComponents(*It);
		for (UStaticMeshComponent* MeshComponent : MeshComponents)
		{
			UMaterialInterface* Material = MeshComponent->GetMaterial(0);
			UTexture* StateMask = nullptr;
			float LakeSize = 0.f;
			FLinearColor LakeOrigin;
			if (not IsValid(Material) ||
				not Material->GetTextureParameterValue(FHashedMaterialParameterInfo(StateMaskParameter), StateMask) ||
				not Material->GetScalarParameterValue(FHashedMaterialParameterInfo(LakeSizeParameter), LakeSize) ||
				not Material->GetVectorParameterValue(FHashedMaterialParameterInfo(LakeOriginParameter), LakeOrigin) ||
				not IsValid(StateMask) || LakeSize <= 0.f)
			{
				continue;
			}
			M_IceSurface = MeshComponent;
			M_AuthoredStateMask = StateMask;
			M_LakeSize = LakeSize;
			M_LakeOrigin = FVector2D(LakeOrigin.R, LakeOrigin.G);
			M_IceSurfaceZ = MeshComponent->GetComponentLocation().Z;
			return true;
		}
	}
	return false;
}

void UFrozenLakeIceSubsystem::BeginPlay_ReadNavAreas(UWorld& InWorld)
{
	for (TActorIterator<AActor> It(&InWorld); It; ++It)
	{
		TInlineComponentArray<UBoxComponent*> Boxes(*It);
		for (const UBoxComponent* Box : Boxes)
		{
			if (not Box->bDynamicObstacle)
			{
				continue;
			}
			const uint8 BlockedAgentsMask = GetBlockedAgentsMask(Box->GetDesiredAreaClass().Get(), InWorld);
			if (BlockedAgentsMask == 0)
			{
				continue;
			}
			FFrozenLakeIceArea& Area = M_Areas.AddDefaulted_GetRef();
			Area.Bounds = Box->Bounds.GetBox();
			Area.BlockedAgentsMask = BlockedAgentsMask;
		}
	}
}

bool UFrozenLakeIceSubsystem::BeginPlay_SetupStateTarget()
{
	if (not M_IceSurface.IsValid())
	{
		return false;
	}
	M_StateTarget = NewObject<UTextureRenderTarget2D>(this);
	M_StateTarget->RenderTargetFormat = RTF_RGBA8;
	// The mask holds data, not colour: no gamma on the canvas draws into it, sampled linear like the authored texture.
	M_StateTarget->TargetGamma = 1.f;
	M_StateTarget->bForceLinearGamma = true;
	M_StateTarget->ClearColor = FLinearColor(0.5f, 0.f, 0.f, 0.f);
	const int32 Width = FMath::Max(1, FMath::RoundToInt(M_AuthoredStateMask->GetSurfaceWidth()));
	const int32 Height = FMath::Max(1, FMath::RoundToInt(M_AuthoredStateMask->GetSurfaceHeight()));
	M_StateTarget->InitAutoFormat(Width, Height);
	M_StateTarget->UpdateResourceImmediate(true);
	M_IceMaterial = UMaterialInstanceDynamic::Create(M_IceSurface->GetMaterial(0), this);
	if (not IsValid(M_IceMaterial))
	{
		return false;
	}
	M_IceSurface->SetMaterial(0, M_IceMaterial);
	M_IceMaterial->SetTextureParameterValue(FrozenLakeIceConstants::StateMaskParameter, M_StateTarget);
	RedrawStateMask();
	return true;
}

void UFrozenLakeIceSubsystem::BeginPlay_RegisterVehicles(UWorld& InWorld)
{
	for (TActorIterator<ATankMaster> It(&InWorld); It; ++It)
	{
		RegisterVehicle(*It);
	}
	M_ActorSpawnedHandle = InWorld.AddOnActorSpawnedHandler(
		FOnActorSpawned::FDelegate::CreateUObject(this, &UFrozenLakeIceSubsystem::OnActorSpawned));
}

uint8 UFrozenLakeIceSubsystem::GetBlockedAgentsMask(const UClass* AreaClass, const UWorld& InWorld)
{
	if (AreaClass == nullptr)
	{
		return 0;
	}
	if (AreaClass->IsChildOf(UNavArea_Null::StaticClass()))
	{
		return GetAgentBit(ERTSNavAgents::Character) | GetAgentBit(ERTSNavAgents::LightTank) |
			GetAgentBit(ERTSNavAgents::MediumTank) | GetAgentBit(ERTSNavAgents::HeavyTank);
	}
	const UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(&InWorld);
	if (not AreaClass->IsChildOf(UNavAreaMeta_SwitchByAgent::StaticClass()) || not IsValid(NavigationSystem))
	{
		return 0;
	}
	// The meta area names one area per navmesh agent (DefaultEngine.ini order); Null for an agent keeps it out.
	const UObject* MetaArea = AreaClass->GetDefaultObject();
	const TArray<FNavDataConfig>& Agents = NavigationSystem->GetSupportedAgents();
	uint8 Mask = 0;
	for (int32 AgentIndex = 0; AgentIndex < Agents.Num(); ++AgentIndex)
	{
		const FClassProperty* AreaProperty = FindFProperty<FClassProperty>(
			UNavAreaMeta_SwitchByAgent::StaticClass(), *FString::Printf(TEXT("Agent%dArea"), AgentIndex));
		if (AreaProperty == nullptr)
		{
			break;
		}
		const UClass* AgentArea = Cast<UClass>(AreaProperty->GetObjectPropertyValue_InContainer(MetaArea));
		if (AgentArea == nullptr || not AgentArea->IsChildOf(UNavArea_Null::StaticClass()))
		{
			continue;
		}
		const FString AgentName = Agents[AgentIndex].Name.ToString();
		for (const ERTSNavAgents Agent : {ERTSNavAgents::Character, ERTSNavAgents::LightTank,
		                                 ERTSNavAgents::MediumTank, ERTSNavAgents::HeavyTank})
		{
			if (Global_GetRTSNavAgentName(Agent) == AgentName)
			{
				Mask |= GetAgentBit(Agent);
			}
		}
	}
	return Mask;
}

uint8 UFrozenLakeIceSubsystem::GetAgentBit(const ERTSNavAgents Agent)
{
	return static_cast<uint8>(1u << static_cast<uint8>(Agent));
}

void UFrozenLakeIceSubsystem::OnActorSpawned(AActor* SpawnedActor)
{
	if (ATankMaster* Tank = Cast<ATankMaster>(SpawnedActor))
	{
		RegisterVehicle(Tank);
	}
}

void UFrozenLakeIceSubsystem::RegisterVehicle(ATankMaster* Tank)
{
	if (not IsValid(Tank))
	{
		return;
	}
	const ERTSNavAgents Agent = Tank->GetRTSNavAgentType();
	if (Agent == ERTSNavAgents::NONE || Agent == ERTSNavAgents::Character)
	{
		return;
	}
	for (const FFrozenLakeIceVehicle& Existing : M_Vehicles)
	{
		if (Existing.Tank.Get() == Tank)
		{
			return;
		}
	}
	FFrozenLakeIceVehicle& Vehicle = M_Vehicles.AddDefaulted_GetRef();
	Vehicle.Tank = Tank;
	Vehicle.Agent = Agent;
}

void UFrozenLakeIceSubsystem::CheckVehicles(const float DeltaSeconds)
{
	for (int32 Index = M_Vehicles.Num() - 1; Index >= 0; --Index)
	{
		if (not M_Vehicles[Index].Tank.IsValid())
		{
			M_Vehicles.RemoveAtSwap(Index);
			continue;
		}
		UpdateVehicle(M_Vehicles[Index], DeltaSeconds);
	}
}

void UFrozenLakeIceSubsystem::UpdateVehicle(FFrozenLakeIceVehicle& Vehicle, const float DeltaSeconds)
{
	ATankMaster* Tank = Vehicle.Tank.Get();
	const bool bIsGoingOrGone = Vehicle.State == EFrozenLakeIceVehicleState::Sinking ||
		Vehicle.State == EFrozenLakeIceVehicleState::Sunk;
	if (bIsGoingOrGone || not Tank->IsUnitAlive() || Tank->GetAttachParentActor() != nullptr)
	{
		return;
	}
	Vehicle.SecondsInState += DeltaSeconds;
	const bool bIsTooWeak = GetIsIceTooWeakFor(Vehicle.Agent, Tank->GetActorLocation());
	if (not bIsTooWeak)
	{
		if (Vehicle.State != EFrozenLakeIceVehicleState::Safe)
		{
			// Drove off in time: a warning glow fades with the redraw.
			bM_IsStateMaskDirty |= Vehicle.State == EFrozenLakeIceVehicleState::Warning;
			SetVehicleState(Vehicle, EFrozenLakeIceVehicleState::Safe);
		}
		return;
	}
	if (Vehicle.State == EFrozenLakeIceVehicleState::Safe)
	{
		SetVehicleState(Vehicle, EFrozenLakeIceVehicleState::Stressed);
		return;
	}
	if (Vehicle.State == EFrozenLakeIceVehicleState::Stressed &&
		Vehicle.SecondsInState >= FrozenLakeIceConstants::StressSeconds)
	{
		Vehicle.BreakCentre = FVector2D(Tank->GetActorLocation());
		Vehicle.HoleRadius = GetHullRadius(*Tank) * FrozenLakeIceConstants::HoleRadiusPerHullRadius;
		bM_IsStateMaskDirty = true;
		SetVehicleState(Vehicle, EFrozenLakeIceVehicleState::Warning);
		return;
	}
	if (Vehicle.State == EFrozenLakeIceVehicleState::Warning &&
		Vehicle.SecondsInState >= FrozenLakeIceConstants::WarningSeconds)
	{
		BreakIce(Vehicle, *Tank);
	}
}

void UFrozenLakeIceSubsystem::SetVehicleState(FFrozenLakeIceVehicle& Vehicle,
                                              const EFrozenLakeIceVehicleState NewState)
{
	Vehicle.State = NewState;
	Vehicle.SecondsInState = 0.f;
	ATankMaster* Tank = Vehicle.Tank.Get();
	UE_LOG(LogRTSFrozenLakeIce, Display, TEXT("Frozen lake ice: %s (%s) -> %s"),
	       IsValid(Tank) ? *Tank->GetName() : TEXT("?"), *Global_GetRTSNavAgentName(Vehicle.Agent),
	       *UEnum::GetValueAsString(NewState));
	M_OnVehicleStateChanged.Broadcast(Tank, NewState);
}

void UFrozenLakeIceSubsystem::BreakIce(FFrozenLakeIceVehicle& Vehicle, ATankMaster& Tank)
{
	using namespace FrozenLakeIceConstants;
	// The hole opens where the hull is now (it may have crept on during the warning).
	Vehicle.BreakCentre = FVector2D(Tank.GetActorLocation());
	const float HullRadius = GetHullRadius(Tank);
	Vehicle.HoleRadius = HullRadius * HoleRadiusPerHullRadius;
	FFrozenLakeIceDisc& Hole = M_Holes.AddDefaulted_GetRef();
	Hole.Centre = Vehicle.BreakCentre;
	Hole.Radius = Vehicle.HoleRadius;
	// Anything else that rolls into the hole goes under too.
	FFrozenLakeIceArea& HoleArea = M_Areas.AddDefaulted_GetRef();
	HoleArea.Bounds = FBox(FVector(Hole.Centre - FVector2D(Hole.Radius), M_IceSurfaceZ - HoleNavBottomBelowIceCm),
	                       FVector(Hole.Centre + FVector2D(Hole.Radius), M_IceSurfaceZ + HoleNavTopAboveIceCm));
	HoleArea.BlockedAgentsMask = GetAgentBit(ERTSNavAgents::LightTank) | GetAgentBit(ERTSNavAgents::MediumTank) |
		GetAgentBit(ERTSNavAgents::HeavyTank);
	SpawnHoleActor(Hole.Centre, Hole.Radius);
	Vehicle.SinkStartLocation = Tank.GetActorLocation();
	Vehicle.SinkStartRotation = Tank.GetActorRotation();
	Vehicle.SinkDepth = GetSinkDepth(Tank);
	DisableForSinking(Tank);
	bM_IsStateMaskDirty = true;
	SetVehicleState(Vehicle, EFrozenLakeIceVehicleState::Sinking);
}

float UFrozenLakeIceSubsystem::GetHullRadius(const ATankMaster& Tank)
{
	const UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Tank.GetRootComponent());
	if (not IsValid(Root))
	{
		return FrozenLakeIceConstants::MinHullRadiusCm;
	}
	const FVector Extent = Root->Bounds.BoxExtent;
	return FMath::Max(FrozenLakeIceConstants::MinHullRadiusCm, FMath::Max(Extent.X, Extent.Y));
}

float UFrozenLakeIceSubsystem::GetSinkDepth(const ATankMaster& Tank) const
{
	using namespace FrozenLakeIceConstants;
	const UWorld* World = GetWorld();
	const FVector Location = Tank.GetActorLocation();
	if (not IsValid(World))
	{
		return MaxSinkDepthCm;
	}
	// Start under the ice so the trace finds the lake bed, not the ice the hull stood on.
	const FVector TraceStart(Location.X, Location.Y, M_IceSurfaceZ - BedTraceStartBelowIceCm);
	const FVector TraceEnd = TraceStart - FVector(0.f, 0.f, BedTraceLengthCm);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FrozenLakeIceBed), false, &Tank);
	if (not World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
	{
		return MaxSinkDepthCm;
	}
	return FMath::Clamp(Location.Z - Hit.ImpactPoint.Z + BuryIntoBedCm, 0.f, MaxSinkDepthCm);
}

void UFrozenLakeIceSubsystem::DisableForSinking(ATankMaster& Tank)
{
	if (ICommands* Commands = Cast<ICommands>(&Tank))
	{
		Commands->StopCommand();
	}
	if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Tank.GetRootComponent()))
	{
		Root->SetSimulatePhysics(false);
	}
	Tank.SetActorEnableCollision(false);
}

void UFrozenLakeIceSubsystem::UpdateSinking(FFrozenLakeIceVehicle& Vehicle, const float DeltaSeconds)
{
	using namespace FrozenLakeIceConstants;
	ATankMaster* Tank = Vehicle.Tank.Get();
	if (not IsValid(Tank))
	{
		Vehicle.State = EFrozenLakeIceVehicleState::Sunk;
		return;
	}
	Vehicle.SecondsInState += DeltaSeconds;
	const float Alpha = FMath::Clamp(Vehicle.SecondsInState / SinkSeconds, 0.f, 1.f);
	// Slow at first (the ice gives), fast through the middle, settling on the bed.
	const float Ease = FMath::SmoothStep(0.f, 1.f, Alpha);
	const FVector Location = Vehicle.SinkStartLocation - FVector(0.f, 0.f, Vehicle.SinkDepth * Ease);
	const FRotator Rotation = Vehicle.SinkStartRotation + FRotator(-SinkPitchDegrees * Ease, 0.f,
	                                                               SinkRollDegrees * Ease);
	Tank->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	if (Alpha < 1.f)
	{
		return;
	}
	FinishSinking(*Tank);
	SetVehicleState(Vehicle, EFrozenLakeIceVehicleState::Sunk);
}

void UFrozenLakeIceSubsystem::FinishSinking(ATankMaster& Tank)
{
	Tank.DieSunkThroughIce();
	if (IsValid(&Tank))
	{
		Tank.SetActorHiddenInGame(true);
	}
}

void UFrozenLakeIceSubsystem::SpawnHoleActor(const FVector2D& Centre, const float Radius)
{
	using namespace FrozenLakeIceConstants;
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector WaterLocation(Centre, M_IceSurfaceZ - HoleWaterBelowIceCm);
	AStaticMeshActor* HoleActor = World->SpawnActor<AStaticMeshActor>(WaterLocation, FRotator::ZeroRotator,
	                                                                  SpawnParameters);
	if (not IsValid(HoleActor))
	{
		return;
	}
	UStaticMeshComponent* Water = HoleActor->GetStaticMeshComponent();
	Water->SetMobility(EComponentMobility::Movable);
	Water->SetCanEverAffectNavigation(false);
	Water->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Water->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, PlaneMeshPath));
	Water->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr, HoleWaterMaterialPath));
	const float WaterScale = 2.f * Radius * HoleWaterOversize / PlaneMeshSizeCm;
	Water->SetWorldScale3D(FVector(WaterScale, WaterScale, 1.f));

	// Null for every agent from the lake bed to over the ice: units route round the hole (DynamicModifiersOnly nav).
	UBoxComponent* NavBox = NewObject<UBoxComponent>(HoleActor);
	NavBox->SetupAttachment(Water);
	NavBox->SetUsingAbsoluteScale(true);
	NavBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NavBox->SetHiddenInGame(true);
	NavBox->bDynamicObstacle = true;
	NavBox->SetAreaClassOverride(UNavArea_Null::StaticClass());
	NavBox->SetCanEverAffectNavigation(true);
	const float HalfHeight = (HoleNavBottomBelowIceCm + HoleNavTopAboveIceCm) * 0.5f;
	const float BoxCentreZ = M_IceSurfaceZ + (HoleNavTopAboveIceCm - HoleNavBottomBelowIceCm) * 0.5f;
	NavBox->SetBoxExtent(FVector(Radius * HoleNavOversize, Radius * HoleNavOversize, HalfHeight), false);
	NavBox->SetWorldLocation(FVector(Centre, BoxCentreZ));
	HoleActor->AddInstanceComponent(NavBox);
	NavBox->RegisterComponent();
	M_HoleActors.Add(HoleActor);
	UE_LOG(LogRTSFrozenLakeIce, Display, TEXT("Frozen lake ice: hole %.0f cm at %s"), Radius, *Centre.ToString());
}

TArray<FFrozenLakeIceDisc> UFrozenLakeIceSubsystem::GetWarningDiscs() const
{
	TArray<FFrozenLakeIceDisc> Warnings;
	for (const FFrozenLakeIceVehicle& Vehicle : M_Vehicles)
	{
		const ATankMaster* Tank = Vehicle.Tank.Get();
		if (Vehicle.State != EFrozenLakeIceVehicleState::Warning || not IsValid(Tank))
		{
			continue;
		}
		FFrozenLakeIceDisc& Disc = Warnings.AddDefaulted_GetRef();
		Disc.Centre = FVector2D(Tank->GetActorLocation());
		Disc.Radius = GetHullRadius(*Tank) * FrozenLakeIceConstants::WarningRadiusPerHullRadius;
	}
	return Warnings;
}

void UFrozenLakeIceSubsystem::RedrawStateMask()
{
	bM_IsStateMaskDirty = false;
	if (not GetIsValidStateTarget() || not IsValid(M_AuthoredStateMask))
	{
		return;
	}
	UCanvas* Canvas = nullptr;
	FVector2D Size;
	FDrawToRenderTargetContext Context;
	UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(GetWorld(), M_StateTarget, Canvas, Size, Context);
	if (Canvas == nullptr)
	{
		return;
	}
	// The authored state replaces everything (opaque, all four channels), then the glows and holes add on top.
	FCanvasTileItem Base(FVector2D::ZeroVector, M_AuthoredStateMask->GetResource(), Size, FLinearColor::White);
	Base.BlendMode = SE_BLEND_Opaque;
	Canvas->DrawItem(Base);
	DrawDiscs(*Canvas, GetWarningDiscs(), FLinearColor(0.f, 1.f, 0.f, 0.f));
	DrawDiscs(*Canvas, M_Holes, FLinearColor(0.f, 0.f, 1.f, 0.f));
	UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(GetWorld(), Context);
}

void UFrozenLakeIceSubsystem::DrawDiscs(UCanvas& Canvas, const TArray<FFrozenLakeIceDisc>& Discs,
                                        const FLinearColor& Colour) const
{
	using namespace FrozenLakeIceConstants;
	if (Discs.IsEmpty())
	{
		return;
	}
	TArray<FCanvasUVTri> Triangles;
	Triangles.Reserve(Discs.Num() * DiscSegments * 3);
	const FLinearColor Clear(0.f, 0.f, 0.f, 0.f);
	const float PixelsPerCm = M_StateTarget->SizeX / M_LakeSize;
	for (const FFrozenLakeIceDisc& Disc : Discs)
	{
		const FVector2D Centre = WorldToStatePixel(Disc.Centre);
		const float Inner = Disc.Radius * DiscInnerFraction * PixelsPerCm;
		const float Outer = Disc.Radius * DiscOuterFraction * PixelsPerCm;
		for (int32 Segment = 0; Segment < DiscSegments; ++Segment)
		{
			const float AngleA = 2.f * PI * Segment / DiscSegments;
			const float AngleB = 2.f * PI * (Segment + 1) / DiscSegments;
			const FVector2D DirA(FMath::Cos(AngleA), FMath::Sin(AngleA));
			const FVector2D DirB(FMath::Cos(AngleB), FMath::Sin(AngleB));
			FCanvasUVTri& Core = Triangles.AddDefaulted_GetRef();
			Core.V0_Pos = Centre;
			Core.V1_Pos = Centre + DirA * Inner;
			Core.V2_Pos = Centre + DirB * Inner;
			Core.V0_Color = Core.V1_Color = Core.V2_Color = Colour;
			// The soft rim: full strength at the inner ring, nothing at the outer ring.
			FCanvasUVTri& RimA = Triangles.AddDefaulted_GetRef();
			RimA.V0_Pos = Centre + DirA * Inner;
			RimA.V1_Pos = Centre + DirA * Outer;
			RimA.V2_Pos = Centre + DirB * Outer;
			RimA.V0_Color = Colour;
			RimA.V1_Color = RimA.V2_Color = Clear;
			FCanvasUVTri& RimB = Triangles.AddDefaulted_GetRef();
			RimB.V0_Pos = Centre + DirA * Inner;
			RimB.V1_Pos = Centre + DirB * Outer;
			RimB.V2_Pos = Centre + DirB * Inner;
			RimB.V0_Color = RimB.V2_Color = Colour;
			RimB.V1_Color = Clear;
		}
	}
	FCanvasTriangleItem Item(Triangles, GWhiteTexture);
	Item.BlendMode = SE_BLEND_Additive;
	Canvas.DrawItem(Item);
}

FVector2D UFrozenLakeIceSubsystem::WorldToStatePixel(const FVector2D& WorldLocation) const
{
	// u follows world X (north), v follows world Y (east): image right is north, image down is east.
	const FVector2D Uv = (WorldLocation - M_LakeOrigin) / M_LakeSize;
	return FVector2D(Uv.X * M_StateTarget->SizeX, Uv.Y * M_StateTarget->SizeY);
}
