// Copyright (C) Bas Blokzijl - All rights reserved.

#include "BuriedVehicleSite.h"

#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"
#include "NavAreas/NavArea_Null.h"
#include "RTS_Survival/RTSCollisionTraceChannels.h"
#include "RTS_Survival/Game/Players/RTSPlayers.h"
#include "RTS_Survival/RTSComponents/TimeProgressBarWidget.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/RTSDeathType/RTSDeathType.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/CollisionSetup/RTSPlayerCollision.h"
#include "RTS_Survival/Utils/UnitSpawning/RTSUnitOwnerSpawning.h"

namespace BuriedVehicleSitePrivate
{
	constexpr float ProgressBarHeight = 350.f;
	const FVector NavBlockerDefaultExtent(300.0, 170.0, 150.0);
}

ABuriedVehicleSite::ABuriedVehicleSite(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Sand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Sand"));
	RootComponent = Sand;
	Sand->SetCanEverAffectNavigation(false);

	Pose = CreateDefaultSubobject<USceneComponent>(TEXT("Pose"));
	Pose->SetupAttachment(Sand);

	ProgressBarWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("ProgressBarWidget"));
	ProgressBarWidget->SetupAttachment(Sand);
	ProgressBarWidget->SetWidgetSpace(EWidgetSpace::Screen);
	ProgressBarWidget->SetRelativeLocation(FVector(0.0, 0.0, BuriedVehicleSitePrivate::ProgressBarHeight));
	ProgressBarWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProgressBarWidget->SetCanEverAffectNavigation(false);

	NavBlocker = CreateDefaultSubobject<UBoxComponent>(TEXT("NavBlocker"));
	NavBlocker->SetupAttachment(Sand);
	NavBlocker->SetBoxExtent(BuriedVehicleSitePrivate::NavBlockerDefaultExtent);
	NavBlocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NavBlocker->SetGenerateOverlapEvents(false);
	NavBlocker->SetCanEverAffectNavigation(true);
	NavBlocker->bDynamicObstacle = true;
	// Also turns off the system default obstacle area.
	NavBlocker->SetAreaClassOverride(UNavArea_Null::StaticClass());
	NavBlocker->SetHiddenInGame(true);

	// The squad takes command of the vehicle; it is the reward, so no resources.
	bConsumeSquadWhenFinished = true;
	bGivePlayerResourceRewards = false;
	ScavengeTime = 30.f;

	// Before Super::PostInitializeComponents, which otherwise searches for a tagged mesh.
	SetScavengeMesh(Sand);
}

AActor* ABuriedVehicleSite::GetRecoveredVehicle() const
{
	return M_RecoveredVehicle.Get();
}

void ABuriedVehicleSite::PostInitializeComponents()
{
	// Gives the sand the scavengeable collision; the two setups below adjust it.
	Super::PostInitializeComponents();
	PostInitializeComponents_SetupSandCollision();
	PostInitializeComponents_SetupBuriedPartCollision();
}

void ABuriedVehicleSite::PostInitializeComponents_SetupSandCollision() const
{
	if (not GetIsValidSand())
	{
		return;
	}
	// Units wade through the low drift ring; the nav blocker keeps them off the hull and the high sand.
	Sand->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Sand->SetCollisionResponseToChannel(ECC_Vehicle, ECR_Ignore);
	Sand->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);
	Sand->SetCollisionResponseToChannel(COLLISION_OBJ_PLAYER, ECR_Ignore);
	Sand->SetCollisionResponseToChannel(COLLISION_OBJ_ENEMY, ECR_Ignore);
	Sand->SetCollisionResponseToChannel(COLLISION_OBJ_ENEMY_HULL, ECR_Ignore);
}

void ABuriedVehicleSite::PostInitializeComponents_SetupBuriedPartCollision() const
{
	if (not GetIsValidPose())
	{
		return;
	}
	// The hull and gun stick out of the sand: they must take the scavenge click (visibility) and stop shells.
	TArray<USceneComponent*> BuriedParts;
	Pose->GetChildrenComponents(true, BuriedParts);
	for (USceneComponent* const BuriedPart : BuriedParts)
	{
		UPrimitiveComponent* const Primitive = Cast<UPrimitiveComponent>(BuriedPart);
		if (not IsValid(Primitive))
		{
			continue;
		}
		Primitive->SetCollisionObjectType(ECC_Destructible);
		Primitive->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Primitive->SetCollisionResponseToAllChannels(ECR_Ignore);
		Primitive->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Primitive->SetCollisionResponseToChannel(COLLISION_OBJ_PROJECTILE, ECR_Block);
		FRTSPlayerCollision::SetWeaponHitResponsesForAllPlayers(Primitive);
	}
}

void ABuriedVehicleSite::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_InitProgressBar();
}

void ABuriedVehicleSite::BeginPlay_InitProgressBar()
{
	if (not GetIsValidProgressBarWidget())
	{
		return;
	}
	UTimeProgressBarWidget* const TimeProgressBar = Cast<UTimeProgressBarWidget>(
		ProgressBarWidget->GetUserWidgetObject());
	if (not IsValid(TimeProgressBar))
	{
		RTSFunctionLibrary::ReportError("Buried vehicle site has no UTimeProgressBarWidget on ProgressBarWidget."
			"\n Set the widget class in BP_BuriedArmour_Base. Site: " + GetName());
		return;
	}
	TimeProgressBar->InitTimeProgressComponent(ProgressBarWidget);
	InitScavObject(TimeProgressBar);
}

