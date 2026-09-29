// Copyright (C) Bas Blokzijl - All rights reserved.

#include "ArmorCalculation.h"

#include "Components/MeshComponent.h"
#include "Engine/World.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

using namespace VehicleModuleBalance;

/** @brief Stack value combining the six explicit impact arguments with the source rule and hit context. */
struct FModuleDamageInput
{
	EArmorPlate PlateHit = EArmorPlate::Plate_Front;
	int32 RuleRow = INDEX_NONE;
	float EffectiveArmor = 0.f;
	bool bPen = false;
	float DamageDealt = 0.f;
	float ProjectileBaseDamage = 0.f;
	float ProjectileCalibre = 0.f;
	FVehicleModuleHitContext HitContext;
	FShellModuleRule SourceRule;
	uint32 ShotKey = 0;
};

namespace ArmorCalculationModuleDamageHelpers
{
	// Converts a 24-bit hash fraction into [0, 1).
	constexpr float RollFractionScale = 1.f / 16777216.f;
	constexpr uint32 RollFractionShift = 8;

	uint32 FinalizeHash(uint32 Hash)
	{
		Hash ^= Hash >> 16;
		Hash *= 0x85ebca6bu;
		Hash ^= Hash >> 13;
		Hash *= 0xc2b2ae35u;
		Hash ^= Hash >> 16;
		return Hash;
	}

	FModuleDamageInput MakeModuleDamageInput(
		const EArmorPlate PlateHit,
		const float EffectiveArmor,
		const bool bPen,
		const float DamageDealt,
		const float ProjectileBaseDamage,
		const float ProjectileCalibre,
		const FVehicleModuleHitContext& HitContext)
	{
		FModuleDamageInput Input;
		Input.PlateHit = PlateHit;
		Input.RuleRow = TryGetPlateRuleIndex(PlateHit);
		Input.EffectiveArmor = EffectiveArmor;
		Input.bPen = bPen;
		Input.DamageDealt = DamageDealt;
		Input.ProjectileBaseDamage = ProjectileBaseDamage;
		Input.ProjectileCalibre = ProjectileCalibre;
		Input.HitContext = HitContext;
		Input.SourceRule = GetShellModuleRule(HitContext.ShellType);
		return Input;
	}
}

// ----------------------------------------------------------------------------------------------------
// Public entry points
// ----------------------------------------------------------------------------------------------------

void UArmorCalculation::CalculateModuleDamage(
	const EArmorPlate PlateHit,
	const float EffectiveArmor,
	const bool bPen,
	const float DamageDealt,
	const float ProjectileBaseDamage,
	const float ProjectileCalibre)
{
	if (not bM_AreModulesFinalized || M_InstalledModuleCount <= 0)
	{
		return;
	}
	FVehicleModuleHitContext HitContext;
	if (not TryMakeDefaultModuleHitContext(PlateHit, HitContext))
	{
		return;
	}
	CalculateModuleDamage(PlateHit, EffectiveArmor, bPen, DamageDealt, ProjectileBaseDamage, ProjectileCalibre,
	                      HitContext);
}

void UArmorCalculation::CalculateModuleDamage(
	const EArmorPlate PlateHit,
	const float EffectiveArmor,
	const bool bPen,
	const float DamageDealt,
	const float ProjectileBaseDamage,
	const float ProjectileCalibre,
	const FVehicleModuleHitContext& HitContext)
{
	if (not bM_AreModulesFinalized || M_InstalledModuleCount <= 0)
	{
		return;
	}

	FModuleDamageInput Input = ArmorCalculationModuleDamageHelpers::MakeModuleDamageInput(
		PlateHit, EffectiveArmor, bPen, DamageDealt, ProjectileBaseDamage, ProjectileCalibre, HitContext);
	Input.ShotKey = MakeShotKey(HitContext);
	if (not ValidateImpactInputsAndLivingOwner(Input))
	{
		return;
	}

	const FModuleChangeBatch Changes = ResolveModuleDamage(Input);
	// Behaviours, Blueprint and UI work may allocate; it runs after the fixed-storage resolver finished.
	DispatchModuleChangesAfterMutation(Changes);
}

