// Copyright (C) Bas Blokzijl - All rights reserved.

#include "ArmorPlateAuditSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "RTS_Survival/GameUI/GameDifficultyPicker/W_GameDifficultyPicker.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/ArmorCalculation.h"
#include "RTS_Survival/RTSComponents/ArmorCalculationComponent/VehicleModules/VehicleModuleBalance.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/Tanks/TankMaster.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSArmorPlateAudit, Log, All);

namespace ArmorPlateAuditConstants
{
	const TCHAR* const AuditSwitch = TEXT("ArmorPlateAudit");
	const TCHAR* const ListSwitch = TEXT("ArmorPlateAuditList=");
	const TCHAR* const AsDrawnSwitch = TEXT("ArmorPlateAuditAsDrawn");
	const TCHAR* const AsDrawnSuffix = TEXT("@asdrawn");
	const TCHAR* const AnchorClassPrefix = TEXT("BP_UnitImage_LandscapeAnchor");
	const FName DifficultyButtonName(TEXT("M_NormalDifficultyButton"));

	// Real seconds: the HUD and start widget need a moment before the game can be started.
	constexpr double HudBuildSeconds = 5.0;
	constexpr double MaxStartSeconds = 60.0;
	// Game seconds. Modules finalize a tick after the turrets register; a vehicle that never finalizes is audited
	// after the wait and reported. Kept short: the map's artillery and infantry can kill a tank within two seconds.
	constexpr float LandingSeconds = 0.5f;
	constexpr float MaxModuleWaitSeconds = 3.f;

	constexpr float SpawnHeight = 150.f;
	constexpr float GroundTraceHalfHeight = 5000.f;
	// Probe shells start this far outside the registered meshes' bounds.
	constexpr float ProbeStartMargin = 300.f;
	constexpr float ProbePenetration = 100.f;

	// Along the side as a fraction of the length (sides) or width (front and rear), from the centre.
	const TArray<float> SideAlongFractions = {-0.4f, -0.2f, 0.f, 0.2f, 0.4f};
	const TArray<float> FrontAlongFractions = {-0.25f, 0.f, 0.25f};
	// Height as a fraction of the registered meshes' height, from their bottom.
	// Render bounds reach above the armoured hull (antennas, crew, guns), so the probes stay in the lower half.
	const TArray<float> HeightFractions = {0.2f, 0.35f, 0.5f};

	/** Which way a plate faces on the vehicle; Neutral plates may be hit from several sides. */
	enum class EPlateFacing : uint8
	{
		Left,
		Right,
		Front,
		Rear,
		Neutral
	};

	EPlateFacing GetPlateFacing(const EArmorPlate Plate)
	{
		switch (Plate)
		{
		case EArmorPlate::Plate_SideLeft:
		case EArmorPlate::Plate_SideLowerLeft:
		case EArmorPlate::Turret_SideLeft:
			return EPlateFacing::Left;
		case EArmorPlate::Plate_SideRight:
		case EArmorPlate::Plate_SideLowerRight:
		case EArmorPlate::Turret_SideRight:
			return EPlateFacing::Right;
		case EArmorPlate::Plate_Front:
		case EArmorPlate::Plate_FrontUpperGlacis:
		case EArmorPlate::Plate_FrontLowerGlacis:
		case EArmorPlate::Turret_Front:
		case EArmorPlate::Turret_Mantlet:
			return EPlateFacing::Front;
		case EArmorPlate::Plate_Rear:
		case EArmorPlate::Plate_RearLowerGlacis:
		case EArmorPlate::Plate_RearUpperGlacis:
		case EArmorPlate::Turret_Rear:
			return EPlateFacing::Rear;
		default:
			return EPlateFacing::Neutral;
		}
	}

	const TCHAR* GetSideName(const EArmorPlateAuditSide Side)
	{
		switch (Side)
		{
		case EArmorPlateAuditSide::Left:
			return TEXT("left");
		case EArmorPlateAuditSide::Right:
			return TEXT("right");
		case EArmorPlateAuditSide::Front:
			return TEXT("front");
		default:
			return TEXT("rear");
		}
	}

