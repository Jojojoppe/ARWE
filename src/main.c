#include "main.h"
#include "gpio.h"

extern void SystemClock_Config();

int main(){
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();

  for(;;){

  }
}