void UArmorCalculation::DamageModule(const int32 ModuleId, const float Damage)
{
	const int32 SlotIndex = GetModuleSlotById(ModuleId);
	if (SlotIndex == INDEX_NONE || not bM_AreModulesFinalized || not FMath::IsFinite(Damage) || Damage <= 0.f)
	{
		return;
	}
	if (bM_IsMutatingModules || not GetIsValidModuleTank() || not M_ModuleTank->IsUnitAlive())
	{
		return;
	}

	{
		TGuardValue<bool> MutationGuard(bM_IsMutatingModules, true);
		SetModuleHealth(SlotIndex, M_Modules[SlotIndex].CurrentHp - Damage, EModuleChangeCause::Damage);
		++M_ModuleDamageRevision;
	}
	DispatchModuleChangesAfterMutation(TakePendingModuleChanges());
}

void UArmorCalculation::DamageModuleOfType(const EVehicleModuleTypes Type, const float Damage)
{
	int32 MatchingSlot = INDEX_NONE;
	int32 MatchingCount = 0;
	for (int32 SlotIndex = 0; SlotIndex < MaxModuleInstances; ++SlotIndex)
	{
		if (M_Modules[SlotIndex].bInstalled && M_Modules[SlotIndex].Type == Type)
		{
			MatchingSlot = SlotIndex;
			++MatchingCount;
		}
	}
	if (MatchingCount != 1)
	{
		RTSFunctionLibrary::ReportError(TEXT("DamageModuleOfType: requires exactly one installed instance of ")
			+ UEnum::GetValueAsString(Type) + TEXT(" on ") + GetNameSafe(GetOwner()));
		return;
	}
	DamageModule(M_Modules[MatchingSlot].ModuleId, Damage);
}

FVehicleModuleHitContext UArmorCalculation::MakeHitContextForWorldHit(const FVector& WorldHitLocation) const
{
	FVehicleModuleHitContext HitContext;
	HitContext.RuleVersion = RuleVersion;
	const AActor* Owner = GetOwner();
	if (not IsValid(Owner) || WorldHitLocation.ContainsNaN())
	{
		return HitContext;
	}
	HitContext.VictimId = Owner->GetUniqueID();
	HitContext.HullLocalHitPosition = Owner->GetActorTransform().InverseTransformPosition(WorldHitLocation);
	HitContext.bHasHullLocalHitPosition = true;
	return HitContext;
}

// ----------------------------------------------------------------------------------------------------
// Resolver
// ----------------------------------------------------------------------------------------------------

bool UArmorCalculation::ValidateImpactInputsAndLivingOwner(const FModuleDamageInput& Input) const
{
	if (bM_IsMutatingModules || not GetIsValidModuleTank() || not M_ModuleTank->IsUnitAlive())
	{
		return false;
	}
	if (Input.RuleRow == INDEX_NONE || M_SelectedProfilePlateRules == nullptr)
	{
		return false;
	}
	const int32 PlateRegistrationId = Input.HitContext.StablePlateRegistrationId;
	if (PlateRegistrationId < 0 || PlateRegistrationId >= MaxPlateBindings
		|| M_PlateRuleRows[PlateRegistrationId] != Input.RuleRow)
	{
		return false;
	}
	const bool bFiniteInputs = FMath::IsFinite(Input.EffectiveArmor) && FMath::IsFinite(Input.DamageDealt)
		&& FMath::IsFinite(Input.ProjectileBaseDamage) && FMath::IsFinite(Input.ProjectileCalibre);
	if (not bFiniteInputs || Input.ProjectileBaseDamage <= 0.f || Input.ProjectileCalibre < 0.f
		|| Input.DamageDealt < 0.f)
	{
		return false;
	}
	// Rejected hull damage (immunity, full reduction) never damages modules on a penetrating hit.
	if (Input.bPen && Input.DamageDealt <= 0.f)
	{
		return false;
	}
	// Unsupported source profiles fail closed.
	return Input.SourceRule.bIsSupported && Input.HitContext.DamageType == ERTSDamageType::Kinetic;
}