	// Out: the facing that must never resolve from this side, and the two facings that are only noted.
	void GetForbiddenAndNotedFacings(const EArmorPlateAuditSide Side, EPlateFacing& OutForbidden,
	                                 EPlateFacing& OutNotedA, EPlateFacing& OutNotedB)
	{
		const bool bIsSideProbe = Side == EArmorPlateAuditSide::Left || Side == EArmorPlateAuditSide::Right;
		OutNotedA = bIsSideProbe ? EPlateFacing::Front : EPlateFacing::Left;
		OutNotedB = bIsSideProbe ? EPlateFacing::Rear : EPlateFacing::Right;
		switch (Side)
		{
		case EArmorPlateAuditSide::Left:
			OutForbidden = EPlateFacing::Right;
			return;
		case EArmorPlateAuditSide::Right:
			OutForbidden = EPlateFacing::Left;
			return;
		case EArmorPlateAuditSide::Front:
			OutForbidden = EPlateFacing::Rear;
			return;
		default:
			OutForbidden = EPlateFacing::Front;
		}
	}

	FString GetMeshAssetName(const UMeshComponent* Mesh)
	{
		if (const UStaticMeshComponent* const StaticMeshComponent = Cast<UStaticMeshComponent>(Mesh))
		{
			return GetNameSafe(StaticMeshComponent->GetStaticMesh());
		}
		if (const USkeletalMeshComponent* const SkeletalMeshComponent = Cast<USkeletalMeshComponent>(Mesh))
		{
			return GetNameSafe(SkeletalMeshComponent->GetSkeletalMeshAsset());
		}
		return TEXT("none");
	}

	FString GetPlateName(const EArmorPlate Plate)
	{
		return UEnum::GetValueAsString(Plate).Replace(TEXT("EArmorPlate::"), TEXT(""));
	}
}

bool UArmorPlateAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	return IsValid(OuterWorld) && OuterWorld->IsGameWorld()
		&& FParse::Param(FCommandLine::Get(), ArmorPlateAuditConstants::AuditSwitch);
}

void UArmorPlateAuditSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// Error dialogs are answered automatically in unattended runs; log them instead.
	RTSFunctionLibrary::PushModalDialogSuppression();
	bM_IsSuppressingErrorDialogs = true;
	bM_RunAsDrawn = FParse::Param(FCommandLine::Get(), ArmorPlateAuditConstants::AsDrawnSwitch);
	EnterStep(EArmorPlateAuditStep::WaitingForGameStart);
	if (not LoadVehicleList())
	{
		Finish();
		return;
	}
	UE_LOG(LogRTSArmorPlateAudit, Display, TEXT("RTS_ARMOR_AUDIT started vehicles=%d"), M_VehicleClassPaths.Num());
}

void UArmorPlateAuditSubsystem::Deinitialize()
{
	if (bM_IsSuppressingErrorDialogs)
	{
		RTSFunctionLibrary::PopModalDialogSuppression();
		bM_IsSuppressingErrorDialogs = false;
	}
	Super::Deinitialize();
}

TStatId UArmorPlateAuditSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UArmorPlateAuditSubsystem, STATGROUP_Tickables);
}

void UArmorPlateAuditSubsystem::Tick(const float DeltaTime)
{
	using namespace ArmorPlateAuditConstants;
	switch (M_Step)
	{
	case EArmorPlateAuditStep::WaitingForGameStart:
		if (FPlatformTime::Seconds() - M_StepStartRealSeconds > MaxStartSeconds)
		{
			UE_LOG(LogRTSArmorPlateAudit, Error, TEXT("RTS_ARMOR_AUDIT FAIL the game did not start"));
			Finish();
			return;
		}
		if (FPlatformTime::Seconds() - M_StepStartRealSeconds < HudBuildSeconds || not TryStartGame())
		{
			return;
		}
		FindBaseLocation();
		EnterStep(EArmorPlateAuditStep::SpawnNext);
		return;
	case EArmorPlateAuditStep::SpawnNext:
		KeepGameRunning();
		SpawnNextVehicle();
		return;
	case EArmorPlateAuditStep::WaitingForModules:
		KeepGameRunning();
		if (not M_Tank.IsValid())
		{
			UE_LOG(LogRTSArmorPlateAudit, Error, TEXT("RTS_ARMOR_AUDIT FAIL vehicle %d was lost before its audit"),
			       M_NextVehicleIndex - 1);
			EnterStep(EArmorPlateAuditStep::SpawnNext);
			return;
		}
		if (GetGameSecondsInStep() < LandingSeconds)
		{
			return;
		}
		if (M_Armor.IsValid() && not M_Armor->GetAreModulesFinalized() && GetGameSecondsInStep() < MaxModuleWaitSeconds)
		{
			return;
		}
		AuditCurrentVehicle();
		M_Tank->Destroy();
		EnterStep(EArmorPlateAuditStep::SpawnNext);
		return;
	default:
		return;
	}
}

