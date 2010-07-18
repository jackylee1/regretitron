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

//forward declarations
template <int FLAGBITS> class IndexDataBits;
typedef IndexDataBits<2> IndexData; //define number of bits used in flags right here
template <class T> class TypedPooledArray;
typedef TypedPooledArray<IndexData> IndexArray;
class PooledArray;

//must be macro to be used in switch case statements
#define flagswitch(flags, numa) (2*(flags)+((numa)-2))

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
	inline void* GetAddressAt( int64 offset ) { if(IsFree() || offset<0 || offset%2!=0) REPORT("getaddrat err: "+tostring(offset)); return (byte*)ptr + offset; }
	inline void SetHeaderAddress(void* addr) { ptr = (void*)((PooledArray**)addr + 1); }
	inline void SetPageAddress(void* addr) { ptr = addr; }
	inline bool IsFree() { return (HeaderRef() == NULL); }
	inline PooledArray* & HeaderRef() { if(ptr==NULL) REPORT("nullptr"); return *((PooledArray**)ptr - 1); } //the header is 8 bytes before the beginning of a page
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
	inline int64 GetBytes() { return maxlength * blocksize; }

protected: //not meant to be used directly
	PooledArray(int blocksizebytes, int nblocks, uint16 flagsmask) 
		: location(hugebuffer->AllocLocation(blocksizebytes*nblocks, this, flagsmask==0/*compacted if flagsmask indicates nodes can't be removed*/)), 
		  flagmask(flagsmask), numofcounts(0), blocksize(blocksizebytes), maxlength(nblocks), nextfree(0) { }

	uint16 AddNewNodeRaw(int nblocks); //adds a node into the buffer, returns its index.
	void RemoveNodeRaw(uint16 index, int nblocks, IndexArray &indexarray);
	void* GetRaw(uint16 index) { if(index>=nextfree) REPORT("bad index: "+tostring(index)); return location.GetAddressAt( index * blocksize ); }

private:
	friend int64 HugeBuffer::CompactData();
	void Compact(); //only called by HugeBuffer
	void CopyTo( PagePtr newlocation );

	PagePtr location;
	const uint16 flagmask; //defines what type of array this is so that RemoveNode can determine which elements of indexarray point to THIS array
	const uint8 numofcounts; //the first numofcounts * maxlength bytes of this array are chars
	const uint8 blocksize; //all sizes & lengths in units of this. this is in units of bytes. does not include counts.
	uint16 maxlength;
	uint16 nextfree;

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
	STRATN_ONLY,
	REGRET_ONLY,
	FULL_ALLOC,
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
	uint16 GetLocation() { CheckLocation(); return loc; }
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
template <int NUMA> class StratnData
{
public:
	StratnData() { for(int i=0; i<NUMA; i++) stratn[i] = 0; }
	float stratn[NUMA];
private:
	StratnData(const StratnData&);
	StratnData& operator=(const StratnData&);
};
BOOST_STATIC_ASSERT(sizeof(StratnData<2>) == 8);
BOOST_STATIC_ASSERT(sizeof(StratnData<3>) == 12);

template <int NUMA> class RegretData
{
public:
	RegretData() { for(int i=0; i<NUMA; i++) regret[i] = 0; }
	double regret[NUMA];
private:
	RegretData(const RegretData&);
	RegretData& operator=(const RegretData&);
};
BOOST_STATIC_ASSERT(sizeof(RegretData<2>) == 16);
BOOST_STATIC_ASSERT(sizeof(RegretData<3>) == 24);

