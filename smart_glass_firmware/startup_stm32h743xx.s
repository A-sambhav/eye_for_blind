.syntax unified
.cpu cortex-m7
.fpu fpv5-d16
.thumb

.global g_pfnVectors
.global Default_Handler
.global Reset_Handler

.word _sstack
.word _estack

.section .isr_vector, "a", %progbits
.type g_pfnVectors, %object
g_pfnVectors:
.word _estack
.word Reset_Handler
.word NMI_Handler
.word HardFault_Handler
.word MemManage_Handler
.word BusFault_Handler
.word UsageFault_Handler
.word 0
.word 0
.word 0
.word 0
.word SVC_Handler
.word DebugMon_Handler
.word 0
.word PendSV_Handler
.word SysTick_Handler

.rept 140
.word Default_Handler
.endr

.size g_pfnVectors, .-g_pfnVectors

.section .text.Reset_Handler, "ax", %progbits
.type Reset_Handler, %function
Reset_Handler:
    ldr r1, =_estack
    msr msp, r1
    bl SystemInit
    ldr r0, =_sbss
    ldr r1, =_ebss
    mov r2, #0
    b LoopFillBss
FillBss:
    str r2, [r0], #4
LoopFillBss:
    cmp r0, r1
    bcc FillBss
    ldr r0, =_sdata
    ldr r1, =_edata
    ldr r2, =_sidata
    b LoopCopyDataInit
CopyDataInit:
    ldr r3, [r2], #4
    str r3, [r0], #4
LoopCopyDataInit:
    cmp r0, r1
    bcc CopyDataInit
    bl main
    b .

.size Reset_Handler, .-Reset_Handler

.section .text.Default_Handler, "ax", %progbits
.type Default_Handler, %function
Default_Handler:
    b .
.size Default_Handler, .-Default_Handler

.macro def_isr name
.thumb_func
.weak \name
.type \name, %function
\name:
    b Default_Handler
.size \name, .- \name
.endm

def_isr NMI_Handler
def_isr HardFault_Handler
def_isr MemManage_Handler
def_isr BusFault_Handler
def_isr UsageFault_Handler
def_isr SVC_Handler
def_isr DebugMon_Handler
def_isr PendSV_Handler
def_isr SysTick_Handler

.weak SystemInit
.type SystemInit, %function
SystemInit:
    bx lr
.size SystemInit, .-SystemInit