void UArmorPlateAuditSubsystem::EnterStep(const EArmorPlateAuditStep NewStep)
{
	M_Step = NewStep;
	M_StepStartRealSeconds = FPlatformTime::Seconds();
	M_StepStartGameSeconds = IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.f;
}

float UArmorPlateAuditSubsystem::GetGameSecondsInStep() const
{
	return IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() - M_StepStartGameSeconds : 0.f;
}

bool UArmorPlateAuditSubsystem::TryStartGame()
{
	UWorld* World = GetWorld();
	M_PlayerController = IsValid(World) ? Cast<ACPPController>(World->GetFirstPlayerController()) : nullptr;
	if (not M_PlayerController.IsValid())
	{
		return false;
	}
	UPlayerStartGameControl* StartGameControl = M_PlayerController->FindComponentByClass<UPlayerStartGameControl>();
	if (IsValid(StartGameControl) && StartGameControl->GetIsWaitingForPlayerToStart())
	{
		StartGameControl->PlayerStartedGame();
	}
	KeepGameRunning();
	return true;
}

void UArmorPlateAuditSubsystem::KeepGameRunning() const
{
	TArray<UUserWidget*> Pickers;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), Pickers, UW_GameDifficultyPicker::StaticClass(), false);
	for (UUserWidget* const Picker : Pickers)
	{
		if (not IsValid(Picker) || not IsValid(Picker->WidgetTree) || not Picker->IsInViewport())
		{
			continue;
		}
		UButton* const NormalButton = Cast<UButton>(
			Picker->WidgetTree->FindWidget(ArmorPlateAuditConstants::DifficultyButtonName));
		if (IsValid(NormalButton))
		{
			NormalButton->OnClicked.Broadcast();
		}
	}
	if (M_PlayerController.IsValid() && M_PlayerController->GetIsGameOnPause())
	{
		M_PlayerController->PauseGame(ERTSPauseGameOptions::ForceUnpause);
	}
}

bool UArmorPlateAuditSubsystem::LoadVehicleList()
{
	FString ListFile;
	if (not FParse::Value(FCommandLine::Get(), ArmorPlateAuditConstants::ListSwitch, ListFile))
	{
		UE_LOG(LogRTSArmorPlateAudit, Error, TEXT("RTS_ARMOR_AUDIT FAIL no -ArmorPlateAuditList=<file>"));
		return false;
	}
	TArray<FString> Lines;
	if (not FFileHelper::LoadFileToStringArray(Lines, *ListFile))
	{
		UE_LOG(LogRTSArmorPlateAudit, Error, TEXT("RTS_ARMOR_AUDIT FAIL cannot read %s"), *ListFile);
		return false;
	}
	for (const FString& Line : Lines)
	{
		const FString ClassPath = Line.TrimStartAndEnd();
		if (not ClassPath.IsEmpty() && not ClassPath.StartsWith(TEXT("#")))
		{
			M_VehicleClassPaths.Add(ClassPath);
		}
	}
	return M_VehicleClassPaths.Num() > 0;
}

void UArmorPlateAuditSubsystem::FindBaseLocation()
{
	using namespace ArmorPlateAuditConstants;
	UWorld* World = GetWorld();
	for (TActorIterator<AActor> It(World); It && M_BaseLocation.IsZero(); ++It)
	{
		if (It->GetClass()->GetName().StartsWith(AnchorClassPrefix))
		{
			M_BaseLocation = It->GetActorLocation();
		}
	}
	for (TActorIterator<APlayerStart> It(World); It && M_BaseLocation.IsZero(); ++It)
	{
		M_BaseLocation = It->GetActorLocation();
	}
	const FVector TraceOffset(0.f, 0.f, GroundTraceHalfHeight);
	FHitResult GroundHit;
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ArmorPlateAuditGround), false);
	if (World->LineTraceSingleByChannel(GroundHit, M_BaseLocation + TraceOffset, M_BaseLocation - TraceOffset,
	                                    ECC_Visibility, QueryParams))
	{
		M_BaseLocation = GroundHit.ImpactPoint;
	}
}