template <int NUMA> class FullData
{
public:
	//FullData() { for(int i=0; i<NUMA; i++) stratn[i] = regret[i] = 0; } do not allow promoted from null
	FullData( const StratnData<NUMA> & sd ) { for(int i=0; i<NUMA; i++) { stratn[i] = sd.stratn[i]; regret[i] = 0; } }
	FullData( const RegretData<NUMA> & rd ) { for(int i=0; i<NUMA; i++) { stratn[i] = 0; regret[i] = rd.regret[i]; } }
	double regret[NUMA];
	float stratn[NUMA];
private:
	FullData(const FullData&);
	FullData& operator=(const FullData&);
} __attribute__((packed));
BOOST_STATIC_ASSERT(sizeof(FullData<2>) == 24);
BOOST_STATIC_ASSERT(sizeof(FullData<3>) == 36);

//---------------------------------------------------------------------

//usable class that does type-safe access to array. Suitable for any array that stores only one type of data.
template < class T >
class TypedPooledArray : public PooledArray
{
public:
	//set blocksize to the size of our data type
	TypedPooledArray(int maxlength, uint16 flagmask) 
		: PooledArray(sizeof(T), maxlength, flagmask) { }

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
	TypedPooledArray< StratnData<2> > stratnarr2;
	TypedPooledArray< RegretData<2> > regretarr2;
	TypedPooledArray< FullData<2> > fullarr2;
	TypedPooledArray< StratnData<3> > stratnarr3;
	TypedPooledArray< RegretData<3> > regretarr3;
	TypedPooledArray< FullData<3> > fullarr3;
	TypedPooledArray<int> readregretcount;
	TypedPooledArray<int> writeregretcount;
	TypedPooledArray<int> writestratncount;
};
//keep up to date so printed info to screen is good
const int EXTRA_CARDSI_BYTES_PER_NODE = 3*sizeof(int); //beyond stratn and regret data

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
	if(nbytes%4 != 0)
		REPORT("I would prefer if you allocated on 4 byte boundaries..", WARN);

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
	if( ( maxlength - nextfree ) * blocksize >= BYTES_ALLOWED_SLACK_IN_POOLEDARRAY) 
		maxlength = nextfree + (BYTES_TO_COMPACT_TO_IN_POOLEDARRAY + blocksize/2) / blocksize; //round to nearest blocksize
}

void PooledArray::CopyTo( PagePtr newlocation )
{
	location.DeepCopyTo( newlocation ); //copies user page data, calls GetBytes on us
	newlocation.HeaderRef() = this;
	location = newlocation;
}

uint16 PooledArray::AddNewNodeRaw(int nblocks) 
{ 
	if(nblocks <= 0) REPORT("bad size"); 
	uint16 r = nextfree;
   	nextfree += nblocks;
	if(nextfree > maxlength)
	{
		uint16 newlength = nextfree + (BYTES_TO_GROW_POOLEDARRAY_WHEN_FULL + blocksize/2) / blocksize;
		PagePtr newlocation = hugebuffer->AllocLocation(newlength*blocksize, this, false /* not compacted */);
		PagePtr oldlocation = location;
		CopyTo( newlocation ); //calls GetBytes on us
		hugebuffer->FreeLocation(oldlocation, maxlength*blocksize, this);
		maxlength = newlength;
	}
   	return r;
}

