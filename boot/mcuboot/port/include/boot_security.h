#ifndef BOOT_SECURITY_H
#define BOOT_SECURITY_H

/*
 * Takes over the memory protection STiRoT leaves for HDPL2: clears its MPU regions and leaves the
 * MPU off, then marks the Non-secure ranges BL2 reaches from Secure state; everything else stays
 * Secure.
 */
void boot_security_init(void);

#endif /* BOOT_SECURITY_H */
