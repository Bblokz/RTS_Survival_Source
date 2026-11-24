// Copyright (C) Bas Blokzijl - All rights reserved.

#include "MissionFuelCrane.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace MissionFuelCraneConstants
{
	// SM_FuelCrane_Cable is 1 m long; SM_FuelCrane_Sling spans a 1 m square and 1 m height.
	constexpr float CableMeshUnitLength = 100.f;
	constexpr float SlingMeshUnitSize = 100.f;
	constexpr float GroundTraceUp = 300.f;
	constexpr float GroundTraceDown = 600.f;
	constexpr float MinCableScale = 0.01f;
	constexpr float SwayRollFraction = 0.4f;
	constexpr float SwayRestThreshold = 0.02f;
	// Sway dies out faster while the hook is being lowered onto a target.
	constexpr float LoweringSwayDampingMultiplier = 4.f;
	constexpr float EaseExponent = 2.f;
	constexpr float FullTurnDegrees = 360.f;

	// Dies to this many Pz IV KwK 40 shots (see DeveloperSettings SovietBuildingShotHealth).
	constexpr int32 MediumTankShotsToKill = 3;

	// Weapons aim above the 3 m base wall at the mast, jib root and cab instead of the ground at the mast foot.
	const TArray<FVector> AimOffsets = {
		FVector(0.f, 0.f, 450.f), FVector(0.f, 0.f, 650.f), FVector(0.f, 0.f, 880.f), FVector(0.f, 0.f, 350.f)
	};

	// Default death: large destruction smoke, 8 s sink, one big scorch under the mast.
	FMissionPropSinkDeathSettings MakeDefaultDeathSettings()
	{
		FMissionPropSinkDeathSettings Settings;
		Settings.SmokeSystem = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT(
			"/Game/RTS_Survival/Blueprints/Environment/DestructableEnvActor/Effects/NS_DestructionSmoke_Large."
			"NS_DestructionSmoke_Large")));
		Settings.SinkSeconds = 8.f;
		// Mast 8.4 m + jib 3.7 m above it: sink the full height so nothing stays above ground.
		Settings.SinkDepth = 1300.f;
		Settings.MaxTiltDegrees = 6.f;
		FMissionPropDeathDecal Scorch;
		Scorch.Materials = {
			TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT(
				"/Game/Crater_Decals/Decals/Scorched/MI_Decal_Scorch_03.MI_Decal_Scorch_03"))),
			TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT(
				"/Game/Crater_Decals/Decals/Scorched/MI_Decal_Scorch_04.MI_Decal_Scorch_04")))
		};
		Scorch.Count = 1;
		Scorch.SizeRange = FVector2D(650.f, 750.f);
		Settings.Decals.Add(Scorch);
		return Settings;
	}
}

AMissionFuelCrane::AMissionFuelCrane(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	M_Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = M_Root;

	M_BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
	M_BaseMesh->SetupAttachment(M_Root);
	// The base is the only part that blocks: a small footprint around the mast.
	M_BaseMesh->SetCanEverAffectNavigation(true);

	M_SlewPivot = CreateDefaultSubobject<USceneComponent>(TEXT("SlewPivot"));
	M_SlewPivot->SetupAttachment(M_Root);

	M_JibMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("JibMesh"));
	M_JibMesh->SetupAttachment(M_SlewPivot);

	M_TrolleyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TrolleyMesh"));
	M_TrolleyMesh->SetupAttachment(M_SlewPivot);

	M_SwayPivot = CreateDefaultSubobject<USceneComponent>(TEXT("SwayPivot"));
	M_SwayPivot->SetupAttachment(M_TrolleyMesh);

	M_CableMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CableMesh"));
	M_CableMesh->SetupAttachment(M_SwayPivot);

	M_HookMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HookMesh"));
	M_HookMesh->SetupAttachment(M_SwayPivot);

	M_SlingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SlingMesh"));
	M_SlingMesh->SetupAttachment(M_HookMesh);
	M_SlingMesh->SetVisibility(false);

	M_CarriedCargoMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CarriedCargoMesh"));
	M_CarriedCargoMesh->SetupAttachment(M_HookMesh);
	M_CarriedCargoMesh->SetVisibility(false);

	ConfigureMovingPart(M_JibMesh);
	ConfigureMovingPart(M_TrolleyMesh);
	ConfigureMovingPart(M_CableMesh);
	ConfigureMovingPart(M_HookMesh);
	ConfigureMovingPart(M_SlingMesh);
	ConfigureMovingPart(M_CarriedCargoMesh);

	M_MastHitBox = CreateDefaultSubobject<UBoxComponent>(TEXT("MastHitBox"));
	M_MastHitBox->SetupAttachment(M_Root);
	ConfigureHitBox(M_MastHitBox, true);

	M_JibHitBox = CreateDefaultSubobject<UBoxComponent>(TEXT("JibHitBox"));
	M_JibHitBox->SetupAttachment(M_SlewPivot);
	ConfigureHitBox(M_JibHitBox, false);

	{
		using namespace DeveloperSettings::GameBalance::SovietBuildingShotHealth;
		Health = static_cast<int32>((MissionFuelCraneConstants::MediumTankShotsToKill - ShotBracketCentreOffset)
			* MediumTankShotDamage / HealthRoundingStep + 0.5f) * HealthRoundingStep;
	}
	AimOffsetPoints = MissionFuelCraneConstants::AimOffsets;
	DeathSettings = MissionFuelCraneConstants::MakeDefaultDeathSettings();
}