void PooledArray::RemoveNodeRaw(uint16 index, int nblocks, IndexArray & indexarray)
{
	if(nblocks <= 0) 
		REPORT("bad size: "+tostring(index)+" "+tostring(nblocks)); 
	if(flagmask == 0)
		REPORT("flagmask is zero, should not be removing from this array");
	for(int i=0; i<indexarray.nextfree; i++)
	{
		//any elements of indexarray that have flags for THIS array and for with index larger than this one are shifted back
		if( ( ( 1 << flagswitch(indexarray[i].GetFlags(), memory->getnumafromriveractioni(i)) ) & flagmask ) && indexarray[i].GetLocation() > index )
		{
			if(indexarray[i].GetLocation() < index + nblocks)
				REPORT("the nblocks="+tostring(nblocks)+" clashes with another element at "+tostring(indexarray[i].GetLocation())+" vs "+tostring(index));
			indexarray[i].SetLocation( indexarray[i].GetLocation() - nblocks );
		}
	}
	memmove( location.GetAddressAt(index*blocksize), location.GetAddressAt((index+nblocks)*blocksize), (nextfree-(index+nblocks))*blocksize );
	nextfree -= nblocks;
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
	: indexarr( memory->riveractionimax() , 0), 
	  stratnarr2 ( memory->getactmax(3,2) / 3 , 1 << flagswitch( STRATN_ONLY, 2 ) ), 
	  regretarr2 ( memory->getactmax(3,2) / 3 , 1 << flagswitch( REGRET_ONLY, 2 ) ), 
	  fullarr2   ( memory->getactmax(3,2) / 3 , 1 << flagswitch( FULL_ALLOC,  2 ) ),
	  stratnarr3 ( memory->getactmax(3,3) / 3 , 1 << flagswitch( STRATN_ONLY, 3 ) ), 
	  regretarr3 ( memory->getactmax(3,3) / 3 , 1 << flagswitch( REGRET_ONLY, 3 ) ),  
	  fullarr3   ( memory->getactmax(3,3) / 3 , 1 << flagswitch( FULL_ALLOC,  3 ) ),
	  readregretcount( memory->riveractionimax() , 0), 
	  writeregretcount( memory->riveractionimax() , 0), 
	  writestratncount( memory->riveractionimax() , 0)
{ 
	for(int i=0; i<memory->getactmax(3,2)+memory->getactmax(3,3); i++)
	{
	   indexarr.AddNewNode(); // adds new nodes with the default constructor
	   readregretcount.AddNewNode(0); //inits all counts to 0
	   writeregretcount.AddNewNode(0);
	   writestratncount.AddNewNode(0);
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
template <typename FStore> 
void computestratt(FWorking_type * stratt, FStore * regret, int numa)
{
	//find total regret

	FWorking_type totalregret=0;

	for(int i=0; i<numa; i++)
		if (regret[i]>0) totalregret += regret[i];

	//set strategy proportional to positive regret, or 1/numa if no positive regret

	if (totalregret > 0)
		for(int i=0; i<numa; i++)
			stratt[i] = (regret[i]>0) ? regret[i] / totalregret : 0;
	else
		for(int i=0; i<numa; i++)
			stratt[i] = (FWorking_type)1/(FWorking_type)numa;
}

template <typename FStore>
void computeprobs(unsigned char * buffer, unsigned char &checksum, FStore const * const stratn, int numa)
{
	checksum = 0;

	FStore max_value = *max_element(stratn, stratn+numa);

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

			int temp = int(256.0 * stratn[a] / max_value);
			if(temp == 256) temp = 255;
			if(temp < 0 || temp > 255)
				REPORT("failure to divide");
			checksum += buffer[a] = (unsigned char)temp;
		}
	}
}

template <typename FStore>
void computestratn(FStore * stratn, FWorking_type prob, FWorking_type * stratt, int numa)
{
	for(int a=0; a<numa; a++)
		stratn[a] += prob * stratt[a];
}

template <int P, typename FStore>
void computeregret(FStore * regret, FWorking_type prob, FWorking_type avgutility, tuple<FWorking_type,FWorking_type> * utility, int numa )
{
	for(int a=0; a<numa; a++)
		regret[a] += prob * (utility[a].get<P>() - avgutility);
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
void MemoryManager::readstratt( FWorking_type * stratt, int gr, int numa, int actioni, int cardsi )
{
	if(gr == 3)
	{
		const int fullindex = riveractioni(numa, actioni);
		uint16 allocindex;
		AllocType flags;
		riverdata[cardsi].indexarr[fullindex].GetData(allocindex, flags);
		switch(flagswitch(flags, numa))
		{
		case flagswitch(NO_ALLOC, 2):
		case flagswitch(NO_ALLOC, 3):
		case flagswitch(STRATN_ONLY, 2):
		case flagswitch(STRATN_ONLY, 3):
			for(int i=0; i<numa; i++)
				stratt[i] = (FWorking_type)1/(FWorking_type)numa;
			break;

		case flagswitch(REGRET_ONLY, 2):
			computestratt(stratt, riverdata[cardsi].regretarr2[allocindex].regret, numa);
			riverdata[cardsi].readregretcount[fullindex]++;
			break;

		case flagswitch(REGRET_ONLY, 3):
			computestratt(stratt, riverdata[cardsi].regretarr3[allocindex].regret, numa);
			riverdata[cardsi].readregretcount[fullindex]++;
			break;

		case flagswitch(FULL_ALLOC, 2):
			computestratt(stratt, riverdata[cardsi].fullarr2[allocindex].regret, numa);
			riverdata[cardsi].readregretcount[fullindex]++;
			break;

		case flagswitch(FULL_ALLOC, 3):
			computestratt(stratt, riverdata[cardsi].fullarr3[allocindex].regret, numa);
			riverdata[cardsi].readregretcount[fullindex]++;
			break;

		default: REPORT("catastrophic error: flags="+tostring(flags)+" numa="+tostring(numa));
		}
	}
	else
	{
		FStore_type * regret;
		data[gr][numa]->getregret(combine(cardsi, actioni, getactmax(gr,numa)), regret, numa);
		computestratt(stratt, regret, numa);
	}
}


void MemoryManager::readstratn( unsigned char * buffer, unsigned char & checksum, int gr, int numa, int actioni, int cardsi )
{
	if(gr == 3)
	{
		const int fullindex = riveractioni(numa, actioni);
		uint16 allocindex;
		AllocType flags;
		riverdata[cardsi].indexarr[fullindex].GetData(allocindex, flags);
		switch(flagswitch(flags, numa))
		{
		case flagswitch(NO_ALLOC, 2):
		case flagswitch(NO_ALLOC, 3):
		case flagswitch(REGRET_ONLY, 2):
		case flagswitch(REGRET_ONLY, 3):
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

		case flagswitch(FULL_ALLOC, 2):
			computeprobs( buffer, checksum, riverdata[cardsi].fullarr2[allocindex].stratn, numa );
			break;
		case flagswitch(FULL_ALLOC, 3):
			computeprobs( buffer, checksum, riverdata[cardsi].fullarr3[allocindex].stratn, numa );
			break;
	
		default: REPORT("catastrophic error: flags="+tostring(flags)+" numa="+tostring(numa));
		}
	}
	else
	{
		FStore_type * stratn;
		data[gr][numa]->getstratn( combine(cardsi, actioni, getactmax(gr,numa)), stratn, numa );
		computeprobs( buffer, checksum, stratn, numa );
	}
}

void MemoryManager::writestratn( int gr, int numa, int actioni, int cardsi, FWorking_type prob, FWorking_type * stratt )
{
	if(gr == 3)
	{
		const int fullindex = riveractioni(numa, actioni);
		riverdata[cardsi].writestratncount[fullindex]++;
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

		case flagswitch(REGRET_ONLY, 2):
		{
			FullData<2> & newnode = promote( allocindex, riverdata[cardsi].regretarr2, riverdata[cardsi].fullarr2, FULL_ALLOC, riverdata[cardsi].indexarr, myindexdata );
			computestratn(newnode.stratn, prob, stratt, numa);
			break;
		}

		case flagswitch(REGRET_ONLY, 3):
		{
			FullData<3> & newnode = promote( allocindex, riverdata[cardsi].regretarr3, riverdata[cardsi].fullarr3, FULL_ALLOC, riverdata[cardsi].indexarr, myindexdata );
			computestratn(newnode.stratn, prob, stratt, numa);
			break;
		}

		case flagswitch(FULL_ALLOC, 2):
			computestratn(riverdata[cardsi].fullarr2[allocindex].stratn, prob, stratt, numa);
			break;

		case flagswitch(FULL_ALLOC, 3):
			computestratn(riverdata[cardsi].fullarr3[allocindex].stratn, prob, stratt, numa);
			break;

		default: REPORT("catastrophic error: flags="+tostring(flags)+" numa="+tostring(numa));
		}
	}
	else
	{
		FStore_type * stratn;
		data[gr][numa]->getstratn(combine(cardsi, actioni, getactmax(gr,numa)), stratn, numa);
		computestratn(stratn, prob, stratt, numa);
	}
}

template < int P >
inline void MemoryManager::writeregret( int gr, int numa, int actioni, int cardsi, 
		FWorking_type prob, FWorking_type avgutility, tuple<FWorking_type,FWorking_type> * utility )
{
	if(gr == 3)
	{
		const int fullindex = riveractioni(numa, actioni);
		riverdata[cardsi].writeregretcount[fullindex]++;
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

		case flagswitch(STRATN_ONLY, 2):
		{
			FullData<2> & newnode = promote( allocindex, riverdata[cardsi].stratnarr2, riverdata[cardsi].fullarr2, FULL_ALLOC, riverdata[cardsi].indexarr, myindexdata );
			computeregret<P> ( newnode.regret, prob, avgutility, utility, numa );
		}
		break;

		case flagswitch(STRATN_ONLY, 3):
		{
			FullData<3> & newnode = promote( allocindex, riverdata[cardsi].stratnarr3, riverdata[cardsi].fullarr3, FULL_ALLOC, riverdata[cardsi].indexarr, myindexdata );
			computeregret<P> ( newnode.regret, prob, avgutility, utility, numa );
		}
		break;

		case flagswitch(REGRET_ONLY, 2):
			computeregret<P> ( riverdata[cardsi].regretarr2[allocindex].regret, prob, avgutility, utility, numa );
			break;

		case flagswitch(REGRET_ONLY, 3):
			computeregret<P> ( riverdata[cardsi].regretarr3[allocindex].regret, prob, avgutility, utility, numa );
			break;

		case flagswitch(FULL_ALLOC, 2):
			computeregret<P> ( riverdata[cardsi].fullarr2[allocindex].regret, prob, avgutility, utility, numa );
			break;

		case flagswitch(FULL_ALLOC, 3):
			computeregret<P> ( riverdata[cardsi].fullarr3[allocindex].regret, prob, avgutility, utility, numa );
			break;

		default: REPORT("catastrophic error: flags="+tostring(flags)+" numa="+tostring(numa));
		}
	}
	else
	{
		FStore_type * regret;
		data[gr][numa]->getregret(combine(cardsi, actioni, getactmax(gr,numa)), regret, numa);
		computeregret<P>(regret, prob, avgutility, utility, numa);
	}
}

