// Copyright Epic Games, Inc. All Rights Reserved.

#include "Heatbox.h"

#define LOCTEXT_NAMESPACE "FHeatboxModule"

void FHeatboxModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FHeatboxModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FHeatboxModule, Heatbox)

DEFINE_LOG_CATEGORY(Firebox);
DEFINE_LOG_CATEGORY(TemperatureLog);
DEFINE_LOG_CATEGORY(MiscLog);
DEFINE_LOG_CATEGORY(HeatDamageLog);