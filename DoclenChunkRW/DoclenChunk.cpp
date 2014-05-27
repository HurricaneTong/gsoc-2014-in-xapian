#include "DoclenChunk.h"

void read_number_of_entries(const char ** posptr,
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


static string
	make_start_of_first_chunk(doccount entries,
	termcount collectionfreq,
	docid new_did)
{
	string chunk;
	pack_uint(chunk, entries);
	pack_uint(chunk, collectionfreq);
	pack_uint(chunk, new_did);
	return chunk;
}

static inline string
	make_start_of_chunk(bool new_is_last_chunk,
	docid new_first_did,
	docid new_final_did)
{
	string chunk;
	pack_bool(chunk, new_is_last_chunk);
	pack_uint(chunk, new_final_did - new_first_did);
	return chunk;
}

bool DoclenChunkWriter::get_new_doclen( const map<docid,doclen>*& p_new_doclen )
{
	if ( chunk_from.empty() )
	{
		p_new_doclen = &changes;
	}
	else
	{
		map<docid,doclen>::const_iterator it = changes.begin();
		const char* pos = chunk_from.data();
		const char* end = pos+chunk_from.size();

		if ( is_first_chunk )
		{
			read_start_of_first_chunk( &pos, end, NULL, NULL );
		}
		read_start_of_chunk( &pos, end, 0, &is_last_chunk ); 

		docid bias = 0, cur_did = 0, inc_did = 0;
		doclen doc_len = 0;
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

		map<docid,doclen>::const_iterator chg_it = changes.begin();
		map<docid,doclen>::iterator ori_it = new_doclen.begin();

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
	const map<docid,doclen>* p_new_doclen = NULL;
	get_new_doclen( p_new_doclen );

	map<docid,doclen>::const_iterator start_pos, end_pos;
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
		b_tree->insert( make_pair(cur_key,cur_chunk) ); 
	}
	else
	{
		vector< map<docid,doclen> > doc_len_list;
		int count = 0;
		while ( end_pos!=p_new_doclen->end() )
		{
			end_pos++;
			count++;
			if ( count == MAX_ENTRIES_IN_CHUNK )
			{
				doc_len_list.push_back( map<docid,doclen>(start_pos,end_pos) );
				count = 0;
				start_pos = end_pos;
			}
		}
		if ( start_pos != end_pos )
		{
			doc_len_list.push_back( map<docid,doclen>(start_pos,end_pos) );
		}

		for ( int i=0 ; i<(int)doc_len_list.size() ; ++i )
		{
			string cur_chunk;
			map<docid,doclen>::iterator it = doc_len_list[i].end();
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
					make_start_of_first_chunk( p_new_doclen->size(), 0, it->first );
				cur_chunk = head_of_first_chunk+cur_chunk;
			}

			string cur_key = make_key( string(), doc_len_list[i].begin()->first );
			b_tree->insert( make_pair(cur_key,cur_chunk) );

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

docid DoclenChunkReader::get_doclen( docid desired_did )
{
	return p_fwcr->getDoclen(desired_did);
}