void UArmorPlateAuditSubsystem::SpawnNextVehicle()
{
	if (M_NextVehicleIndex >= M_VehicleClassPaths.Num())
	{
		Finish();
		return;
	}
	const FString& ClassPath = M_VehicleClassPaths[M_NextVehicleIndex++];
	UClass* const TankClass = LoadClass<ATankMaster>(nullptr, *ClassPath);
	if (not IsValid(TankClass))
	{
		UE_LOG(LogRTSArmorPlateAudit, Error, TEXT("RTS_ARMOR_AUDIT FAIL class does not load: %s"), *ClassPath);
		return;
	}
	const FTransform SpawnTransform(FRotator::ZeroRotator,
	                                M_BaseLocation + FVector(0.f, 0.f, ArmorPlateAuditConstants::SpawnHeight));
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ATankMaster* const Tank = GetWorld()->SpawnActor<ATankMaster>(TankClass, SpawnTransform, SpawnParameters);
	if (not IsValid(Tank))
	{
		UE_LOG(LogRTSArmorPlateAudit, Error, TEXT("RTS_ARMOR_AUDIT FAIL %s does not spawn"), *ClassPath);
		return;
	}
	M_Tank = Tank;
	M_Armor = Tank->FindComponentByClass<UArmorCalculation>();
	EnterStep(EArmorPlateAuditStep::WaitingForModules);
}

void UArmorPlateAuditSubsystem::AuditCurrentVehicle()
{
	const FString VehicleName = M_Tank->GetClass()->GetName().LeftChop(2);
	if (not M_Armor.IsValid())
	{
		++M_VehiclesWithoutModules;
		UE_LOG(LogRTSArmorPlateAudit, Warning, TEXT("RTS_ARMOR_AUDIT vehicle %s armour_component=0"), *VehicleName);
		return;
	}
	LogModules(VehicleName);
	LogPlateBoxes(VehicleName);
	const int32 WrongProbes = RunProbes(VehicleName);
	M_VehiclesWithWrongPlates += WrongProbes > 0 ? 1 : 0;
	M_VehiclesWithoutModules += M_Armor->GetAreModulesFinalized() ? 0 : 1;
	if (not bM_RunAsDrawn)
	{
		return;
	}
	ReRegisterPlatesAtDrawnSize();
	const FString AsDrawnName = VehicleName + ArmorPlateAuditConstants::AsDrawnSuffix;
	LogPlateBoxes(AsDrawnName);
	(void)RunProbes(AsDrawnName);
}

void UArmorPlateAuditSubsystem::ReRegisterPlatesAtDrawnSize() const
{
	const FArmorSetup Setup = M_Armor->GetArmorSetup();
	UMeshComponent* const Meshes[] = {Setup.MeshWithArmor0, Setup.MeshWithArmor1, Setup.MeshWithArmor2};
	const FArmorSettings* const PlateArrays[] = {Setup.ArmorSettings0, Setup.ArmorSettings1, Setup.ArmorSettings2};
	const int32 PlateCounts[] = {Setup.NumArmorPlates0, Setup.NumArmorPlates1, Setup.NumArmorPlates2};
	const URTSComponent* const RTSComponent = M_Tank->FindComponentByClass<URTSComponent>();
	const uint8 OwningPlayer = IsValid(RTSComponent) ? RTSComponent->GetOwningPlayer() : 0;
	M_Armor->ClearArmorSetup();
	for (int32 MeshSlot = 0; MeshSlot < UE_ARRAY_COUNT(Meshes); ++MeshSlot)
	{
		if (not IsValid(Meshes[MeshSlot]))
		{
			continue;
		}
		TArray<FArmorSettings> DrawnPlates;
		for (int32 PlateIndex = 0; PlateIndex < PlateCounts[MeshSlot]; ++PlateIndex)
		{
			// The stored box is already extent * scale: the drawn size. Unit scale keeps it at that size.
			FArmorSettings Plate = PlateArrays[MeshSlot][PlateIndex];
			Plate.ArmorBoxTransform.SetScale3D(FVector::OneVector);
			DrawnPlates.Add(Plate);
		}
		M_Armor->InitArmorCalculation(Meshes[MeshSlot], DrawnPlates, OwningPlayer);
	}
}

