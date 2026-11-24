// Copyright (C) Bas Blokzijl - All rights reserved.

#include "WallGate.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "Kismet/GameplayStatics.h"
#include "NavAreas/NavArea_Default.h"
#include "NavAreas/NavArea_Null.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "RTS_Survival/Navigation/NavQueryFilterSettings/RTSNavQueryFilterSettings.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/CollisionSetup/RTSPlayerCollision.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "WallGateNavigation.h"

DEFINE_LOG_CATEGORY_STATIC(LogWallGate, Log, All);

namespace WallGateConstants
{
	// Same rate as the nomadic deploy VAT; the gate only ticks while the leaf moves.
	constexpr float MotionTickIntervalSeconds = 1.f / 30.f;
	constexpr float MinMoveDurationSeconds = 0.05f;
	// URTSComponent reports -1 as unset; a wall is never placed in a formation.
	constexpr float WallFormationInnerRadius = 0.f;
	// Opening runs the progress down to 0, closing back up to 1.
	constexpr float OpeningDirection = -1.f;
	constexpr float ClosingDirection = 1.f;
	constexpr float GateSoundVolume = 1.f;
	constexpr float GateSoundPitch = 1.f;
	constexpr float GateSoundStartTime = 0.f;

	/**
	 * @brief Registry lists hold every unit of a side; the gate wants live ones of one exact player near a point.
	 * @param Unit Tank or squad unit from the registry.
	 * @param Player Owning player the unit must have.
	 * @param Centre Doorway centre.
	 * @param RadiusSquared Squared 2D reach.
	 * @return True when the unit is alive, owned by Player and within reach.
	 */
	template <typename TUnit>
	bool GetIsUnitOfPlayerWithin(TUnit* const Unit, const uint8 Player, const FVector& Centre,
	                             const double RadiusSquared)
	{
		if (not RTSFunctionLibrary::RTSIsValid(Unit))
		{
			return false;
		}
		const URTSComponent* const RTSComponent = Unit->GetRTSComponent();
		if (not IsValid(RTSComponent) || RTSComponent->GetOwningPlayer() != Player)
		{
			return false;
		}
		return FVector::DistSquared2D(Unit->GetActorLocation(), Centre) <= RadiusSquared;
	}

	/**
	 * @brief Whether a live unit's centre lies in the padded doorway box.
	 * @param Units Registry list of one player.
	 * @param LeafTransform The leaf component's world transform.
	 * @param PaddedLeafSpaceBox Doorway box in leaf space, already padded.
	 * @return True when any unit stands in the doorway.
	 */
	template <typename TUnit>
	bool GetIsAnyUnitInsideBox(const TConstArrayView<TUnit*> Units, const FTransform& LeafTransform,
	                           const FBox& PaddedLeafSpaceBox)
	{
		for (TUnit* const Unit : Units)
		{
			if (not RTSFunctionLibrary::RTSIsValid(Unit))
			{
				continue;
			}
			if (PaddedLeafSpaceBox.IsInsideOrOn(LeafTransform.InverseTransformPosition(Unit->GetActorLocation())))
			{
				return true;
			}
		}
		return false;
	}
}

UWallGateRTSComponent::UWallGateRTSComponent()
{
	OwningPlayer = WallGateDefaults::EnemyAIPlayer;
	FormationUnitInnerRadius = WallGateConstants::WallFormationInnerRadius;
	// Like the Rus walls: not a unit, so not registered with the game state.
	bAddToGameState = false;
}

