#ifndef BOOT_HANDOFF_H
#define BOOT_HANDOFF_H

struct boot_rsp;

/* Returns only on failure, with a negative errno. */
int boot_handoff_jump(const struct boot_rsp *rsp);

#endif /* BOOT_HANDOFF_H */