void AMissionFuelCrane::OnUnitDies(const ERTSDeathType DeathType)
{
	if (not IsUnitAlive())
	{
		return;
	}
	Super::OnUnitDies(DeathType);
	if (not IsValid(this) || IsActorBeingDestroyed())
	{
		return;
	}
	StartCraneDeath();
}

void AMissionFuelCrane::StartCraneDeath()
{
	if (bM_DeathStarted)
	{
		return;
	}
	bM_DeathStarted = true;
	StopUnloading();
	bM_FinishCycleThenStop = false;
	SetActorTickEnabled(false);
	SetHitBoxesEnabled(false);
	// The sink also stops BaseMesh from blocking weapons; it still blocks units while it goes down.
	FMissionPropSinkDeath::Start(this, DeathSettings);
}

void AMissionFuelCrane::ConfigureHitBox(UBoxComponent* HitBox, const bool bBlockCursor) const
{
	if (not IsValid(HitBox))
	{
		return;
	}
	HitBox->SetHiddenInGame(true);
	HitBox->SetCanEverAffectNavigation(false);
	HitBox->SetGenerateOverlapEvents(false);
	HitBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HitBox->SetCollisionObjectType(ECC_Destructible);
	HitBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	// Cursor: hovering or right-clicking the crane resolves to an attack on it.
	HitBox->SetCollisionResponseToChannel(ECC_Visibility, bBlockCursor ? ECR_Block : ECR_Ignore);
	HitBox->SetCollisionResponseToChannel(COLLISION_OBJ_PROJECTILE, ECR_Block);
	HitBox->SetCollisionResponseToChannel(COLLISION_TRACE_ENEMY, ECR_Block);
	HitBox->SetCollisionResponseToChannel(COLLISION_TRACE_PLAYER, ECR_Block);
	HitBox->SetCollisionResponseToChannel(COLLISION_OBJ_BUILDING_PLACEMENT, ECR_Overlap);
}

void AMissionFuelCrane::ApplyHitBoxLayout() const
{
	if (IsValid(M_MastHitBox))
	{
		M_MastHitBox->SetBoxExtent(MastHitBoxExtent);
		M_MastHitBox->SetRelativeLocation(FVector(0.f, 0.f, MastHitBoxExtent.Z));
	}
	if (IsValid(M_JibHitBox))
	{
		// Jib mesh pivot sits on the slew axis; fall back to SM_FuelCrane_Jib's bounds without a mesh.
		FBox JibBounds(FVector(-450.f, -135.f, 0.f), FVector(1235.f, 85.f, 367.f));
		if (IsValid(M_JibMesh) && IsValid(M_JibMesh->GetStaticMesh()))
		{
			JibBounds = M_JibMesh->GetStaticMesh()->GetBoundingBox();
		}
		M_JibHitBox->SetBoxExtent(JibBounds.GetExtent() + JibHitBoxPadding);
		M_JibHitBox->SetRelativeLocation(JibBounds.GetCenter());
	}
}

void AMissionFuelCrane::SetHitBoxesEnabled(const bool bEnabled) const
{
	const ECollisionEnabled::Type Collision = bEnabled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision;
	if (IsValid(M_MastHitBox))
	{
		M_MastHitBox->SetCollisionEnabled(Collision);
	}
	if (IsValid(M_JibHitBox))
	{
		M_JibHitBox->SetCollisionEnabled(Collision);
	}
}

void AMissionFuelCrane::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (not bM_IsUnloading)
	{
		return;
	}
	if (GetIsTruckLost())
	{
		OnTruckLost();
		if (not bM_IsUnloading)
		{
			return;
		}
	}

	const float ScaledDelta = DeltaSeconds * SpeedMultiplier;
	M_Move.Elapsed += ScaledDelta;
	const float Alpha = FMath::Clamp(M_Move.Elapsed / FMath::Max(M_Move.Duration, KINDA_SMALL_NUMBER), 0.f, 1.f);
	const float Eased = FMath::InterpEaseInOut(0.f, 1.f, Alpha, MissionFuelCraneConstants::EaseExponent);

	FMissionFuelCranePose Pose;
	Pose.YawDegrees = M_Move.Start.YawDegrees + M_Move.YawDelta * Eased;
	Pose.TrolleyRadius = FMath::Lerp(M_Move.Start.TrolleyRadius, M_Move.Target.TrolleyRadius, Eased);
	Pose.HookDepth = FMath::Lerp(M_Move.Start.HookDepth, M_Move.Target.HookDepth, Eased);
	ApplyPose(Pose);
	TickSway(ScaledDelta);

	if (Alpha >= 1.f)
	{
		M_CurrentPose.YawDegrees = FRotator::NormalizeAxis(M_CurrentPose.YawDegrees);
		OnPhaseFinished();
	}
}

