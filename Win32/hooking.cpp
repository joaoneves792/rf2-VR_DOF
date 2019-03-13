#include <hooking.h>
#include <cstdio>

#define BEA_ENGINE_STATIC  /* specify the usage of a static version of BeaEngine */
#define BEA_USE_STDCALL    /* specify the usage of a stdcall version of BeaEngine */
#include <BeaEngine.h>
#pragma comment(lib, "BeaEngine_s_d.lib")
//#pragma comment(lib, "BeaEngine64.lib")

extern FILE* out_file;

struct HookContext{
        BYTE original_code[64];
        SIZE_T dst_ptr;
        BYTE jump[6+8]; //Use 5 for near jump
};
HookContext* hook[2] = {NULL, NULL};


const DWORD DisasmRecalculateOffset(const SIZE_T srcaddress, const SIZE_T detourAddress){
    DISASM disasm;
    memset(&disasm, 0, sizeof(DISASM));
    disasm.EIP = (UIntPtr)srcaddress;
    disasm.Archi = 0x40;
    Disasm(&disasm);
    if(disasm.Instruction.BranchType == JmpType){
        DWORD originalOffset = *((DWORD*) ( ((BYTE*)srcaddress)+1 ));
        fprintf(out_file, "originalOffset: 0x%016x\n", originalOffset);
        DWORD64 hookedFunction = (DWORD64)(srcaddress+originalOffset);
        fprintf(out_file, "hooked pointer: 0x%016x\n", hookedFunction);
        DWORD64 newOffset = (DWORD64)(hookedFunction - detourAddress);
        fprintf(out_file, "new offset: 0x%016x\n", newOffset);
        fflush(out_file);
        return (DWORD)newOffset; //it has to fit in 32bits
    }else{
        return NULL;
    }

}
 
const unsigned int DisasmLog(const SIZE_T address, const unsigned int length){
	    DISASM disasm;
        memset(&disasm, 0, sizeof(DISASM));
        disasm.EIP = (UIntPtr)address;
        disasm.Archi = 0x40;
		
		fprintf(out_file, "DisasmLog:\n");
        unsigned int processed = 0;
        while (processed < length){
            const int len = Disasm(&disasm);
            if (len == UNKNOWN_OPCODE)
            {
                ++disasm.EIP;
            }else{
                fprintf(out_file, "%s\n", disasm.CompleteInstr);
                processed += len;
                disasm.EIP += len;
            }
        }
        return processed;
}

const unsigned int DisasmLengthCheck(const SIZE_T address, const unsigned int jumplength){
        DISASM disasm;
        memset(&disasm, 0, sizeof(DISASM));
 
        disasm.EIP = (UIntPtr)address;
        disasm.Archi = 0x40;
 
        unsigned int processed = 0;
        while (processed < jumplength)
        {
            const int len = Disasm(&disasm);
            if (len == UNKNOWN_OPCODE)
            {
                ++disasm.EIP;
            }
            else if(disasm.Instruction.BranchType == JmpType)
            {
                //Bad news, someone got here first, we have a jump....
                fprintf(out_file, "Found a jump in function to detour...\n");
                fprintf(out_file, "%s\n", disasm.CompleteInstr);
                hexDump("Dumping what was found in function...", (void*)address, 64);
                return -1;
            }
            else
            {
				fprintf(out_file, "%s\n", disasm.CompleteInstr);
                processed += len;
                disasm.EIP += len;
            }
        }
 
        return processed;
}

void hexDump (char *desc, void *addr, int len) {
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char*)addr;
    
    // Output description if given.
    if (desc != NULL)
      fprintf(out_file, "%s:\n", desc);

    if (len == 0) {
      fprintf(out_file, "  ZERO LENGTH\n");
      return;
    }
    if (len < 0) {
      fprintf(out_file, "  NEGATIVE LENGTH: %i\n",len);
      return;
    }

    // Process every byte in the data.
    for (i = 0; i < len; i++) {
      // Multiple of 16 means new line (with line offset).

      if ((i % 16) == 0) {
        // Just don't print ASCII for the zeroth line.
        if (i != 0)
          fprintf(out_file, "  %s\n", buff);

        // Output the offset.
        fprintf(out_file, "  %04x ", i);
      }

      // Now the hex code for the specific character.
      fprintf(out_file, " %02x", pc[i]);

      // And store a printable ASCII character for later.
      if ((pc[i] < 0x20) || (pc[i] > 0x7e))
        buff[i % 16] = '.';
      else
        buff[i % 16] = pc[i];
      buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0) {
      fprintf(out_file, "   ");
      i++;
    }

    // And print the final ASCII bit.
    fprintf(out_file, "  %s\n", buff);
    fflush(out_file);
}

