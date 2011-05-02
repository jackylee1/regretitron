#include "stdafx.h"
#include "memorymgr.h"
#include "../PokerLibrary/cardmachine.h" //for cardsi_max
#include "../PokerLibrary/treenolimit.h"
#include "../utility.h"
#include <iostream> //for allocate function
#include <iomanip>
#include <fstream> //for save function
#include <algorithm> //for max_element
#include <boost/static_assert.hpp>
#include <cstring> //for memcpy
#include <pthread.h>
#include <boost/math/common_factor_rt.hpp>
#include <cassert>
using namespace std;

BOOST_STATIC_ASSERT(MAX_ACTIONS_SOLVER == 3);

//should be at least as many bytes as can possibly be allocated in n_threads iterations
const int64 BYTES_WHEN_NEED_TO_COMPACT_HUGE_BUFFER = 5*1024*1024; 

//if every array used an extra 100 bytes, we'd have about 0.5% overhead
//tighter numbers will use memory more efficiently, but will also trigger more compactings, and more memory jostling.
//when compacting at the pooledarray level...
const int BYTES_ALLOWED_SLACK_IN_POOLEDARRAY = 100; //...if a PooledArray has at least this much slack at the end of it...
const int BYTES_TO_COMPACT_TO_IN_POOLEDARRAY = 50; //...adjust so that it has only this much slack at the end of it.
//when overflowinga pooledarray
const int BYTES_TO_GROW_POOLEDARRAY_WHEN_FULL = 1500; //it would get compacted later on anyway
const uint16 ALIGN_TO = 8; // ensure the start of arrays are on 8-byte boundaries (setting to 16 doesn't work due to 8-byte headers). 
//For the same reason only divisors of 8 work too, so your choices are 1, 2, 4, 8.

//forward declarations
template <int FLAGBITS> class IndexDataBits;
typedef IndexDataBits<2> IndexData; //define number of bits used in flags right here
template <class T> class TypedPooledArray;
typedef TypedPooledArray<IndexData> IndexArray;
class PooledArray;

//must be macro to be used in switch case statements
#define flagswitch(flags, numa) (2*(flags)+((numa)-2))
#if !defined(DYNAMIC_ALLOC_STRATN) || !defined(DYNAMIC_ALLOC_REGRET) || !defined(DYNAMIC_ALLOC_COUNTS)
#error "You need to define these things"
#endif
#if DYNAMIC_ALLOC_COUNTS
#if !DYNAMIC_ALLOC_STRATN && !DYNAMIC_ALLOC_REGRET
#error "Counts require dynamic allocation"
#endif
#define DO_IF_COUNTS(x) x
#else
#define DO_IF_COUNTS(x)
#endif

//global variables
HugeBuffer* hugebuffer = NULL;
MemoryManager* memory = NULL;

//********
// pages look like this:
//   A Free Page:
//      Header: 8 bytes, NULL pointer of type PooledArray*
//   Page Size: first 8 bytes of page is of type int64 and contains the size of this page.
//              If this page is at the END of the buffer, the "size" is the number of usable bytes between the end of this page's header and the end of the buffer.
//              If this page is in the MIDDLE of the buffer, the "size" is the number of bytes between the end of the header, and the BEGINNING of the NEXT header.
//
//   A Used Page:
//      Header: 8 bytes, pointer to the PooledArray that is using this page
//   Page Size: used by the PooledArray.. The PooledArray can be queried for the size of this page, and it will return the number of usable bytes IT can use.
//
//   The ptr in a PagePtr is void* and always points to what I call "Page" in the above, NOT the "Header".
//

typedef uint8 byte;

class PagePtr // awesome class that handles all pointer arithmatic and casting that is involved in a memory page
{
public:
	inline PagePtr() : ptr(NULL) { }
	inline PagePtr(void* pointer) : ptr(pointer) { }
	inline PagePtr(const PagePtr& pageptr) : ptr(pageptr.ptr) { }
	inline PagePtr& operator=(const PagePtr &rhs) { ptr = rhs.ptr; return *this; }
	inline PagePtr& operator=(void* rhs) { ptr = rhs; return *this; }
	inline bool operator==(const PagePtr &other) const { return ptr == other.ptr; }
	inline bool operator!=(const PagePtr &other) const { return ptr != other.ptr; }
	inline int64 operator-(const PagePtr &other) const { return (byte*)ptr - (byte*)other.ptr; }
	inline int64 operator-(const void* &rhs) const { return (byte*)ptr - (byte*)rhs; }
	inline void* GetAddressAt( int64 offset ) { assert(!IsFree() && offset>=0 && offset%2==0); return (byte*)ptr + offset; }
	inline void SetHeaderAddress(void* addr) { ptr = (void*)((PooledArray**)addr + 1); }
	inline void SetPageAddress(void* addr) { ptr = addr; }
	inline bool IsFree() { return (HeaderRef() == NULL); }
	inline PooledArray* & HeaderRef() { assert(ptr!=NULL); return *((PooledArray**)ptr - 1); } //the header is 8 bytes before beginning of a page
	inline int64 & SizeRef() { if(!IsFree()) REPORT("page not free"); return *((int64*)ptr); } //the page stores its own size, if it's free.
	void JumpToNextPage(); // uses SizeRef for size if free, uses HeaderRef->GetBytes() if not free.
	void JumpToNextPage( int64 nbytes ); // for when HeaderRef->GetBytes() should not be used
	void DeepCopyTo( PagePtr & other ); //copies all user page data
private:
	// only allow masters to use these things
	inline PagePtr& operator+=(const int64 &nbytes) { ptr = (void*)((byte*)ptr + nbytes); return *this; }
	inline const PagePtr operator+(const int64 &other) const { return PagePtr(*this) += other; }
	void* ptr;
};
BOOST_STATIC_ASSERT(sizeof(PagePtr) == 8);


class HugeBuffer // 1, and only 1.
{
public:
	HugeBuffer( int64 nbytes );
	~HugeBuffer() { operator delete(baseptr); pthread_mutex_destroy(&nextfreepagelock); }
	PagePtr AllocLocation(int nbytes, PooledArray * backptr, bool iscompacted);
	void FreeLocation(PagePtr location, int64 nbytes/*same as output of GetBytes()*/, PooledArray * checkptr);
	int64 CompactData(); //should only be called by the appropriate thread in Solver when it has ALL data locked! (returns data used)
	int64 GetSize() const { return mysize; } //returns all space allocated

private:
	PagePtr GetFirstPage() { PagePtr firstpg; firstpg.SetHeaderAddress(baseptr); return firstpg; }

	void * const baseptr; //address returned by new or malloc, all other void* should be contained in PagePtr
	const uint64 mysize;
	PagePtr nextfreepage; //the data in here is shared by various cardsi
	volatile bool arecompacted;
	//arecompacted will always be syncronously set to true (either at initialization or by CompactData(). The only thing that is done
	//to it asyncronously is to set it to false. as long as i always read it in either the new or the old state (false or true)
	//i don't care. do I need a lock to make sure I don't read garbage? as long as i at least get true or false, i'm fine
	pthread_mutex_t nextfreepagelock; //lock for above private variable

	HugeBuffer(const HugeBuffer& rhs);
	HugeBuffer& operator=(const HugeBuffer& rhs);
};

//can be used different ways. 
//can store only one data type, then blocksize is size of that data type.
//can store different datatypes, then blocksize divides all sizes of the types that are stored here. 
//   only problem with that ^^ is that blocksize can get small and indices can overflow my 12-14 bits.
//PooledArray are not used directly. I define typed pooled arrays later that are easier to use.
//same could be done if multiple types were stored here.
//at this level (the base class level), it's just a raw array of blocks, with no type.
class PooledArray // 1 per cardsi per arraytype
{
public:
	inline int64 GetBytes() { return maxnblocks * blocksize; }

protected: //not meant to be used directly
	PooledArray(int blocksizebytes, int nblocks, uint16 flagsmask) 
		: flagmask(flagsmask), numofcounts(0), blocksize(blocksizebytes), nextfreeblock(0)
	{
		maxnblocks = NextLargestLegalNBlocks(nblocks);
		location = hugebuffer->AllocLocation(blocksizebytes*maxnblocks, this, flagsmask==0/*compacted if flagsmask indicates nodes can't be removed*/);
	}

