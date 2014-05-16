#include "iostream"
#include "map"
#include "math.h"
#include "string"
#include "vector"
#include "fstream"
#define SEPERATOR ((unsigned)-1)

using namespace std;


typedef unsigned docid;
typedef unsigned doclen;

template<class U>
inline void
pack_uint(std::string & s, U value)
{
	// Check U is an unsigned type.
	//STATIC_ASSERT_UNSIGNED_TYPE(U);

	while (value >= 128) {
		s += static_cast<char>(static_cast<unsigned char>(value) | 0x80);
		value >>= 7;
	}
	s += static_cast<char>(value);
}

/** Decode an unsigned integer from a string.
 *
 *  @param p	    Pointer to pointer to the current position in the string.
 *  @param end	    Pointer to the end of the string.
 *  @param result   Where to store the result (or NULL to just skip it).
 */
template<class U>
inline bool
unpack_uint(const char ** p, const char * end, U * result)
{
    // Check U is an unsigned type.
   // STATIC_ASSERT_UNSIGNED_TYPE(U);

    const char * ptr = *p;
   //Assert(ptr);
    const char * start = ptr;

    // Check the length of the encoded integer first.
    do {
	if (ptr == end) {
	    // Out of data.
	    *p = NULL;
	    return false;
	}
    } while (static_cast<unsigned char>(*ptr++) >= 128);

    *p = ptr;

    if (!result) return true;

    *result = U(*--ptr);
    if (ptr == start) {
	// Special case for small values.
	return true;
    }

    size_t maxbits = size_t(ptr - start) * 7;
    if (maxbits <= sizeof(U) * 8) {
	// No possibility of overflow.
	do {
	    unsigned char chunk = static_cast<unsigned char>(*--ptr) & 0x7f;
	    *result = (*result << 7) | U(chunk);
	} while (ptr != start);
	return true;
    }

    size_t minbits = maxbits - 6;
    if (minbits > sizeof(U) * 8) {
	// Overflow.
	return false;
    }

    while (--ptr != start) {
	unsigned char chunk = static_cast<unsigned char>(*--ptr) & 0x7f;
	*result = (*result << 7) | U(chunk);
    }

    U tmp = *result;
    *result <<= 7;
    if (*result < tmp) {
	// Overflow.
	return false;
    }
    *result |= U(static_cast<unsigned char>(*ptr) & 0x7f);
    return true;
}

template<class U>
inline 
bool pack_uint_in_bytes( U n, int n_bytes, string& chunk )
{
	while ( n_bytes-- )
	{
		chunk += static_cast<char>(n&0xff);
		n >>= 8;
	}
	return true;
}

template<class U>
inline
bool unpack_uint_in_bytes( const char** pos, int n_bytes, U* result )
{
	int inc = n_bytes;
	while ( inc-- )
	{
		(*pos)++;
	}
	const char* ptr = (*pos)-1;
	U r = 0;
	while ( n_bytes-- )
	{
		r <<= 8;
		r |= *ptr;
		ptr--;
	}
	*result = r;
	return true;
}

inline
unsigned get_max_bytes( unsigned n )
{
	if ( n== 0 )
	{
		return 1;
	}
	unsigned l=0;
	while ( n )
	{
		l++;
		n >>= 8;
	}
	return l;
}

bool buildVector( const map<docid,doclen>& postlist, vector<unsigned>& src, unsigned& bias )
{
	map<docid,doclen>::const_iterator it = postlist.begin(), start_pos;
	bias = it->first;
	docid docid_before_start_pos = it->first;
	unsigned min_contiguous_length = 3;

	while ( it!=postlist.end() )
	{
		unsigned length_contiguous = 1;
		docid last_docid = it->first, cur_docid = 0;
		unsigned max_bytes = get_max_bytes(it->second);

		start_pos = it;
		it++;

		while ( it!=postlist.end() )
		{
			cur_docid = it->first;
			if ( cur_docid != last_docid+1 || get_max_bytes(it->second)>max_bytes )
			{
				break;
			}
			length_contiguous++;
			last_docid = cur_docid;
			it++;
		}

		if ( length_contiguous > min_contiguous_length )
		{
			src.push_back(SEPERATOR);
			src.push_back(start_pos->first-docid_before_start_pos);	
			src.push_back(length_contiguous);
			src.push_back(max_bytes);
			while ( start_pos!=it )
			{
				src.push_back(start_pos->second);
				docid_before_start_pos = start_pos->first;
				start_pos++;
			}
		}
		else
		{
			while ( start_pos!=it )
			{
				src.push_back(start_pos->first-docid_before_start_pos);
				src.push_back(start_pos->second);
				docid_before_start_pos = start_pos->first;
				start_pos++;
			}
		}

	}
	return true;
}

bool encode( const vector<unsigned>& src, unsigned bias, string& chunk )
{
	int i=0;
	pack_uint( chunk, bias );
	while ( i<(int)src.size() )
	{
		if ( src[i] != SEPERATOR )
		{
			pack_uint( chunk, src[i] );
			pack_uint( chunk, src[i+1] );
			i += 2;
		}
		else
		{
			pack_uint( chunk, src[i] );
			pack_uint( chunk, src[i+1] );
			unsigned len = src[i+2];
			unsigned bytes = src[i+3];
			pack_uint_in_bytes( len, 2, chunk );
			pack_uint_in_bytes( bytes, 1, chunk );
			for ( unsigned j=0 ; j<len ; ++j )
			{
				pack_uint_in_bytes( src[i+4+j], bytes, chunk );
			}
			i += 4+len;
		}
		
	}
	return true;
}

void inputMap( map<docid,doclen>& postlist )
{
	postlist[4]=5;
	postlist[6]=254;
	for ( int i=9 ; i<13 ; ++i )
	{
		postlist[i]=2*i;
	}
	postlist[27]=32;
	postlist[35]=68;
	for ( int i=40 ; i<42 ; ++i )
	{
		postlist[i]=i-39;
	}
	for ( int i=47; i<52 ; ++i )
	{
		postlist[i]=256+3*i;
	}
}

ostream& operator << ( ostream& out, const map<docid,doclen>& postlist )
{
	map<docid,doclen>::const_iterator it = postlist.begin();
	while ( it!=postlist.end() )
	{
		out << it->first << " : " << it->second << endl;
		it++;
	}
	return out;
}

int main()
{

	map<docid,doclen> postlist;
	vector<unsigned> src;
	string chunk;
	unsigned r = 0;
	unsigned bias;
	
	inputMap(postlist);
	
	//ofstream out("data.txt");
	//out<<postlist;
	//out.close();

	//pack_uint_in_bytes( 25, 1, chunk );
	//pack_uint_in_bytes( 563, 2, chunk );
	//pack_uint_in_bytes( 1, 2, chunk );
	//const char* pos = chunk.data();
	//unpack_uint_in_bytes( &pos, 1, &r );
	//unpack_uint_in_bytes( &pos, 2, &r );
	//unpack_uint_in_bytes( &pos, 1, &r );

	buildVector(postlist,src,bias);	
	encode( src,bias,chunk );
	return 0;
}