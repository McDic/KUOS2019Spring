/*
	Author: Minsung Kim / spongbob9876@gmail.com
	
	Description: 
		Term project for Korea University's Operating System lecture.
		
	Goals:
		(o) - Creating process: ID, CPU burst, IO burst, Arrival time, Given priority 
		(?) - Config: Ready queue, Waiting queue
		(o) - Random I/O performing
			- Schedule (Implement both preemptive and non-preemptive)
				(o) - First Come First Serve
				(o) - Shortest Job First
				(o) - Given Priority
				(o) - Round Robin
		(o) - Gantt chart displaying
		(o) - Evaluation: Calculate average waiting time, turnaround time
			- Additional features
				(o) - Dynamically changing priorities
				(o) - Aging priorities (Preemptive, Non-preemptive)
				(o) - Context switching cost (Generalized adaptivity)
*/

// --------------------------------------------------------------------------------------------------------------------
// Libraries

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

// --------------------------------------------------------------------------------------------------------------------
// Constants

const int inf = 1<<30;
typedef enum {false, true} bool;

// --------------------------------------------------------------------------------------------------------------------
// Base functions

// Min and Max
int min2(int a, int b){return a<b ? a:b;}
int max2(int a, int b){return a>b ? a:b;}

// Repeated printing.
void printRepeat(const char *line, int count, bool everyNewline){
	for(int i=0; i<count; i++){
		printf("%s", line);
		if(everyNewline) printf("\n");
	}
}

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
int myRandom(){
	called++;
	if(lastSetSeed == -1 || callThreshold <= called) setSeed();
	return rand() % (1<<15);
}

// Uniform number has $bits bits 
int randomBits(bool positiveOnly){
	int val = (myRandom() << 15) + myRandom();
	val ^= (myRandom() & (positiveOnly ? 1:3)) << 30;
	return val;
}

// Pseudo-uniform random between min_ and max_
unsigned int superrandom(unsigned int min_, unsigned int max_){
	if(min_ > max_){
		printf("[Error] Min value(%u) is bigger than max value(%u)\n", min_, max_);
		return -1;
	}
	return (unsigned int)randomBits(false) % (max_ - min_ + 1) + min_;
}

// --------------------------------------------------------------------------------------------------------------------
// Process structure

// Nonce for identifying process uniquely
static int processCounter = 1;
typedef enum {
	criteria_FCFS, criteria_SJF, criteria_P,
	criteria_AGING, criteria_RR, criteria_PDy
} ProcessComparisonCriteria;
const bool ProcessComparisonTicking[6] = {false, false, false, true, false, false};
const char ProcessComparisonNames[6][100] = 
	{"CriteriaFCFS", "CriteriaSJF", "CriteriaPriority", "CriteriaAging", "CriteriaRoundRobin",
	 "CriteriaPriorityDynamic"};
const double agingFactor = pow(0.75, 1);

// Struct
struct Process__{
	
	// Native features
	int PID;
	int CPUburst, IOburst;
	int arrivalTime;
	int givenPriority; // Small priority value means high priority
	
	// Statistics
	bool RRcycleUsed; // For Round Robin
	int CPUburstleft;
	int finishedTime;
	
}; typedef struct Process__ Process;

