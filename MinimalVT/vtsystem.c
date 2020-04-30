﻿// 中文
#include "vtsystem.h"
#include "vtasm.h"
#include "exithandler.h"

VMX_CPU g_VMXCPU;

NTSTATUS AllocateVMXRegion()
{
    PVOID pVMXONRegion;
    PVOID pVMCSRegion;
    PVOID pStack;

    pVMXONRegion = ExAllocatePoolWithTag(NonPagedPool,0x1000,'vmon'); //4KB // 申请内存 存放 VMX 信息
    if (!pVMXONRegion)  
    {
        Log("ERROR:申请VMXON内存区域失败!",0);
        return STATUS_MEMORY_NOT_ALLOCATED;
    }
    RtlZeroMemory(pVMXONRegion,0x1000);

    pVMCSRegion = ExAllocatePoolWithTag(NonPagedPool,0x1000,'vmcs');
    if (!pVMCSRegion)
    {
        Log("ERROR:申请VMCS内存区域失败!",0);
        ExFreePool(pVMXONRegion);
        return STATUS_MEMORY_NOT_ALLOCATED;
    }
    RtlZeroMemory(pVMCSRegion,0x1000);

    pStack = ExAllocatePoolWithTag(NonPagedPool,0x2000,'stck');
    if (!pStack)
    {
        Log("ERROR:申请宿主机堆载区域失败!",0);
        ExFreePool(pVMXONRegion);
        ExFreePool(pVMCSRegion);
        return STATUS_MEMORY_NOT_ALLOCATED;
    }
    RtlZeroMemory(pStack,0x2000);

    Log("TIP:VMXON内存区域地址",pVMXONRegion);
    Log("TIP:VMCS内存区域地址",pVMCSRegion);
    Log("TIP:宿主机堆载区域地址",pStack);

    g_VMXCPU.pVMXONRegion = pVMXONRegion;
    g_VMXCPU.pVMXONRegion_PA = MmGetPhysicalAddress(pVMXONRegion); // 需要的是物理地址
    g_VMXCPU.pVMCSRegion = pVMCSRegion;
    g_VMXCPU.pVMCSRegion_PA = MmGetPhysicalAddress(pVMCSRegion);
    g_VMXCPU.pStack = pStack;

    return STATUS_SUCCESS;
}

void SetupVMXRegion()
{
    VMX_BASIC_MSR Msr;
    ULONG uRevId;
    _CR4 uCr4;
    _EFLAGS uEflags;

    RtlZeroMemory(&Msr,sizeof(Msr));

    *((PULONG)&Msr) = (ULONG)Asm_ReadMsr(MSR_IA32_VMX_BASIC); // MSR[480H]
    uRevId = Msr.RevId;

    *((PULONG)g_VMXCPU.pVMXONRegion) = uRevId; // 设置版本号信息.不然无法vmoff
    *((PULONG)g_VMXCPU.pVMCSRegion) = uRevId;

    Log("TIP:VMX版本号信息",uRevId);

    *((PULONG)&uCr4) = Asm_GetCr4();
    uCr4.VMXE = 1;  // 锁,VMXOn之后不能清零,不然保护异常
    Asm_SetCr4(*((PULONG)&uCr4));


    Vmx_VmxOn(g_VMXCPU.pVMXONRegion_PA.LowPart, g_VMXCPU.pVMXONRegion_PA.HighPart);
    *((PULONG)&uEflags) = Asm_GetEflags();

    if (uEflags.CF != 0)    // 开启VT之后 CF位会==1
    {
        Log("ERROR:VMXON指令调用失败!",0);
        return;
    }
    Log("SUCCESS:VMXON指令调用成功!",0);
}


extern ULONG g_vmcall_arg;
void g_exit(void);

void __declspec(naked) GuestEntry()
{
    __asm{
        mov ax, es
        mov es, ax

        mov ax, ds
        mov ds, ax

        mov ax, fs
        mov fs, ax

        mov ax, gs
        mov gs, ax

        mov ax, ss
        mov ss, ax
    }

    __asm{
        //jmp g_exit
    }
}

