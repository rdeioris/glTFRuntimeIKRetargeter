// Copyright 2025, Roberto De Ioris.

#pragma once

#include "Modules/ModuleManager.h"

class FglTFRuntimeIKRetargeterModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
