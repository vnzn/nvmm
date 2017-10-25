/**
  ******************************************************************************
  * File Name          : main.c
  * Description        : Main program body
  ******************************************************************************
  ** This notice applies to any and all portions of this file
  * that are not between comment pairs USER CODE BEGIN and
  * USER CODE END. Other portions of this file, whether 
  * inserted by the user or by software development tools
  * are owned by their respective copyright owners.
  *
  * COPYRIGHT(c) 2017 STMicroelectronics
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32f1xx_hal.h"

/* USER CODE BEGIN Includes */
#include <string.h>
#include "nvmm.h"
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/
#undef putchar


//------------------------------------------------------------------------------
/// Outputs a character on the debug line.
/// \note This function is synchronous (i.e. uses polling).
/// \param c  Character to send.
//------------------------------------------------------------------------------
void debug_putchar(unsigned char c)
{
	HAL_UART_Transmit(&huart1, (uint8_t* )(&c), 1, HAL_MAX_DELAY) ;
}
#ifndef NOFPUT
#include <stdio.h>

//------------------------------------------------------------------------------
/// \exclude
/// Implementation of fputc using the DBGU as the standard output. Required
/// for printf().
/// \param c  Character to write.
/// \param pStream  Output stream.
/// \param The character written if successful, or -1 if the output stream is
/// not stdout or stderr.
//------------------------------------------------------------------------------
signed int fputc(signed int c, FILE *pStream)
{
    if ((pStream == stdout) || (pStream == stderr)) {
    
        debug_putchar(c);
        return c;
    }
    else {

        return EOF;
    }
}

//------------------------------------------------------------------------------
/// \exclude
/// Implementation of fputs using the DBGU as the standard output. Required
/// for printf(). Does NOT currently use the PDC.
/// \param pStr  String to write.
/// \param pStream  Output stream.
/// \return Number of characters written if successful, or -1 if the output
/// stream is not stdout or stderr.
//------------------------------------------------------------------------------
signed int fputs(const char *pStr, FILE *pStream)
{
    signed int num = 0;

    while (*pStr != 0) {

        if (fputc(*pStr, pStream) == -1) {

            return -1;
        }
        num++;
        pStr++;
    }

    return num;
}

#undef putchar

//------------------------------------------------------------------------------
/// \exclude
/// Outputs a character on the DBGU.
/// \param c  Character to output.
/// \return The character that was output.
//------------------------------------------------------------------------------
signed int putchar(signed int c)
{
    return fputc(c, stdout);
}


#endif //#ifndef NOFPUT
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

#define FLASHMEM_SIZE		((uint32_t)(*((uint16_t* )FLASHSIZE_BASE)) * 1024)
#define FLASH_BASE_ADDRESS	FLASH_BASE
#define FLASH_MAX_ADDRESS	(FLASHMEM_SIZE + FLASH_BASE_ADDRESS)
#define FLASH_NVMM_PAGEA		254
#define FLASH_NVMM_PAGEB		255


/*
 * read non-volatile memory BYTES function type.
 */
static int read_nvbytes(uint32_t address, uint8_t* buf, size_t bufsize, size_t datlen)
{
	size_t i ;
	
	if(address + FLASH_BASE_ADDRESS + datlen >= FLASH_MAX_ADDRESS)
	{
		printf("NVMM INFO" "Attempting to read out of range.\n") ;
		return -1 ;
	}
	
	if(buf == 0 || bufsize == 0 || datlen == 0 || bufsize < datlen)
	{
		return -1 ;
	}
	
	for(i=0;i<datlen;i++)
	{
		*buf++ = *((uint8_t* )(address + FLASH_BASE_ADDRESS + i)) ;
	}
	
	return 0 ;
}



#define CHECK_PADDING(address)	(address % 4)
typedef union{
	uint32_t myword ;
	uint8_t mybyte[4] ;
}word_mkr_t ;


/*
 * write non-volatile memory WORDS function type.
 */
