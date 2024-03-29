/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <PID.h>
#include <control.h>
#include "string.h"
#include "stdio.h"
#include <stdbool.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
int kt = 0;
struct ControlPosition
{
    float vitri;
    bool goback;
};

struct ControlPosition vitri_st =
    { 0, false };

int presumindex = 0;
int i_SumIndexArry = 0;
static int demoutline = 0;
static struct ControlPosition previtri_st =
    { 0, false };
typedef enum
{
    STOP = 0,
    LINE_FOLLOW,
    MANUAL,
} mode;
mode mode_current = LINE_FOLLOW;
typedef enum
{
    LINE_OFF = 0,
    LINE_START,
} LINE_parameter;

typedef enum
{
    LINE_external = 0,
    LINE_internal,
} LINE_Control;

bool Line_ON = true;

void Control_Value_Line (int value);
void Control_vitri_Line (struct ControlPosition vitri_st);
void CONTROL ();

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
typedef enum
{
    UART_START = 0,
    UART_APP,
} Uart_statemachine;
Uart_statemachine my_state = UART_START;
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;
UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_usart1_rx;
DMA_HandleTypeDef hdma_usart1_tx;

/* USER CODE BEGIN PV */
uint8_t pwm1 = 160;
uint8_t pwm2 = 160;
uint8_t offset = 10;
//uint8_t mode = 0;    // 2 : control   , 1: line follow
int i = 0;
int k = 0;
int kq = 0;
uint8_t S[4];
float result_PWM;
uint8_t Rx_buff[10];
PID_parameter PID_set_parameters =
    { .Kp = 30, .Ki = 0.0002, .Kd = 26, .Ts = 0.005, .PID_Saturation = 180,
            .error = 0, .pre_error = 0, .pre2_error = 0, .pre_Out = 0, .Out = 0 };
PID_parameter PID_set_parameters_160 =
    { .Kp = 30, .Ki = 0.0002, .Kd = 26, .Ts = 0.005, .PID_Saturation = 160,
            .error = 0, .pre_error = 0, .pre2_error = 0, .pre_Out = 0, .Out = 0 };
PID_parameter PID_set_parameters_120 =
    { .Kp = 30, .Ki = 0.0002, .Kd = 26, .Ts = 0.005, .PID_Saturation = 120,
            .error = 0, .pre_error = 0, .pre2_error = 0, .pre_Out = 0, .Out = 0 };
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config (void);
static void MX_GPIO_Init (void);
static void MX_USART1_UART_Init (void);
static void MX_TIM4_Init (void);
static void MX_TIM3_Init (void);
static void MX_DMA_Init (void);
/* USER CODE BEGIN PFP */

/*
 PB0  ---> IN4
 PB1  ---> IN3
 PB10 ---> IN2
 PB11 ---> IN1
 */
void HAL_TIM_PeriodElapsedCallback (TIM_HandleTypeDef *htim)   // ngat 5ms
{
    if (htim->Instance == htim3.Instance)
    {
        if (mode_current == LINE_FOLLOW)
        {
            S[3] = HAL_GPIO_ReadPin (GPIOB, GPIO_PIN_5); // dat gia tri = 7 // ngoai cung ben trai
            S[2] = HAL_GPIO_ReadPin (GPIOB, GPIO_PIN_4); // dat gia tri = 5 // o giua ben trai
            S[1] = HAL_GPIO_ReadPin (GPIOB, GPIO_PIN_3); // dat gia tri = 3 // o giua ben phai
            S[0] = HAL_GPIO_ReadPin (GPIOA, GPIO_PIN_15); // dat gia tri = 1 // ngoai cung ben phai

            /*If Sensor detects line, add to sum Index*/
            for (int i = 0; i <= 3; i++)
            {
                if (S[i] == 0)
                {
                    i_SumIndexArry += (2 * i + 1);
                }
            }

            if (i_SumIndexArry == 0)
            {
                Line_ON = false; // khong bat line
            }
            else
            {
                Line_ON = true; // bat duoc line
            }

            if (Line_ON)
            {
                Control_Value_Line (i_SumIndexArry);
            }
            else
            {
                demoutline += 1;
                if (demoutline == 10) // out time line -> nhan lai tri tri truoc khi out
                {

                    vitri_st = previtri_st;
                    Control_vitri_Line (vitri_st);
                    Line_ON = true;
                    demoutline = 0;
                }
            }

            presumindex = i_SumIndexArry;
            i_SumIndexArry = 0;

        }
        else if (mode_current == STOP)
        {
            __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_3, 0);
            __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_4, 0);
        }
    }
}
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void HAL_UART_RxCpltCallback (UART_HandleTypeDef *huart)
{
    if (huart->Instance == huart1.Instance)
    {
        switch (my_state)
        {
            case UART_START:
                {
                    if (Rx_buff[0] == 's')  // khi nhan nut start de bat dau
                    {
                        my_state = UART_APP;
                        HAL_UART_Receive_DMA (&huart1, Rx_buff, 1);
                        break;
                    }
                    else if (Rx_buff[0] == '1')  // khi nhan nut stop
                    {
                        my_state = UART_START;
                        HAL_UART_Receive_DMA (&huart1, Rx_buff, 1);
                        break;
                    }
                }
            case UART_APP:
                {
                    HAL_UART_Receive_DMA (&huart1, (uint8_t*) Rx_buff, 1);
                    if (Rx_buff[0] == 'f' || Rx_buff[0] == 'b'
                            || Rx_buff[0] == 'r' || Rx_buff[0] == 'l') // khi o che do dieu khien tay
                    {
                        mode_current = MANUAL;  // control
                        my_state = UART_APP;
                        HAL_UART_Receive_DMA (&huart1, Rx_buff, 1);
                    }
                    else if (Rx_buff[0] == 'i')
                    {
                        mode_current = STOP;

                    }
                    else if (Rx_buff[0] == 'd') // bat che do do line
                    {
                        mode_current = LINE_FOLLOW;  // che do do line
                        HAL_UART_Receive_DMA (&huart1, Rx_buff, 1);
                        my_state = UART_APP;
                    }
                    else if (Rx_buff[0] == 'k') // tat che do do line
                    {
                        mode_current = STOP;
                        HAL_UART_Receive_DMA (&huart1, Rx_buff, 1);
                        my_state = UART_APP;
                    }
                    break;
                }
        }
    }
}
/*-----------------------------------------------------------------------------
 @brief  Control_Value_Line
 @param
 light :         0    1    2    3
 Reference:      1    3    5    7
 */
