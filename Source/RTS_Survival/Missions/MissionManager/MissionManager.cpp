#include "MissionManager.h"

#include "Kismet/GameplayStatics.h"
#include "RTS_Survival/Missions/MissionClasses/MissionBase/MissionBase.h"
#include "RTS_Survival/Missions/MissionTrigger/MissionTrigger.h"
#include "RTS_Survival/Missions/MissionWidgets/W_Mission.h"
#include "RTS_Survival/Missions/MissionWidgets/MissionWidgetManager/W_MissionWidgetManager.h"
#include "RTS_Survival/Player/CPPController.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"
#include "RTS_Survival/Utils/RTS_Statics/RTS_Statics.h"
#include "Sound/SoundCue.h"

AMissionManager::AMissionManager()
{
	// Set to true to tick missions.
	PrimaryActorTick.bCanEverTick = true;
}

void AMissionManager::OnAnyMissionCompleted(UMissionBase* CompletedMission, const bool bPlaySound)
{
	RemoveActiveMission(CompletedMission);
	if (bPlaySound)
	{
		PlayMissionSound(EMissionSoundType::MissionCompleted);
	}
}

void AMissionManager::OnAnyMissionStarted(UMissionBase* LoadedMission)
{
	PlayMissionSound(EMissionSoundType::MissionLoaded);
}

void AMissionManager::OnAnyMissionFailed(UMissionBase* FailedMission)
{
	RemoveActiveMission(FailedMission);
	PlayMissionSound(EMissionSoundType::MissionFailed);
}

UW_Mission* AMissionManager::OnMissionRequestSetupWidget(UMissionBase* RequestingMission)
{
	if (not EnsureMissionIsValid(RequestingMission) || not EnsureMissionWidgetIsValid())
	{
		return nullptr;
	}
	if (not M_ActiveMissions.Contains(RequestingMission))
	{
		RTSFunctionLibrary::ReportError("A mission requested for a widget to be setup for it but this mission is not"
			"registered as an active mission on the manager."
			"\n Mission : " + RequestingMission->GetName() +
			"\n See function : OnMissionRequestSetupWidget");
		return nullptr;
	}
	UW_Mission* MissionWidget = M_MissionWidgetManager->GetFreeMissionWidget();
	if (MissionWidget)
	{
		return MissionWidget;
	}
	RTSFunctionLibrary::ReportError("A mission requested a widget but could not get a free one"
		"\n Mission : " + RequestingMission->GetName());
	return nullptr;
}

void AMissionManager::PlaySound2DForMission(USoundBase* SoundToPlay) const
{
	UWorld* World = GetWorld();
	if(not World)
	{
		return;
	}
	UGameplayStatics::PlaySound2D(World, SoundToPlay, 1, 1, 0);
	                              
}

void AMissionManager::ActivateNewMission(UMissionBase* NewMission)
{
	if (not EnsureMissionIsValid(NewMission))
	{
		return;
	}
	M_ActiveMissions.Add(NewMission);
	NewMission->LoadMission(this);
}

void AMissionManager::BeginPlay()
{
	Super::BeginPlay();
	BeginPlay_InitPlayerController();
	BeginPlay_InitMissionWidgetManager();

	// Start all configured missions.
	for (UMissionBase* Mission : Missions)
	{
		ActivateNewMission(Mission);
	}
	Missions.Empty();
}

void AMissionManager::InitMissionSounds(const FMissionSoundSettings MissionSettings)
{
	M_MissionSoundSettings = MissionSettings;
}

void AMissionManager::SetMissionManagerWidgetClass(TSubclassOf<UW_MissionWidgetManager> WidgetClass)
{
	M_WidgetManagerClass = WidgetClass;
}

void AMissionManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (const auto Mission : Missions)
	{
		if (not EnsureMissionIsValid(Mission))
		{
			continue;
		}
		Mission->OnCleanUpMission();
	}
	Super::EndPlay(EndPlayReason);
}

void AMissionManager::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    for (int32 i = 0; i < M_ActiveMissions.Num(); ++i)
    {
        UMissionBase* Mission = M_ActiveMissions[i];
        if (!EnsureMissionIsValid(Mission))
        {
        	RTSFunctionLibrary::ReportError("Invalid mission in mission manager");
            continue;
        }
        if (Mission->GetCanCheckTrigger())
        {
            Mission->MissionTrigger->TickTrigger();    
        }
        if (Mission->GetCanTickMission())
        {
            Mission->TickMission(DeltaSeconds);
        }
    }
	
}
void AMissionManager::InitMissionManagerWidget()
{
	// Create widget class and add to viewport.
	M_MissionWidgetManager = CreateWidget<UW_MissionWidgetManager>(
		M_PlayerController.Get(), M_WidgetManagerClass);
	if (not IsValid(M_MissionWidgetManager))
	{
		OnCouldNotInitWidgetManager();
		return;
	}
	M_MissionWidgetManager->AddToViewport(10);
	ProvideMissionWidgetToMainGameUI();
}

