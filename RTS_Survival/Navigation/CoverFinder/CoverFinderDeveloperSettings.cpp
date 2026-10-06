#include "CoverFinderDeveloperSettings.h"

URTSCoverFinderDeveloperSettings::URTSCoverFinderDeveloperSettings()
{
	CategoryName = TEXT("RTS");
	SectionName = TEXT("Infantry Cover Finder");
}

const URTSCoverFinderDeveloperSettings* URTSCoverFinderDeveloperSettings::Get()
{
	return GetDefault<URTSCoverFinderDeveloperSettings>();
}
