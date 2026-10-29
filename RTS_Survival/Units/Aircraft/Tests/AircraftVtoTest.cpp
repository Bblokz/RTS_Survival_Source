// Copyright (C) Bas Blokzijl - All rights reserved.

// -AircraftVtoValidate: the vertical take-off and the guns of one parked aircraft (UAircraftTestSubsystem).

#include "AircraftTestSubsystem.h"
#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RTS_Survival/Units/Aircraft/AircraftAnimInstance/AircraftAnimInstance.h"
#include "RTS_Survival/Units/Aircraft/AircraftMaster/AAircraftMaster.h"
#include "RTS_Survival/Units/Aircraft/AircraftState/AircraftState.h"
#include "RTS_Survival/Weapons/WeaponData/MultiProjectileWeapon/UWeaponStateMultiProjectile.h"
#include "RTS_Survival/Weapons/WeaponData/WeaponData.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSAircraftVtoTest, Log, All);

namespace AircraftVtoTestPrivate
{
	const TCHAR* const AircraftRoot = TEXT("/Game/RTS_Survival/Blueprints/Aircraft/");
	const TCHAR* const DefaultClassPath = TEXT("GerAircraft/HO229/Ger_Aircraft_Horten229.Ger_Aircraft_Horten229_C");
	const TCHAR* const DefaultGearBone = TEXT("B_NoseWheel");
	const TCHAR* const HortenClassMarker = TEXT("Horten229");
	constexpr uint8 HumanPlayer = 1;

	constexpr float SettleSeconds = 2.f;
	// The Ho 229 climbs 18.5 m at 15 cm/s^2: about 16 s.
	constexpr float TakeOffTimeoutSeconds = 60.f;
	constexpr float MinClimbHeight = 1000.f;
	constexpr float FireTimeoutSeconds = 45.f;
	constexpr float GearSettleSeconds = 1.5f;
	// The attack-ground point, ahead of the parked aircraft.
	constexpr float AttackPointDistance = 4000.f;
	// Gear bones are shrunk to 1% in flight and are at full size on the ground.
	constexpr float GearDownMinScale = 0.9f;
	constexpr float GearUpMaxScale = 0.1f;
	constexpr int32 HortenGunCount = 2;

	USkeletalMeshComponent* GetAircraftMesh(const AAircraftMaster& Aircraft)
	{
		return Aircraft.FindComponentByClass<USkeletalMeshComponent>();
	}

	/** @return Largest component-space scale of the bone, or -1 when the mesh or bone is missing. */
	float GetBoneScale(const AAircraftMaster& Aircraft, const FName BoneName)
	{
		const USkeletalMeshComponent* Mesh = GetAircraftMesh(Aircraft);
		if (not IsValid(Mesh) || BoneName.IsNone() || Mesh->GetBoneIndex(BoneName) == INDEX_NONE)
		{
			return -1.f;
		}
		return Mesh->GetSocketTransform(BoneName, RTS_Component).GetScale3D().GetMax();
	}

	/** Engine bones (B_Rotor*) where the death component looks for a burning engine, in component space. */
	FString DescribeEngineBones(const AAircraftMaster& Aircraft)
	{
		const USkeletalMeshComponent* Mesh = GetAircraftMesh(Aircraft);
		if (not IsValid(Mesh))
		{
			return TEXT("no mesh");
		}
		FString Description;
		for (int32 BoneIndex = 0; BoneIndex < Mesh->GetNumBones(); ++BoneIndex)
		{
			const FName BoneName = Mesh->GetBoneName(BoneIndex);
			if (BoneName.ToString().StartsWith(TEXT("B_Rotor")))
			{
				Description += FString::Printf(TEXT("%s=%s "), *BoneName.ToString(),
				                               *Mesh->GetSocketTransform(BoneName, RTS_Component).GetLocation().
				                                      ToCompactString());
			}
		}
		return Description;
	}

	UWeaponState* GetFirstWeapon(const AAircraftMaster& Aircraft)
	{
		const TArray<UWeaponState*> Weapons = Aircraft.GetAllAircraftWeapons();
		return Weapons.Num() > 0 ? Weapons[0] : nullptr;
	}
}

void UAircraftTestSubsystem::StartAircraftVto()
{
	using namespace AircraftVtoTestPrivate;
	FAircraftVtoUnderTest& Entry = M_VtoAircraft;
	FString RelativeClassPath = DefaultClassPath;
	FParse::Value(FCommandLine::Get(), TEXT("AircraftVtoClass="), RelativeClassPath);
	Entry.ClassPath = FString(AircraftRoot) + RelativeClassPath;
	FString GearBone = DefaultGearBone;
	FParse::Value(FCommandLine::Get(), TEXT("AircraftVtoGearBone="), GearBone);
	Entry.GearBone = GearBone.Equals(TEXT("none"), ESearchCase::IgnoreCase) ? NAME_None : FName(*GearBone);

	Entry.Aircraft = Cast<AAircraftMaster>(SpawnUnitForPlayer(Entry.ClassPath, FTransform(M_Anchor), HumanPlayer));
	Check(Entry.Aircraft.IsValid(), TEXT("vto_spawned"), Entry.ClassPath);
	EnterPhase(EAircraftTestPhase::AircraftVtoRunning);
	if (not Entry.Aircraft.IsValid())
	{
		Finish();
		return;
	}
	Entry.StartZ = Entry.Aircraft->GetActorLocation().Z;
	EnterVtoStep(Entry, EAircraftVtoTestStep::Settling);
}