static ULONG  VmxAdjustControls(ULONG Ctl, ULONG Msr)
{
    LARGE_INTEGER MsrValue;
    MsrValue.QuadPart = Asm_ReadMsr(Msr);
    Ctl &= MsrValue.HighPart;     /* bit == 0 in high word ==> must be zero */
    Ctl |= MsrValue.LowPart;      /* bit == 1 in low word  ==> must be one  */
    return Ctl;
}

void SetupVMCS()
{
    _EFLAGS uEflags;
    ULONG GdtBase,IdtBase;
    ULONG uCPUBase,uExceptionBitmap;

    Vmx_VmClear(g_VMXCPU.pVMCSRegion_PA.LowPart, g_VMXCPU.pVMCSRegion_PA.HighPart);
    *((PULONG)&uEflags) = Asm_GetEflags();
    if (uEflags.CF != 0 || uEflags.ZF != 0)
    {
        Log("ERROR:VMCLEAR指令调用失败!",0)
        return;
    }
    Log("SUCCESS:VMCLEAR指令调用成功!",0)
    Vmx_VmPtrld(g_VMXCPU.pVMCSRegion_PA.LowPart, g_VMXCPU.pVMCSRegion_PA.HighPart);

    GdtBase = Asm_GetGdtBase();
    IdtBase = Asm_GetIdtBase();

    //
    // 1.Guest State Area
    //
    Vmx_VmWrite(GUEST_CR0, Asm_GetCr0());
    Vmx_VmWrite(GUEST_CR3, Asm_GetCr3());
    Vmx_VmWrite(GUEST_CR4, Asm_GetCr4());

    Vmx_VmWrite(GUEST_DR7, 0x400);
    Vmx_VmWrite(GUEST_RFLAGS, Asm_GetEflags() & ~0x200);

    Vmx_VmWrite(GUEST_ES_SELECTOR, Asm_GetEs() & 0xFFF8);
    Vmx_VmWrite(GUEST_CS_SELECTOR, Asm_GetCs() & 0xFFF8);
    Vmx_VmWrite(GUEST_DS_SELECTOR, Asm_GetDs() & 0xFFF8);
    Vmx_VmWrite(GUEST_FS_SELECTOR, Asm_GetFs() & 0xFFF8);
    Vmx_VmWrite(GUEST_GS_SELECTOR, Asm_GetGs() & 0xFFF8);
    Vmx_VmWrite(GUEST_SS_SELECTOR, Asm_GetSs() & 0xFFF8);
    Vmx_VmWrite(GUEST_TR_SELECTOR, Asm_GetTr() & 0xFFF8);

    Vmx_VmWrite(GUEST_ES_AR_BYTES,      0x10000);
    Vmx_VmWrite(GUEST_FS_AR_BYTES,      0x10000);
    Vmx_VmWrite(GUEST_DS_AR_BYTES,      0x10000);
    Vmx_VmWrite(GUEST_SS_AR_BYTES,      0x10000);
    Vmx_VmWrite(GUEST_GS_AR_BYTES,      0x10000);
    Vmx_VmWrite(GUEST_LDTR_AR_BYTES,    0x10000);

    Vmx_VmWrite(GUEST_CS_AR_BYTES,  0xc09b);
    Vmx_VmWrite(GUEST_CS_BASE,      0);
    Vmx_VmWrite(GUEST_CS_LIMIT,     0xffffffff);

    Vmx_VmWrite(GUEST_TR_AR_BYTES,  0x008b);
    Vmx_VmWrite(GUEST_TR_BASE,      0x80042000);
    Vmx_VmWrite(GUEST_TR_LIMIT,     0x20ab);


    Vmx_VmWrite(GUEST_GDTR_BASE,    GdtBase);
    Vmx_VmWrite(GUEST_GDTR_LIMIT,   Asm_GetGdtLimit());
    Vmx_VmWrite(GUEST_IDTR_BASE,    IdtBase);
    Vmx_VmWrite(GUEST_IDTR_LIMIT,   Asm_GetIdtLimit());

    Vmx_VmWrite(GUEST_IA32_DEBUGCTL,        Asm_ReadMsr(MSR_IA32_DEBUGCTL)&0xFFFFFFFF);
    Vmx_VmWrite(GUEST_IA32_DEBUGCTL_HIGH,   Asm_ReadMsr(MSR_IA32_DEBUGCTL)>>32);

    Vmx_VmWrite(GUEST_SYSENTER_CS,          Asm_ReadMsr(MSR_IA32_SYSENTER_CS)&0xFFFFFFFF);
    Vmx_VmWrite(GUEST_SYSENTER_ESP,         Asm_ReadMsr(MSR_IA32_SYSENTER_ESP)&0xFFFFFFFF);
    Vmx_VmWrite(GUEST_SYSENTER_EIP,         Asm_ReadMsr(MSR_IA32_SYSENTER_EIP)&0xFFFFFFFF); // KiFastCallEntry

    Vmx_VmWrite(GUEST_RSP,  ((ULONG)g_VMXCPU.pStack) + 0x1000);     //Guest 临时栈
    Vmx_VmWrite(GUEST_RIP,  (ULONG)GuestEntry);                     // 客户机的入口点

    Vmx_VmWrite(VMCS_LINK_POINTER, 0xffffffff);
    Vmx_VmWrite(VMCS_LINK_POINTER_HIGH, 0xffffffff);

    //
    // 2.Host State Area
    //
    Vmx_VmWrite(HOST_CR0, Asm_GetCr0());
    Vmx_VmWrite(HOST_CR3, Asm_GetCr3());
    Vmx_VmWrite(HOST_CR4, Asm_GetCr4());

    Vmx_VmWrite(HOST_ES_SELECTOR, Asm_GetEs() & 0xFFF8);
    Vmx_VmWrite(HOST_CS_SELECTOR, Asm_GetCs() & 0xFFF8);
    Vmx_VmWrite(HOST_DS_SELECTOR, Asm_GetDs() & 0xFFF8);
    Vmx_VmWrite(HOST_FS_SELECTOR, Asm_GetFs() & 0xFFF8);
    Vmx_VmWrite(HOST_GS_SELECTOR, Asm_GetGs() & 0xFFF8);
    Vmx_VmWrite(HOST_SS_SELECTOR, Asm_GetSs() & 0xFFF8);
    Vmx_VmWrite(HOST_TR_SELECTOR, Asm_GetTr() & 0xFFF8);

    Vmx_VmWrite(HOST_TR_BASE, 0x80042000);

    Vmx_VmWrite(HOST_GDTR_BASE, GdtBase);
    Vmx_VmWrite(HOST_IDTR_BASE, IdtBase);

    Vmx_VmWrite(HOST_IA32_SYSENTER_CS,  Asm_ReadMsr(MSR_IA32_SYSENTER_CS)&0xFFFFFFFF);
    Vmx_VmWrite(HOST_IA32_SYSENTER_ESP, Asm_ReadMsr(MSR_IA32_SYSENTER_ESP)&0xFFFFFFFF);
    Vmx_VmWrite(HOST_IA32_SYSENTER_EIP, Asm_ReadMsr(MSR_IA32_SYSENTER_EIP)&0xFFFFFFFF); // KiFastCallEntry

    Vmx_VmWrite(HOST_RSP,   ((ULONG)g_VMXCPU.pStack) + 0x2000);     //Host 临时栈
    Vmx_VmWrite(HOST_RIP,   (ULONG)VMMEntryPoint);                  //这里定义我们的VMM处理程序入口

    //
    // 3.虚拟机运行控制域
    //
    Vmx_VmWrite(PIN_BASED_VM_EXEC_CONTROL, VmxAdjustControls(0, MSR_IA32_VMX_PINBASED_CTLS));
    Vmx_VmWrite(CPU_BASED_VM_EXEC_CONTROL, VmxAdjustControls(0, MSR_IA32_VMX_PROCBASED_CTLS));

    //
    // 4.VMEntry运行控制域
    //
    Vmx_VmWrite(VM_ENTRY_CONTROLS, VmxAdjustControls(0, MSR_IA32_VMX_ENTRY_CTLS));

    //
    // 5.VMExit运行控制域
    //
    Vmx_VmWrite(VM_EXIT_CONTROLS, VmxAdjustControls(0, MSR_IA32_VMX_EXIT_CTLS));


    Vmx_VmLaunch();                     //打开新世界大门
//==========================================================
    g_VMXCPU.bVTStartSuccess = FALSE;

    Log("ERROR:VmLaunch指令调用失败!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!", Vmx_VmRead(VM_INSTRUCTION_ERROR))
    StopVirtualTechnology();
}

