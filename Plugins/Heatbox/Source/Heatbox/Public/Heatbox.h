// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FHeatboxModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

DECLARE_LOG_CATEGORY_EXTERN(Firebox, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(TemperatureLog, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(MiscLog, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(HeatDamageLog, Log, All);
