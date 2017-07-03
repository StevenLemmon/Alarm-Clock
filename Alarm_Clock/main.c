#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "math.h"
#include "stdio.h"
#include "stm32f4xx_usart.h"
#include "stm32f4xx_rtc.h"

// Macro to use CCM (Core Coupled Memory) in STM32F4
#define CCM_RAM __attribute__((section(".ccmram")))

#define FPU_TASK_STACK_SIZE 256
#define STACK_SIZE 1024
#define STACK_SIZE_MIN 128
//#define RTC_CLOCK_SOURCE_LSE
#define RTC_CLOCK_SOURCE_LSI

StackType_t fpuTaskStack[FPU_TASK_STACK_SIZE] CCM_RAM;  // Put task stack in CCM
StaticTask_t fpuTaskBuffer CCM_RAM;  // Put TCB in CCM
RTC_InitTypeDef RTC_InitStruct;
RTC_TimeTypeDef RTC_TimeStruct;
RTC_AlarmTypeDef RTC_AlarmStruct;


void init_USART(void);
void init_RTC(void);

void test_FPU_test(void* p);
void light_test(void* p);
void vLedBlinkOrange(void* p);
void STM_EVAL_LEDToggle();
void STM_EVAL_LEDInit();

int main(void) {
	EXTI_InitTypeDef EXTI_InitStruct;
	NVIC_InitTypeDef NVIC_InitStruct;
	
	SystemInit();
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
	init_USART();
	init_RTC();
	
	STM_EVAL_LEDInit(GPIO_Pin_13);
	
	EXTI_ClearITPendingBit(EXTI_Line17);
	EXTI_InitStruct.EXTI_Line = EXTI_Line17;
	EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Rising;
	EXTI_InitStruct.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStruct);
	
	//Enable the Alarm A interrupt
	NVIC_InitStruct.NVIC_IRQChannel = RTC_Alarm_IRQn;
	NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStruct);
	
	vTaskStartScheduler();  // should never return
	
	for(;;);
}

void vApplicationTickHook(void) {
}

/* vApplicationMallocFailedHook() will only be called if
   configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h.  It is a hook
   function that will get called if a call to pvPortMalloc() fails.
   pvPortMalloc() is called internally by the kernel whenever a task, queue,
   timer or semaphore is created.  It is also called by various parts of the
   demo application.  If heap_1.c or heap_2.c are used, then the size of the
   heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
   FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
   to query the size of free heap space that remains (although it does not
   provide information on how the remaining heap might be fragmented). */
void vApplicationMallocFailedHook(void) {
	taskDISABLE_INTERRUPTS();
	for(;;);
}

/* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
   to 1 in FreeRTOSConfig.h.  It will be called on each iteration of the idle
   task.  It is essential that code added to this hook function never attempts
   to block in any way (for example, call xQueueReceive() with a block time
   specified, or call vTaskDelay()).  If the application makes use of the
   vTaskDelete() API function (as this demo application does) then it is also
   important that vApplicationIdleHook() is permitted to return to its calling
   function, because it is the responsibility of the idle task to clean up
   memory allocated by the kernel to any task that has since been deleted. */
void vApplicationIdleHook(void) {
}

void vApplicationStackOverflowHook(xTaskHandle pxTask, signed char *pcTaskName) {
	(void) pcTaskName;
	(void) pxTask;
	/* Run time stack overflow checking is performed if
	configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	function is called if a stack overflow is detected. */
	taskDISABLE_INTERRUPTS();
	for(;;);
}

StaticTask_t xIdleTaskTCB CCM_RAM;
StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE] CCM_RAM;

/* configUSE_STATIC_ALLOCATION is set to 1, so the application must provide an
implementation of vApplicationGetIdleTaskMemory() to provide the memory that is
used by the Idle task. */
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize) {
	/* Pass out a pointer to the StaticTask_t structure in which the Idle task's
	state will be stored. */
	*ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
	
	/* Pass out the array that will be used as the Idle task's stack. */
	*ppxIdleTaskStackBuffer = uxIdleTaskStack;
	
		/* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
	Note that, as the array is necessarily of type StackType_t,
	configMINIMAL_STACK_SIZE is specified in words, not bytes. */
	*pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

static StaticTask_t xTimerTaskTCB CCM_RAM;
static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH] CCM_RAM;

