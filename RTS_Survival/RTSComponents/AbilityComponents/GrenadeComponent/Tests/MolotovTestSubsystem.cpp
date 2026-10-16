// Copyright (C) Bas Blokzijl - All rights reserved.

#include "MolotovTestSubsystem.h"

#include "RTS_Survival/Utils/RTSMapTests/RTSMapTests.h"

#if RTS_WITH_MAP_TESTS

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/SCS_Node.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "UObject/UObjectIterator.h"
#include "RTS_Survival/GameUI/GameDifficultyPicker/W_GameDifficultyPicker.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PauseGame/PauseGameOptions.h"
#include "RTS_Survival/Player/StartGameControl/PlayerStartGameControl.h"
#include "RTS_Survival/RTSComponents/AbilityComponents/GrenadeComponent/GrenadeComponent.h"
#include "RTS_Survival/RTSComponents/HealthComponent.h"
#include "RTS_Survival/RTSComponents/RTSComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/Squads/SquadUnit/SquadUnit.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Weapons/InfantryWeapon/InfantryWeaponMaster.h"

DEFINE_LOG_CATEGORY_STATIC(LogRTSMolotovTest, Log, All);

namespace MolotovTestPrivate
{
	const TCHAR* const ValidateSwitch = TEXT("MolotovValidate");
	const TCHAR* const DirectorySwitch = TEXT("MolotovDir=");
	// X,Y of the test spot; the squads spawn around it.
	const TCHAR* const LocationSwitch = TEXT("MolotovAt=");
	const TCHAR* const AnchorClassPrefix = TEXT("BP_UnitImage_LandscapeAnchor");
	const FName DifficultyButtonName(TEXT("M_NormalDifficultyButton"));

	const TCHAR* const MolotovSquadClassPath = TEXT("/Game/RTS_Survival/Blueprints/Squads/Rus/Rus_Squad_Mosin.Rus_Squad_Mosin_C");
	const TCHAR* const TargetSquadClassPath = TEXT("/Game/RTS_Survival/Blueprints/Squads/Ger/Squad_Ger_Mauser.Squad_Ger_Mauser_C");
	const TCHAR* const RagFlamePath = TEXT("/Game/RTS_Survival/SpecialEffects/Molotov/NS_Molotov_RagFlame.NS_Molotov_RagFlame");
	const TCHAR* const ImpactPath = TEXT("/Game/RTS_Survival/SpecialEffects/Molotov/NS_MolotovImpact.NS_MolotovImpact");

	// Soldiers keep the owner of their own Blueprint (player 1), only the squad controller takes the test's owner.
	// The grenade picks its victims from the squad's owner, so an enemy-owned thrower squad burns a player squad,
	// and the two squads' soldiers never shoot each other: only the fire hurts the target.
	constexpr uint8 MolotovPlayer = 2;
	constexpr uint8 TargetPlayer = 1;
	// Beyond the throw range, so the squad also walks into range like in a real order.
	const FVector TargetSquadOffset(1100.f, 0.f, 0.f);
	constexpr float NavProjectionExtent = 500.f;
	constexpr float SpawnHeightOffset = 100.f;
	constexpr float GroundTraceHalfHeight = 10000.f;
	// Map anchors are not always on the navmesh; the squads must be able to walk into throw range.
	const FVector NavSearchExtent(20000.f, 20000.f, 5000.f);

	// Real seconds: the HUD and start widget need a moment before the game can be started.
	constexpr double HudBuildSeconds = 5.0;
	constexpr double MaxStartSeconds = 60.0;
	// Game seconds.
	constexpr float MaxSquadLoadSeconds = 60.f;
	// Soldiers spawn a few frames after their squad; both squads stand this long before the order.
	constexpr int32 MinLivingSoldiersPerSquad = 2;
	constexpr float SquadSettleSeconds = 3.f;
	constexpr float MaxThrowSeconds = 40.f;
	// Other squads on the map may throw molotovs too; only an impact this close to the ordered target counts.
	constexpr float OwnImpactMaxDistance = 1000.f;
	// Impact damage and the first burn tick land within this time after the impact.
	constexpr float AfterImpactDamageSeconds = 1.2f;
	// The burn lasts 6 s; the impact effect is stopped then and its last flames die out.
	constexpr float BurnEndCheckSeconds = 6.6f;
	constexpr float EffectGoneCheckSeconds = 9.5f;
	constexpr float BurnPictureSeconds[] = {0.4f, 1.5f, 3.f, 5.f, 6.6f, 9.5f};

