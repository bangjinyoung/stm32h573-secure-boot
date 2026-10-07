#include <stdint.h>
#include <stdio.h>

#include "build_id.h"

#include "console.h"
#include "stm32h5xx.h"
#include "tz_nsc.h"
#include "tz_s_symbols.h"

/* E10.3 probe targets: Secure SRAM1 (the Secure app's data) and the Secure image 0 primary slot. */
#define PROBE_SECURE_SRAM  0x30000000U
#define PROBE_SECURE_FLASH 0x0C018000U

typedef uint32_t probe_call_t(void);

static void print_info(const char *how, uint32_t info)
{
    printf("[ns] %s secure_boot_info=0x%08lx hdpl=0x%02lx s_version=%lu.%lu\r\n", how, info, TZ_NSC_INFO_HDPL(info),
           TZ_NSC_INFO_MAJOR(info), TZ_NSC_INFO_MINOR(info));
}

/* Each probe either faults in the Secure world (SecureFault, the expected result) or prints what it got. */
static void probe_read(char key, uint32_t address)
{
    uint32_t value;

    printf("[ns] probe %c: read 0x%08lx\r\n", key, address);
    value = *(const volatile uint32_t *)address;
    printf("[ns] probe %c: read returned 0x%08lx\r\n", key, value);
}

static void probe_call(void)
{
    probe_call_t *body = (probe_call_t *)(TZ_S_SECURE_BOOT_INFO_BODY | 1U);

    printf("[ns] probe c: veneer secure_boot_info=0x%08lx body=0x%08lx\r\n", (uint32_t)&secure_boot_info,
           (uint32_t)TZ_S_SECURE_BOOT_INFO_BODY);
    print_info("probe c: via veneer", secure_boot_info());
    printf("[ns] probe c: branch to body\r\n");
    print_info("probe c: via body", body());
}

int main(void)
{
    char c;

    printf("\r\n[ns] stm32h573 non-secure app build=%s\r\n", BUILD_ID);
    printf("[ns] vtor=0x%08lx sp=0x%08lx\r\n", SCB->VTOR, __get_MSP());
    print_info("nsc", secure_boot_info());
    printf("[ns] running; s/f/c probe secure sram/flash/call, any other key for a status line\r\n");

    while (1) {
        if (console_getc(&c) == 0) {
            continue;
        }
        switch (c) {
        case 's':
            probe_read(c, PROBE_SECURE_SRAM);
            break;
        case 'f':
            probe_read(c, PROBE_SECURE_FLASH);
            break;
        case 'c':
            probe_call();
            break;
        default:
            printf("[ns] status vtor=0x%08lx\r\n", SCB->VTOR);
            print_info("nsc", secure_boot_info());
            break;
        }
    }
}