FModuleChangeBatch UArmorCalculation::ResolveModuleDamage(const FModuleDamageInput& Input)
{
	TGuardValue<bool> MutationGuard(bM_IsMutatingModules, true);
	const FPlateModuleRuleSet& Rules = M_SelectedProfilePlateRules[Input.RuleRow];
	FModuleDamageBatch Batch;
	const float Energy = CalculateModuleDamageEnergy(Input);

	for (int32 CandidateIndex = 0; CandidateIndex < MaxModulesPerPlate; ++CandidateIndex)
	{
		TryAppendCandidateDamage(Rules.Entries[CandidateIndex], CandidateIndex, Energy, Input, Batch);
	}

	LimitBatchToEnergyBudget(Energy, Batch);
	ApplyPerModuleDamageCaps(Input.bPen, Batch);
	ApplyNonPenRateBudgetIfNeeded(Input, Batch);
	LimitNewDestroyedModules(Batch);
	return CommitModuleDamageBatch(Batch);
}

float UArmorCalculation::CalculateModuleDamageEnergy(const FModuleDamageInput& Input) const
{
	const float BaseDamage = Input.ProjectileBaseDamage;
	if (not Input.bPen)
	{
		return BaseDamage * Input.SourceRule.NonPenEnergyMultiplier;
	}
	// EffectiveArmor stays a diagnostic value; module HP is never divided by armor millimetres.
	const float FlooredDamage = FMath::Max(Input.DamageDealt, BaseDamage * PenDamageFloorFromBase);
	const float PenEnergy = FMath::Min(BaseDamage * PenDamageCeilingFromBase, FlooredDamage)
		* Input.SourceRule.PenEnergyMultiplier;
	return Input.HitContext.bOverpenetrating ? PenEnergy * OverpenetrationEnergyMultiplier : PenEnergy;
}

void UArmorCalculation::TryAppendCandidateDamage(const FPlateModuleDamage& Rule, const int32 CandidateIndex,
                                                 const float Energy, const FModuleDamageInput& Input,
                                                 FModuleDamageBatch& Batch) const
{
	if (Rule.TypeToDamage == EVehicleModuleTypes::None)
	{
		return;
	}
	const FModulePlateRoute& Route = M_PlateRoutes[Input.HitContext.StablePlateRegistrationId][CandidateIndex];
	const int32 SlotIndex = ResolveModuleSlot(Route, Input);
	// An absent instance is skipped without reallocating its probability or damage.
	if (SlotIndex == INDEX_NONE || not GetIsCandidateEligibleForSource(Rule, Input))
	{
		return;
	}

	// The roll depends only on the shot key and candidate index, so skipped candidates never shift others.
	const float Probability = CalculateCandidateProbability(Rule, Input);
	const float Roll01 = GetDeterministicModuleRoll(Input.ShotKey, CandidateIndex);
	if (Roll01 < Probability)
	{
		Batch.Append(SlotIndex, CandidateIndex, Energy * Rule.DamageMultiplier);
	}
}

int32 UArmorCalculation::ResolveModuleSlot(const FModulePlateRoute& Route, const FModuleDamageInput& Input) const
{
	if (not Route.bUsesHitSide)
	{
		return Route.PrimarySlot;
	}
	// Never fabricate a side: without a hull-local hit position the running gear candidate is skipped.
	if (not Input.HitContext.bHasHullLocalHitPosition)
	{
		return INDEX_NONE;
	}
	// Stable centreline tie-break: exactly on the centreline counts as the left side.
	const bool bIsRightSide = Input.HitContext.HullLocalHitPosition.Y > 0.f;
	return bIsRightSide ? Route.AlternateSlot : Route.PrimarySlot;
}

bool UArmorCalculation::GetIsCandidateEligibleForSource(const FPlateModuleDamage& Rule,
                                                        const FModuleDamageInput& Input) const
{
	const bool bIsBallistic = Input.HitContext.DeliveryType == EVehicleModuleDelivery::Ballistic;
	if (bIsBallistic && Input.ProjectileCalibre < GetMinBallisticCalibreMm(Rule.TypeToDamage))
	{
		return false;
	}
	if (Input.bPen)
	{
		return true;
	}
	switch (Rule.NonPenPolicy)
	{
	case EModuleNonPenPolicy::External:
		return true;
	case EModuleNonPenPolicy::MantletOnly:
		// Splash reaches external modules only.
		return bIsBallistic && Input.PlateHit == EArmorPlate::Turret_Mantlet;
	default:
		return false;
	}
}