void UArmorPlateAuditSubsystem::LogModules(const FString& VehicleName) const
{
	TArray<FString> ModuleNames;
	for (int32 SlotIndex = 0; SlotIndex < VehicleModuleBalance::MaxModuleInstances; ++SlotIndex)
	{
		const FVehicleModuleSnapshot Snapshot = M_Armor->GetModuleSnapshotForSlot(SlotIndex);
		if (Snapshot.bInstalled)
		{
			ModuleNames.Add(UEnum::GetValueAsString(Snapshot.Type).Replace(TEXT("EVehicleModuleTypes::"), TEXT("")));
		}
	}
	UE_LOG(LogRTSArmorPlateAudit, Display, TEXT("RTS_ARMOR_AUDIT vehicle %s armour_component=1 modules_finalized=%d modules=%s"),
	       *VehicleName, M_Armor->GetAreModulesFinalized() ? 1 : 0,
	       ModuleNames.IsEmpty() ? TEXT("none") : *FString::Join(ModuleNames, TEXT(",")));
}

void UArmorPlateAuditSubsystem::LogPlateBoxes(const FString& VehicleName) const
{
	using namespace ArmorPlateAuditConstants;
	const FArmorSetup Setup = M_Armor->GetArmorSetup();
	const UMeshComponent* const Meshes[] = {Setup.MeshWithArmor0, Setup.MeshWithArmor1, Setup.MeshWithArmor2};
	const FArmorSettings* const PlateArrays[] = {Setup.ArmorSettings0, Setup.ArmorSettings1, Setup.ArmorSettings2};
	const int32 PlateCounts[] = {Setup.NumArmorPlates0, Setup.NumArmorPlates1, Setup.NumArmorPlates2};
	const FTransform ActorTransform = M_Tank->GetActorTransform();
	for (int32 MeshSlot = 0; MeshSlot < UE_ARRAY_COUNT(Meshes); ++MeshSlot)
	{
		const UMeshComponent* const Mesh = Meshes[MeshSlot];
		if (not IsValid(Mesh))
		{
			continue;
		}
		const FTransform MeshTransform = Mesh->GetComponentTransform();
		UE_LOG(LogRTSArmorPlateAudit, Display, TEXT("RTS_ARMOR_AUDIT mesh %s slot=%d component=%s asset=%s actor_rotation=%s plates=%d"),
		       *VehicleName, MeshSlot, *Mesh->GetName(), *GetMeshAssetName(Mesh),
		       *MeshTransform.GetRelativeTransform(ActorTransform).Rotator().ToCompactString(), PlateCounts[MeshSlot]);
		for (int32 PlateIndex = 0; PlateIndex < PlateCounts[MeshSlot]; ++PlateIndex)
		{
			const FArmorSettings& Plate = PlateArrays[MeshSlot][PlateIndex];
			// The hit test inverts this transform, so its corners mapped forward are the space that resolves the plate.
			const FTransform WorldArmorTransform = Plate.ArmorBoxTransform * MeshTransform;
			FVector Corners[8];
			Plate.ArmorBox.GetVertices(Corners);
			FBox ActorSpaceBox(ForceInit);
			for (const FVector& Corner : Corners)
			{
				ActorSpaceBox += ActorTransform.InverseTransformPosition(WorldArmorTransform.TransformPosition(Corner));
			}
			UE_LOG(LogRTSArmorPlateAudit, Display,
			       TEXT("RTS_ARMOR_AUDIT plate %s slot=%d index=%d type=%s armour=%.1f addon=%.1f box_scale=%s actor_min=%s actor_max=%s"),
			       *VehicleName, MeshSlot, PlateIndex, *GetPlateName(Plate.ArmorType), Plate.ArmorValue,
			       Plate.AddOnArmorValue, *Plate.ArmorBoxTransform.GetScale3D().ToCompactString(),
			       *ActorSpaceBox.Min.ToCompactString(), *ActorSpaceBox.Max.ToCompactString());
		}
	}
}

