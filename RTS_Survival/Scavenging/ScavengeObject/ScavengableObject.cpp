#include "ScavengableObject.h"

#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/DeveloperSettings.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Player/PlayerResourceManager/PlayerResourceManager.h"
#include "RTS_Survival/Resources/ResourceTypes/ResourceTypes.h"
#include "RTS_Survival/RTSGamePlayTags/FRTSGamePlayTags.h"
#include "RTS_Survival/RTSGamePlayTags/FRTSGamePlayTagsBPLibrary.h"
#include "RTS_Survival/Scavenging/ProjectSettings/ScavengingSettings.h"
#include "RTS_Survival/Scavenging/ScavengeRewardWidget/ScavRewardWidgetComp/ScavRewardComponent.h"
#include "RTS_Survival/Units/SquadController.h"
#include "RTS_Survival/Units/RTSDeathType/RTSDeathType.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTSBlueprintFunctionLibrary.h"
#include "RTS_Survival/Utils/CollisionSetup/FRTS_CollisionSetup.h"
#include "RTS_Survival/Utils/RTSRichTextConverters/FRTSRichTextConverter.h"

AScavengeableObject::AScavengeableObject(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void AScavengeableObject::StartScavengeTimer(const TObjectPtr<ASquadController> ScavengingSquadController)
{
	if (not StartScavTimer_EnsureAliveScavObj(ScavengingSquadController))
	{
		return;
	}
	if (GetWorld()->GetTimerManager().IsTimerActive(M_ScavengeTimerHandle))
	{
		// Timer is already active, do nothing
		return;
	}

	// Save the scavenging squad
	M_ScavengingSquad = ScavengingSquadController;
	// Calculate remaining scavenge time
	if (M_ScavengeTimeLeft <= 0.0f)
	{
		M_ScavengeTimeLeft = ScavengeTime;
	}

	// Start the scavenging timer
	GetWorld()->GetTimerManager().SetTimer(M_ScavengeTimerHandle, this, &AScavengeableObject::OnScavengingComplete,
	                                       M_ScavengeTimeLeft, false);

	// Record the start time
	M_ScavengeStartTime = GetWorld()->GetTimeSeconds();

	// Start the progress bar
	if (EnsureIsValidProgressBarWidget())
	{
		// Initialize the progress bar widget
		// Assuming the MeshComp is the progress bar's mesh component
		M_ProgressBarWidget->StartProgressBar(M_ScavengeTimeLeft);
	}
}

void AScavengeableObject::PauseScavenging(const TObjectPtr<ASquadController>& RequestingSquadController)
{
	if (not GetWorld())
	{
		return;
	}

	// Only the active scavenging squad may pause.
	if (!EnsureRequestingSquadIsActiveScavenger(RequestingSquadController))
	{
		if (bM_IsScavengeEnabled) { return; } // already available; nothing to do
		const FString Scavenger = IsValid(M_ScavengingSquad) ? M_ScavengingSquad->GetName() : "Invalid";
		const FString Requester = IsValid(RequestingSquadController) ? RequestingSquadController->GetName() : "Invalid";
		RTSFunctionLibrary::ReportError("PauseScavenging requested by non-active squad.\n Scavenging: " + Scavenger +
			"\n Requesting: " + Requester + "\n Object: " + GetName());
		return;
	}

	// If object is dead or no timer is running, nothing to pause.
	if (!IsUnitAlive() || !GetWorld()->GetTimerManager().IsTimerActive(M_ScavengeTimerHandle))
	{
		return;
	}
	// Pause the scavenging timer
	if (GetWorld()->GetTimerManager().IsTimerActive(M_ScavengeTimerHandle))
	{
		// Calculate the remaining time
		M_ScavengeTimeLeft -= (GetWorld()->GetTimeSeconds() - M_ScavengeStartTime);

		GetWorld()->GetTimerManager().ClearTimer(M_ScavengeTimerHandle);
	}

	// Re-enable the scavengeable object
	SetIsScavengeEnabled(true);

	// Pause the progress bar
	if (IsValid(M_ProgressBarWidget))
	{
		constexpr bool bHideBarWhilePaused = true;
		M_ProgressBarWidget->PauseProgressBar(bHideBarWhilePaused);
	}

	// Reset the scavenging squad
	M_ScavengingSquad = nullptr;
}


float AScavengeableObject::GetScavengeTimeLeft() const
{
	// Get the time left for scavenging to complete
	if (GetWorld()->GetTimerManager().IsTimerActive(M_ScavengeTimerHandle))
	{
		return GetWorld()->GetTimerManager().GetTimerRemaining(M_ScavengeTimerHandle);
	}
	// Timer is not active, this is the time left after pausing.
	return M_ScavengeTimeLeft;
}

bool AScavengeableObject::EnsureIsValidProgressBarWidget() const
{
	if (not IsValid(M_ProgressBarWidget))
	{
		RTSFunctionLibrary::ReportNullErrorComponent(this, "ProgressBarWidget", "EnsureIsValidProgressBarWidget");
		return false;
	}
	return true;
}

bool AScavengeableObject::StartScavTimer_EnsureAliveScavObj(const TObjectPtr<ASquadController>& ScavengingSquad)
{
	if (IsUnitAlive())
	{
		return true;
	}
	const FString SquadName = IsValid(ScavengingSquad) ? ScavengingSquad->GetName() : "Invalid";
	RTSFunctionLibrary::ReportError("Squad attempted to start scavenging on a dead scavengable object!"
		"\n terminating action."
		"\n Scavenging squad: " + SquadName);
	if (IsValid(ScavengingSquad))
	{
		// Sets scav object to active again.
		ScavengingSquad->OnScavengingComplete();
		bM_IsScavengeEnabled = false;
	}
	return false;
}

bool AScavengeableObject::EnsureRequestingSquadIsActiveScavenger(
	const TObjectPtr<ASquadController>& RequestingSquadController) const
{
	if (IsValid(M_ScavengingSquad) && M_ScavengingSquad == RequestingSquadController)
	{
		return true;
	}
	return false;
}

void AScavengeableObject::OnScavengingComplete()
{
	MakeScavengingSquadStopScavenging_AndDisableScavObj();
	DisableProgressBarAndTimers();

	if (not IsUnitAlive())
	{
		RTSFunctionLibrary::ReportError("Scavenging completed on dead scav object! "
			"\n Stopping scavenging squad without giving rewards.");
		return;
	}

	// Player obtains scavenging resources.
	RandomRewardPlayer();

	AsyncLoadAndCreateRewardWidget();


	// Trigger destruction in ADestructibleActor with BP_OnUnitDies event for collapse mesh or swap mesh logic.
	// Sets unit's bIsAlive=false;
	TriggerDestroyActor(ERTSDeathType::Scavenging);
}

void AScavengeableObject::DisableProgressBarAndTimers()
{
	// Stop the progress bar
	if (IsValid(M_ProgressBarWidget))
	{
		M_ProgressBarWidget->StopProgressBar();
	}

	// Clear the timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_ScavengeTimerHandle);
	}
}

