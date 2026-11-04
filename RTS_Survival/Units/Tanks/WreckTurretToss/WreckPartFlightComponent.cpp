#include "WreckPartFlightComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "PhysicsEngine/BodySetup.h"
#include "Sound/SoundCue.h"
#include "TimerManager.h"
#include "RTS_Survival/Game/GameState/GameExplosionManager/ExplosionManager.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"

namespace WreckPartFlightConstants
{
	constexpr float LandingCheckInterval = 0.1f;
	// Ignore the first moments of flight: the part still touches the hull and has no velocity before the launch.
	constexpr float MinFlightTimeBeforeLanding = 0.5f;
	// Angular damping while flying so a rolling part comes to rest instead of rocking on a curved face.
	constexpr float FlyingAngularDamping = 0.6f;
	// A part this far below its launch location when it settles fell through the world and is hidden.
	constexpr float FellThroughWorldDepth = 5000.f;
}

UWreckPartFlightComponent::UWreckPartFlightComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UWreckPartFlightComponent* UWreckPartFlightComponent::AddToWreck(AActor* Wreck)
{
	if (not IsValid(Wreck))
	{
		return nullptr;
	}
	UWreckPartFlightComponent* PartFlight = NewObject<UWreckPartFlightComponent>(Wreck);
	Wreck->AddInstanceComponent(PartFlight);
	PartFlight->RegisterComponent();
	return PartFlight;
}

bool UWreckPartFlightComponent::GetCanSimulatePart(UStaticMeshComponent& PartMesh)
{
	// Non-const: the engine's GetBodySetup is not const.
	const UBodySetup* PartBodySetup = PartMesh.GetBodySetup();
	return IsValid(PartMesh.GetStaticMesh()) && IsValid(PartBodySetup)
		&& PartBodySetup->AggGeom.GetElementCount() > 0;
}

void UWreckPartFlightComponent::SpawnWreckExplosion(const UObject* WorldContext, const ERTS_ExplosionType ExplosionType,
                                                    const FVector& Location, USoundCue* OverrideSound)
{
	UGameExplosionsManager* ExplosionsManager = FRTS_Statics::GetGameExplosionsManager(WorldContext);
	if (not IsValid(ExplosionsManager))
	{
		return;
	}
	if (IsValid(OverrideSound))
	{
		ExplosionsManager->SpawnExplosionAtLocationCustomSound(ExplosionType, Location, OverrideSound);
		return;
	}
	ExplosionsManager->SpawnRTSExplosionAtLocation(ExplosionType, Location, true);
}

void UWreckPartFlightComponent::SetupPart(UStaticMeshComponent* PartMesh, const FWreckPartFlightSettings& Settings)
{
	M_PartMesh = PartMesh;
	M_Settings = Settings;
	StorePartState();
}

void UWreckPartFlightComponent::Launch(const FVector& LaunchVelocity, const FVector& AngularVelocityDegrees)
{
	UWorld* World = GetWorld();
	if (not GetIsValidPartMesh() || not IsValid(World))
	{
		return;
	}

	M_LaunchLocation = M_PartMesh->GetComponentLocation();
	PrepareForFlight();
	// The body only turns dynamic once physics has processed the simulate switch; a velocity set now is lost.
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(
		this, [this, LaunchVelocity, AngularVelocityDegrees]()
		{
			ApplyLaunchVelocity(LaunchVelocity, AngularVelocityDegrees);
		}));
	StartLandingChecks();
}

void UWreckPartFlightComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_LandingCheckTimerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void UWreckPartFlightComponent::StorePartState()
{
	if (not GetIsValidPartMesh())
	{
		return;
	}
	M_RestoreState.CollisionResponses = M_PartMesh->GetCollisionResponseToChannels();
	M_RestoreState.ObjectType = M_PartMesh->GetCollisionObjectType();
	M_RestoreState.CollisionEnabled = M_PartMesh->GetCollisionEnabled();
	M_RestoreState.Mobility = M_PartMesh->Mobility;
}

void UWreckPartFlightComponent::PrepareForFlight()
{
	if (not GetIsValidPartMesh())
	{
		return;
	}

	M_PartMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	M_PartMesh->SetMobility(EComponentMobility::Movable);
	// Only the world blocks the flying part: it must not shove units or bounce off its own hull.
	M_PartMesh->SetCollisionObjectType(ECC_PhysicsBody);
	M_PartMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	M_PartMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	M_PartMesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	M_PartMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	M_PartMesh->SetNotifyRigidBodyCollision(true);
	M_PartMesh->OnComponentHit.AddDynamic(this, &UWreckPartFlightComponent::OnPartHit);
	M_PartMesh->SetAngularDamping(WreckPartFlightConstants::FlyingAngularDamping);
	// Wreck parts often have gravity off since they never simulate; without it the part never comes down.
	M_PartMesh->SetEnableGravity(true);
	M_PartMesh->SetSimulatePhysics(true);
	bM_IsFlying = true;
}