void AMissionFuelCrane::StartUnloading(AActor* Truck)
{
	if (not GetIsValidRig())
	{
		return;
	}
	if (M_StackPieces.Num() == 0)
	{
		// Called before BeginPlay (e.g. from a construction script); pools are created in BeginPlay.
		BeginPlay_CreatePiecePools();
		BeginPlay_InitStackGround();
	}

	M_SourcePieces.Reset();
	M_Carry.TargetSourceIndex = INDEX_NONE;
	bM_SourcesAreTruckCargo = false;
	bM_DependsOnTruck = false;
	M_Truck = Truck;

	bool bHasSources = false;
	if (RTSFunctionLibrary::RTSIsValid(Truck))
	{
		bHasSources = GatherTruckCargoPieces(Truck);
		if (not bHasSources)
		{
			const FTransform TruckTransform = Truck->GetActorTransform();
			const FVector BedCenter = TruckTransform.TransformPosition(TruckBedLocalOffset);
			if (GetIsWithinReach(BedCenter))
			{
				bHasSources = UsePileAt(FTransform(TruckTransform.GetRotation(), BedCenter), false);
				bM_DependsOnTruck = bHasSources;
			}
		}
		if (not bHasSources)
		{
			UE_LOG(LogRTS, Warning, TEXT("%s: truck %s has no cargo within %.0f cm, using the pick-up point pile."),
			       *GetName(), *Truck->GetName(), MaxTrolleyRadius);
		}
	}
	if (not bHasSources)
	{
		const FTransform PileFrame(GetActorRotation(), GetActorTransform().TransformPosition(PickupPointLocation));
		bHasSources = UsePileAt(PileFrame, bTraceDropGround);
	}
	if (not bHasSources)
	{
		RTSFunctionLibrary::ReportError(GetName() + TEXT(": StartUnloading found no cargo to lift. ")
			TEXT("Set CargoMesh on the Blueprint or pass a truck with \"") + TruckCargoComponentPrefix
			+ TEXT("*\" components."));
		return;
	}

	bM_IsUnloading = true;
	bM_FinishCycleThenStop = false;
	SetActorTickEnabled(true);
	BeginPhase(M_Carry.bIsCarrying ? EMissionFuelCranePhase::SlewToDrop : EMissionFuelCranePhase::SlewToPickup);
}

void AMissionFuelCrane::StopUnloading()
{
	Halt();
}

bool AMissionFuelCrane::GetIsUnloading() const
{
	return bM_IsUnloading;
}

int32 AMissionFuelCrane::GetUnloadedCount() const
{
	return M_UnloadedCount;
}

void AMissionFuelCrane::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (not GetIsValidRig())
	{
		return;
	}
	ApplyRigLayout();
	ApplyHitBoxLayout();
	ApplyPose(GetIdlePose());
}

void AMissionFuelCrane::BeginPlay()
{
	Super::BeginPlay();
	FMissionPropSinkDeath::PreloadAssets(DeathSettings, M_PreloadedDeathAssets);
	if (not GetIsValidRig())
	{
		return;
	}
	ApplyRigLayout();
	ApplyHitBoxLayout();
	ApplyPose(GetIdlePose());
	BeginPlay_CreatePiecePools();
	BeginPlay_InitStackGround();
	if (bStartWithoutTruckOnBeginPlay)
	{
		StartUnloading(nullptr);
	}
}

void AMissionFuelCrane::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The stack leaves with the crane, so a surviving truck gets its cargo back.
	RestoreTruckCargoVisibility();
	Halt();
	Super::EndPlay(EndPlayReason);
}

bool AMissionFuelCrane::GetIsValidRig() const
{
	const UObject* const Parts[] = {
		M_Root.Get(), M_BaseMesh.Get(), M_SlewPivot.Get(), M_JibMesh.Get(), M_TrolleyMesh.Get(),
		M_SwayPivot.Get(), M_CableMesh.Get(), M_HookMesh.Get(), M_SlingMesh.Get(), M_CarriedCargoMesh.Get()
	};
	const TCHAR* const PartNames[] = {
		TEXT("M_Root"), TEXT("M_BaseMesh"), TEXT("M_SlewPivot"), TEXT("M_JibMesh"), TEXT("M_TrolleyMesh"),
		TEXT("M_SwayPivot"), TEXT("M_CableMesh"), TEXT("M_HookMesh"), TEXT("M_SlingMesh"), TEXT("M_CarriedCargoMesh")
	};
	static_assert(UE_ARRAY_COUNT(Parts) == UE_ARRAY_COUNT(PartNames), "Every rig part needs a name.");
	for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(Parts)); ++Index)
	{
		if (IsValid(Parts[Index]))
		{
			continue;
		}
		RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, PartNames[Index], TEXT("GetIsValidRig"), this);
		return false;
	}
	return true;
}

void AMissionFuelCrane::ConfigureMovingPart(UStaticMeshComponent* Part) const
{
	if (not IsValid(Part))
	{
		return;
	}
	Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Part->SetCanEverAffectNavigation(false);
	Part->SetGenerateOverlapEvents(false);
}

void AMissionFuelCrane::ApplyRigLayout() const
{
	M_SlewPivot->SetRelativeLocation(FVector(0.f, 0.f, SlewPivotHeight));
	M_SwayPivot->SetRelativeLocation(FVector(0.f, 0.f, -CableOriginOffset));
	M_SlingMesh->SetRelativeLocation(FVector(0.f, 0.f, -HookEyeOffset));
}

FMissionFuelCranePose AMissionFuelCrane::GetIdlePose() const
{
	FMissionFuelCranePose Pose;
	Pose.YawDegrees = IdleJibYaw;
	Pose.TrolleyRadius = FMath::Clamp(IdleTrolleyRadius, MinTrolleyRadius, MaxTrolleyRadius);
	Pose.HookDepth = TravelHookDepth;
	return Pose;
}

