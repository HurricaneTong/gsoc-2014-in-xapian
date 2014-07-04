#pragma once
#include "string"

#define SEPERATOR ((unsigned)-1)
#define DOCLEN_CHUNK_SKIPLIST 1u
#define DOCLEN_CHUNK_FIXEDWIDTH 2u
#define DOCLEN_CHUNK_MIN_SKIPLIST_LENGTH 8
#define DOCLEN_CHUNK_MIN_CONTIGUOUS_LENGTH 2
#define DOCLEN_CHUNK_MIN_CONTIGUOUS_BLOCKS 1
#define DOCLEN_CHUNK_MIN_CONTIGUOUS_PRECENTAGE 0.5


typedef unsigned docid;
typedef unsigned doclen;

using std::string;

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
		r |= (*ptr)&0xff;
		ptr--;
	}
	*result = r;
	return true;
}

template<class U>
inline
bool read_chunk_header( const char** pos, const char* end, U* format_info )
{
	U u=0;
	if ( format_info == NULL )
	{
		format_info = &u;
	}
	return unpack_uint( pos,end,format_info );
}