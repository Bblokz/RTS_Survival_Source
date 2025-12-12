#include "URTSMusicSubsystem.h"

#include "GameFramework/GameUserSettings.h"
#include "RTS_Survival/Game/RTSGameInstance/RTSGameInstance.h"
#include "RTS_Survival/Utils/HFunctionLibary.h"

void URTSMusicSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Grab and cache your custom GameInstance
	RTSGameInstance = Cast<URTSGameInstance>(GetGameInstance());
	if (!RTSGameInstance)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("RTSMusicSubsystem: Failed to cast GameInstance to URTSGameInstance. ")
			TEXT("Make sure your project uses URTSGameInstance as its GameInstance class.")
		);
	}

	// UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
	// if (Settings)
	// {
	// 	Settings->SetFullscreenMode(EWindowMode::Fullscreen); // Exclusive fullscreen mode  
	// 	Settings->ApplySettings(false); // Apply without saving (true to save)  
	// }
}

void URTSMusicSubsystem::Deinitialize()
{
	RTSGameInstance = nullptr;
	Super::Deinitialize();
}

URTSMusicManager* URTSMusicSubsystem::GetMusicManager() const
{
	if (!RTSGameInstance)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("RTSMusicSubsystem::GetMusicManager – GameInstance pointer is null.")
		);
		return nullptr;
	}

	URTSMusicManager* Manager = RTSGameInstance->GetMusicManager();
	if (!Manager)
	{
		RTSFunctionLibrary::ReportError(
			TEXT("RTSMusicSubsystem::GetMusicManager – MusicManager is not initialized in RTSGameInstance. ")
			TEXT("Have you called SetupMusicManager() / InitMusicManager() in your GameInstance Blueprint?")
		);
	}
	return Manager;
}