void AMissionFuelCrane::ApplyPose(const FMissionFuelCranePose& Pose)
{
	M_CurrentPose = Pose;
	M_SlewPivot->SetRelativeRotation(FRotator(0.f, Pose.YawDegrees, 0.f));
	M_TrolleyMesh->SetRelativeLocation(FVector(Pose.TrolleyRadius, 0.f, TrolleyRailHeight));
	const float CableScale = FMath::Max(Pose.HookDepth / MissionFuelCraneConstants::CableMeshUnitLength,
	                                    MissionFuelCraneConstants::MinCableScale);
	M_CableMesh->SetRelativeScale3D(FVector(1.f, 1.f, CableScale));
	M_HookMesh->SetRelativeLocation(FVector(0.f, 0.f, -Pose.HookDepth));
}

void AMissionFuelCrane::TickSway(const float DeltaSeconds)
{
	if (M_Sway.AmplitudeDegrees < MissionFuelCraneConstants::SwayRestThreshold)
	{
		return;
	}
	const bool bIsLowering = M_Phase == EMissionFuelCranePhase::LowerToPickup
		or M_Phase == EMissionFuelCranePhase::LowerToDrop;
	const float Damping = SwayDamping * (bIsLowering ? MissionFuelCraneConstants::LoweringSwayDampingMultiplier : 1.f);
	M_Sway.TimeSeconds += DeltaSeconds;
	M_Sway.AmplitudeDegrees *= FMath::Exp(-Damping * DeltaSeconds);
	if (M_Sway.AmplitudeDegrees < MissionFuelCraneConstants::SwayRestThreshold)
	{
		SnapSwayToRest();
		return;
	}
	SetSwayRotation(M_Sway.AmplitudeDegrees);
}

void AMissionFuelCrane::SetSwayRotation(const float AmplitudeDegrees) const
{
	const float Phase = 2.f * PI * SwayFrequencyHz * M_Sway.TimeSeconds;
	const float Pitch = AmplitudeDegrees * FMath::Sin(Phase);
	const float Roll = AmplitudeDegrees * MissionFuelCraneConstants::SwayRollFraction * FMath::Cos(Phase);
	M_SwayPivot->SetRelativeRotation(FRotator(Pitch, 0.f, Roll));
}

void AMissionFuelCrane::SnapSwayToRest()
{
	M_Sway.AmplitudeDegrees = 0.f;
	M_SwayPivot->SetRelativeRotation(FRotator::ZeroRotator);
}

void AMissionFuelCrane::BeginPlay_CreatePiecePools()
{
	for (UStaticMeshComponent* Piece : M_StackPieces)
	{
		if (IsValid(Piece))
		{
			Piece->DestroyComponent();
		}
	}
	for (UStaticMeshComponent* Piece : M_PilePieces)
	{
		if (IsValid(Piece))
		{
			Piece->DestroyComponent();
		}
	}
	M_StackPieces.Reset();
	M_PilePieces.Reset();
	for (int32 Index = 0; Index < GetEffectiveMaxStack(); ++Index)
	{
		M_StackPieces.Add(CreatePoolPiece());
	}
	for (int32 Index = 0; Index < PickupPileCount; ++Index)
	{
		M_PilePieces.Add(CreatePoolPiece());
	}
	M_NextStackSlot = 0;
}

void AMissionFuelCrane::BeginPlay_InitStackGround()
{
	const int32 CellCount = GetStackCellCount();
	M_StackCellGroundZ.SetNum(CellCount);
	M_StackCellTopZ.SetNum(CellCount);
	for (int32 Cell = 0; Cell < CellCount; ++Cell)
	{
		// Slot index == cell index on the bottom layer; only the slot's XY is used here.
		M_StackCellTopZ[Cell] = 0.f;
		const FVector CellPoint = GetStackSlotBottomCenter(Cell);
		const float FallbackZ = GetActorTransform().TransformPosition(DropStackLocation).Z;
		const float GroundZ = bTraceDropGround ? TraceGroundZ(CellPoint, FallbackZ) : FallbackZ;
		M_StackCellGroundZ[Cell] = GroundZ;
		M_StackCellTopZ[Cell] = GroundZ;
	}
}

UStaticMeshComponent* AMissionFuelCrane::CreatePoolPiece()
{
	UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(this);
	ConfigureMovingPart(Piece);
	Piece->SetupAttachment(M_Root);
	Piece->SetVisibility(false);
	Piece->RegisterComponent();
	return Piece;
}

void AMissionFuelCrane::RestoreTruckCargoVisibility()
{
	if (not bM_SourcesAreTruckCargo)
	{
		return;
	}
	for (const TWeakObjectPtr<UStaticMeshComponent>& Piece : M_SourcePieces)
	{
		if (Piece.IsValid())
		{
			Piece->SetVisibility(true);
		}
	}
}

bool AMissionFuelCrane::GatherTruckCargoPieces(AActor* Truck)
{
	TArray<UStaticMeshComponent*> MeshComponents;
	Truck->GetComponents<UStaticMeshComponent>(MeshComponents);
	for (UStaticMeshComponent* Component : MeshComponents)
	{
		if (not IsValid(Component) or not IsValid(Component->GetStaticMesh()))
		{
			continue;
		}
		if (not Component->GetName().StartsWith(TruckCargoComponentPrefix) or not Component->IsVisible())
		{
			continue;
		}
		if (not GetIsWithinReach(Component->Bounds.Origin))
		{
			continue;
		}
		M_SourcePieces.Add(Component);
	}
	bM_SourcesAreTruckCargo = M_SourcePieces.Num() > 0;
	bM_DependsOnTruck = bM_SourcesAreTruckCargo;
	return bM_SourcesAreTruckCargo;
}

