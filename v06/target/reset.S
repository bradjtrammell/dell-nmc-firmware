/* Research-only ARM7 startup entry. Never install: no validated H910P BSP. */
.syntax unified
.cpu arm7tdmi
.arm
.section .vectors,"ax",%progbits
.balign 4
.global _start
.type _start,%function
_start:
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
    /* Stack is mapped relative to the image; the physical board mapping
     * has not been established, so this cannot run safely on a live NMC. */
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
3:
    b 3b
.size ou_reset, . - ou_reset
