// Copyright (C) Bas Blokzijl - All rights reserved.

#include "AircraftCannonPodsComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "TimerManager.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/AircraftWeapon/AircraftWeapon.h"
#include "RTS_Survival/Weapons/BombComponent/BombComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSCannonPods, Log, All);

UAircraftCannonPodsComponent::UAircraftCannonPodsComponent()
	: CannonRange(DeveloperSettings::GameBalance::Ranges::AircraftSmallArmsRange)
{
	PrimaryComponentTick.bCanEverTick = false;
}

TArray<UWeaponState*> UAircraftCannonPodsComponent::GetCannons() const
{
	TArray<UWeaponState*> Cannons;
	for (const TObjectPtr<UWeaponState>& Cannon : M_Cannons)
	{
		if (IsValid(Cannon))
		{
			Cannons.Add(Cannon);
		}
	}
	return Cannons;
}

void UAircraftCannonPodsComponent::BeginPlay()
{
	Super::BeginPlay();
	M_Aircraft = Cast<AAircraftMaster>(GetOwner());
	if (not GetIsValidAircraft())
	{
		return;
	}
	// Components begin play inside the aircraft's Super::BeginPlay, before it initialises its bomb bay.
	if (bRemoveBombBay)
	{
		BeginPlay_RemoveBombBay();
	}
	USkeletalMeshComponent* AircraftMesh = M_Aircraft->FindComponentByClass<USkeletalMeshComponent>();
	if (not IsValid(AircraftMesh))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, TEXT("AircraftMesh"),
		                                             TEXT("UAircraftCannonPodsComponent::BeginPlay"));
		return;
	}
	BeginPlay_CreatePods(*AircraftMesh);
	M_Aircraft->SetTargetPreference(TargetPreference);
	BeginPlay_QueueCannonSetup();
}

bool UAircraftCannonPodsComponent::GetIsValidAircraft() const
{
	if (M_Aircraft.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportErrorVariableNotInitialised_Object(this, TEXT("M_Aircraft"),
	                                                             TEXT("GetIsValidAircraft"), GetOwner());
	return false;
}

void UAircraftCannonPodsComponent::BeginPlay_RemoveBombBay() const
{
	UBombComponent* BombBay = M_Aircraft->FindComponentByClass<UBombComponent>();
	if (not IsValid(BombBay))
	{
		return;
	}
	// The aircraft found its bomb bay in PostInitializeComponents; a destroyed component fails its validity checks,
	// so no bombs are armed, shown or thrown.
	BombBay->DestroyComponent();
}

void UAircraftCannonPodsComponent::BeginPlay_CreatePods(USkeletalMeshComponent& AircraftMesh)
{
	for (int32 PodIndex = 0; PodIndex < Pods.Num(); ++PodIndex)
	{
		const FAircraftCannonPod& Pod = Pods[PodIndex];
		if (not IsValid(Pod.PodMesh) || not AircraftMesh.DoesSocketExist(Pod.AttachSocketName))
		{
			RTSFunctionLibrary::ReportError(FString::Printf(
				TEXT("UAircraftCannonPodsComponent on %s: pod %d has no mesh or its socket %s is missing."),
				*GetOwner()->GetName(), PodIndex, *Pod.AttachSocketName.ToString()));
			continue;
		}
		const FName ComponentName(*FString::Printf(TEXT("CannonPod_%d"), PodIndex));
		UStaticMeshComponent* PodComponent = NewObject<UStaticMeshComponent>(GetOwner(), ComponentName);
		PodComponent->SetStaticMesh(Pod.PodMesh);
		PodComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PodComponent->SetCanEverAffectNavigation(false);
		PodComponent->SetupAttachment(&AircraftMesh, Pod.AttachSocketName);
		// The aircraft's bones carry the import scale; the pod is modelled in centimetres.
		PodComponent->SetUsingAbsoluteScale(true);
		PodComponent->SetWorldScale3D(FVector::OneVector);
		PodComponent->RegisterComponent();
		GetOwner()->AddInstanceComponent(PodComponent);
		M_PodComponents.Add(PodComponent);
	}
}

void UAircraftCannonPodsComponent::BeginPlay_QueueCannonSetup()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return;
	}
	TWeakObjectPtr<UAircraftCannonPodsComponent> WeakThis(this);
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->SetupCannons();
		}
	}));
}

void UAircraftCannonPodsComponent::SetupCannons()
{
	if (not GetIsValidAircraft())
	{
		return;
	}
	UAircraftWeapon* AircraftWeapon = M_Aircraft->FindComponentByClass<UAircraftWeapon>();
	USkeletalMeshComponent* AircraftMesh = M_Aircraft->FindComponentByClass<USkeletalMeshComponent>();
	const URTSComponent* RTSComponent = M_Aircraft->FindComponentByClass<URTSComponent>();
	if (not IsValid(AircraftWeapon) || not IsValid(AircraftMesh) || not IsValid(RTSComponent))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, TEXT("AircraftWeapon/AircraftMesh/RTSComponent"),
		                                             TEXT("UAircraftCannonPodsComponent::SetupCannons"));
		return;
	}
	const int32 OwningPlayer = RTSComponent->GetOwningPlayer();
	for (const FAircraftCannonPod& Pod : Pods)
	{
		SetupCannon(*AircraftWeapon, *AircraftMesh, Pod, OwningPlayer);
	}
}