void Control_Value_Line (int value)
{
    switch (value)
    {
        case 7:
            vitri_st.vitri = 1.8;
            vitri_st.goback = false;
            break;
        case (12 | 15):
            vitri_st.vitri = 1.65;
            vitri_st.goback = false;
            break;
        case 5:
            vitri_st.vitri = 1.4;
            vitri_st.goback = false;
            break;
        case 3:
            vitri_st.vitri = -1.4;
            vitri_st.goback = false;
            break;
        case 8:
            vitri_st.vitri = 0;
            vitri_st.goback = false;
            break;
        case 0:
            vitri_st.vitri = 0;
            vitri_st.goback = true;
            break;
        case (4 | 9):
            vitri_st.vitri = -1.65;
            vitri_st.goback = false;
            break;
        case 1:
            vitri_st.vitri = -1.8;
            vitri_st.goback = false;
            break;
        default:
            break;
    }
    Control_vitri_Line (vitri_st);
}

void Control_vitri_Line (struct ControlPosition vitri_st)
{
    previtri_st = vitri_st;
    //result_PWM = PID_PROCESS (&PID_set_parameters, vitri, 0);
    if ((vitri_st.vitri == 0) && (vitri_st.goback == false))
    {
        backward ();
        __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_3, 190); //kenh 3 dong co trai
        __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_4, 190); //kenh 4 dong co phai
    }
    else if ((vitri_st.vitri == 0) && (vitri_st.goback == true))
    {
        backward ();
        __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_3, -180); //kenh 3 dong co trai
        __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_4, -180); //kenh 4 dong co pha
    }
    if (vitri_st.vitri < 0)
    {
        if (vitri_st.vitri < -1.5)
        {
            backward ();
            __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_3, 190); //kenh 3 dong co trai
            HAL_GPIO_WritePin (GPIOB, GPIO_PIN_10, GPIO_PIN_SET); // dong co phai
            HAL_GPIO_WritePin (GPIOB, GPIO_PIN_11, GPIO_PIN_RESET);
            result_PWM = PID_PROCESS (&PID_set_parameters_120, vitri_st.vitri, 0);
            __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_4, result_PWM);
        }
        else
        {
            backward ();
            __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_3, 190); //kenh 3 dong co trai
            result_PWM = PID_PROCESS (&PID_set_parameters_160, vitri_st.vitri, 0);
            __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_4, result_PWM);
        }
    }
    if (vitri_st.vitri > 0)
    {
        if (vitri_st.vitri > 1.5)
        {
            backward ();
            __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_4, 190); //kenh 4 dong co phai
            HAL_GPIO_WritePin (GPIOB, GPIO_PIN_1, GPIO_PIN_RESET); // dong co trai
            HAL_GPIO_WritePin (GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
            result_PWM = PID_PROCESS (&PID_set_parameters_120, vitri_st.vitri, 0);
            __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_3, result_PWM * (-1)); // kenh 3 dong co trai
        }
        else
        {
            backward ();
            __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_4, 190); //kenh 4 dong co phai
            result_PWM = PID_PROCESS (&PID_set_parameters_160, vitri_st.vitri, 0);
            __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_3, result_PWM * (-1)); // kenh 3 dong co trai
        }
    }

}
/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main (void)
{
    /* USER CODE BEGIN 1 */

    /* USER CODE END 1 */

    /* MCU Configuration--------------------------------------------------------*/

    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    HAL_Init ();

    /* USER CODE BEGIN Init */

    /* USER CODE END Init */

    /* Configure the system clock */
    SystemClock_Config ();

    /* USER CODE BEGIN SysInit */

    /* USER CODE END SysInit */

    /* Initialize all configured peripherals */
    MX_GPIO_Init ();

    MX_TIM4_Init ();
    MX_TIM3_Init ();
    MX_DMA_Init ();
    MX_USART1_UART_Init ();
    /* USER CODE BEGIN 2 */
    HAL_TIM_PWM_Start (&htim4, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start (&htim4, TIM_CHANNEL_4);
    HAL_TIM_Base_Start_IT (&htim3);
    HAL_UART_Receive_DMA (&huart1, Rx_buff, 1);
    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1)
    {
        /*
         */
        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */
    }
    /* USER CODE END 3 */
}