AWallGate::AWallGate(const FObjectInitializer& ObjectInitializer)
	: AWallActor(ObjectInitializer)
{
	// Ticks only while the leaf moves.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickInterval = WallGateConstants::MotionTickIntervalSeconds;

	GateRoot = CreateDefaultSubobject<USceneComponent>(TEXT("GateRoot"));
	SetRootComponent(GateRoot);

	GateLeafMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GateLeafMesh"));
	GateLeafMesh->SetupAttachment(GateRoot);
	// The doorway modifier carries the navigation; the leaf itself must not carve the mesh.
	GateLeafMesh->SetCanEverAffectNavigation(false);
	GateLeafMesh->SetDefaultCustomPrimitiveDataFloat(WallGatePrimitiveData::ProgressIndex,
	                                                 WallGatePrimitiveData::ClosedProgress);

	GateNavModifier = CreateDefaultSubobject<UWallGateNavModifier>(TEXT("GateNavModifier"));
	GateNavModifier->AreaClass = UNavArea_WallGateEnemySide::StaticClass();

	FrameNavModifier = CreateDefaultSubobject<UWallGateFrameNavModifier>(TEXT("FrameNavModifier"));

	// AWallActor only looks the RTS component up; the gate brings its own so its Blueprint needs none.
	RTSComponent = CreateDefaultSubobject<UWallGateRTSComponent>(TEXT("RTSComponent"));
}

void AWallGate::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (not GetIsValidGateLeafMesh())
	{
		return;
	}
	EnsureClosedDefaultPrimitiveData();
	float ShownProgress = WallGatePrimitiveData::ClosedProgress;
#if WITH_EDITORONLY_DATA
	const UWorld* const World = GetWorld();
	if (IsValid(World) && not World->IsGameWorld())
	{
		// Transient data only: a preview saved into the defaults would come back on every reset.
		ShownProgress = WallGatePrimitiveData::ClosedProgress - FMath::Clamp(PreviewOpenAmount, 0.f, 1.f);
	}
#endif
	GateLeafMesh->SetCustomPrimitiveDataFloat(WallGatePrimitiveData::ProgressIndex, ShownProgress);
	if (not GetIsValidGateNavModifier())
	{
		return;
	}
	// The editor-built navigation mesh shows the closed gate the way the game starts.
	bM_UsePlayerAwareNavArea = NavMode == EWallGateNavMode::PlayerAwareArea;
	GateNavModifier->SetAreaClass(GetClosedNavAreaClass());
	// The meshes or their placement may have changed: re-read the doorway and frame boxes.
	RefreshFrameAndDoorwayNavBounds();
}

void AWallGate::RefreshFrameAndDoorwayNavBounds() const
{
	if (GetIsValidGateNavModifier())
	{
		GateNavModifier->UpdateNavigationBounds();
		GateNavModifier->RefreshNavigationModifiers();
	}
	if (GetIsValidFrameNavModifier())
	{
		FrameNavModifier->UpdateNavigationBounds();
		FrameNavModifier->RefreshNavigationModifiers();
	}
}

void AWallGate::EnsureClosedDefaultPrimitiveData() const
{
	const TArray<float>& DefaultData = GateLeafMesh->GetDefaultCustomPrimitiveData().Data;
	const bool bHasClosedDefault = DefaultData.IsValidIndex(WallGatePrimitiveData::ProgressIndex)
		&& DefaultData[WallGatePrimitiveData::ProgressIndex] == WallGatePrimitiveData::ClosedProgress;
	if (bHasClosedDefault)
	{
		return;
	}
	GateLeafMesh->SetDefaultCustomPrimitiveDataFloat(WallGatePrimitiveData::ProgressIndex,
	                                                 WallGatePrimitiveData::ClosedProgress);
}

void AWallGate::BeginPlay()
{
	// Components begin play inside Super, so the leaf's hole component has written its mask (8..34) by then.
	Super::BeginPlay();
	M_Motion = FWallGateMotionState();
	WriteLeafProgress(M_Motion.Progress);
	BeginPlay_SetupFrameCollision();
	BeginPlay_SetupLeafCollision();
	// Before the navigation set-up: the closed doorway area is the allowed player's.
	BeginPlay_ResolveAllowedPlayer();
	BeginPlay_SetupNavigation();
	BeginPlay_StartDetection();
	BeginPlay_BindOwnerChange();
}

