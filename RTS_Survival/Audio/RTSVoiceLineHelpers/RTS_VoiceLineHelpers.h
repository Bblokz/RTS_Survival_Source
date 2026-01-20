#pragma once
#include "RTS_Survival/Audio/RTSVoiceLines/RTSVoicelines.h"

enum class EPlayerError : uint8;
enum class ETankSubtype : uint8;
enum class EAbilityID : uint8;
enum class ERTSVoiceLine : uint8;
enum class EBxpOptionSection : uint8;

class FRTS_VoiceLineHelpers
{
public:
	static ERTSVoiceLine GetVoiceLineFromAbility(const EAbilityID Ability);
	static bool NeedToPlayAnnouncerLineForAbility(const EAbilityID Ability, EAnnouncerVoiceLineType& OutAnnouncerLine);
	static ERTSVoiceLine GetStressedVoiceLineVersion(const ERTSVoiceLine VoiceLineType);
	static void PlayUnitDeathVoiceLineOnRadio(AActor* UnitThatDied, const bool bForcePlay = true, const bool bQueueIfNotPlayed = false);

	static EAnnouncerVoiceLineType GetDeathVoiceLineForTank(const ETankSubtype Type);
static EAnnouncerVoiceLineType GetAnnouncerForBxp(const EBxpOptionSection Section);
	static EAnnouncerVoiceLineType GetAnnouncerForPlayerError(const EPlayerError Error);

	
};