// Return True if p1 < p2, otherwise False.
int processComparisonValues_timelinetimestamp = -1;
bool processComparisonGT(Process p1, Process p2, ProcessComparisonCriteria criteria){
	double calculatedValue1, calculatedValue2;
	switch(criteria){ // PID comparison is final method, it's used after this switch
		case criteria_FCFS: // (arrivalTime)
			if(p1.arrivalTime != p2.arrivalTime) return p1.arrivalTime < p2.arrivalTime;
			else break;
		case criteria_SJF: // (CPUburstleft, givenPriority)
			if(p1.CPUburstleft != p2.CPUburstleft) return p1.CPUburstleft < p2.CPUburstleft;
			else if(p1.givenPriority != p2.givenPriority) return p1.givenPriority < p2.givenPriority;
			else break;
		case criteria_P: // (givenPriority)
		case criteria_PDy:
			if(p1.givenPriority != p2.givenPriority) return p1.givenPriority < p2.givenPriority;
			else break;
		case criteria_AGING: // (aged priority = CPUburstleft * agingFactor ** age)
			calculatedValue1 = pow(agingFactor, processComparisonValues_timelinetimestamp - p1.arrivalTime) / (double)(1 + p1.CPUburstleft);
			calculatedValue2 = pow(agingFactor, processComparisonValues_timelinetimestamp - p2.arrivalTime) / (double)(1 + p2.CPUburstleft);
			if(calculatedValue1 != calculatedValue2) return calculatedValue1 < calculatedValue2;
			else break;
		case criteria_RR: // (consumed count)
			if(p1.RRcycleUsed == true && p2.RRcycleUsed == false) return false;
			else if(p1.RRcycleUsed == false && p2.RRcycleUsed == true) return true;
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
	newCreatedOne.finishedTime = 0;
	newCreatedOne.RRcycleUsed = false;
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
Process* deepCopyProcesses(Process *origins, int processNum){
	Process *newProcesses = (Process*)malloc(sizeof(Process) * processNum);
	for(int i=0; i<processNum; i++) *(newProcesses+i) = *(origins+i);
	return newProcesses;
}

// Represent
typedef enum {
	ProcessRepresentMinimal, ProcessRepresentBurst,
	ProcessRepresentStatistics 
} ProcessRepresentingMode;
void reprSingleProcess(Process *p, ProcessRepresentingMode mode){
	if(p == NULL) printf("[Process NULL]");
	else switch(mode){
		case ProcessRepresentMinimal:
			printf("[Process #%03d: CPU burst %03d, I/O burst %03d, arrival time %03d, given priority = %03d]",
				p->PID, p->CPUburst, p->IOburst, p->arrivalTime, p->givenPriority); break;
		case ProcessRepresentBurst:
			printf("[Process #%03d: CPU burst %03d (%03d left), I/O burst %03d, arrival time %03d, given priority = %03d]",
				p->PID, p->CPUburst, p->CPUburstleft, p->IOburst, p->arrivalTime, p->givenPriority); break;
		case ProcessRepresentStatistics:
			printf("[Process #%03d: CPU %03d, I/O %03d, Arrival %03d, Prio = %03d, Turnaround = %03d, Waiting = %03d]",
				p->PID, p->CPUburst, p->IOburst, p->arrivalTime, p->givenPriority, p->finishedTime - p->arrivalTime, 
				p->finishedTime - p->arrivalTime - p->CPUburst); break;
	}
}
void reprMultiProcesses(Process *p, int limit, ProcessRepresentingMode mode){
	printf("Representing processes:\n");
	for(int i=0; i<limit; i++){
		printf("  "); reprSingleProcess(p+i, mode); printf("\n");
	}
}

// --------------------------------------------------------------------------------------------------------------------
// Naive scheduling
// Given process array: *processes.

// Pick processes in [start, end) and swap (start, optimal).
int pick(Process *processes, int start, int end, ProcessComparisonCriteria criteria){
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
	return targetIndex;
}

// Selection sort with given criteria in range [start, end).
void selectionSort(Process *processes, int start, int end, ProcessComparisonCriteria criteria){
	for(int i=start; i<end; i++) pick(processes, i, end, criteria);
}

// --------------------------------------------------------------------------------------------------------------------
// Timeline structure

// Timeline
#define maxTimelineLength 10005
struct Timeline__{
	
	// Nonarray attributes
	int timelinesize, timestamp;
	int processNum, contextswitchingcost;
	Process *processes;
	
	// Timerelated attributes: [(usedProcessesPID[i], interval[i][0], interval[i][1]), ...]
	// For all i, <PID = usedProcessesPID[i]> process did job in time interval [interval[i][0], interval[i][1])
	int interval[maxTimelineLength][2];
	int usedProcessesPID[maxTimelineLength]; // This can be null since CPU can kill time without doing any jobs
	
}; typedef struct Timeline__ Timeline;

static int globalRRQuantumTime = 10;

// Create new one
Timeline newTimeline(Process *processes, int processNum, int contextswitchingcost){
	Timeline newCreatedOne;
	newCreatedOne.timelinesize = 0;
	newCreatedOne.timestamp = 0;
	newCreatedOne.processes = processes;
	newCreatedOne.processNum = processNum;
	newCreatedOne.contextswitchingcost = contextswitchingcost;
	return newCreatedOne;
}

// Make job. If given interval is bigger than given process's length then make interval lower
// Parameter 'process' can be NULL if we intended to CPU kills time
void doJobFor(Timeline *timeline, Process *process, int duration){
	//printf("Do job process #%d for %d seconds\n", process == NULL ? -1 : process->PID, duration);
	
	// Critical validation
	if(duration <= 0){ // Duration validation
		printf("[Error] Invalid duration interval(%d) got in function doJobFor\n", duration);
		exit(-1);
	}
	else if(process != NULL && process->CPUburstleft == 0){ // Tried to give job for burned process
		printf("[Error] Given "); reprSingleProcess(process, ProcessRepresentMinimal); 
		printf(" is already burned out in function doJobFor\n");
		exit(-1);
	}
	
	// Weak validation
	if(process != NULL && process->CPUburstleft < duration){ // Duration modification
		printf("[Warning] Given duration(%d) is larger than process's CPU burst left(%d), automatically fixed.\n",
			duration, process->CPUburstleft);
		duration = process->CPUburstleft;
	}
	
	// Debug
	/*printf("Timestamp %03d, timeline size %d, doing job = ", timeline->timestamp, timeline->timelinesize);
	if(process == NULL) printf("<nothing>, "); else printf("<PID #%d>, ", process->PID);
	printf("last used process = ");
	if(timeline->timelinesize == 0 || timeline->usedProcessesPID[timeline->timelinesize - 1] == -1) printf("<nothing>, ");
	else printf("<PID #%d>, ", timeline->usedProcessesPID[timeline->timelinesize - 1]); //printf("\n");*/
	
	// Timeline modification
	if(timeline->timelinesize == 0 || 
		(timeline->usedProcessesPID[timeline->timelinesize - 1] != -1 && process == NULL) || 
		(process != NULL && timeline->usedProcessesPID[timeline->timelinesize - 1] != process->PID)){ // Append new one
		//printf("Yeah it's different.\n");
		if(process != NULL && timeline->timelinesize > 0 && 
			timeline->usedProcessesPID[timeline->timelinesize - 1] != -1 &&
			timeline->contextswitchingcost > 0) {
			// Previous process and current processes are different and not null -> Add context switching cost
			//printf("Adding context switching cost:\n");
			doJobFor(timeline, NULL, timeline->contextswitchingcost);
		}
		timeline->timestamp += duration;
		timeline->usedProcessesPID[timeline->timelinesize] = (process == NULL ? -1 : process->PID);
		timeline->interval[timeline->timelinesize][0] = timeline->timestamp - duration;
		timeline->interval[timeline->timelinesize][1] = timeline->timestamp;
		timeline->timelinesize++;
	}
	else{ // Modify latest one
		//printf("No, it's modifying last one\n");
		timeline->timestamp += duration;
		timeline->interval[timeline->timelinesize - 1][1] = timeline->timestamp;
	}
	
	// Process modification
	if(process != NULL){
		process->CPUburstleft -= duration;
		process->RRcycleUsed = true;
		if(process->CPUburstleft == 0){
			process->finishedTime = timeline->timestamp;
			if(process->IOburst != 0) printf("[Random I/O] Random I/O performing from process #%d\n", process->PID);
		}
	}
}

// --------------------------------------------------------------------------------------------------------------------
// Naive scheduling

// General scheduling method
Timeline ScheduleGeneral(Process *processes, int processNum, bool preemptive, 
		ProcessComparisonCriteria criteria, int contextswitchingcost, bool detailedDebug,
		const char *timelineTitle){

	// Prefix decoration
	printf("\n"); printRepeat("-", 80, false);
	printf("\nScheduling for timeline %s.\n\n", timelineTitle);
	
	// Scheduling
	Timeline timeline = newTimeline(processes, processNum, contextswitchingcost);
	selectionSort(processes, 0, processNum, criteria_FCFS);
	int start = 0, end = 0;
	while(start < processNum){
		
		// Move front pointer until all processes come
		while(end < processNum && processes[end].arrivalTime <= timeline.timestamp) end++;
		
		// Now we should check for [start, end)
		int next_come = (end < processNum ? processes[end].arrivalTime : inf);
		if(start == end){ // Nothing to do; Just wait until next process comes.
			if(next_come == inf){
				printf("[Error] Something wrong happened in ScheduleGeneral (%s), all processes done but loop is not ended.\n",
					ProcessComparisonNames[criteria]);
				exit(-1);
			}
			doJobFor(&timeline, NULL, next_come - timeline.timestamp);
			continue;
		}
		
		if(criteria == criteria_PDy){ // Dynamically changing priorities
			int randomChangingIndex = superrandom(start, end-1);
			int currentPriority = (processes + randomChangingIndex)->givenPriority;
			(processes+randomChangingIndex)->givenPriority = superrandom(currentPriority / 2, currentPriority * 2 + 1);
			if(detailedDebug) printf("Process #%d's priority changed from %d to %d\n", (processes+randomChangingIndex)->PID,
				currentPriority, (processes+randomChangingIndex)->givenPriority);
		}
		
		// Pick optimal processes
		processComparisonValues_timelinetimestamp = timeline.timestamp;
		int picked = pick(processes, start, end, criteria);
		if(criteria == criteria_RR && (processes+start)->RRcycleUsed == true){ // For round robin: If all processes are used, refresh the cycle.
			if(detailedDebug) printf("RoundRobin: All processes used cycle, refresh all cycles.\n");
			for(int i=start; i<end; i++) (processes+i)->RRcycleUsed = false;
			picked = pick(processes, start, end, criteria);
		}
		
		if(detailedDebug){
			printf("Timestamp %03d: Picked #%d from among\n", timeline.timestamp, (processes+start)->PID);
			//for(int i=start; i<end; i++){
			//	printf("  "); reprSingleProcess(processes + i, ProcessRepresentMinimal); printf("\n");
			//}
		}
		
		// Do job
		if(criteria == criteria_RR) // If round-robin, then use quantum time
			doJobFor(&timeline, processes+start, min2((processes+start)->CPUburstleft, globalRRQuantumTime));
		else if(preemptive) // Do until next process comes
			//printf("Preemptive: Doing %d seconds\n", max2(1, min2((processes+start)->CPUburstleft, next_come - timeline.timestamp))),
			doJobFor(&timeline, processes+start, 
				ProcessComparisonTicking[criteria] ? 1 : max2(1, min2((processes+start)->CPUburstleft, next_come - timeline.timestamp)));
		else // Do all and go next
			//printf("Non-preemptive: Doing %d seconds\n", (processes+start)->CPUburstleft),
			doJobFor(&timeline, processes+start, (processes+start)->CPUburstleft);
			
		// If current process bursted then move back pointer
		if((processes+start)->CPUburstleft == 0) start++;
	} return timeline;
}

// --------------------------------------------------------------------------------------------------------------------
// Gantt chart displaying

// Display Gantt chart
void GanttChart(Timeline *timeline, const char *timelineTitle){
	
	// Validation
	if(timeline->timelinesize == 0){
		printf("[Warning] Tried to print empty timeline\n");
		return;
	}
	
	// Prefix decoration
	printf("\n"); printRepeat("-", 80, false);
	printf("\nGantt chart for timeline %s.\n\n", timelineTitle);
	
	// Main 1: Processes info
	printf("Processes: \n");
	for(int i=0; i<timeline->processNum; i++){
		printf("  "); 
		reprSingleProcess(timeline->processes + i, ProcessRepresentStatistics); 
		printf("\n");
	} printf("\n");
	
	// Main 2: Vertical Gantt chart
	printf("Timeline: \n");
	for(int i=0; i<timeline->timelinesize; i++){
		printf("%4d +-----------+\n     | PID = ", timeline->interval[i][0]);
		if(timeline->usedProcessesPID[i] == -1) printf("---");
		else printf("%03d", timeline->usedProcessesPID[i]);
		printf(" |\n");
	} printf("%4d +-----------+\n\n", timeline->interval[timeline->timelinesize-1][1]);
	
	// Main 3: Calculate average turnaround time and waiting time.
	int total_turnaround = 0, total_waiting = 0;
	for(int i=0; i<timeline->processNum; i++){
		int finished = timeline->processes[i].finishedTime, arrived = timeline->processes[i].arrivalTime;
		total_turnaround += finished - arrived;
		total_waiting += finished - arrived - timeline->processes[i].CPUburst;
	} printf("Average turnaround %.2f, average waiting %.2f\n", 
		(double)total_turnaround / timeline->processNum, (double)total_waiting / timeline->processNum);
}

// --------------------------------------------------------------------------------------------------------------------
// Functionality testing

// Testing selection sort on processes
void SelectionSortFunctionalityTest(){
	
	// Process randomizing
	const int processNum = 10;
	Process *processes = (Process*)malloc(sizeof(Process) * processNum);
	for(int i=0; i<processNum; i++) *(processes+i) = createRandomProcess(20, 0, 0, 10, 1, 5);
	
	// Sort in 3 different criterias
	const char criteria_str[3][100] = {"FCFS", "SJF", "Priority"};
	printf("Before sorted - "); reprMultiProcesses(processes, processNum, ProcessRepresentMinimal);
	for(int criteria = 0; criteria < 3; criteria++){
		selectionSort(processes, 0, processNum, criteria);
		printf("\n\nAfter sorted by %s - ", criteria_str[criteria]);
		reprMultiProcesses(processes, processNum, ProcessRepresentMinimal);
	}
	free(processes);
}

// Evaluation
void schedulingTests(int processNum, int burstScale, int arrivalScale, int contextSwitchingCost, bool detailedDebug){
	
	// Parameter evaluation
	if(burstScale <= 0 || processNum <= 0 || arrivalScale < 0){
		printf("[Error] Nonpositive process num(%d) or scalars(%d, %d) given - evaluation will be terminated.\n",
			processNum, burstScale, arrivalScale);
		exit(-1);
	}

	// Process randomizing
	Process *processes = (Process*)malloc(sizeof(Process) * processNum);
	for(int i=0; i<processNum; i++) *(processes+i) = createRandomProcess(burstScale, 2, 0, i * arrivalScale, 1, 5);
	printf("Initial processes:\n");
	reprMultiProcesses(processes, processNum, ProcessRepresentMinimal);
	printRepeat("-", 60, false); printf("\n");
	
	// FCFS
	Process *processesFCFS = deepCopyProcesses(processes, processNum);
	Timeline FCFSscheduled = ScheduleGeneral(processesFCFS, processNum, false, criteria_FCFS, contextSwitchingCost, detailedDebug, "FCFS");
	GanttChart(&FCFSscheduled, "FCFS");
	
	// SJF non preemptive
	Process *processesSJF = deepCopyProcesses(processes, processNum);
	Timeline SJFscheduled = ScheduleGeneral(processesSJF, processNum, false, criteria_SJF, contextSwitchingCost, detailedDebug, "SJF");
	GanttChart(&SJFscheduled, "SJF");
	
	// SJF preemptive
	Process *processesSJFP = deepCopyProcesses(processes, processNum);
	Timeline SJFPscheduled = ScheduleGeneral(processesSJFP, processNum, true, criteria_SJF, contextSwitchingCost, detailedDebug, "SJF-preemptive");
	GanttChart(&SJFPscheduled, "SJF-preemptive");
	
	// Priority non preemptive
	Process *processesP = deepCopyProcesses(processes, processNum);
	Timeline Pscheduled = ScheduleGeneral(processesP, processNum, false, criteria_P, contextSwitchingCost, detailedDebug, "Priority");
	GanttChart(&Pscheduled, "Priority");
	
	// Priority preemptive
	Process *processesPP = deepCopyProcesses(processes, processNum);
	Timeline PPscheduled = ScheduleGeneral(processesPP, processNum, true, criteria_P, contextSwitchingCost, detailedDebug, "Priority-preemptive");
	GanttChart(&PPscheduled, "Priority-preemptive");
	
	// Aging
	Process *processesAG = deepCopyProcesses(processes, processNum);
	Timeline AGscheduled = ScheduleGeneral(processesAG, processNum, true, criteria_AGING, contextSwitchingCost, detailedDebug, "CustomizedAging-preemptive");
	GanttChart(&AGscheduled, "CustomizedAging-preemptive");
	
	// RR
	Process *processesRR = deepCopyProcesses(processes, processNum);
	Timeline RRscheduled = ScheduleGeneral(processesRR, processNum, false, criteria_RR, contextSwitchingCost, detailedDebug, "RoundRobin");
	GanttChart(&RRscheduled, "RoundRobin");
	printf("Round Robin Quantum time = %d\n", globalRRQuantumTime);
	
	// Priority dynamic preemptive
	Process *processesPDP = deepCopyProcesses(processes, processNum);
	Timeline PDPscheduled = ScheduleGeneral(processesPDP, processNum, true, criteria_PDy, contextSwitchingCost, detailedDebug, "DynamicPriority-preemptive");
	GanttChart(&PDPscheduled, "DynamicPriority-preemptive");
}

// --------------------------------------------------------------------------------------------------------------------
// Main function

int main(void){
	
	//DequeFunctionalityTest1();
	//SelectionSortFunctionalityTest();
	
	printf("Welcome to the Minsung's CPU scheduling world!\n");
	printf("Please input the number of processes(positive number): ");
	int processNum; scanf("%d", &processNum); if(processNum <= 0) exit(-1);
	printf("Please input the burst scale(positive number): ");
	int burstScale; scanf("%d", &burstScale); if(burstScale <= 0) exit(-1);
	printf("Please input the arrival scale(non-negative number): ");
	int arrivalScale; scanf("%d", &arrivalScale); if(arrivalScale < 0) exit(-1);
	printf("Please input the context switching cost(non-negative number): ");
	int contextswitchingcost; scanf("%d", &contextswitchingcost); if(contextswitchingcost < 0) exit(-1);
	printf("Please input the RR quantum time(positive number): ");
	scanf("%d", &globalRRQuantumTime); if(globalRRQuantumTime <= 0) exit(-1);
	schedulingTests(processNum, burstScale, arrivalScale, contextswitchingcost, false);
	
	return 0;
}
