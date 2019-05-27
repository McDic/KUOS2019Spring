/*
	Author: Minsung Kim / spongbob9876@gmail.com
	
	Description: 
		Term project for Korea University's Operating System lecture.
		
	Goals:
		(o) - Creating process: ID, CPU burst, IO burst, Arrival time, Given priority 
		(x) - Config: Ready queue, Waiting queue
		(x) - Schedule (Implement both preemptive and non-preemptive)
				(x) - First Come First Serve
				(x) - Shortest Job First
				(x) - Given Priority
				(x) - Round Robin
		(x) - Gantt chart displaying
		(x) - Evaluation: Calculate average waiting time, turnaround time
		
*/

// --------------------------------------------------------------------------------------------------------------------
// Libraries

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// --------------------------------------------------------------------------------------------------------------------
// Data structure - Boolean

typedef enum {false, true} bool;

// --------------------------------------------------------------------------------------------------------------------
// Random

// Seed
const unsigned int seedRefreshInterval = 10, callThreshold = 1<<16;
unsigned int lastSetSeed = -1, called = 0;
void setSeed(){
	unsigned int currentTime = (unsigned int)time(NULL);
	if(currentTime - lastSetSeed >= seedRefreshInterval){
		srand(currentTime);
		lastSetSeed = currentTime;
		called = 0;
	}
}

// Redeclare random [0 ~ RAND_MAX] with advanced features
int random(){
	called++;
	if(lastSetSeed == -1 || callThreshold <= called) setSeed();
	return rand();
}

// Uniform number has $bits bits 
int randomBits(bool positiveOnly){
	int val = (random() << 15) + random();
	val ^= (random() & (positiveOnly ? 1:3)) << 30;
	return val;
}

// Pseudo-uniform random between min_ and max_
unsigned int superrandom(unsigned int min_, unsigned int max_){
	if(min_ > max_){
		printf("Error: Min value(%u) is bigger than max value(%u)\n", min_, max_);
		return -1;
	}
	return (unsigned int)randomBits(false) % (max_ - min_ + 1) + min_;
}

// --------------------------------------------------------------------------------------------------------------------
// Process structure

// Nonce for identifying process uniquely
static int processCounter = 0;
typedef enum {criteria_FCFS, criteria_SJF, criteria_P} ProcessComparisonCriteria;

// Struct
struct Process__{
	
	// Native features
	int PID;
	int CPUburst, IOburst;
	int arrivalTime;
	int givenPriority; // Small priority value means high priority
	
	// Statistics
	int CPUburstleft;
	int turnaroundTime, waitingTime;
	
}; typedef struct Process__ Process;

// Return True if p1 < p2, otherwise False.
bool processComparisonGT(Process p1, Process p2, ProcessComparisonCriteria criteria){
	switch(criteria){ // PID comparison is final method, it's used after this switch
		case criteria_FCFS: // (arrivalTime, givenPriority, PID)
			if(p1.arrivalTime != p2.arrivalTime) return p1.arrivalTime < p2.arrivalTime;
			else if(p1.givenPriority != p2.givenPriority) return p1.givenPriority < p2.givenPriority;
			else break;
		case criteria_SJF: // (CPUburstleft, givenPriority, PID)
			if(p1.CPUburstleft != p2.CPUburstleft) return p1.CPUburstleft < p2.CPUburstleft;
			else if(p1.givenPriority != p2.givenPriority) return p1.givenPriority < p2.givenPriority;
			else break;
		case criteria_P: // (givenPriority, PID)
			if(p1.givenPriority != p2.givenPriority) return p1.givenPriority < p2.givenPriority;
			else break;
	} return p1.PID < p2.PID;
}

