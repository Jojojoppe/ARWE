#include "main.h"
#include "SEGGER_RTT.h"
#include "gpio.h"
#include "tim.h"
#include <stdbool.h>

#define DEBOUNCE_TIME 100
#define COOLDOWN_TIME 1000
#define ON_TIME 1000
#define HALF_OFF_TIME 5000
#define QUARTER_OFF_TIME 10000

// #define WFI_SLEEP_ENABLED
// #define WFE_SLEEP_ENABLED

// ----------------------------------------------------------------------------

enum {
  SUBTIMER_DEBOUNCE_SWIN = 0,
  SUBTIMER_DEBOUNCE_SENSE,
  SUBTIMER_COOLDOWN,
  SUBTIMER_ONOFF,
  _SUBTIMER_NUMBER,
};

enum {
  STATE_IDLE = 0,
  STATE_CONSTANT,
  STATE_HALF,
  STATE_QUARTER,
  _STATE_NUMBER,
};

// ----------------------------------------------------------------------------

extern void SystemClock_Config();
void SWIN_Changed();
void SWIN_Debounced();
void SENSE_Changed();
void SENSE_Debounced();
void Cooldown();
void OnOff();

// ----------------------------------------------------------------------------

extern TIM_HandleTypeDef htim6;
static unsigned long long timerValue;
static unsigned long long timerExpiredValues[_SUBTIMER_NUMBER];
static void (*timerFunctions[_SUBTIMER_NUMBER])();
static bool timerUsed[_SUBTIMER_NUMBER];
static unsigned int state;
static bool outputState;

// ----------------------------------------------------------------------------

int main() {
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_TIM6_Init();

  SEGGER_RTT_Init();
  SEGGER_RTT_printf(0, "Starting ARWE...\r\n");

  state = STATE_IDLE;
  outputState = false;

  // Reset subtimers
  timerValue = 0;
  for (int i = 0; i < _SUBTIMER_NUMBER; i++) {
    timerExpiredValues[i] = 0;
    timerFunctions[i] = NULL;
    timerUsed[i] = false;
  }
  // Install subtimer handlers
  timerFunctions[SUBTIMER_DEBOUNCE_SWIN] = SWIN_Debounced;
  timerFunctions[SUBTIMER_DEBOUNCE_SENSE] = SENSE_Debounced;
  timerFunctions[SUBTIMER_COOLDOWN] = Cooldown;
  timerFunctions[SUBTIMER_ONOFF] = OnOff;
  // Start millisecond timer
  HAL_TIM_Base_Start_IT(&htim6);

  for (;;) {
#ifdef WFI_SLEEP_ENABLED
    // Enter sleep mode to wait for timer or EXTI interrupts
    HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFI);
#endif
  }
}

// ----------------------------------------------------------------------------
// EVENTS
// ----------------------------------------------------------------------------

void SWIN_Debounced() {
  timerUsed[SUBTIMER_DEBOUNCE_SWIN] = false;
  // Get current pin state
  GPIO_PinState state = HAL_GPIO_ReadPin(SWIN_GPIO_Port, SWIN_Pin);
  if (state) {
    // Rising edge SWIN
    // Make sure the cooldown period is stopped
    timerUsed[SUBTIMER_COOLDOWN] = false;
    // Increase state by 1 if not already at max
    if (state < _STATE_NUMBER - 1) {
      state++;
    }
    // Turn on output
    outputState = true;
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, 1);
    HAL_GPIO_WritePin(RELAY_GPIO_Port, RELAY_Pin, 1);
    // Determine how to proceed
    switch (state) {
    case STATE_CONSTANT:
      // Make sure the onoff timer is disabled
      timerUsed[SUBTIMER_ONOFF] = false;
      break;
    case STATE_HALF:
    case STATE_QUARTER:
      // Start on timer
      timerExpiredValues[SUBTIMER_ONOFF] = timerValue + ON_TIME;
      timerUsed[SUBTIMER_ONOFF] = true;
      break;
    default:
      break;
    }
  } else {
    // Falling edge SWIN
    // Start cooldown period
    timerExpiredValues[SUBTIMER_COOLDOWN] = timerValue + COOLDOWN_TIME;
    timerUsed[SUBTIMER_COOLDOWN] = true;
  }
}

void SENSE_Debounced() {
  timerUsed[SUBTIMER_DEBOUNCE_SENSE] = false;
  // Get current pin state
  GPIO_PinState state = HAL_GPIO_ReadPin(SENSE_GPIO_Port, SENSE_Pin);
  if (state) {
    // Rising edge SENSE
  } else {
    // Falling edge SENSE
  }
}

void Cooldown() {
  timerUsed[SUBTIMER_COOLDOWN] = false;
  // Cooldown period expired: reset state back to idle
  state = STATE_IDLE;
  timerUsed[SUBTIMER_ONOFF] = false;
  outputState = false;
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, 0);
  HAL_GPIO_WritePin(RELAY_GPIO_Port, RELAY_Pin, 0);

#ifdef WFE_SLEEP_ENABLED
  // Enter deep sleep mode and wait for EXTI interrupt
  HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFE);
#endif
}

void OnOff() {
  timerUsed[SUBTIMER_ONOFF] = false;
  if (outputState) {
    // Turn the output off
    outputState = false;
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, 0);
    HAL_GPIO_WritePin(RELAY_GPIO_Port, RELAY_Pin, 0);
    // Set off timer
    switch (state) {
    case STATE_HALF:
      timerExpiredValues[SUBTIMER_ONOFF] = timerValue + HALF_OFF_TIME;
      break;
    case STATE_QUARTER:
      timerExpiredValues[SUBTIMER_ONOFF] = timerValue + QUARTER_OFF_TIME;
      break;
    default:
      break;
    }
    timerUsed[SUBTIMER_ONOFF] = true;
  } else {
    // Turn the output on
    outputState = true;
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, 1);
    HAL_GPIO_WritePin(RELAY_GPIO_Port, RELAY_Pin, 1);
    // Set on timer
    timerExpiredValues[SUBTIMER_ONOFF] = timerValue + ON_TIME;
    timerUsed[SUBTIMER_ONOFF] = true;
  }
}

// ----------------------------------------------------------------------------
// HARDWARE INTERRUPTS
// ----------------------------------------------------------------------------

void SWIN_Changed() {
  // Start debouncing if not already busy
  if (!timerUsed[SUBTIMER_DEBOUNCE_SWIN]) {
    timerExpiredValues[SUBTIMER_DEBOUNCE_SWIN] = timerValue + DEBOUNCE_TIME;
    timerUsed[SUBTIMER_DEBOUNCE_SWIN] = true;
  }
}

void SENSE_Changed() {
  // Start debouncing if not already busy
  if (!timerUsed[SUBTIMER_DEBOUNCE_SENSE]) {
    timerExpiredValues[SUBTIMER_DEBOUNCE_SENSE] = timerValue + DEBOUNCE_TIME;
    timerUsed[SUBTIMER_DEBOUNCE_SENSE] = true;
  }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  if (htim->Instance == TIM6) {
    // Millisecond timer
    timerValue++;
    // Check subtimers and call if expired
    for (int i = 0; i < _SUBTIMER_NUMBER; i++) {
      if (timerUsed[i] && timerExpiredValues[i] == timerValue &&
          timerFunctions[i] != NULL) {
        timerFunctions[i]();
      }
    }
  }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
  switch (GPIO_Pin) {
  case SWIN_Pin:
    SWIN_Changed();
    break;
  case SENSE_Pin:
    SENSE_Changed();
    break;
  default:
    break;
  }
}

// ----------------------------------------------------------------------------