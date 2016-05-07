#ifndef _MACH_HOOKS_H
#define _MACH_HOOKS_H

#define pre_intr_init_hook() init_VISWS_APIC_irqs()

extern void visws_get_board_type_and_rev(void);
#define pre_setup_arch_hook() visws_get_board_type_and_rev()

extern void visws_time_init_hook(void);
#define time_init_hook() visws_time_init_hook()

extern void visws_trap_init_hook(void);
#define trap_init_hook() visws_trap_init_hook()

#endif
