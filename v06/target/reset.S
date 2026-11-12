/* Research-only ARM7 entry. No H910P BSP or loader recovery yet: NOT FLASHABLE. */
.syntax unified
.cpu arm7tdmi
.arm
.section .vectors,"ax",%progbits
.balign 4
.global _start
.type _start,%function
_start:
    /* OEM-format entry vector/literal layout is preserved. */
    ldr pc, [pc, #0x38]
    .rept 15
    nop
    .endr
    .word ou_reset
    .rept 7
    .word ou_reset
    .endr
.size _start, . - _start

.section .text.boot,"ax",%progbits
.balign 4
.global ou_reset
.type ou_reset,%function
ou_reset:
    /* A loader handoff in User mode must never initialize an SVC stack. */
    mrs r0, cpsr
    and r1, r0, #0x1f
    cmp r1, #0x10
    beq ou_boot_halt
    /* Mask IRQ/FIQ, force ARM/SVC until vector and clock mapping is proven. */
    bic r0, r0, #0x3f
    orr r0, r0, #0xd3
    msr cpsr_c, r0
    ldr sp, =__stack_top
    ldr r0, =__bss_start
    ldr r1, =__bss_end
    mov r2, #0
1:
    cmp r0, r1
    bhs 2f
    str r2, [r0], #4
    b 1b
2:
    bl ou_target_main
ou_boot_halt:
    /* No bootloader fallback is assumed and no flash commands are issued. */
    b ou_boot_halt
.size ou_reset, . - ou_reset