void AScavengeableObject::OnRewardWidgetClassLoaded()
{
	CreateRewardWidgetComponent();
}

UStaticMeshComponent* AScavengeableObject::FindScavengeMeshComponent() const
{
	// Prefer explicit tagging over “first mesh” heuristics.
	TArray<UStaticMeshComponent*> StaticMeshComponents;
	GetComponents<UStaticMeshComponent>(StaticMeshComponents);

	for (UStaticMeshComponent* const Candidate : StaticMeshComponents)
	{
		if (IsValid(Candidate) && FRTSGamePlayTagsHelpers::DoesComponentHaveScavengeTag(Candidate))
		{
			return Candidate;
		}
	}

	return nullptr;
}

void AScavengeableObject::ExtractScavengeSocketNames(
	const UStaticMeshComponent& InMeshComponent,
	TArray<FName>& OutSocketNames) const
{
	OutSocketNames.Reset();

	const UStaticMesh* const StaticMesh = InMeshComponent.GetStaticMesh();
	if (!IsValid(StaticMesh))
	{
		return;
	}

	// Access mesh's sockets directly
	const TArray<TObjectPtr<UStaticMeshSocket>>& MeshSockets = StaticMesh->Sockets;

	constexpr TCHAR SearchSubstring[] = TEXT("scav");

	for (const TObjectPtr<UStaticMeshSocket>& Socket : MeshSockets)
	{
		if (!IsValid(Socket))
		{
			continue;
		}

		const FString SocketString = Socket->SocketName.ToString();

		// Case-insensitive substring search
		if (SocketString.Contains(SearchSubstring, ESearchCase::IgnoreCase, ESearchDir::FromStart))
		{
			OutSocketNames.Add(Socket->SocketName);
		}
	}
}


