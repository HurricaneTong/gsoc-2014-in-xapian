#pragma once
#include "string"
#include "map"

#define SEPERATOR ((unsigned)-1)
#define DOCLEN_CHUNK_MIN_CONTIGUOUS_LENGTH 5
#define DOCLEN_CHUNK_MIN_GOOD_BYTES_RATIO 0.8
#define MAX_ENTRIES_IN_CHUNK 2000


typedef unsigned docid;
typedef unsigned doclen;
typedef unsigned doccount;
typedef unsigned termcount;
typedef std::map<std::string,std::string> BTree;

using std::string;

/** How many bits to store the length of a sortable uint in.
 *
 *  Setting this to 2 limits us to 2**32 documents in the database.  If set
 *  to 3, then 2**64 documents are possible, but the database format isn't
 *  compatible.
 */
const unsigned int SORTABLE_UINT_LOG2_MAX_BYTES = 2;

/// Calculated value used below.
const unsigned int SORTABLE_UINT_MAX_BYTES = 1 << SORTABLE_UINT_LOG2_MAX_BYTES;

/// Calculated value used below.
const unsigned int SORTABLE_UINT_1ST_BYTE_MASK =
	(0xffu >> SORTABLE_UINT_LOG2_MAX_BYTES);

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

/** Append an encoded bool to a string.
 *
 *  @param s		The string to append to.
 *  @param value	The bool to encode.
 */
inline void
pack_bool(std::string & s, bool value)
{
    s += char('0' | static_cast<char>(value));
}

/** Decode a bool from a string.
 *
 *  @param p	    Pointer to pointer to the current position in the string.
 *  @param end	    Pointer to the end of the string.
 *  @param result   Where to store the result.
 */
inline bool
unpack_bool(const char ** p, const char * end, bool * result)
{
    const char * & ptr = *p;

    char ch;
    if (ptr == end || ((ch = *ptr++ - '0') &~ 1)) {
	ptr = NULL;
	return false;
    }
    *result = static_cast<bool>(ch);
    return true;
}

/** Append an encoded unsigned integer to a string, preserving the sort order.
 *
 *  The appended string data will sort in the same order as the unsigned
 *  integer being encoded.
 *
 *  Note that the first byte of the encoding will never be \xff, so it is
 *  safe to store the result of this function immediately after the result of
 *  pack_string_preserving_sort().
 *
 *  @param s		The string to append to.
 *  @param value	The unsigned integer to encode.
 */
template<class U>
inline void
pack_uint_preserving_sort(std::string & s, U value)
{
    // Check U is an unsigned type.

    char tmp[sizeof(U) + 1];
    char * p = tmp + sizeof(tmp);

    do {
	*--p = char(value & 0xff);
	value >>= 8;
    } while (value &~ SORTABLE_UINT_1ST_BYTE_MASK);

    unsigned char len = static_cast<unsigned char>(tmp + sizeof(tmp) - p);
    *--p = char((len - 1) << (8 - SORTABLE_UINT_LOG2_MAX_BYTES) | value);
    s.append(p, len + 1);
}

/** Decode an "sort preserved" unsigned integer from a string.
 *
 *  The unsigned integer must have been encoded with
 *  pack_uint_preserving_sort().
 *
 *  @param p	    Pointer to pointer to the current position in the string.
 *  @param end	    Pointer to the end of the string.
 *  @param result   Where to store the result.
 */
template<class U>
inline bool
unpack_uint_preserving_sort(const char ** p, const char * end, U * result)
{
    // Check U is an unsigned type.
    const char * ptr = *p;


    if (ptr == end) {
	return false;
    }

    unsigned char len_byte = static_cast<unsigned char>(*ptr++);
    *result = len_byte & SORTABLE_UINT_1ST_BYTE_MASK;
    size_t len = (len_byte >> (8 - SORTABLE_UINT_LOG2_MAX_BYTES)) + 1;

    if (size_t(end - ptr) < len) {
	return false;
    }

    end = ptr + len;
    *p = end;

    // Check for overflow.
    if (len > int(sizeof(U))) {
	return false;
    }

    while (ptr != end) {
	*result = (*result << 8) | U(static_cast<unsigned char>(*ptr++));
    }

    return true;
}


/** Append an encoded std::string to a string, preserving the sort order.
 *
 *  The byte which follows this encoded value *must not* be \xff, or the sort
 *  order won't be correct.  You may need to store a padding byte (\0 say) to
 *  ensure this.  Note that pack_uint_preserving_sort() can never produce
 *  \xff as its first byte so is safe to use immediately afterwards.
 *
 *  @param s		The string to append to.
 *  @param value	The std::string to encode.
 *  @param last		If true, this is the last thing to be encoded in this
 *			string - see note below (default: false)
 *
 *  It doesn't make sense to use pack_string_preserving_sort() if nothing can
 *  ever follow, but if optional items can, you can set last=true in cases
 *  where nothing does and get a shorter encoding in those cases.
 */
