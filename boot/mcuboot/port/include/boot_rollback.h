#ifndef BOOT_ROLLBACK_H
#define BOOT_ROLLBACK_H

/*
 * App rollback state in the HDPL2 option-byte keys, after ST OEMuRoT's HDPL2 sections
 * (0x0FFD0B10, which OEMuRoT never reads). Finishes an OBK swap a reset interrupted, loads the
 * record and logs its state. With MCUBOOT_HW_ROLLBACK_PROT the same record backs MCUboot's
 * security counter hooks; without it BL2 only reports what it found.
 * Call once at HDPL2, before boot_go().
 */
void boot_rollback_init(void);

#endif /* BOOT_ROLLBACK_H */
