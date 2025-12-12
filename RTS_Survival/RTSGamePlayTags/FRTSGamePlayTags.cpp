#include "RTS_Survival/RTSGamePlayTags/FRTSGamePlayTags.h"
#include "NativeGameplayTags.h"

// Define the native tags once here. These auto-register during startup.
UE_DEFINE_GAMEPLAY_TAG_COMMENT(
	TAG_Component_ScavengeMesh,
	"RTS.Component.ScavengeMesh",
	"Mesh used by AScavengableObject to find sockets whose names start with 'scav_'."
);