void ABuriedVehicleSite::OnScavengeCompletedBySquad(ASquadController& ScavengingSquad)
{
	M_RecoveringPlayer = FRTSPlayers::GetOwningPlayerOfActor(&ScavengingSquad);
}

void ABuriedVehicleSite::OnUnitDies(const ERTSDeathType DeathType)
{
	// Scavenging completion triggers this death; the vehicle spawns before BP_OnUnitDies runs in the base.
	if (IsUnitAlive() && DeathType == ERTSDeathType::Scavenging)
	{
		RecoverVehicle();
	}
	Super::OnUnitDies(DeathType);
}

float ABuriedVehicleSite::TakeDamage(const float DamageAmount, FDamageEvent const& DamageEvent,
                                     AController* EventInstigator, AActor* DamageCauser)
{
	if (not bCanBeDestroyedByDamage)
	{
		return 0.f;
	}
	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

void ABuriedVehicleSite::RecoverVehicle()
{
	// Hide and clear first: the vehicle spawns inside the mound and must be able to path out of it.
	HideBuriedParts();
	ClearMoundBlocking();
	AActor* const Vehicle = SpawnRecoveredVehicle();
	if (not IsValid(Vehicle))
	{
		return;
	}
	M_RecoveredVehicle = Vehicle;
	OnVehicleRecovered.Broadcast(Vehicle);
	BP_OnVehicleRecovered(Vehicle);
}

void ABuriedVehicleSite::HideBuriedParts() const
{
	if (not GetIsValidPose())
	{
		return;
	}
	TArray<USceneComponent*> BuriedParts;
	Pose->GetChildrenComponents(true, BuriedParts);
	for (USceneComponent* const BuriedPart : BuriedParts)
	{
		if (UPrimitiveComponent* const Primitive = Cast<UPrimitiveComponent>(BuriedPart))
		{
			Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
	constexpr bool bPropagateToChildren = true;
	Pose->SetVisibility(false, bPropagateToChildren);
}

void ABuriedVehicleSite::ClearMoundBlocking()
{
	// The mound stays as a low plateau to look at; clicks and shells now go to the vehicle, not the dead site.
	if (GetIsValidSand())
	{
		Sand->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (GetIsValidNavBlocker())
	{
		// Unregistering removes the dynamic obstacle from the navmesh.
		NavBlocker->DestroyComponent();
		NavBlocker = nullptr;
	}
}

AActor* ABuriedVehicleSite::SpawnRecoveredVehicle() const
{
	UWorld* const World = GetWorld();
	if (not IsValid(World))
	{
		return nullptr;
	}
	if (not IsValid(RecoveredVehicleClass))
	{
		RTSFunctionLibrary::ReportError("Buried vehicle site has no RecoveredVehicleClass: " + GetName());
		return nullptr;
	}
	const FRotator YawOnly(0.0, GetActorRotation().Yaw, 0.0);
	const FVector SpawnLocation = GetActorLocation() + YawOnly.RotateVector(VehicleSpawnOffset);
	const FTransform SpawnTransform(YawOnly, SpawnLocation);
	AActor* const Vehicle = FRTSUnitOwnerSpawning::SpawnUnitForPlayer(
		*World, RecoveredVehicleClass.Get(), SpawnTransform, static_cast<uint8>(GetRecoveringPlayerOrHuman()),
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (not IsValid(Vehicle))
	{
		RTSFunctionLibrary::ReportError("Buried vehicle site failed to spawn " + RecoveredVehicleClass->GetName() +
			" at " + GetName());
	}
	return Vehicle;
}

int32 ABuriedVehicleSite::GetRecoveringPlayerOrHuman() const
{
	if (FRTSPlayers::IsValidPlayer(M_RecoveringPlayer))
	{
		return M_RecoveringPlayer;
	}
	RTSFunctionLibrary::ReportError("Buried vehicle site could not read the scavenging squad's owner;"
		" the vehicle goes to the human player. Site: " + GetName());
	return RTSPlayers::HumanPlayer;
}

bool ABuriedVehicleSite::GetIsValidSand() const
{
	if (IsValid(Sand))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "Sand", "GetIsValidSand", this);
	return false;
}

bool ABuriedVehicleSite::GetIsValidProgressBarWidget() const
{
	if (IsValid(ProgressBarWidget))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "ProgressBarWidget", "GetIsValidProgressBarWidget",
	                                                      this);
	return false;
}

bool ABuriedVehicleSite::GetIsValidNavBlocker() const
{
	if (IsValid(NavBlocker))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "NavBlocker", "GetIsValidNavBlocker", this);
	return false;
}

bool ABuriedVehicleSite::GetIsValidPose() const
{
	if (IsValid(Pose))
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised(this, "Pose", "GetIsValidPose", this);
	return false;
}