	uint16 AddNewNodeRaw(int nblocks); //adds a node into the buffer, returns its index.
	void RemoveNodeRaw(uint16 index, int nblocks, IndexArray &indexarray);
	inline void* GetRaw(uint16 index) { assert(index < nextfreeblock); return location.GetAddressAt( index * blocksize ); }

private:
	friend int64 HugeBuffer::CompactData();
	void Compact(); //only called by HugeBuffer
	void CopyTo( PagePtr newlocation );
	uint16 NearestLegalNBlocks( uint16 desiredblocks );
	uint16 NextLargestLegalNBlocks( uint16 desiredblocks );

	PagePtr location;
	const uint16 flagmask; //defines what type of array this is so that RemoveNode can determine which elements of indexarray point to THIS array
	const uint8 numofcounts; //the first numofcounts * maxnblocks bytes of this array are chars
	const uint8 blocksize; //all sizes & lengths in units of this. this is in units of bytes. does not include counts.
	uint16 maxnblocks;
	uint16 nextfreeblock;

	PooledArray(const PooledArray& rhs);
	PooledArray& operator=(const PooledArray& rhs);
};
BOOST_STATIC_ASSERT(sizeof(PooledArray) == 16);
BOOST_STATIC_ASSERT(sizeof(PooledArray*) == 8);
BOOST_STATIC_ASSERT(sizeof(PooledArray**) == 8);

// ====================================================================== edit here
// AllocType -- change these flags to match the ways data can be allocated
//  Change the number of bits that IndexDataBits template uses at the top of the
//  file if appropriate

enum AllocType
{
	NO_ALLOC,
#if DYNAMIC_ALLOC_STRATN
	STRATN_ONLY,
#endif
#if DYNAMIC_ALLOC_REGRET
	REGRET_ONLY,
#endif
#if DYNAMIC_ALLOC_STRATN && DYNAMIC_ALLOC_REGRET
	FULL_ALLOC,
#endif
	ALLOCTYPE_MAX
};

// ----------------------------------------------------------------------

//data types that are usually stored in a PooledArray
template <int FLAGBITS> class IndexDataBits
{
	BOOST_STATIC_ASSERT( 1<<FLAGBITS >= ALLOCTYPE_MAX );
public:
	IndexDataBits() : loc(0xFFFF>>FLAGBITS), fl(NO_ALLOC) { } //constructor used for nodes at beginning of solving ==> nothing unallocated, location meaningless
	inline void GetData(uint16 & location, AllocType & flags) { location = loc; flags = (AllocType)fl; }
	uint16 GetLocation() { assert(loc < (1<<(16-FLAGBITS)) - 5); return loc; }
	AllocType GetFlags() { return (AllocType)fl; }
	void SetLocation(uint16 location) { loc = location; CheckLocation(); }
	void SetFlags(AllocType flags) { if(flags >= 1<<FLAGBITS) REPORT("bad flags: "+tostring(flags)); fl = flags; }
private:
	inline void CheckLocation() { if(loc >= (1<<(16-FLAGBITS)) - 5) REPORT("bad location: "+tostring(loc)); } //top 5 reserved
	uint16 loc : 16-FLAGBITS;
	uint16 fl : FLAGBITS;
};
BOOST_STATIC_ASSERT(sizeof(IndexData) == 2); //typedefed at top of file as forward delclaration

// ====================================================================== edit here
//  constructors in these define which data types can be promotedto which other data types (nice!)
//  a default constructor means that data type can be promoted from null
#if DYNAMIC_ALLOC_STRATN
template <int NUMA> class StratnData
{
public:
	StratnData() { for(int i=0; i<NUMA; i++) stratn[i] = 0; }
	RiverStratn_type stratn[NUMA];
private:
	StratnData(const StratnData&);
	StratnData& operator=(const StratnData&);
} __attribute__((packed));
#endif

#if DYNAMIC_ALLOC_REGRET
template <int NUMA> class RegretData
{
public:
	RegretData() { for(int i=0; i<NUMA; i++) regret[i] = 0; }
	RiverRegret_type regret[NUMA];
private:
	RegretData(const RegretData&);
	RegretData& operator=(const RegretData&);
} __attribute__((packed));
#endif

#if DYNAMIC_ALLOC_REGRET && DYNAMIC_ALLOC_STRATN
template <int NUMA> class FullData
{
	FullData() { for(int i=0; i<NUMA; i++) stratn[i] = regret[i] = 0; } //do not allow promoted from null
public:
	FullData( const StratnData<NUMA> & sd ) { for(int i=0; i<NUMA; i++) { stratn[i] = sd.stratn[i]; regret[i] = 0; } }
	FullData( const RegretData<NUMA> & rd ) { for(int i=0; i<NUMA; i++) { stratn[i] = 0; regret[i] = rd.regret[i]; } }
	RiverRegret_type regret[NUMA];
	RiverStratn_type stratn[NUMA];
private:
	FullData(const FullData&);
	FullData& operator=(const FullData&);
} __attribute__((packed));
#endif

//---------------------------------------------------------------------

//usable class that does type-safe access to array. Suitable for any array that stores only one type of data.
template < class T >
class TypedPooledArray : public PooledArray
{
public:
	//set blocksize to the size of our data type
	TypedPooledArray(int maxnblocks, uint16 flagmask) 
		: PooledArray(sizeof(T), maxnblocks, flagmask) { }

	//typed access of elements
	inline T & operator[](uint16 index) { return *(T*)GetRaw(index); }

	//Any arguments passed to AddNewNode are passed to the constructor of T
	uint16 AddNewNode();
	template < typename P1 > uint16 AddNewNode( const P1 & p1 );

	//our data type uses 1 block
	inline void RemoveNode(uint16 index, IndexArray & indexarray) { RemoveNodeRaw(index, 1, indexarray); }
};

//=========================================================================== edit here
// Edit this container as it has all the arrays that are used for every cardsi node

//holds all the arrays a cardsi has
struct CardsiContainer 
{
	CardsiContainer();
	IndexArray indexarr;
	BOOST_STATIC_ASSERT(MAX_ACTIONS_SOLVER == 3);
#if DYNAMIC_ALLOC_STRATN
	TypedPooledArray< StratnData<2> > stratnarr2;
	TypedPooledArray< StratnData<3> > stratnarr3;
#endif
#if DYNAMIC_ALLOC_REGRET
	TypedPooledArray< RegretData<2> > regretarr2;
	TypedPooledArray< RegretData<3> > regretarr3;
#endif
#if DYNAMIC_ALLOC_STRATN && DYNAMIC_ALLOC_REGRET
	TypedPooledArray< FullData<2> > fullarr2;
	TypedPooledArray< FullData<3> > fullarr3;
#endif
#if DYNAMIC_ALLOC_COUNTS
	TypedPooledArray<int> readregretcount;
	TypedPooledArray<int> writeregretcount;
	TypedPooledArray<int> writestratncount;
#endif
};

//keep up to date so printed info to screen is good
#if DYNAMIC_ALLOC_STRATN && DYNAMIC_ALLOC_REGRET
const int64 EXTRA_SLACK_PER_CC = 6 * BYTES_ALLOWED_SLACK_IN_POOLEDARRAY;
#elif DYNAMIC_ALLOC_STRATN || DYNAMIC_ALLOC_REGRET
const int64 EXTRA_SLACK_PER_CC = 2 * BYTES_ALLOWED_SLACK_IN_POOLEDARRAY;
#else
const int64 EXTRA_SLACK_PER_CC = 0;
#endif

#if DYNAMIC_ALLOC_COUNTS
const int64 EXTRA_COUNT_BYTES_PER_NODE = 3 * sizeof(int); //beyond stratn and regret data
#endif

#if DYNAMIC_ALLOC_STRATN || DYNAMIC_ALLOC_REGRET
const int64 EXTRA_INDEX_BYTES_PER_NODE = sizeof( IndexData ); //beyond stratn and regret data
#endif

//----------------------------------------------------------------------------

void PagePtr::JumpToNextPage() 
{ 
	if(IsFree())
		JumpToNextPage(SizeRef());
	else
		JumpToNextPage(HeaderRef()->GetBytes());
}

