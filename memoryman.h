# include <stdio.h>
# include <sys/mman.h>
# include <stdbool.h>
# include <errno.h>

# define ALLOCATED_SIZE 4096
# define MAGIC_NUMBER 1234567
# define INITIAL_ADDR 0x7f41002b3000

typedef struct Node
{
    int size;
    struct Node *next;
} Node_t;

typedef struct 
{
    int size;
    int magic;
} Header_t;

Node_t *head;

void traverseFreeList() {
    Node_t *currentNode = head;

    printf("================== START TRAVERSAL ====================\n");

    while (currentNode != NULL) {
        printf("================== current node location = %p ====================\n", currentNode);
        printf("================== current node size = %d ====================\n\n", currentNode -> size);

        currentNode = currentNode -> next;
    }
}

void initializeFreeList() {
    void* initialAddr = (void *)INITIAL_ADDR;
    
    head = mmap(initialAddr, ALLOCATED_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    head -> size = ALLOCATED_SIZE - sizeof(Node_t);
    head -> next = NULL;
}

/*
    Splits the node into two nodes of same size until finding the smallest node able to fulfill a request for
    a memory chunk for size neededSize.
*/
Node_t* splitFreeList(Node_t *node, int neededSize) {
    Node_t *currentNode = node;
    // Splits the free list until finding the smallest block of size ALLOCATED_SIZE / 2^N able to satisfy the request
    while ((currentNode -> size - sizeof(Node_t)) / 2 > neededSize) {
        int splitableSize = (currentNode -> size) - sizeof(Node_t);

        Node_t *nextNode = currentNode -> next;

        void *newNodeAddr = currentNode;
        newNodeAddr += sizeof(Node_t) + splitableSize / 2;
        Node_t *newNode = (Node_t*) newNodeAddr;
        
        currentNode -> size = splitableSize / 2;
        currentNode -> next = newNode;

        newNode -> size = splitableSize / 2;
        newNode -> next = nextNode;
    };

    return currentNode;
}

void* allocatePtr(int size, Node_t *previousNode, Node_t *currentNode) {
    void *ptr;

    // We need the requested size + header size
    long neededSize = size + sizeof(Header_t);
    
    // Split the node in half to find the smallest node able to satisfy the request
    Node_t* targetNode = splitFreeList(currentNode, neededSize);

    int targetNodeSize = targetNode -> size;
    Node_t* nextNode = targetNode -> next;
    // Make the header come right after the chosen node
    void *headerAddr = (void *) targetNode;
    headerAddr += sizeof(Node_t);
    Header_t *header = (Header_t *) headerAddr;
    
    // Set the header attributes
    header -> size = targetNodeSize - sizeof(Header_t);
    header -> magic = MAGIC_NUMBER;

    // Return a reference to the memory location where the requested chunk starts
    ptr = headerAddr + sizeof(Header_t);

    // Set the chosen node size to 0 to mark it as allocated/unavailable
    targetNode -> size = 0;
    targetNode -> next = nextNode;

    if (previousNode == NULL) {
        // Update the head if needed
        head = targetNode;
    } else {
        // Make the previous node point to the current node
        previousNode -> next = targetNode;
    }

    return ptr;
}

void* customMalloc(int size) {
    if (head == NULL) {
        initializeFreeList();
    }

    Node_t *previousNode = NULL;
    Node_t *currentNode = head;

    // We need the requested size + header size
    long neededSize = size + sizeof(Header_t);

    while (currentNode != NULL) {
        if (currentNode -> size >= neededSize) {
            return allocatePtr(size, previousNode, currentNode);
        }

        previousNode = currentNode;
        currentNode = currentNode -> next;
    }

    return NULL;
}

/* 
    Merges node1 and node2. node1 must come before node2.
*/
Node_t* mergeNodes(Node_t *node1, Node_t *node2) {
    int node1Size = node1 -> size;
    int node2Size = node2 -> size;

    Node_t *nextNode = node2 -> next;

    node2 -> next = NULL;

    node1 -> size = node1Size + node2Size + sizeof(Node_t);
    node1 -> next = nextNode;

    return node1;
}

/*
    Merges buddy nodes from the free list.
*/
void mergeBuddyNodes(Node_t *node) {
    Node_t *currentNode = node;

    // We want to merge buddy nodes as much as possible 
    // (in the best case scenario, until getting the whole memory space back)
    while (currentNode -> size != ALLOCATED_SIZE - sizeof(Node_t)) {
        void *nodeAddr = currentNode;
        void *deltaAddr = nodeAddr - INITIAL_ADDR;
        long buddyNumber = (long) deltaAddr / (currentNode -> size + sizeof(Node_t));

        void *buddyNodeAddr = nodeAddr;    

        if (buddyNumber % 2 == 0) {
            // Buddy node is after the target node
            buddyNodeAddr += currentNode -> size;
            buddyNodeAddr += sizeof(Node_t);

            Node_t *buddyNode = buddyNodeAddr;
            
            // Check if the buddy is in the free list
            if (buddyNode -> size != currentNode -> size) {
                break;
            }

            currentNode = mergeNodes(currentNode, buddyNode);
        } else {
            // Buddy node is before the target node
            buddyNodeAddr -= currentNode -> size;
            buddyNodeAddr -= sizeof(Node_t);

            Node_t *buddyNode = buddyNodeAddr;

            // Check if the buddy is in the free list
            if (buddyNode -> size != currentNode -> size) {
                break;
            }

            currentNode = mergeNodes(buddyNode, currentNode);
        }
    }
}

void customFree(void *ptr) {
    Header_t *hptr = (Header_t*) ptr - 1;
    
    int size = hptr -> size;
    int magic = hptr -> magic;

    // Integrity check
    if (magic == MAGIC_NUMBER) {
        void *freedNodeAddr = (void *) hptr; 
        freedNodeAddr -= sizeof(Node_t);
        
        Node_t *freedNode = (Node_t *) freedNodeAddr;
        freedNode -> size = size + sizeof(Header_t);
        
        mergeBuddyNodes(freedNode);
    }
}