float UArmorCalculation::CalculateCandidateProbability(const FPlateModuleDamage& Rule,
                                                       const FModuleDamageInput& Input) const
{
	// Rule.DamageProbability already includes the class chance multiplier; it is not applied again.
	const float SourceMultiplier = Input.bPen ? 1.f : Input.SourceRule.NonPenProbabilityMultiplier;
	return FMath::Clamp(Rule.DamageProbability * SourceMultiplier, 0.f, 1.f);
}

float UArmorCalculation::GetDeterministicModuleRoll(const uint32 ShotKey, const int32 CandidateIndex)
{
	const uint32 CandidateHash = HashCombineFast(ShotKey, GetTypeHash(CandidateIndex + 1));
	const uint32 FinalHash = ArmorCalculationModuleDamageHelpers::FinalizeHash(CandidateHash);
	return static_cast<float>(FinalHash >> ArmorCalculationModuleDamageHelpers::RollFractionShift)
		* ArmorCalculationModuleDamageHelpers::RollFractionScale;
}

uint32 UArmorCalculation::MakeShotKey(const FVehicleModuleHitContext& HitContext)
{
	uint32 ShotKey = HashCombineFast(GetTypeHash(HitContext.ShotActivationId), GetTypeHash(HitContext.ImpactOrdinal));
	ShotKey = HashCombineFast(ShotKey, GetTypeHash(HitContext.VictimId));
	return HashCombineFast(ShotKey, GetTypeHash(HitContext.RuleVersion));
}

