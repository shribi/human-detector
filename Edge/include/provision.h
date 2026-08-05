#pragma once 
#include "base.h"

void createProvisioningTask();
void provisionQueueSend(eEventType eventType, const Config& config, bool performFactoryReset = false);
void provisioningTask();