NTSTATUS StartVirtualTechnology()
{
    NTSTATUS status = STATUS_SUCCESS;
    if (!IsVTEnabled())
        return STATUS_NOT_SUPPORTED;

    status = AllocateVMXRegion();
    if (!NT_SUCCESS(status))
    {
        Log("ERROR:VMX内存区域申请失败",0);
        return STATUS_UNSUCCESSFUL;
    }
    Log("SUCCESS:VMX内存区域申请成功!",0);

    SetupVMXRegion();
    g_VMXCPU.bVTStartSuccess = TRUE;

    SetupVMCS();

    if (g_VMXCPU.bVTStartSuccess)
    {
        Log("SUCCESS:开启VT成功!",0);
        Log("SUCCESS:现在这个CPU进入了VMX模式.",0);
        return STATUS_SUCCESS;
    }
    else Log("ERROR:开启VT失败!",0);
    return STATUS_UNSUCCESSFUL;
}

extern ULONG g_stop_esp, g_stop_eip;
NTSTATUS StopVirtualTechnology()
{
    _CR4 uCr4;
    if(g_VMXCPU.bVTStartSuccess)
    {
        g_vmcall_arg = 'SVT';
        __asm{
            pushad
            pushfd
            mov g_stop_esp, esp
            mov g_stop_eip, offset LLL
        }
        Vmx_VmCall();
LLL:
        __asm{
            popfd
            popad
        }
        g_VMXCPU.bVTStartSuccess = FALSE;
        *((PULONG)&uCr4) = Asm_GetCr4();
        uCr4.VMXE = 0;
        Asm_SetCr4(*((PULONG)&uCr4));

        ExFreePool(g_VMXCPU.pVMXONRegion);
        ExFreePool(g_VMXCPU.pVMCSRegion);
        ExFreePool(g_VMXCPU.pStack);

        Log("SUCCESS:关闭VT成功!",0);
        Log("SUCCESS:现在这个CPU退出了VMX模式.",0);
    }

    return STATUS_SUCCESS;
}

