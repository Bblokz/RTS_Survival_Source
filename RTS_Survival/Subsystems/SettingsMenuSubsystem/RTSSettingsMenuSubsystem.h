#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "RTS_Survival/Audio/Settings/RTSAudioType.h"
#include "RTS_Survival/Game/UserSettings/RTSGameUserSettings.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "RTSSettingsMenuSubsystem.generated.h"

class APlayerController;
class USoundClass;
class USoundMix;

UENUM(BlueprintType)
enum class ERTSWindowMode : uint8
{
	Fullscreen UMETA(DisplayName="Fullscreen"),
	WindowedFullscreen UMETA(DisplayName="Windowed Fullscreen"),
	Windowed UMETA(DisplayName="Windowed")
};

UENUM(BlueprintType)
enum class ERTSScalabilityQuality : uint8
{
	Low UMETA(DisplayName="Low"),
	Medium UMETA(DisplayName="Medium"),
	High UMETA(DisplayName="High"),
	Epic UMETA(DisplayName="Epic")
};

UENUM(BlueprintType)
enum class ERTSScalabilityGroup : uint8
{
	ViewDistance UMETA(DisplayName="View Distance"),
	Shadows UMETA(DisplayName="Shadows"),
	Textures UMETA(DisplayName="Textures"),
	Effects UMETA(DisplayName="Effects"),
	PostProcessing UMETA(DisplayName="Post Processing")
};

USTRUCT(BlueprintType)
struct FRTSScalabilityGroupSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Graphics")
	ERTSScalabilityQuality M_ViewDistance = ERTSScalabilityQuality::High;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Graphics")
	ERTSScalabilityQuality M_Shadows = ERTSScalabilityQuality::High;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Graphics")
	ERTSScalabilityQuality M_Textures = ERTSScalabilityQuality::High;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Graphics")
	ERTSScalabilityQuality M_Effects = ERTSScalabilityQuality::High;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Graphics")
	ERTSScalabilityQuality M_PostProcessing = ERTSScalabilityQuality::High;
};

USTRUCT(BlueprintType)
struct FRTSDisplaySettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Graphics")
	ERTSWindowMode M_WindowMode = ERTSWindowMode::WindowedFullscreen;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Graphics")
	FIntPoint M_Resolution = FIntPoint(1920, 1080);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Graphics")
	bool bM_VSyncEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Graphics")
	ERTSScalabilityQuality M_OverallQuality = ERTSScalabilityQuality::High;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Graphics")
	float M_FrameRateLimit = 0.0f;
};

USTRUCT(BlueprintType)
struct FRTSGraphicsSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Graphics")
	FRTSDisplaySettings M_DisplaySettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Graphics")
	FRTSScalabilityGroupSettings M_ScalabilityGroups;
};

USTRUCT(BlueprintType)
struct FRTSAudioSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Audio")
	float M_MasterVolume = RTSGameUserSettingsRanges::DefaultVolume;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Audio")
	float M_MusicVolume = RTSGameUserSettingsRanges::DefaultVolume;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Audio")
	float M_SfxAndWeaponsVolume = RTSGameUserSettingsRanges::DefaultVolume;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Audio")
	float M_VoicelinesVolume = RTSGameUserSettingsRanges::DefaultVolume;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Audio")
	float M_AnnouncerVolume = RTSGameUserSettingsRanges::DefaultVolume;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Audio")
	float M_UiVolume = RTSGameUserSettingsRanges::DefaultVolume;
};

USTRUCT(BlueprintType)
struct FRTSControlSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Controls")
	float M_MouseSensitivity = RTSGameUserSettingsRanges::DefaultMouseSensitivity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Controls")
	bool bM_InvertYAxis = false;
};

USTRUCT(BlueprintType)
struct FRTSGameplaySettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Gameplay")
	bool bM_HideActionButtonHotkeys = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Gameplay")
	ERTSPlayerHealthBarVisibilityStrategy M_OverwriteAllPlayerHpBarStrat = ERTSPlayerHealthBarVisibilityStrategy::NotInitialized;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Gameplay")
	ERTSPlayerHealthBarVisibilityStrategy M_PlayerTankHpBarStrat = ERTSPlayerHealthBarVisibilityStrategy::UnitDefaults;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Gameplay")
	ERTSPlayerHealthBarVisibilityStrategy M_PlayerSquadHpBarStrat = ERTSPlayerHealthBarVisibilityStrategy::UnitDefaults;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Gameplay")
	ERTSPlayerHealthBarVisibilityStrategy M_PlayerNomadicHpBarStrat = ERTSPlayerHealthBarVisibilityStrategy::UnitDefaults;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Gameplay")
	ERTSPlayerHealthBarVisibilityStrategy M_PlayerBxpHpBarStrat = ERTSPlayerHealthBarVisibilityStrategy::UnitDefaults;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Gameplay")
	ERTSPlayerHealthBarVisibilityStrategy M_PlayerAircraftHpBarStrat = ERTSPlayerHealthBarVisibilityStrategy::UnitDefaults;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Gameplay")
	ERTSEnemyHealthBarVisibilityStrategy M_OverwriteAllEnemyHpBarStrat = ERTSEnemyHealthBarVisibilityStrategy::NotInitialized;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Gameplay")
	ERTSEnemyHealthBarVisibilityStrategy M_EnemyTankHpBarStrat = ERTSEnemyHealthBarVisibilityStrategy::UnitDefaults;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Gameplay")
	ERTSEnemyHealthBarVisibilityStrategy M_EnemySquadHpBarStrat = ERTSEnemyHealthBarVisibilityStrategy::UnitDefaults;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Gameplay")
	ERTSEnemyHealthBarVisibilityStrategy M_EnemyNomadicHpBarStrat = ERTSEnemyHealthBarVisibilityStrategy::UnitDefaults;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Gameplay")
	ERTSEnemyHealthBarVisibilityStrategy M_EnemyBxpHpBarStrat = ERTSEnemyHealthBarVisibilityStrategy::UnitDefaults;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings|Gameplay")
	ERTSEnemyHealthBarVisibilityStrategy M_EnemyAircraftHpBarStrat = ERTSEnemyHealthBarVisibilityStrategy::UnitDefaults;
};

