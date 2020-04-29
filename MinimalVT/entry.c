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

    // Tips: ������������ָ��.��ô����� .obj �ļ� ..�Ͻ�IDA���濴
    // Vmx_VmCall(); 

    // ���ȵ�һ���¾��ǲ�ѯ Cpu �Ƿ�֧�� Vm
    // 23.6 DISCOVERING SUPPORT FOR VMX
    if (!IsVTEnabled()){
        Log( ("Cpu ��֧��VT����"), -1);
        return STATUS_UNSUCCESSFUL;
    }

    if (!StartVirtualTechnology()){
        Log(("Cpu ����VTʧ��"), -1);
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

    //StartVirtualTechnology();       //�Դ˲���
RETPOSITION:
    __asm{
        popfd
        popad
    }
    Log("GuestEntry~~~~~~~~~~~~~~~~~~~~", 0)

	return STATUS_SUCCESS;
}