void UWreckPartFlightComponent::ApplyLaunchVelocity(const FVector& LaunchVelocity,
                                                    const FVector& AngularVelocityDegrees) const
{
	if (not GetIsValidPartMesh())
	{
		return;
	}
	M_PartMesh->WakeRigidBody();
	M_PartMesh->SetPhysicsLinearVelocity(LaunchVelocity);
	M_PartMesh->SetPhysicsAngularVelocityInDegrees(AngularVelocityDegrees);
}

void UWreckPartFlightComponent::StartLandingChecks()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}

	M_LaunchWorldTime = World->GetTimeSeconds();
	M_TimeBelowSettleSpeed = 0.f;
	FTimerDelegate LandingCheckDelegate;
	LandingCheckDelegate.BindWeakLambda(this, [this]()
	{
		CheckIfLanded();
	});
	World->GetTimerManager().SetTimer(M_LandingCheckTimerHandle, LandingCheckDelegate,
	                                  WreckPartFlightConstants::LandingCheckInterval, true);
}

void UWreckPartFlightComponent::CheckIfLanded()
{
	const UWorld* World = GetWorld();
	if (not bM_IsFlying || not IsValid(World) || not GetIsValidPartMesh())
	{
		return;
	}

	const float FlightTime = World->GetTimeSeconds() - M_LaunchWorldTime;
	if (FlightTime >= M_Settings.MaxFlightTime)
	{
		Settle();
		return;
	}
	if (FlightTime < WreckPartFlightConstants::MinFlightTimeBeforeLanding)
	{
		return;
	}
	if (not M_PartMesh->IsAnyRigidBodyAwake())
	{
		Settle();
		return;
	}

	const bool bIsSlow = M_PartMesh->GetPhysicsLinearVelocity().Size() <= M_Settings.SettleLinearSpeed
		&& M_PartMesh->GetPhysicsAngularVelocityInDegrees().Size() <= M_Settings.SettleAngularSpeed;
	M_TimeBelowSettleSpeed = bIsSlow ? M_TimeBelowSettleSpeed + WreckPartFlightConstants::LandingCheckInterval : 0.f;
	if (M_TimeBelowSettleSpeed >= M_Settings.SettleDuration)
	{
		Settle();
	}
}

void UWreckPartFlightComponent::Settle()
{
	bM_IsFlying = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_LandingCheckTimerHandle);
	}
	if (not GetIsValidPartMesh())
	{
		return;
	}

	// From here on the part is an ordinary static wreck part: no simulation, the wreck's own collision again.
	M_PartMesh->SetSimulatePhysics(false);
	M_PartMesh->SetNotifyRigidBodyCollision(false);
	M_PartMesh->OnComponentHit.RemoveDynamic(this, &UWreckPartFlightComponent::OnPartHit);
	M_PartMesh->SetCollisionObjectType(M_RestoreState.ObjectType);
	M_PartMesh->SetCollisionResponseToChannels(M_RestoreState.CollisionResponses);
	M_PartMesh->SetCollisionEnabled(M_RestoreState.CollisionEnabled);
	if (M_RestoreState.Mobility != EComponentMobility::Movable)
	{
		M_PartMesh->SetMobility(M_RestoreState.Mobility);
	}

	if (M_PartMesh->GetComponentLocation().Z < M_LaunchLocation.Z - WreckPartFlightConstants::FellThroughWorldDepth)
	{
		M_PartMesh->SetHiddenInGame(true);
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("UWreckPartFlightComponent::Settle - thrown part %s fell through the world and was hidden."),
			*M_PartMesh->GetName()));
	}
}

void UWreckPartFlightComponent::OnPartHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
                                          UPrimitiveComponent* OtherComp, FVector NormalImpulse,
                                          const FHitResult& Hit)
{
	if (bM_HasPlayedLandingImpact || not M_Settings.bPlayLandingImpact)
	{
		return;
	}
	const UWorld* World = GetWorld();
	if (not IsValid(World)
		|| World->GetTimeSeconds() - M_LaunchWorldTime < WreckPartFlightConstants::MinFlightTimeBeforeLanding)
	{
		return;
	}

	bM_HasPlayedLandingImpact = true;
	SpawnWreckExplosion(this, M_Settings.LandingExplosionType, Hit.ImpactPoint, M_Settings.LandingSound);
}

bool UWreckPartFlightComponent::GetIsValidPartMesh() const
{
	if (M_PartMesh.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(
		this, "M_PartMesh", "GetIsValidPartMesh", this);
	return false;
}