USTRUCT(BlueprintType)
struct FRTSSettingsSnapshot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings")
	FRTSGraphicsSettings M_GraphicsSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings")
	FRTSAudioSettings M_AudioSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings")
	FRTSControlSettings M_ControlSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Settings")
	FRTSGameplaySettings M_GameplaySettings;
};

/**
 * @brief Central settings manager that widgets call to stage, apply, save, and revert runtime settings changes.
 *
 * The subsystem owns the pending settings state, applies it through UGameUserSettings, and keeps UI logic decoupled.
 */
UCLASS()
class RTS_SURVIVAL_API URTSSettingsMenuSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category="Settings")
	void LoadSettings();

	UFUNCTION(BlueprintCallable, Category="Settings")
	bool GetCurrentSettingsValues(FRTSSettingsSnapshot& OutSettingsValues) const;

	UFUNCTION(BlueprintCallable, Category="Settings")
	bool GetPendingSettingsValues(FRTSSettingsSnapshot& OutSettingsValues) const;

	UFUNCTION(BlueprintCallable, Category="Settings")
	TArray<FIntPoint> GetSupportedResolutions() const;

	UFUNCTION(BlueprintCallable, Category="Settings")
	TArray<FString> GetSupportedResolutionLabels() const;

	UFUNCTION(BlueprintCallable, Category="Settings")
	void SetPendingResolution(const FIntPoint& NewResolution);

	UFUNCTION(BlueprintCallable, Category="Settings")
	void SetPendingWindowMode(ERTSWindowMode NewWindowMode);

	UFUNCTION(BlueprintCallable, Category="Settings")
	void SetPendingVSyncEnabled(bool bNewVSyncEnabled);

	UFUNCTION(BlueprintCallable, Category="Settings")
	void SetPendingGraphicsQuality(ERTSScalabilityQuality NewGraphicsQuality);

	UFUNCTION(BlueprintCallable, Category="Settings")
	void SetPendingScalabilityGroupQuality(ERTSScalabilityGroup ScalabilityGroup, ERTSScalabilityQuality NewQuality);

	UFUNCTION(BlueprintCallable, Category="Settings")
	void SetPendingFrameRateLimit(float NewFrameRateLimit);

	UFUNCTION(BlueprintCallable, Category="Settings")
	void SetPendingMasterVolume(float NewVolume);

	UFUNCTION(BlueprintCallable, Category="Settings")
	void SetPendingAudioVolume(ERTSAudioType AudioType, float NewVolume);

	UFUNCTION(BlueprintCallable, Category="Settings")
	void SetPendingMouseSensitivity(float NewMouseSensitivity);

	UFUNCTION(BlueprintCallable, Category="Settings")
	void SetPendingInvertYAxis(bool bNewInvertYAxis);

	UFUNCTION(BlueprintCallable, Category="Settings")
	void SetPendingHideActionButtonHotkeys(bool bNewHideActionButtonHotkeys);

	UFUNCTION(BlueprintCallable, Category="Settings")
	void SetPendingOverwriteAllPlayerHpBarStrat(ERTSPlayerHealthBarVisibilityStrategy NewStrategy);

	UFUNCTION(BlueprintCallable, Category="Settings")
	void SetPendingPlayerTankHpBarStrat(ERTSPlayerHealthBarVisibilityStrategy NewStrategy);

	UFUNCTION(BlueprintCallable, Category="Settings")
	void SetPendingPlayerSquadHpBarStrat(ERTSPlayerHealthBarVisibilityStrategy NewStrategy);

	UFUNCTION(BlueprintCallable, Category="Settings")
	void SetPendingPlayerNomadicHpBarStrat(ERTSPlayerHealthBarVisibilityStrategy NewStrategy);

	UFUNCTION(BlueprintCallable, Category="Settings")
	void SetPendingPlayerBxpHpBarStrat(ERTSPlayerHealthBarVisibilityStrategy NewStrategy);

	UFUNCTION(BlueprintCallable, Category="Settings")
	void SetPendingPlayerAircraftHpBarStrat(ERTSPlayerHealthBarVisibilityStrategy NewStrategy);

	UFUNCTION(BlueprintCallable, Category="Settings")
	void SetPendingOverwriteAllEnemyHpBarStrat(ERTSEnemyHealthBarVisibilityStrategy NewStrategy);

	UFUNCTION(BlueprintCallable, Category="Settings")
	void SetPendingEnemyTankHpBarStrat(ERTSEnemyHealthBarVisibilityStrategy NewStrategy);

	UFUNCTION(BlueprintCallable, Category="Settings")
	void SetPendingEnemySquadHpBarStrat(ERTSEnemyHealthBarVisibilityStrategy NewStrategy);

	UFUNCTION(BlueprintCallable, Category="Settings")
	void SetPendingEnemyNomadicHpBarStrat(ERTSEnemyHealthBarVisibilityStrategy NewStrategy);

	UFUNCTION(BlueprintCallable, Category="Settings")
	void SetPendingEnemyBxpHpBarStrat(ERTSEnemyHealthBarVisibilityStrategy NewStrategy);

	UFUNCTION(BlueprintCallable, Category="Settings")
	void SetPendingEnemyAircraftHpBarStrat(ERTSEnemyHealthBarVisibilityStrategy NewStrategy);

	UFUNCTION(BlueprintCallable, Category="Settings")
	void ApplySettings();

	UFUNCTION(BlueprintCallable, Category="Settings")
	void SaveSettings();

	UFUNCTION(BlueprintCallable, Category="Settings")
	void RevertSettings();

	UFUNCTION(BlueprintCallable, Category="Settings")
	void SetPendingSettingsToDefaults();

