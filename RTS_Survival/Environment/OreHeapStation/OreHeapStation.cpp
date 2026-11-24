// Copyright (C) Bas Blokzijl - All rights reserved.

#include "OreHeapStation.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowComp.h"
#include "RTS_Survival/FOWSystem/FowComponent/FowType.h"
#include "RTS_Survival/Game/GameState/CPPGameState.h"
#include "RTS_Survival/Game/GameState/GameUnitManager/GameUnitManager.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "RTS_Survival/Units/Enums/Enum_UnitType.h"
#include "RTS_Survival/Utils/CollisionSetup/RTSPlayerCollision.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

namespace OreHeapStationConstants
{
	// More than any squad has: a held station (or one being captured) refuses capture orders.
	constexpr int32 LockedCaptureUnitsNeeded = 999;
	constexpr int32 DefaultCaptureUnitsNeeded = 2;
	constexpr float DefaultCaptureDurationSeconds = 20.f;
	// The drop-off reads this socket; the drop-off point has no mesh, so it resolves to the component itself.
	const FName DropOffSocketName = TEXT("DropOff");
	// Registered with no storage: capacity is added while held (AddExpansionStorage) and taken away when knocked out.
	constexpr int32 RegisteredWithoutStorage = 0;
}

UOreHeapStationRTSComponent::UOreHeapStationRTSComponent()
{
	OwningPlayer = RTSPlayers::NeutralOwner;
	UnitType = EAllUnitType::UNType_GarrisonBuilding;
	// The station puts itself in the unit manager while held (URTSComponent does not keep this class).
	bAddToGameState = false;
	FormationUnitInnerRadius = 0.f;
}

void UOreHeapStationDropOff::InitStationDropOff(const TMap<ERTSResourceType, int32>& MaxDropOffPerResource,
                                                UMeshComponent* OwnerMeshComponent, const FName DropOffSocketName)
{
	InitResourceDropOff(MaxDropOffPerResource, OwnerMeshComponent, DropOffSocketName);
}

AOreHeapStation::AOreHeapStation(const FObjectInitializer& ObjectInitializer)
	: ACaptureActor(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;

	StationRoot = CreateDefaultSubobject<USceneComponent>(TEXT("StationRoot"));
	SetRootComponent(StationRoot);

	StationMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StationMesh"));
	StationMesh->SetupAttachment(StationRoot);
	StationMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	StationMesh->SetCanEverAffectNavigation(true);

	DropOffPoint = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DropOffPoint"));
	DropOffPoint->SetupAttachment(StationRoot);
	DropOffPoint->SetRelativeLocation(FVector(700.f, 0.f, 0.f));
	DropOffPoint->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DropOffPoint->SetCanEverAffectNavigation(false);
	DropOffPoint->SetHiddenInGame(true);

	StationRTSComponent = CreateDefaultSubobject<UOreHeapStationRTSComponent>(TEXT("RTSComponent"));
	StationDropOff = CreateDefaultSubobject<UOreHeapStationDropOff>(TEXT("ResourceDropOff"));
	StationFowComp = CreateDefaultSubobject<UFowComp>(TEXT("FowComp"));

	Health = StationSettings.HeldHealth;
	CaptureSettings.AmountCaptureUnitsNeeded = OreHeapStationConstants::DefaultCaptureUnitsNeeded;
	CaptureSettings.CaptureDuration = OreHeapStationConstants::DefaultCaptureDurationSeconds;
	CaptureProgressText = NSLOCTEXT("OreHeapStation", "CaptureProgress", "Taking the loading station...");
}

void AOreHeapStation::BeginPlay()
{
	Super::BeginPlay();

	// Off until the first capture registers it; harvesters skip inactive drop-offs.
	if (GetIsValidStationDropOff())
	{
		StationDropOff->SetIsDropOffActive(false);
	}
	Health = StationSettings.HeldHealth;
	BeginPlay_SetupStationMesh();
	BeginPlay_CacheTaggedComponents();
	ApplyLook();
}

void AOreHeapStation::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_StationCaptureTimerHandle);
	}
	SetTargetRegistered(false, M_RegisteredTargetPlayer);
	Super::EndPlay(EndPlayReason);
}

int32 AOreHeapStation::GetCaptureUnitAmountNeeded() const
{
	if (bM_IsHeld || bM_IsCaptureInProgress)
	{
		return OreHeapStationConstants::LockedCaptureUnitsNeeded;
	}
	return Super::GetCaptureUnitAmountNeeded();
}

