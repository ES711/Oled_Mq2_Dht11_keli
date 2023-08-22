/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "adc.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "usb_otg.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"
#include "oled.h"
#include "u8g2.h"
//#include "stdlib.h"
#include "stdio.h"
#include "MQ2.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define DHT11_PORT GPIOF
#define DHT11_PIN GPIO_PIN_12
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
QueueHandle_t queueTemp;
QueueHandle_t queueHumi;
QueueHandle_t queueMQ2LPG;
QueueHandle_t queueMQ2CO;
QueueHandle_t queueMQ2SMOKE;
SemaphoreHandle_t xMutex;
SemaphoreHandle_t xSem;
TaskHandle_t handleOled;
TaskHandle_t handleDHT11;
TaskHandle_t handleMQ2;
TaskHandle_t handleBlank;
u8g2_t u8g2;


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void delayMicroSecond(uint32_t time){
	__HAL_TIM_SET_COUNTER(&htim2, 0);//set counter 0
	__HAL_TIM_ENABLE(&htim2);//start counter 
	while(__HAL_TIM_GET_COUNTER(&htim2) < time){
	}
	__HAL_TIM_DISABLE(&htim2);//close counter
}

void setPinOutput(GPIO_TypeDef *GPIOx, uint16_t GPIO_PIN){
	GPIO_InitTypeDef GPIO_InitStruct;
	GPIO_InitStruct.Pin = GPIO_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
}

void setPinInput(GPIO_TypeDef *GPIOx, uint16_t GPIO_PIN){
	GPIO_InitTypeDef GPIO_InitStruct;
	GPIO_InitStruct.Pin = GPIO_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
}

uint8_t dht11RstCheck(){
	uint8_t timer = 0;
	setPinOutput(DHT11_PORT, DHT11_PIN);
	HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_RESET);
	delayMicroSecond(20000);
	HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);
	delayMicroSecond(30);
	setPinInput(DHT11_PORT, DHT11_PIN);
	
	while(!HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN)){
		timer ++;
		delayMicroSecond(1);
	}
	if(timer > 100 || timer < 20){
		return 0;
	}
	
	timer = 0;
	while(HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN)){
		timer ++;
		delayMicroSecond(1);
	}
	if(timer >100 || timer < 20){
		return 0;
	}
	
	return 1;
}

uint8_t DHT11ReadByte(){
	uint8_t byte;
	for(int i = 0; i < 8;i ++){
		while(HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN));
		while(!HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN));
		
		delayMicroSecond(40);
		byte <<= 1;
		if(HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN)){
			byte |= 0x01;
		}
	}
	return byte;
}