private:
	bool GetIsValidGameUserSettings() const;
	URTSGameUserSettings* GetGameUserSettingsChecked() const;

	void EnsureGameUserSettingsInstance();
	void CacheSupportedResolutions();
	void CacheSettingsSnapshots();
	void CacheAudioSettingsFromDeveloperSettings();
	void ResetPendingSettingsToCurrent();

	void ApplyPendingSettingsToGameUserSettings();
	void ApplyResolutionSettings();
	void ApplyNonResolutionSettings();
	void ApplyAudioSettings();
	void ApplyControlSettings();
	void ApplyGameplaySettingsDiff(const FRTSGameplaySettings& PreviousSettings, const FRTSGameplaySettings& CurrentSettings);

	void ApplyAudioChannelVolume(ERTSAudioType AudioType, float VolumeToApply);
	bool TryGetSoundClassForAudioType(ERTSAudioType AudioType, TSoftObjectPtr<USoundClass>& OutSoundClass) const;
	void ApplyControlSettingsToPlayerController(APlayerController* PlayerControllerToApply) const;

	FRTSSettingsSnapshot BuildSnapshotFromSettings(const URTSGameUserSettings& GameUserSettings) const;

	/**
	 * @brief Applies a snapshot onto UGameUserSettings so staged values can be committed in one place.
	 * @param SnapshotToApply Settings snapshot staged by the menu subsystem.
	 * @param GameUserSettingsToApply Settings object that receives the staged values.
	 */
	void ApplySnapshotToSettings(const FRTSSettingsSnapshot& SnapshotToApply, URTSGameUserSettings& GameUserSettingsToApply);

	void AddSupportedResolutionUnique(const FIntPoint& ResolutionToAdd);

	ERTSScalabilityQuality GetScalabilityQualityFromLevel(int32 ScalabilityLevel) const;
	int32 GetLevelFromScalabilityQuality(ERTSScalabilityQuality ScalabilityQuality) const;
	EWindowMode::Type GetNativeWindowMode(ERTSWindowMode WindowMode) const;
	ERTSWindowMode GetMenuWindowMode(EWindowMode::Type WindowMode) const;

	// Stores the baseline input scales so mouse sensitivity can be applied as a multiplier without drifting each apply.
	UPROPERTY(Transient)
	float M_BaseYawScale = 1.0f;

	UPROPERTY(Transient)
	float M_BasePitchScale = 1.0f;

	UPROPERTY(Transient)
	bool bM_BaseInputScaleInitialised = false;

	UPROPERTY(Transient)
	TWeakObjectPtr<URTSGameUserSettings> M_GameUserSettings;

	UPROPERTY(Transient)
	FRTSSettingsSnapshot M_CurrentSettings;

	UPROPERTY(Transient)
	FRTSSettingsSnapshot M_PendingSettings;

	UPROPERTY(Transient)
	TArray<FIntPoint> M_SupportedResolutions;

	UPROPERTY(Transient)
	TArray<FString> M_SupportedResolutionLabels;

	UPROPERTY(Transient)
	TSoftObjectPtr<USoundMix> M_SettingsSoundMix;

	UPROPERTY(Transient)
	TMap<ERTSAudioType, TSoftObjectPtr<USoundClass>> M_SoundClassesByType;
};