FBox UArmorPlateAuditSubsystem::GetRegisteredMeshes(TArray<UMeshComponent*>& OutMeshes) const
{
	const FArmorSetup Setup = M_Armor->GetArmorSetup();
	UMeshComponent* const Meshes[] = {Setup.MeshWithArmor0, Setup.MeshWithArmor1, Setup.MeshWithArmor2};
	const FTransform ActorTransform = M_Tank->GetActorTransform();
	FBox ActorBounds(ForceInit);
	for (UMeshComponent* const Mesh : Meshes)
	{
		if (not IsValid(Mesh))
		{
			continue;
		}
		OutMeshes.Add(Mesh);
		ActorBounds += Mesh->CalcBounds(Mesh->GetComponentTransform().GetRelativeTransform(ActorTransform)).GetBox();
	}
	return ActorBounds;
}

int32 UArmorPlateAuditSubsystem::RunProbes(const FString& VehicleName) const
{
	using namespace ArmorPlateAuditConstants;
	TArray<UMeshComponent*> Meshes;
	const FBox ActorBounds = GetRegisteredMeshes(Meshes);
	if (not ActorBounds.IsValid)
	{
		UE_LOG(LogRTSArmorPlateAudit, Warning, TEXT("RTS_ARMOR_AUDIT summary %s registered_meshes=0"), *VehicleName);
		return 0;
	}
	TArray<TPair<EArmorPlateAuditSide, const TArray<float>*>> Patterns = {
		{EArmorPlateAuditSide::Left, &SideAlongFractions}, {EArmorPlateAuditSide::Right, &SideAlongFractions},
		{EArmorPlateAuditSide::Front, &FrontAlongFractions}, {EArmorPlateAuditSide::Rear, &FrontAlongFractions}
	};
	int32 WrongProbes = 0;
	int32 NotedProbes = 0;
	int32 MissedProbes = 0;
	for (const TPair<EArmorPlateAuditSide, const TArray<float>*>& Pattern : Patterns)
	{
		EPlateFacing Forbidden;
		EPlateFacing NotedA;
		EPlateFacing NotedB;
		GetForbiddenAndNotedFacings(Pattern.Key, Forbidden, NotedA, NotedB);
		for (const float HeightFraction : HeightFractions)
		{
			for (const float AlongFraction : *Pattern.Value)
			{
				const FArmorPlateAuditProbe Probe = ResolveProbe(Pattern.Key, AlongFraction, HeightFraction, ActorBounds, Meshes);
				const EPlateFacing Facing = GetPlateFacing(Probe.PlateHit);
				const bool bResolved = Probe.bHitMesh && Probe.PlateRegistrationId != INDEX_NONE;
				const TCHAR* Verdict = not bResolved ? TEXT("MISS")
					: Facing == Forbidden ? TEXT("WRONG")
					: Facing == NotedA || Facing == NotedB ? TEXT("NOTE") : TEXT("ok");
				WrongProbes += bResolved && Facing == Forbidden ? 1 : 0;
				NotedProbes += bResolved && (Facing == NotedA || Facing == NotedB) ? 1 : 0;
				MissedProbes += bResolved ? 0 : 1;
				UE_LOG(LogRTSArmorPlateAudit, Display,
				       TEXT("RTS_ARMOR_AUDIT probe %s from=%s along=%.2f height=%.2f %s mesh=%s hit=%s plate=%s raw=%.1f"),
				       *VehicleName, GetSideName(Pattern.Key), AlongFraction, HeightFraction, Verdict, *Probe.HitMeshName,
				       *Probe.ActorSpaceHit.ToCompactString(), bResolved ? *GetPlateName(Probe.PlateHit) : TEXT("none"),
				       Probe.RawArmor);
			}
		}
	}
	UE_LOG(LogRTSArmorPlateAudit, Display, TEXT("RTS_ARMOR_AUDIT summary %s modules_finalized=%d wrong=%d noted=%d missed=%d bounds_min=%s bounds_max=%s"),
	       *VehicleName, M_Armor->GetAreModulesFinalized() ? 1 : 0, WrongProbes, NotedProbes, MissedProbes,
	       *ActorBounds.Min.ToCompactString(), *ActorBounds.Max.ToCompactString());
	return WrongProbes;
}