static int write_nvwords(uint32_t address, uint8_t* dat, size_t wordnum)
{
	
	
	word_mkr_t word_mkr ;
	size_t i ;
	
	if(CHECK_PADDING(address))
	{
		printf("NVMM INFO" "Attempting to write to no correct padding address.\n") ;
		return -1 ;
	}
	if(dat == 0 || wordnum == 0)
	{
		return -1 ;
	}
	
	if(address + FLASH_BASE_ADDRESS + wordnum * 4 >= FLASH_MAX_ADDRESS)
	{
		printf("NVMM INFO" "Attempting to write out of range.\n") ;
		return -1 ;
	}
	
	
	HAL_FLASH_Unlock() ;
	for(i=0;i<wordnum*4;i+=4)
	{
		word_mkr.mybyte[0] = dat[i] ;
		word_mkr.mybyte[1] = dat[i+1] ;
		word_mkr.mybyte[2] = dat[i+2] ;
		word_mkr.mybyte[3] = dat[i+3] ;
		
		if(HAL_OK != HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address + FLASH_BASE_ADDRESS + i, \
									word_mkr.myword))
		{//re-programming a no erased memory will also come here.
//			HAL_FLASH_Lock() ;
//			printf("NVMM INFO" "Programming Error with address->0x%08x and word->0x%08x.\n", \
//					address, word_mkr.myword) ;
//			return -1 ;
		}
	}
	HAL_FLASH_Lock() ;
	
	return 0 ;
	
}



/*
 * erase non-volatile page function type.
 */
static int erase_nvpage(uint32_t address)
{
	FLASH_EraseInitTypeDef erase_info ;
	
	
	uint32_t erase_error = 0 ;
	
	if(address + FLASH_BASE_ADDRESS>= FLASH_MAX_ADDRESS)
	{
		printf("NVMM INFO" "Attempting to erase out of range.\n") ;
		return -1 ;
	}
	
	erase_info.TypeErase = FLASH_TYPEERASE_PAGES ;
	erase_info.PageAddress = address + FLASH_BASE_ADDRESS ;
	erase_info.NbPages = 1 ;
	HAL_FLASH_Unlock() ;
	if(HAL_OK != HAL_FLASHEx_Erase(&erase_info, &erase_error))
	{
		HAL_FLASH_Lock() ;
		return -1 ;
	}
	HAL_FLASH_Lock() ;
	
	
	return 0 ;
}

int nvmm_buf[1024] = {0, } ;
/* USER CODE END 0 */

int main(void)
{

  /* USER CODE BEGIN 1 */
	volatile int rc = -1 ;
  /* USER CODE END 1 */

  /* MCU Configuration----------------------------------------------------------*/

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
  MX_USART1_UART_Init();

  /* USER CODE BEGIN 2 */
	
	rc = g_init_nvmm(read_nvbytes, write_nvwords, erase_nvpage, \
				FLASH_NVMM_PAGEA, FLASH_NVMM_PAGEB, FLASH_PAGE_SIZE) ;
	rc = g_read_nvmm(0, 30, nvmm_buf, sizeof(nvmm_buf)) ;
	rc = g_write_nvmm(0, strlen("Hello NVMM!"), "Hello NVMM!") ;
	rc = g_read_nvmm(0, 30, nvmm_buf, sizeof(nvmm_buf)) ;

	rc = g_write_nvmm(1, strlen("1Hello NVMM!"), "1Hello NVMM!") ;
	rc = g_read_nvmm(1, 30, nvmm_buf, sizeof(nvmm_buf)) ;

	rc = g_write_nvmm(0, strlen("2Hello NVMM!"), "2Hello NVMM!") ;
	rc = g_read_nvmm(0, 30, nvmm_buf, sizeof(nvmm_buf)) ;

	rc = g_write_nvmm(0, strlen("3Hello NVMM!"), "3Hello NVMM!") ;
	rc = g_read_nvmm(0, 30, nvmm_buf, sizeof(nvmm_buf)) ;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */

  }
  /* USER CODE END 3 */

}

/** System Clock Configuration
*/
void SystemClock_Config(void)
{

  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;

    /**Initializes the CPU, AHB and APB busses clocks 
    */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = 16;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

    /**Initializes the CPU, AHB and APB busses clocks 
    */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

    /**Configure the Systick interrupt time 
    */
  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

    /**Configure the Systick 
    */
  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

  /* SysTick_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

/* USART1 init function */
static void MX_USART1_UART_Init(void)
{

  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    _Error_Handler(__FILE__, __LINE__);
  }

}

/** Configure pins as 
        * Analog 
        * Input 
        * Output
        * EVENT_OUT
        * EXTI
*/
static void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct;

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, GPIO_PIN_12, GPIO_PIN_RESET);

  /*Configure GPIO pin : PE12 */
  GPIO_InitStruct.Pin = GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
void _Error_Handler(char * file, int line)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  while(1) 
  {
  }
  /* USER CODE END Error_Handler_Debug */ 
}

#ifdef USE_FULL_ASSERT

/**
   * @brief Reports the name of the source file and the source line number
   * where the assert_param error has occurred.
   * @param file: pointer to the source file name
   * @param line: assert_param error line source number
   * @retval None
   */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
    ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */

}

#endif

/**
  * @}
  */ 

/**
  * @}
*/ 

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