void PagePtr::JumpToNextPage( int64 nbytes )
{
	*this += nbytes + sizeof(PooledArray*); //defines SizeRef() and GetBytes() to give the same thing: amount of user-availble page data
}

void PagePtr::DeepCopyTo( PagePtr & other )
{
	if(IsFree())
		REPORT("cannot copy from a free page!");
	if(*this == other)
		REPORT("cannot copy same page!");
	memmove(other.ptr, ptr, HeaderRef()->GetBytes()); //uses GetBytes() as amount of user-available page data, memmove handles copy-overlap case
}

HugeBuffer::HugeBuffer( int64 nbytes ) : baseptr( operator new(nbytes) ), mysize(nbytes), arecompacted(true)
{
	if(baseptr != memset(baseptr, 0xCC, mysize)) REPORT("excuse me memset?");
	pthread_mutex_init(&nextfreepagelock, NULL);
	nextfreepage = GetFirstPage(); //valid now that baseptr is set
	nextfreepage.HeaderRef() = NULL; //set header to NULL
	nextfreepage.SizeRef() = mysize - sizeof(PooledArray*); //number of bytes between end of this header and the end of the buffer
}

//iscompacted means this newly allocated page has more data allocated to it than it really needs, 
//so if it's true we set the whole buffer could use compaction
PagePtr HugeBuffer::AllocLocation(int nbytes, PooledArray * backptr, bool iscompacted)
{
	if(nbytes < 8 || backptr == NULL) 
		REPORT("invalid params to AllocLocation");
	if(nbytes%ALIGN_TO != 0)
		REPORT("Not allocating on ALIGN_TO boundaries...");

	pthread_mutex_lock(&nextfreepagelock);
	// check for size issues
	//nextfreepage is always the last page and always stores the number ofbytes between the end of its header and the end of the buffer as its size
	if(nextfreepage.SizeRef()/*bytes before end of buffer*/ < nbytes/*body of this page*/ + 8/*header of next page*/ + 8/*size tag of next page*/)
		REPORT("there is no memory left, sir!!!", KNOWN);
	if(nextfreepage.SizeRef() < BYTES_WHEN_NEED_TO_COMPACT_HUGE_BUFFER && !arecompacted) 
		memory->SetMasterCompactFlag(); //signals that solver should compact us as soon as possible by calling CompactData

	//set the compacted flag, and the backptr for this new page
	if(iscompacted == false) 
		arecompacted = false; 
	int64 oldfreesize = nextfreepage.SizeRef(); //SizeRef won't work after we set the backptr
	nextfreepage.HeaderRef() = backptr; //store backptr as a header for this page
	PagePtr retval = nextfreepage;

	//set up the next new free page
	nextfreepage.JumpToNextPage( nbytes ); //specify nbytes so no use of HeaderRef is used!
	nextfreepage.HeaderRef() = NULL;
	nextfreepage.SizeRef() = oldfreesize - nbytes - sizeof(PooledArray*); // new number of bytes between the end of this header and the end of the buffer
	pthread_mutex_unlock(&nextfreepagelock);

	return retval;
}

void HugeBuffer::FreeLocation(PagePtr location, int64 nbytes/*same as output of GetBytes()*/, PooledArray * checkptr)
{
	if(location.HeaderRef() != checkptr)
		REPORT("the location you passed in has an invalid pointer");

	location.HeaderRef() = NULL;
	location.SizeRef() = nbytes;// the number of bytes between the end of this header and the beginning of the NEXT header
	arecompacted = false;
}

int64 HugeBuffer::CompactData()
{
	PagePtr currentnewpage = GetFirstPage(); //location where we are writing data TO
	PagePtr currentoldpage = GetFirstPage(); //location where we are reading data FROM

	//nextfreepage is used here but we don't use nextfreepagelock because this code is run only when we have all data locks in Solver aquired
	while(currentoldpage != nextfreepage)
	{
		if(currentoldpage.IsFree()) //this page is free, and there must be more pages after it, as it is not equal to nextfreepage
		{
			//keep new page the same, advance onto next old page
			currentoldpage.JumpToNextPage();
		}
		else //this page is not free, it has a valid PooledArray attached to it
		{
			PagePtr sourcepage = currentoldpage; //source because we are going to move it up and it is the source

			//update currentoldpage first, as the next page is at the end of the CURRENT size of this page
			currentoldpage.JumpToNextPage();

			//now shrink the sourcepage, THEN copy it. 
			sourcepage.HeaderRef()->Compact();

			if(sourcepage != currentnewpage) //then move sourcepage back to currentnewpage
				sourcepage.HeaderRef()->CopyTo( currentnewpage );

			currentnewpage.JumpToNextPage();
		}
	}

	//finally set the free page at the end of the buffer
	//cout << "HugeBuffer compacted the data from " << space(nextfreepage - GetFirstPage()) << " to " << space(currentnewpage - GetFirstPage()) << " saving " << space(nextfreepage-currentnewpage) << " of " << space(mysize) << "!" << endl;
	nextfreepage = currentnewpage;
	nextfreepage.HeaderRef() = NULL;
	nextfreepage.SizeRef() = mysize - (nextfreepage - baseptr); //number of bytes between end of the header and the end of the buffer
	arecompacted = true; // we sure are!
	return nextfreepage - GetFirstPage(); //return current space used
}

void PooledArray::Compact( ) //only called by CompactData above
{
	//we are friends to HugeBuffer for this reason
	if( ( maxnblocks - nextfreeblock ) * blocksize >= BYTES_ALLOWED_SLACK_IN_POOLEDARRAY ) 
	{
		const uint16 desirednblocks = nextfreeblock + (BYTES_TO_COMPACT_TO_IN_POOLEDARRAY + blocksize/2) / blocksize; //round to nearest blocksize
		maxnblocks = NearestLegalNBlocks( desirednblocks ); //might round down to something very small
		if(maxnblocks <= nextfreeblock + 2) // ... if it did, round up.
			maxnblocks = NextLargestLegalNBlocks( desirednblocks );
	}
}

void PooledArray::CopyTo( PagePtr newlocation )
{
	location.DeepCopyTo( newlocation ); //copies user page data, calls GetBytes on us
	newlocation.HeaderRef() = this;
	location = newlocation;
}

//ensure things are aligned somewhat decently
uint16 PooledArray::NearestLegalNBlocks( uint16 desiredblocks )
{
	const uint16 roundto = boost::math::lcm( (uint16)blocksize, ALIGN_TO ) / blocksize;
	return roundto*((desiredblocks+(roundto/2))/roundto);
}

uint16 PooledArray::NextLargestLegalNBlocks( uint16 desiredblocks )
{
	const uint16 roundto = boost::math::lcm( (uint16)blocksize, ALIGN_TO ) / blocksize;
	return roundto*((desiredblocks+roundto-1)/roundto);
}

uint16 PooledArray::AddNewNodeRaw(int nblocks) 
{ 
	if(nblocks <= 0) REPORT("bad size"); 
	uint16 r = nextfreeblock;
   	nextfreeblock += nblocks;
	if(nextfreeblock > maxnblocks)
	{
		uint16 newlength = nextfreeblock + (BYTES_TO_GROW_POOLEDARRAY_WHEN_FULL + blocksize/2) / blocksize;
		PagePtr newlocation = hugebuffer->AllocLocation(newlength*blocksize, this, false /* not compacted */);
		PagePtr oldlocation = location;
		CopyTo( newlocation ); //calls GetBytes on us
		hugebuffer->FreeLocation(oldlocation, maxnblocks*blocksize, this);
		maxnblocks = newlength;
	}
   	return r;
}

void PooledArray::RemoveNodeRaw(uint16 index, int nblocks, IndexArray & indexarray)
{
	if(nblocks <= 0) 
		REPORT("bad size: "+tostring(index)+" "+tostring(nblocks)); 
	if(flagmask == 0)
		REPORT("flagmask is zero, should not be removing from this array");
	for(int i=0; i<indexarray.nextfreeblock; i++)
	{
		//any elements of indexarray that have flags for THIS array and for with index larger than this one are shifted back
		if( ( ( 1 << flagswitch(indexarray[i].GetFlags(), memory->getnumafromriveractioni(i)) ) & flagmask ) && indexarray[i].GetLocation() > index )
		{
			if(indexarray[i].GetLocation() < index + nblocks)
				REPORT("the nblocks="+tostring(nblocks)+" clashes with another element at "+tostring(indexarray[i].GetLocation())+" vs "+tostring(index));
			indexarray[i].SetLocation( indexarray[i].GetLocation() - nblocks );
		}
	}
	memmove( location.GetAddressAt(index*blocksize), location.GetAddressAt((index+nblocks)*blocksize), (nextfreeblock-(index+nblocks))*blocksize );
	nextfreeblock -= nblocks;
}