FArmorPlateAuditProbe UArmorPlateAuditSubsystem::ResolveProbe(const EArmorPlateAuditSide Side, const float AlongFraction,
                                                              const float HeightFraction, const FBox& ActorBounds,
                                                              const TArray<UMeshComponent*>& Meshes) const
{
	using namespace ArmorPlateAuditConstants;
	FArmorPlateAuditProbe Probe;
	Probe.Side = Side;
	Probe.AlongFraction = AlongFraction;
	Probe.HeightFraction = HeightFraction;
	const FVector Centre = ActorBounds.GetCenter();
	const FVector Size = ActorBounds.GetSize();
	const float Height = ActorBounds.Min.Z + HeightFraction * Size.Z;
	const bool bIsSideProbe = Side == EArmorPlateAuditSide::Left || Side == EArmorPlateAuditSide::Right;
	// The vehicle faces +X; its left side is -Y.
	FVector LocalStart;
	FVector LocalEnd;
	if (bIsSideProbe)
	{
		const float AlongX = Centre.X + AlongFraction * Size.X;
		const bool bFromLeft = Side == EArmorPlateAuditSide::Left;
		LocalStart = FVector(AlongX, bFromLeft ? ActorBounds.Min.Y - ProbeStartMargin : ActorBounds.Max.Y + ProbeStartMargin, Height);
		LocalEnd = FVector(AlongX, bFromLeft ? ActorBounds.Max.Y + ProbeStartMargin : ActorBounds.Min.Y - ProbeStartMargin, Height);
	}
	else
	{
		const float AlongY = Centre.Y + AlongFraction * Size.Y;
		const bool bFromFront = Side == EArmorPlateAuditSide::Front;
		LocalStart = FVector(bFromFront ? ActorBounds.Max.X + ProbeStartMargin : ActorBounds.Min.X - ProbeStartMargin, AlongY, Height);
		LocalEnd = FVector(bFromFront ? ActorBounds.Min.X - ProbeStartMargin : ActorBounds.Max.X + ProbeStartMargin, AlongY, Height);
	}
	const FTransform ActorTransform = M_Tank->GetActorTransform();
	const FVector Start = ActorTransform.TransformPosition(LocalStart);
	const FVector End = ActorTransform.TransformPosition(LocalEnd);
	const FVector Direction = (End - Start).GetSafeNormal();

	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ArmorPlateAuditProbe), true);
	FHitResult NearestHit;
	UMeshComponent* NearestMesh = nullptr;
	for (UMeshComponent* const Mesh : Meshes)
	{
		FHitResult Hit;
		if (Mesh->LineTraceComponent(Hit, Start, End, QueryParams) && (NearestMesh == nullptr || Hit.Time < NearestHit.Time))
		{
			NearestHit = Hit;
			NearestMesh = Mesh;
		}
	}
	if (NearestMesh == nullptr)
	{
		Probe.HitMeshName = TEXT("none");
		return Probe;
	}
	Probe.bHitMesh = true;
	Probe.HitMeshName = NearestMesh->GetName();
	Probe.ActorSpaceHit = ActorTransform.InverseTransformPosition(NearestHit.ImpactPoint);
	float AdjustedPenetration = ProbePenetration;
	(void)M_Armor->GetEffectiveArmorOnHit(NearestMesh, NearestHit.ImpactPoint, Direction, NearestHit.ImpactNormal,
	                                      Probe.RawArmor, AdjustedPenetration, Probe.PlateHit,
	                                      Probe.PlateRegistrationId);
	return Probe;
}

void UArmorPlateAuditSubsystem::Finish()
{
	M_Step = EArmorPlateAuditStep::Done;
	UE_LOG(LogRTSArmorPlateAudit, Display, TEXT("RTS_ARMOR_AUDIT RESULT vehicles=%d with_wrong_plates=%d without_finalized_modules=%d"),
	       M_NextVehicleIndex, M_VehiclesWithWrongPlates, M_VehiclesWithoutModules);
	FPlatformMisc::RequestExit(false, TEXT("ArmorPlateAudit"));
}

#else

bool UArmorPlateAuditSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UArmorPlateAuditSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

void UArmorPlateAuditSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UArmorPlateAuditSubsystem::Tick(float DeltaTime)
{
}

TStatId UArmorPlateAuditSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UArmorPlateAuditSubsystem, STATGROUP_Tickables);
}

#endif // RTS_WITH_MAP_TESTS
