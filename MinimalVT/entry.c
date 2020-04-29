#pragma once

#include <ntddk.h>
#include "vtasm.h"
#include "vtsystem.h"


VOID DriverUnload(PDRIVER_OBJECT pDriver){
    //StopVirtualTechnology();
    DbgPrint("Driver is unloading...\r\n");
}

NTSTATUS 
  DriverEntry( 
    PDRIVER_OBJECT  pDriver,
    PUNICODE_STRING pRegistryPath){
    pDriver->DriverUnload = DriverUnload;

    DbgPrint("Driver Entered!\r\n");

    // Tips: 如果看不懂汇编指令.那么编译成 .obj 文件 ..拖进IDA里面看
    // Vmx_VmCall(); 

    // 首先第一件事就是查询 Cpu 是否支持 Vm
    // 23.6 DISCOVERING SUPPORT FOR VMX
    if (!IsVTEnabled()){
        Log( ("Cpu 不支持VT功能"), -1);
        return STATUS_UNSUCCESSFUL;
    }

    if (!StartVirtualTechnology()){
        Log(("Cpu 开启VT失败"), -1);
        return STATUS_UNSUCCESSFUL;
    }
    // file:E:\Vs\Learning\VT_Learn\VT\VT\Source\Image\VMXON.png
    // 31.5 VMM SETUP & TEAR DOWN
    __asm{
        pushad
        pushfd
        mov g_exit_esp, esp
        mov back_position, offset RETPOSITION
    }

    //StartVirtualTechnology();       //自此不归
RETPOSITION:
    __asm{
        popfd
        popad
    }
    Log("GuestEntry~~~~~~~~~~~~~~~~~~~~", 0)

	return STATUS_SUCCESS;
}