template < class T > uint16 TypedPooledArray<T>::AddNewNode()  //argumentns passed to constructor for T
{ 
	const uint16 newindex = AddNewNodeRaw( 1 ); //our data type uses 1 block
	void* rawaddr = GetRaw( newindex );
	new( rawaddr ) T(); //call the constructor on that address
	return newindex;
}

template <class T> template <typename P1> uint16 TypedPooledArray<T>::AddNewNode( const P1 & p1 )  //argumentns passed to constructor for T
{ 
	const uint16 newindex = AddNewNodeRaw( 1 ); //our data type uses 1 block
	void* rawaddr = GetRaw( newindex );
	new( rawaddr ) T( p1 ); //call the constructor on that address
	return newindex;
}

//============================================================================== edit here
// constructor for CardsiContainer needs to be set for how we expect things to be at
// the beginning of simulating -- allocate any persistent testing counts
// allocate regrets if you don't want them done dynamically, and of course
// allocate all indexarray values. Also set the flags for any given array.

CardsiContainer::CardsiContainer()
	: indexarr( memory->riveractionimax() , 0)
#if DYNAMIC_ALLOC_STRATN
	, stratnarr2 ( memory->getactmax(3,2) / 3 , 1 << flagswitch( STRATN_ONLY, 2 ) ) 
	, stratnarr3 ( memory->getactmax(3,3) / 3 , 1 << flagswitch( STRATN_ONLY, 3 ) ) 
#endif
#if DYNAMIC_ALLOC_REGRET
	, regretarr2 ( memory->getactmax(3,2) / 3 , 1 << flagswitch( REGRET_ONLY, 2 ) ) 
	, regretarr3 ( memory->getactmax(3,3) / 3 , 1 << flagswitch( REGRET_ONLY, 3 ) )
#endif
#if DYNAMIC_ALLOC_STRATN && DYNAMIC_ALLOC_REGRET
	, fullarr2   ( memory->getactmax(3,2) / 3 , 1 << flagswitch( FULL_ALLOC,  2 ) )
	, fullarr3   ( memory->getactmax(3,3) / 3 , 1 << flagswitch( FULL_ALLOC,  3 ) )
#endif
#if DYNAMIC_ALLOC_COUNTS
	, readregretcount( memory->riveractionimax() , 0) 
	, writeregretcount( memory->riveractionimax() , 0) 
	, writestratncount( memory->riveractionimax() , 0)
#endif
{ 
	for(int i=0; i<memory->getactmax(3,2)+memory->getactmax(3,3); i++)
	{
	   indexarr.AddNewNode(); // adds new nodes with the default constructor
#if DYNAMIC_ALLOC_COUNTS
	   readregretcount.AddNewNode(0); //inits all counts to 0
	   writeregretcount.AddNewNode(0);
	   writestratncount.AddNewNode(0);
#endif
	}
}

//----------------------------------------------------------------------------------

int MemoryManager::riveractionimax()
{
	return getactmax(3,2) + getactmax(3,3);
}

int MemoryManager::riveractioni(int numa, int actioni)
{
	BOOST_STATIC_ASSERT(MAX_ACTIONS_SOLVER == 3); // ractioni assumes 3
	const static int actmax32 = getactmax(3,2); //for speed
	return (numa-2)*actmax32 + actioni;
}

int MemoryManager::getnumafromriveractioni(int ractioni) //opposite of riveractioni function
{
	const static int actmax32 = getactmax(3,2); //for speed
	return (ractioni >= actmax32) + 2;
}

int MemoryManager::getactionifromriveractioni(int ractioni) //opposite of riveractioni function
{
	return ractioni - (ractioni >= getactmax(3,2))*getactmax(3,2);
}

//internal, used by the functions Solver calls

//note: two versions of computestratt

//this is a version of the function when we don't have to cast the regret values to 
//Working_type. In this case, I don't read them in first and just use them.
void computestratt(Working_type * stratt, Working_type const * const regret, int numa)
{
	//find total regret

	Working_type totalregret=0;

	for(int i=0; i<numa; i++)
		if (regret[i]>0) totalregret += regret[i];

	//set strategy proportional to positive regret, or 1/numa if no positive regret

	if (totalregret > 0)
		for(int i=0; i<numa; i++)
			stratt[i] = (regret[i]>0) ? regret[i] / totalregret : 0;
	else
		for(int i=0; i<numa; i++)
			stratt[i] = (Working_type)1/(Working_type)numa;
}

//this is a version of the function that gets chosen by the compiler if T is
// not Working_type. T may be my FloatCustom's, and I only want to read them once.
template <typename T> 
void computestratt(Working_type * stratt, T const * const regret_slow, int numa)
{
	Working_type regret[ MAX_ACTIONS_SOLVER ];
	for( int i = 0; i < numa; i++ )
		regret[ i ] = static_cast< Working_type >( regret_slow[ i ] ); //cast is slow

	computestratt( stratt, regret, numa ); //should call function above
}

template <typename T>
void computeprobs(unsigned char * buffer, unsigned char &checksum, T const * const stratn, int numa)
{
	checksum = 0;

	T max_value = *max_element(stratn, stratn+numa);

	if(*min_element(stratn, stratn+numa) < 0)
		REPORT("stratn has negative values...");

	if(max_value==0) //algorithm never reached this node.
		for(int a=0; a<numa; a++)
			checksum += buffer[a]=1; //assigns equal probability weighting to each
	else
	{
		for(int a=0; a<numa; a++)
		{
			//we have values in the range [0.0, max_value]
			//map them to [0.0, 256.0]
			//then cast to int. almost always all values will get rounded down,
			//But max_value will never round down. we change 256 to 255.

			int temp = int(256.0 * (Working_type)stratn[a] / max_value);
			if(temp == 256) temp = 255;
			if(temp < 0 || temp > 255)
				REPORT("failure to divide");
			checksum += buffer[a] = (unsigned char)temp;
		}
	}
}

template <typename T>
void computestratn(T * stratn, Working_type prob, Working_type * stratt, int numa)
{
	for(int a=0; a<numa; a++)
		stratn[a] += ( prob * stratt[a] );
}

template <int P, typename T>
void computeregret(T * regret, Working_type prob, Working_type avgutility, tuple<Working_type,Working_type> * utility, int numa )
{
	for(int a=0; a<numa; a++)
		regret[a] += ( prob * (utility[a].get<P>() - avgutility) );
}

//very nicely do all teh work of promoting data from one array to another
template <typename T2>
T2 & promotefromnull( TypedPooledArray<T2> & newarray, AllocType newflags, IndexData & myindex )
{
	const uint16 newindex = newarray.AddNewNode( );
	myindex.SetLocation( newindex );
	myindex.SetFlags( newflags );
	return newarray[newindex];
}

template <typename T1, typename T2>
T2 & promote( uint16 oldindex, TypedPooledArray<T1> & oldarray, TypedPooledArray<T2> & newarray, AllocType newflags, IndexArray & indices, IndexData & myindex )
{
	const uint16 newindex = newarray.AddNewNode( oldarray[oldindex] );
	oldarray.RemoveNode( oldindex, indices );
	myindex.SetLocation( newindex );
	myindex.SetFlags( newflags );
	return newarray[newindex];
}

//========================================================================= edit here
// define the promotion path of data as it gets used, 
// increment counts as needed

