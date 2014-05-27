#include "FixedWidthChunk.h"

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

FixedWidthChunk::FixedWidthChunk( const map<docid,doclen>& postlist )
{
	buildVector( postlist );
}

bool FixedWidthChunk::buildVector( const map<docid,doclen>& postlist )
{
	map<docid,doclen>::const_iterator it = postlist.begin(), start_pos;
	bias = it->first;
	docid docid_before_start_pos = it->first;

	int chunk_size = 0;

	while ( it!=postlist.end() )
	{
		unsigned length_contiguous = 1;
		docid last_docid = it->first, cur_docid = 0;
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

doclen FixedWidthChunkReader::getDoclen( docid desired_did )
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


	docid incre_did = 0;

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
			docid old_cur_did = cur_did;
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

//bool FixedWidthChunkWriter::merge_doclen_change( const map<docid,doclen>& changes )
//{
//	map<docid,doclen>::const_iterator it = changes.begin();
//	const char* pos = chunk.data();
//	const char* end = pos+chunk.size();
//	map<docid,doclen> original_postlist;
//	docid bias = 0, cur_did = 0, inc_did = 0;
//	doclen doc_len = 0;
//	unpack_uint( &pos, end, &bias );
//	cur_did = bias;
//	while ( pos!=end )
//	{
//		unpack_uint( &pos, end, &inc_did );
//		if ( inc_did != SEPERATOR )
//		{
//			cur_did += inc_did;
//			unpack_uint( &pos, end, &doc_len );
//			original_postlist[cur_did] = doc_len;
//			continue;
//		}
//		else
//		{
//			unpack_uint( &pos, end, &inc_did );
//			unsigned len=0, bytes=0;
//			unpack_uint_in_bytes( &pos, 2, &len );
//			unpack_uint_in_bytes( &pos, 1, &bytes );
//			cur_did += inc_did;
//			while ( len-- )
//			{
//				unpack_uint_in_bytes( &pos, bytes, &doc_len );
//				original_postlist[cur_did] = doc_len;
//				cur_did++;
//			}
//			cur_did--;
//		}
//
//	}
//
//	map<docid,doclen>::const_iterator chg_it = changes.begin();
//	map<docid,doclen>::iterator ori_it = original_postlist.begin();
//
//	while ( chg_it != changes.end() )
//	{
//		while ( chg_it->first > ori_it->first )
//		{
//			++ori_it;
//			if ( ori_it == original_postlist.end() )
//			{
//				break;
//			}
//		}
//		if ( ori_it == original_postlist.end() )
//		{
//			original_postlist.insert( ori_it, *chg_it );
//			++chg_it;
//			while ( chg_it != changes.end() )
//			{
//				original_postlist.insert( ori_it, *chg_it );
//				++chg_it;
//			}
//			break;
//		}
//		if ( ori_it->first == chg_it->first )
//		{
//			if ( chg_it->second != SEPERATOR )
//			{
//				ori_it->second = chg_it->second;
//			}
//			else
//			{
//				ori_it = original_postlist.erase( ori_it );
//			}
//		}
//		else
//		{
//			original_postlist.insert( ori_it, *chg_it );
//		}
//		++chg_it;
//	}
//
//	chunk.clear();
//	FixedWidthChunk fwc( original_postlist );
//	fwc.encode( chunk );
//	return true;
//}