uint8_t DHT11GetData(float *Humi, float *Temp){
	int8_t sta = 0;
	uint8_t buffer[5];
	if(dht11RstCheck()){
		for(int i = 0; i< 5 ;i++){
			buffer[i] = DHT11ReadByte();
			
		}
		if(buffer[0] + buffer[1] + buffer[2] + buffer[3] == buffer[4]){
			uint8_t humiInt = buffer[0];
			uint8_t humiFloat = buffer[1];
			uint8_t tempInt = buffer[2];
			uint8_t tempFloat = buffer[3];
			char tmpHumi[8];
			char tmpTemp[8];
			snprintf(tmpHumi, 8 , "%d.%d",humiInt, humiFloat);
			snprintf(tmpTemp, 8, "%d.%d", tempInt, tempFloat);
			sscanf(tmpHumi, "%f", Humi);
			sscanf(tmpTemp, "%f", Temp);
		}
		sta = 0;
	}
	else
	{
		*Humi = 99;
		*Temp = 99;
		sta = -1;
	}
	return sta;
}
void taskOledControll(void *pvParm){
	uint8_t rstCounter = 0;
	u8g2Init(&u8g2);
	u8g2_ClearBuffer(&u8g2);
	u8g2_ClearDisplay(&u8g2);
	u8g2_SetFont(&u8g2,u8g2_font_squeezed_b6_tr);
	u8g2_DrawStr(&u8g2, 0, 10,"Waiting");
	u8g2_DrawStr(&u8g2, 0, 20, "DHT11 Response");
	u8g2_SendBuffer(&u8g2);
	for(;;){
		HAL_GPIO_WritePin(GPIOB, LD2_Pin, 0);//pb7 
		HAL_GPIO_WritePin(GPIOB, LD1_Pin, 1);//pb0 
		u8g2_ClearBuffer(&u8g2);
		float Temp = 0;
		float Humi = 0;
		char strTemp[32];
		char strHumi[32];
		char strMemOled[32];
		char strLPG[32],strCO[32],strSMOKE[32];
		int LPG,CO,SMOKE=0;
//		BaseType_t receiveHumi = xQueueReceive(queueHumi, &Humi, pdMS_TO_TICKS(3000));
//		BaseType_t receiveTemp = xQueueReceive(queueTemp, &Temp, pdMS_TO_TICKS(3000));
		if(((xQueueReceive(queueHumi, &Humi, pdMS_TO_TICKS(3000)) == pdPASS) && (xQueueReceive(queueTemp, &Temp, pdMS_TO_TICKS(3000)) == pdPASS))){
			rstCounter = 0;
			snprintf(strTemp, 32, "Temp:%.1f C", Temp);
			snprintf(strHumi, 32, "Humi:%.1f %%", Humi);
			u8g2_DrawStr(&u8g2, 0, 10,strTemp);
			u8g2_DrawStr(&u8g2, 0, 20, strHumi);
			//u8g2_DrawStr(&u8g2, 0, 30, );
			//uxTaskGetStackHighWaterMark(handleOled) >> ?task??????????????
			UBaseType_t memOled = 512 - uxTaskGetStackHighWaterMark(handleOled);
			snprintf(strMemOled, 32, "useMemOled:%lu", memOled);
			u8g2_DrawStr(&u8g2, 0, 30, strMemOled);
			u8g2_SendBuffer(&u8g2);
		}
		else{
			taskYIELD();//queue ?????,??????
		}
		if((xQueueReceive(queueMQ2LPG, &LPG, pdMS_TO_TICKS(3000))==pdPASS && xQueueReceive(queueMQ2CO, &CO, pdMS_TO_TICKS(3000))==pdPASS &&xQueueReceive(queueMQ2SMOKE, &SMOKE, pdMS_TO_TICKS(3000))==pdPASS)){
		snprintf(strLPG, 32, "LPG:%d PPM", LPG);
		snprintf(strCO, 32, "CO:%d PPM", CO);
		snprintf(strSMOKE, 32, "SMOKE:%d PPM", SMOKE);
		
		u8g2_DrawStr(&u8g2, 0, 40, strLPG);
		u8g2_DrawStr(&u8g2, 0, 50, strCO);
		u8g2_DrawStr(&u8g2, 0, 60, strSMOKE);
		u8g2_SendBuffer(&u8g2);
		}
		else{
			taskYIELD();//queue ?????,??????
		}
		vTaskDelay(100);
	}
}

