/*
 * app.c
 *
 *  Created on: Mar 27, 2023
 *      Author: ferna
 */

#include "app.h"
#include "main.h"
#include "cmsis_os.h"
#include "arm_math.h"
#include "arm_const_structs.h"
#include "metodos_de_cuadrados_minimos.h"
#include "stdio.h"
#include "stdlib.h"
#include "pid_controller.h"

extern ADC_HandleTypeDef hadc1;
extern UART_HandleTypeDef huart2;
extern DAC_HandleTypeDef hdac1;
extern TIM_HandleTypeDef htim6;


#define DAC_REFERENCE_VALUE_HIGH 3000 // 1023 = 3.3V, 666 = 2.15V
#define DAC_REFERENCE_VALUE_LOW 356	  // 1023 = 3.3V, 356 = 1.15V

void receiveData(float *buffer);

t_IRLSdata *tIRLS1;
uint32_t data;
volatile GPIO_PinState state;
uint8_t buffer[20];

float Y, U;
float y = 0.0f;
float r = 0.0f;
float u_1 = 0.0f;

void task_identification(void *taskParmPtr)
{
	t_IRLSdata *tIRLS;
	tIRLS = (t_IRLSdata *)taskParmPtr;
	while (1)
	{
		IRLS_Run(tIRLS);
		HAL_ADC_Start(&hadc1);
		HAL_ADC_PollForConversion(&hadc1, 0);
		data = HAL_ADC_GetValue(&hadc1);
		sprintf(buffer, "data %u \r\n", data);
		HAL_UART_Transmit(&huart2, buffer, strlen(buffer), 10);
		HAL_ADC_Stop(&hadc1);
	}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	HAL_GPIO_TogglePin(signal_output_GPIO_Port, signal_output_Pin);
	state = HAL_GPIO_ReadPin(signal_output_GPIO_Port, signal_output_Pin);
}

void receiveData(float *buffer)
{

	//dacValue = DAC_REFERENCE_VALUE_LOW + rand() % (DAC_REFERENCE_VALUE_HIGH + 1 - DAC_REFERENCE_VALUE_LOW);
	//U = (float)dacValue * (3.3 / 4095.0);
	//HAL_DAC_SetValue(&hdac1, DAC1_CHANNEL_1, DAC_ALIGN_12B_R, dacValue);

	U = (float)HAL_GPIO_ReadPin(signal_output_GPIO_Port, signal_output_Pin);
	HAL_ADC_Start(&hadc1);
	HAL_ADC_PollForConversion(&hadc1, 0);
	Y = (float)(HAL_ADC_GetValue(&hadc1) * (3.3 / 4095.0));
	HAL_ADC_Stop(&hadc1);

	buffer[0] = U;
	buffer[1] = Y;
}

void task_pid(void *parameter)
{

	PIDController_t PsPIDController;
	uint32_t h_ms = 1;
	float h_s = ((float)h_ms)/1000.0f;
	pidInit(&PsPIDController,
			3.0f,		// Kp
			0.01f / h_s, // Ki
			0.0f * h_s, // Kd
			h_s,		// h en [s]
			20.0f,		// N
			1.0f,		// b
			0.0f,		// u_min
			3.3f		// u_max
	);
	while (1)
	{
		HAL_ADC_Start(&hadc1);
		HAL_ADC_PollForConversion(&hadc1, 0);
		y = (HAL_ADC_GetValue(&hadc1) * (3.3 / 4095));
		HAL_ADC_Stop(&hadc1);
		r = (HAL_GPIO_ReadPin(signal_output_GPIO_Port, signal_output_Pin) * 3.3);
		u_1 = (pidCalculateControllerOutput(&PsPIDController, y, r));
		u_1 =u_1*1240.909090f;
		HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R,u_1 );
		sprintf(buffer, "data %0.5f \r\n", u_1);
		HAL_UART_Transmit(&huart2, buffer, strlen(buffer), 10);
		pidUpdateController(&PsPIDController, y, r);
	}
}

void app(void)
{
	BaseType_t res;
	tIRLS1 = (t_IRLSdata *)pvPortMalloc(sizeof(t_IRLSdata));

	IRLS_Init(tIRLS1, 10, receiveData);
	HAL_TIM_Base_Start_IT(&htim6);
	HAL_DAC_Start(&hdac1, DAC_CHANNEL_1);
	//res = xTaskCreate(task_identification, (const char *)"task_identification", configMINIMAL_STACK_SIZE, (void *)tIRLS1, tskIDLE_PRIORITY + 1, NULL);
	//configASSERT(res == pdPASS);
	res = xTaskCreate(task_pid, (const char *)"tarea pid ", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL);
	configASSERT(res == pdPASS);

	osKernelStart();

}
