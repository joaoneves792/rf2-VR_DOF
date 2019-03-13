#ifndef __hooking_H__
#define __hooking_H__

#include<Windows.h>



void hexDump (char *desc, void *addr, int len);
const unsigned int DisasmLengthCheck(const SIZE_T address, const unsigned int jumplength);
const unsigned int DisasmLog(const SIZE_T address, const unsigned int length);
const DWORD DisasmRecalculateOffset(const SIZE_T srcaddress, const SIZE_T detourAddress);
void* placeDetour(BYTE* src, BYTE* dest, int index);



void* findInstance(void* pvReplica, DWORD dwVTable, bool (*test)(DWORD* current));

#endif