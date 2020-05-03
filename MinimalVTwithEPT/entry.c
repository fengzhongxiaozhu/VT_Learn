#include <ntddk.h>
#include "vtsystem.h"

PULONG test_data;

VOID DriverUnload(PDRIVER_OBJECT driver)
{
    StopVirtualTechnology();
    ExFreePool(test_data);
    DbgPrint("Driver is unloading...\r\n");
}

ULONG g_exit_esp;
ULONG back_position;
void __declspec(naked) g_exit()
{
    __asm{
        mov esp, g_exit_esp
        jmp back_position
    }
}
void initEptPagesPool();
ULONG64* MyEptInitialization();

NTSTATUS 
  DriverEntry( 
    PDRIVER_OBJECT  driver,
    PUNICODE_STRING RegistryPath
    )
{

    DbgPrint("Driver Entered!\r\n");
	driver->DriverUnload = DriverUnload;
    test_data = ExAllocatePoolWithTag(NonPagedPool, 0x1000, 'test');
    Log("test_data at:", test_data);


    initEptPagesPool();
    ULONG64* ept4;

//kd > r cr3
//cr3 = 0030a000
//kd > !vtop 0030a000 0x815c4000
//Virtual address 815c4000 translates to physical address 17c4000.
//kd > .formats 17c4000
//Binary : 00000001 01111100 01000000 00000000
// 32位.前面补 00 但是
//00000000 0      0
//0000000 00      0
//000001 011      c
//11100 0100      1c4
//0000 00000000   0

//kd > dq 0x815c4000
//815c4000  00000000`017bd007 00000000`00000000

//kd > !dq 00000000`017bd000+0 * 8      // 为什么少一级 +0*8?可能是函数没写好吧?
//# 17bd000 00000000`017bc007 00000000`0e85e007

//kd > !dq 00000000`017bc000+c * 8
//# 17bc060 00000000`0c1ea007 00000000`023ab007

//kd > !dq 00000000`0c1ea000+1bc * 8    // 为什么多一级 +0? 可能是函数没写好吧?
//# c1eade0 00000000`019bc037 00000000`019bd037

//kd > !dq 00000000`019bc000 + 0
//# 19bc000 00000000`feaddeaf 00000000`81137000

    ept4 = MyEptInitialization();
    Log("ept4 %p", ept4);
    __asm int 3

    __asm
    {
        pushad
        pushfd
        mov g_exit_esp, esp
        mov back_position, offset RETPOSITION
    }

    StartVirtualTechnology();       //自此不归
//=============================================================
RETPOSITION:
    __asm{
        popfd
        popad
    }
    Log("GuestEntry~~~~~~~~~~~~~~~~~~~~", 0)
    *test_data = 0x1234;
    Log("test_data:", *test_data)

	return STATUS_SUCCESS;
}