/* configUSE_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, so the
application must provide an implementation of vApplicationGetTimerTaskMemory()
to provide the memory that is used by the Timer service task. */
void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize) {
	*ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
	*ppxTimerTaskStackBuffer = uxTimerTaskStack;
	*pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

/*
 * Configure USART3(PB10, PB11) to redirect printf data to host PC.
 */
void init_USART(void) {
	GPIO_InitTypeDef GPIO_InitStruct;
	USART_InitTypeDef USART_InitStruct;
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
	
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_Init(GPIOB, &GPIO_InitStruct);
	
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource6, GPIO_AF_USART1);
	GPIO_PinAFConfig(GPIOB, GPIO_PinSource7, GPIO_AF_USART1);
	
	USART_InitStruct.USART_BaudRate = 9600;
	USART_InitStruct.USART_WordLength = USART_WordLength_8b;
	USART_InitStruct.USART_StopBits = USART_StopBits_1;
	USART_InitStruct.USART_Parity = USART_Parity_No;
	USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStruct.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
	USART_Init(USART1, &USART_InitStruct);
	USART_Cmd(USART1, ENABLE);
}

void init_RTC(void) {
	//Enable the PWR clock
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);

	//Allow access to RTC
	PWR_BackupAccessCmd(ENABLE);

	//Enable the LSI
	RCC_LSICmd(ENABLE);

	//Wait until LSI is ready
	while(RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET);

	//Select the RTC source
	RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);

	//Enable the RTC
	RCC_RTCCLKCmd(ENABLE);

	//Wait for RTC APB registers synchronisation
	RTC_WaitForSynchro();

	//Configure the RTC data register and prescaler
	RTC_InitStruct.RTC_AsynchPrediv = 0x7F;
	RTC_InitStruct.RTC_SynchPrediv = 0xFF;
	RTC_InitStruct.RTC_HourFormat = RTC_HourFormat_24;
	RTC_Init(&RTC_InitStruct);

	//Setting the time to 5:20.00 AM
	RTC_TimeStruct.RTC_H12 = RTC_H12_AM;
	RTC_TimeStruct.RTC_Hours = 0x05;
	RTC_TimeStruct.RTC_Minutes = 0x20;
	RTC_TimeStruct.RTC_Seconds = 0x00;
	RTC_SetTime(RTC_Format_BCD, &RTC_TimeStruct);

	//Setting alarm to 5:21.00 AM
	RTC_AlarmStruct.RTC_AlarmTime.RTC_H12 = RTC_H12_AM;
	RTC_AlarmStruct.RTC_AlarmTime.RTC_Hours = 0x05;
	RTC_AlarmStruct.RTC_AlarmTime.RTC_Minutes = 0x21;
	RTC_AlarmStruct.RTC_AlarmTime.RTC_Seconds = 0x00;
	RTC_AlarmStruct.RTC_AlarmDateWeekDay = 0x31;
	RTC_AlarmStruct.RTC_AlarmDateWeekDaySel = RTC_AlarmDateWeekDaySel_Date;
	RTC_AlarmStruct.RTC_AlarmMask = RTC_AlarmMask_DateWeekDay;
	RTC_SetAlarm(RTC_Format_BCD, RTC_Alarm_A, &RTC_AlarmStruct);

	//Enable alarm A interrupt
	RTC_ITConfig(RTC_IT_ALRA, ENABLE);

	//Enable the alarm
	RTC_AlarmCmd(RTC_Alarm_A, ENABLE);
	RTC_ClearFlag(RTC_FLAG_ALRAF);

	//Indicator for the RTC configuration
	RTC_WriteBackupRegister(RTC_BKP_DR0, 0x32F2);
}

void STM_EVAL_LEDInit() {
	GPIO_InitTypeDef  GPIO_InitStructure;
  
	/* Enable the GPIO_LED Clock */
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);

	/* Configure the GPIO_LED pin */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOD, &GPIO_InitStructure);
}

void STM_EVAL_LEDToggle() {
	GPIOD->ODR ^= GPIO_Pin_13;
}
