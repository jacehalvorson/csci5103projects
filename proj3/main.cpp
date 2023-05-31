/*
Main program for the virtual memory project.
Make all of your modifications to this file.
You may add or rearrange any code or data as you need.
The header files page_table.h and disk.h explain
how to use the page table and disk interfaces.
*/

#include "page_table.h"
#include "disk.h"
#include "program.h"

#include <cassert>
#include <iostream>
#include <string.h>
#include <queue>
#include <vector>
#include <unordered_map>

using namespace std;

#define DEBUG 0
#define DEBUGC 0

// Type for which algorithm is used for page eviction
typedef enum page_replacement_algorithm
{
	RAND,
	FIFO,
	CUSTOM // Yet to be decided, we're not implementing LRU
} page_replacement_algorithm;

/* Custom comparison for priority queue */
struct minComp {
    constexpr bool operator()(
        pair<int,int> const& a,
        pair<int,int> const& b)
        const noexcept
    {
        return a.second > b.second;
    }
};

/* Array of <frame, page> mappings for physical memory */
int *pageList = nullptr; 

/* Queue of pages for FIFO algorithm */
queue<int> pageQueue;

/* Number of frames occupied */
int framesOccupied = 0;

/* Queue of <page, faults> for pages in physical memory (ordered by faults) */
typedef pair<int,int> pd;
priority_queue<pd, vector<pd>, minComp> activePages;

/* Hashmap of <page, faults> for pages not in physical memory */
unordered_map<int,int> idlePages;

// Prototype for test program
typedef void (*program_f)(char *data, int length);

// Number of physical frames
int nframes;
// Number of page faults
int numPageFaults = 0;
/* Number of disk reads */
int numDiskReads = 0;
/* Number of disk writes */
int numDiskWrites = 0;

// Algorithm used for page reaplacement
page_replacement_algorithm algorithm;

// Pointer to disk for access from handlers
struct disk *disk = nullptr;


/* Used for printing elements in the activePages priority queue */
void print_priority_queue( priority_queue<pd, vector<pd>, minComp> pq )
{
	while ( !pq.empty() )
	{
		pair<int,int> p = pq.top();
		pq.pop();
		cout << "page: " << p.first << ", faults: " << p.second << endl;
	}
}

/*
   Inputs - page table
				frame#

	Returns a pointer to the data associated with the frame
*/
char *get_data_pointer_from_frame( page_table *pt, int frame )
{
	char *physicalMemoryPtr = page_table_get_physmem( pt );
	assert( physicalMemoryPtr != nullptr );
	return ( physicalMemoryPtr + ( frame * PAGE_SIZE ) );
}

/*
   Inputs - page table
				page#

	Returns the frame number associated with the page
*/
int get_frame_from_page( page_table *pt, int page )
{
	int frame;
	int bits;

	page_table_get_entry( pt, page, &frame, &bits );

	return ( frame );
}

int evict_page ( page_table *pt, int newPage, page_replacement_algorithm alg )
{
	int evictedFrame; // Frame in physical memory that we're going to evict
	int evictedPage;  // Page associated with evictedFrame

	switch ( alg )
	{
		case RAND: {
			evictedFrame = rand( ) % page_table_get_nframes( pt );
			evictedPage = pageList[evictedFrame];
			// Update data structure
			pageList[evictedFrame] = newPage;
			break;
		}
		case FIFO: {
			assert(!pageQueue.empty());
			evictedPage = pageQueue.front();
			evictedFrame = pt->page_mapping[evictedPage];
			// Update data structure
			pageQueue.pop();
			pageQueue.push(newPage);
			break;
		}
		case CUSTOM: {
			assert(!activePages.empty());
		#if DEBUGC
			cout << "pageFaults Before:" << endl;
			print_priority_queue(activePages);
		#endif
			// Get least frequently faulted page
			pair<int,int> evictedP = activePages.top();
			activePages.pop();
		
			// Get eviction details
			evictedPage = evictedP.first;
			evictedFrame = pt->page_mapping[evictedPage];
		
		#if DEBUGC
			cout << endl << "Evicting page " << evictedPage << endl << endl;
		#endif

			// Add evicted page back to idle data structure
			idlePages[evictedPage] = evictedP.second;

			// Get and update new page details
			int newPageFaults = idlePages[newPage];
			newPageFaults++; // Update fault count for new page
			idlePages.erase(newPage);

			// Indicate that new page is now in physical memory
			activePages.push( {newPage, newPageFaults} );

		#if DEBUGC
			cout << "pageFaults After:" << endl;
			print_priority_queue(activePages);
		#endif

			break;
		}
		break;
	}

	// Offload the evicted page's contents on disk so it can
	// be replaced by another page's data
	disk_write( disk, evictedPage, get_data_pointer_from_frame( pt, evictedFrame ) );
	numDiskWrites++;

	// Update bits for evicted page
	page_table_set_entry( pt, evictedPage, evictedFrame, 0 );

	return evictedFrame;
}

