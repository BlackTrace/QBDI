#include <stdio.h>
#include <assert.h>
#include <stdbool.h>

#include <QBDI.h>

#define FAKE_RET_ADDR 42
#define STACK_SIZE 0x100000


QBDI_NOINLINE int secretFunc(unsigned int value) {
    return value ^ 0x5c;
}


VMAction showInstruction(VMInstanceRef vm, GPRState *gprState, FPRState *fprState, void *data) {
    // Obtain an analysis of the instruction from the VM
    const InstAnalysis* instAnalysis = qbdi_getInstAnalysis(vm, QBDI_ANALYSIS_INSTRUCTION | QBDI_ANALYSIS_DISASSEMBLY);
    // Printing disassembly
    printf("0x%" PRIRWORD ": %s\n", instAnalysis->address, instAnalysis->disassembly);
    return QBDI_CONTINUE;
}


int main(int argc, char** argv) {
    VMInstanceRef vm = NULL;
    uint8_t *fakestack = NULL;

    // init VM
    qbdi_initVM(&vm, NULL, NULL);

    // Get a pointer to the GPR state of the VM
    GPRState *state = qbdi_getGPRState(vm);
    assert(state != NULL);

    // Setup initial GPR state, this fakestack will produce a ret NULL at the end of the execution
    bool res = qbdi_allocateVirtualStack(state, STACK_SIZE, &fakestack);
    assert(res == true);

    // Add callback on our instruction range
    uint32_t uid = qbdi_addCodeRangeCB(vm, (rword) &secretFunc, (rword) &secretFunc + 100, QBDI_PREINST, showInstruction, vm);
    assert(uid != QBDI_INVALID_EVENTID);

    // add executable code range
    res = qbdi_addInstrumentedModuleFromAddr(vm, (rword) &main);
    assert(res == true);

    // Simulate a call in stack
    qbdi_simulateCall(state, FAKE_RET_ADDR, 1, (rword) 666);

    // call secretFunc using VM, custom state and fake stack
    // eq: secretFunc(666);
    res = qbdi_run(vm, (rword) secretFunc, (rword) FAKE_RET_ADDR);
    assert(res == true);

    // get return value from current state
    rword retval = QBDI_GPR_GET(state, REG_RETURN);
    printf("[*] retval=0x%"PRIRWORD"\n", retval);

    // free everything
    qbdi_alignedFree(fakestack);
    qbdi_terminateVM(vm);

    return 0;
}
