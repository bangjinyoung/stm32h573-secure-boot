#ifndef BOOT_SAU_H
#define BOOT_SAU_H

/* Marks the Non-secure ranges BL2 reaches from Secure state; everything else stays Secure. */
void boot_sau_init(void);

#endif /* BOOT_SAU_H */