bool AScavengeableObject::GetIsValidMeshComp(const FString& InContext) const
{
	if (IsValid(M_ScavengeMeshComp))
	{
		return true;
	}

	const FGameplayTag ScavengeTag = FRTSGamePlayTags::GetScavengeMeshComponentTag();

	RTSFunctionLibrary::ReportError(
		FString::Printf(
			TEXT("Missing Scavenge Mesh Component (tagged '%s') in %s"),
			*ScavengeTag.ToString(),
			*InContext
		)
	);

	return false;
}


void AScavengeableObject::BeginPlay_LoadSoundSettings()
{
	if (const UScavengingSettings* Settings = UScavengingSettings::Get())
	{
		M_ScavRewardSoundSettings.Attenuation = Settings->ScavRewardAttenuation;
		M_ScavRewardSoundSettings.Concurrency = Settings->ScavRewardConcurrency;
		M_ScavRewardSoundSettings.BasicRewardSound = Settings->ScavRewardSound;
	}
}

void AScavengeableObject::PlayRewardSound() const
{
	USoundBase* SoundToPlay = IsValid(RewardSoundOverride)
		                          ? RewardSoundOverride
		                          : M_ScavRewardSoundSettings.BasicRewardSound;
	// Play one-shot
	if (IsValid(SoundToPlay))
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			SoundToPlay,
			GetActorLocation() + FVector(0.0, 0.0, RewardWidgetHeightOffset),
			1.f, // VolumeMultiplier
			1.f, // PitchMultiplier
			0.f, // StartTime
			M_ScavRewardSoundSettings.Attenuation,
			M_ScavRewardSoundSettings.Concurrency,
			nullptr // OwningActor
		);
	}
}

void AScavengeableObject::CreateRewardWidgetComponent()
{
	if (RewardComponentWidgetClass.IsValid())
	{
		// Create the widget component
		UScavRewardComponent* RewardComponent = NewObject<UScavRewardComponent>(this, RewardComponentWidgetClass.Get());
		if (RewardComponent)
		{
			RewardComponent->RegisterComponent();
			RewardComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
			RewardComponent->SetRelativeLocation(FVector(0.f, 0.f, RewardWidgetHeightOffset));

			// Set the reward text
			const FString RewardText = GetRewardText();
			RewardComponent->SetRewardText(FText::FromString(RewardText));

			if (DeveloperSettings::Debugging::GScavenging_Compile_DebugSymbols)
			{
				RTSFunctionLibrary::PrintString(RewardText, FColor::White);
			}

			// Set durations on the component
			RewardComponent->SetVisibleDuration(VisibleTimeRewardWidget);
			RewardComponent->SetFadeDuration(FadeTimeRewardWidget);

			RewardComponent->StartDisplay();
		}
	}
	else
	{
		RTSFunctionLibrary::ReportError("RewardWidgetClass failed to load in CreateRewardWidgetComponent");
	}
}

FString AScavengeableObject::GetRewardText()
{
	FString RewardText;
	for (const auto Reward : M_PlayerResourceAmountRewards)
	{
		FString ResourceDisplayName = Global_GetResourceTypeDisplayString(Reward.Key);
		// Obtain the resource display text and styling.
		ResourceDisplayName = FRTSRichTextConverter::MakeStringRTSResource(
			ResourceDisplayName, FRTSRichTextConverter::ConvertResourceToRichTextType(Reward.Key));

		// Obtain the amount text and styling.
		FString Amount = FRTSRichTextConverter::MakeStringRTSResource("+" + FString::FromInt(Reward.Value),
		                                                              ERTSResourceRichText::Red);

		// Obtain the resource image.
		FString ResourceImage = FRTSRichTextConverter::GetResourceRichImage(Reward.Key);

		RewardText += "Obtained " + ResourceDisplayName + " " + Amount + " " + ResourceImage + "\n";
	}
	return RewardText;
}