void page_fault_handler( struct page_table *pt, int page )
{
#if DEBUG
	cout << "page fault on page #" << page << endl;

	// Print the page table contents
	cout << "Before ---------------------------" << endl;
	page_table_print(pt);
	cout << "----------------------------------" << endl;
#endif

	int oldFrame;
	int oldBits;
	int updatedFrame = -1;

	page_table_get_entry( pt, page, &oldFrame, &oldBits );
	switch ( oldBits )
	{
		case 0:
			// Page not resident in physical memory and it is being read/written.
			// Allocate a frame in physical memory for it.

			if ( framesOccupied < page_table_get_nframes( pt ) )
			{
				// If there are available frames, pick the first free one
				for ( int i = 0; i < page_table_get_nframes( pt ); i++ )
				{
					if ( pageList[i] == -1 )
					{
						updatedFrame = i;
						pageList[updatedFrame] = page;
						framesOccupied++;
						pageQueue.push(page);
						// priority queue
						activePages.push({page, 1});
						idlePages.erase(page);
						break;
					}
				}
				assert( updatedFrame != -1 );
			}
			else
			{
				// Otherwise use the eviction algorithm
				updatedFrame = evict_page( pt, page, algorithm );
			}

			// Updating bits for new page in physical memory
			page_table_set_entry( pt, page, updatedFrame, PROT_READ );

			// Update the physical memory for this new page with the data from disk
			disk_read( disk, page, get_data_pointer_from_frame( pt, updatedFrame ) );
			numDiskReads++;

			// Only increment page faults when moving from NOT_RESIDENT to READ_ONLY
			numPageFaults++;

		#if DEBUG
			cout << "numPageFaults = " << numPageFaults << endl;
		#endif

			break;
			
		case PROT_READ:
			// Read-only page is being written to. Keep the 
			// same physical frame and add PROT_WRITE bit to
			// its current protection bits.
			page_table_set_entry( pt, page, oldFrame, oldBits | PROT_WRITE );

			// Update "most recently used" variables or other metadata

			break;
		case ( PROT_READ | PROT_WRITE ):
			// Shouldn't have a page fault with these protection bits
			cerr << "Page fault on a read/write page" << endl;
			break;
		default:
			cerr << "Default case reached in page fault handler - consider handling PROT_EXEC" << endl;
			break;
	}

#if DEBUG
	// Print the page table contents
	cout << "After ----------------------------" << endl;
	page_table_print(pt);
	cout << "----------------------------------" << endl;
#endif
}

int main(int argc, char *argv[]) {
	// Check argument count
	if (argc != 5) {
		cerr << "Usage: virtmem <npages> <nframes> <rand|fifo|cust> <sort|scan|focus>" << endl;
		exit(1);
	}

	// Parse command line arguments
	int npages = atoi(argv[1]);
	nframes = atoi(argv[2]);
	const char *algorithmString = argv[3];
	const char *program_name = argv[4];

	// Validate the algorithm specified
	if ( !strcmp( algorithmString, "rand" ) )
	{
		algorithm = RAND;
		// Set seed to the number of clock cycles that have passed
		srand( clock( ) );
	}
	else if ( !strcmp( algorithmString, "fifo" ) )
	{
		algorithm = FIFO;
	}
	else if ( !strcmp( algorithmString, "cust" ) )
	{
		algorithm = CUSTOM;
	}
	else
	{
		cerr << "ERROR: Unknown algorithm: " << algorithmString << endl;
		exit(1);
	}

	// Validate the program specified
	program_f program = NULL;
	if (!strcmp(program_name, "sort")) {
		if (nframes < 2) {
			cerr << "ERROR: nFrames >= 2 for sort program" << endl;
			exit(1);
		}

		program = sort_program;
	}
	else if (!strcmp(program_name, "scan")) {
		program = scan_program;
	}
	else if (!strcmp(program_name, "focus")) {
		program = focus_program;
	}
	else {
		cerr << "ERROR: Unknown program: " << program_name << endl;
		exit(1);
	}

	// TODO - Any init needed

	/* Initializing array of <frame, page> mappings for physical memory */
	pageList = new int[nframes];
	for ( int i = 0; i < nframes; i++ )
	{
		pageList[i] = -1;
	}

	/* Initializing pages to have no faults */
	for ( int i = 0; i < npages; i++ )
	{
		idlePages[i] = 0;
	}

	// Create a virtual disk
	disk = disk_open("myvirtualdisk", npages);
	if (!disk) {
		cerr << "ERROR: Couldn't create virtual disk: " << strerror(errno) << endl;
		return 1;
	}

	// Create a page table
	struct page_table *pt = page_table_create(npages, nframes, page_fault_handler );
	if (!pt) {
		cerr << "ERROR: Couldn't create page table: " << strerror(errno) << endl;
		return 1;
	}

	// Run the specified program
	char *virtmem = page_table_get_virtmem(pt);
	program(virtmem, npages * PAGE_SIZE);


	/* Printing out final stats */
	cout << "Number of page faults: " << numPageFaults << endl;
	cout << "Number of disk reads: " << numDiskReads << endl;
	cout << "Number of disk writes: " << numDiskWrites << endl;

	// Clean up the page table and disk
	page_table_delete(pt);
	disk_close(disk);
	delete[] pageList;

	return 0;
}