	constexpr int32 PictureWidth = 1280;
	constexpr int32 PictureHeight = 720;
	constexpr float CloseFieldOfView = 40.f;
	constexpr float GameFieldOfView = 90.f;
	// The RTS camera: spring arm 3000 at -70 degrees; the close view looks at the thrower from the side.
	const FRotator GameCameraRotation(-70.f, 0.f, 0.f);
	constexpr float GameCameraDistance = 3000.f;
	const FRotator CloseCameraRotation(-15.f, 90.f, 0.f);
	constexpr float CloseCameraDistance = 650.f;
	const FVector CloseCameraLookAtOffset(0.f, 0.f, 130.f);
	constexpr int32 FirstHoldPictureFrame = 12;
	constexpr int32 SecondHoldPictureFrame = 30;
	constexpr int32 BurnPictureCount = UE_ARRAY_COUNT(BurnPictureSeconds);

	void OverrideRTSComponentTemplateOwners(UClass* UnitClass, const uint8 OwningPlayer,
	                                        TArray<TPair<TWeakObjectPtr<UObject>, uint8>>& OutRestore)
	{
		const FByteProperty* OwnerProperty = FindFProperty<FByteProperty>(URTSComponent::StaticClass(), TEXT("OwningPlayer"));
		if (OwnerProperty == nullptr)
		{
			return;
		}
		TArray<UActorComponent*> Templates;
		for (UClass* Class = UnitClass; Class != nullptr; Class = Class->GetSuperClass())
		{
			UBlueprintGeneratedClass* BlueprintClass = Cast<UBlueprintGeneratedClass>(Class);
			if (BlueprintClass == nullptr)
			{
				continue;
			}
			if (BlueprintClass->SimpleConstructionScript != nullptr)
			{
				for (const USCS_Node* Node : BlueprintClass->SimpleConstructionScript->GetAllNodes())
				{
					Templates.Add(Node != nullptr ? Node->ComponentTemplate : nullptr);
				}
			}
			if (UInheritableComponentHandler* Handler = BlueprintClass->GetInheritableComponentHandler(false))
			{
				for (auto RecordIterator = Handler->CreateRecordIterator(); RecordIterator; ++RecordIterator)
				{
					Templates.Add(RecordIterator->ComponentTemplate);
				}
			}
		}
		for (UActorComponent* Template : Templates)
		{
			if (not IsValid(Template) || not Template->IsA<URTSComponent>())
			{
				continue;
			}
			uint8* OwnerValue = OwnerProperty->ContainerPtrToValuePtr<uint8>(Template);
			OutRestore.Emplace(Template, *OwnerValue);
			*OwnerValue = OwningPlayer;
		}
	}

	void RestoreRTSComponentTemplateOwners(const TArray<TPair<TWeakObjectPtr<UObject>, uint8>>& Restore)
	{
		const FByteProperty* OwnerProperty = FindFProperty<FByteProperty>(URTSComponent::StaticClass(), TEXT("OwningPlayer"));
		for (const TPair<TWeakObjectPtr<UObject>, uint8>& Entry : Restore)
		{
			if (OwnerProperty != nullptr && Entry.Key.IsValid())
			{
				*OwnerProperty->ContainerPtrToValuePtr<uint8>(Entry.Key.Get()) = Entry.Value;
			}
		}
	}

	// IsUnitAlive is not const on AActor.
	bool GetIsLivingSoldier(ASquadUnit* Soldier)
	{
		return IsValid(Soldier) && Soldier->IsUnitAlive();
	}

	int32 GetLivingSoldierCount(ASquadController* Squad)
	{
		int32 LivingSoldiers = 0;
		for (ASquadUnit* Soldier : Squad->GetSquadUnitsChecked())
		{
			LivingSoldiers += GetIsLivingSoldier(Soldier) ? 1 : 0;
		}
		return LivingSoldiers;
	}
}

bool UMolotovTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* OuterWorld = Cast<UWorld>(Outer);
	if (not IsValid(OuterWorld) || not OuterWorld->IsGameWorld())
	{
		return false;
	}
	return FParse::Param(FCommandLine::Get(), MolotovTestPrivate::ValidateSwitch);
}

void UMolotovTestSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	M_OutputDirectory = FPaths::ProjectSavedDir() / TEXT("MolotovTest");
	FParse::Value(FCommandLine::Get(), MolotovTestPrivate::DirectorySwitch, M_OutputDirectory);
	M_RagFlameSystem = LoadObject<UNiagaraSystem>(nullptr, MolotovTestPrivate::RagFlamePath);
	M_ImpactSystem = LoadObject<UNiagaraSystem>(nullptr, MolotovTestPrivate::ImpactPath);
	// Error dialogs are answered automatically in unattended runs, but each one stalls a frame; log them instead.
	RTSFunctionLibrary::PushModalDialogSuppression();
	bM_IsSuppressingErrorDialogs = true;
	EnterPhase(EMolotovTestPhase::WaitingForGameStart);
	UE_LOG(LogRTSMolotovTest, Display, TEXT("RTS_MOLOTOV_TEST started, pictures in %s"), *M_OutputDirectory);
}

void UMolotovTestSubsystem::Deinitialize()
{
	if (bM_IsSuppressingErrorDialogs)
	{
		RTSFunctionLibrary::PopModalDialogSuppression();
		bM_IsSuppressingErrorDialogs = false;
	}
	Super::Deinitialize();
}

void UMolotovTestSubsystem::Tick(float DeltaTime)
{
	if (M_Phase == EMolotovTestPhase::Inactive || M_Phase == EMolotovTestPhase::Done)
	{
		return;
	}
	if (M_Phase != EMolotovTestPhase::WaitingForGameStart)
	{
		KeepGameRunning();
	}
	switch (M_Phase)
	{
	case EMolotovTestPhase::WaitingForGameStart:
		TickWaitingForGameStart();
		break;
	case EMolotovTestPhase::WaitingForSquads:
		TickWaitingForSquads();
		break;
	case EMolotovTestPhase::WaitingForImpact:
		TickWaitingForImpact();
		break;
	case EMolotovTestPhase::Burning:
		TickBurning();
		break;
	default:
		break;
	}
}

TStatId UMolotovTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMolotovTestSubsystem, STATGROUP_Tickables);
}

void UMolotovTestSubsystem::EnterPhase(const EMolotovTestPhase NewPhase)
{
	M_Phase = NewPhase;
	M_PhaseStartGameSeconds = IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.f;
	M_PhaseStartRealSeconds = FPlatformTime::Seconds();
}

float UMolotovTestSubsystem::GetGameSecondsInPhase() const
{
	return IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() - M_PhaseStartGameSeconds : 0.f;
}

double UMolotovTestSubsystem::GetRealSecondsInPhase() const
{
	return FPlatformTime::Seconds() - M_PhaseStartRealSeconds;
}

void UMolotovTestSubsystem::Check(const bool bCondition, const TCHAR* Description, const FString& Details)
{
	M_FailedCheckCount += bCondition ? 0 : 1;
	UE_LOG(LogRTSMolotovTest, Display, TEXT("RTS_MOLOTOV_TEST check=%s %s %s (game t=%.1f s)"),
	       bCondition ? TEXT("PASS") : TEXT("FAIL"), Description, *Details,
	       IsValid(GetWorld()) ? GetWorld()->GetTimeSeconds() : 0.f);
}

void UMolotovTestSubsystem::Finish()
{
	M_Phase = EMolotovTestPhase::Done;
	UE_LOG(LogRTSMolotovTest, Display, TEXT("RTS_MOLOTOV_TEST RESULT %s failed_checks=%d"),
	       M_FailedCheckCount == 0 ? TEXT("PASS") : TEXT("FAIL"), M_FailedCheckCount);
	FPlatformMisc::RequestExit(false, TEXT("MolotovTest"));
}

bool UMolotovTestSubsystem::TryStartGame()
{
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	M_PlayerController = Cast<ACPPController>(World->GetFirstPlayerController());
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

void UMolotovTestSubsystem::KeepGameRunning() const
{
	TArray<UUserWidget*> Pickers;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), Pickers, UW_GameDifficultyPicker::StaticClass(), false);
	for (UUserWidget* const Picker : Pickers)
	{
		if (not IsValid(Picker) || not IsValid(Picker->WidgetTree) || not Picker->IsInViewport())
		{
			continue;
		}
		UButton* const NormalButton = Cast<UButton>(Picker->WidgetTree->FindWidget(MolotovTestPrivate::DifficultyButtonName));
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

bool UMolotovTestSubsystem::FindTestLocation()
{
	using namespace MolotovTestPrivate;
	FString LocationText;
	if (FParse::Value(FCommandLine::Get(), LocationSwitch, LocationText, false))
	{
		TArray<FString> Coordinates;
		LocationText.ParseIntoArray(Coordinates, TEXT(","), true);
		if (Coordinates.Num() >= 2)
		{
			M_TestLocation = FVector(FCString::Atof(*Coordinates[0]), FCString::Atof(*Coordinates[1]), 0.f);
			DropToGround(M_TestLocation);
			return true;
		}
	}
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->GetClass()->GetName().StartsWith(AnchorClassPrefix))
		{
			M_TestLocation = It->GetActorLocation();
			DropToGround(M_TestLocation);
			return true;
		}
	}
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		M_TestLocation = It->GetActorLocation();
		DropToGround(M_TestLocation);
		return true;
	}
	UE_LOG(LogRTSMolotovTest, Error, TEXT("RTS_MOLOTOV_TEST no -MolotovAt, %s or player start on this map"),
	       AnchorClassPrefix);
	return false;
}

