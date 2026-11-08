// Copyright (C) Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "AddOnArmorPanelAudioSubsystem.generated.h"

class ARTSPooledAudio;
class UAudioComponent;
class USoundBase;
enum class ETankSubtype : uint8;

/** @brief Which armour-off clang a vehicle plays; follows the tank's weight class. */
UENUM()
enum class EAddOnArmorOffSoundWeight : uint8
{
	Light,
	Medium,
	Heavy
};

/**
 * Sound cues for shot-off add-on armour panels, one per weight class. They sit next to the debris system in a folder
 * that is always cooked, because the subsystem loads them by soft path.
 */
namespace AddOnArmorPanelAudio
{
	inline const TCHAR* LightSoundPath =
		TEXT("/Game/RTS_Survival/SpecialEffects/AddOnArmor/Sounds/Cue_AddOnArmorOff_Light.Cue_AddOnArmorOff_Light");
	inline const TCHAR* MediumSoundPath =
		TEXT("/Game/RTS_Survival/SpecialEffects/AddOnArmor/Sounds/Cue_AddOnArmorOff_Medium.Cue_AddOnArmorOff_Medium");
	inline const TCHAR* HeavySoundPath =
		TEXT("/Game/RTS_Survival/SpecialEffects/AddOnArmor/Sounds/Cue_AddOnArmorOff_Heavy.Cue_AddOnArmorOff_Heavy");
}

/**
 * @brief Plays the clang of add-on armour panels (side skirts) shot off a tank from a fixed pool of audio components
 * (DeveloperSettings::GamePlay::Audio::AddOnArmorOffAudioPoolSize), so hits create no audio components for the GC.
 * A free component is one that stopped playing; when all are busy the one that started longest ago is reused.
 * The cues carry their own attenuation and sound class.
 */
UCLASS()
class RTS_SURVIVAL_API UAddOnArmorPanelAudioSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/**
	 * @brief Plays the weight class's armour-off sound at a world location.
	 * @param Weight Weight class of the vehicle that lost the panels.
	 * @param WorldLocation Where the panels came off.
	 */
	void PlayArmorOffSound(EAddOnArmorOffSoundWeight Weight, const FVector& WorldLocation);

	/** @return Heavy for heavy tanks, Light for light tanks and armoured cars, Medium for everything else. */
	static EAddOnArmorOffSoundWeight GetSoundWeightForTank(ETankSubtype TankSubtype);

private:
	UPROPERTY()
	TObjectPtr<ARTSPooledAudio> M_PoolOwner;

	UPROPERTY()
	TArray<TObjectPtr<UAudioComponent>> M_PoolComponents;

	// Play order of each pool entry's current sound, indexed like M_PoolComponents; the lowest started first. An order
	// instead of a time, so sounds started in the same frame still reuse their entries in turn.
	TArray<uint64> M_PoolPlayOrders;

	uint64 M_NextPlayOrder = 1;

	// Indexed by EAddOnArmorOffSoundWeight; hard references so the GC keeps the loaded cues.
	UPROPERTY()
	TArray<TObjectPtr<USoundBase>> M_Sounds;

	void LoadSounds();
	void SpawnPool(UWorld& World);
	bool GetIsValidPoolOwner() const;

	/** @return A pool entry that is not playing, else the one that started longest ago; INDEX_NONE when empty. */
	int32 AcquirePoolIndex() const;
};