void UAircraftTestSubsystem::TickAircraftVto()
{
	FAircraftVtoUnderTest& Entry = M_VtoAircraft;
	if (not Entry.Aircraft.IsValid())
	{
		Check(false, TEXT("vto_aircraft_alive"), TEXT("destroyed during the test"));
		Finish();
		return;
	}
	switch (Entry.Step)
	{
	case EAircraftVtoTestStep::Settling:
		TickVtoSettling(Entry);
		return;
	case EAircraftVtoTestStep::TakingOff:
		TickVtoTakingOff(Entry);
		return;
	case EAircraftVtoTestStep::Firing:
		TickVtoFiring(Entry);
		return;
	case EAircraftVtoTestStep::Done:
		Finish();
		return;
	}
}

void UAircraftTestSubsystem::TickVtoSettling(FAircraftVtoUnderTest& Entry)
{
	using namespace AircraftVtoTestPrivate;
	if (GetNow() - Entry.StepStartSeconds < SettleSeconds)
	{
		return;
	}
	AAircraftMaster& Aircraft = *Entry.Aircraft;
	Check(Aircraft.GetLandedState() == EAircraftLandingState::Landed, TEXT("vto_starts_landed"),
	      FString::Printf(TEXT("state=%d"), static_cast<int32>(Aircraft.GetLandedState())));
	CheckVtoLoadout(Entry);
	const UWeaponState* Weapon = GetFirstWeapon(Aircraft);
	Entry.MagazineBeforeAttack = IsValid(Weapon) ? Weapon->GetCurrentMagCapacity() : 0;
	// An attack order to a parked aircraft takes off first, the way the player sends it out.
	const FVector AttackPoint = M_Anchor + Aircraft.GetActorForwardVector() * AttackPointDistance;
	const ECommandQueueError Error = Aircraft.AttackGround(AttackPoint, true);
	Check(Error == ECommandQueueError::NoError, TEXT("vto_attack_ground_order_accepted"),
	      FString::Printf(TEXT("error=%d"), static_cast<int32>(Error)));
	EnterVtoStep(Entry, EAircraftVtoTestStep::TakingOff);
}

void UAircraftTestSubsystem::CheckVtoLoadout(const FAircraftVtoUnderTest& Entry)
{
	using namespace AircraftVtoTestPrivate;
	const AAircraftMaster& Aircraft = *Entry.Aircraft;
	const USkeletalMeshComponent* Mesh = GetAircraftMesh(Aircraft);
	const UAircraftAnimInstance* AnimInstance = IsValid(Mesh)
		                                            ? Cast<UAircraftAnimInstance>(Mesh->GetAnimInstance())
		                                            : nullptr;
	Check(IsValid(AnimInstance), TEXT("vto_aircraft_anim_instance"),
	      IsValid(Mesh) && Mesh->GetAnimInstance() ? Mesh->GetAnimInstance()->GetClass()->GetName() : TEXT("none"));
	if (not Entry.GearBone.IsNone())
	{
		const float GearScale = GetBoneScale(Aircraft, Entry.GearBone);
		Check(GearScale >= GearDownMinScale, TEXT("vto_gear_down_when_parked"),
		      FString::Printf(TEXT("%s scale=%.3f"), *Entry.GearBone.ToString(), GearScale));
	}
	UE_LOG(LogRTSAircraftVtoTest, Display, TEXT("RTS_AIRCRAFT_TEST VTO_ENGINES parked %s"),
	       *DescribeEngineBones(Aircraft));
	const TArray<UWeaponState*> Weapons = Aircraft.GetAllAircraftWeapons();
	Check(Weapons.Num() > 0, TEXT("vto_has_weapons"), FString::Printf(TEXT("count=%d"), Weapons.Num()));
	for (const UWeaponState* Weapon : Weapons)
	{
		const FWeaponData& Data = Weapon->GetRawWeaponData();
		const UWeaponStateMultiProjectile* MultiProjectile = Cast<UWeaponStateMultiProjectile>(Weapon);
		UE_LOG(LogRTSAircraftVtoTest, Display,
		       TEXT("RTS_AIRCRAFT_TEST VTO_WEAPON %s name=%s mag=%d calibre=%.0f damage=%.1f cooldown=%.2f projectiles=%d"),
		       *Weapon->GetClass()->GetName(), *Global_GetWeaponEnumAsString(Data.WeaponName), Data.MagCapacity,
		       Data.WeaponCalibre, Data.BaseDamage, Data.BaseCooldown,
		       IsValid(MultiProjectile) ? MultiProjectile->GetProjectileCount() : 1);
	}
	if (not Entry.ClassPath.Contains(HortenClassMarker) || Weapons.Num() == 0)
	{
		return;
	}
	// The Ho 229: one weapon state, both MK 108s firing together from the wing-root muzzle bones.
	const UWeaponStateMultiProjectile* Cannons = Cast<UWeaponStateMultiProjectile>(Weapons[0]);
	Check(Weapons[0]->GetRawWeaponData().WeaponName == EWeaponName::MK108_30MM_Horten, TEXT("vto_horten_mk108"),
	      Global_GetWeaponEnumAsString(Weapons[0]->GetRawWeaponData().WeaponName));
	Check(IsValid(Cannons) && Cannons->GetProjectileCount() == HortenGunCount, TEXT("vto_horten_two_muzzles"),
	      FString::Printf(TEXT("projectiles=%d"), IsValid(Cannons) ? Cannons->GetProjectileCount() : 0));
}