void AWallGate::BeginPlay_ResolveAllowedPlayer()
{
	const URTSComponent* const GateRTSComponent = GetRTSComponent();
	if (not IsValid(GateRTSComponent))
	{
		return;
	}
	const uint8 GateOwner = GateRTSComponent->GetOwningPlayer();
	if (FRTSPlayers::NormalizePlayer(GateOwner) == FRTSPlayers::NormalizePlayer(AllowedPlayer))
	{
		return;
	}
	// The owner was set before BeginPlay (placed with another owner, or spawned for a player), which broadcasts no
	// owner change. An AllowedPlayer left at the class default follows the owner, as a runtime capture does.
	const AWallGate* const ClassDefaults = GetClass()->GetDefaultObject<AWallGate>();
	const bool bIsAllowedPlayerClassDefault = IsValid(ClassDefaults)
		&& FRTSPlayers::NormalizePlayer(ClassDefaults->AllowedPlayer) == FRTSPlayers::NormalizePlayer(AllowedPlayer);
	if (bIsAllowedPlayerClassDefault && not FRTSPlayers::IsNeutralOwner(GateOwner))
	{
		AllowedPlayer = GateOwner;
		return;
	}
	// Supported but unusual: the authored AllowedPlayer decides who opens the gate, the owner whose shots pass it.
	UE_LOG(LogWallGate, Warning, TEXT("RTS_WALL_GATE %s: AllowedPlayer %d differs from the gate's owner %d; the gate "
		       "opens for player %d and its allies, while the shots of player %d and its allies pass it."), *GetName(),
	       AllowedPlayer, GateOwner, AllowedPlayer, GateOwner);
}

void AWallGate::BeginPlay_BindOwnerChange()
{
	URTSComponent* const GateRTSComponent = GetRTSComponent();
	if (not IsValid(GateRTSComponent))
	{
		return;
	}
	M_OwnerChangedHandle = GateRTSComponent->OnOwningPlayerChanged.AddUObject(this, &AWallGate::OnGateOwnerChanged);
}

void AWallGate::OnGateOwnerChanged(const uint8 OldOwner, const uint8 NewOwner)
{
	// A gate authored for another player than its owner keeps that player; one that let its owner's side through
	// now lets the new owner's side through.
	if (FRTSPlayers::NormalizePlayer(AllowedPlayer) != FRTSPlayers::NormalizePlayer(OldOwner))
	{
		return;
	}
	SetAllowedPlayer(NewOwner);
}

void AWallGate::SetAllowedPlayer(const uint8 NewAllowedPlayer)
{
	if (AllowedPlayer == NewAllowedPlayer)
	{
		return;
	}
	AllowedPlayer = NewAllowedPlayer;
	ApplyClosedNavAreaIfShut();
	UE_LOG(LogWallGate, Log, TEXT("RTS_WALL_GATE %s allowed_player=%d"), *GetName(), AllowedPlayer);
}

void AWallGate::ApplyClosedNavAreaIfShut() const
{
	// SetGatePassable(false) reads the allowed player again when the gate shuts; a wreck never blocks.
	if (M_Motion.bIsPassable || M_Motion.State == EWallGateState::Wrecked || not GetIsValidGateNavModifier())
	{
		return;
	}
	GateNavModifier->SetAreaClass(GetClosedNavAreaClass());
}

void AWallGate::BeginPlay_SetupFrameCollision()
{
	UStaticMeshComponent* const FrameMesh = FindFrameMesh();
	if (not IsValid(FrameMesh))
	{
		return;
	}
	// Same call the Rus wall graphs make; it only sets fixed values, so a Blueprint that also calls it changes nothing.
	SetupWallCollision(FrameMesh, bFrameAffectsNavMesh);
}