// ---------------------------------------------------------------------------

// these are here to avoid putting my templated functions in the header file, 
// instantiate them in this cpp file to be linked to writeregret<..,..> is defined inline
void MemoryManager::writeregret0( int gr, int numa, int actioni, int cardsi, 
		FWorking_type prob, FWorking_type avgutility, tuple<FWorking_type,FWorking_type> * utility )
{
	writeregret<0>(gr,numa,actioni,cardsi,prob,avgutility,utility);
}

void MemoryManager::writeregret1( int gr, int numa, int actioni, int cardsi, 
		FWorking_type prob, FWorking_type avgutility, tuple<FWorking_type,FWorking_type> * utility )
{
	writeregret<1>(gr,numa,actioni,cardsi,prob,avgutility,utility);
}

//used for outputting info in MemoryManager ctor only
int getsize(int r, int nacts)
{
	return (r==3?sizeof(double):sizeof(FStore_type)) * 2 * nacts;
}
	
//cout how much memory will use, then allocate it
MemoryManager::MemoryManager(const BettingTree &bettingtree, const CardMachine &cardmachine)
	: cardmach(cardmachine),
	tree(bettingtree),
	data( boost::extents[3][boost::multi_array_types::extent_range(2,10)] ),
	mastercompactflag( false )
{
	//print out space requirements, then pause

	cout << "floating point type FWorking_type is " << FWORKING_TYPENAME << " and uses " << sizeof(FWorking_type) << " bytes." << endl;

	int64 totalbytes = 0;
	int64 riverbytes = 0;

	for(int gr=0; gr<4; gr++)
	{
		cout << "round " << gr << " uses " << cardmach.getcardsimax(gr) << " card indexings..." << endl;

		for(int a=2; a<=MAX_ACTIONS; a++)
		{
			if((int64)getactmax(gr,a)*(int64)cardmach.getcardsimax(gr) > 0x000000007fffffffLL)
				REPORT("your indices may overflow. fix that.");
			int64 mybytes = getactmax(gr,a)*getsize(gr,a)*cardmach.getcardsimax(gr);
			if(mybytes>0)
			{
				totalbytes += mybytes;
				cout << "round " << gr << " uses " << getactmax(gr,a) << " nodes with " << a << " members for a total of " << space(mybytes) << endl;
			}
			if(gr==3) riverbytes += mybytes;
		}
	}

	for(int gr=1; gr<4; gr++)
	{
		cout << cardmach.getparams().bin_max[gr] << " rnd" << gr << " bins use: " << space(cardmach.getparams().filesize[gr]) << endl;
		totalbytes += cardmach.getparams().filesize[gr];
	}

	cout << "CardsiContainers use " << sizeof(CardsiContainer) << " bytes each for total of " << space(cardmach.getcardsimax(3)*sizeof(CardsiContainer)) << endl;
	totalbytes += cardmach.getcardsimax(3)*sizeof(CardsiContainer);

	cout << "Extra counts per node are " << EXTRA_CARDSI_BYTES_PER_NODE << " bytes amounting to " 
		<< space(cardmach.getcardsimax(3)*(getactmax(3,2)+getactmax(3,3))*EXTRA_CARDSI_BYTES_PER_NODE) << " in the hugebuffer." << endl;
	totalbytes += cardmach.getcardsimax(3)*(getactmax(3,2)+getactmax(3,3))*EXTRA_CARDSI_BYTES_PER_NODE;

	cout << "River nodes use " << space(riverbytes) << " when fully allocated using datatype of size " << space(getsize(3,2)/(2*2)) << endl;

	cout << "total: " << space(totalbytes) << endl;

	cout << endl << "How much memory (in GB) would you like? ";
	double gigabytes;
	cin >> gigabytes;
	int64 nbytes = (int64)(gigabytes*1024.0*1024.0*1024.0) + 1;
	nbytes = (nbytes >> 23) << 23; //round down to nearest 8 MB
	cout << "Very well. I shall allocate " << nbytes << " bytes." << endl;

	cout << "allocating memory..." << endl;

	//now allocate memory

	//for river
	hugebuffer = new HugeBuffer( nbytes ); //global
	memory = this; //global, must be set before CardiContainer constructor is run. (next line)
	riverdata = new CardsiContainer[ cardmach.getcardsimax(3) ] ( );

	//... for preflop, flop, turn...
	for (int r=0; r<3; r++)
		for(int n=2; n<10; n++)
			data[r][n] = new DataContainer<FStore_type>(n, getactmax(r,n)*cardmach.getcardsimax(r));
}

MemoryManager::~MemoryManager()
{
	for (int r=0; r<3; r++)
		for(int n=2; n<10; n++)
			delete data[r][n];

	if(riverdata != NULL)
		delete[] riverdata;

	if(hugebuffer != NULL)
		delete hugebuffer;
}

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

int64 MemoryManager::save(const string &filename)
{
	savecounts(filename);

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
	return hugebuffer->CompactData();
}

int64 MemoryManager::GetHugeBufferSize()
{
	return hugebuffer->GetSize();
}