void AScavengeableObject::RandomRewardPlayer()
{
	if (not GetIsValidPlayerController() || not M_PlayerController->GetPlayerResourceManager())
	{
		RTSFunctionLibrary::ReportError("Scavengable Object is not able to provide player with reward as either"
			"the player controller or the player resource manager is null!"
			"\n for object: " + GetName());
		return;
	}
	TArray<FString> RewardTexts;
	UPlayerResourceManager* PlayerResourceManager = M_PlayerController->GetPlayerResourceManager();
	bool bDidGetAnyReward = false;
	for (auto& EachReward : ScavengeResources)
	{
		const ERTSResourceType ResourceType = EachReward.Key;
		FScavengeAmount& RewardInfo = EachReward.Value;

		int32 Reward = RewardInfo.bIsSetAmount
			               ? RewardInfo.SetAmount
			               : FMath::RandRange(RewardInfo.Min, RewardInfo.Max);

		// Check if the player will get the reward.
		const bool GetReward = RewardInfo.Chance == 100 || FMath::RandRange(0, 100) <= RewardInfo.Chance;
		if (not GetReward)
		{
			continue;
		}
		bDidGetAnyReward = true;
		PlayerResourceManager->AddResource(ResourceType, Reward);
		// fill in map.
		M_PlayerResourceAmountRewards.Add(ResourceType, Reward);

		if (DeveloperSettings::Debugging::GScavenging_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString("\n Player random reward: ", FColor::White);
			RTSFunctionLibrary::PrintString("Reward: " + FString::FromInt(Reward), FColor::Red);
			RTSFunctionLibrary::PrintString("ResourceType: " + Global_GetResourceTypeAsString(ResourceType),
			                                FColor::Blue);
			RTSFunctionLibrary::PrintString("-----------------------------\n", FColor::White);
		}
	}
	if (bDidGetAnyReward)
	{
		PlayRewardSound();
	}
}

void AScavengeableObject::AsyncLoadAndCreateRewardWidget()
{
	// Create the reward widget component
	if (RewardComponentWidgetClass.IsNull())
	{
		RTSFunctionLibrary::ReportError("RewardWidgetClass is null in AScavengeableObject::OnScavengingComplete");
		return;
	}
	// Ensure the class is loaded
	if (RewardComponentWidgetClass.IsValid())
	{
		CreateRewardWidgetComponent();
	}
	else
	{
		// Asynchronously load the class
		FStreamableManager& StreamableManager = UAssetManager::GetStreamableManager();
		StreamableManager.RequestAsyncLoad(RewardComponentWidgetClass.ToSoftObjectPath(),
		                                   FStreamableDelegate::CreateUObject(
			                                   this, &AScavengeableObject::OnRewardWidgetClassLoaded));
	}
}


bool AScavengeableObject::GetCanScavenge() const
{
	return bM_IsScavengeEnabled;
}

FVector AScavengeableObject::GetScavLocationClosestToPosition(const FVector& Position) const
{
	float MinDistance = TNumericLimits<float>::Max();
	FVector ClosestLocation = GetActorLocation();
	for (const auto EachSocket : M_ScavengeSocketNames)
	{
		if (M_ScavengeMeshComp->DoesSocketExist(EachSocket))
		{
			const FVector SocketLocation = M_ScavengeMeshComp->GetSocketLocation(EachSocket);
			const float Distance = FVector::Dist(Position, SocketLocation);
			if (Distance < MinDistance)
			{
				MinDistance = Distance;
				ClosestLocation = SocketLocation;
			}
		}
	}
	return ClosestLocation;
}

void AScavengeableObject::SetIsScavengeEnabled(const bool bEnabled)
{
	bM_IsScavengeEnabled = bEnabled;
}

