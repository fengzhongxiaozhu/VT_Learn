#include <ntddk.h>
#define PAGE_SIZE 0x1000

#define Log(message,value) {{KdPrint(("[MinVT] %-40s [%p]\n",message,value));}}

ULONG64* ept_PML4T;

static PVOID *pagesToFree;
static int index = 0;


void initEptPagesPool()
{
    pagesToFree = ExAllocatePoolWithTag(NonPagedPool, 12*1024*1024, 'ept');
    if(!pagesToFree)
        __asm int 3
    RtlZeroMemory(pagesToFree, 12*1024*1024);
}

static ULONG64* AllocateOnePage()
{
    PVOID page;
    page = ExAllocatePoolWithTag(NonPagedPool, PAGE_SIZE, 'ept');
    if(!page)
        __asm int 3

    RtlZeroMemory(page, PAGE_SIZE);
    pagesToFree[index] = page;
    index++;
    return (ULONG64 *)page;
}

extern PULONG test_data;
PHYSICAL_ADDRESS hook_pa;
ULONG64 *hook_ept_pt;


//file://E:\Vs\fengzhongxiaozhu\VT_Learn\VTEPT\Source\PML4E.png
ULONG64* MyEptInitialization()
{
    ULONG64 *ept_PDPT, *ept_PDT, *ept_PT;
    PHYSICAL_ADDRESS FirstPtePA, FirstPdePA, FirstPdptePA;
    int a, b, c;

    hook_pa = MmGetPhysicalAddress(test_data);

    initEptPagesPool();                                 
    ept_PML4T = AllocateOnePage();                      // 蓝
    ept_PDPT = AllocateOnePage();                       // 红
    FirstPdptePA = MmGetPhysicalAddress(ept_PDPT);      
    *ept_PML4T = (FirstPdptePA.QuadPart) + 7;           // 修改属性 类似 chmod 777
    for (a = 0; a < 4; a++)                             // 黄
    {
        ept_PDT = AllocateOnePage();               
        FirstPdePA = MmGetPhysicalAddress(ept_PDT);
        *ept_PDPT = (FirstPdePA.QuadPart) + 7;
        ept_PDPT++;
        for (b = 0; b < 512; b++)                       // 绿
        {
            ept_PT = AllocateOnePage();                 
            FirstPtePA = MmGetPhysicalAddress(ept_PT);
            *ept_PDT = (FirstPtePA.QuadPart) + 7;
            ept_PDT++;
            for (c = 0; c < 512; c++)                  
            {
                *ept_PT  = ((a << 30) | (b << 21) | (c << 12) | 0x37) & 0xFFFFFFFF;
                if ((((a << 30) | (b << 21) | (c << 12) | 0x37) & 0xFFFFF000) == (hook_pa.LowPart & 0xFFFFF000))
                {
                    *ept_PT = 0;
                    hook_ept_pt = ept_PT;
                }
                ept_PT++;
            }
        }
    }

    return ept_PML4T;
}

void MyEptFree()
{
    int i;
    for (i = 0; i < index; i++) {
        ExFreePool(pagesToFree[i]);
    }
    ExFreePool(pagesToFree);
    index = 0;
}
 