void taskDHT11(void *pvParm){
	for(;;){
		HAL_GPIO_WritePin(GPIOB, LD2_Pin, 1);
		HAL_GPIO_WritePin(GPIOB, LD1_Pin, 0);
		float Temp = 0;
		float Humi = 0;
		HAL_GPIO_TogglePin(GPIOB,LD3_Pin);
		xSemaphoreTake(xSem, portMAX_DELAY);
		DHT11GetData(&Humi, &Temp);
		
		HAL_GPIO_TogglePin(GPIOB,LD3_Pin);
		//??semaphore????code
//		xSemaphoreTake(xSem, portMAX_DELAY);
//		HAL_GPIO_WritePin(GPIOB,LD3_Pin, 1);
//		vTaskDelay(3000);
//		HAL_GPIO_WritePin(GPIOB,LD3_Pin, 0);
//		xSemaphoreGive(xSem);
		xQueueSend(queueTemp, &Temp, portMAX_DELAY);
		xQueueSend(queueHumi, &Humi, portMAX_DELAY);
		vTaskDelay(1000);
		xSemaphoreGive(xSem);
	}
	
}
void taskBlank(void *pvParm){


	
	uint8_t counter = 0;
	for(;;){
		counter ++;
		if(counter < 10){
			HAL_GPIO_TogglePin(GPIOB, LD3_Pin);
		}
		else{
			vTaskSuspend(handleBlank);
		}
		vTaskDelay(500);
	}
}
void taskMQ2(void *pvParm){
	for(;;){
	HAL_ADC_Start(&hadc1);
	int LPG,CO,SMOKE=0;
	LPG=MQGetGasPercentage(MQRead()/Ro,GAS_LPG);//PPM ro=10
	xQueueSend(queueMQ2LPG, &LPG, portMAX_DELAY);
	CO=MQGetGasPercentage(MQRead()/Ro,GAS_CO);//PPM ro=10
	xQueueSend(queueMQ2CO, &CO, portMAX_DELAY);
	SMOKE=MQGetGasPercentage(MQRead()/Ro,GAS_SMOKE);//PPM ro=10
	xQueueSend(queueMQ2SMOKE, &SMOKE, portMAX_DELAY);
	vTaskDelay(1000);
	xSemaphoreGive(xSem);}
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();
  MX_USART3_UART_Init();
  MX_USB_OTG_FS_PCD_Init();
  MX_ADC1_Init();
  /* USER CODE BEGIN 2 */
	queueTemp = xQueueCreate(5, 4);
	queueHumi = xQueueCreate(5, 4);
	queueMQ2CO = xQueueCreate(5, 4);
	queueMQ2LPG = xQueueCreate(5, 4);
	queueMQ2SMOKE = xQueueCreate(5, 4);
	xMutex = xSemaphoreCreateMutex();
	xSem = xSemaphoreCreateBinary();
	xTaskCreate(taskOledControll, "display", 512, NULL, 1, &handleOled);
	xTaskCreate(taskDHT11, "sensorDHT11", 1200, NULL, 2, &handleDHT11);
	xTaskCreate(taskMQ2, "sensorMQ2", 1200, NULL, 3, &handleMQ2);
	xSemaphoreGive(xSem);
	vTaskStartScheduler();
  /* USER CODE END 2 */

  /* Init scheduler */
	osKernelInitialize();  /* Call init function for freertos objects (in freertos.c) */
  MX_FREERTOS_Init();

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */
  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	u8g2Init(&u8g2);
	u8g2_ClearBuffer(&u8g2);
	u8g2_ClearDisplay(&u8g2);
	u8g2_SetFont(&u8g2,u8g2_font_DigitalDiscoThin_tf);
	int count = 0;
  while (1)
  {
		count ++;
		float Temp = 0;
		float Humi = 0;
		char strTemp[32];
		char strHumi[32];
		DHT11GetData(&Humi, &Temp);
		snprintf(strTemp, 32, "Temp:%.2f C", Temp);
		snprintf(strHumi, 32, "Humi:%.2f %%", Humi);
		u8g2_ClearBuffer(&u8g2);
		u8g2_DrawStr(&u8g2, 0, 20,strTemp);
		u8g2_DrawStr(&u8g2, 0, 30, strHumi);
		u8g2_SendBuffer(&u8g2);
		uint16_t adcvalue=0;
		int LPG,CO,SMOKE=0;
		HAL_ADC_Start(&hadc1);
		adcvalue=HAL_ADC_GetValue(&hadc1);
		LPG=MQGetGasPercentage(MQRead()/Ro,GAS_LPG);//PPM ro=10
		CO=MQGetGasPercentage(MQRead()/Ro,GAS_CO);//PPM ro=10
		SMOKE=MQGetGasPercentage(MQRead()/Ro,GAS_SMOKE);//PPM ro=10
		HAL_Delay(1000);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM1 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM1) {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
