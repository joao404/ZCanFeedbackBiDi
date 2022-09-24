/*********************************************************************
 * ZCan Feedback BiDi Decoder for ESP32
 *
 * Copyright (C) 2022 Marcel Maage
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * LICENSE file for more details.
 */

#include <Arduino.h>
#include <memory>

#include "ZCan/CanInterfaceEsp32.h"
#include "FeedbackDecoder.h"


twai_timing_config_t timingConfig = TWAI_TIMING_CONFIG_125KBITS();
std::shared_ptr<CanInterfaceEsp32> canInterface = std::make_shared<CanInterfaceEsp32>(timingConfig, GPIO_NUM_4, GPIO_NUM_5);

// I will need in the end two of those moduls to handle each of the 8 inputs
FeedbackDecoder feedbackDecoder("feedbackModul1", "modulConfig", true);


void setup()
{
  Serial.begin(230000);

  if (nullptr != canInterface.get())
  {
    canInterface->begin();
  }

  if (!feedbackDecoder.setCanObserver(canInterface))
  {
    Serial.println("ERROR: No can interface defined");
  }

  feedbackDecoder.begin();

}

void loop()
{
  canInterface->cyclic();
  feedbackDecoder.cyclic();
}