//
// CS252: MyMalloc Project
//
// The current implementation gets memory from the OS
// every time memory is requested and never frees memory.
//
// You will implement the allocator as indicated in the handout.
// 
// Also you will need to add the necessary locking mechanisms to
// support multi-threaded programs.
//

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <pthread.h>
#include "MyMalloc.h"

static pthread_mutex_t mutex;

const int ArenaSize = 2097152;
const int NumberOfFreeLists = 1;

// Header of an object. Used both when the object is allocated and freed
struct ObjectHeader {
    size_t _objectSize;         // Real size of the object.
    int _allocated;             // 1 = yes, 0 = no 2 = sentinel
    struct ObjectHeader * _next;       // Points to the next object in the freelist (if free).
    struct ObjectHeader * _prev;       // Points to the previous object.
};

struct ObjectFooter {
    size_t _objectSize;
    int _allocated;
};

  //STATE of the allocator

  // Size of the heap
  static size_t _heapSize;

  // initial memory pool
  static void * _memStart;

  // number of chunks request from OS
  static int _numChunks;

  // True if heap has been initialized
  static int _initialized;

  // Verbose mode
  static int _verbose;

  // # malloc calls
  static int _mallocCalls;

  // # free calls
  static int _freeCalls;

  // # realloc calls
  static int _reallocCalls;
  
  // # realloc calls
  static int _callocCalls;

  // Free list is a sentinel
  static struct ObjectHeader _freeListSentinel; // Sentinel is used to simplify list operations
  static struct ObjectHeader *_freeList;


  //FUNCTIONS

  //Initializes the heap
  void initialize();

  // Allocates an object 
  void * allocateObject( size_t size );

  // Frees an object
  void freeObject( void * ptr );

  // Returns the size of an object
  size_t objectSize( void * ptr );

  // At exit handler
  void atExitHandler();

  //Prints the heap size and other information about the allocator
  void print();
  void print_list();

  // Gets memory from the OS
  void * getMemoryFromOS( size_t size );

  void increaseMallocCalls() { _mallocCalls++; }

  void increaseReallocCalls() { _reallocCalls++; }

  void increaseCallocCalls() { _callocCalls++; }

  void increaseFreeCalls() { _freeCalls++; }

extern void
atExitHandlerInC()
{
  atExitHandler();
}

void initialize()
{
  // Environment var VERBOSE prints stats at end and turns on debugging
  // Default is on
  _verbose = 1;
  const char * envverbose = getenv( "MALLOCVERBOSE" );
  if ( envverbose && !strcmp( envverbose, "NO") ) {
    _verbose = 0;
  }

  pthread_mutex_init(&mutex, NULL);
  void * _mem = getMemoryFromOS( ArenaSize + (2*sizeof(struct ObjectHeader)) + (2*sizeof(struct ObjectFooter)) );

  // In verbose mode register also printing statistics at exit
  atexit( atExitHandlerInC );

  //establish fence posts
  struct ObjectFooter * fencepost1 = (struct ObjectFooter *)_mem;
  fencepost1->_allocated = 1;
  fencepost1->_objectSize = 123456789;
  char * temp = 
      (char *)_mem + (2*sizeof(struct ObjectFooter)) + sizeof(struct ObjectHeader) + ArenaSize;
  struct ObjectHeader * fencepost2 = (struct ObjectHeader *)temp;
  fencepost2->_allocated = 1;
  fencepost2->_objectSize = 123456789;
  fencepost2->_next = NULL;
  fencepost2->_prev = NULL;

  //initialize the list to point to the _mem
  temp = (char *) _mem + sizeof(struct ObjectFooter);
  struct ObjectHeader * currentHeader = (struct ObjectHeader *) temp;
  temp = (char *)_mem + sizeof(struct ObjectFooter) + sizeof(struct ObjectHeader) + ArenaSize;
  struct ObjectFooter * currentFooter = (struct ObjectFooter *) temp;
  _freeList = &_freeListSentinel;
  currentHeader->_objectSize = ArenaSize + sizeof(struct ObjectHeader) + sizeof(struct ObjectFooter); //2MB
  currentHeader->_allocated = 0;
  currentHeader->_next = _freeList;
  currentHeader->_prev = _freeList;
  currentFooter->_allocated = 0;
  currentFooter->_objectSize = currentHeader->_objectSize;
  _freeList->_prev = currentHeader;
  _freeList->_next = currentHeader; 
  _freeList->_allocated = 2; // sentinel. no coalescing.
  _freeList->_objectSize = 0;
  _memStart = (char*) currentHeader;
}

