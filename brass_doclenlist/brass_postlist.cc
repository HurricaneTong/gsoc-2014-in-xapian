/* brass_postlist.cc: Postlists in a brass database
 *
 * Copyright 1999,2000,2001 BrightStation PLC
 * Copyright 2002,2003,2004,2005,2007,2008,2009,2011,2013 Olly Betts
 * Copyright 2007,2008,2009 Lemur Consulting Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#include <config.h>

#include "brass_postlist.h"

#include "brass_cursor.h"
#include "brass_database.h"
#include "debuglog.h"
#include "noreturn.h"
#include "pack.h"
#include "str.h"
#include "unicode/description_append.h"

using Xapian::Internal::intrusive_ptr;

// Static functions

/// Report an error when reading the posting list.
XAPIAN_NORETURN(static void report_read_error(const char * position));
static void report_read_error(const char * position)
{
	if (position == 0) {
		// data ran out
		LOGLINE(DB, "BrassPostList data ran out");
		throw Xapian::DatabaseCorruptError("Data ran out unexpectedly when reading posting list.");
	}
	// overflow
	LOGLINE(DB, "BrassPostList value too large");
	throw Xapian::RangeError("Value in posting list too large.");
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
		if (!get_tname_from_key(keypos, keyend, tname_in_key))
			report_read_error(*keypos);
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

/// Read the start of the first chunk in the posting list.
static Xapian::docid
	read_start_of_first_chunk(const char ** posptr,
	const char * end,
	Xapian::doccount * number_of_entries_ptr,
	Xapian::termcount * collection_freq_ptr)
{
	LOGCALL_STATIC(DB, Xapian::docid, "read_start_of_first_chunk", (const void *)posptr | (const void *)end | (void *)number_of_entries_ptr | (void *)collection_freq_ptr);

	BrassPostList::read_number_of_entries(posptr, end,
		number_of_entries_ptr, collection_freq_ptr);
	if (number_of_entries_ptr)
		LOGVALUE(DB, *number_of_entries_ptr);
	if (collection_freq_ptr)
		LOGVALUE(DB, *collection_freq_ptr);

	Xapian::docid did;
	// Read the docid of the first entry in the posting list.
	if (!unpack_uint(posptr, end, &did))
		report_read_error(*posptr);
	++did;
	LOGVALUE(DB, did);
	RETURN(did);
}

static inline void
	read_did_increase(const char ** posptr, const char * end,
	Xapian::docid * did_ptr)
{
	Xapian::docid did_increase;
	if (!unpack_uint(posptr, end, &did_increase)) report_read_error(*posptr);
	*did_ptr += did_increase + 1;
}

/// Read the wdf for an entry.
static inline void
	read_wdf(const char ** posptr, const char * end, Xapian::termcount * wdf_ptr)
{
	if (!unpack_uint(posptr, end, wdf_ptr)) report_read_error(*posptr);
}

/// Read the start of a chunk.
static Xapian::docid
	read_start_of_chunk(const char ** posptr,
	const char * end,
	Xapian::docid first_did_in_chunk,
	bool * is_last_chunk_ptr)
{
	LOGCALL_STATIC(DB, Xapian::docid, "read_start_of_chunk", reinterpret_cast<const void*>(posptr) | reinterpret_cast<const void*>(end) | first_did_in_chunk | reinterpret_cast<const void*>(is_last_chunk_ptr));
	Assert(is_last_chunk_ptr);

	// Read whether this is the last chunk
	if (!unpack_bool(posptr, end, is_last_chunk_ptr))
		report_read_error(*posptr);
	LOGVALUE(DB, *is_last_chunk_ptr);

	// Read what the final document ID in this chunk is.
	Xapian::docid increase_to_last;
	if (!unpack_uint(posptr, end, &increase_to_last))
		report_read_error(*posptr);
	Xapian::docid last_did_in_chunk = first_did_in_chunk + increase_to_last;
	LOGVALUE(DB, last_did_in_chunk);
	RETURN(last_did_in_chunk);
}

static inline
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

/** Make the data to go at the start of the very first chunk.
 */
static inline string
make_start_of_first_chunk(Xapian::doccount entries,
			  Xapian::termcount collectionfreq,
			  Xapian::docid new_did)
{
    string chunk;
    pack_uint(chunk, entries);
    pack_uint(chunk, collectionfreq);
    pack_uint(chunk, new_did - 1);
    return chunk;
}

/** Make the data to go at the start of a standard chunk.
 */
static inline string
make_start_of_chunk(bool new_is_last_chunk,
		    Xapian::docid new_first_did,
		    Xapian::docid new_final_did)
{
    Assert(new_final_did >= new_first_did);
    string chunk;
    pack_bool(chunk, new_is_last_chunk);
    pack_uint(chunk, new_final_did - new_first_did);
    return chunk;
}

static void
write_start_of_chunk(string & chunk,
		     unsigned int start_of_chunk_header,
		     unsigned int end_of_chunk_header,
		     bool is_last_chunk,
		     Xapian::docid first_did_in_chunk,
		     Xapian::docid last_did_in_chunk)
{
    Assert((size_t)(end_of_chunk_header - start_of_chunk_header) <= chunk.size());

    chunk.replace(start_of_chunk_header,
		  end_of_chunk_header - start_of_chunk_header,
		  make_start_of_chunk(is_last_chunk, first_did_in_chunk,
				      last_did_in_chunk));
}


FixedWidthChunk::FixedWidthChunk( const map<Xapian::docid,Xapian::doclen>& postlist )
{
	buildVector( postlist );
}

