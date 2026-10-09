#ifndef BOOT_CLOCK_H
#define BOOT_CLOCK_H

/*
 * Raises SYSCLK to the H573 maximum, 250 MHz: VOS0, PLL1 from the 25 MHz ST-LINK MCO on HSE
 * (digital bypass), 5 flash wait states. Same settings as STM32CubeH5 STM32H573I-DK SystemClock_Config.
 * Built with BL2_SYSCLK=pll250. Needs HAL_Init() first (HAL timeouts); the later images inherit the clock.
 * Returns 0 or -EIO; after -EIO the caller reports the clock actually in use (HAL_Init recomputes it).
 */
int boot_clock_set_max(void);

#endif /* BOOT_CLOCK_H */