void AMissionManager::OnCouldNotInitWidgetManager() const
{
	RTSFunctionLibrary::ReportError("At InitMissionManagerWidget the mission widget manager could not be initialized."
		"\n Reported by mission manager."
		"\n See AMissionManager::InitMissionManagerWidget");
}

void AMissionManager::BeginPlay_InitPlayerController()
{
	ACPPController* PlayerController = FRTS_Statics::GetRTSController(this);
	if (not PlayerController)
	{
		RTSFunctionLibrary::ReportError("Could not get player controller in mission manager.");
		return;
	}
	M_PlayerController = PlayerController;
}

void AMissionManager::BeginPlay_InitMissionWidgetManager()
{
	if (not EnsureValidPlayerController())
	{
		return;
	}
	InitMissionManagerWidget();
}

bool AMissionManager::EnsureValidPlayerController() const
{
	if (M_PlayerController.IsValid())
	{
		return true;
	}
	RTSFunctionLibrary::ReportError("Player controller is not valid in mission manager.");
	return false;
}

void AMissionManager::RemoveActiveMission(UMissionBase* Mission)
{
	if (not EnsureMissionIsValid(Mission))
	{
		return;
	}
	if (not M_ActiveMissions.Contains(Mission))
	{
		RTSFunctionLibrary::ReportError(
			"Attempted to remove active mission that is not registered as active on the manager"
			"\n Mission : " + Mission->GetName());
	}
	M_ActiveMissions.Remove(Mission);
}

bool AMissionManager::EnsureMissionIsValid(UMissionBase* Mission)
{
	if (not IsValid(Mission))
	{
		RTSFunctionLibrary::ReportError("A mission is not valid for the mission manager.");
		return false;
	}
	return true;
}

bool AMissionManager::EnsureMissionWidgetIsValid() const
{
	if (not IsValid(M_MissionWidgetManager))
	{
		RTSFunctionLibrary::ReportError("Mission widget manager is not valid in mission manager.");
		return false;
	}
	return true;
}

bool AMissionManager::EnsureMissionIsNotAlreadyActivated(UMissionBase* Mission)
{
	if (not EnsureMissionIsValid(Mission))
	{
		return false;
	}
	if (M_ActiveMissions.Contains(Mission))
	{
		RTSFunctionLibrary::ReportError("Mission already activated.");
		return false;
	}
	return true;
}

void AMissionManager::PlayMissionSound(const EMissionSoundType SoundType)
{
	switch (SoundType)
	{
	case EMissionSoundType::None:
		return;
	case EMissionSoundType::MissionLoaded:
		PlayMissionLoaded();
		break;
	case EMissionSoundType::MissionCompleted:
		PlayMissionCompleted();
		break;
	case EMissionSoundType::MissionFailed:
		PlayMissionFailed();
		break;
	}
}

void AMissionManager::PlayMissionLoaded()
{
	USoundCue* MissionLoadedSound = M_MissionSoundSettings.MissionLoadedSound;
	if (not IsValid(MissionLoadedSound))
	{
		RTSFunctionLibrary::ReportError("NO mission loaded sound set on mission manager");
		return;
	}
	UGameplayStatics::PlaySound2D(this, MissionLoadedSound, 1, 1, 0,
	                              M_MissionSoundSettings.MissionSoundConcurrency);
}

void AMissionManager::PlayMissionCompleted()
{
	USoundCue* MissionCompletedSound = M_MissionSoundSettings.MissionCompleteSound;
	if (not IsValid(MissionCompletedSound))
	{
		RTSFunctionLibrary::ReportError("NO mission completed sound set on mission manager");
		return;
	}
	UGameplayStatics::PlaySound2D(this, MissionCompletedSound, 1, 1, 0,
	                              M_MissionSoundSettings.MissionSoundConcurrency);
}

void AMissionManager::PlayMissionFailed()
{
	USoundCue* MissionFailedSound = M_MissionSoundSettings.MissionFailedSound;
	if (not IsValid(MissionFailedSound))
	{
		RTSFunctionLibrary::ReportError("NO mission failed sound set on mission manager");
		return;
	}
	UGameplayStatics::PlaySound2D(this, MissionFailedSound, 1, 1, 0,
	                              M_MissionSoundSettings.MissionSoundConcurrency);
}

void AMissionManager::ProvideMissionWidgetToMainGameUI() const
{
	UMainGameUI* MainGameUI = FRTS_Statics::GetMainGameUI(this);
	if(not MainGameUI)
	{
		return;
	}
	MainGameUI->SetMissionManagerWidget(M_MissionWidgetManager);
}