TArray<FVector> AScavengeableObject::GetScavengePositions(const int32 NumUnits)
{
	TArray<FVector> ScavengeLocations;

	if (!IsValid(M_ScavengeMeshComp))
	{
		RTSFunctionLibrary::ReportError("Invalid mesh component in AScavengeableObject::GetScavengePositions");
		return {GetActorLocation()};
	}

	bool bSocketsFound = true;

	// Attempt to get socket locations
	for (const FName& SocketName : M_ScavengeSocketNames)
	{
		if (M_ScavengeMeshComp->DoesSocketExist(SocketName))
		{
			FVector SocketLocation = M_ScavengeMeshComp->GetSocketLocation(SocketName);
			ScavengeLocations.Add(SocketLocation);
		}
		else
		{
			RTSFunctionLibrary::ReportError("Socket " + SocketName.ToString() + " not found on mesh.");
			bSocketsFound = false;
			break;
		}
	}

	// If no valid sockets are found or sockets are empty, create default positions
	if (!bSocketsFound || ScavengeLocations.Num() == 0)
	{
		if (DeveloperSettings::Debugging::GScavenging_Compile_DebugSymbols)
		{
			RTSFunctionLibrary::PrintString(
				"Could not find sockets on scavengable object. Generating default positions."
				"\n for object: " + GetName());
		}
		const FVector ActorLocation = GetActorLocation();
		constexpr float Offset = 200.0f;

		ScavengeLocations.Add(ActorLocation + FVector(Offset, Offset, 0.0f));
		ScavengeLocations.Add(ActorLocation + FVector(-Offset, Offset, 0.0f));
		ScavengeLocations.Add(ActorLocation + FVector(-Offset, -Offset, 0.0f));
		ScavengeLocations.Add(ActorLocation + FVector(Offset, -Offset, 0.0f));
	}

	// Generate additional positions if needed
	if (ScavengeLocations.Num() < NumUnits)
	{
		GenerateAdditionalPositions(ScavengeLocations, NumUnits);
	}

	return ScavengeLocations;
}

bool AScavengeableObject::GetIsValidPlayerController()
{
	if (IsValid(M_PlayerController))
	{
		return true;
	}
	M_PlayerController = Cast<ACPPController>(GetWorld()->GetFirstPlayerController());
	if (IsValid(M_PlayerController))
	{
		return true;
	}
	RTSFunctionLibrary::ReportError(
		"Player controller is not valid in AScavengeableObject::GetIsValidPlayerController");
	return false;
}

void AScavengeableObject::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Don't execute on CDO (class default object) or if pending kill / invalid
	if (HasAnyFlags(RF_ClassDefaultObject) || IsPendingKillPending())
	{
		RTSFunctionLibrary::ReportError("PostInitializeComponents called on CDO or pending kill in AScavengeableObject,"
			"will not call bp event to implement the scavenger component (Static Mesh Comp)");
		return;
	}
	BP_PostInitSetScavengerComponent();
	if (not M_ScavengeMeshComp)
	{
		RTSFunctionLibrary::DisplayNotification(
			"scav object has no scav mesh set in Ctor, therefore looks for tagged mesh instead."
			"\n object: " + GetName());
		M_ScavengeMeshComp = FindScavengeMeshComponent();
	}
	if (!IsValid(M_ScavengeMeshComp))
	{
		RTSFunctionLibrary::ReportError("No mesh component found in AScavengeableObject::PostInitializeComponents");
	}
	else
	{
		// Find all sockets on the mesh and save them in ScavengeSocketNames
		ExtractScavengeSocketNames(*M_ScavengeMeshComp, M_ScavengeSocketNames);
		FRTS_CollisionSetup::SetupScavengeableObjectCollision(M_ScavengeMeshComp);
	}
}

void AScavengeableObject::BeginPlay()
{
	Super::BeginPlay();
	// get player controller.
	GetIsValidPlayerController();
	BeginPlay_LoadSoundSettings();
}

void AScavengeableObject::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_ScavengeTimerHandle);
	}
}

void AScavengeableObject::BeginDestroy()
{
	Super::BeginDestroy();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(M_ScavengeTimerHandle);
	}
}