// Create new process with automatically generated PID
Process createProcess(int CPUburst, int IOburst, int arrivalTime, int givenPriority){
	Process newCreatedOne;
	newCreatedOne.PID = processCounter++;
	newCreatedOne.CPUburst = CPUburst, newCreatedOne.IOburst = IOburst;
	newCreatedOne.arrivalTime = arrivalTime;
	newCreatedOne.givenPriority = givenPriority;
	newCreatedOne.CPUburstleft = CPUburst;
	newCreatedOne.turnaroundTime = 0, newCreatedOne.waitingTime = 0;
	return newCreatedOne;
}
Process* createProcessAlloc(int CPUburst, int IOburst, int arrivalTime, int givenPriority){
	Process* newCreatedOne = (Process*)malloc(sizeof(Process));
	*newCreatedOne = createProcess(CPUburst, IOburst, arrivalTime, givenPriority);
	return newCreatedOne;
}

// Create random process
Process createRandomProcess(
		int maxCPUburst, int maxIOburst, int minimumArrival, int maximumArrival, 
		int minPriority, int maxPriority){
	return createProcess(superrandom(1, maxCPUburst), superrandom(0, maxIOburst), 
		superrandom(minimumArrival, maximumArrival), superrandom(minPriority, maxPriority));
}
Process* createRandomProcessAlloc(
		int maxCPUburst, int maxIOburst, int minimumArrival, int maximumArrival, 
		int minPriority, int maxPriority){
	Process* newCreatedOne = (Process*)malloc(sizeof(Process));
	*newCreatedOne = createRandomProcess(maxCPUburst, maxIOburst, 
		minimumArrival, maximumArrival, minPriority, maxPriority);
	return newCreatedOne;
}

// Deep copy
Process* deepCopyProcess(Process* origin){
	Process* newProcess = (Process*)malloc(sizeof(Process));
	*newProcess = *origin;
	return newProcess;
}

// --------------------------------------------------------------------------------------------------------------------
// Data structure - Deque

// Single bi-directional node contains customized feature
struct BiDirectionalNode__{
	struct BiDirectionalNode__ *prev, *next;
	void *feature;
}; typedef struct BiDirectionalNode__ BiDirectionalNode;

// Construct new node
BiDirectionalNode* newBiDirectionalNode(void *feature){
	BiDirectionalNode* newNode = (BiDirectionalNode*)malloc(sizeof(BiDirectionalNode));
	newNode->prev = NULL, newNode->next = NULL;
	newNode->feature = feature;
	return newNode;
}

// Whole deque
struct Deque__{
	BiDirectionalNode *front, *back; // front = back->next->...->next, back = front->prev->...->prev
	int maxSize, currentSize;
	char* name;
}; typedef struct Deque__ Deque;

// Construct base deque
Deque* newDeque(int maxsize, const char* name){
	if(maxsize < 0){ // Invalid size leads to return NULL
		printf("Negative max size(%d) given in function newDeque\n", maxsize);
		return NULL;
	}
	Deque *newDq = (Deque*)malloc(sizeof(Deque));
	newDq->front = NULL, newDq->back = NULL;
	newDq->currentSize = 0; newDq->maxSize = 0;
	newDq->name = strdup(name);
	return newDq;
}

// Push new feature to front of the deque. Return true if push was successful, otherwise false.
bool pushFront(Deque *dq, void* newFeature){
	if(dq->maxSize == 0 || dq->currentSize < dq->maxSize){ // If given dq is unbounded or have enough extra space
		BiDirectionalNode* newNode = newBiDirectionalNode(newFeature);
		if(dq->currentSize == 0){ // Create single
			dq->front = newNode;
			dq->back = newNode;
		}
		else{ // (front) newNode dq->front ... dq->back (back)
			newNode->prev = dq->front;
			dq->front->next = newNode;
			dq->front = newNode;
		}
		dq->currentSize++;
		return true;
	}
	else{ // Deque reached size limit; Cancel allocation.
		printf("Deque <%s> size limit reached in pushFront\n", dq->name);
		return false;
	}
}

// Pop back
void* popBack(Deque *dq){
	if(dq->currentSize == 0){
		printf("Tried pop_back to empty Deque <%s>\n", dq->name);
		return NULL;
	}
	else{
		BiDirectionalNode* poppedNode = dq->back;
		void* poppedFeature = poppedNode->feature;
		dq->back = poppedNode->next;
		if(dq->back != NULL) dq->back->prev = NULL;
		dq->currentSize--;
		free(poppedNode);
		return poppedFeature;
	}
}