void UAircraftCannonPodsComponent::SetupCannon(UAircraftWeapon& AircraftWeapon, USkeletalMeshComponent& AircraftMesh,
                                               const FAircraftCannonPod& Pod, const int32 OwningPlayer)
{
	if (not AircraftMesh.DoesSocketExist(Pod.MuzzleSocketName))
	{
		RTSFunctionLibrary::ReportError(FString::Printf(
			TEXT("UAircraftCannonPodsComponent on %s: muzzle socket %s is missing on the aircraft mesh."),
			*GetOwner()->GetName(), *Pod.MuzzleSocketName.ToString()));
		return;
	}
	FInitWeaponStateProjectile Parameters = CannonWeapon;
	// Every Setup call re-applies its owner to all of the aircraft's guns, so this also corrects the Blueprint's own.
	Parameters.OwningPlayer = OwningPlayer;
	Parameters.WeaponOwner = TScriptInterface<IWeaponOwner>(&AircraftWeapon);
	Parameters.MeshComponent = &AircraftMesh;
	Parameters.FireSocketName = Pod.MuzzleSocketName;
	const int32 WeaponCountBefore = AircraftWeapon.GetWeapons().Num();
	if (not CallSetupProjectileWeapon(AircraftWeapon, Parameters))
	{
		return;
	}
	const TArray<UWeaponState*> Weapons = AircraftWeapon.GetWeapons();
	if (Weapons.Num() <= WeaponCountBefore || not IsValid(Weapons.Last()))
	{
		RTSFunctionLibrary::ReportError(TEXT("UAircraftCannonPodsComponent: the aircraft weapon did not add a cannon."));
		return;
	}
	UWeaponState* Cannon = Weapons.Last();
	SelectFiredShell(*Cannon);
	ApplyCannonRange(*Cannon);
	M_Cannons.Add(Cannon);
	UE_LOG(LogRTSCannonPods, Log, TEXT("%s cannon %s on %s: shell %s range %.0f"), *GetOwner()->GetName(),
	       *Parameters.WeaponName.ToString(), *Pod.MuzzleSocketName.ToString(),
	       *UEnum::GetValueAsString(Cannon->GetRawWeaponData().ShellType), Cannon->GetRange());
}

bool UAircraftCannonPodsComponent::CallSetupProjectileWeapon(UAircraftWeapon& AircraftWeapon,
                                                             const FInitWeaponStateProjectile& Parameters) const
{
	// The setup functions are Blueprint-callable but protected in C++: call it the way the aircraft Blueprints do.
	static const FName SetupFunctionName(TEXT("SetupProjectileWeapon"));
	static const FName ParameterName(TEXT("ProjectileWeaponParameters"));
	UFunction* SetupFunction = AircraftWeapon.FindFunction(SetupFunctionName);
	const FStructProperty* ParameterProperty = IsValid(SetupFunction)
		                                           ? CastField<FStructProperty>(
			                                           SetupFunction->FindPropertyByName(ParameterName))
		                                           : nullptr;
	if (ParameterProperty == nullptr || ParameterProperty->Struct != FInitWeaponStateProjectile::StaticStruct())
	{
		RTSFunctionLibrary::ReportError(TEXT("UAircraftCannonPodsComponent: UAircraftWeapon::SetupProjectileWeapon "
			"or its parameter was not found."));
		return false;
	}
	uint8* ParameterBuffer = static_cast<uint8*>(FMemory_Alloca(SetupFunction->ParmsSize));
	FMemory::Memzero(ParameterBuffer, SetupFunction->ParmsSize);
	SetupFunction->InitializeStruct(ParameterBuffer);
	ParameterProperty->CopyCompleteValue(ParameterProperty->ContainerPtrToValuePtr<void>(ParameterBuffer), &Parameters);
	AircraftWeapon.ProcessEvent(SetupFunction, ParameterBuffer);
	SetupFunction->DestroyStruct(ParameterBuffer);
	return true;
}

void UAircraftCannonPodsComponent::SelectFiredShell(UWeaponState& Cannon) const
{
	if (not Cannon.GetRawWeaponData().ShellTypes.Contains(FiredShellType))
	{
		Cannon.UpgradeWeaponWithExtraShellType(FiredShellType);
	}
	Cannon.ChangeWeaponShellType(FiredShellType);
}

void UAircraftCannonPodsComponent::ApplyCannonRange(UWeaponState& Cannon) const
{
	const float DataRange = Cannon.GetRawWeaponData().Range;
	if (CannonRange <= 0.f || DataRange <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	Cannon.UpgradeWeaponWithRangeMlt(CannonRange / DataRange);
}
