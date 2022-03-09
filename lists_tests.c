#include "lists.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "staticMalloc.h"

void linearListTests();
void circularListTests();

char mallocArray[1000];

int main() {
    printf("Running tests...\n");
    initMalloc(mallocArray);
    printf("Running circular linked list tests...");
    circularListTests();
    printf(" Passed!\n");
    printf("Running linear linked list tests...");
    linearListTests();
    printf(" Passed!\n");
    printf("All tests passed!\n");
    return 0;
}

void linearListTests() {
    int nums[10] = {1,2,3,4,5,6,7,8,9,10};
    list_t testList = create_list();
    list_t listCur = testList;
    listCur = add_as_next(listCur, (void *)&nums[0]);
    testList = listCur;
    //printf("initial list: 0x%x\n", (unsigned int)testList);
    for (int i = 1; i < 10; i++) {
        listCur = add_as_next(listCur, (void *)&nums[i]);
    }
    //printf("done with init\n");
    //printf("final list: 0x%x\n", (unsigned int)testList);
    for (int i = 0; i < 10; i++) {
        assert(nums[i] == *(int *)testList->data);
        //printf("0x%x\n", (unsigned int)testList->data);
        //printf("0x%x\n", (unsigned int)&nums[i]);
        if (testList!=NULL)
            testList = testList->next;
            //printf("next node: 0x%x\n", (unsigned int)testList);
    }
    //printf("final node: 0x%x\n", (unsigned int)testList->next);
}

void circularListTests() {
    int nums[10] = {1,2,3,4,5,6,7,8,9,10};
    list_t testList = create_circular_list((void *)&nums[0]);
    list_t listCur = testList;
    testList = listCur;
    //printf("initial list: 0x%x\n", (unsigned int)testList);
    for (int i = 1; i < 10; i++) {
        listCur = add_as_next(listCur, (void *)&nums[i]);
    }
    //printf("done with init\n");
    //printf("final list: 0x%x\n", (unsigned int)testList);
    for (int i = 0; i < 20; i++) {
        assert(nums[i%10] == *(int *)testList->data);
        //printf("0x%x\n", (unsigned int)testList->data);
        //printf("0x%x\n", (unsigned int)&nums[i%10]);
        if (testList!=NULL)
            testList = testList->next;
            //printf("next node: 0x%x\n", (unsigned int)testList);
    }
    //printf("final node: 0x%x\n", (unsigned int)testList->next);
}

// TODO: delete node tests