void AWallGate::BeginPlay_SetupLeafCollision()
{
	if (not GetIsValidGateLeafMesh())
	{
		return;
	}
	GateLeafMesh->SetCanEverAffectNavigation(false);
	// The Blueprint's BeginPlay (inside Super) has set up the frame by now.
	const UStaticMeshComponent* const FrameMesh = FindFrameMesh();
	if (IsValid(FrameMesh))
	{
		GateLeafMesh->SetCollisionObjectType(FrameMesh->GetCollisionObjectType());
		GateLeafMesh->SetCollisionResponseToChannels(FrameMesh->GetCollisionResponseToChannels());
		// The responses do not say whose body it is: without the frame's owner bit the gate owner's own shots
		// would hit the leaf.
		FRTSPlayerCollision::CopyOwnerMaskFilter(FrameMesh, GateLeafMesh);
		M_LeafBlockingCollision = FrameMesh->GetCollisionEnabled();
	}
	else
	{
		RTSFunctionLibrary::ReportError(TEXT("AWallGate: no static mesh component named ")
			+ FrameMeshComponentName.ToString() + TEXT(" to copy the leaf collision from on ") + GetName());
		M_LeafBlockingCollision = GateLeafMesh->GetCollisionEnabled();
	}
	GateLeafMesh->SetCollisionEnabled(M_LeafBlockingCollision);
}

void AWallGate::BeginPlay_SetupNavigation()
{
	bM_UsePlayerAwareNavArea = NavMode == EWallGateNavMode::PlayerAwareArea;
	if (bM_UsePlayerAwareNavArea && not GetAreRTSNavFiltersGateAware())
	{
		bM_UsePlayerAwareNavArea = false;
		RTSFunctionLibrary::ReportError(TEXT("AWallGate ") + GetName()
			+ TEXT(": NavMode PlayerAwareArea needs every RTS nav filter (Project Settings > RTS Navigation Filters) to "
				"derive from URTSWallGateNavQueryFilter. Falling back to BlockEveryone: allowed units only route "
				"through this gate while it is open."));
	}
	if (GetIsValidGateNavModifier())
	{
		GateNavModifier->SetAreaClass(GetClosedNavAreaClass());
	}
	// All Blueprint components are registered and the frame is set up: compute both boxes now.
	RefreshFrameAndDoorwayNavBounds();
}

void AWallGate::BeginPlay_StartDetection()
{
	UWorld* const World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	const float Interval = FMath::Max(DetectionIntervalSeconds, WallGateConstants::MinMoveDurationSeconds);
	// Staggered so a row of gates does not scan on the same frame.
	const float FirstDelay = FMath::FRandRange(WallGateConstants::MinMoveDurationSeconds, Interval);
	World->GetTimerManager().SetTimer(M_DetectionTimerHandle, this, &AWallGate::OnDetectionTimer, Interval, true,
	                                  FirstDelay);
}

void AWallGate::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_DetectionTimerHandle);
	}
	URTSComponent* const GateRTSComponent = GetRTSComponent();
	if (IsValid(GateRTSComponent))
	{
		GateRTSComponent->OnOwningPlayerChanged.Remove(M_OwnerChangedHandle);
	}
	M_OwnerChangedHandle.Reset();
	Super::EndPlay(EndPlayReason);
}

void AWallGate::OnDetectionTimer()
{
	const UWorld* const World = GetWorld();
	if (not IsValid(World) || M_Motion.State == EWallGateState::Wrecked)
	{
		return;
	}
	const float Now = World->GetTimeSeconds();
	const bool bIsClosedOrClosing = M_Motion.State == EWallGateState::Closed
		|| M_Motion.State == EWallGateState::Closing;
	if (GetIsAllowedUnitInRange())
	{
		M_Motion.LastAllowedUnitSeenTime = Now;
		if (bIsClosedOrClosing)
		{
			StartMoving(EWallGateState::Opening);
		}
		return;
	}
	if (bIsClosedOrClosing || Now - M_Motion.LastAllowedUnitSeenTime < CloseDelaySeconds)
	{
		return;
	}
	// Never shut on a unit standing in the doorway, whichever side it is on; try again next scan.
	if (GetIsAnyUnitInDoorway())
	{
		return;
	}
	StartMoving(EWallGateState::Closing);
}