/**
 * @brief TIM4 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM4_Init (void)
{

    /* USER CODE BEGIN TIM4_Init 0 */

    /* USER CODE END TIM4_Init 0 */

    TIM_ClockConfigTypeDef sClockSourceConfig =
        { 0 };
    TIM_MasterConfigTypeDef sMasterConfig =
        { 0 };
    TIM_OC_InitTypeDef sConfigOC =
        { 0 };

    /* USER CODE BEGIN TIM4_Init 1 */

    /* USER CODE END TIM4_Init 1 */
    htim4.Instance = TIM4;
    htim4.Init.Prescaler = 9;
    htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim4.Init.Period = 254;
    htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init (&htim4) != HAL_OK)
    {
        Error_Handler ();
    }
    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource (&htim4, &sClockSourceConfig) != HAL_OK)
    {
        Error_Handler ();
    }
    if (HAL_TIM_PWM_Init (&htim4) != HAL_OK)
    {
        Error_Handler ();
    }
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization (&htim4, &sMasterConfig)
            != HAL_OK)
    {
        Error_Handler ();
    }
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 0;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel (&htim4, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
    {
        Error_Handler ();
    }
    if (HAL_TIM_PWM_ConfigChannel (&htim4, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
    {
        Error_Handler ();
    }
    /* USER CODE BEGIN TIM4_Init 2 */

    /* USER CODE END TIM4_Init 2 */
    HAL_TIM_MspPostInit (&htim4);

}
/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config (void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct =
        { 0 };
    RCC_ClkInitTypeDef RCC_ClkInitStruct =
        { 0 };

    /** Initializes the CPU, AHB and APB busses clocks
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig (&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler ();
    }
    /** Initializes the CPU, AHB and APB busses clocks
     */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
            | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig (&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler ();
    }
}

/**
 * @brief TIM3 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM3_Init (void)
{

    /* USER CODE BEGIN TIM3_Init 0 */

    /* USER CODE END TIM3_Init 0 */

    TIM_ClockConfigTypeDef sClockSourceConfig =
        { 0 };
    TIM_MasterConfigTypeDef sMasterConfig =
        { 0 };

    /* USER CODE BEGIN TIM3_Init 1 */

    /* USER CODE END TIM3_Init 1 */
    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 7199;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 49;
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init (&htim3) != HAL_OK)
    {
        Error_Handler ();
    }
    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource (&htim3, &sClockSourceConfig) != HAL_OK)
    {
        Error_Handler ();
    }
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization (&htim3, &sMasterConfig)
            != HAL_OK)
    {
        Error_Handler ();
    }
    /* USER CODE BEGIN TIM3_Init 2 */

    /* USER CODE END TIM3_Init 2 */

}

/**
 * @brief USART1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART1_UART_Init (void)
{

    /* USER CODE BEGIN USART1_Init 0 */

    /* USER CODE END USART1_Init 0 */

    /* USER CODE BEGIN USART1_Init 1 */

    /* USER CODE END USART1_Init 1 */
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 9600;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init (&huart1) != HAL_OK)
    {
        Error_Handler ();
    }
    /* USER CODE BEGIN USART1_Init 2 */

    /* USER CODE END USART1_Init 2 */

}

/**
 * Enable DMA controller clock
 */
static void MX_DMA_Init (void)
{

    /* DMA controller clock enable */
    __HAL_RCC_DMA1_CLK_ENABLE();

    /* DMA interrupt init */
    /* DMA1_Channel4_IRQn interrupt configuration */
    HAL_NVIC_SetPriority (DMA1_Channel4_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ (DMA1_Channel4_IRQn);
    /* DMA1_Channel5_IRQn interrupt configuration */
    HAL_NVIC_SetPriority (DMA1_Channel5_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ (DMA1_Channel5_IRQn);

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init (void)
{
    GPIO_InitTypeDef GPIO_InitStruct =
        { 0 };

    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin (GPIOB,
    GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_10 | GPIO_PIN_11, GPIO_PIN_RESET);

    /*Configure GPIO pins : PB0 PB1 PB10 PB11 */
    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_10 | GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init (GPIOB, &GPIO_InitStruct);

    /*Configure GPIO pin : PA15 */
    GPIO_InitStruct.Pin = GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init (GPIOA, &GPIO_InitStruct);

    /*Configure GPIO pins : PB3 PB4 PB5 */
    GPIO_InitStruct.Pin = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init (GPIOB, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler (void)
{
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */

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
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