void * allocateObject( size_t size )	//size is size in bytes
{
  //Make sure that allocator is initialized
  if ( !_initialized ) {
    _initialized = 1;
    initialize();
  }

  // Add the ObjectHeader/Footer to the size and round the total size up to a multiple of
  // 8 bytes for alignment.
  size_t roundedSize = (size + sizeof(struct ObjectHeader) + sizeof(struct ObjectFooter) + 7) & ~7;


  // Naively get memory from the OS every time
  //void * _mem = getMemoryFromOS( roundedSize );

  // Store the size in the header
  //struct ObjectHeader * o = (struct ObjectHeader *) _mem;

  //o->_objectSize = roundedSize;

  //new header to allocate
  struct ObjectHeader * currentHeader = _freeList->_next;
  
  //change pointers
  //_freeList->_next = (struct ObjectHeader*) ((char*)currentHeader + currentHeader->_objectSize);
  
  //Check if object Header has enough space, Get space
  
  int gotMem = 1;
  if (currentHeader->_objectSize <= roundedSize) {	//No space in this chunk of memory
	
	
	while (1) {
	
		  	currentHeader = currentHeader->_next;
		  	
		  	if (currentHeader->_objectSize >= roundedSize) {
		  		gotMem = 1;
		  		break;
		  	}
		  	
		  	if (currentHeader->_next == _freeList) {
		  		
		  		  //Create new chunk of mem from OS
		  		  void * _mem = getMemoryFromOS( ArenaSize + (2*sizeof(struct ObjectHeader)) + (2*sizeof(struct ObjectFooter)) );


				  //establish fence posts
				  struct ObjectFooter * fencepost1 = (struct ObjectFooter *)_mem;
				  fencepost1->_allocated = 1;
				  fencepost1->_objectSize = 123456789;
				  char * temp = (char *)_mem + (2*sizeof(struct ObjectFooter)) + sizeof(struct ObjectHeader) + ArenaSize;
				  struct ObjectHeader * fencepost2 = (struct ObjectHeader *)temp;
				  fencepost2->_allocated = 1;
				  fencepost2->_objectSize = 123456789;
				  fencepost2->_next = NULL;
				  fencepost2->_prev = NULL;

				  //initialize the list to point to the _mem
				  temp = (char *) _mem + sizeof(struct ObjectFooter);
				  struct ObjectHeader * chunkHeader = (struct ObjectHeader *) temp;
				  temp = (char *)_mem + sizeof(struct ObjectFooter) + sizeof(struct ObjectHeader) + ArenaSize;
				  struct ObjectFooter * chunkFooter = (struct ObjectFooter *) temp;
				  //_freeList = &_freeListSentinel;
				  chunkHeader->_objectSize = ArenaSize + sizeof(struct ObjectHeader) + sizeof(struct ObjectFooter); //2MB
				  chunkHeader->_allocated = 0;
				  chunkHeader->_next = _freeList;
				  chunkHeader->_prev = _freeList->_prev;
				  chunkFooter->_allocated = 0;
				  chunkFooter->_objectSize = chunkHeader->_objectSize;
				  _freeList->_prev->_next = chunkHeader;
				  _freeList->_prev = chunkHeader; 
				  //_freeList->_allocated = 2; // sentinel. no coalescing.
				  //_freeList->_objectSize = 0;
				  //_memStart = (char*) currentHeader;
				  
				  currentHeader = chunkHeader;
				  break;
		  	}
		  
  	}
  }
  
  
  //new footer to allocate
  struct ObjectFooter * currentFooter = (struct ObjectFooter*) ( (char*) currentHeader + roundedSize - sizeof(struct ObjectFooter));
  
  //initialize new footer with values
  currentFooter->_allocated = 1;
  currentFooter->_objectSize = roundedSize;
  
  struct ObjectHeader * newHeader = (struct ObjectHeader*) (currentFooter + 1);
  
  //Assign a new header
  newHeader->_allocated = 0;
  //newHeader->_next = currentHeader->_next;
  //newHeader->_prev = currentHeader->_prev;
  newHeader->_objectSize = currentHeader->_objectSize - roundedSize;
    
    
    
  //change pointers
  
  if (newHeader->_objectSize > 56) {
  	newHeader->_next = currentHeader->_next;
    newHeader->_prev = currentHeader->_prev;
    
    currentHeader->_next->_prev = newHeader;
  	currentHeader->_prev->_next = newHeader;
  	
  	
  	//_freeList->_next = newHeader;
  }
   else {
    
    newHeader->_next = currentHeader->_next;
	newHeader->_prev = currentHeader->_prev;    
   
    currentHeader->_next->_prev = currentHeader->_prev;
    currentHeader->_prev->_next = currentHeader->_next;
  	_freeList->_next = currentHeader->_next;
  	
  }
    //currentHeader->_next->_prev = newHeader;
  	//currentHeader->_prev->_next = newHeader;
  	
  	//_freeList->_next = newHeader;
  	
 
  
  //Allocated chunk of memory doesnt point to anything
  //initialize new allocated header with values
  currentHeader->_allocated = 1;
  currentHeader->_objectSize = roundedSize;
  currentHeader->_next = NULL;
  currentHeader->_prev = NULL;
  

  pthread_mutex_unlock(&mutex);

  // Return a pointer to usable memory
  return (void *) (currentHeader + 1);

}

