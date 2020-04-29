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

    // file:///E:\Vs\fengzhongxiaozhu\VT_Learn\VT\Source\Image\VMXON.png
    // 31.5 VMM SETUP & TEAR DOWN -> IA32_VMX_BASIC MSR (index 480H)
    // 开启 VT 环境 需要 一块内存.用来储存 VMM 信息.客户机和主机 等等
    // 这块需求的内存大小和其它信息在 
    // APPENDIX AVMX CAPABILITY REPORTING FACILITY -> A.1 BASIC VMX INFORMATION
    // IA32_VMX_BASIC MSR (index 480H)
    // 00d81000 00000001
    // 30:0 版本信息
    // Bit 31 is always 0.
    // 44:32 需要的内存大小
    // bit 48    ?
    // bit 49    ?
    // bit 53:50 ?
    if (!StartVirtualTechnology()){
        Log(("Cpu 开启VT失败"), -1);
        return STATUS_UNSUCCESSFUL;
    }


    //__asm{
    //    pushad
    //    pushfd
    //    mov g_exit_esp, esp
    //    mov back_position, offset RETPOSITION
    //}

    //StartVirtualTechnology();       //自此不归

	return STATUS_SUCCESS;
}