BOOLEAN IsVTEnabled()
{
    ULONG       uRet_EAX, uRet_ECX, uRet_EDX, uRet_EBX;
    _CPUID_ECX  uCPUID;
    _CR0        uCr0;
    _CR4    uCr4;
    IA32_FEATURE_CONTROL_MSR msr;
    //1. CPUID          //23.6 DISCOVERING SUPPORT FOR VMX  // Cpu 是否支持 vt 功能
    Asm_CPUID(1, &uRet_EAX, &uRet_EBX, &uRet_ECX, &uRet_EDX);   // mov eax,1  &&  cpuid    
    *((PULONG)&uCPUID) = uRet_ECX;

    if (uCPUID.VMX != 1)  // CPUID.1:   ECX.VMX[bit 5] == 1
    {
        Log("ERROR: 这个CPU不支持VT!",0);
        return FALSE;
    }

	// 3. MSR             // 23.7 ENABLING AND ENTERING VMX OPERATION
	*((PULONG)&msr) = (ULONG)Asm_ReadMsr(MSR_IA32_FEATURE_CONTROL);  // MSR[3AH]
	if (msr.Lock != 1) {  // 主板 vt 功能是否开启
		Log("ERROR:VT指令未被锁定!", 0);
		return FALSE;
	}

    // 2. CR0 CR4
    *((PULONG)&uCr0) = Asm_GetCr0();
    *((PULONG)&uCr4) = Asm_GetCr4();    // 软件可以控制开启vt..你如果进入vt必须设置这个位 // pe 段保护模式要开启 pg 页保护模式要开启

    if (uCr0.PE != 1 || uCr0.PG!=1 || uCr0.NE!=1)
    {
        Log("ERROR:这个CPU没有开启VT!",0);
        return FALSE;
    }

    if (uCr4.VMXE == 1)
    {
        Log("ERROR:这个CPU已经开启了VT!",0);
        Log("可能是别的驱动已经占用了VT，你必须关闭它后才能开启。",0);   // 刚开始框架简单一点 ..不重入了
        return FALSE;
    }


    Log("SUCCESS:这个CPU支持VT!",0);
    return TRUE;
}