void freeObject( void * ptr )
{
  // Add your code here
  
  //jump to header pointer
  struct ObjectHeader * currentHeader = (struct ObjectHeader *) ( (char*) ptr - sizeof(struct ObjectHeader) );
  currentHeader->_allocated = 0;
  
  //Jump to footer
  struct ObjectFooter * currentFooter = (struct ObjectFooter *) ((char*) currentHeader + currentHeader->_objectSize - sizeof(struct ObjectFooter));
  //Change footer allocation
  currentFooter->_allocated = 0;
  
  struct ObjectFooter * prevFooter = (struct ObjectFooter *) ((char*) currentHeader - sizeof(struct ObjectFooter));
  
  struct ObjectHeader * nextHeader = (struct ObjectHeader *) ((char*) currentFooter + sizeof(struct ObjectFooter));
  
  
  if (prevFooter->_allocated == 0 && nextHeader->_allocated == 0) {	//Both prev and next objects are free
  	  	
  	  	struct ObjectHeader * prevHeader = (struct ObjectHeader*) ((char*) prevFooter - prevFooter->_objectSize + sizeof(struct ObjectFooter));
  	  	
  	  	struct ObjectFooter * nextFooter = (struct ObjectFooter*) ((char*) nextHeader + nextHeader->_objectSize - sizeof(struct ObjectFooter));
  	  	
  	  	prevHeader->_objectSize = prevHeader->_objectSize + currentHeader->_objectSize + nextHeader->_objectSize;
  	  	
  	  	nextFooter->_objectSize = prevHeader->_objectSize;
  	  	
  	  	
  	  	prevHeader->_next = nextHeader->_next;
  	  	
  	  	nextHeader->_next->_prev = prevHeader;
  	  	
  	  	  	
  	  	
  }
  
  else if(prevFooter->_allocated == 0) {	//Check if prev object is free
  	
  	//Previous Object is free. Coalesce
  	struct ObjectHeader * prevHeader = (struct ObjectHeader*) ((char*) prevFooter - prevFooter->_objectSize + sizeof(struct ObjectFooter));
  	
  	prevHeader->_objectSize = prevHeader->_objectSize + currentHeader->_objectSize;
  	
  	currentFooter->_objectSize = prevHeader->_objectSize;
  	
  	
  }
  
  else if (nextHeader->_allocated == 0) {	//Check if next object is free
  	
  	
  	currentHeader->_objectSize = currentHeader->_objectSize + nextHeader->_objectSize;
  	
  	//Change Pointers
  	currentHeader->_next = nextHeader->_next;
  	currentHeader->_prev = nextHeader->_prev;
  	nextHeader->_prev->_next = currentHeader;
  	nextHeader->_next->_prev = currentHeader;
  	
  	struct ObjectFooter * nextFooter = (struct ObjectFooter*) ((char*) nextHeader + nextHeader->_objectSize - sizeof(struct ObjectFooter));
  	
  	nextFooter->_objectSize = currentHeader->_objectSize;
  	
  
  }
  
  else {
  
  	  //Get offset
  	  long offset = (long) currentHeader - (long)_memStart; //Position of currentHeader in memory
  	  
  	  struct ObjectHeader * n = _freeList->_next;
  	  while (n != _freeList) {
  	  	
  	  	long tempOffset = (long) n - (long) _memStart;  //position of n in memory
  	  	
  	  	if (offset < tempOffset) {
  	  		
  	  		currentHeader->_next = n;
  	  		currentHeader->_prev = n->_prev;
  	  		
  	  		n->_prev->_next = currentHeader;
  	  		n->_prev = currentHeader;
  	  		
  	  		return;
  	  		
  	  	}
  	  	
  	  	n = n->_next;
  	  	
  	  }
	  
	  //Place object at the end of the free List
	  currentHeader->_next = _freeList;
	  currentHeader->_prev = _freeList->_prev;
	  
	  _freeList->_prev->_next = currentHeader;
	  _freeList->_prev = currentHeader;
  }
    
  return;

}