bool AMissionFuelCrane::UsePileAt(const FTransform& PileFrame, const bool bTraceGround)
{
	if (not IsValid(CargoMesh))
	{
		return false;
	}
	const int32 Columns = FMath::CeilToInt(FMath::Sqrt(static_cast<float>(M_PilePieces.Num())));
	const int32 Rows = FMath::DivideAndRoundUp(M_PilePieces.Num(), FMath::Max(Columns, 1));
	const float PileYaw = PileFrame.Rotator().Yaw;
	for (int32 Index = 0; Index < M_PilePieces.Num(); ++Index)
	{
		UStaticMeshComponent* Piece = M_PilePieces[Index];
		if (not IsValid(Piece))
		{
			continue;
		}
		const float CellX = (Index % Columns - (Columns - 1) * 0.5f) * StackSpacing;
		const float CellY = (Index / Columns - (Rows - 1) * 0.5f) * StackSpacing;
		FVector Bottom = PileFrame.TransformPositionNoScale(FVector(CellX, CellY, 0.f));
		if (bTraceGround)
		{
			Bottom.Z = TraceGroundZ(Bottom, Bottom.Z);
		}
		Piece->SetStaticMesh(CargoMesh);
		Piece->SetWorldTransform(GetCargoTransformOnPoint(CargoMesh, CargoScale, PileYaw, Bottom));
		Piece->SetVisibility(true);
		M_SourcePieces.Add(Piece);
	}
	return M_SourcePieces.Num() > 0;
}

int32 AMissionFuelCrane::ChooseNextSourcePiece()
{
	if (CountVisibleSources() == 0)
	{
		RefillSources();
	}
	int32 BestIndex = INDEX_NONE;
	float BestRadius = TNumericLimits<float>::Max();
	for (int32 Index = 0; Index < M_SourcePieces.Num(); ++Index)
	{
		const UStaticMeshComponent* Piece = M_SourcePieces[Index].Get();
		if (not IsValid(Piece) or not Piece->IsVisible())
		{
			continue;
		}
		const FVector Top = GetPieceTopCenter(Piece);
		const float Radius = GetHorizontalRadiusFromMast(Top);
		// Nearest first; the truck may have rolled, so re-check the reach every pick.
		if (not GetIsWithinReach(Top) or Radius >= BestRadius)
		{
			continue;
		}
		BestRadius = Radius;
		BestIndex = Index;
	}
	return BestIndex;
}

int32 AMissionFuelCrane::CountVisibleSources() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<UStaticMeshComponent>& Piece : M_SourcePieces)
	{
		if (Piece.IsValid() and Piece->IsVisible())
		{
			++Count;
		}
	}
	return Count;
}

void AMissionFuelCrane::RefillSources()
{
	for (const TWeakObjectPtr<UStaticMeshComponent>& Piece : M_SourcePieces)
	{
		if (Piece.IsValid())
		{
			Piece->SetVisibility(true);
		}
	}
}

bool AMissionFuelCrane::GetIsTruckLost() const
{
	if (not bM_DependsOnTruck)
	{
		return false;
	}
	return not RTSFunctionLibrary::RTSIsValidWeak(M_Truck);
}

void AMissionFuelCrane::OnTruckLost()
{
	if (not bM_SourcesAreTruckCargo)
	{
		// The fallback pile stood on the truck bed; it goes with the truck.
		for (UStaticMeshComponent* Piece : M_PilePieces)
		{
			if (IsValid(Piece))
			{
				Piece->SetVisibility(false);
			}
		}
	}
	bM_DependsOnTruck = false;
	bM_SourcesAreTruckCargo = false;
	M_Truck.Reset();
	M_SourcePieces.Reset();
	M_Carry.TargetSourceIndex = INDEX_NONE;
	bM_FinishCycleThenStop = true;
	if (M_Carry.bIsCarrying)
	{
		// Finish placing the piece already on the hook; HoistFromDrop then halts.
		return;
	}
	StartHoist(EMissionFuelCranePhase::HoistFromDrop);
}

float AMissionFuelCrane::GetHorizontalRadiusFromMast(const FVector& WorldPoint) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldPoint);
	return FVector2D(Local.X, Local.Y).Size();
}

bool AMissionFuelCrane::GetIsWithinReach(const FVector& WorldPoint) const
{
	const float Radius = GetHorizontalRadiusFromMast(WorldPoint);
	return Radius >= MinTrolleyRadius and Radius <= MaxTrolleyRadius;
}

FMissionFuelCranePose AMissionFuelCrane::GetPoseAbove(const FVector& WorldPoint, const float HookDepth) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldPoint);
	FMissionFuelCranePose Pose;
	Pose.YawDegrees = FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X));
	Pose.TrolleyRadius = FMath::Clamp(FVector2D(Local.X, Local.Y).Size(), MinTrolleyRadius, MaxTrolleyRadius);
	Pose.HookDepth = HookDepth;
	return Pose;
}

