// Copyright (C) CoreMinimal Software Bas Blokzijl - All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * @brief Primary game module for RTS_Survival.
 * Used to initialize systems that must be ready before gameplay starts,
 * such as native gameplay tag registration.
 */
class FRTSSurvivalModule final : public FDefaultGameModuleImpl
{
public:
    /** Called on module startup (engine + this module loaded). */
    virtual void StartupModule() override;

    /** Called on module shutdown (editor/game exit). */
    virtual void ShutdownModule() override;
};
