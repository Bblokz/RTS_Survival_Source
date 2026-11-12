// Copyright (C) Bas Blokzijl - All rights reserved.

#include "InfoTakeoverStyle.h"

namespace InfoTakeoverStyleDefaults
{
	const TCHAR* const TitleTextStyleSetPath =
		TEXT("/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/DT_InfoTakeoverTitleText.DT_InfoTakeoverTitleText");
	const TCHAR* const BodyTextStyleSetPath =
		TEXT("/Game/RTS_Survival/Blueprints/GameUI/InfoTakeover/DT_InfoTakeoverBodyText.DT_InfoTakeoverBodyText");
	const TCHAR* const TitleTypingSoundPath =
		TEXT("/Game/RTS_Survival/Sounds/UI/TextAnimations/Mission_Text_OneSecond.Mission_Text_OneSecond");
	const TCHAR* const DescriptionStartSoundShortPath =
		TEXT("/Game/RTS_Survival/Sounds/UI/TextAnimations/Mission_Title_2Seconds.Mission_Title_2Seconds");
	const TCHAR* const DescriptionStartSoundLongPath =
		TEXT("/Game/RTS_Survival/Sounds/UI/TextAnimations/Mission_Title_3Seconds.Mission_Title_3Seconds");
	const TCHAR* const DescriptionOngoingSoundPath = TEXT(
		"/Game/RTS_Survival/Sounds/UI/TextAnimations/Mission_Title_Fast_OnGoing_3Sec.Mission_Title_Fast_OnGoing_3Sec");
}

FInfoTakeoverStyle::FInfoTakeoverStyle()
{
	using namespace InfoTakeoverStyleDefaults;
	TitleTextStyleSet = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TitleTextStyleSetPath));
	BodyTextStyleSet = TSoftObjectPtr<UDataTable>(FSoftObjectPath(BodyTextStyleSetPath));
	TitleTypingSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TitleTypingSoundPath));
	DescriptionStartSounds = {
		TSoftObjectPtr<USoundBase>(FSoftObjectPath(DescriptionStartSoundShortPath)),
		TSoftObjectPtr<USoundBase>(FSoftObjectPath(DescriptionStartSoundLongPath))
	};
	DescriptionOngoingSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(DescriptionOngoingSoundPath));
}
