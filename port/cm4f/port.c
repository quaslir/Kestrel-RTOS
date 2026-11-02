__attribute__((naked)) void SVC_Handler(void)
{
    __asm volatile("ldr r0, =kestrel_current\n"
                   "ldr r0, [r0]\n"
                   "ldr r0, [r0]\n"
                   "ldmia r0!, {r4-r11}\n"
                   "msr psp, r0\n"
                   "ldr lr, =0xFFFFFFFD\n"
                   "bx lr");
}