//functions that solver calls to do its work
void MemoryManager::readstratt( Working_type * stratt, int gr, int numa, int actioni, int cardsi )
{
	if (gr==3)
	{
#if DYNAMIC_ALLOC_REGRET
		const int fullindex = riveractioni(numa, actioni);
		uint16 allocindex;
		AllocType flags;
		riverdata[cardsi].indexarr[fullindex].GetData(allocindex, flags);
		switch(flagswitch(flags, numa))
		{
			case flagswitch(NO_ALLOC, 2):
			case flagswitch(NO_ALLOC, 3):
#if DYNAMIC_ALLOC_STRATN
			case flagswitch(STRATN_ONLY, 2):
			case flagswitch(STRATN_ONLY, 3):
#endif
				for(int i=0; i<numa; i++)
					stratt[i] = (Working_type)1/(Working_type)numa;
				break;

			case flagswitch(REGRET_ONLY, 2):
				computestratt(stratt, riverdata[cardsi].regretarr2[allocindex].regret, numa);
				DO_IF_COUNTS( riverdata[cardsi].readregretcount[fullindex]++ );
				break;

			case flagswitch(REGRET_ONLY, 3):
				computestratt(stratt, riverdata[cardsi].regretarr3[allocindex].regret, numa);
				DO_IF_COUNTS( riverdata[cardsi].readregretcount[fullindex]++ );
				break;

#if DYNAMIC_ALLOC_STRATN
			case flagswitch(FULL_ALLOC, 2):
				computestratt(stratt, riverdata[cardsi].fullarr2[allocindex].regret, numa);
				DO_IF_COUNTS( riverdata[cardsi].readregretcount[fullindex]++ );
				break;

			case flagswitch(FULL_ALLOC, 3):
				computestratt(stratt, riverdata[cardsi].fullarr3[allocindex].regret, numa);
				DO_IF_COUNTS( riverdata[cardsi].readregretcount[fullindex]++ );
				break;
#endif

			default: REPORT("catastrophic error: flags="+tostring(flags)+" numa="+tostring(numa));
		}
#elif DYNAMIC_ALLOC_STRATN /*stratn dynamic, regret is not*/
		RiverRegret_type * regret;
		riverdata_regret[numa]->getdata(combine(cardsi, actioni, getactmax(gr,numa)), regret, numa);
		computestratt(stratt, regret, numa);
#else /*stratn not dynamic, regret not dynamic*/
		RiverRegret_type * regret;
		riverdata_oldmethod[numa]->getregret(combine(cardsi, actioni, getactmax(gr,numa)), regret, numa);
		computestratt(stratt, regret, numa);
#endif
	}
	else if (gr == 2)
	{
		TurnRegret_type * regret;
		turndata[numa]->getregret(combine(cardsi, actioni, getactmax(gr,numa)), regret, numa);
		computestratt(stratt, regret, numa);
	}
	else if (gr == 1)
	{
		FlopRegret_type * regret;
		flopdata[numa]->getregret(combine(cardsi, actioni, getactmax(gr,numa)), regret, numa);
		computestratt(stratt, regret, numa);
	}
	else if (gr == 0)
	{
		PFlopRegret_type * regret;
		pflopdata[numa]->getregret(combine(cardsi, actioni, getactmax(gr,numa)), regret, numa);
		computestratt(stratt, regret, numa);
	}
}


void MemoryManager::readstratn( unsigned char * buffer, unsigned char & checksum, int gr, int numa, int actioni, int cardsi )
{
	if (gr == 3)
	{
#if DYNAMIC_ALLOC_STRATN
		const int fullindex = riveractioni(numa, actioni);
		uint16 allocindex;
		AllocType flags;
		riverdata[cardsi].indexarr[fullindex].GetData(allocindex, flags);
		switch(flagswitch(flags, numa))
		{
		case flagswitch(NO_ALLOC, 2):
		case flagswitch(NO_ALLOC, 3):
#if DYNAMIC_ALLOC_REGRET
		case flagswitch(REGRET_ONLY, 2):
		case flagswitch(REGRET_ONLY, 3):
#endif
			checksum = 0;
			for(int i=0; i<numa; i++)
				checksum += buffer[i] = 1;
			break;

		case flagswitch(STRATN_ONLY, 2):
			computeprobs( buffer, checksum, riverdata[cardsi].stratnarr2[allocindex].stratn, numa );
			break;
		case flagswitch(STRATN_ONLY, 3):
			computeprobs( buffer, checksum, riverdata[cardsi].stratnarr3[allocindex].stratn, numa );
			break;

#if DYNAMIC_ALLOC_REGRET
		case flagswitch(FULL_ALLOC, 2):
			computeprobs( buffer, checksum, riverdata[cardsi].fullarr2[allocindex].stratn, numa );
			break;
		case flagswitch(FULL_ALLOC, 3):
			computeprobs( buffer, checksum, riverdata[cardsi].fullarr3[allocindex].stratn, numa );
			break;
#endif
	
		default: REPORT("catastrophic error: flags="+tostring(flags)+" numa="+tostring(numa));
		}
#elif DYNAMIC_ALLOC_REGRET /*stratn not dynamic, but regret is*/
		RiverStratn_type * stratn;
		riverdata_stratn[numa]->getdata( combine(cardsi, actioni, getactmax(gr,numa)), stratn, numa);
		computeprobs( buffer, checksum, stratn, numa );
#else /*stratn not dynamic, regret not dynamic*/
		RiverStratn_type * stratn;
		riverdata_oldmethod[numa]->getstratn( combine(cardsi, actioni, getactmax(gr,numa)), stratn, numa);
		computeprobs( buffer, checksum, stratn, numa );
#endif
	}
	else if (gr == 2)
	{
		TurnStratn_type * stratn;
		turndata[numa]->getstratn( combine(cardsi, actioni, getactmax(gr,numa)), stratn, numa );
		computeprobs( buffer, checksum, stratn, numa );
	}
	else if (gr == 1)
	{
		FlopStratn_type * stratn;
		flopdata[numa]->getstratn( combine(cardsi, actioni, getactmax(gr,numa)), stratn, numa );
		computeprobs( buffer, checksum, stratn, numa );
	}
	else if (gr == 0)
	{
		PFlopStratn_type * stratn;
		pflopdata[numa]->getstratn( combine(cardsi, actioni, getactmax(gr,numa)), stratn, numa );
		computeprobs( buffer, checksum, stratn, numa );
	}
}

void MemoryManager::writestratn( int gr, int numa, int actioni, int cardsi, Working_type prob, Working_type * stratt )
{
	if (gr == 3)
	{
#if DYNAMIC_ALLOC_STRATN
		const int fullindex = riveractioni(numa, actioni);
		DO_IF_COUNTS( riverdata[cardsi].writestratncount[fullindex]++ );
		IndexData & myindexdata = riverdata[cardsi].indexarr[fullindex];
		uint16 allocindex;
		AllocType flags;
		myindexdata.GetData(allocindex, flags);
		switch(flagswitch(flags,numa))
		{
		case flagswitch(NO_ALLOC, 2):
		{
			StratnData<2> & newnode = promotefromnull( riverdata[cardsi].stratnarr2, STRATN_ONLY, myindexdata );
			computestratn(newnode.stratn, prob, stratt, numa);
			break;
		}

		case flagswitch(NO_ALLOC, 3):
		{
			StratnData<3> & newnode = promotefromnull( riverdata[cardsi].stratnarr3, STRATN_ONLY, myindexdata );
			computestratn(newnode.stratn, prob, stratt, numa);
			break;
		}

		case flagswitch(STRATN_ONLY, 2):
			computestratn(riverdata[cardsi].stratnarr2[allocindex].stratn, prob, stratt, numa);
			break;

		case flagswitch(STRATN_ONLY, 3):
			computestratn(riverdata[cardsi].stratnarr3[allocindex].stratn, prob, stratt, numa);
			break;

#if DYNAMIC_ALLOC_REGRET
		case flagswitch(REGRET_ONLY, 2):
		{
			FullData<2> & newnode = 
				promote( allocindex, riverdata[cardsi].regretarr2, riverdata[cardsi].fullarr2, FULL_ALLOC, riverdata[cardsi].indexarr, myindexdata );
			computestratn(newnode.stratn, prob, stratt, numa);
			break;
		}

		case flagswitch(REGRET_ONLY, 3):
		{
			FullData<3> & newnode = 
				promote( allocindex, riverdata[cardsi].regretarr3, riverdata[cardsi].fullarr3, FULL_ALLOC, riverdata[cardsi].indexarr, myindexdata );
			computestratn(newnode.stratn, prob, stratt, numa);
			break;
		}

		case flagswitch(FULL_ALLOC, 2):
			computestratn(riverdata[cardsi].fullarr2[allocindex].stratn, prob, stratt, numa);
			break;

		case flagswitch(FULL_ALLOC, 3):
			computestratn(riverdata[cardsi].fullarr3[allocindex].stratn, prob, stratt, numa);
			break;
#endif

		default: REPORT("catastrophic error: flags="+tostring(flags)+" numa="+tostring(numa));
		}
#elif DYNAMIC_ALLOC_REGRET /*regret is dynamic, stratn is not */
		RiverStratn_type * stratn;
		riverdata_stratn[numa]->getdata(combine(cardsi, actioni, getactmax(gr,numa)), stratn, numa);
		computestratn(stratn, prob, stratt, numa);
#else /*nothing is dynamic*/
		RiverStratn_type * stratn;
		riverdata_oldmethod[numa]->getstratn(combine(cardsi, actioni, getactmax(gr,numa)), stratn, numa);
		computestratn(stratn, prob, stratt, numa);
#endif
	}
	else if (gr == 2)
	{
		TurnStratn_type * stratn;
		turndata[numa]->getstratn(combine(cardsi, actioni, getactmax(gr,numa)), stratn, numa);
		computestratn(stratn, prob, stratt, numa);
	}
	else if (gr == 1)
	{
		FlopStratn_type * stratn;
		flopdata[numa]->getstratn(combine(cardsi, actioni, getactmax(gr,numa)), stratn, numa);
		computestratn(stratn, prob, stratt, numa);
	}
	else if (gr == 0)
	{
		PFlopStratn_type * stratn;
		pflopdata[numa]->getstratn(combine(cardsi, actioni, getactmax(gr,numa)), stratn, numa);
		computestratn(stratn, prob, stratt, numa);
	}
}

