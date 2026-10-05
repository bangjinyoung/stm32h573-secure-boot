#include "board.h"

#include <errno.h>

#include "stm32h5xx_hal.h"

#define BOARD_LED_PORT           GPIOI
#define BOARD_LED_PIN            GPIO_PIN_9

#define BOARD_CONSOLE_PORT       GPIOA
#define BOARD_CONSOLE_TX_PIN     GPIO_PIN_9
#define BOARD_CONSOLE_RX_PIN     GPIO_PIN_10
#define BOARD_CONSOLE_BAUDRATE   115200U
#define BOARD_CONSOLE_TIMEOUT_MS 100U

static UART_HandleTypeDef console_uart;
static uint32_t reset_flags;

static void board_init_led(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOI_CLK_ENABLE();

    /* LED1 is active low on MB1677. */
    HAL_GPIO_WritePin(BOARD_LED_PORT, BOARD_LED_PIN, GPIO_PIN_SET);

    gpio.Pin = BOARD_LED_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BOARD_LED_PORT, &gpio);
}

static int board_init_console(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();

    gpio.Pin = BOARD_CONSOLE_TX_PIN | BOARD_CONSOLE_RX_PIN;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(BOARD_CONSOLE_PORT, &gpio);

    console_uart.Instance = USART1;
    console_uart.Init.BaudRate = BOARD_CONSOLE_BAUDRATE;
    console_uart.Init.WordLength = UART_WORDLENGTH_8B;
    console_uart.Init.StopBits = UART_STOPBITS_1;
    console_uart.Init.Parity = UART_PARITY_NONE;
    console_uart.Init.Mode = UART_MODE_TX_RX;
    console_uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    console_uart.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&console_uart) != HAL_OK) {
        return -EIO;
    }

    return 0;
}

int board_init(void)
{
    reset_flags = RCC->RSR;
    __HAL_RCC_CLEAR_RESET_FLAGS();

    if (HAL_Init() != HAL_OK) {
        return -EIO;
    }

    /* SBS registers (HDPLSR) read as 0 while its clock is off; under STiRoT only RSS turns it on. */
    __HAL_RCC_SBS_CLK_ENABLE();

    board_init_led();

    return board_init_console();
}

void board_get_boot_info(struct board_boot_info *info)
{
    const uint32_t *vector_table = (const uint32_t *)SCB->VTOR;

    info->vtor = SCB->VTOR;
    info->initial_sp = vector_table[0];
    info->reset_handler = vector_table[1];
    info->reset_flags = reset_flags;
    info->core_clock_hz = SystemCoreClock;
    info->hdpl = SBS->HDPLSR & SBS_HDPLSR_HDPL;
    info->mpu_ctrl = MPU->CTRL;
}

void board_led_toggle(void)
{
    HAL_GPIO_TogglePin(BOARD_LED_PORT, BOARD_LED_PIN);
}

void board_delay_ms(uint32_t delay_ms)
{
    HAL_Delay(delay_ms);
}

uint32_t board_uptime_ms(void)
{
    return HAL_GetTick();
}

int board_console_getc(char *c)
{
    if (HAL_UART_Receive(&console_uart, (uint8_t *)c, 1U, 0U) != HAL_OK) {
        return -EAGAIN;
    }

    return 0;
}

int _write(int file, char *ptr, int len)
{
    (void)file;

    if (HAL_UART_Transmit(&console_uart, (uint8_t *)ptr, (uint16_t)len, BOARD_CONSOLE_TIMEOUT_MS) != HAL_OK) {
        return -1;
    }

    return len;
}

void SysTick_Handler(void)
{
    HAL_IncTick();
}
