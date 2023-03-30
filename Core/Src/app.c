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
extern ADC_HandleTypeDef hadc1;
extern UART_HandleTypeDef huart2;

#define DAC_REFERENCE_VALUE_HIGH   666  // 1023 = 3.3V, 666 = 2.15V
#define DAC_REFERENCE_VALUE_LOW    356  // 1023 = 3.3V, 356 = 1.15V

void receiveData (float* buffer);

t_IRLSdata *tIRLS1;
extern TIM_HandleTypeDef htim6;
uint32_t data;
GPIO_PinState state;
uint8_t cont;
uint8_t flag=0;

float Y, U;
uint8_t buffer[20];
void task_pid(void *taskParmPtr)
{
    t_IRLSdata* tIRLS;
	tIRLS = (t_IRLSdata*) taskParmPtr;
	while(1)
	{
		IRLS_Run(tIRLS);
		//sprintf(buffer,"c= %.f %.f %.f %.f %.f",tIRLS->buffer_T[0],tIRLS->buffer_T[1],tIRLS->buffer_T[2],tIRLS->buffer_T[3],tIRLS->buffer_T[4]);
		//HAL_UART_Transmit(&huart2, buffer, 20, 20);
		//vTaskDelayUntil( &xLastWakeTime, ( tIRLS->ts_Ms / portTICK_RATE_MS ) );
		HAL_ADC_Start(&hadc1);
		HAL_ADC_PollForConversion(&hadc1, 0);
		data=HAL_ADC_GetValue(&hadc1);
		/*if (data>=409 && data<=3685){
			cont++;
			flag=1;
		}
		else {
			if (flag==1) {
				sprintf(buffer,"cont %u \r\n",cont);
				HAL_UART_Transmit(&huart2,buffer,strlen(buffer),10);
				cont=0;
				flag=0;
			}
		}*/
		sprintf(buffer,"data %u \r\n",data);
		HAL_UART_Transmit(&huart2,buffer,strlen(buffer),10);
		HAL_ADC_Stop(&hadc1);
	}
}


void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	HAL_GPIO_TogglePin(signal_output_GPIO_Port, signal_output_Pin);
	state=HAL_GPIO_ReadPin(signal_output_GPIO_Port, signal_output_Pin);
}

void receiveData (float* buffer)
{

    U = (float)HAL_GPIO_ReadPin(signal_output_GPIO_Port, signal_output_Pin);
	HAL_ADC_Start(&hadc1);
	HAL_ADC_PollForConversion(&hadc1,0);
	Y=(float)HAL_ADC_GetValue(&hadc1);
	HAL_ADC_Stop(&hadc1);

	buffer[0] = U;
	buffer[1] = Y;
}

void app(void)
{
	tIRLS1 = (t_IRLSdata*)pvPortMalloc (sizeof(t_IRLSdata));
	IRLS_Init(tIRLS1, 10, receiveData);
	HAL_TIM_Base_Start_IT(&htim6);
	BaseType_t res;
	res=xTaskCreate(task_pid, (const char*) "tarea pid ", configMINIMAL_STACK_SIZE, (void *)tIRLS1, tskIDLE_PRIORITY + 1, NULL);
	configASSERT(res == pdPASS);
	osKernelStart();
}