bool AWallGate::GetIsAllowedUnitInRange() const
{
	// Read every scan: a team change opens (or stops opening) the gate for the new allies from the next scan on.
	// Zero for a neutral allowed player, which opens the gate for nobody.
	const uint8 AlliedPlayersMask = FRTSPlayers::GetFriendlyMask(AllowedPlayer);
	if (AlliedPlayersMask == 0 || not GetIsValidGameUnitManger())
	{
		return false;
	}
	const FVector Centre = GetDoorwayWorldCentre();
	const double RadiusSquared = FMath::Square(static_cast<double>(OpenRadius));
	for (int32 Player = RTSPlayers::HumanPlayer; Player <= RTSPlayers::MaxPlayers; ++Player)
	{
		if (FRTSPlayers::IsPlayerInMask(Player, AlliedPlayersMask)
			&& GetIsUnitOfPlayerInRange(static_cast<uint8>(Player), Centre, RadiusSquared))
		{
			return true;
		}
	}
	return false;
}

bool AWallGate::GetIsUnitOfPlayerInRange(const uint8 Player, const FVector& Centre, const double RadiusSquared) const
{
	for (ATankMaster* const Tank : M_GameUnitManager->GetTanksOfPlayerView(Player))
	{
		if (WallGateConstants::GetIsUnitOfPlayerWithin(Tank, Player, Centre, RadiusSquared))
		{
			return true;
		}
	}
	for (ASquadUnit* const SquadUnit : M_GameUnitManager->GetSquadUnitsOfPlayerView(Player))
	{
		if (WallGateConstants::GetIsUnitOfPlayerWithin(SquadUnit, Player, Centre, RadiusSquared))
		{
			return true;
		}
	}
	return false;
}

bool AWallGate::GetIsAnyUnitInDoorway() const
{
	FBox LeafSpaceBox;
	if (not GetIsValidGameUnitManger() || not GetIsValidGateLeafMesh() || not GetLeafRestBox(LeafSpaceBox))
	{
		return false;
	}
	const FTransform LeafTransform = GateLeafMesh->GetComponentTransform();
	const FVector LeafScale = LeafTransform.GetScale3D().GetAbs().ComponentMax(FVector(UE_KINDA_SMALL_NUMBER));
	const FBox PaddedBox = LeafSpaceBox.ExpandBy(FVector(DoorwayClearancePadding) / LeafScale);
	// The registry keeps the units of each player apart; the units of every player count here.
	for (int32 Player = RTSPlayers::HumanPlayer; Player <= RTSPlayers::MaxPlayers; ++Player)
	{
		const uint8 PlayerIndex = static_cast<uint8>(Player);
		if (WallGateConstants::GetIsAnyUnitInsideBox<ATankMaster>(
				M_GameUnitManager->GetTanksOfPlayerView(PlayerIndex), LeafTransform, PaddedBox)
			|| WallGateConstants::GetIsAnyUnitInsideBox<ASquadUnit>(
				M_GameUnitManager->GetSquadUnitsOfPlayerView(PlayerIndex), LeafTransform, PaddedBox))
		{
			return true;
		}
	}
	return false;
}

void AWallGate::StartMoving(const EWallGateState NewState)
{
	const bool bIsOpening = NewState == EWallGateState::Opening;
	M_Motion.State = NewState;
	if (bIsOpening)
	{
		// Reversing a closing gate that is still open enough keeps it passable.
		if (M_Motion.Progress <= PassableProgressThreshold)
		{
			SetGatePassable(true);
		}
		PlayGateSound(OpenSound);
		BP_OnGateStartedOpening();
	}
	else
	{
		// Shots and planning treat the doorway as shut from the first moment of closing.
		SetGatePassable(false);
		PlayGateSound(CloseSound);
		BP_OnGateStartedClosing();
	}
	SetActorTickEnabled(true);
}

