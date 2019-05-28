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