bool UMolotovTestSubsystem::MoveTestLocationOntoNavMesh()
{
	UNavigationSystemV1* NavigationSystem = UNavigationSystemV1::GetCurrent(GetWorld());
	FNavLocation NavLocation;
	if (not IsValid(NavigationSystem) ||
		not NavigationSystem->ProjectPointToNavigation(M_TestLocation, NavLocation, MolotovTestPrivate::NavSearchExtent))
	{
		return false;
	}
	UE_LOG(LogRTSMolotovTest, Display, TEXT("RTS_MOLOTOV_TEST test spot moved onto the navmesh: %s -> %s"),
	       *M_TestLocation.ToCompactString(), *NavLocation.Location.ToCompactString());
	M_TestLocation = NavLocation.Location;
	return true;
}

bool UMolotovTestSubsystem::DropToGround(FVector& InOutLocation) const
{
	// Map anchors sit at camera height; squads must spawn on the ground.
	const FVector TraceOffset(0.f, 0.f, MolotovTestPrivate::GroundTraceHalfHeight);
	FHitResult GroundHit;
	const FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MolotovTestGround), false);
	if (not GetWorld()->LineTraceSingleByChannel(GroundHit, InOutLocation + TraceOffset, InOutLocation - TraceOffset,
	                                             ECC_Visibility, QueryParams))
	{
		UE_LOG(LogRTSMolotovTest, Verbose, TEXT("RTS_MOLOTOV_TEST no ground below %s yet"), *InOutLocation.ToCompactString());
		return false;
	}
	InOutLocation = GroundHit.ImpactPoint;
	return true;
}