void AOreHeapStation::OnCaptureByPlayer(const int32 Player)
{
	if (bM_IsHeld || bM_IsCaptureInProgress)
	{
		// A second squad that slipped through must not restart the progress bar.
		return;
	}
	bM_IsCaptureInProgress = true;
	Super::OnCaptureByPlayer(Player);

	const float CaptureDurationSeconds = CaptureSettings.CaptureDuration;
	UWorld* World = GetWorld();
	if (CaptureDurationSeconds <= 0.f || not IsValid(World))
	{
		OnStationCaptured(Player);
		return;
	}
	TWeakObjectPtr<AOreHeapStation> WeakThis(this);
	World->GetTimerManager().SetTimer(
		M_StationCaptureTimerHandle,
		[WeakThis, Player]()
		{
			if (not WeakThis.IsValid())
			{
				return;
			}
			WeakThis->OnStationCaptured(Player);
		},
		CaptureDurationSeconds,
		false);
}

FVector AOreHeapStation::GetCaptureLocationClosestTo(const FVector& FromLocation)
{
	if (IsValid(DropOffPoint))
	{
		return DropOffPoint->GetComponentLocation();
	}
	return Super::GetCaptureLocationClosestTo(FromLocation);
}

void AOreHeapStation::OnStationCaptured(const int32 Player)
{
	bM_IsCaptureInProgress = false;
	if (bM_IsHeld || not FRTSPlayers::IsValidPlayer(Player))
	{
		return;
	}
	bM_IsHeld = true;
	bM_WasEverHeld = true;
	M_HoldingPlayer = Player;
	Health = StationSettings.HeldHealth;

	if (GetIsValidStationRTSComponent())
	{
		StationRTSComponent->SetOwningPlayerRuntime(static_cast<uint8>(Player));
	}
	BeginPlay_SetupStationMesh();
	// Drop-offs only exist for the human player (UGameResourceManager refuses other owners).
	if (FRTSPlayers::IsHumanPlayer(Player))
	{
		InitDropOffOnce();
		SetDropOffHeld(true);
		StartStationVision();
	}
	SetTargetRegistered(true, Player);
	ApplyLook();

	OnHeapStationStateChanged.Broadcast(this, true, Player);
	BP_OnStationStateChanged(true, Player);
}

void AOreHeapStation::KnockOutStation()
{
	if (not bM_IsHeld)
	{
		return;
	}
	const int32 FormerHolder = M_HoldingPlayer;
	bM_IsHeld = false;
	M_HoldingPlayer = INDEX_NONE;
	Health = StationSettings.HeldHealth;

	SetDropOffHeld(false);
	SetTargetRegistered(false, FormerHolder);
	// Before the owner change: the fog component would otherwise switch to the enemy-vision behaviour and hide it.
	StopStationVision();
	if (GetIsValidStationRTSComponent() && FRTSPlayers::IsValidPlayer(StationSettings.KnockedOutOwner))
	{
		StationRTSComponent->SetOwningPlayerRuntime(static_cast<uint8>(StationSettings.KnockedOutOwner));
	}
	BeginPlay_SetupStationMesh();
	ApplyLook();

	OnHeapStationStateChanged.Broadcast(this, false, FormerHolder);
	BP_OnStationStateChanged(false, FormerHolder);
}

float AOreHeapStation::TakeDamage(const float DamageAmount, FDamageEvent const& DamageEvent,
                                  AController* EventInstigator, AActor* DamageCauser)
{
	// Only a held station can be knocked out; it never dies, so it can be captured again.
	if (not bM_IsHeld)
	{
		return 0.f;
	}
	const float DamageTaken = FMath::Max(0.f, DamageAmount - FMath::Max(0.f, DamageReduction));
	Health -= DamageTaken;
	if (Health <= 0.f)
	{
		KnockOutStation();
	}
	return DamageTaken;
}

void AOreHeapStation::OnResourceStorageChanged(int32 PercentageResourcesFilled, const ERTSResourceType ResourceType)
{
	// No fill stages on the placeholder look.
}

void AOreHeapStation::OnResourceStorageEmpty()
{
}

TArray<AOreHeapStation*> AOreHeapStation::GetAllHeapStations(const UObject* WorldContextObject)
{
	TArray<AOreHeapStation*> Stations;
	UWorld* World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	if (not IsValid(World))
	{
		return Stations;
	}
	for (TActorIterator<AOreHeapStation> It(World); It; ++It)
	{
		Stations.Add(*It);
	}
	return Stations;
}

void AOreHeapStation::BeginPlay_SetupStationMesh() const
{
	if (not IsValid(StationMesh))
	{
		return;
	}
	// Also after every owner change: the owner mask must not let the holder's or anyone's shots pass through.
	StationMesh->SetCollisionResponseToChannel(COLLISION_OBJ_PROJECTILE, ECR_Block);
	FRTSPlayerCollision::SetWeaponHitResponsesForAllPlayers(StationMesh, ECR_Block);
}

void AOreHeapStation::BeginPlay_CacheTaggedComponents()
{
	M_HeldComponents.Reset();
	M_KnockedOutComponents.Reset();
	M_IntactComponents.Reset();
	TArray<USceneComponent*> SceneComponents;
	GetComponents<USceneComponent>(SceneComponents);
	for (USceneComponent* SceneComponent : SceneComponents)
	{
		if (SceneComponent->ComponentHasTag(StationSettings.HeldTag))
		{
			M_HeldComponents.Add(SceneComponent);
		}
		if (SceneComponent->ComponentHasTag(StationSettings.KnockedOutTag))
		{
			M_KnockedOutComponents.Add(SceneComponent);
		}
		if (SceneComponent->ComponentHasTag(StationSettings.IntactTag))
		{
			M_IntactComponents.Add(SceneComponent);
		}
	}
}