float AMissionFuelCrane::GetHookDepthForEyeAt(const float EyeWorldZ) const
{
	const FVector EyeLocal = GetActorTransform().InverseTransformPosition(
		FVector(GetActorLocation().X, GetActorLocation().Y, EyeWorldZ));
	const float CableOriginLocalZ = SlewPivotHeight + TrolleyRailHeight - CableOriginOffset;
	const float HookOriginLocalZ = EyeLocal.Z + HookEyeOffset;
	const float MaxDepth = FMath::Max(CableOriginLocalZ - HookEyeOffset, MinHookDepth);
	return FMath::Clamp(CableOriginLocalZ - HookOriginLocalZ, MinHookDepth, MaxDepth);
}

FVector AMissionFuelCrane::GetPieceTopCenter(const UStaticMeshComponent* Piece)
{
	const FBoxSphereBounds& Bounds = Piece->Bounds;
	return Bounds.Origin + FVector(0.f, 0.f, Bounds.BoxExtent.Z);
}

FTransform AMissionFuelCrane::GetCargoTransformOnPoint(const UStaticMesh* Mesh, const FVector& Scale,
                                                       const float YawDegrees, const FVector& BottomCenter)
{
	const FBox LocalBox = Mesh->GetBoundingBox();
	const FVector LocalBottomCenter(LocalBox.GetCenter().X, LocalBox.GetCenter().Y, LocalBox.Min.Z);
	const FRotator Rotation(0.f, YawDegrees, 0.f);
	const FVector Location = BottomCenter - Rotation.RotateVector(LocalBottomCenter * Scale);
	return FTransform(Rotation, Location, Scale);
}

float AMissionFuelCrane::TraceGroundZ(const FVector& WorldPoint, const float FallbackZ) const
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return FallbackZ;
	}
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MissionFuelCraneGround), false, this);
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
	const FVector Start(WorldPoint.X, WorldPoint.Y, FallbackZ + MissionFuelCraneConstants::GroundTraceUp);
	const FVector End(WorldPoint.X, WorldPoint.Y, FallbackZ - MissionFuelCraneConstants::GroundTraceDown);
	FHitResult Hit;
	if (not World->LineTraceSingleByObjectType(Hit, Start, End, ObjectParams, QueryParams))
	{
		return FallbackZ;
	}
	return Hit.ImpactPoint.Z;
}

int32 AMissionFuelCrane::GetEffectiveMaxStack() const
{
	return FMath::Clamp(MaxStackCount, 1, GetStackCellCount() * FMath::Max(StackLayers, 1));
}

int32 AMissionFuelCrane::GetStackCellCount() const
{
	return FMath::Max(StackColumns, 1) * FMath::Max(StackRows, 1);
}

FVector AMissionFuelCrane::GetStackSlotBottomCenter(const int32 SlotIndex) const
{
	const int32 Columns = FMath::Max(StackColumns, 1);
	const int32 Rows = FMath::Max(StackRows, 1);
	const int32 Cell = SlotIndex % GetStackCellCount();
	const float CellX = (Cell % Columns - (Columns - 1) * 0.5f) * StackSpacing;
	const float CellY = (Cell / Columns - (Rows - 1) * 0.5f) * StackSpacing;
	FVector World = GetActorTransform().TransformPosition(DropStackLocation + FVector(CellX, CellY, 0.f));
	if (M_StackCellTopZ.IsValidIndex(Cell))
	{
		World.Z = M_StackCellTopZ[Cell];
	}
	return World;
}

void AMissionFuelCrane::BeginPhase(const EMissionFuelCranePhase Phase)
{
	switch (Phase)
	{
	case EMissionFuelCranePhase::SlewToPickup:
		StartSlewToPickup();
		break;
	case EMissionFuelCranePhase::LowerToPickup:
		StartLowerToPickup();
		break;
	case EMissionFuelCranePhase::AttachCargo:
		StartAttachCargo();
		break;
	case EMissionFuelCranePhase::HoistFromPickup:
	case EMissionFuelCranePhase::HoistFromDrop:
		StartHoist(Phase);
		break;
	case EMissionFuelCranePhase::SlewToDrop:
		StartSlewToDrop();
		break;
	case EMissionFuelCranePhase::LowerToDrop:
		StartLowerToDrop();
		break;
	case EMissionFuelCranePhase::ReleaseCargo:
		StartReleaseCargo();
		break;
	case EMissionFuelCranePhase::Idle:
	default:
		Halt();
		break;
	}
}

void AMissionFuelCrane::OnPhaseFinished()
{
	switch (M_Phase)
	{
	case EMissionFuelCranePhase::SlewToPickup:
		M_Sway.AmplitudeDegrees = SwayKickDegrees;
		BeginPhase(EMissionFuelCranePhase::LowerToPickup);
		break;
	case EMissionFuelCranePhase::LowerToPickup:
		BeginPhase(EMissionFuelCranePhase::AttachCargo);
		break;
	case EMissionFuelCranePhase::AttachCargo:
		BeginPhase(M_Carry.bIsCarrying
			           ? EMissionFuelCranePhase::HoistFromPickup
			           : EMissionFuelCranePhase::HoistFromDrop);
		break;
	case EMissionFuelCranePhase::HoistFromPickup:
		BeginPhase(EMissionFuelCranePhase::SlewToDrop);
		break;
	case EMissionFuelCranePhase::SlewToDrop:
		M_Sway.AmplitudeDegrees = SwayKickDegrees;
		BeginPhase(EMissionFuelCranePhase::LowerToDrop);
		break;
	case EMissionFuelCranePhase::LowerToDrop:
		BeginPhase(EMissionFuelCranePhase::ReleaseCargo);
		break;
	case EMissionFuelCranePhase::ReleaseCargo:
		BeginPhase(EMissionFuelCranePhase::HoistFromDrop);
		break;
	case EMissionFuelCranePhase::HoistFromDrop:
		BeginPhase(bM_FinishCycleThenStop ? EMissionFuelCranePhase::Idle : EMissionFuelCranePhase::SlewToPickup);
		break;
	case EMissionFuelCranePhase::Idle:
	default:
		Halt();
		break;
	}
}