bool FixedWidthChunk::buildVector( const map<Xapian::docid,Xapian::doclen>& postlist )
{
	map<Xapian::docid,Xapian::doclen>::const_iterator it = postlist.begin(), start_pos;
	bias = it->first;
	Xapian::docid docid_before_start_pos = it->first;

	int chunk_size = 0;

	while ( it!=postlist.end() )
	{
		unsigned length_contiguous = 1;
		Xapian::docid last_docid = it->first, cur_docid = 0;
		unsigned max_bytes = get_max_bytes(it->second);
		unsigned used_bytes = 0;
		unsigned good_bytes = 0;

		start_pos = it;
		it++;

		while ( it!=postlist.end() )
		{
			cur_docid = it->first;
			unsigned cur_bytes = get_max_bytes(it->second);
			if ( cur_docid != last_docid+1 || cur_bytes>max_bytes )
			{
				break;
			}
			used_bytes += max_bytes;
			good_bytes += cur_bytes;
			if ( (float)good_bytes/used_bytes < DOCLEN_CHUNK_MIN_GOOD_BYTES_RATIO )
			{
				used_bytes = 0;
				good_bytes = 0;
				break;
			}

			length_contiguous++;
			last_docid = cur_docid;
			it++;
		}

		if ( length_contiguous > DOCLEN_CHUNK_MIN_CONTIGUOUS_LENGTH )
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

bool FixedWidthChunk::encode( string& chunk ) const
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

Xapian::doclen FixedWidthChunkReader::getDoclen( Xapian::docid desired_did )
{
	if ( cur_did==desired_did && doc_length!=-1 )
	{
		return doc_length;
	}
	if ( cur_did > desired_did )
	{
		pos = start_pos;
		cur_did = 0;
		unpack_uint( &pos, end, &cur_did );
	}


	Xapian::docid incre_did = 0;

	while ( pos!=end )
	{
		const char* old_pos = pos;
		unpack_uint( &pos, end, &incre_did );
		if ( incre_did != SEPERATOR )
		{
			cur_did += incre_did;
			unpack_uint( &pos, end, &doc_length );
			if ( cur_did == desired_did )
			{
				return doc_length;
			}
			continue;
		}
		else
		{
			unpack_uint( &pos, end, &incre_did );
			unsigned len=0, bytes=0;
			unpack_uint_in_bytes( &pos, 2, &len );
			unpack_uint_in_bytes( &pos, 1, &bytes );
			Xapian::docid old_cur_did = cur_did;
			cur_did += incre_did;
			if ( desired_did <= cur_did+len-1 )
			{
				pos += bytes*(desired_did-cur_did);
				unpack_uint_in_bytes( &pos, bytes, &doc_length );			

				pos = old_pos;
				cur_did = old_cur_did;

				return doc_length;
			}
			pos += bytes*len;
			cur_did += len-1;
		}

	}
	return -1;
}


bool DoclenChunkWriter::get_new_doclen( const map<Xapian::docid,Xapian::doclen>*& p_new_doclen )
{
	if ( chunk_from.empty() )
	{
		p_new_doclen = &changes;
	}
	else
	{
		map<Xapian::docid,Xapian::doclen>::const_iterator it = changes.begin();
		const char* pos = chunk_from.data();
		const char* end = pos+chunk_from.size();

		if ( is_first_chunk )
		{
			read_start_of_first_chunk( &pos, end, NULL, NULL );
		}
		read_start_of_chunk( &pos, end, 0, &is_last_chunk ); 

		Xapian::docid bias = 0, cur_did = 0, inc_did = 0;
		Xapian::doclen doc_len = 0;
		unpack_uint( &pos, end, &bias );
		cur_did = bias;
		while ( pos!=end )
		{
			unpack_uint( &pos, end, &inc_did );
			if ( inc_did != SEPERATOR )
			{
				cur_did += inc_did;
				unpack_uint( &pos, end, &doc_len );
				//new_doclen.insert( new_doclen.end(), make_pair(cur_did,doc_len) );
				new_doclen[cur_did] = doc_len;
				continue;
			}
			else
			{
				unpack_uint( &pos, end, &inc_did );
				unsigned len=0, bytes=0;
				unpack_uint_in_bytes( &pos, 2, &len );
				unpack_uint_in_bytes( &pos, 1, &bytes );
				cur_did += inc_did;
				while ( len-- )
				{
					unpack_uint_in_bytes( &pos, bytes, &doc_len );
					//new_doclen.insert( new_doclen.end(), make_pair(cur_did,doc_len) );
					new_doclen[cur_did] = doc_len;
					cur_did++;
				}
				cur_did--;
			}

		}

		map<Xapian::docid,Xapian::doclen>::const_iterator chg_it = changes.begin();
		map<Xapian::docid,Xapian::doclen>::iterator ori_it = new_doclen.begin();

		while ( chg_it != changes.end() )
		{
			while ( chg_it->first > ori_it->first )
			{
				++ori_it;
				if ( ori_it == new_doclen.end() )
				{
					break;
				}
			}
			if ( ori_it == new_doclen.end() )
			{
				new_doclen.insert( ori_it, *chg_it );
				++chg_it;
				while ( chg_it != changes.end() )
				{
					new_doclen.insert( ori_it, *chg_it );
					++chg_it;
				}
				break;
			}
			if ( ori_it->first == chg_it->first )
			{
				if ( chg_it->second != SEPERATOR )
				{
					ori_it->second = chg_it->second;
				}
				else
				{
					ori_it = new_doclen.erase( ori_it );
				}
			}
			else
			{
				new_doclen.insert( ori_it, *chg_it );
			}
			++chg_it;
		}

		p_new_doclen = &new_doclen;
	}
	return true;
}

bool DoclenChunkWriter::merge_doclen_changes( )
{
	const map<Xapian::docid,Xapian::doclen>* p_new_doclen = NULL;
	get_new_doclen( p_new_doclen );

	map<Xapian::docid,Xapian::doclen>::const_iterator start_pos, end_pos;
	start_pos = end_pos = p_new_doclen->begin();
	if ( p_new_doclen->size() <= MAX_ENTRIES_IN_CHUNK )
	{
		string cur_chunk;
		FixedWidthChunk fwc( changes );
		end_pos = p_new_doclen->end();
		end_pos--;
		string head_of_chunk = make_start_of_chunk( is_last_chunk,start_pos->first,end_pos->first );
		cur_chunk = head_of_chunk+cur_chunk;
		fwc.encode( cur_chunk );
		if ( is_first_chunk )
		{
			string head_of_first_chunk = 
				make_start_of_first_chunk( changes.size(), 0, start_pos->first );
			cur_chunk = head_of_first_chunk+cur_chunk;
		}

		string cur_key = make_key( string(), start_pos->first );
		postlist_table->add(cur_key,cur_chunk);
		//b_tree->insert( make_pair(cur_key,cur_chunk) ); 
	}
	else
	{
		vector< map<Xapian::docid,Xapian::doclen> > doc_len_list;
		int count = 0;
		while ( end_pos!=p_new_doclen->end() )
		{
			end_pos++;
			count++;
			if ( count == MAX_ENTRIES_IN_CHUNK )
			{
				doc_len_list.push_back( map<Xapian::docid,Xapian::doclen>(start_pos,end_pos) );
				count = 0;
				start_pos = end_pos;
			}
		}
		if ( start_pos != end_pos )
		{
			doc_len_list.push_back( map<Xapian::docid,Xapian::doclen>(start_pos,end_pos) );
		}

		for ( int i=0 ; i<(int)doc_len_list.size() ; ++i )
		{
			string cur_chunk, cur_key;
			map<Xapian::docid,Xapian::doclen>::iterator it = doc_len_list[i].end();
			it--;
			if ( i==(int)doc_len_list.size()-1 && is_last_chunk )
			{
				cur_chunk = make_start_of_chunk( true, 
					doc_len_list[i].begin()->first, it->first );
			}
			else
			{
				cur_chunk = make_start_of_chunk( false, 
					doc_len_list[i].begin()->first, it->first );
			}

			FixedWidthChunk fwc( doc_len_list[i] );
			fwc.encode( cur_chunk );

			if ( i==0 && is_first_chunk ) 
			{
				string head_of_first_chunk =
					make_start_of_first_chunk( p_new_doclen->size(), 0, doc_len_list[i].begin()->first );
				cur_chunk = head_of_first_chunk+cur_chunk;
				cur_key = make_key( string() );
			}
			else
			{
				cur_key = make_key( string(), doc_len_list[i].begin()->first );
			}

			postlist_table->add(cur_key,cur_chunk);

		}
	}			
	return true;
}

DoclenChunkReader::DoclenChunkReader( const string& chunk_, bool is_first_chunk )
	: chunk(chunk_)
{
	const char* pos = chunk.data();
	const char* end = pos+chunk.size();
	if ( is_first_chunk )
	{
		read_start_of_first_chunk( &pos, end, NULL, NULL );
	}
	bool is_last_chunk;
	read_start_of_chunk( &pos, end, 0, &is_last_chunk );
	p_fwcr = new FixedWidthChunkReader(pos,end);
}

Xapian::docid DoclenChunkReader::get_doclen( Xapian::docid desired_did )
{
	return p_fwcr->getDoclen(desired_did);
}

Xapian::doccount
BrassPostListTable::get_termfreq(const string & term) const
{
    string key = make_key(term);
    string tag;
    if (!get_exact_entry(key, tag)) return 0;

    Xapian::doccount termfreq;
    const char * p = tag.data();
    BrassPostList::read_number_of_entries(&p, p + tag.size(), &termfreq, NULL);
    return termfreq;
}

Xapian::termcount
BrassPostListTable::get_collection_freq(const string & term) const
{
    string key = make_key(term);
    string tag;
    if (!get_exact_entry(key, tag)) return 0;

    Xapian::termcount collfreq;
    const char * p = tag.data();
    BrassPostList::read_number_of_entries(&p, p + tag.size(), NULL, &collfreq);
    return collfreq;
}

Xapian::termcount
BrassPostListTable::get_doclength(Xapian::docid did,
				  intrusive_ptr<const BrassDatabase> db) const {
    if (!doclen_pl.get()) {
	// Don't keep a reference back to the database, since this
	// would make a reference loop.
	doclen_pl.reset(new BrassPostList(db, string(), false));
    }
    if (!doclen_pl->jump_to(did))
	throw Xapian::DocNotFoundError("Document " + str(did) + " not found");
    return doclen_pl->get_wdf();
}

bool
BrassPostListTable::document_exists(Xapian::docid did,
				    intrusive_ptr<const BrassDatabase> db) const
{
    if (!doclen_pl.get()) {
	// Don't keep a reference back to the database, since this
	// would make a reference loop.
	doclen_pl.reset(new BrassPostList(db, string(), false));
    }
    return (doclen_pl->jump_to(did));
}

// How big should chunks in the posting list be?  (They
// will grow slightly bigger than this, but not more than a
// few bytes extra) - FIXME: tune this value to try to
// maximise how well blocks are used.  Or performance.
// Or indexing speed.  Or something...
const unsigned int CHUNKSIZE = 2000;

/** PostlistChunkWriter is a wrapper which acts roughly as an
 *  output iterator on a postlist chunk, taking care of the
 *  messy details.  It's intended to be used with deletion and
 *  replacing of entries, not for adding to the end, when it's
 *  not really needed.
 */
class Brass::PostlistChunkWriter {
    public:
	PostlistChunkWriter(const string &orig_key_,
			    bool is_first_chunk_,
			    const string &tname_,
			    bool is_last_chunk_);

	/// Append an entry to this chunk.
	void append(BrassTable * table, Xapian::docid did,
		    Xapian::termcount wdf);

	/// Append a block of raw entries to this chunk.
	void raw_append(Xapian::docid first_did_, Xapian::docid current_did_,
			const string & s) {
	    Assert(!started);
	    first_did = first_did_;
	    current_did = current_did_;
	    if (!s.empty()) {
		chunk.append(s);
		started = true;
	    }
	}

	/** Flush the chunk to the buffered table.  Note: this may write it
	 *  with a different key to the original one, if for example the first
	 *  entry has changed.
	 */
	void flush(BrassTable *table);

    private:
	string orig_key;
	string tname;
	bool is_first_chunk;
	bool is_last_chunk;
	bool started;

	Xapian::docid first_did;
	Xapian::docid current_did;

	string chunk;
};

using Brass::PostlistChunkWriter;



/** PostlistChunkReader is essentially an iterator wrapper
 *  around a postlist chunk.  It simply iterates through the
 *  entries in a postlist.
 */
class Brass::PostlistChunkReader {
    string data;

    const char *pos;
    const char *end;

    bool at_end;

    Xapian::docid did;
    Xapian::termcount wdf;

  public:
    /** Initialise the postlist chunk reader.
     *
     *  @param first_did  First document id in this chunk.
     *  @param data       The tag string with the header removed.
     */
    PostlistChunkReader(Xapian::docid first_did, const string & data_)
	: data(data_), pos(data.data()), end(pos + data.length()), at_end(data.empty()), did(first_did)
    {
	if (!at_end) read_wdf(&pos, end, &wdf);
    }

    Xapian::docid get_docid() const {
	return did;
    }
    Xapian::termcount get_wdf() const {
	return wdf;
    }

    bool is_at_end() const {
	return at_end;
    }

    /** Advance to the next entry.  Set at_end if we run off the end.
     */
    void next();
};

using Brass::PostlistChunkReader;

void
PostlistChunkReader::next()
{
    if (pos == end) {
	at_end = true;
    } else {
	read_did_increase(&pos, end, &did);
	read_wdf(&pos, end, &wdf);
    }
}

PostlistChunkWriter::PostlistChunkWriter(const string &orig_key_,
					 bool is_first_chunk_,
					 const string &tname_,
					 bool is_last_chunk_)
	: orig_key(orig_key_),
	  tname(tname_), is_first_chunk(is_first_chunk_),
	  is_last_chunk(is_last_chunk_),
	  started(false)
{
    LOGCALL_CTOR(DB, "PostlistChunkWriter", orig_key_ | is_first_chunk_ | tname_ | is_last_chunk_);
}

void
PostlistChunkWriter::append(BrassTable * table, Xapian::docid did,
			    Xapian::termcount wdf)
{
    if (!started) {
	started = true;
	first_did = did;
    } else {
	Assert(did > current_did);
	// Start a new chunk if this one has grown to the threshold.
	if (chunk.size() >= CHUNKSIZE) {
	    bool save_is_last_chunk = is_last_chunk;
	    is_last_chunk = false;
	    flush(table);
	    is_last_chunk = save_is_last_chunk;
	    is_first_chunk = false;
	    first_did = did;
	    chunk.resize(0);
	    orig_key = BrassPostListTable::make_key(tname, first_did);
	} else {
	    pack_uint(chunk, did - current_did - 1);
	}
    }
    current_did = did;
    pack_uint(chunk, wdf);
}


void
PostlistChunkWriter::flush(BrassTable *table)
{
    LOGCALL_VOID(DB, "PostlistChunkWriter::flush", table);

    /* This is one of the more messy parts involved with updating posting
     * list chunks.
     *
     * Depending on circumstances, we may have to delete an entire chunk
     * or file it under a different key, as well as possibly modifying both
     * the previous and next chunk of the postlist.
     */

    if (!started) {
	/* This chunk is now empty so disappears entirely.
	 *
	 * If this was the last chunk, then the previous chunk
	 * must have its "is_last_chunk" flag updated.
	 *
	 * If this was the first chunk, then the next chunk must
	 * be transformed into the first chunk.  Messy!
	 */
	LOGLINE(DB, "PostlistChunkWriter::flush(): deleting chunk");
	Assert(!orig_key.empty());
	if (is_first_chunk) {
	    LOGLINE(DB, "PostlistChunkWriter::flush(): deleting first chunk");
	    if (is_last_chunk) {
		/* This is the first and the last chunk, ie the only
		 * chunk, so just delete the tag.
		 */
		table->del(orig_key);
		return;
	    }

	    /* This is the messiest case.  The first chunk is to
	     * be removed, and there is at least one chunk after
	     * it.  Need to rewrite the next chunk as the first
	     * chunk.
	     */
	    AutoPtr<BrassCursor> cursor(table->cursor_get());

	    if (!cursor->find_entry(orig_key)) {
		throw Xapian::DatabaseCorruptError("The key we're working on has disappeared");
	    }

	    // FIXME: Currently the doclen list has a special first chunk too,
	    // which reduces special casing here.  The downside is a slightly
	    // larger than necessary first chunk and needless fiddling if the
	    // first chunk is deleted.  But really we should look at
	    // redesigning the whole postlist format with an eye to making it
	    // easier to update!

	    // Extract existing counts from the first chunk so we can reinsert
	    // them into the block we're renaming.
	    Xapian::doccount num_ent;
	    Xapian::termcount coll_freq;
	    {
		cursor->read_tag();
		const char *tagpos = cursor->current_tag.data();
		const char *tagend = tagpos + cursor->current_tag.size();

		(void)read_start_of_first_chunk(&tagpos, tagend,
						&num_ent, &coll_freq);
	    }

	    // Seek to the next chunk.
	    cursor->next();
	    if (cursor->after_end()) {
		throw Xapian::DatabaseCorruptError("Expected another key but found none");
	    }
	    const char *kpos = cursor->current_key.data();
	    const char *kend = kpos + cursor->current_key.size();
	    if (!check_tname_in_key(&kpos, kend, tname)) {
		throw Xapian::DatabaseCorruptError("Expected another key with the same term name but found a different one");
	    }

	    // Read the new first docid
	    Xapian::docid new_first_did;
	    if (!unpack_uint_preserving_sort(&kpos, kend, &new_first_did)) {
		report_read_error(kpos);
	    }

	    cursor->read_tag();
	    const char *tagpos = cursor->current_tag.data();
	    const char *tagend = tagpos + cursor->current_tag.size();

	    // Read the chunk header
	    bool new_is_last_chunk;
	    Xapian::docid new_last_did_in_chunk =
		read_start_of_chunk(&tagpos, tagend, new_first_did,
				    &new_is_last_chunk);

	    string chunk_data(tagpos, tagend);

	    // First remove the renamed tag
	    table->del(cursor->current_key);

	    // And now write it as the first chunk
	    string tag;
	    tag = make_start_of_first_chunk(num_ent, coll_freq, new_first_did);
	    tag += make_start_of_chunk(new_is_last_chunk,
					      new_first_did,
					      new_last_did_in_chunk);
	    tag += chunk_data;
	    table->add(orig_key, tag);
	    return;
	}

	LOGLINE(DB, "PostlistChunkWriter::flush(): deleting secondary chunk");
	/* This isn't the first chunk.  Check whether we're the last chunk. */

	// Delete this chunk
	table->del(orig_key);

	if (is_last_chunk) {
	    LOGLINE(DB, "PostlistChunkWriter::flush(): deleting secondary last chunk");
	    // Update the previous chunk's is_last_chunk flag.
	    AutoPtr<BrassCursor> cursor(table->cursor_get());

	    /* Should not find the key we just deleted, but should
	     * find the previous chunk. */
	    if (cursor->find_entry(orig_key)) {
		throw Xapian::DatabaseCorruptError("Brass key not deleted as we expected");
	    }
	    // Make sure this is a chunk with the right term attached.
	    const char * keypos = cursor->current_key.data();
	    const char * keyend = keypos + cursor->current_key.size();
	    if (!check_tname_in_key(&keypos, keyend, tname)) {
		throw Xapian::DatabaseCorruptError("Couldn't find chunk before delete chunk");
	    }

	    bool is_prev_first_chunk = (keypos == keyend);

	    // Now update the last_chunk
	    cursor->read_tag();
	    string tag = cursor->current_tag;

	    const char *tagpos = tag.data();
	    const char *tagend = tagpos + tag.size();

	    // Skip first chunk header
	    Xapian::docid first_did_in_chunk;
	    if (is_prev_first_chunk) {
		first_did_in_chunk = read_start_of_first_chunk(&tagpos, tagend,
							       0, 0);
	    } else {
		if (!unpack_uint_preserving_sort(&keypos, keyend, &first_did_in_chunk))
		    report_read_error(keypos);
	    }
	    bool wrong_is_last_chunk;
	    string::size_type start_of_chunk_header = tagpos - tag.data();
	    Xapian::docid last_did_in_chunk =
		read_start_of_chunk(&tagpos, tagend, first_did_in_chunk,
				    &wrong_is_last_chunk);
	    string::size_type end_of_chunk_header = tagpos - tag.data();

	    // write new is_last flag
	    write_start_of_chunk(tag,
				 start_of_chunk_header,
				 end_of_chunk_header,
				 true, // is_last_chunk
				 first_did_in_chunk,
				 last_did_in_chunk);
	    table->add(cursor->current_key, tag);
	}
    } else {
	LOGLINE(DB, "PostlistChunkWriter::flush(): updating chunk which still has items in it");
	/* The chunk still has some items in it.  Two major subcases:
	 * a) This is the first chunk.
	 * b) This isn't the first chunk.
	 *
	 * The subcases just affect the chunk header.
	 */
	string tag;

	/* First write the header, which depends on whether this is the
	 * first chunk.
	 */
	if (is_first_chunk) {
	    /* The first chunk.  This is the relatively easy case,
	     * and we just have to write this one back to disk.
	     */
	    LOGLINE(DB, "PostlistChunkWriter::flush(): rewriting the first chunk, which still has items in it");
	    string key = BrassPostListTable::make_key(tname);
	    bool ok = table->get_exact_entry(key, tag);
	    (void)ok;
	    Assert(ok);
	    Assert(!tag.empty());

	    Xapian::doccount num_ent;
	    Xapian::termcount coll_freq;
	    {
		const char * tagpos = tag.data();
		const char * tagend = tagpos + tag.size();
		(void)read_start_of_first_chunk(&tagpos, tagend,
						&num_ent, &coll_freq);
	    }

	    tag = make_start_of_first_chunk(num_ent, coll_freq, first_did);

	    tag += make_start_of_chunk(is_last_chunk, first_did, current_did);
	    tag += chunk;
	    table->add(key, tag);
	    return;
	}

	LOGLINE(DB, "PostlistChunkWriter::flush(): updating secondary chunk which still has items in it");
	/* Not the first chunk.
	 *
	 * This has the easy sub-sub-case:
	 *   The first entry in the chunk hasn't changed
	 * ...and the hard sub-sub-case:
	 *   The first entry in the chunk has changed.  This is
	 *   harder because the key for the chunk changes, so
	 *   we've got to do a switch.
	 */

	// First find out the initial docid
	const char *keypos = orig_key.data();
	const char *keyend = keypos + orig_key.size();
	if (!check_tname_in_key(&keypos, keyend, tname)) {
	    throw Xapian::DatabaseCorruptError("Have invalid key writing to postlist");
	}
	Xapian::docid initial_did;
	if (!unpack_uint_preserving_sort(&keypos, keyend, &initial_did)) {
	    report_read_error(keypos);
	}
	string new_key;
	if (initial_did != first_did) {
	    /* The fiddlier case:
	     * Create a new tag with the correct key, and replace
	     * the old one.
	     */
	    new_key = BrassPostListTable::make_key(tname, first_did);
	    table->del(orig_key);
	} else {
	    new_key = orig_key;
	}

	// ...and write the start of this chunk.
	tag = make_start_of_chunk(is_last_chunk, first_did, current_did);

	tag += chunk;
	table->add(new_key, tag);
    }
}

/** Read the number of entries in the posting list.
 *  This must only be called when *posptr is pointing to the start of
 *  the first chunk of the posting list.
 */
void BrassPostList::read_number_of_entries(const char ** posptr,
				   const char * end,
				   Xapian::doccount * number_of_entries_ptr,
				   Xapian::termcount * collection_freq_ptr)
{
    if (!unpack_uint(posptr, end, number_of_entries_ptr))
	report_read_error(*posptr);
    if (!unpack_uint(posptr, end, collection_freq_ptr))
	report_read_error(*posptr);
}

/** The format of a postlist is:
 *
 *  Split into chunks.  Key for first chunk is the termname (encoded as
 *  length : name).  Key for subsequent chunks is the same, followed by the
 *  document ID of the first document in the chunk (encoded as length of
 *  representation in first byte, and then docid).
 *
 *  A chunk (except for the first chunk) contains:
 *
 *  1)  bool - true if this is the last chunk.
 *  2)  difference between final docid in chunk and first docid.
 *  3)  wdf for the first item.
 *  4)  increment in docid to next item, followed by wdf for the item.
 *  5)  (4) repeatedly.
 *
 *  The first chunk begins with the number of entries, the collection
 *  frequency, then the docid of the first document, then has the header of a
 *  standard chunk.
 */
BrassPostList::BrassPostList(intrusive_ptr<const BrassDatabase> this_db_,
			     const string & term_,
			     bool keep_reference)
	: LeafPostList(term_),
	  this_db(keep_reference ? this_db_ : NULL),
	  have_started(false),
	  is_at_end(false),
	  cursor(this_db_->postlist_table.cursor_get())
{
    LOGCALL_CTOR(DB, "BrassPostList", this_db_.get() | term_ | keep_reference);
    init();
}

BrassPostList::BrassPostList(intrusive_ptr<const BrassDatabase> this_db_,
			     const string & term_,
			     BrassCursor * cursor_)
	: LeafPostList(term_),
	  this_db(this_db_),
	  have_started(false),
	  is_at_end(false),
	  cursor(cursor_)
{
    LOGCALL_CTOR(DB, "BrassPostList", this_db_.get() | term_ | cursor_);
    init();
}

void
BrassPostList::init()
{
    string key = BrassPostListTable::make_key(term);
    int found = cursor->find_entry(key);
    if (!found) {
	LOGLINE(DB, "postlist for term not found");
	number_of_entries = 0;
	is_at_end = true;
	pos = 0;
	end = 0;
	first_did_in_chunk = 0;
	last_did_in_chunk = 0;
	return;
    }
    cursor->read_tag();
    pos = cursor->current_tag.data();
    end = pos + cursor->current_tag.size();

    did = read_start_of_first_chunk(&pos, end, &number_of_entries, NULL);
    first_did_in_chunk = did;
    last_did_in_chunk = read_start_of_chunk(&pos, end, first_did_in_chunk,
					    &is_last_chunk);
    read_wdf(&pos, end, &wdf);
    LOGLINE(DB, "Initial docid " << did);
}

BrassPostList::~BrassPostList()
{
    LOGCALL_DTOR(DB, "BrassPostList");
}

LeafPostList *
BrassPostList::open_nearby_postlist(const std::string & term_) const
{
    LOGCALL(DB, LeafPostList *, "BrassPostList::open_nearby_postlist", term_);
    if (term_.empty())
	return NULL;
    if (!this_db.get() || this_db->postlist_table.is_writable())
	return NULL;
    return new BrassPostList(this_db, term_, cursor->clone());
}

Xapian::termcount
BrassPostList::get_doclength() const
{
    LOGCALL(DB, Xapian::termcount, "BrassPostList::get_doclength", NO_ARGS);
    Assert(have_started);
    Assert(this_db.get());
    RETURN(this_db->get_doclength(did));
}

bool
BrassPostList::next_in_chunk()
{
    LOGCALL(DB, bool, "BrassPostList::next_in_chunk", NO_ARGS);
    if (pos == end) RETURN(false);

    read_did_increase(&pos, end, &did);
    read_wdf(&pos, end, &wdf);

    // Either not at last doc in chunk, or pos == end, but not both.
    Assert(did <= last_did_in_chunk);
    Assert(did < last_did_in_chunk || pos == end);
    Assert(pos != end || did == last_did_in_chunk);

    RETURN(true);
}

void
BrassPostList::next_chunk()
{
    LOGCALL_VOID(DB, "BrassPostList::next_chunk", NO_ARGS);
    if (is_last_chunk) {
	is_at_end = true;
	return;
    }

    cursor->next();
    if (cursor->after_end()) {
	is_at_end = true;
	throw Xapian::DatabaseCorruptError("Unexpected end of posting list for '" +
				     term + "'");
    }
    const char * keypos = cursor->current_key.data();
    const char * keyend = keypos + cursor->current_key.size();
    // Check we're still in same postlist
    if (!check_tname_in_key_lite(&keypos, keyend, term)) {
	is_at_end = true;
	throw Xapian::DatabaseCorruptError("Unexpected end of posting list for '" +
				     term + "'");
    }

    Xapian::docid newdid;
    if (!unpack_uint_preserving_sort(&keypos, keyend, &newdid)) {
	report_read_error(keypos);
    }
    if (newdid <= did) {
	throw Xapian::DatabaseCorruptError("Document ID in new chunk of postlist (" +
		str(newdid) +
		") is not greater than final document ID in previous chunk (" +
		str(did) + ")");
    }
    did = newdid;

    cursor->read_tag();
    pos = cursor->current_tag.data();
    end = pos + cursor->current_tag.size();

    first_did_in_chunk = did;
    last_did_in_chunk = read_start_of_chunk(&pos, end, first_did_in_chunk,
					    &is_last_chunk);
    read_wdf(&pos, end, &wdf);
}

PositionList *
BrassPostList::read_position_list()
{
    LOGCALL(DB, PositionList *, "BrassPostList::read_position_list", NO_ARGS);
    Assert(this_db.get());
    positionlist.read_data(&this_db->position_table, did, term);
    RETURN(&positionlist);
}

PositionList *
BrassPostList::open_position_list() const
{
    LOGCALL(DB, PositionList *, "BrassPostList::open_position_list", NO_ARGS);
    Assert(this_db.get());
    RETURN(new BrassPositionList(&this_db->position_table, did, term));
}

PostList *
BrassPostList::next(double w_min)
{
    LOGCALL(DB, PostList *, "BrassPostList::next", w_min);
    (void)w_min; // no warning

    if (!have_started) {
	have_started = true;
    } else {
	if (!next_in_chunk()) next_chunk();
    }

    if (is_at_end) {
	LOGLINE(DB, "Moved to end");
    } else {
	LOGLINE(DB, "Moved to docid " << did << ", wdf = " << wdf);
    }

    RETURN(NULL);
}

bool
BrassPostList::current_chunk_contains(Xapian::docid desired_did)
{
    LOGCALL(DB, bool, "BrassPostList::current_chunk_contains", desired_did);
    if (desired_did >= first_did_in_chunk &&
	desired_did <= last_did_in_chunk) {
	RETURN(true);
    }
    RETURN(false);
}

void
BrassPostList::move_to_chunk_containing(Xapian::docid desired_did)
{
    LOGCALL_VOID(DB, "BrassPostList::move_to_chunk_containing", desired_did);
    (void)cursor->find_entry(BrassPostListTable::make_key(term, desired_did));
    Assert(!cursor->after_end());

    const char * keypos = cursor->current_key.data();
    const char * keyend = keypos + cursor->current_key.size();
    // Check we're still in same postlist
    if (!check_tname_in_key_lite(&keypos, keyend, term)) {
	// This should only happen if the postlist doesn't exist at all.
	is_at_end = true;
	is_last_chunk = true;
	return;
    }
    is_at_end = false;

    cursor->read_tag();
    pos = cursor->current_tag.data();
    end = pos + cursor->current_tag.size();

    if (keypos == keyend) {
	// In first chunk
#ifdef XAPIAN_ASSERTIONS
	Xapian::doccount old_number_of_entries = number_of_entries;
	did = read_start_of_first_chunk(&pos, end, &number_of_entries, NULL);
	Assert(old_number_of_entries == number_of_entries);
#else
	did = read_start_of_first_chunk(&pos, end, NULL, NULL);
#endif
    } else {
	// In normal chunk
	if (!unpack_uint_preserving_sort(&keypos, keyend, &did)) {
	    report_read_error(keypos);
	}
    }

    first_did_in_chunk = did;
    last_did_in_chunk = read_start_of_chunk(&pos, end, first_did_in_chunk,
					    &is_last_chunk);
    read_wdf(&pos, end, &wdf);

    // Possible, since desired_did might be after end of this chunk and before
    // the next.
    if (desired_did > last_did_in_chunk) next_chunk();
}

bool
BrassPostList::move_forward_in_chunk_to_at_least(Xapian::docid desired_did)
{
    LOGCALL(DB, bool, "BrassPostList::move_forward_in_chunk_to_at_least", desired_did);
    if (did >= desired_did)
	RETURN(true);

    if (desired_did <= last_did_in_chunk) {
	while (pos != end) {
	    read_did_increase(&pos, end, &did);
	    if (did >= desired_did) {
		read_wdf(&pos, end, &wdf);
		RETURN(true);
	    }
	    // It's faster to just skip over the wdf than to decode it.
	    read_wdf(&pos, end, NULL);
	}

	// If we hit the end of the chunk then last_did_in_chunk must be wrong.
	Assert(false);
    }

    pos = end;
    RETURN(false);
}

PostList *
BrassPostList::skip_to(Xapian::docid desired_did, double w_min)
{
    LOGCALL(DB, PostList *, "BrassPostList::skip_to", desired_did | w_min);
    (void)w_min; // no warning
    // We've started now - if we hadn't already, we're already positioned
    // at start so there's no need to actually do anything.
    have_started = true;

    // Don't skip back, and don't need to do anything if already there.
    if (is_at_end || desired_did <= did) RETURN(NULL);

    // Move to correct chunk
    if (!current_chunk_contains(desired_did)) {
	move_to_chunk_containing(desired_did);
	// Might be at_end now, so we need to check before trying to move
	// forward in chunk.
	if (is_at_end) RETURN(NULL);
    }

    // Move to correct position in chunk
    bool have_document = move_forward_in_chunk_to_at_least(desired_did);
    (void)have_document;
    Assert(have_document);

    if (is_at_end) {
	LOGLINE(DB, "Skipped to end");
    } else {
	LOGLINE(DB, "Skipped to docid " << did << ", wdf = " << wdf);
    }

    RETURN(NULL);
}

// Used for doclens.
bool
BrassPostList::jump_to(Xapian::docid desired_did)
{
    LOGCALL(DB, bool, "BrassPostList::jump_to", desired_did);
    // We've started now - if we hadn't already, we're already positioned
    // at start so there's no need to actually do anything.
    have_started = true;

    // If the list is empty, give up right away.
    if (pos == 0) RETURN(false);

    // Move to correct chunk, or reload the current chunk to go backwards in it
    // (FIXME: perhaps handle the latter case more elegantly, though it won't
    // happen during sequential access which is most common).
    if (is_at_end || !current_chunk_contains(desired_did) || desired_did < did) {
	// Clear is_at_end flag since we can rewind.
	is_at_end = false;

	move_to_chunk_containing(desired_did);
	// Might be at_end now, so we need to check before trying to move
	// forward in chunk.
	if (is_at_end) RETURN(false);
    }

    // Move to correct position in chunk.
    if (!move_forward_in_chunk_to_at_least(desired_did)) RETURN(false);
    RETURN(desired_did == did);
}

string
BrassPostList::get_description() const
{
    string desc;
    description_append(desc, term);
    desc += ":";
    desc += str(number_of_entries);
    return desc;
}


// Returns the last did to allow in this chunk.
Xapian::docid
BrassPostListTable::get_chunk(const string &tname,
	  Xapian::docid did, bool adding,
	  PostlistChunkReader ** from, PostlistChunkWriter **to)
{
    LOGCALL(DB, Xapian::docid, "BrassPostListTable::get_chunk", tname | did | adding | from | to);

	if ( tname.empty() )
	{
		*from = NULL;
		*to = NULL;
		RETURN(Xapian::docid(-1));
	}

    // Get chunk containing entry
    string key = make_key(tname, did);

    // Find the right chunk
    AutoPtr<BrassCursor> cursor(cursor_get());

    (void)cursor->find_entry(key);
    Assert(!cursor->after_end());

    const char * keypos = cursor->current_key.data();
    const char * keyend = keypos + cursor->current_key.size();

    if (!check_tname_in_key(&keypos, keyend, tname)) {
	// Postlist for this termname doesn't exist.
	//
	// NB "adding" will only be true if we are adding, but it may sometimes
	// be false in some cases where we are actually adding.
	if (!adding)
	    throw Xapian::DatabaseCorruptError("Attempted to delete or modify an entry in a non-existent posting list for " + tname);

	*from = NULL;
	*to = new PostlistChunkWriter(string(), true, tname, true);
	RETURN(Xapian::docid(-1));
    }

    // See if we're appending - if so we can shortcut by just copying
    // the data part of the chunk wholesale.
    bool is_first_chunk = (keypos == keyend);
    LOGVALUE(DB, is_first_chunk);

    cursor->read_tag();
    const char * pos = cursor->current_tag.data();
    const char * end = pos + cursor->current_tag.size();
    Xapian::docid first_did_in_chunk;
    if (is_first_chunk) {
	first_did_in_chunk = read_start_of_first_chunk(&pos, end, NULL, NULL);
    } else {
	if (!unpack_uint_preserving_sort(&keypos, keyend, &first_did_in_chunk)) {
	    report_read_error(keypos);
	}
    }

    bool is_last_chunk;
    Xapian::docid last_did_in_chunk;
    last_did_in_chunk = read_start_of_chunk(&pos, end, first_did_in_chunk, &is_last_chunk);
    *to = new PostlistChunkWriter(cursor->current_key, is_first_chunk, tname,
				  is_last_chunk);
    if (did > last_did_in_chunk) {
	// This is the shortcut.  Not very pretty, but I'll leave refactoring
	// until I've a clearer picture of everything which needs to be done.
	// (FIXME)
	*from = NULL;
	(*to)->raw_append(first_did_in_chunk, last_did_in_chunk,
			  string(pos, end));
    } else {
	*from = new PostlistChunkReader(first_did_in_chunk, string(pos, end));
    }
    if (is_last_chunk) RETURN(Xapian::docid(-1));

    // Find first did of next tag.
    cursor->next();
    if (cursor->after_end()) {
	throw Xapian::DatabaseCorruptError("Expected another key but found none");
    }
    const char *kpos = cursor->current_key.data();
    const char *kend = kpos + cursor->current_key.size();
    if (!check_tname_in_key(&kpos, kend, tname)) {
	throw Xapian::DatabaseCorruptError("Expected another key with the same term name but found a different one");
    }

    // Read the new first docid
    Xapian::docid first_did_of_next_chunk;
    if (!unpack_uint_preserving_sort(&kpos, kend, &first_did_of_next_chunk)) {
	report_read_error(kpos);
    }
    RETURN(first_did_of_next_chunk - 1);
}

void
BrassPostListTable::merge_doclen_changes(const map<Xapian::docid, Xapian::termcount> & doclens)
{
    LOGCALL_VOID(DB, "BrassPostListTable::merge_doclen_changes", doclens);

    // The cursor in the doclen_pl will no longer be valid, so reset it.
    doclen_pl.reset(0);

    LOGVALUE(DB, doclens.size());
    if (doclens.empty()) return;

    // Ensure there's a first chunk.
    string current_key = make_key(string());

    if (!key_exists(current_key)) {
	LOGLINE(DB, "Adding dummy first chunk");
	string newtag = make_start_of_first_chunk(0, 0, 0);
	newtag += make_start_of_chunk(true, 0, 0);
	add(current_key, newtag);
    }

	map<Xapian::docid, Xapian::termcount>::const_iterator it, pre_it;
	pre_it = it = doclens.begin();
	Assert(it != doclens.end());

	while ( it!=doclens.end() )
	{
		string key = make_key(string(), it->first);

		AutoPtr<BrassCursor> cursor(cursor_get());

		(void)cursor->find_entry(key);
		Assert(!cursor->after_end());

		const char * keypos = cursor->current_key.data();
		const char * keyend = keypos + cursor->current_key.size();

		check_tname_in_key(&keypos, keyend, string());

		bool is_first_chunk = (keypos == keyend);
		LOGVALUE(DB, is_first_chunk);

		cursor->read_tag();
		string desired_chunk(cursor->current_tag);

		const char * pos = cursor->current_tag.data();
		const char * end = pos + cursor->current_tag.size();
		Xapian::docid first_did_in_chunk;
		if (is_first_chunk) {
			first_did_in_chunk = read_start_of_first_chunk(&pos, end, NULL, NULL);
		} else {
			if (!unpack_uint_preserving_sort(&keypos, keyend, &first_did_in_chunk)) {
				report_read_error(keypos);
			}
		}

		bool is_last_chunk;
		Xapian::docid last_did_in_chunk;
		last_did_in_chunk = read_start_of_chunk(&pos, end, first_did_in_chunk, &is_last_chunk);

		while ( it!=doclens.end() && it->first<=last_did_in_chunk )
		{
			++it;
		}

		cursor->del();
		DoclenChunkWriter writer(desired_chunk
			map<Xapian::docid,Xapian::termcount>(pre_it,it),
			this, is_first_chunk);
		writer.merge_doclen_changes();
		pre_it = it;
	}
}

void
BrassPostListTable::merge_changes(const string &term,
				  const Inverter::PostingChanges & changes)
{
    {
	// Rewrite the first chunk of this posting list with the updated
	// termfreq and collfreq.
	string current_key = make_key(term);
	string tag;
	(void)get_exact_entry(current_key, tag);

	// Read start of first chunk to get termfreq and collfreq.
	const char *pos = tag.data();
	const char *end = pos + tag.size();
	Xapian::doccount termfreq;
	Xapian::termcount collfreq;
	Xapian::docid firstdid, lastdid;
	bool islast;
	if (pos == end) {
	    termfreq = 0;
	    collfreq = 0;
	    firstdid = 0;
	    lastdid = 0;
	    islast = true;
	} else {
	    firstdid = read_start_of_first_chunk(&pos, end,
						 &termfreq, &collfreq);
	    // Handle the generic start of chunk header.
	    lastdid = read_start_of_chunk(&pos, end, firstdid, &islast);
	}

	termfreq += changes.get_tfdelta();
	if (termfreq == 0) {
	    // All postings deleted!  So we can shortcut by zapping the
	    // posting list.
	    if (islast) {
		// Only one entry for this posting list.
		del(current_key);
		return;
	    }
	    MutableBrassCursor cursor(this);
	    bool found = cursor.find_entry(current_key);
	    Assert(found);
	    if (!found) return; // Reduce damage!
	    while (cursor.del()) {
		const char *kpos = cursor.current_key.data();
		const char *kend = kpos + cursor.current_key.size();
		if (!check_tname_in_key_lite(&kpos, kend, term)) break;
	    }
	    return;
	}
	collfreq += changes.get_cfdelta();

	// Rewrite start of first chunk to update termfreq and collfreq.
	string newhdr = make_start_of_first_chunk(termfreq, collfreq, firstdid);
	newhdr += make_start_of_chunk(islast, firstdid, lastdid);
	if (pos == end) {
	    add(current_key, newhdr);
	} else {
	    Assert((size_t)(pos - tag.data()) <= tag.size());
	    tag.replace(0, pos - tag.data(), newhdr);
	    add(current_key, tag);
	}
    }
    map<Xapian::docid, Xapian::termcount>::const_iterator j;
    j = changes.pl_changes.begin();
    Assert(j != changes.pl_changes.end()); // This case is caught above.

    Xapian::docid max_did;
    PostlistChunkReader *from;
    PostlistChunkWriter *to;
    max_did = get_chunk(term, j->first, false, &from, &to);
    for ( ; j != changes.pl_changes.end(); ++j) {
	Xapian::docid did = j->first;

next_chunk:
	LOGLINE(DB, "Updating term=" << term << ", did=" << did);
	if (from) while (!from->is_at_end()) {
	    Xapian::docid copy_did = from->get_docid();
	    if (copy_did >= did) {
		if (copy_did == did) {
		    from->next();
		}
		break;
	    }
	    to->append(this, copy_did, from->get_wdf());
	    from->next();
	}
	if ((!from || from->is_at_end()) && did > max_did) {
	    delete from;
	    to->flush(this);
	    delete to;
	    max_did = get_chunk(term, did, false, &from, &to);
	    goto next_chunk;
	}

	Xapian::termcount new_wdf = j->second;
	if (new_wdf != Xapian::termcount(-1)) {
	    to->append(this, did, new_wdf);
	}
    }

    if (from) {
	while (!from->is_at_end()) {
	    to->append(this, from->get_docid(), from->get_wdf());
	    from->next();
	}
	delete from;
    }
    to->flush(this);
    delete to;
}