void AOreHeapStation::ApplyLook() const
{
	// Knocked out = held once and not now; a station nobody has taken yet looks intact.
	const bool bIsKnockedOut = not bM_IsHeld && bM_WasEverHeld;
	SetTaggedVisible(M_HeldComponents, bM_IsHeld);
	SetTaggedVisible(M_KnockedOutComponents, bIsKnockedOut);
	SetTaggedVisible(M_IntactComponents, not bIsKnockedOut);
}

void AOreHeapStation::SetTaggedVisible(const TArray<TObjectPtr<USceneComponent>>& Components, const bool bVisible) const
{
	constexpr bool bPropagateToChildren = true;
	for (USceneComponent* Component : Components)
	{
		if (not IsValid(Component))
		{
			continue;
		}
		Component->SetVisibility(bVisible, bPropagateToChildren);
	}
}

void AOreHeapStation::InitDropOffOnce()
{
	if (bM_IsDropOffInitialised || not GetIsValidStationDropOff())
	{
		return;
	}
	// Registers with the game state for the RTS component's owner, which is the human by now. A second init would
	// reset the stored amounts, so it runs once; holding and losing the station only moves storage.
	TMap<ERTSResourceType, int32> Capacity;
	Capacity.Add(StationSettings.DropOffResource, OreHeapStationConstants::RegisteredWithoutStorage);
	StationDropOff->InitStationDropOff(Capacity, DropOffPoint, OreHeapStationConstants::DropOffSocketName);
	bM_IsDropOffInitialised = true;
}

void AOreHeapStation::SetDropOffHeld(const bool bHeld) const
{
	if (not bM_IsDropOffInitialised || not GetIsValidStationDropOff())
	{
		return;
	}
	if (bHeld)
	{
		StationDropOff->AddExpansionStorage(StationSettings.DropOffResource, StationSettings.StorageCapacity);
		StationDropOff->SetIsDropOffActive(true);
		return;
	}
	StationDropOff->SetIsDropOffActive(false);
	// What is stored here is lost with the storage (the player resource manager drops it from the totals).
	StationDropOff->RemoveExpansionStorage(StationSettings.DropOffResource, StationSettings.StorageCapacity);
}

void AOreHeapStation::SetTargetRegistered(const bool bRegister, const int32 Player)
{
	if (bRegister == bM_IsRegisteredTarget)
	{
		return;
	}
	const UWorld* World = GetWorld();
	const ACPPGameState* GameState = IsValid(World) ? Cast<ACPPGameState>(World->GetGameState()) : nullptr;
	UGameUnitManager* GameUnitManager = IsValid(GameState) ? GameState->GetGameUnitManager() : nullptr;
	if (not IsValid(GameUnitManager))
	{
		bM_IsRegisteredTarget = false;
		M_RegisteredTargetPlayer = INDEX_NONE;
		return;
	}
	if (bRegister)
	{
		// The generic actor list feeds the target snapshot: enemy weapons engage the held station like a building.
		GameUnitManager->AddActorForPlayer(this, static_cast<uint8>(Player));
		bM_IsRegisteredTarget = true;
		M_RegisteredTargetPlayer = Player;
		return;
	}
	GameUnitManager->RemoveActorForPlayer(this, static_cast<uint8>(M_RegisteredTargetPlayer));
	bM_IsRegisteredTarget = false;
	M_RegisteredTargetPlayer = INDEX_NONE;
}

void AOreHeapStation::StartStationVision()
{
	// The active fog component also tags the station when Soviet vision sees it; AI weapons need that tag.
	if (not IsValid(StationFowComp))
	{
		return;
	}
	if (StationSettings.VisionRadius > 0.f)
	{
		StationFowComp->SetVisionRadius(StationSettings.VisionRadius);
	}
	constexpr bool bStartImmediately = true;
	StationFowComp->StartFow(EFowBehaviour::Fow_Active, bStartImmediately);
}

void AOreHeapStation::StopStationVision() const
{
	if (not IsValid(StationFowComp))
	{
		return;
	}
	StationFowComp->StopFowParticipation();
}

bool AOreHeapStation::GetIsValidStationRTSComponent() const
{
	if (IsValid(StationRTSComponent))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "StationRTSComponent",
	                                                      "AOreHeapStation::GetIsValidStationRTSComponent", this);
	return false;
}

bool AOreHeapStation::GetIsValidStationDropOff() const
{
	if (IsValid(StationDropOff))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "StationDropOff",
	                                                      "AOreHeapStation::GetIsValidStationDropOff", this);
	return false;
}