void AMissionFuelCrane::StartMove(const EMissionFuelCranePhase Phase, const FMissionFuelCranePose& Target,
                                  const bool bAllowPreferredSide)
{
	M_Phase = Phase;
	M_Move.Start = M_CurrentPose;
	M_Move.Target = Target;
	M_Move.YawDelta = GetSlewDelta(M_CurrentPose.YawDegrees, Target.YawDegrees, bAllowPreferredSide);
	M_Move.Elapsed = 0.f;
	const float SlewSeconds = FMath::Abs(M_Move.YawDelta) / SlewSpeedDegreesPerSecond;
	const float TrolleySeconds = FMath::Abs(Target.TrolleyRadius - M_CurrentPose.TrolleyRadius) / TrolleySpeed;
	const float HoistSeconds = FMath::Abs(Target.HookDepth - M_CurrentPose.HookDepth) / HoistSpeed;
	M_Move.Duration = FMath::Max3(SlewSeconds, TrolleySeconds, FMath::Max(HoistSeconds, MinMoveSeconds));
}

void AMissionFuelCrane::StartPause(const EMissionFuelCranePhase Phase, const float Seconds)
{
	M_Phase = Phase;
	M_Move.Start = M_CurrentPose;
	M_Move.Target = M_CurrentPose;
	M_Move.YawDelta = 0.f;
	M_Move.Elapsed = 0.f;
	M_Move.Duration = FMath::Max(Seconds, KINDA_SMALL_NUMBER);
}

float AMissionFuelCrane::GetSlewDelta(const float FromYaw, const float ToYaw, const bool bAllowPreferredSide) const
{
	const float Shortest = FMath::FindDeltaAngleDegrees(FromYaw, ToYaw);
	if (not bAllowPreferredSide or not bUsePreferredSlewSide or FMath::Abs(Shortest) < PreferredSideMinSwing)
	{
		return Shortest;
	}
	using MissionFuelCraneConstants::FullTurnDegrees;
	// Counter-clockwise sweep in [0, 360) and where the preferred bearing falls on it.
	const float PositiveSweep = FMath::Fmod(ToYaw - FromYaw + 2.f * FullTurnDegrees, FullTurnDegrees);
	const float ViaOnSweep = FMath::Fmod(PreferredSlewViaYaw - FromYaw + 2.f * FullTurnDegrees, FullTurnDegrees);
	return ViaOnSweep <= PositiveSweep ? PositiveSweep : PositiveSweep - FullTurnDegrees;
}

void AMissionFuelCrane::Halt()
{
	bM_IsUnloading = false;
	bM_FinishCycleThenStop = false;
	M_Phase = EMissionFuelCranePhase::Idle;
	SetActorTickEnabled(false);
}

void AMissionFuelCrane::StartSlewToPickup()
{
	M_Carry.TargetSourceIndex = ChooseNextSourcePiece();
	if (M_Carry.TargetSourceIndex == INDEX_NONE)
	{
		// Nothing reachable left (e.g. the truck rolled away): park at travel height and stop.
		bM_FinishCycleThenStop = true;
		StartHoist(EMissionFuelCranePhase::HoistFromDrop);
		return;
	}
	const FVector Top = GetPieceTopCenter(M_SourcePieces[M_Carry.TargetSourceIndex].Get());
	StartMove(EMissionFuelCranePhase::SlewToPickup, GetPoseAbove(Top, TravelHookDepth), true);
}

void AMissionFuelCrane::StartLowerToPickup()
{
	const UStaticMeshComponent* Piece = M_SourcePieces.IsValidIndex(M_Carry.TargetSourceIndex)
		                                    ? M_SourcePieces[M_Carry.TargetSourceIndex].Get()
		                                    : nullptr;
	if (not IsValid(Piece) or not Piece->IsVisible())
	{
		StartHoist(EMissionFuelCranePhase::HoistFromDrop);
		return;
	}
	const FVector Top = GetPieceTopCenter(Piece);
	StartMove(EMissionFuelCranePhase::LowerToPickup,
	          GetPoseAbove(Top, GetHookDepthForEyeAt(Top.Z + SlingHeight)), false);
}

void AMissionFuelCrane::StartAttachCargo()
{
	if (M_NextStackSlot >= GetEffectiveMaxStack())
	{
		// Restart the stack while the hook is away at the truck.
		ClearStack();
	}
	AttachSourcePiece(M_Carry.TargetSourceIndex);
	StartPause(EMissionFuelCranePhase::AttachCargo, AttachPauseSeconds);
}

void AMissionFuelCrane::StartSlewToDrop()
{
	const FVector SlotBottom = GetStackSlotBottomCenter(M_NextStackSlot);
	StartMove(EMissionFuelCranePhase::SlewToDrop, GetPoseAbove(SlotBottom, TravelHookDepth), true);
}