template < int P >
void MemoryManager::writeregret( int gr, int numa, int actioni, int cardsi, 
		Working_type prob, Working_type avgutility, tuple<Working_type,Working_type> * utility )
{
	if (gr == 3)
	{
#if DYNAMIC_ALLOC_REGRET
		const int fullindex = riveractioni(numa, actioni);
		DO_IF_COUNTS( riverdata[cardsi].writeregretcount[fullindex]++ );
		IndexData & myindexdata = riverdata[cardsi].indexarr[fullindex];
		uint16 allocindex;
		AllocType flags;
		myindexdata.GetData(allocindex, flags);
		switch(flagswitch(flags, numa))
		{
		case flagswitch(NO_ALLOC, 2):
		{
			RegretData<2> & newnode = promotefromnull( riverdata[cardsi].regretarr2, REGRET_ONLY, myindexdata );
			computeregret<P> ( newnode.regret, prob, avgutility, utility, numa );
			break;
		}

		case flagswitch(NO_ALLOC, 3):
		{
			RegretData<3> & newnode = promotefromnull( riverdata[cardsi].regretarr3, REGRET_ONLY, myindexdata );
			computeregret<P> ( newnode.regret, prob, avgutility, utility, numa );
			break;
		}

#if DYNAMIC_ALLOC_STRATN
		case flagswitch(STRATN_ONLY, 2):
		{
			FullData<2> & newnode = 
				promote( allocindex, riverdata[cardsi].stratnarr2, riverdata[cardsi].fullarr2, FULL_ALLOC, riverdata[cardsi].indexarr, myindexdata );
			computeregret<P> ( newnode.regret, prob, avgutility, utility, numa );
		}
		break;

		case flagswitch(STRATN_ONLY, 3):
		{
			FullData<3> & newnode = 
				promote( allocindex, riverdata[cardsi].stratnarr3, riverdata[cardsi].fullarr3, FULL_ALLOC, riverdata[cardsi].indexarr, myindexdata );
			computeregret<P> ( newnode.regret, prob, avgutility, utility, numa );
		}
		break;
#endif

		case flagswitch(REGRET_ONLY, 2):
			computeregret<P> ( riverdata[cardsi].regretarr2[allocindex].regret, prob, avgutility, utility, numa );
			break;

		case flagswitch(REGRET_ONLY, 3):
			computeregret<P> ( riverdata[cardsi].regretarr3[allocindex].regret, prob, avgutility, utility, numa );
			break;

#if DYNAMIC_ALLOC_STRATN
		case flagswitch(FULL_ALLOC, 2):
			computeregret<P> ( riverdata[cardsi].fullarr2[allocindex].regret, prob, avgutility, utility, numa );
			break;

		case flagswitch(FULL_ALLOC, 3):
			computeregret<P> ( riverdata[cardsi].fullarr3[allocindex].regret, prob, avgutility, utility, numa );
			break;
#endif

		default: REPORT("catastrophic error: flags="+tostring(flags)+" numa="+tostring(numa));
		}
#elif DYNAMIC_ALLOC_STRATN /*regret is not dynamic, but stratn is*/
		RiverRegret_type * regret;
		riverdata_regret[numa]->getdata(combine(cardsi, actioni, getactmax(gr,numa)), regret, numa);
		computeregret<P>(regret, prob, avgutility, utility, numa);
#else /*nothing is dynamic*/
		RiverRegret_type * regret;
		riverdata_oldmethod[numa]->getregret(combine(cardsi, actioni, getactmax(gr,numa)), regret, numa);
		computeregret<P>(regret, prob, avgutility, utility, numa);
#endif
	}
	else if (gr == 2)
	{
		TurnRegret_type * regret;
		turndata[numa]->getregret(combine(cardsi, actioni, getactmax(gr,numa)), regret, numa);
		computeregret<P>(regret, prob, avgutility, utility, numa);
	}
	else if (gr == 1)
	{
		FlopRegret_type * regret;
		flopdata[numa]->getregret(combine(cardsi, actioni, getactmax(gr,numa)), regret, numa);
		computeregret<P>(regret, prob, avgutility, utility, numa);
	}
	else if (gr == 0)
	{
		PFlopRegret_type * regret;
		pflopdata[numa]->getregret(combine(cardsi, actioni, getactmax(gr,numa)), regret, numa);
		computeregret<P>(regret, prob, avgutility, utility, numa);
	}
}
template void MemoryManager::writeregret< 0 >( int gr, int numa, int actioni, int cardsi, 
		Working_type prob, Working_type avgutility, tuple<Working_type,Working_type> * utility );
template void MemoryManager::writeregret< 1 >( int gr, int numa, int actioni, int cardsi, 
		Working_type prob, Working_type avgutility, tuple<Working_type,Working_type> * utility );

// ---------------------------------------------------------------------------

//used for outputting info in MemoryManager ctor only
double getsize(int r)
{
	switch(r)
	{
		case 3: return sizeof( RiverStratn_type ) + sizeof( RiverRegret_type );
		case 2: return sizeof( TurnStratn_type ) + sizeof( TurnRegret_type );
		case 1: return sizeof( FlopStratn_type ) + sizeof( FlopRegret_type );
		case 0: return sizeof( PFlopStratn_type ) + sizeof( PFlopRegret_type );
		default: REPORT("bad r"); return 0;
	}
}
	
//cout how much memory will use, then allocate it
MemoryManager::MemoryManager(const BettingTree &bettingtree, const CardMachine &cardmachine)
	: cardmach( cardmachine )
	, tree( bettingtree )
	, pflopdata( boost::extents[boost::multi_array_types::extent_range(2,10)] )
	, flopdata( boost::extents[boost::multi_array_types::extent_range(2,10)] )
	, turndata( boost::extents[boost::multi_array_types::extent_range(2,10)] )
#if !DYNAMIC_ALLOC_STRATN && !DYNAMIC_ALLOC_REGRET /*both use old method*/
	, riverdata_oldmethod( boost::extents[boost::multi_array_types::extent_range(2,10)] )
#elif !DYNAMIC_ALLOC_STRATN /*only stratn uses old method*/
	, riverdata_stratn( boost::extents[boost::multi_array_types::extent_range(2,10)] )
#elif !DYNAMIC_ALLOC_REGRET /*only regret uses old method*/
	, riverdata_regret( boost::extents[boost::multi_array_types::extent_range(2,10)] )
#endif
#if DYNAMIC_ALLOC_STRATN || DYNAMIC_ALLOC_REGRET /*at least one uses the new method*/
	, riverdata( NULL )
	, mastercompactflag( false )
