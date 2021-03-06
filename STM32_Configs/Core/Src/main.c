#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "main.h"
#include "cmsis_os.h"

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_SPI1_Init(void);
void StartDefaultTask(void *argument);
int uart2_write(int ch);
int __io_putchar(int ch);		//reroutes printf to uart communication
void data_transfer(void *pvParameters);
void led_pattern(void *pvParameters);
void green_press(void);
void yellow_press(void);
void checkConditions(void);

SPI_HandleTypeDef hspi1;
UART_HandleTypeDef huart2;
TaskHandle_t led_handle;

uint8_t MasterSend = 0, MasterReceive;
uint16_t led_arr[13] = {
		GPIO_PIN_0,
		GPIO_PIN_1,
		GPIO_PIN_2,
		GPIO_PIN_12,
		GPIO_PIN_13,
		GPIO_PIN_14,
		GPIO_PIN_10,
		GPIO_PIN_4,
		GPIO_PIN_5,
		GPIO_PIN_6,
		GPIO_PIN_7,
		GPIO_PIN_8,
		GPIO_PIN_9,
};
uint16_t delay_time = 500;
uint16_t greenScore = 0, yellowScore = 0;
bool startPhase = false;

int main(void)
{

  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_SPI1_Init();
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);		//writing CS pin to default as high

  //CREATING RTOS TASKS
  //start button
  while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13));

  xTaskCreate(data_transfer, "Data Transfer", 100, NULL, 1, NULL);		  //task for SPI data communication
  xTaskCreate(led_pattern, "LED Pattern", 100, NULL, 1, &led_handle);



  //start the scheduler
  vTaskStartScheduler();

  while (1);

  return 0;
}

void data_transfer(void *pvParameters)
{
	while (1)
	{
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);
		HAL_SPI_Transmit(&hspi1, (uint8_t*)&MasterSend, 1, 10);
		HAL_SPI_Receive(&hspi1, (uint8_t*)&MasterReceive, 1, 10);
		vTaskDelay(pdMS_TO_TICKS(500));
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);

		printf("%d\n\r", MasterReceive);
	}
}

void led_pattern(void *pvParameters)
{
	  //led loop
	while (1)
	{
		for (uint8_t i = 0; i < 13; ++i)
		{
			HAL_GPIO_TogglePin(GPIOB, led_arr[i]);

			if (i > 0)
				HAL_GPIO_TogglePin(GPIOB, led_arr[i - 1]);

			if (i == 12)
			{
				green_press();
				checkConditions();
				HAL_GPIO_TogglePin(GPIOB, led_arr[i]);

				if (!startPhase)
				{
					delay_time -= 30;
				}
			}

			vTaskDelay(pdMS_TO_TICKS(delay_time));

		}

		if (startPhase)
		{
			startPhase = false;
			delay_time = 500;
			break;
		}

		for (uint8_t i = 13; i > 0; --i)
		{
			HAL_GPIO_TogglePin(GPIOB, led_arr[i-1]);

			if ((i -1) != 12)
				HAL_GPIO_TogglePin(GPIOB, led_arr[i]);

			if ((i - 1) == 0)
			{
				yellow_press();
				checkConditions();
				HAL_GPIO_TogglePin(GPIOB, led_arr[i - 1]);

				if (!startPhase)
				{
					delay_time -= 30;
				}
			}

			vTaskDelay(pdMS_TO_TICKS(delay_time));
		}

		if (startPhase)
		{
			startPhase = false;
			delay_time = 500;
			break;
		}
	  }
}

void green_press(void)
{
	uint32_t startTime = HAL_GetTick();
	uint32_t currentTime = HAL_GetTick();

	while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_2) != 0)
	{
		currentTime = HAL_GetTick();
		if (currentTime - startTime == delay_time)
		{
			MasterSend = 3;
			return;
		}
	}

	if (greenScore > yellowScore)
		MasterSend = 1;
	else if (greenScore == yellowScore)
		MasterSend = 6;
	else if (yellowScore > greenScore)
		MasterSend = 2;
}

void yellow_press(void)
{
	uint32_t startTime = HAL_GetTick();
	uint32_t currentTime = HAL_GetTick();

	while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_3) != 0)
	{
		currentTime = HAL_GetTick();
		if (currentTime - startTime == delay_time)
		{
			MasterSend = 4;
			return;
		}
	}

	if (greenScore > yellowScore)
		MasterSend = 1;
	else if (greenScore == yellowScore)
		MasterSend = 6;
	else if (yellowScore > greenScore)
		MasterSend = 2;
}

void checkConditions(void)
{
	if (MasterSend == 3 || MasterSend == 4)
	{
		startPhase = true;

		if (MasterSend == 3)
			yellowScore++;
		else
			greenScore++;

		if (yellowScore == 10 || greenScore == 10)
		{
			vTaskSuspend(led_handle);		//suspend led task if game is won by a player
			MasterSend = 5;
		}

		HAL_Delay(1000);

	}
}

//rerouting printf more efficiently
int uart2_write(int ch)
{
	while(!(USART2->SR & 0x0080)){}
	USART2->DR = (ch & 0xFF);

	return ch;
}

//outputs to serial monitor
int __io_putchar(int ch)
{
	uart2_write(ch);
	return ch;
}


void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_SPI1_Init(void)
{
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
}


void SPI1_IRQHandler(void)
{
	HAL_SPI_IRQHandler(&hspi1);
}

static void MX_USART2_UART_Init(void)
{
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_12
                          |GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_10|GPIO_PIN_4
                          |GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_8
                          |GPIO_PIN_9, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin, Green Button, yellow Button */
  GPIO_InitStruct.Pin = B1_Pin|GPIO_PIN_2|GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
	
  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PA4 */
  GPIO_InitStruct.Pin = GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB1 PB2 PB12
                           PB13 PB14 PB15 PB4
                           PB5 PB6 PB7 PB8
                           PB9 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_12
                          |GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_10|GPIO_PIN_4
                          |GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_8
                          |GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM1) {
    HAL_IncTick();
  }
}

void Error_Handler(void)
{
  __disable_irq();
  while (1);
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif

