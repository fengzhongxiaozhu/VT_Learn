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

    // file:///E:\Vs\fengzhongxiaozhu\VT_Learn\VT\Source\Image\VMXON.png
    // 31.5 VMM SETUP & TEAR DOWN -> IA32_VMX_BASIC MSR (index 480H)
    // ���� VT ���� ��Ҫ һ���ڴ�.�������� VMM ��Ϣ.�ͻ��������� �ȵ�
    // ���������ڴ��С��������Ϣ�� 
    // APPENDIX AVMX CAPABILITY REPORTING FACILITY -> A.1 BASIC VMX INFORMATION
    // IA32_VMX_BASIC MSR (index 480H)
    // 00d81000 00000001
    // 30:0 �汾��Ϣ
    // Bit 31 is always 0.
    // 44:32 ��Ҫ���ڴ��С
    // bit 48    ?
    // bit 49    ?
    // bit 53:50 ?
    if (!StartVirtualTechnology()){
        Log(("Cpu ����VTʧ��"), -1);
        return STATUS_UNSUCCESSFUL;
    }


    //__asm{
    //    pushad
    //    pushfd
    //    mov g_exit_esp, esp
    //    mov back_position, offset RETPOSITION
    //}

    //StartVirtualTechnology();       //�Դ˲���

	return STATUS_SUCCESS;
}