void AScavengeableObject::SetStaticMeshCompOfNameAsScavenegeMesh(const FName MeshCompName)
{
	if (MeshCompName.IsNone())
	{
		RTSFunctionLibrary::ReportError("MeshCompName is None in SetStaticMeshCompOfNameAsScavenegeMesh");
		return;
	}
	UStaticMeshComponent* FoundMeshComp = Cast<UStaticMeshComponent>(GetDefaultSubobjectByName(MeshCompName));
	if (IsValid(FoundMeshComp))
	{
		SetScavengeMesh(FoundMeshComp);
		return;
	}
	RTSFunctionLibrary::ReportError("Could not find static mesh component with name: " + MeshCompName.ToString() +
		" in SetStaticMeshCompOfNameAsScavenegeMesh");
}

void AScavengeableObject::SetupAsDestroyedVehicle(const ETankSubtype TankSubtype)
{
	Health = URTSBlueprintFunctionLibrary::GetDestroyedTankHealth(this, TankSubtype);
	const float VehiclePartsReward =
		URTSBlueprintFunctionLibrary::GetDestroyedTankVehiclePartsRewardAndScavTime(this, TankSubtype, ScavengeTime);
	ScavengeResources.Empty();
	ScavengeResources.Add(ERTSResourceType::Resource_VehicleParts,
	                      FScavengeAmount(true, VehiclePartsReward, 0, 0, 100));
}


void AScavengeableObject::SetScavengeMesh(UStaticMeshComponent* NewMeshComp)
{
	M_ScavengeMeshComp = NewMeshComp;
}

void AScavengeableObject::InitScavObject(UTimeProgressBarWidget* NewProgressBarWidget)
{
	if (IsValid(NewProgressBarWidget))
	{
		M_ProgressBarWidget = NewProgressBarWidget;
		return;
	}
	RTSFunctionLibrary::ReportNullErrorComponent(this, "ProgressBarWidget", "InitScavObject");
}

void AScavengeableObject::OnUnitDies(const ERTSDeathType DeathType)
{
	OnUnitDies_StopScavSquadIfValid();
	OnUnitDies_StopTimersAndDisableScavObj();
	// Sets the death flag and calls BP_OnUnitDies which implements collapse mesh logic, swap logic etc.
	Super::OnUnitDies(DeathType);
}

void AScavengeableObject::OnUnitDies_StopScavSquadIfValid()
{
	if (not IsValid(M_ScavengingSquad))
	{
		return;
	}
	MakeScavengingSquadStopScavenging_AndDisableScavObj();
}

void AScavengeableObject::OnUnitDies_StopTimersAndDisableScavObj()
{
	DisableProgressBarAndTimers();
	// Ensure no other squad can target this scav object for scavenging again.
	bM_IsScavengeEnabled = false;
}

void AScavengeableObject::MakeScavengingSquadStopScavenging_AndDisableScavObj()
{
	if (IsValid(M_ScavengingSquad))
	{
		// Sets scav object to active again.
		M_ScavengingSquad->OnScavengingComplete();
		bM_IsScavengeEnabled = false;
		return;
	}
	RTSFunctionLibrary::ReportError(
		"Scavengable object has attempted to make squad stop scavenging but it is not valid!"
		"Squad died while scavenging?"
		"\n for object: " + GetName());
}

void AScavengeableObject::GenerateAdditionalPositions(TArray<FVector>& Positions, int32 NumRequired) const
{
	// Generate additional positions by interpolating between existing positions
	while (Positions.Num() < NumRequired)
	{
		int32 NumExisting = Positions.Num();
		for (int32 i = 0; i < NumExisting - 1; ++i)
		{
			FVector MidPoint = (Positions[i] + Positions[i + 1]) / 2.0f;
			Positions.Insert(MidPoint, i + 1);
			if (Positions.Num() >= NumRequired)
			{
				break;
			}
			++i; // Skip the next point since we've just inserted
		}
		if (Positions.Num() >= NumRequired)
		{
			break;
		}
		if (NumExisting == Positions.Num())
		{
			// Unable to generate more positions
			break;
		}
	}
}