void AWallGate::Tick(const float DeltaTime)
{
	Super::Tick(DeltaTime);
	const bool bIsOpening = M_Motion.State == EWallGateState::Opening;
	if (not bIsOpening && M_Motion.State != EWallGateState::Closing)
	{
		SetActorTickEnabled(false);
		return;
	}
	const float Duration = FMath::Max(bIsOpening ? OpenDurationSeconds : CloseDurationSeconds,
	                                  WallGateConstants::MinMoveDurationSeconds);
	const float Direction = bIsOpening ? WallGateConstants::OpeningDirection : WallGateConstants::ClosingDirection;
	M_Motion.Progress = FMath::Clamp(M_Motion.Progress + Direction * DeltaTime / Duration,
	                                 WallGatePrimitiveData::OpenProgress, WallGatePrimitiveData::ClosedProgress);
	WriteLeafProgress(M_Motion.Progress);
	if (bIsOpening && M_Motion.Progress <= PassableProgressThreshold)
	{
		SetGatePassable(true);
	}
	const bool bReachedEnd = bIsOpening
		                         ? M_Motion.Progress <= WallGatePrimitiveData::OpenProgress
		                         : M_Motion.Progress >= WallGatePrimitiveData::ClosedProgress;
	if (bReachedEnd)
	{
		FinishMoving();
	}
}

void AWallGate::FinishMoving()
{
	SetActorTickEnabled(false);
	if (M_Motion.State == EWallGateState::Opening)
	{
		M_Motion.State = EWallGateState::Open;
		BP_OnGateFullyOpen();
		return;
	}
	M_Motion.State = EWallGateState::Closed;
	// Exactly 1: the VAT shader skips the leaf offset at >= 0.9999.
	M_Motion.Progress = WallGatePrimitiveData::ClosedProgress;
	WriteLeafProgress(M_Motion.Progress);
	BP_OnGateFullyClosed();
}

void AWallGate::SetGatePassable(const bool bNewIsPassable)
{
	if (M_Motion.bIsPassable == bNewIsPassable)
	{
		return;
	}
	M_Motion.bIsPassable = bNewIsPassable;
	if (GetIsValidGateLeafMesh())
	{
		// The leaf's collision stays at the rest pose, so it must not exist while the leaf is out of the doorway.
		GateLeafMesh->SetCollisionEnabled(bNewIsPassable ? ECollisionEnabled::NoCollision : M_LeafBlockingCollision.GetValue());
	}
	if (GetIsValidGateNavModifier())
	{
		const TSubclassOf<UNavArea> PassableArea = UNavArea_Default::StaticClass();
		GateNavModifier->SetAreaClass(bNewIsPassable ? PassableArea : GetClosedNavAreaClass());
	}
	UE_LOG(LogWallGate, Verbose, TEXT("RTS_WALL_GATE %s passable=%d progress=%.2f"), *GetName(),
	       bNewIsPassable ? 1 : 0, M_Motion.Progress);
}

void AWallGate::WriteLeafProgress(const float Progress) const
{
	if (not GetIsValidGateLeafMesh())
	{
		return;
	}
	// One index only: the hole mask lives in 8..34 of the same array.
	GateLeafMesh->SetCustomPrimitiveDataFloat(WallGatePrimitiveData::ProgressIndex, Progress);
}

void AWallGate::PlayGateSound(USoundBase* const Sound) const
{
	if (not IsValid(Sound))
	{
		return;
	}
	UGameplayStatics::PlaySoundAtLocation(this, Sound, GetDoorwayWorldCentre(), FRotator::ZeroRotator,
	                                      WallGateConstants::GateSoundVolume, WallGateConstants::GateSoundPitch,
	                                      WallGateConstants::GateSoundStartTime,
	                                      GateSoundAttenuation, GateSoundConcurrency);
}