void AMissionFuelCrane::StartLowerToDrop()
{
	const FVector SlotBottom = GetStackSlotBottomCenter(M_NextStackSlot);
	const float EyeZ = SlotBottom.Z + M_Carry.CargoHeight + SlingHeight;
	StartMove(EMissionFuelCranePhase::LowerToDrop, GetPoseAbove(SlotBottom, GetHookDepthForEyeAt(EyeZ)), false);
}

void AMissionFuelCrane::StartReleaseCargo()
{
	PlaceCarriedCargoOnStack();
	StartPause(EMissionFuelCranePhase::ReleaseCargo, ReleasePauseSeconds);
}

void AMissionFuelCrane::StartHoist(const EMissionFuelCranePhase Phase)
{
	FMissionFuelCranePose Target = M_CurrentPose;
	Target.HookDepth = TravelHookDepth;
	StartMove(Phase, Target, false);
}

bool AMissionFuelCrane::AttachSourcePiece(const int32 SourceIndex)
{
	UStaticMeshComponent* Piece = M_SourcePieces.IsValidIndex(SourceIndex) ? M_SourcePieces[SourceIndex].Get() : nullptr;
	if (not IsValid(Piece) or not IsValid(Piece->GetStaticMesh()))
	{
		return false;
	}
	// Attach in the rest pose so the copied transform carries no sway tilt.
	SnapSwayToRest();
	M_CarriedCargoMesh->SetStaticMesh(Piece->GetStaticMesh());
	CopyMaterials(Piece, M_CarriedCargoMesh);
	M_CarriedCargoMesh->SetWorldTransform(Piece->GetComponentTransform());
	M_CarriedCargoMesh->SetVisibility(true);
	Piece->SetVisibility(false);
	SizeSlingToPiece(Piece);
	M_Carry.bIsCarrying = true;
	M_Carry.CargoHeight = Piece->Bounds.BoxExtent.Z * 2.f;
	return true;
}

void AMissionFuelCrane::SizeSlingToPiece(const UStaticMeshComponent* Piece) const
{
	const FVector Size = Piece->GetStaticMesh()->GetBoundingBox().GetSize() * Piece->GetComponentScale().GetAbs();
	using MissionFuelCraneConstants::SlingMeshUnitSize;
	M_SlingMesh->SetRelativeLocation(FVector(0.f, 0.f, -HookEyeOffset));
	M_SlingMesh->SetWorldRotation(FRotator(0.f, Piece->GetComponentRotation().Yaw, 0.f));
	M_SlingMesh->SetRelativeScale3D(FVector(Size.X * SlingFootprintFraction / SlingMeshUnitSize,
	                                        Size.Y * SlingFootprintFraction / SlingMeshUnitSize,
	                                        SlingHeight / SlingMeshUnitSize));
	M_SlingMesh->SetVisibility(true);
}

void AMissionFuelCrane::PlaceCarriedCargoOnStack()
{
	SnapSwayToRest();
	UStaticMesh* Mesh = M_CarriedCargoMesh->GetStaticMesh();
	const int32 Slot = M_NextStackSlot;
	const int32 Cell = Slot % GetStackCellCount();
	if (M_Carry.bIsCarrying and IsValid(Mesh) and M_StackPieces.IsValidIndex(Slot) and IsValid(M_StackPieces[Slot]))
	{
		UStaticMeshComponent* Placed = M_StackPieces[Slot];
		const FVector Bottom = GetStackSlotBottomCenter(Slot);
		Placed->SetStaticMesh(Mesh);
		CopyMaterials(M_CarriedCargoMesh, Placed);
		Placed->SetWorldTransform(GetCargoTransformOnPoint(Mesh, M_CarriedCargoMesh->GetComponentScale(),
		                                                   M_CarriedCargoMesh->GetComponentRotation().Yaw, Bottom));
		Placed->SetVisibility(true);
		if (M_StackCellTopZ.IsValidIndex(Cell))
		{
			M_StackCellTopZ[Cell] = Bottom.Z + M_Carry.CargoHeight;
		}
		++M_UnloadedCount;
		++M_NextStackSlot;
	}
	M_CarriedCargoMesh->SetVisibility(false);
	M_SlingMesh->SetVisibility(false);
	M_Carry.bIsCarrying = false;
	M_Carry.TargetSourceIndex = INDEX_NONE;
	if (CountVisibleSources() == 0)
	{
		// Truck emptied: reload it while the hook is away at the stack.
		RefillSources();
	}
}

void AMissionFuelCrane::ClearStack()
{
	for (UStaticMeshComponent* Piece : M_StackPieces)
	{
		if (IsValid(Piece))
		{
			Piece->SetVisibility(false);
		}
	}
	for (int32 Cell = 0; Cell < M_StackCellTopZ.Num() and Cell < M_StackCellGroundZ.Num(); ++Cell)
	{
		M_StackCellTopZ[Cell] = M_StackCellGroundZ[Cell];
	}
	M_NextStackSlot = 0;
}

void AMissionFuelCrane::CopyMaterials(const UStaticMeshComponent* From, UStaticMeshComponent* To)
{
	if (not IsValid(From) or not IsValid(To))
	{
		return;
	}
	for (int32 Index = 0; Index < From->GetNumMaterials(); ++Index)
	{
		To->SetMaterial(Index, From->GetMaterial(Index));
	}
}
