#pragma once

#include "CoreMinimal.h"
#include "RTS_Survival/GameUI/BuildingExpansion/WidgetBxpOptionState/BxpOptionData.h"
#include "RTS_Survival/GameUI/BuildingExpansion/WidgetBxpOptionState/BxpOptionData.h"
#include "RTS_Survival/GameUI/BuildingExpansion/WidgetBxpOptionState/BxpOptionData.h"

#include "RTSVoicelines.generated.h"


enum class EBxpOptionSection : uint8;

UENUM(BlueprintType)
enum class ERTSVoiceLine : uint8
{
	None,
	Select,
	SelectExcited,
	SelectStressed,
	SelectNeedRepairs,
	SelectAnnoyed,
	Move,
	MoveStressed,
	ReverseMove,
	ReverseMoveStressed,
	Stop,
	StopStressed,
	Attack,
	AttackExcited,
	AttackStressed,
	Harvest,
	Building,
	BuildingStressed,
	Rotation,
	RotationStressed,
	Confirm,
	ConfirmStressed,
	Scavenge,
	Repair,
	RepairStressed,
	Pickup,
	CommandCompleted,
	Fortified,
	ShotConnected,
	ShotBounced,
	Fire,
	EnemyDestroyed,
	LostEngine,
	LostGun,
	LostTrack,
	LoadAP,
	LoadHE,
	LoadHEAT,
	LoadRadixite,
	LoadAPCR,
	NeedHelp,
	UnitDies,
	IdleTalk,
	InCombatTalk,
	UnitPromoted,
	FireExtraWeapons,
	SquadUnitLost,
	SquadFullyReinforced,
	EnterPosition,
	ExitPosition,
	FallBackToHQ,
};

UENUM(Blueprintable)
enum class EAnnouncerVoiceLineType : uint8
{
	None,
	Custom,
	LostSquad,
	LostLightVehicle,
	HarvesterTakingFire,
	LostArmoredCar,
	LostHarvester,
	LostMediumVehicle,
	LostHeavyVehicle,
	LostNomadicBuilding_Truck,
	LostNomadicBuilding_StaticBuilding,
	NewExpansionConstructed,
	NomadicBuildingConvertedToBuilding,
	RadixiteStorageFull,
	MetalStorageFull,
	VehiclePartsStorageFull,
	NotEnoughEnergy,
	ReinforcementsHaveArrived,
	NotEnoughRadixite,
	NotEnoughMetal,
	NotEnoughVehicleParts,
	ErrorLocationNotReachable,
	ErrorCannotStackBuildingAbilities,
	ErrorNoFreeTruckAvailable,
	ErrorCannotBuildHere,
	StartDefensiveBxpConstruction,
	StartEconomicBxpConstruction,
	StartTechnologyBxpConstruction,
	SelectTargetForNomadicBuilding,
	SelectTargetForAimAbility,
	NotEnoughSquadUnitsToCapture,
	SquadWillCaptureObjective,
	NoMobileHQToFallBackto,
	Training,
	OnHold,
	Cancelled,
	GamePaused,
	GameResumed,
	AbilityNotInRange,
	LowBaseEnergy,
	BaseEnergyRestored,
};

USTRUCT(BlueprintType)
struct FVoiceLineData
{
	GENERATED_BODY()

	USoundBase* GetVoiceLine();

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<USoundBase*> VoiceLines;

private:
	// Start at -1 to know whether the voicelines were initiated.
	int32 M_VoiceLineIndex = INDEX_NONE;

	UPROPERTY()
	USoundBase* M_LastPlayedVoiceLine = nullptr;

	void ReshuffleVoiceLines(USoundBase* LastPlayedVoiceLine = nullptr);

	void ValidateVoiceLines();
	bool EnsureVoiceLineIndexIsValid(int32& Index) const;
};

UENUM(BlueprintType)
enum class ERTSVoiceLineUnitType : uint8
{
	None,
	LightTank,
	LightTankDestroyer,
	ArmoredCar,
	MediumTank,
	MediumTankDestroyer,
	HeavyTank,
	TigerTank,
	PantherTank,
	HeavyTankDestroyer,
	Scavengers,
	Vultures,
	VulturesSniper,
	FemaleSniper,
	SturmPioneers,
	GerT2FlameTroopers,
	GerEliteLightBringers,
	GerEliteSturmKommando,
	LightJagers,
	AntiTankInfantry,
	RusRegular,
	RusHazmat,
	RusElite
};

USTRUCT(BlueprintType)
struct FUnitVoiceLinesData
{
	GENERATED_BODY()

	/** The type that identifies the unit voice lines. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ERTSVoiceLineUnitType UnitVoiceLineType = ERTSVoiceLineUnitType::None;

	/** All the voice-line arrays, one per VoiceLine event. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<ERTSVoiceLine, FVoiceLineData> VoiceLines;

	/**
	 * @brief Retrieve the _actual_ FVoiceLineData stored for this event.
	 * @param VoiceLineType  The event you want (Select, Attack, etc.)
	 * @param OutVoiceLines  Pointer to the real struct in the map.
	 * @return true if found, false (and logs) otherwise.
	 */
	bool GetVoiceLinesForType(
	    ERTSVoiceLine VoiceLineType,
	    FVoiceLineData*& OutVoiceLines);    // ← pointer to real data

};

USTRUCT(BlueprintType)
struct FAnnouncerVoiceLineData
{
	GENERATED_BODY()
	
	/** All the voice-line arrays, one per VoiceLine event. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<EAnnouncerVoiceLineType, FVoiceLineData> VoiceLines;
	
	/**
	 * @brief Retrieve the _actual_ FVoiceLineData stored for this event.
	 * @param VoiceLineType  The event you want (Select, Attack, etc.)
	 * @param OutVoiceLines  Pointer to the real struct in the map.
	 * @return true if found, false (and logs) otherwise.
	 */
	bool GetVoiceLinesForType(
	    EAnnouncerVoiceLineType VoiceLineType,
	    FVoiceLineData*& OutVoiceLines);    // ← pointer to real data
};



// A settings struct to setup voice line data from blueprints.
USTRUCT(BlueprintType)
struct FRTSVoiceLineSettings
{
	GENERATED_BODY()

	// The type of units that use these voice lines.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ERTSVoiceLineUnitType VoiceLineType = ERTSVoiceLineUnitType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<ERTSVoiceLine, FVoiceLineData> VoiceLines = {};
};