void AWallGate::OnUnitDies(const ERTSDeathType DeathType)
{
	// Before Super: BP_OnUnitDies may destroy the actor right away.
	if (IsUnitAlive())
	{
		StopGateOnDeath();
	}
	Super::OnUnitDies(DeathType);
}

void AWallGate::StopGateOnDeath()
{
	M_Motion.State = EWallGateState::Wrecked;
	SetActorTickEnabled(false);
	if (UWorld* const World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_DetectionTimerHandle);
	}
	// The leaf goes with the wall: its hole component (if any) still throws its pieces when the owner dies.
	if (GetIsValidGateLeafMesh())
	{
		GateLeafMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		GateLeafMesh->SetVisibility(false, false);
	}
	// Nothing is left to stop units in the doorway.
	if (GetIsValidGateNavModifier())
	{
		GateNavModifier->SetAreaClass(UNavArea_Default::StaticClass());
	}
}

bool AWallGate::GetDoorwayNavBox(FBox& OutLeafSpaceBox, FTransform& OutLeafTransform) const
{
	if (not IsValid(GateLeafMesh) || not GetLeafRestBox(OutLeafSpaceBox))
	{
		return false;
	}
	OutLeafTransform = GateLeafMesh->GetComponentTransform();
	const FVector LeafScale = OutLeafTransform.GetScale3D().GetAbs().ComponentMax(FVector(UE_KINDA_SMALL_NUMBER));
	const FVector LeafPadding = FVector(NavDoorwayPadding) / LeafScale;
	const FVector WorldSize = OutLeafSpaceBox.GetSize() * LeafScale;
	// Thicken across the wall (the leaf's thinner horizontal axis) and down into the ground; not along the doorway,
	// where the box would reach into the pillars' blocked boxes.
	const bool bIsThinAlongX = WorldSize.X <= WorldSize.Y;
	const FVector AcrossPadding(bIsThinAlongX ? LeafPadding.X : 0.0, bIsThinAlongX ? 0.0 : LeafPadding.Y, 0.0);
	OutLeafSpaceBox.Min -= AcrossPadding + FVector(0.0, 0.0, LeafPadding.Z);
	OutLeafSpaceBox.Max += AcrossPadding;
	return true;
}

bool AWallGate::GetFrameNavBoxes(TArray<FBox>& OutFrameSpaceBoxes, FTransform& OutFrameTransform) const
{
	OutFrameSpaceBoxes.Reset();
	const UStaticMeshComponent* const FrameMesh = FindFrameMesh();
	const UStaticMesh* const Mesh = IsValid(FrameMesh) ? FrameMesh->GetStaticMesh() : nullptr;
	const UBodySetup* const BodySetup = IsValid(Mesh) ? Mesh->GetBodySetup() : nullptr;
	if (not IsValid(BodySetup))
	{
		return false;
	}
	OutFrameTransform = FrameMesh->GetComponentTransform();
	const FKAggregateGeom& Geometry = BodySetup->AggGeom;
	TArray<FBox> ElementBoxes;
	for (const FKBoxElem& BoxElem : Geometry.BoxElems)
	{
		ElementBoxes.Add(BoxElem.CalcAABB(FTransform::Identity, 1.f));
	}
	for (const FKSphereElem& SphereElem : Geometry.SphereElems)
	{
		ElementBoxes.Add(SphereElem.CalcAABB(FTransform::Identity, 1.f));
	}
	for (const FKSphylElem& SphylElem : Geometry.SphylElems)
	{
		ElementBoxes.Add(SphylElem.CalcAABB(FTransform::Identity, 1.f));
	}
	for (const FKConvexElem& ConvexElem : Geometry.ConvexElems)
	{
		ElementBoxes.Add(ConvexElem.ElemBox);
	}
	for (const FBox& ElementBox : ElementBoxes)
	{
		// A lintel over the doorway: units pass under it.
		if (not ElementBox.IsValid || ElementBox.Min.Z > FrameNavMaxBoxBottom)
		{
			continue;
		}
		OutFrameSpaceBoxes.Add(ElementBox);
	}
	return not OutFrameSpaceBoxes.IsEmpty();
}