void UAircraftTestSubsystem::TickVtoTakingOff(FAircraftVtoUnderTest& Entry)
{
	using namespace AircraftVtoTestPrivate;
	AAircraftMaster& Aircraft = *Entry.Aircraft;
	const EAircraftLandingState State = Aircraft.GetLandedState();
	if (State == EAircraftLandingState::VerticalTakeOff)
	{
		Entry.bSawVerticalTakeOff = true;
		const USkeletalMeshComponent* Mesh = GetAircraftMesh(Aircraft);
		const UAnimInstance* AnimInstance = IsValid(Mesh) ? Mesh->GetAnimInstance() : nullptr;
		Entry.bSawTakeOffMontage |= IsValid(AnimInstance) && AnimInstance->IsAnyMontagePlaying();
	}
	const float Waited = GetNow() - Entry.StepStartSeconds;
	if (State != EAircraftLandingState::Airborne)
	{
		if (Waited < TakeOffTimeoutSeconds)
		{
			return;
		}
		Check(false, TEXT("vto_airborne"), FString::Printf(TEXT("state=%d after=%.1fs"), static_cast<int32>(State),
		                                                   Waited));
		EnterVtoStep(Entry, EAircraftVtoTestStep::Done);
		return;
	}
	const float Climb = Aircraft.GetActorLocation().Z - Entry.StartZ;
	Check(true, TEXT("vto_airborne"), FString::Printf(TEXT("after=%.1fs"), Waited));
	Check(Entry.bSawVerticalTakeOff, TEXT("vto_vertical_take_off_state"));
	Check(Entry.bSawTakeOffMontage, TEXT("vto_take_off_montage_played"));
	Check(Climb >= MinClimbHeight, TEXT("vto_climbed"), FString::Printf(TEXT("climb=%.0f"), Climb));
	EnterVtoStep(Entry, EAircraftVtoTestStep::Firing);
}

void UAircraftTestSubsystem::TickVtoFiring(FAircraftVtoUnderTest& Entry)
{
	using namespace AircraftVtoTestPrivate;
	const AAircraftMaster& Aircraft = *Entry.Aircraft;
	const UWeaponState* Weapon = GetFirstWeapon(Aircraft);
	const int32 Magazine = IsValid(Weapon) ? Weapon->GetCurrentMagCapacity() : Entry.MagazineBeforeAttack;
	const float Waited = GetNow() - Entry.StepStartSeconds;
	const bool bFired = Magazine < Entry.MagazineBeforeAttack;
	// The take-off montage blends out when the aircraft is airborne; the gear is only folded once it has.
	const bool bGearSettled = Waited >= GearSettleSeconds;
	if ((not bFired || not bGearSettled) && Waited < FireTimeoutSeconds)
	{
		return;
	}
	Check(bFired, TEXT("vto_fired_at_ground"), FString::Printf(TEXT("mag %d -> %d after=%.1fs"),
	                                                         Entry.MagazineBeforeAttack, Magazine, Waited));
	UE_LOG(LogRTSAircraftVtoTest, Display, TEXT("RTS_AIRCRAFT_TEST VTO_ENGINES flying %s"),
	       *DescribeEngineBones(Aircraft));
	if (not Entry.GearBone.IsNone())
	{
		// By now the take-off montage has blended out and the flying sequence holds the gear in the wing.
		const float GearScale = GetBoneScale(Aircraft, Entry.GearBone);
		Check(GearScale >= 0.f && GearScale <= GearUpMaxScale, TEXT("vto_gear_up_in_flight"),
		      FString::Printf(TEXT("%s scale=%.3f"), *Entry.GearBone.ToString(), GearScale));
	}
	EnterVtoStep(Entry, EAircraftVtoTestStep::Done);
}

void UAircraftTestSubsystem::EnterVtoStep(FAircraftVtoUnderTest& Entry, const EAircraftVtoTestStep NewStep)
{
	Entry.Step = NewStep;
	Entry.StepStartSeconds = GetNow();
	UE_LOG(LogRTSAircraftVtoTest, Display, TEXT("RTS_AIRCRAFT_TEST VTO_STEP %d at=%.1f"), static_cast<int32>(NewStep),
	       Entry.StepStartSeconds);
}

#endif
