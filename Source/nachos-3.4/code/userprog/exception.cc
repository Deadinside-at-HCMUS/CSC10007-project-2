// exception.cc
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "system.h"

#include "syscall.h"

//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2.
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions
//	are in machine.h.
//----------------------------------------------------------------------

void IncreaseProgramCounter() {
    machine -> WriteRegister(PrevPCReg,
        machine -> ReadRegister(PCReg));

    machine -> WriteRegister(PCReg,
        machine -> ReadRegister(NextPCReg));

    machine -> WriteRegister(NextPCReg,
        machine -> ReadRegister(NextPCReg) + 4);
}

char * User2System(int virtAddr, int limit) {
    int i; // index
    int oneChar;
    char * kernelBuf = NULL;

    kernelBuf = new char[limit + 1]; // need for terminal string

    if (kernelBuf == NULL)
        return kernelBuf;

    memset(kernelBuf, 0, limit + 1);

    // printf("\n Filename u2s:");
    for (i = 0; i < limit; i++) {
        machine -> ReadMem(virtAddr + i, 1, & oneChar);
        kernelBuf[i] = (char) oneChar;
        // printf("%c",kernelBuf[i]);
        if (oneChar == 0)
            break;
    }

    return kernelBuf;
}

int System2User(int virtAddr, int len, char * buffer) {
    if (len < 0)
        return -1;

    if (len == 0)
        return len;

    int i = 0;
    int oneChar = 0;
    do {
        oneChar = (int) buffer[i];
        machine -> WriteMem(virtAddr + i, 1, oneChar);
        i++;
    } while (i < len && oneChar != 0);

    return i;
}

void HaltHandler() {
    DEBUG('a', "Shutdown, initiated by user program!");
    printf("\n\nShutdown, initiated by user program!");
    interrupt -> Halt();
}

void ReadIntHandler() {
    DEBUG('a', "Read integer number from console.\n");

    // Use a fixed-size buffer on the stack
    char buffer[INT_LEN]; 

    int length = synchConsole -> Read(buffer, INT_LEN);
    int result = 0, index = 0;

    bool valid = true; // check if input is a valid number

    if (buffer[index] == '-') {
        // If the first character is '-', increment the index and set the sign
        index++;

        // input only contains '-'
        if (length == 1) {
            machine -> WriteRegister(2, 0);
            printf("\nYour input is not an integer number!\n");
            valid = false;
        }
    }

    while (index < length) {
        if (buffer[index] >= '0' && buffer[index] <= '9') {
            // If the character is a digit, add it to the result
            result = result * 10 + (buffer[index] - '0');
        } else {
            machine -> WriteRegister(2, 0);
            printf("\nYour input is not an integer number!\n");
            valid = false;

            break;
        }

        index++;
    }

    // Multiply the result by -1 if the sign is negative
    if (buffer[0] == '-') {
        result *= -1;
    }

    if (valid) {
        machine -> WriteRegister(2, result);
    }
}

void PrintIntHandler() {
    int n = machine -> ReadRegister(4);
    bool negative = false;

    if (n < 0) {
        negative = true;
        n = -n;
    }

    char s[INT_LEN];
    int i = 0;

    do {
        s[i++] = n % 10 + '0';
    } while (n /= 10);

    if (negative) {
        s[i++] = '-';
    }

    for (int j = i - 1; j >= 0; j--) {
        synchConsole -> Write(&s[j], 1);
    }
}

void ReadCharHandler() {
    char buffer[1];
    int length = synchConsole -> Read(buffer, 1);

    if (length != 1) {
        printf("\nError occurred!\n");
        return;
    }

    machine -> WriteRegister(2, buffer[0]);
}

void PrintCharHandler() {
    char c = (char) machine -> ReadRegister(4);
    synchConsole -> Write( & c, 1);
}

void ReadStringHandler() {
    int virtualAddress = machine -> ReadRegister(4);
    int length = machine -> ReadRegister(5);
    char * buffer = new char[length + 1];

    synchConsole -> Read(buffer, length);
    System2User(virtualAddress, length, buffer);

    delete[] buffer;
}

void PrintStringHandler() {
    int virtualAddress = machine -> ReadRegister(4);
    char * buffer = User2System(virtualAddress, 255);
    int length = 0;
    while (buffer[length] != '\0') {
        ++length;
    }

    synchConsole -> Write(buffer, length + 1);
}


// Exception handler
void ExceptionHandler(ExceptionType which) {
    int type = machine -> ReadRegister(2);

    switch (which) {
            // Handle system call exceptions
        case SyscallException: {
            switch (type) {
                case SC_Halt:
                    HaltHandler();
                    break;

                case SC_ReadInt:
                    ReadIntHandler();
                    break;

                case SC_PrintInt:
                    PrintIntHandler();
                    break;

                case SC_ReadChar:
                    ReadCharHandler();
                    break;

                case SC_PrintChar:
                    PrintCharHandler();
                    break;

                case SC_ReadString:
                    ReadStringHandler();
                    break;

                case SC_PrintString:
                    PrintStringHandler();
                    break;
            }

            IncreaseProgramCounter();
            break;
        }

        // Handle exceptions in machine.h
        case NoException:
            return;

        case PageFaultException:
            DEBUG('a', "\nPageFaultException: No valid translation found!");
            printf("\n\nPageFaultException: No valid translation found!");
            interrupt -> Halt();
            break;

        case ReadOnlyException:
            DEBUG('a', "\nReadOnlyException: Write attempted to page marked read-only!");
            printf("\n\nReadOnlyException: Write attempted to page marked read-only!");
            interrupt -> Halt();
            break;

        case BusErrorException:
            DEBUG('a', "\nBusErrorException: Translation resulted in an invalid physical address!");
            printf("\n\nBusErrorException: Translation resulted in an invalid physical address!");
            interrupt -> Halt();
            break;

        case AddressErrorException:
            DEBUG('a', "\nAddressErrorException: Unaligned reference or one that was beyond the end of the address space!");
            printf("\n\nAddressErrorException: Unaligned reference or one that was beyond the end of the address space!");
            interrupt -> Halt();
            break;

        case OverflowException:
            DEBUG('a', "\nOverflowException: Integer overflow in add or sub!");
            printf("\n\nOverflowException: Integer overflow in add or sub!");
            interrupt -> Halt();
            break;

        case IllegalInstrException:
            DEBUG('a', "\nIllegalInstrException: Unimplemented or reserved instr!");
            printf("\n\nIllegalInstrException: Unimplemented or reserved instr!");
            interrupt -> Halt();
            break;

        case NumExceptionTypes:
            DEBUG('a', "\nNumExceptionTypes: Num exeption types occurred!");
            printf("\n\nNum exeption types occurred!");
            interrupt -> Halt();
            break;
    }
}