// Clear all elements
void clearDeque(Deque *dq, bool freeFeature){
	while(dq->currentSize > 0){
		void* feature = popBack(dq);
		if(freeFeature) free(feature);
	}
	dq->front = NULL;
	dq->back = NULL;
}

// Delete deque itself
void deleteDeque(Deque *dq, bool freeFeature){
	clearDeque(dq, freeFeature);
	free(dq);
}

// --------------------------------------------------------------------------------------------------------------------
// Naive scheduling
// Given process array: *processes.

// Pick processes in [start, end) and swap (start, optimal).
void pick(Process *processes, int start, int end, ProcessComparisonCriteria criteria){
	int targetIndex = start;
	for(int i=start+1; i<end; i++){
		// if Pi < Pt => if Pi has higher priority than Pt
		if(processComparisonGT(*(processes+i), *(processes+targetIndex), criteria)){ 
			targetIndex = i;
		}
	}
	Process temp = *(processes+start);
	*(processes+start) = *(processes+targetIndex);
	*(processes+targetIndex) = temp;
}

// Selection sort with given criteria in range [start, end).
void selectionSort(Process *processes, int start, int end, ProcessComparisonCriteria criteria){
	for(int i=start; i<end; i++) pick(processes, i, end, criteria);
}

// Represent
void reprSingle(Process *p){
	printf("[Process #%d: CPU burst %d (%d left), IO burst %d, arrival time %d, given priority = %d]",
		p->PID, p->CPUburst, p->CPUburstleft, p->IOburst, p->arrivalTime, p->givenPriority);
}
void reprMulti(Process *p, int limit){
	printf("Representing processes:\n");
	for(int i=0; i<limit; i++){
		printf("  "); reprSingle(p+i); printf("\n");
	}
}

// --------------------------------------------------------------------------------------------------------------------
// Timeline structure

// Timeline
#define maxTimelineLength 10005
struct Timeline__{
	
	// Nonarray attributes
	int timelinesize, processNum, timestamp;
	Process *processes;
	
	// Timerelated attributes: [(usedProcesses[i], interval[i][0], interval[i][1]), ...]
	// For all i, <PID = usedProcesses[i]> process did job in time interval [interval[i][0], interval[i][1])
	int interval[maxTimelineLength][2];
	Process *usedProcesses[maxTimelineLength]; // This can be null since CPU can kill time without doing any jobs
	
}; typedef struct Timeline__ Timeline;

// Create new one
Timeline newTimeline(Process *processes, int processNum){
	Timeline newCreatedOne;
	newCreatedOne.timelinesize = 0;
	newCreatedOne.timestamp = 0;
	newCreatedOne.processes = processes;
	newCreatedOne.processNum = processNum;
	return newCreatedOne;
}

// Make job. If given interval is bigger than given process's length then make interval lower
// Parameter 'process' can be NULL if we intended to CPU kills time
void doJobFor(Timeline *timeline, Process *process, int duration){
	
	// Critical validation
	if(duration <= 0){ // Duration validation
		printf("Invalid duration interval(%d) got in function doJobFor\n", duration);
		return;
	}
	else if(process != NULL && process->CPUburstleft == 0){ // Tried to give job for burned process
		printf("Given "); reprSingle(process); printf(" is already burned out in function doJobFor\n");
		return;
	}
	
	// Weak validation
	if(process != NULL && process->CPUburstleft < duration){ // Duration modification
		duration = process->CPUburstleft;
	}
	
	// Process modification
	if(process != NULL) process->CPUburstleft -= duration;
	
	// Timeline modification
	timeline->timestamp += duration;
	if(timeline->timelinesize == 0 || timeline->usedProcesses[timeline->timelinesize - 1] != process){ // Append new one
		timeline->usedProcesses[timeline->timelinesize] = process;
		timeline->interval[timeline->timelinesize][0] = timeline->timestamp - duration;
		timeline->interval[timeline->timelinesize][1] = timeline->timestamp;
		timeline->timelinesize++;
	}
	else{ // Modify latest one
		timeline->interval[timeline->timelinesize - 1][1] = timeline->timestamp;
	}
}