#endif
{
	for(int gr=0; gr<4; gr++) 
		for(int a=2; a<=MAX_ACTIONS; a++)
			if( (int64)getactmax( gr, a ) * (int64)cardmach.getcardsimax( gr ) > 0x000000007fffffffLL )
				REPORT("your indices may overflow. fix that.");

	//print out data types that are used and their sizes

	for(unsigned i=0; i<sizeof(TYPENAMES)/sizeof(TYPENAMES[0]); i++)
		cout << "floating point type " << TYPENAMES[i][0] << " is " << TYPENAMES[i][1] << " (" << space( TYPESIZES[i] ) << ")." << endl;

	//print a table of where what memory is used where

	cout << endl << "STATICALLY ALLOCATED:" << endl;

	const int c = 18;
	cout << endl
		<< setw(c) << "GAMEROUND" 
		<< setw(c) << "N ACTS"
		<< setw(c) << "N NODES" 
		<< setw(c) << "N BINS" 
		<< setw(c) << "SPACE"
		<< endl;

	//print out space requirements by game round for preflop flop and turn, track total space used

	int64 totalbytes = 0; //only statically allocated

#if DYNAMIC_ALLOC_STRATN || DYNAMIC_ALLOC_REGRET /*if anything is dynamic*/
	for(int gr=0; gr<3; gr++) 
#else /*if both are static*/
	for(int gr=0; gr<4; gr++) 
#endif
	{
		int64 totalactsthisround = 0;
		int64 totalbytesthisround = 0;

		for(int a=2; a<=MAX_ACTIONS; a++)
		{
			if( getactmax( gr, a ) == 0 )
				continue;

			int64 thisbytes = getactmax( gr, a ) * a * getsize( gr ) * cardmach.getcardsimax( gr );
			totalactsthisround += getactmax( gr, a );
			totalbytesthisround += thisbytes;

			cout 
				<< setw( c ) << gameroundstring( gr )
				<< setw( c ) << a
				<< setw( c ) << getactmax( gr, a )
				<< setw( c ) << cardmach.getcardsimax( gr )
				<< setw( c ) << space( thisbytes )
				<< endl;
		}
		cout
			<< setw( c ) << gameroundstring( gr )
			<< setw( c ) << "TOTAL"
			<< setw( c ) << totalactsthisround
			<< setw( c ) << cardmach.getcardsimax( gr )
			<< setw( c ) << space( totalbytesthisround ) << '*'
			<< endl;

		totalbytes += totalbytesthisround;
	}

#if ( DYNAMIC_ALLOC_STRATN && !DYNAMIC_ALLOC_REGRET ) || ( DYNAMIC_ALLOC_REGRET && !DYNAMIC_ALLOC_STRATN ) /*if exactly one thing is static*/
	{
		int64 totalactsthisround = 0;
		int64 totalbytesthisround = 0;

		for(int a=2; a<=MAX_ACTIONS; a++)
		{
			if( getactmax( 3, a ) == 0 )
				continue;

#if !DYNAMIC_ALLOC_STRATN
			int64 thisbytes = getactmax( 3, a ) * ( 1 * a ) * sizeof( RiverStratn_type ) * cardmach.getcardsimax( 3 );
#else
			int64 thisbytes = getactmax( 3, a ) * ( 1 * a ) * sizeof( RiverRegret_type ) * cardmach.getcardsimax( 3 );
#endif
			totalactsthisround += getactmax( 3, a );
			totalbytesthisround += thisbytes;

			cout 
#if !DYNAMIC_ALLOC_STRATN
				<< setw( c ) << "River Stratn"
#else
				<< setw( c ) << "River Regret"
#endif
				<< setw( c ) << a
				<< setw( c ) << getactmax( 3, a )
				<< setw( c ) << cardmach.getcardsimax( 3 )
				<< setw( c ) << space( thisbytes )
				<< endl;
		}
		cout
#if !DYNAMIC_ALLOC_STRATN
			<< setw( c ) << "River Stratn"
#else
			<< setw( c ) << "River Regret"
#endif
			<< setw( c ) << "TOTAL"
			<< setw( c ) << totalactsthisround
			<< setw( c ) << cardmach.getcardsimax( 3 )
			<< setw( c ) << space( totalbytesthisround ) << '*'
			<< endl;

		totalbytes += totalbytesthisround;
	}
#endif
	
	//print space used by bin files

	cout << endl
		<< setw( c ) << "GAMEROUND"
		<< setw( c ) << "BIN MAX"
		<< setw( c ) << "BIN FILE SPACE"
		<< endl;

	for(int gr=0; gr<4; gr++)
	{
		cout 
			<< setw( c ) << gameroundstring( gr )
			<< setw( c ) << cardmach.getparams().bin_max[ gr ]
			<< setw( c ) << space( cardmach.getparams().filesize[ gr ] ) 
			<< endl;
		totalbytes += cardmach.getparams().filesize[ gr ];
	}

	//print out total bytes used

	cout << endl << "TOTAL BYTES STATICALLY ACCOUNTED FOR: " << space(totalbytes) << endl;

	//print out space possibly used by dynamic allocator

#if DYNAMIC_ALLOC_STRATN || DYNAMIC_ALLOC_REGRET /*at least one uses the new method*/
	cout << endl << "DYNAMICALLY ALLOCATED (River only):" << endl;

	cout << endl
		<< setw( c ) << "THING"
		<< setw( c ) << "SIZE EACH"
		<< setw( c ) << "HOW MANY"
		<< setw( c ) << "TOTAL"
		<< endl;

	cout << setw( c ) << "CardsiContainers" << setw( c ) << space( sizeof( CardsiContainer ) ) << setw( c ) << cardmach.getcardsimax( 3 ) 
		<< setw( c ) << space( cardmach.getcardsimax( 3 ) * sizeof( CardsiContainer ) ) << endl;
	totalbytes += cardmach.getcardsimax( 3 ) * sizeof( CardsiContainer );

	cout << setw( c ) << "Extra Slack per CC" << setw( c ) << space( EXTRA_SLACK_PER_CC ) << setw( c ) << cardmach.getcardsimax( 3 ) 
		<< setw( c ) << space( cardmach.getcardsimax( 3 ) * EXTRA_SLACK_PER_CC ) << endl;
	totalbytes += cardmach.getcardsimax( 3 ) * EXTRA_SLACK_PER_CC;

	cout 
		<< setw( c ) << "IndexData" << setw( c ) << space( EXTRA_INDEX_BYTES_PER_NODE )
		<< setw( c ) << cardmach.getcardsimax( 3 ) * ( getactmax( 3, 2 ) + getactmax( 3, 3 ) )
		<< setw( c ) << space( cardmach.getcardsimax( 3 ) * ( getactmax( 3, 2 ) + getactmax( 3, 3 ) ) * EXTRA_INDEX_BYTES_PER_NODE ) 
		<< endl; 
	totalbytes += cardmach.getcardsimax( 3 ) * ( getactmax( 3, 2 ) + getactmax( 3, 3 ) ) * EXTRA_INDEX_BYTES_PER_NODE;
#endif

#if DYNAMIC_ALLOC_COUNTS
	cout 
		<< setw( c ) << "Diagnostic counts" << setw( c ) << space( EXTRA_COUNT_BYTES_PER_NODE )
		<< setw( c ) << cardmach.getcardsimax( 3 ) * ( getactmax( 3, 2 ) + getactmax( 3, 3 ) )
		<< setw( c ) << space( cardmach.getcardsimax( 3 ) * ( getactmax( 3, 2 ) + getactmax( 3, 3 ) ) * EXTRA_COUNT_BYTES_PER_NODE ) 
		<< endl; 
	totalbytes += cardmach.getcardsimax( 3 ) * ( getactmax( 3, 2 ) + getactmax( 3, 3 ) ) * EXTRA_COUNT_BYTES_PER_NODE;
#endif

#if DYNAMIC_ALLOC_STRATN
	cout 
		<< setw( c ) << "Stratn @ 100%" << setw( c ) << space( sizeof( RiverStratn_type ) )
		<< setw( c ) << (int64) cardmach.getcardsimax( 3 ) * ( getactmax( 3, 2 ) * 2 + getactmax( 3, 3 ) * 3 )
		<< setw( c ) << space( (int64) cardmach.getcardsimax( 3 ) * ( getactmax( 3, 2 ) * 2 + getactmax( 3, 3 ) * 3 ) * sizeof( RiverStratn_type ) )
		<< endl;
#endif

#if DYNAMIC_ALLOC_REGRET
	cout 
		<< setw( c ) << "Regret @ 100%" << setw( c ) << space( sizeof( RiverRegret_type ) )
		<< setw( c ) << (int64) cardmach.getcardsimax( 3 ) * ( getactmax( 3, 2 ) * 2 + getactmax( 3, 3 ) * 3 )
		<< setw( c ) << space( (int64) cardmach.getcardsimax( 3 ) * ( getactmax( 3, 2 ) * 2 + getactmax( 3, 3 ) * 3 ) * sizeof( RiverRegret_type ) )
		<< endl;
#endif

	//allocate for river

#if DYNAMIC_ALLOC_STRATN || DYNAMIC_ALLOC_REGRET /*at least one uses the new method*/
	cout << endl << "How much memory (in GB) would you like? ";
	double gigabytes;
	cin >> gigabytes;
	int64 nbytes = (int64)(gigabytes*1024.0*1024.0*1024.0) + 1;
	nbytes = (nbytes >> 23) << 23; //round down to nearest 8 MB
	cout << "Very well. I shall allocate " << nbytes << " bytes." << endl;

	hugebuffer = new HugeBuffer( nbytes ); //global, HugeBuffer must exist ant this set before CardiContainer's is created
	memory = this; //global, must be set before CardiContainer constructor is run. (next line)
	riverdata = new CardsiContainer[ cardmach.getcardsimax(3) ] ( );
#else
	// pause();
#endif
#if !DYNAMIC_ALLOC_STRATN && !DYNAMIC_ALLOC_REGRET /*both use old method*/
#if SEPARATE_STRATN_REGRET
	for(int n=2; n<10; n++) riverdata_oldmethod[n] = getactmax(3,n) > 0 ? new DataContainer< RiverStratn_type, RiverRegret_type >(n, getactmax(3,n)*cardmach.getcardsimax(3)) : NULL;
#else
	for(int n=2; n<10; n++) riverdata_oldmethod[n] = getactmax(3,n) > 0 ? new DataContainer< RiverStratn_type >(n, getactmax(3,n)*cardmach.getcardsimax(3)) : NULL;
#endif
#elif !DYNAMIC_ALLOC_STRATN /*only stratn uses old method*/
	for(int n=2; n<10; n++) riverdata_stratn[n] = getactmax(3,n) > 0 ? new DataContainerSingle<RiverStratn_type>(n, getactmax(3,n)*cardmach.getcardsimax(3)) : NULL;
#elif !DYNAMIC_ALLOC_REGRET /*only regret uses old method*/
	for(int n=2; n<10; n++) riverdata_regret[n] = getactmax(3,n) > 0 ? new DataContainerSingle<RiverRegret_type>(n, getactmax(3,n)*cardmach.getcardsimax(3)) : NULL;
#endif

	//allocate for preflop, flop, and turn.

#if SEPARATE_STRATN_REGRET
	for(int n=2; n<10; n++) pflopdata[n] = getactmax(0,n) > 0 ? new DataContainer< PFlopStratn_type, PFlopRegret_type >(n, getactmax(0,n)*cardmach.getcardsimax(0)) : NULL;
	for(int n=2; n<10; n++) flopdata[n] = getactmax(1,n) > 0 ? new DataContainer< FlopStratn_type, FlopRegret_type >(n, getactmax(1,n)*cardmach.getcardsimax(1)) : NULL;
	for(int n=2; n<10; n++) turndata[n] = getactmax(2,n) > 0 ? new DataContainer< TurnStratn_type, TurnRegret_type >(n, getactmax(2,n)*cardmach.getcardsimax(2)) : NULL;
#else
	for(int n=2; n<10; n++) pflopdata[n] = getactmax(0,n) > 0 ? new DataContainer<PFlopStratn_type>(n, getactmax(0,n)*cardmach.getcardsimax(0)) : NULL;
	for(int n=2; n<10; n++) flopdata[n] = getactmax(1,n) > 0 ? new DataContainer<FlopStratn_type>(n, getactmax(1,n)*cardmach.getcardsimax(1)) : NULL;
	for(int n=2; n<10; n++) turndata[n] = getactmax(2,n) > 0 ? new DataContainer<TurnStratn_type>(n, getactmax(2,n)*cardmach.getcardsimax(2)) : NULL;
#endif
}