//based on: https://www.unknowncheats.me/forum/d3d-tutorials-and-source/88369-universal-d3d11-hook.html by evolution536
void* placeDetour(BYTE* src, BYTE* dest, int index){
#define _PTR_MAX_VALUE 0x7FFFFFFEFFFF
#define JMPLEN (6+8)
#define PRESENT_INDEX 8
//16 because we are 64bit
#define PRESENT_JUMP_LENGTH 16 
//#define JMPLEN 5 //Use this for near jump

#ifdef LINUX
    MEMORY_BASIC_INFORMATION64 mbi = {0};
    for (DWORD* memptr = (DWORD*)0x10000; memptr < (DWORD*)_PTR_MAX_VALUE; memptr = (DWORD*)(mbi.BaseAddress + mbi.RegionSize)){    
        if(!VirtualQuery(reinterpret_cast<LPCVOID>(memptr),reinterpret_cast<PMEMORY_BASIC_INFORMATION>(&mbi),sizeof(MEMORY_BASIC_INFORMATION))) //Iterate memory by using VirtualQuery
            continue;
#else
    MEMORY_BASIC_INFORMATION mbi;
    for (SIZE_T memptr = (SIZE_T)src; memptr > (SIZE_T)src - 0x80000000; memptr = (SIZE_T)mbi.BaseAddress - 1){
        if (!VirtualQuery((LPCVOID)memptr, &mbi, sizeof(mbi)))
                break;
#endif
        if(mbi.State == MEM_FREE){
            if (hook[index] = (HookContext*)VirtualAlloc((LPVOID)mbi.BaseAddress, sizeof(HookContext), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE)){
                    break;
            }
        }
    }
 
    // If allocating a memory page failed, the function failed.
    if (!hook[index])
        return NULL;

    // Save original code and apply detour. The detour code is:
    // push rax
    // movabs rax, 0xCCCCCCCCCCCCCCCC
    // xchg rax, [rsp]
    // ret
    // 1st calculate how much we need to backup of the src function
    // 2nd copy over the first bytes of the src before they are overritten to original_code
    // 3d copy the detour code to original_code
    // 4th copy the address of the src to the detour 0xCCCCCCCCCCCCCCCC
    BYTE detour[] = { 0x50, 0x48, 0xB8, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0x48, 0x87, 0x04, 0x24, 0xC3 };
    const int length = DisasmLengthCheck((SIZE_T)src, PRESENT_JUMP_LENGTH);
    if(length < 0){
        fprintf(out_file, "Bad news, someone hooked this function already!\n");
        //So we double hook it!!
        //just place a jump to the guys who already hooked it before (Assuming near jump hook, which is not good on x64...)
        DWORD newOffset = DisasmRecalculateOffset((SIZE_T)src, (SIZE_T)hook[index]->original_code);
        hook[index]->original_code[0] = 0xE9;
        memcpy(&(hook[index])->original_code[1], &newOffset, sizeof(DWORD));
    }else{
        memcpy(hook[index]->original_code, src, length);
        memcpy(&(hook[index])->original_code[length], detour, sizeof(detour));
        *(SIZE_T*)&(hook[index])->original_code[length + 3] = (SIZE_T)src + length;
		hexDump("patched original_code", hook[index]->original_code, sizeof(hook[index]->original_code));
		DisasmLog((SIZE_T)hook[index]->original_code, sizeof(hook[index]->original_code));
    }

    // Build a far jump to the destination function.
	BYTE jump[] = { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC};
	memcpy(hook[index]->jump, jump, JMPLEN);
	*(SIZE_T*)&(hook[index])->jump[6] = (SIZE_T)dest; //8 bytes

	//Code for near jump
    //*(WORD*)&(hook[index])->jump = 0xE9;    
    //*(WORD*)(hook[index]->jump + 1) = (WORD)((SIZE_T)dest - (SIZE_T)src - JMPLEN);

     
    // Write the far/near jump code to the src function.
    DWORD flOld = 0;
    VirtualProtect(src, JMPLEN, PAGE_EXECUTE_READWRITE, &flOld);
    memcpy(src, hook[index]->jump, JMPLEN);
	DisasmLog((SIZE_T)src, JMPLEN);
    VirtualProtect(src, JMPLEN, flOld, &flOld);
    
	fprintf(out_file, "MEM: src:%p dest:%p\n", src, dest);
	fflush(out_file);

    // Return a pointer to the code that will jump (using detour) to the src function
    return hook[index]->original_code;
}

//based on https://www.unknowncheats.me/forum/d3d-tutorials-source/121840-hook-directx-11-dynamically.html by smallC
void* findInstance(void* pvReplica, DWORD dwVTable, bool (*test)(DWORD* current)){
#ifdef _AMD64_
#define _PTR_MAX_VALUE 0x7FFFFFFEFFFF
MEMORY_BASIC_INFORMATION64 mbi = { 0 };
#else
#define _PTR_MAX_VALUE 0xFFE00000
MEMORY_BASIC_INFORMATION32 mbi = { 0 };
#endif
    for (DWORD* memptr = (DWORD*)0x10000; memptr < (DWORD*)_PTR_MAX_VALUE; memptr = (DWORD*)(mbi.BaseAddress + mbi.RegionSize)) //For x64 -> 0x10000 ->  0x7FFFFFFEFFFF
    {
        if (!VirtualQuery(reinterpret_cast<LPCVOID>(memptr),reinterpret_cast<PMEMORY_BASIC_INFORMATION>(&mbi),sizeof(MEMORY_BASIC_INFORMATION))) //Iterate memory by using VirtualQuery
            continue;
 
        if (mbi.State != MEM_COMMIT || mbi.Protect == PAGE_NOACCESS || mbi.Protect & PAGE_GUARD) //Filter Regions
            continue;
 
        DWORD* len = (DWORD*)(mbi.BaseAddress + mbi.RegionSize);     //Do once
        DWORD dwVTableAddress;
        for (DWORD* current = (DWORD*)mbi.BaseAddress; current < len; ++current){                                
            __try
                 {
                     dwVTableAddress  = *(DWORD*)current;
                 }
                 __except (1)
                 {
                  continue;
                }
            
                    
            if (dwVTableAddress == dwVTable)
            {
                if (current == (DWORD*)pvReplica){ fprintf(out_file, "Found fake\n"); fflush(out_file); continue; }
                if(!test(current)){
                    fprintf(out_file, "Found a bad instance\n");
					fflush(out_file);
                    continue;
                }
                return ((void*)current);    
            }
        }
    }
    return NULL;

}