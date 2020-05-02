// 中文
#include "exithandler.h"
#include "vtsystem.h"
#include "vtasm.h"

GUEST_REGS g_GuestRegs;


void HandleCPUID()
{
    if (g_GuestRegs.eax == 'Mini')
    {
        g_GuestRegs.ebx = 0x88888888;
        g_GuestRegs.ecx = 0x11111111;
        g_GuestRegs.edx = 0x12345678;
    }
    else Asm_CPUID(g_GuestRegs.eax, &g_GuestRegs.eax, &g_GuestRegs.ebx, &g_GuestRegs.ecx, &g_GuestRegs.edx);
}


ULONG g_vmcall_arg;
ULONG g_stop_esp, g_stop_eip;

void HandleVmCall()
{
    if (g_vmcall_arg == 'SVT')
    {
        Vmx_VmClear(g_VMXCPU.pVMCSRegion_PA.LowPart,g_VMXCPU.pVMCSRegion_PA.HighPart);
        Vmx_VmxOff();
        __asm{
            mov esp, g_stop_esp
            jmp g_stop_eip
        }
    } else {
        __asm int 3
    }
}


void HandleCrAccess()
{
    ULONG		movcrControlRegister;
    ULONG		movcrAccessType;
    ULONG		movcrOperandType;
    ULONG		movcrGeneralPurposeRegister;
    ULONG		movcrLMSWSourceData;
    ULONG		ExitQualification;

    ExitQualification = Vmx_VmRead(EXIT_QUALIFICATION) ;
    movcrControlRegister = ( ExitQualification & 0x0000000F );
    movcrAccessType = ( ( ExitQualification & 0x00000030 ) >> 4 );
    movcrOperandType = ( ( ExitQualification & 0x00000040 ) >> 6 );
    movcrGeneralPurposeRegister = ( ( ExitQualification & 0x00000F00 ) >> 8 );

    if( movcrControlRegister != 3 ){    // not for cr3
        __asm int 3
    }

    if (movcrAccessType == 0) {         // CR3 <-- reg32
        Vmx_VmWrite(GUEST_CR3, *(PULONG)((ULONG)&g_GuestRegs + 4 * movcrGeneralPurposeRegister));
    } else {                            // reg32 <-- CR3
        *(PULONG)((ULONG)&g_GuestRegs + 4 * movcrGeneralPurposeRegister) = Vmx_VmRead(GUEST_CR3);
    }
}

static void  VMMEntryPointEbd(void)
{
    ULONG ExitReason;
    ULONG ExitInstructionLength;
    ULONG GuestResumeEIP = 0;

    // ExitReason = 800000021  
    // 24.9.1 Basic VM-Exit Information 
	// bit 31   VM-entry failure (0 = true VM exit; 1 = VM-entry failure)
    // APPENDIX C    VMX BASIC EXIT REASONS
	// 21 转换成10进制 33 VM-entry failure due to invalid guest state. A VM entry failed one of the checks identified in Section 26.3.1.
    ExitReason              = Vmx_VmRead(VM_EXIT_REASON);
    ExitInstructionLength   = Vmx_VmRead(VM_EXIT_INSTRUCTION_LEN);

    g_GuestRegs.eflags  = Vmx_VmRead(GUEST_RFLAGS);
    g_GuestRegs.esp     = Vmx_VmRead(GUEST_RSP);
    g_GuestRegs.eip     = Vmx_VmRead(GUEST_RIP);

    //Log("ExitReason:      %p", ExitReason);
    //Log("GuestResumeEIP:  %p", &GuestResumeEIP);

    // 27.2.1 Basic VM-Exit Information
    switch(ExitReason) {
        //case EXIT_REASON_CPUID:
        //    HandleCPUID();
        //    Log("EXIT_REASON_CPUID", 0)
        //            break;

        //case EXIT_REASON_VMCALL:
        //    HandleVmCall();
        //    Log("EXIT_REASON_VMCALL", 0)
        //            break;

        // Table 27-2.  Exit Qualification for Task Switch
        case EXIT_REASON_CR_ACCESS:
            HandleCrAccess();
            //Log("EXIT_REASON_CR_ACCESS", 0)
            break;

        default:
            Log("not handled reason: %p", ExitReason);
            __asm int 3
    }

//Resume:
    //Vmx_VmxOff(); // 调试技巧

    GuestResumeEIP = g_GuestRegs.eip + ExitInstructionLength;
    Vmx_VmWrite(GUEST_RIP,      GuestResumeEIP);
    Vmx_VmWrite(GUEST_RSP,      g_GuestRegs.esp);
    Vmx_VmWrite(GUEST_RFLAGS,   g_GuestRegs.eflags);

}

// 裸函数 不建议使用局部变量..因为用到了栈.编译器生成 EBP+4 -4 结果不可预测..也不建议使用全局变量
// 需要使用很长的函数,那么再封装一层.直接调用封装来进行..编译器生成不可预测.
void __declspec(naked) VMMEntryPoint(void)
{
    __asm{
        mov g_GuestRegs.eax, eax
        mov g_GuestRegs.ecx, ecx
        mov g_GuestRegs.edx, edx
        mov g_GuestRegs.ebx, ebx
        mov g_GuestRegs.esp, esp
        mov g_GuestRegs.ebp, ebp
        mov g_GuestRegs.esi, esi
        mov g_GuestRegs.edi, edi

        pushfd
        pop eax
        mov g_GuestRegs.eflags, eax

        mov ax, fs
        mov fs, ax
        mov ax, gs
        mov gs, ax
    }

    // Guest Exit 之后就到这了
    // 调试错误码 
    VMMEntryPointEbd();



    __asm{
        mov  eax, g_GuestRegs.eax
        mov  ecx, g_GuestRegs.ecx
        mov  edx, g_GuestRegs.edx
        mov  ebx, g_GuestRegs.ebx
        mov  esp, g_GuestRegs.esp
        mov  ebp, g_GuestRegs.ebp
        mov  esi, g_GuestRegs.esi
        mov  edi, g_GuestRegs.edi

        //vmresume
        __emit 0x0f
        __emit 0x01
        __emit 0xc3
    }
}
