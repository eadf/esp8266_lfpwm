/*
* lfpwm.h
*
* Copyright (c) 2015, eadf (https://github.com/eadf)
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* * Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
* * Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
* * Neither the name of Redis nor the names of its contributors may be used
* to endorse or promote products derived from this software without
* specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/

#include "lfpwm/lfpwm.h"
#include "easygpio/easygpio.h"
#include "user_config.h"
#include "gpio.h"
#include "osapi.h"

#ifndef LFPWM_FREQUENCY
#error "Please define LFPWM frequency: \"LFPWM_FREQUENCY\" in your user_config.h"
#endif

#define lfpwm_micros (0x7FFFFFFF & system_get_time())

static volatile os_timer_t pwmTimer;
static volatile bool lfpwmRunning = false;
static float timeDelta = 1.0/(float)LFPWM_FREQUENCY;
static uint32_t lastTime = 0;
static float nextTime = 0.0;

// A list of all the potential pwm instances "running"
static LFPWM_Self* pwms[16] = {NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL};

static void ICACHE_FLASH_ATTR
lfpwm_pwmTimerFunc(void) {
  uint8_t i = 0;
  LFPWM_Self* self;

  for (i=0; i<16; i++){
    self = pwms[i];
    if (self != NULL) {
      self->acc += self->setPoint;
      if (self->acc & 0x100) {
        // overflow
        GPIO_OUTPUT_SET(self->pin, 1);
      } else {
        GPIO_OUTPUT_SET(self->pin, 0);
      }
      self->acc &= 0xFF;
    }
  }
  uint32_t now = lfpwm_micros;
  if (now>lastTime) {
    lastTime += 0x7FFFFFFF;
    nextTime -= 0x7FFFFFFF;
  }
  nextTime += timeDelta;

  os_timer_arm(&pwmTimer, (uint32_t)(nextTime)-now, false); // 1 millisecond
}

bool ICACHE_FLASH_ATTR
lfpwm_init(LFPWM_Self *self, uint8_t pin) {
  self->setPoint = 0;
  self->acc = 0;
  if (!lfpwmRunning) {
    lastTime = lfpwm_micros+1000000;
    nextTime = lastTime;
    os_timer_disarm(&pwmTimer);
    os_timer_setfn(&pwmTimer, (os_timer_func_t *) lfpwm_pwmTimerFunc, NULL);
    os_timer_arm(&pwmTimer, 1000, false); // wait 1 sec until we start pwm
    lfpwmRunning = true;
  }

  if (easygpio_pinMode(self->pin, EASYGPIO_NOPULL, EASYGPIO_OUTPUT)) {
    pwms[pin] = self;
    return true;
  } else {
    return false;
  }
}

void ICACHE_FLASH_ATTR
lfpwm_start(LFPWM_Self *self) {
  pwms[self->pin] = self;
}

void ICACHE_FLASH_ATTR
lfpwm_stop(LFPWM_Self *self) {
  pwms[self->pin] = NULL;
}