ASquadController* UMolotovTestSubsystem::SpawnSquadForPlayer(const TCHAR* ClassPath, const FVector& Location,
                                                             const uint8 OwningPlayer) const
{
	using namespace MolotovTestPrivate;
	UWorld* World = GetWorld();
	UClass* SquadClass = LoadClass<ASquadController>(nullptr, ClassPath);
	if (not IsValid(World) || not IsValid(SquadClass))
	{
		UE_LOG(LogRTSMolotovTest, Error, TEXT("RTS_MOLOTOV_TEST could not load squad class %s"), ClassPath);
		return nullptr;
	}
	FVector SpawnLocation = Location;
	DropToGround(SpawnLocation);
	FNavLocation NavLocation;
	UNavigationSystemV1* NavigationSystem = UNavigationSystemV1::GetCurrent(World);
	if (IsValid(NavigationSystem) &&
		NavigationSystem->ProjectPointToNavigation(Location, NavLocation, FVector(NavProjectionExtent)))
	{
		SpawnLocation = NavLocation.Location;
	}
	SpawnLocation.Z += SpawnHeightOffset;
	const FTransform SpawnTransform(SpawnLocation);
	// The owner must be in place before BeginPlay; Blueprint-added components take it from their template.
	TArray<TPair<TWeakObjectPtr<UObject>, uint8>> TemplateOwnersToRestore;
	OverrideRTSComponentTemplateOwners(SquadClass, OwningPlayer, TemplateOwnersToRestore);
	ASquadController* Squad = World->SpawnActorDeferred<ASquadController>(
		SquadClass, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (IsValid(Squad))
	{
		if (URTSComponent* RTSComponent = Squad->FindComponentByClass<URTSComponent>())
		{
			RTSComponent->SetOwningPlayerRuntime(OwningPlayer);
		}
		Squad->FinishSpawning(SpawnTransform);
	}
	RestoreRTSComponentTemplateOwners(TemplateOwnersToRestore);
	UE_LOG(LogRTSMolotovTest, Display, TEXT("RTS_MOLOTOV_TEST spawned %s owner=%d at %s"),
	       IsValid(Squad) ? *Squad->GetName() : TEXT("nothing"), OwningPlayer, *SpawnLocation.ToCompactString());
	return Squad;
}

bool UMolotovTestSubsystem::CreateCameras()
{
	return CreateCamera(M_CloseCamera, MolotovTestPrivate::CloseFieldOfView) &&
		CreateCamera(M_GameCamera, MolotovTestPrivate::GameFieldOfView);
}

bool UMolotovTestSubsystem::CreateCamera(FMolotovTestCamera& OutCamera, const float FieldOfView) const
{
	using namespace MolotovTestPrivate;
	UWorld* World = GetWorld();
	if (not IsValid(World))
	{
		return false;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ASceneCapture2D* Capture = World->SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass(), FTransform::Identity,
	                                                               SpawnParameters);
	UTextureRenderTarget2D* RenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(
		World, PictureWidth, PictureHeight, RTF_RGBA8_SRGB);
	if (not IsValid(Capture) || not IsValid(RenderTarget))
	{
		return false;
	}
	USceneCaptureComponent2D* CaptureComponent = Capture->GetCaptureComponent2D();
	CaptureComponent->TextureTarget = RenderTarget;
	CaptureComponent->CaptureSource = SCS_FinalColorLDR;
	CaptureComponent->FOVAngle = FieldOfView;
	// Rendering every frame with kept state lets auto exposure and temporal AA settle like the game view.
	CaptureComponent->bCaptureEveryFrame = true;
	CaptureComponent->bCaptureOnMovement = false;
	CaptureComponent->bAlwaysPersistRenderingState = true;
	// The fog of war is a post-process material; the pictures show the effect, not what the player may see.
	CaptureComponent->ShowFlags.SetPostProcessMaterial(false);
	OutCamera.Capture = Capture;
	OutCamera.RenderTarget = RenderTarget;
	return true;
}

void UMolotovTestSubsystem::AimCloseCameraAt(const FVector& LookAt) const
{
	using namespace MolotovTestPrivate;
	if (not M_CloseCamera.Capture.IsValid())
	{
		return;
	}
	const FVector Target = LookAt + CloseCameraLookAtOffset;
	M_CloseCamera.Capture->SetActorLocationAndRotation(
		Target - CloseCameraRotation.Vector() * CloseCameraDistance, CloseCameraRotation);
}

void UMolotovTestSubsystem::AimGameCameraAt(const FVector& LookAt) const
{
	using namespace MolotovTestPrivate;
	if (not M_GameCamera.Capture.IsValid())
	{
		return;
	}
	M_GameCamera.Capture->SetActorLocationAndRotation(
		LookAt - GameCameraRotation.Vector() * GameCameraDistance, GameCameraRotation);
}

void UMolotovTestSubsystem::SaveCameraPicture(const FMolotovTestCamera& Camera, const FString& Label) const
{
	if (not IsValid(Camera.RenderTarget) || not IsValid(GetWorld()))
	{
		return;
	}
	UKismetRenderingLibrary::ExportRenderTarget(GetWorld(), Camera.RenderTarget, M_OutputDirectory,
	                                            FString::Printf(TEXT("Molotov_%s.png"), *Label));
}

void UMolotovTestSubsystem::TickWaitingForGameStart()
{
	using namespace MolotovTestPrivate;
	if (GetRealSecondsInPhase() > MaxStartSeconds)
	{
		Check(false, TEXT("the game started on a spot with navmesh"));
		Finish();
		return;
	}
	if (GetRealSecondsInPhase() < HudBuildSeconds || not TryStartGame())
	{
		return;
	}
	// Streaming levels and the navmesh can arrive a little after the game starts; retry until the start timeout.
	if (not FindTestLocation() || not MoveTestLocationOntoNavMesh())
	{
		return;
	}
	if (not CreateCameras())
	{
		Check(false, TEXT("test cameras"));
		Finish();
		return;
	}
	Check(IsValid(M_RagFlameSystem) && IsValid(M_ImpactSystem), TEXT("molotov Niagara systems load"));
	M_MolotovSquad = SpawnSquadForPlayer(MolotovSquadClassPath, M_TestLocation, MolotovPlayer);
	M_TargetSquad = SpawnSquadForPlayer(TargetSquadClassPath, M_TestLocation + TargetSquadOffset, TargetPlayer);
	if (not M_MolotovSquad.IsValid() || not M_TargetSquad.IsValid())
	{
		Check(false, TEXT("both squads spawned"));
		Finish();
		return;
	}
	EnterPhase(EMolotovTestPhase::WaitingForSquads);
}

void UMolotovTestSubsystem::TickWaitingForSquads()
{
	using namespace MolotovTestPrivate;
	const bool bAreSquadsReady = M_MolotovSquad.IsValid() && M_TargetSquad.IsValid() &&
		M_MolotovSquad->GetIsSquadFullyLoadedAndInitialized() && M_TargetSquad->GetIsSquadFullyLoadedAndInitialized() &&
		GetLivingSoldierCount(M_MolotovSquad.Get()) >= MinLivingSoldiersPerSquad &&
		GetLivingSoldierCount(M_TargetSquad.Get()) >= MinLivingSoldiersPerSquad;
	AimGameCameraAt(GetTargetSquadCentre());
	if (bAreSquadsReady && M_SquadsReadyGameSeconds < 0.f)
	{
		M_SquadsReadyGameSeconds = GetWorld()->GetTimeSeconds();
	}
	const bool bHaveSquadsSettled = bAreSquadsReady &&
		GetWorld()->GetTimeSeconds() - M_SquadsReadyGameSeconds >= SquadSettleSeconds;
	if (not bHaveSquadsSettled || not GetIsMolotovOnSquadCard())
	{
		if (GetGameSecondsInPhase() > MolotovTestPrivate::MaxSquadLoadSeconds)
		{
			Check(bAreSquadsReady, TEXT("both squads loaded their soldiers"));
			Check(GetIsMolotovOnSquadCard(), TEXT("the Mosin squad has the molotov on its command card"));
			Finish();
		}
		return;
	}
	Check(true, TEXT("the Mosin squad has the molotov on its command card"));
	SaveCameraPicture(M_GameCamera, TEXT("Game_before"));
	OrderThrow();
	EnterPhase(EMolotovTestPhase::WaitingForImpact);
}

bool UMolotovTestSubsystem::GetIsMolotovOnSquadCard() const
{
	if (not M_MolotovSquad.IsValid())
	{
		return false;
	}
	const int32 MolotovSubtype = static_cast<int32>(EGrenadeAbilityType::RusMolotov);
	for (const FUnitAbilityEntry& AbilityEntry : M_MolotovSquad->GetUnitAbilityEntries())
	{
		if (AbilityEntry.AbilityId == EAbilityID::IdThrowGrenade && AbilityEntry.CustomType == MolotovSubtype)
		{
			return true;
		}
	}
	return false;
}

void UMolotovTestSubsystem::OrderThrow()
{
	M_ImpactLocation = GetTargetSquadCentre();
	const ECommandQueueError OrderResult = M_MolotovSquad->ThrowGrenade(M_ImpactLocation, true,
	                                                                     EGrenadeAbilityType::RusMolotov);
	bM_HasOrderedThrow = OrderResult == ECommandQueueError::NoError;
	Check(bM_HasOrderedThrow, TEXT("the molotov order is accepted"),
	      FString::Printf(TEXT("result=%d target=%s"), static_cast<int32>(OrderResult), *M_ImpactLocation.ToCompactString()));
}

void UMolotovTestSubsystem::TickWaitingForImpact()
{
	WatchThrownMolotovs();
	UNiagaraComponent* ImpactComponent = FindActiveImpactComponent();
	if (IsValid(ImpactComponent))
	{
		M_ImpactComponent = ImpactComponent;
		M_ImpactLocation = ImpactComponent->GetComponentLocation();
		AimGameCameraAt(M_ImpactLocation);
		UE_LOG(LogRTSMolotovTest, Display, TEXT("RTS_MOLOTOV_TEST impact at %s after %.1f s"),
		       *M_ImpactLocation.ToCompactString(), GetGameSecondsInPhase());
		LogTargetSoldiers(TEXT("impact"));
		EnterPhase(EMolotovTestPhase::Burning);
		return;
	}
	if (GetGameSecondsInPhase() > MolotovTestPrivate::MaxThrowSeconds)
	{
		Check(false, TEXT("the molotov landed and spawned its impact effect"));
		Finish();
	}
}

void UMolotovTestSubsystem::LogNewImpacts()
{
	for (TObjectIterator<UNiagaraComponent> It; It; ++It)
	{
		UNiagaraComponent* NiagaraComponent = *It;
		const bool bIsImpact = IsValid(NiagaraComponent) && NiagaraComponent->GetWorld() == GetWorld() &&
			NiagaraComponent->GetAsset() == M_ImpactSystem;
		if (not bIsImpact || M_LoggedImpacts.Contains(NiagaraComponent))
		{
			continue;
		}
		M_LoggedImpacts.Add(NiagaraComponent);
		const float DistanceToTarget = FVector::Dist2D(NiagaraComponent->GetComponentLocation(), M_ImpactLocation);
		UE_LOG(LogRTSMolotovTest, Display, TEXT("RTS_MOLOTOV_TEST molotov impact effect at %s, %.0f from the ordered target (game t=%.1f s)"),
		       *NiagaraComponent->GetComponentLocation().ToCompactString(), DistanceToTarget, GetWorld()->GetTimeSeconds());
	}
}

void UMolotovTestSubsystem::WatchThrownMolotovs()
{
	LogNewImpacts();
	for (TActorIterator<AGrenadeActor> It(GetWorld()); It; ++It)
	{
		AGrenadeActor* Grenade = *It;
		const UNiagaraComponent* RagFlame = FindAttachedRagFlame(*Grenade, M_RagFlameSystem);
		if (Grenade->IsHidden() || not IsValid(RagFlame))
		{
			continue;
		}
		ASquadUnit* Holder = Cast<ASquadUnit>(Grenade->GetAttachParentActor());
		if (GetIsSoldierOfMolotovSquad(Holder))
		{
			bM_SawRagFlameInHand = true;
			M_OwnGrenades.AddUnique(Grenade);
			AimCloseCameraAt(Holder->GetActorLocation());
			// The capture renders a frame behind its aim; two pictures during the throw animation.
			++M_HoldFramesSeen;
			if (M_HoldFramesSeen == MolotovTestPrivate::FirstHoldPictureFrame ||
				M_HoldFramesSeen == MolotovTestPrivate::SecondHoldPictureFrame)
			{
				SaveCameraPicture(M_CloseCamera, FString::Printf(TEXT("Close_hold_%d"), M_HoldFramesSeen));
			}
			continue;
		}
		if (Holder == nullptr && M_OwnGrenades.Contains(Grenade))
		{
			bM_SawRagFlameInFlight = true;
			if (not bM_HasSavedFlightPicture)
			{
				bM_HasSavedFlightPicture = true;
				AimGameCameraAt(Grenade->GetActorLocation());
				SaveCameraPicture(M_GameCamera, TEXT("Game_flight"));
			}
		}
	}
}

UNiagaraComponent* UMolotovTestSubsystem::FindAttachedRagFlame(const AGrenadeActor& Grenade,
                                                               const UNiagaraSystem* RagFlameSystem)
{
	TArray<UNiagaraComponent*> NiagaraComponents;
	Grenade.GetComponents(NiagaraComponents);
	for (UNiagaraComponent* NiagaraComponent : NiagaraComponents)
	{
		if (IsValid(NiagaraComponent) && NiagaraComponent->IsActive() && NiagaraComponent->GetAsset() == RagFlameSystem)
		{
			return NiagaraComponent;
		}
	}
	return nullptr;
}

UNiagaraComponent* UMolotovTestSubsystem::FindActiveImpactComponent() const
{
	for (TObjectIterator<UNiagaraComponent> It; It; ++It)
	{
		UNiagaraComponent* NiagaraComponent = *It;
		const bool bIsActiveImpact = IsValid(NiagaraComponent) && NiagaraComponent->GetWorld() == GetWorld() &&
			NiagaraComponent->GetAsset() == M_ImpactSystem && NiagaraComponent->IsActive();
		if (bIsActiveImpact && FVector::Dist(NiagaraComponent->GetComponentLocation(), M_ImpactLocation) <=
			MolotovTestPrivate::OwnImpactMaxDistance)
		{
			return NiagaraComponent;
		}
	}
	return nullptr;
}

bool UMolotovTestSubsystem::GetIsSoldierOfMolotovSquad(ASquadUnit* Soldier) const
{
	return IsValid(Soldier) && M_MolotovSquad.IsValid() && M_MolotovSquad->GetSquadUnitsChecked().Contains(Soldier);
}

FVector UMolotovTestSubsystem::GetTargetSquadCentre() const
{
	if (not M_TargetSquad.IsValid())
	{
		return M_TestLocation;
	}
	FVector Sum = FVector::ZeroVector;
	int32 LivingSoldiers = 0;
	for (ASquadUnit* Soldier : M_TargetSquad->GetSquadUnitsChecked())
	{
		if (MolotovTestPrivate::GetIsLivingSoldier(Soldier))
		{
			Sum += Soldier->GetActorLocation();
			++LivingSoldiers;
		}
	}
	return LivingSoldiers > 0 ? Sum / LivingSoldiers : M_TargetSquad->GetActorLocation();
}

float UMolotovTestSubsystem::GetTargetSquadHealth() const
{
	if (not M_TargetSquad.IsValid())
	{
		return 0.f;
	}
	float TotalHealth = 0.f;
	for (ASquadUnit* Soldier : M_TargetSquad->GetSquadUnitsChecked())
	{
		const UHealthComponent* SoldierHealth = IsValid(Soldier) ? Soldier->GetHealthComponent() : nullptr;
		TotalHealth += IsValid(SoldierHealth) ? SoldierHealth->GetCurrentHealth() : 0.f;
	}
	return TotalHealth;
}

void UMolotovTestSubsystem::LogTargetSoldiers(const TCHAR* Moment) const
{
	if (not M_TargetSquad.IsValid())
	{
		return;
	}
	for (ASquadUnit* Soldier : M_TargetSquad->GetSquadUnitsChecked())
	{
		if (not IsValid(Soldier))
		{
			continue;
		}
		const UHealthComponent* SoldierHealth = Soldier->GetHealthComponent();
		UE_LOG(LogRTSMolotovTest, Display, TEXT("RTS_MOLOTOV_TEST %s target soldier %s owner=%d distance=%.0f health=%.0f"),
		       Moment, *Soldier->GetName(), Soldier->GetOwningPlayer(),
		       FVector::Dist2D(Soldier->GetActorLocation(), M_ImpactLocation),
		       IsValid(SoldierHealth) ? SoldierHealth->GetCurrentHealth() : -1.f);
	}
}

void UMolotovTestSubsystem::SilenceMolotovSquad() const
{
	if (not M_MolotovSquad.IsValid())
	{
		return;
	}
	for (ASquadUnit* Soldier : M_MolotovSquad->GetSquadUnitsChecked())
	{
		if (MolotovTestPrivate::GetIsLivingSoldier(Soldier) && Soldier->GetIsValidWeapon())
		{
			Soldier->GetInfantryWeapon()->DisableWeaponSearch(false, false);
		}
	}
}

void UMolotovTestSubsystem::TickBurning()
{
	using namespace MolotovTestPrivate;
	WatchThrownMolotovs();
	SilenceMolotovSquad();
	AimGameCameraAt(M_ImpactLocation);
	const float SecondsSinceImpact = GetGameSecondsInPhase();
	if (M_NextBurnPictureIndex < BurnPictureCount &&
		SecondsSinceImpact >= BurnPictureSeconds[M_NextBurnPictureIndex])
	{
		SaveCameraPicture(M_GameCamera, FString::Printf(TEXT("Game_t%04.1f"), BurnPictureSeconds[M_NextBurnPictureIndex]));
		++M_NextBurnPictureIndex;
	}
	if (M_TargetHealthAfterImpact < 0.f && SecondsSinceImpact >= AfterImpactDamageSeconds)
	{
		M_TargetHealthAfterImpact = GetTargetSquadHealth();
	}
	if (SecondsSinceImpact < EffectGoneCheckSeconds)
	{
		if (SecondsSinceImpact >= BurnEndCheckSeconds && M_ImpactComponent.IsValid() && M_ImpactComponent->IsActive())
		{
			// Logged once per frame until the burn ends; the final check below decides.
			UE_LOG(LogRTSMolotovTest, Verbose, TEXT("RTS_MOLOTOV_TEST impact effect still active at %.1f s"),
			       SecondsSinceImpact);
		}
		return;
	}
	const float TargetHealthAtEnd = GetTargetSquadHealth();
	LogTargetSoldiers(TEXT("end"));
	Check(bM_SawRagFlameInHand, TEXT("the rag burns in the thrower's hand"));
	Check(bM_SawRagFlameInFlight, TEXT("the rag burns while the bottle flies"));
	Check(TargetHealthAtEnd < M_TargetHealthAfterImpact, TEXT("the fire keeps hurting the target after the impact"),
	      FString::Printf(TEXT("health after impact=%.0f at end=%.0f"), M_TargetHealthAfterImpact, TargetHealthAtEnd));
	const bool bIsImpactEffectOver = not M_ImpactComponent.IsValid() || not M_ImpactComponent->IsActive();
	Check(bIsImpactEffectOver, TEXT("the impact effect is over once the burn ended"));
	Finish();
}

#else

bool UMolotovTestSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return false;
}

void UMolotovTestSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
}

void UMolotovTestSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

void UMolotovTestSubsystem::Tick(float DeltaTime)
{
}

TStatId UMolotovTestSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMolotovTestSubsystem, STATGROUP_Tickables);
}

#endif
