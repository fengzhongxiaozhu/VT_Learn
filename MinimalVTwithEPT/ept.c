#include <ntddk.h>
#define PAGE_SIZE 0x1000

#define Log(message,value) {{KdPrint(("[MinVT] %-40s [%p]\n",message,value));}}

ULONG64* ept_PML4T;

static PVOID *pagesToFree;
static int index = 0;


// EPT 拆分
//kd > r esp
//esp = 80549bb0
//kd > r cr3
//cr3 = 0030a000
//kd > !vtop 0030a000 80549bb0
//Virtual address 80549bb0 translates to physical address 549bb0.
//kd > .formats 549bb0
//Binary:  00000000 01010100 10011011 10110000
//只有32位,前面补00 
//00000000 00000000 00000000 01010100 10011011 10110000
//00000000 0      0  *8
//0000000 00      0  *8
//000000 010      2  *8
//10100 1001      149*8
//1011 10110000   BB0

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

ULONG64* MyEptInitialization()
{
    ULONG64 *ept_PDPT, *ept_PDT, *ept_PT;
    PHYSICAL_ADDRESS FirstPtePA, FirstPdePA, FirstPdptePA;
    int a, b, c;

    hook_pa = MmGetPhysicalAddress(test_data);

    initEptPagesPool();
    ept_PML4T = AllocateOnePage();
    ept_PDPT = AllocateOnePage();
    FirstPdptePA = MmGetPhysicalAddress(ept_PDPT);
    *ept_PML4T = (FirstPdptePA.QuadPart) + 7;
    for (a = 0; a < 4; a++)
    {
        ept_PDT = AllocateOnePage();
        FirstPdePA = MmGetPhysicalAddress(ept_PDT);
        *ept_PDPT = (FirstPdePA.QuadPart) + 7;
        ept_PDPT++;
        for (b = 0; b < 512; b++)
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
 