// --------------------------------------------------------------------------------------------------------------------
// Naive scheduling

// First Come First Serve
Timeline ScheduleFCFS(Process *processes, int processNum){
	Timeline timeline = newTimeline(processes, processNum);
	selectionSort(processes, 0, processNum, criteria_FCFS);
	for(int i=0; i<processNum; i++){
		if(timeline.timestamp < (processes+i)->arrivalTime)
			doJobFor(&timeline, NULL, (processes+i)->arrivalTime - timeline.timestamp);
		doJobFor(&timeline, processes+i, (processes+i)->CPUburstleft);
	}
	return timeline;
}

// Shortest Job First
Timeline ScheduleSJF(Process *processes, int processNum, bool preemptive){
	
}

// --------------------------------------------------------------------------------------------------------------------
// Gantt chart displaying

void GanttChart(Timeline timeline){
	// results = [PID[0,1), PID[1,2), PID[2,3), ..., -1 (end)]
}

// Evaluation
void evaluation(int processNum, int baseScale){
	
	// Parameter evaluation
	if(baseScale <= 0 || processNum <= 0){
		printf("Nonpositive process num(%d) or base scale(%d) given - evaluation will be terminated.\n",
			processNum, baseScale);
		return;
	}

	// Process randomizing
	Process *processes = (Process*)malloc(sizeof(Process) * processNum);
	for(int i=0; i<processNum; i++) *(processes+i) = createRandomProcess(baseScale * 5, 0, 0, 20 * i * baseScale, 1, 5);
	Timeline FCFSscheduled = ScheduleFCFS(processes, processNum);
	for(int i=0; i<FCFSscheduled.timelinesize; i++){
		printf("Scheduled %03d - %03d: ", FCFSscheduled.interval[i][0], FCFSscheduled.interval[i][1]);
		if(FCFSscheduled.usedProcesses[i] == NULL) printf("No process runned");
		else reprSingle(FCFSscheduled.usedProcesses[i]);
		printf("\n");
	}
}

// --------------------------------------------------------------------------------------------------------------------
// Functionality testing

// Testing deque functionalities
void DequeFunctionalityTest1(){
	
	Deque* dq = newDeque(100, "test deque");
	for(int i=0; i<100; i++){
		int *feature = (int*)malloc(sizeof(int));
		*feature = superrandom(0, 1000);
		bool status = pushFront(dq, (void*)feature);
		if(status){
			printf("Pushed %d into deque <%s>\n", *feature, dq->name);
			int randomsta = superrandom(0, 2);
			if(randomsta){
				printf("Popping: ");
				void *feature = popBack(dq);
				printf("Popped %d from deque <%s>\n", *(int*)feature, dq->name);
			}
		}
		else break;
	}
	deleteDeque(dq, true);
}

// Testing selection sort on processes
void SelectionSortFunctionalityTest(){
	
	// Process randomizing
	const int processNum = 10;
	Process *processes = (Process*)malloc(sizeof(Process) * processNum);
	for(int i=0; i<processNum; i++) *(processes+i) = createRandomProcess(20, 0, 0, 10, 1, 5);
	
	// Sort in 3 different criterias
	const char criteria_str[3][100] = {"FCFS", "SJF", "Priority"};
	printf("Before sorted - "); reprMulti(processes, processNum);
	for(int criteria = 0; criteria < 3; criteria++){
		selectionSort(processes, 0, processNum, criteria);
		printf("\n\nAfter sorted by %s - ", criteria_str[criteria]);
		reprMulti(processes, processNum);
	}
	free(processes);
}

// --------------------------------------------------------------------------------------------------------------------
// Main function

int main(void){
	
	//DequeFunctionalityTest1();
	//SelectionSortFunctionalityTest();
	evaluation(10, 2);
	
	return 0;
}