void UArmorCalculation::LimitBatchToEnergyBudget(const float Energy, FModuleDamageBatch& Batch)
{
	float SumOfProposedDamage = 0.f;
	for (int32 EntryIndex = 0; EntryIndex < Batch.Count; ++EntryIndex)
	{
		SumOfProposedDamage += Batch.Entries[EntryIndex].Damage;
	}
	if (SumOfProposedDamage <= Energy || SumOfProposedDamage <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	const float BudgetScale = FMath::Max(0.f, Energy) / SumOfProposedDamage;
	for (int32 EntryIndex = 0; EntryIndex < Batch.Count; ++EntryIndex)
	{
		Batch.Entries[EntryIndex].Damage *= BudgetScale;
	}
}

void UArmorCalculation::ApplyPerModuleDamageCaps(const bool bPen, FModuleDamageBatch& Batch) const
{
	const float Cap01 = bPen ? PenetratingModuleDamageCap01 : NonPenModuleDamageCap01;
	for (int32 EntryIndex = 0; EntryIndex < Batch.Count; ++EntryIndex)
	{
		FModuleDamageBatch::FEntry& Entry = Batch.Entries[EntryIndex];
		Entry.Damage = FMath::Min(Entry.Damage, M_Modules[Entry.SlotIndex].MaxHp * Cap01);
	}
}

void UArmorCalculation::ApplyNonPenRateBudgetIfNeeded(const FModuleDamageInput& Input, FModuleDamageBatch& Batch)
{
	if (Input.bPen)
	{
		return;
	}
	for (int32 EntryIndex = 0; EntryIndex < Batch.Count; ++EntryIndex)
	{
		FModuleDamageBatch::FEntry& Entry = Batch.Entries[EntryIndex];
		FVehicleModule& Module = M_Modules[Entry.SlotIndex];
		Entry.Damage = ConsumeNonPenBudget(Module.NonPenBudget, Module.MaxHp, Entry.Damage);
	}
}

float UArmorCalculation::ConsumeNonPenBudget(FVehicleModuleNonPenBudget& Budget, const float MaxHp,
                                             const float RequestedDamage) const
{
	const UWorld* World = GetWorld();
	const double NowSeconds = IsValid(World) ? World->GetTimeSeconds() : 0.0;
	const int32 CurrentEpoch = FMath::FloorToInt32(NowSeconds / NonPenBucketSeconds);
	const int32 OldestRetainedEpoch = CurrentEpoch - (NonPenWindowBucketCount - 1);

	float RecentDamage = 0.f;
	for (int32 BucketIndex = 0; BucketIndex < NonPenWindowBucketCount; ++BucketIndex)
	{
		const int32 BucketEpoch = Budget.BucketEpoch[BucketIndex];
		if (BucketEpoch >= OldestRetainedEpoch && BucketEpoch <= CurrentEpoch)
		{
			RecentDamage += Budget.BucketDamage[BucketIndex];
		}
	}

	const float AllowedDamage = FMath::Clamp(MaxHp * NonPenWindowDamageCap01 - RecentDamage, 0.f, RequestedDamage);
	const int32 CurrentBucket = ((CurrentEpoch % NonPenWindowBucketCount) + NonPenWindowBucketCount)
		% NonPenWindowBucketCount;
	if (Budget.BucketEpoch[CurrentBucket] != CurrentEpoch)
	{
		Budget.BucketEpoch[CurrentBucket] = CurrentEpoch;
		Budget.BucketDamage[CurrentBucket] = 0.f;
	}
	Budget.BucketDamage[CurrentBucket] += AllowedDamage;
	return AllowedDamage;
}

bool UArmorCalculation::GetWouldEntryDestroyModule(const FModuleDamageBatch::FEntry& Entry) const
{
	const FVehicleModule& Module = M_Modules[Entry.SlotIndex];
	// AddOnArmor destruction does not consume the per-impact allowance.
	if (Module.Type == EVehicleModuleTypes::AddOnArmor || Module.State == EVehicleModuleState::Destroyed
		|| Module.MaxHp <= 0.f || Entry.Damage <= 0.f)
	{
		return false;
	}
	const float ProposedHealth01 = FMath::Max(0.f, Module.CurrentHp - Entry.Damage) / Module.MaxHp;
	return ProposedHealth01 <= GetDestroyedHealthThreshold01(Module.Type);
}

int32 UArmorCalculation::FindGreatestNormalizedFailure(const FModuleDamageBatch& Batch,
                                                       const bool (&bAllowedToDestroy)[MaxModulesPerPlate]) const
{
	// Greatest normalized loss of the HP remaining above the threshold wins; table order breaks ties.
	int32 BestEntry = INDEX_NONE;
	float BestNormalizedLoss = -1.f;
	for (int32 EntryIndex = 0; EntryIndex < Batch.Count; ++EntryIndex)
	{
		const FModuleDamageBatch::FEntry& Entry = Batch.Entries[EntryIndex];
		if (bAllowedToDestroy[EntryIndex] || not GetWouldEntryDestroyModule(Entry))
		{
			continue;
		}
		const FVehicleModule& Module = M_Modules[Entry.SlotIndex];
		const float ThresholdHp = Module.MaxHp * GetDestroyedHealthThreshold01(Module.Type);
		const float RemainingAboveThreshold = FMath::Max(KINDA_SMALL_NUMBER, Module.CurrentHp - ThresholdHp);
		const float NormalizedLoss = Entry.Damage / RemainingAboveThreshold;
		if (NormalizedLoss > BestNormalizedLoss)
		{
			BestNormalizedLoss = NormalizedLoss;
			BestEntry = EntryIndex;
		}
	}
	return BestEntry;
}

void UArmorCalculation::LimitNewDestroyedModules(FModuleDamageBatch& Batch) const
{
	bool bAllowedToDestroy[MaxModulesPerPlate] = {};
	for (int32 AllowanceIndex = 0; AllowanceIndex < MaxNewDestroyedModulesPerImpact; ++AllowanceIndex)
	{
		const int32 BestEntry = FindGreatestNormalizedFailure(Batch, bAllowedToDestroy);
		if (BestEntry == INDEX_NONE)
		{
			break;
		}
		bAllowedToDestroy[BestEntry] = true;
	}

	for (int32 EntryIndex = 0; EntryIndex < Batch.Count; ++EntryIndex)
	{
		FModuleDamageBatch::FEntry& Entry = Batch.Entries[EntryIndex];
		if (bAllowedToDestroy[EntryIndex] || not GetWouldEntryDestroyModule(Entry))
		{
			continue;
		}
		// Clamp the would-be failure just above its threshold; the clamp can never heal. Prevented damage is discarded.
		const FVehicleModule& Module = M_Modules[Entry.SlotIndex];
		const float SurvivingHp = FMath::Min(Module.CurrentHp, Module.MaxHp
		                                     * (GetDestroyedHealthThreshold01(Module.Type)
			                                     + SurvivingModuleThresholdMargin01));
		Entry.Damage = FMath::Max(0.f, Module.CurrentHp - SurvivingHp);
	}
}

FModuleChangeBatch UArmorCalculation::CommitModuleDamageBatch(const FModuleDamageBatch& Batch)
{
	bool bAppliedAnyDamage = false;
	for (int32 EntryIndex = 0; EntryIndex < Batch.Count; ++EntryIndex)
	{
		const FModuleDamageBatch::FEntry& Entry = Batch.Entries[EntryIndex];
		if (Entry.Damage <= 0.f)
		{
			continue;
		}
		SetModuleHealth(Entry.SlotIndex, M_Modules[Entry.SlotIndex].CurrentHp - Entry.Damage,
		                EModuleChangeCause::Damage);
		bAppliedAnyDamage = true;
	}
	if (bAppliedAnyDamage)
	{
		++M_ModuleDamageRevision;
	}
	return TakePendingModuleChanges();
}

// ----------------------------------------------------------------------------------------------------
// Default-source context
// ----------------------------------------------------------------------------------------------------

bool UArmorCalculation::TryMakeDefaultModuleHitContext(const EArmorPlate PlateHit,
                                                       FVehicleModuleHitContext& OutHitContext)
{
	const int32 PlateRegistrationId = FindUniquePlateRegistration(PlateHit);
	if (PlateRegistrationId == INDEX_NONE)
	{
		RTSFunctionLibrary::ReportError(TEXT("CalculateModuleDamage: plate ") + UEnum::GetValueAsString(PlateHit)
			+ TEXT(" is missing or registered on several meshes; supply FVehicleModuleHitContext. Owner: ")
			+ GetNameSafe(GetOwner()));
		return false;
	}
	for (const FModulePlateRoute& Route : M_PlateRoutes[PlateRegistrationId])
	{
		const bool bNeedsSide = Route.bUsesHitSide
			&& (Route.PrimarySlot != INDEX_NONE || Route.AlternateSlot != INDEX_NONE);
		if (bNeedsSide)
		{
			RTSFunctionLibrary::ReportError(TEXT("CalculateModuleDamage: plate ") + UEnum::GetValueAsString(PlateHit)
				+ TEXT(" needs the struck side; supply FVehicleModuleHitContext. Owner: ") + GetNameSafe(GetOwner()));
			return false;
		}
	}

	OutHitContext = FVehicleModuleHitContext();
	OutHitContext.StablePlateRegistrationId = PlateRegistrationId;
	OutHitContext.ShellType = EWeaponShellType::Shell_AP;
	OutHitContext.DamageType = ERTSDamageType::Kinetic;
	OutHitContext.DeliveryType = EVehicleModuleDelivery::Ballistic;
	OutHitContext.ShotActivationId = ++M_DefaultImpactSerial;
	OutHitContext.VictimId = IsValid(GetOwner()) ? GetOwner()->GetUniqueID() : 0;
	OutHitContext.RuleVersion = RuleVersion;
	return true;
}

int32 UArmorCalculation::FindUniquePlateRegistration(const EArmorPlate PlateHit) const
{
	uint64 MatchingPlates = 0;
	for (int32 ArmorMeshSlot = 0; ArmorMeshSlot < MaxRegisteredArmorMeshes; ++ArmorMeshSlot)
	{
		MatchingPlates |= GetCoverageMaskForPlateType(ArmorMeshSlot, PlateHit);
	}
	// Exactly one registered box of this type makes the binding unambiguous.
	if (FMath::CountBits(MatchingPlates) != 1)
	{
		return INDEX_NONE;
	}
	return static_cast<int32>(FMath::CountTrailingZeros64(MatchingPlates));
}

// ----------------------------------------------------------------------------------------------------
// Mine and splash adapters
// ----------------------------------------------------------------------------------------------------

void UArmorCalculation::CalculateMineModuleDamage(const FVector& ExplosionLocation, const float BaseDamage,
                                                  const FVehicleModuleHitContext& HitContext)
{
	if (not bM_AreModulesFinalized || bM_IsMutatingModules || not FMath::IsFinite(BaseDamage) || BaseDamage <= 0.f
		|| ExplosionLocation.ContainsNaN())
	{
		return;
	}
	if (not GetIsValidModuleTank() || not M_ModuleTank->IsUnitAlive())
	{
		return;
	}
	const int32 SlotIndex = FindRunningGearSlotNearest(ExplosionLocation);
	if (SlotIndex == INDEX_NONE)
	{
		return;
	}

	// Base probability 1 scaled once by the resolved running gear's class chance factor.
	const FVehicleModule& Module = M_Modules[SlotIndex];
	const float Probability = FMath::Clamp(
		MineModuleRule.DamageProbability * GetDamageChanceMultiplier(M_ModuleProfile, Module.Type), 0.f, 1.f);
	constexpr int32 MineCandidateIndex = 0;
	if (GetDeterministicModuleRoll(MakeShotKey(HitContext), MineCandidateIndex) >= Probability)
	{
		return;
	}

	FModuleChangeBatch Changes;
	{
		TGuardValue<bool> MutationGuard(bM_IsMutatingModules, true);
		FModuleDamageBatch Batch;
		Batch.Append(SlotIndex, MineCandidateIndex, BaseDamage * MineModuleRule.DamageMultiplier);
		ApplyPerModuleDamageCaps(true, Batch);
		LimitNewDestroyedModules(Batch);
		Changes = CommitModuleDamageBatch(Batch);
	}
	DispatchModuleChangesAfterMutation(Changes);
}

void UArmorCalculation::CalculateSplashModuleDamage(const FVector& ExplosionLocation, const float AttenuatedDamage,
                                                    const FVehicleModuleHitContext& HitContext)
{
	if (not bM_AreModulesFinalized || M_InstalledModuleCount <= 0 || ExplosionLocation.ContainsNaN())
	{
		return;
	}
	const int32 PlateRegistrationId = FindPlateRegistrationFacingLocation(ExplosionLocation);
	if (PlateRegistrationId == INDEX_NONE)
	{
		return;
	}
	const int32 ArmorMeshSlot = PlateRegistrationId / MaxArmorPlatesPerRegisteredMesh;
	const int32 PlateIndex = PlateRegistrationId % MaxArmorPlatesPerRegisteredMesh;
	UMeshComponent* ArmorMesh = GetArmorMeshForSlot(ArmorMeshSlot);
	const FArmorSettings* ArmorSettings = GetArmorSettingsForSlot(ArmorMeshSlot);
	if (not IsValid(ArmorMesh) || ArmorSettings == nullptr)
	{
		return;
	}
	FVector ContactLocation = ArmorMesh->Bounds.Origin;
	ArmorMesh->GetClosestPointOnCollision(ExplosionLocation, ContactLocation);
	if (GetIsSplashOccluded(ExplosionLocation, ContactLocation))
	{
		return;
	}

	FVehicleModuleHitContext SplashContext = MakeHitContextForWorldHit(ContactLocation);
	SplashContext.StablePlateRegistrationId = PlateRegistrationId;
	SplashContext.DeliveryType = EVehicleModuleDelivery::Splash;
	SplashContext.DamageType = ERTSDamageType::Kinetic;
	SplashContext.ShotActivationId = HitContext.ShotActivationId;
	SplashContext.ImpactOrdinal = HitContext.ImpactOrdinal;

	const EArmorPlate PlateHit = ArmorSettings[PlateIndex].ArmorType;
	FModuleDamageInput Input = ArmorCalculationModuleDamageHelpers::MakeModuleDamageInput(
		PlateHit, 0.f, false, 0.f, AttenuatedDamage, 0.f, SplashContext);
	// Splash has its own source rule: external modules only, attenuated energy and probability.
	Input.SourceRule.bIsSupported = true;
	Input.SourceRule.PenEnergyMultiplier = 0.f;
	Input.SourceRule.NonPenProbabilityMultiplier = SplashModuleRules.ProbabilityMultiplier;
	Input.SourceRule.NonPenEnergyMultiplier = SplashModuleRules.EnergyMultiplier;
	Input.ShotKey = MakeShotKey(SplashContext);
	if (not ValidateImpactInputsAndLivingOwner(Input))
	{
		return;
	}
	DispatchModuleChangesAfterMutation(ResolveModuleDamage(Input));
}

int32 UArmorCalculation::FindRunningGearSlotNearest(const FVector& WorldLocation) const
{
	const AActor* Owner = GetOwner();
	if (not IsValid(Owner))
	{
		return INDEX_NONE;
	}
	const FVector HullLocalLocation = Owner->GetActorTransform().InverseTransformPosition(WorldLocation);
	const int32 SideSlot = HullLocalLocation.Y > 0.f ? RightRunningGearSlot : LeftRunningGearSlot;
	return M_Modules[SideSlot].bInstalled ? SideSlot : INDEX_NONE;
}

int32 UArmorCalculation::FindPlateRegistrationFacingLocation(const FVector& WorldLocation) const
{
	// The registered mesh whose collision is closest to the explosion receives the splash.
	int32 ClosestMeshSlot = INDEX_NONE;
	float ClosestDistanceSquared = TNumericLimits<float>::Max();
	FVector ClosestContact = FVector::ZeroVector;
	for (int32 ArmorMeshSlot = 0; ArmorMeshSlot < MaxRegisteredArmorMeshes; ++ArmorMeshSlot)
	{
		UMeshComponent* ArmorMesh = GetArmorMeshForSlot(ArmorMeshSlot);
		if (not IsValid(ArmorMesh) || GetRegisteredPlateCountForSlot(ArmorMeshSlot) <= 0)
		{
			continue;
		}
		FVector Contact = ArmorMesh->Bounds.Origin;
		ArmorMesh->GetClosestPointOnCollision(WorldLocation, Contact);
		const float DistanceSquared = FVector::DistSquared(Contact, WorldLocation);
		if (DistanceSquared < ClosestDistanceSquared)
		{
			ClosestDistanceSquared = DistanceSquared;
			ClosestMeshSlot = ArmorMeshSlot;
			ClosestContact = Contact;
		}
	}
	if (ClosestMeshSlot == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	const FVector ProbeDirection = (ClosestContact - WorldLocation).GetSafeNormal();
	float RawArmorValue = 0.f;
	float UnusedPenetration = 0.f;
	EArmorPlate PlateHit = EArmorPlate::Plate_Front;
	int32 PlateIndex = INDEX_NONE;
	const UMeshComponent* ClosestMesh = GetArmorMeshForSlot(ClosestMeshSlot);
	(void)EvaluateArmorPlatesForHit(ClosestMeshSlot, ClosestMesh->GetComponentTransform(), ClosestContact,
	                                ProbeDirection, -ProbeDirection, RawArmorValue, UnusedPenetration, PlateHit,
	                                PlateIndex);
	return PlateIndex == INDEX_NONE ? INDEX_NONE : MakePlateRegistrationId(ClosestMeshSlot, PlateIndex);
}

bool UArmorCalculation::GetIsSplashOccluded(const FVector& ExplosionLocation, const FVector& TargetLocation) const
{
	UWorld* World = GetWorld();
	const AActor* Owner = GetOwner();
	if (not IsValid(World) || not IsValid(Owner))
	{
		return true;
	}
	FHitResult OcclusionHit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VehicleModuleSplashOcclusion), false);
	if (not World->LineTraceSingleByChannel(OcclusionHit, ExplosionLocation, TargetLocation, ECC_Visibility,
	                                        QueryParams))
	{
		return false;
	}
	const AActor* BlockingActor = OcclusionHit.GetActor();
	if (BlockingActor == nullptr)
	{
		return true;
	}
	return BlockingActor != Owner && BlockingActor->GetAttachParentActor() != Owner;
}