inline void
pack_string_preserving_sort(std::string & s, const std::string & value,
			    bool last = false)
{
    std::string::size_type b = 0, e;
    while ((e = value.find('\0', b)) != std::string::npos) {
	++e;
	s.append(value, b, e - b);
	s += '\xff';
	b = e;
    }
    s.append(value, b, std::string::npos);
    if (!last) s += '\0';
}


inline std::string
	pack_chert_postlist_key(const std::string &term)
{
	// Special case for doclen lists.
	if (term.empty())
		return std::string("\x00\xe0", 2);

	std::string key;
	pack_string_preserving_sort(key, term, true);
	return key;
}

inline std::string
	pack_chert_postlist_key(const std::string &term, docid did)
{
	// Special case for doclen lists.
	if (term.empty()) {
		std::string key("\x00\xe0", 2);
		pack_uint_preserving_sort(key, did);
		return key;
	}

	std::string key;
	pack_string_preserving_sort(key, term);
	pack_uint_preserving_sort(key, did);
	return key;
}

inline std::string
	pack_brass_postlist_key(const std::string &term)
{
	return pack_chert_postlist_key(term);
}

inline std::string
	pack_brass_postlist_key(const std::string &term, docid did)
{
	return pack_chert_postlist_key(term, did);
}

/// Compose a key from a termname and docid.
static string make_key(const string & term, docid did) {
	return pack_brass_postlist_key(term, did);
}

/// Compose a key from a termname.
static string make_key(const string & term) {
	return pack_brass_postlist_key(term);
}

/** Decode a "sort preserved" std::string from a string.
 *
 *  The std::string must have been encoded with pack_string_preserving_sort().
 *
 *  @param p	    Pointer to pointer to the current position in the string.
 *  @param end	    Pointer to the end of the string.
 *  @param result   Where to store the result.
 */
inline bool
unpack_string_preserving_sort(const char ** p, const char * end,
			      std::string & result)
{
    result.resize(0);

    const char *ptr = *p;

    while (ptr != end) {
	char ch = *ptr++;
	if (ch == '\0') {
	    if (ptr == end || *ptr != '\xff') {
		break;
	    }
	    ++ptr;
	}
	result += ch;
    }
    *p = ptr;
    return true;
}



static inline bool get_tname_from_key(const char **src, const char *end,
									  string &tname)
{
	return unpack_string_preserving_sort(src, end, tname);
}

static inline bool
	check_tname_in_key_lite(const char **keypos, const char *keyend, const string &tname)
{
	string tname_in_key;

	if (keyend - *keypos >= 2 && (*keypos)[0] == '\0' && (*keypos)[1] == '\xe0') {
		*keypos += 2;
	} else {
		// Read the termname.
		get_tname_from_key(keypos, keyend, tname_in_key);
	}

	// This should only fail if the postlist doesn't exist at all.
	return tname_in_key == tname;
}

static inline bool
	check_tname_in_key(const char **keypos, const char *keyend, const string &tname)
{
	if (*keypos == keyend) return false;

	return check_tname_in_key_lite(keypos, keyend, tname);
}


static void read_number_of_entries(const char ** posptr,
							const char * end,
							doccount * number_of_entries_ptr,
							termcount * collection_freq_ptr)
{
	unpack_uint(posptr, end, number_of_entries_ptr);
	unpack_uint(posptr, end, collection_freq_ptr);
}

static docid
	read_start_of_chunk(const char ** posptr,
	const char * end,
	docid first_did_in_chunk,
	bool * is_last_chunk_ptr)
{

	// Read whether this is the last chunk
	unpack_bool(posptr, end, is_last_chunk_ptr);

	// Read what the final document ID in this chunk is.
	docid increase_to_last;
	unpack_uint(posptr, end, &increase_to_last);
	docid last_did_in_chunk = first_did_in_chunk + increase_to_last;
	return last_did_in_chunk;
}


static docid
	read_start_of_first_chunk(const char ** posptr,
	const char * end,
	doccount * number_of_entries_ptr,
	termcount * collection_freq_ptr)
{
	read_number_of_entries(posptr, end,
		number_of_entries_ptr, collection_freq_ptr);

	docid did;
	// Read the docid of the first entry in the posting list.
	unpack_uint(posptr, end, &did);
	return did;
}
