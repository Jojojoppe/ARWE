#include "main.h"
#include "gpio.h"
#include "tim.h"

#include "SEGGER_RTT.h"

#include <stdbool.h>

extern void SystemClock_Config();
extern TIM_HandleTypeDef htim6;
extern TIM_HandleTypeDef htim14;
extern TIM_HandleTypeDef htim16;

typedef enum {
  SWING_MODE_IDLE,
  SWING_MODE_FULL,
  SWING_MODE_HALF_0,
  SWING_MODE_HALF_1,
  _SWING_MODE_MAX,
} swing_mode_t;
swing_mode_t swing_mode;

typedef enum {
  HALF_SWINNG_STATE_ON,
  HALF_SWINNG_STATE_OFF,
} half_swing_state_t;
half_swing_state_t half_swing_state;

int main() {
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_TIM6_Init();
  MX_TIM14_Init();
  MX_TIM16_Init();

  SEGGER_RTT_Init();
  SEGGER_RTT_WriteString(0, "Starting ARWE...\r\n");

  swing_mode = SWING_MODE_IDLE;

  for (;;) {
  }
}

void SWIN_Interrupt() {
  // Start debouncing
  HAL_TIM_Base_Start_IT(&htim14);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  if (htim->Instance == TIM6) {
    // MODE SELECT TIMER INTERRUPT -> OFF
    HAL_TIM_Base_Stop_IT(&htim6);
    HAL_TIM_Base_Stop_IT(&htim16);
    swing_mode = SWING_MODE_IDLE;
    SEGGER_RTT_printf(0, "E tim6: %d\r\n", swing_mode);
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, 0);
    HAL_GPIO_WritePin(RELAY_GPIO_Port, RELAY_Pin, 0);

  } else if (htim->Instance == TIM14) {
    // DEBOUNCE TIMER INTERRUPT
    HAL_TIM_Base_Stop_IT(&htim14);

    int val = HAL_GPIO_ReadPin(SWIN_GPIO_Port, SWIN_Pin);
    if (!val) {
      // Rising SWIN
      if (swing_mode < _SWING_MODE_MAX - 1) {
        swing_mode++;
      }
      HAL_TIM_Base_Stop_IT(&htim6);
      SEGGER_RTT_printf(0, "^ swin: %d\r\n", swing_mode);
      HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, 1);
      HAL_GPIO_WritePin(RELAY_GPIO_Port, RELAY_Pin, 1);
      if (swing_mode >= SWING_MODE_HALF_0) {
        // Start pulsed mode
        htim16.Init.Period = 1000;
        HAL_TIM_Base_Init(&htim16);
        HAL_TIM_Base_Start_IT(&htim16);
        half_swing_state = HALF_SWINNG_STATE_ON;
      }
    } else {
      // Falling SWIN
      HAL_TIM_Base_Start_IT(&htim6);
      SEGGER_RTT_printf(0, "_ swin: %d\r\n", swing_mode);
    }

  } else if (htim->Instance = TIM16) {
    // OUTPUT TIMER INTERRUPT
    if (half_swing_state == HALF_SWINNG_STATE_ON) {
      HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, 0);
      HAL_GPIO_WritePin(RELAY_GPIO_Port, RELAY_Pin, 0);
      half_swing_state = HALF_SWINNG_STATE_OFF;
      if (swing_mode == SWING_MODE_HALF_0) {
        __HAL_TIM_SET_AUTORELOAD(&htim16, 5000);
      } else if (swing_mode == SWING_MODE_HALF_1) {
        __HAL_TIM_SET_AUTORELOAD(&htim16, 10000);
      }
    } else {
      HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, 1);
      HAL_GPIO_WritePin(RELAY_GPIO_Port, RELAY_Pin, 1);
      half_swing_state = HALF_SWINNG_STATE_ON;
      __HAL_TIM_SET_AUTORELOAD(&htim16, 1000);
    }
  }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
  switch (GPIO_Pin) {
  case SWIN_Pin:
    SWIN_Interrupt();
    break;
  default:
    break;
  }
}