bool AWallGate::GetLeafRestBox(FBox& OutLeafSpaceBox) const
{
	const UStaticMesh* const LeafMesh = IsValid(GateLeafMesh) ? GateLeafMesh->GetStaticMesh() : nullptr;
	if (not IsValid(LeafMesh))
	{
		return false;
	}
	// GetBoundingBox includes the mesh's bounds extensions, which a VAT leaf may use to cover its motion; the doorway
	// is the rest pose without them.
	const FBox ExtendedBox = LeafMesh->GetBoundingBox();
	OutLeafSpaceBox = FBox(ExtendedBox.Min + LeafMesh->GetNegativeBoundsExtension(),
	                       ExtendedBox.Max - LeafMesh->GetPositiveBoundsExtension());
	return OutLeafSpaceBox.IsValid != 0;
}

FVector AWallGate::GetDoorwayWorldCentre() const
{
	FBox LeafSpaceBox;
	if (not GetLeafRestBox(LeafSpaceBox))
	{
		// The wall origin sits at a corner, so this is only a fallback.
		return GetActorLocation();
	}
	return GateLeafMesh->GetComponentTransform().TransformPosition(LeafSpaceBox.GetCenter());
}

UStaticMeshComponent* AWallGate::FindFrameMesh() const
{
	TInlineComponentArray<UStaticMeshComponent*> MeshComponents(this);
	for (UStaticMeshComponent* const MeshComponent : MeshComponents)
	{
		if (IsValid(MeshComponent) && MeshComponent != GateLeafMesh && MeshComponent->GetFName() == FrameMeshComponentName)
		{
			return MeshComponent;
		}
	}
	return nullptr;
}

TSubclassOf<UNavArea> AWallGate::GetClosedNavAreaClass() const
{
	if (not bM_UsePlayerAwareNavArea)
	{
		return UNavArea_Null::StaticClass();
	}
	const TSubclassOf<UNavArea> AllowedPlayerArea = RTSWallGateNavAreas::GetClosedGateAreaOfPlayer(AllowedPlayer);
	if (not AllowedPlayerArea)
	{
		return UNavArea_Null::StaticClass();
	}
	return AllowedPlayerArea;
}

bool AWallGate::GetAreRTSNavFiltersGateAware()
{
	const URTSNavQueryFilterSettings* const Settings = URTSNavQueryFilterSettings::Get();
	if (Settings == nullptr)
	{
		return false;
	}
	TArray<TSoftClassPtr<UNavigationQueryFilter>> FilterClasses;
	FilterClasses.Add(Settings->DefaultFilterClass);
	for (const TPair<ERTSNavAgents, TSoftClassPtr<UNavigationQueryFilter>>& AgentFilter : Settings->NavFiltersByAgent)
	{
		FilterClasses.Add(AgentFilter.Value);
	}
	for (const TSoftClassPtr<UNavigationQueryFilter>& FilterClass : FilterClasses)
	{
		// A unit without a filter plans with the navigation data's default, which knows nothing of gate sides.
		const UClass* const LoadedClass = FilterClass.LoadSynchronous();
		if (LoadedClass == nullptr || not LoadedClass->IsChildOf(URTSWallGateNavQueryFilter::StaticClass()))
		{
			return false;
		}
	}
	return true;
}

bool AWallGate::GetIsValidGateLeafMesh() const
{
	if (IsValid(GateLeafMesh))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "GateLeafMesh", "GetIsValidGateLeafMesh", this);
	return false;
}

bool AWallGate::GetIsValidFrameNavModifier() const
{
	if (IsValid(FrameNavModifier))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "FrameNavModifier", "GetIsValidFrameNavModifier", this);
	return false;
}

bool AWallGate::GetIsValidGateNavModifier() const
{
	if (IsValid(GateNavModifier))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "GateNavModifier", "GetIsValidGateNavModifier", this);
	return false;
}