size_t objectSize( void * ptr )
{
  // Return the size of the object pointed by ptr. We assume that ptr is a valid obejct.
  struct ObjectHeader * o =
    (struct ObjectHeader *) ( (char *) ptr - sizeof(struct ObjectHeader) );

  // Substract the size of the header
  return o->_objectSize;
}

void print()
{
  printf("\n-------------------\n");

  printf("HeapSize:\t%zd bytes\n", _heapSize );
  printf("# mallocs:\t%d\n", _mallocCalls );
  printf("# reallocs:\t%d\n", _reallocCalls );
  printf("# callocs:\t%d\n", _callocCalls );
  printf("# frees:\t%d\n", _freeCalls );

  printf("\n-------------------\n");
}

void print_list()
{
  printf("FreeList: ");
  if ( !_initialized ) {
    _initialized = 1;
    initialize();
  }
  struct ObjectHeader * ptr = _freeList->_next;
  while(ptr != _freeList){
      long offset = (long)ptr - (long)_memStart;
      printf("[offset:%ld,size:%zd]",offset,ptr->_objectSize);
      ptr = ptr->_next;
      if(ptr != NULL){
          printf("->");
      }
  }
  printf("\n");
}

void * getMemoryFromOS( size_t size )
{
  // Use sbrk() to get memory from OS
  _heapSize += size;
 
  void * _mem = sbrk( size );

  if(!_initialized){
      _memStart = _mem;
  }

  _numChunks++;

  return _mem;
}

void atExitHandler()
{
  // Print statistics when exit
  if ( _verbose ) {
    print();
  }
}

//
// C interface
//

extern void *
malloc(size_t size)
{
  pthread_mutex_lock(&mutex);
  increaseMallocCalls();
  
  return allocateObject( size );
}

extern void
free(void *ptr)
{
  pthread_mutex_lock(&mutex);
  increaseFreeCalls();
  
  if ( ptr == 0 ) {
    // No object to free
    pthread_mutex_unlock(&mutex);
    return;
  }
  
  freeObject( ptr );
}

extern void *
realloc(void *ptr, size_t size)
{
  pthread_mutex_lock(&mutex);
  increaseReallocCalls();
    
  // Allocate new object
  void * newptr = allocateObject( size );

  // Copy old object only if ptr != 0
  if ( ptr != 0 ) {
    
    // copy only the minimum number of bytes
    size_t sizeToCopy =  objectSize( ptr );
    if ( sizeToCopy > size ) {
      sizeToCopy = size;
    }
    
    memcpy( newptr, ptr, sizeToCopy );

    //Free old object
    freeObject( ptr );
  }

  return newptr;
}

extern void *
calloc(size_t nelem, size_t elsize)
{
  pthread_mutex_lock(&mutex);
  increaseCallocCalls();
    
  // calloc allocates and initializes
  size_t size = nelem * elsize;

  void * ptr = allocateObject( size );

  if ( ptr ) {
    // No error
    // Initialize chunk with 0s
    memset( ptr, 0, size );
  }

  return ptr;
}