MemoryManager::~MemoryManager()
{
	for(int n=2; n<10; n++) if(pflopdata[n] != NULL) delete pflopdata[n];
	for(int n=2; n<10; n++) if(flopdata[n] != NULL) delete flopdata[n];
	for(int n=2; n<10; n++) if(turndata[n] != NULL) delete turndata[n];
#if DYNAMIC_ALLOC_STRATN || DYNAMIC_ALLOC_REGRET /*at least one uses the new method*/
	delete[] riverdata;
	delete hugebuffer;
#endif
#if !DYNAMIC_ALLOC_STRATN && !DYNAMIC_ALLOC_REGRET /*both use old method*/
	for(int n=2; n<10; n++) if(riverdata_oldmethod[n] != NULL) delete riverdata_oldmethod[n];
#elif !DYNAMIC_ALLOC_STRATN /*only stratn uses old method*/
	for(int n=2; n<10; n++) if(riverdata_stratn[n] != NULL) delete riverdata_stratn[n];
#elif !DYNAMIC_ALLOC_REGRET /*only regret uses old method*/
	for(int n=2; n<10; n++) if(riverdata_regret[n] != NULL) delete riverdata_regret[n];
#endif

}

#if DYNAMIC_ALLOC_COUNTS
void MemoryManager::savecounts(const string & filename)
{
	cout << "Saving counts..." << endl;

	ofstream f((filename+"-counts.csv").c_str());

	for(int c=0; c<cardmach.getcardsimax(3); c++)
	{
		f << "#\n# cardsi = " << c << '\n';
		f << "#[numa], [actioni], [write stratn], [write regret], [read regret]\n";
		for(int i=0; i<riveractionimax(); i++)
		{
			f << getnumafromriveractioni(i) << ", ";
			f << getactionifromriveractioni(i) << ", ";
			f << riverdata[c].writestratncount[i] << ", ";
			f << riverdata[c].writeregretcount[i] << ", ";
			f << riverdata[c].readregretcount[i] << '\n';
		}
	}
}
#endif

int64 MemoryManager::save(const string &filename)
{
	DO_IF_COUNTS( savecounts(filename) );

	cout << "Saving strategy data file..." << endl;

	ofstream f(("strategy/" + filename + ".strat").c_str(), ofstream::binary);

	//this starts at zero, the beginning of the file
	int64 tableoffset=0;

	//this starts at the size of the table: one 8-byte offset integer per data-array
	int64 dataoffset= 4 * MAX_NODETYPES * 8;

	for(int r=0; r<4; r++)
	{
		for(int n=9; n>=2; n--)
		{
			// seek to beginning and write the offset
			f.seekp(tableoffset); 
			f.write((char*)&dataoffset, 8); 
			tableoffset += 8; 

			// seek to end and write the data
			f.seekp(dataoffset); 
			for(int i=0; i<getactmax(r,n)*cardmach.getcardsimax(r); i++) 
			{ 
				const int actioni = i % getactmax(r,n);
				const int cardsi = i / getactmax(r,n);
				unsigned char buffer[MAX_ACTIONS_SOLVER];
				unsigned char checksum = 0;
				readstratn( buffer, checksum, r, n, actioni, cardsi );
				f.write((char*)buffer, n); 
				f.write((char*)&checksum, 1); 
			} 
			dataoffset = f.tellp(); 
		}
	}

	f.close();

	return dataoffset;
}

//provide access to HugeBuffer functions via public MemoryManager functions, these are used by Solver
int64 MemoryManager::CompactMemory()
{
#if DYNAMIC_ALLOC_STRATN || DYNAMIC_ALLOC_REGRET
	return hugebuffer->CompactData();
#else
	return -1;
#endif
}

int64 MemoryManager::GetHugeBufferSize()
{
#if DYNAMIC_ALLOC_STRATN || DYNAMIC_ALLOC_REGRET
	return hugebuffer->GetSize();
#else
	return -1;
#endif
}
