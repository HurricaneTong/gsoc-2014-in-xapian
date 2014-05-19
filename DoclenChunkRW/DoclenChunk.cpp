#include "DoclenChunk.h"

bool DoclenChunkWriter::merge_doclen_changes( const map<docid,doclen>& changes )
{
	if ( chunk.empty() )
	{
		if ( changes.size() < DOCLEN_CHUNK_MIN_SKIPLIST_LENGTH )
		{
			FixedWidthChunk fwc( changes );
			fwc.encode(chunk);
		}
		else
		{
			int contiguous_length = 0;
			int contiguous_blocks = 0;
			map<docid,doclen>::const_iterator it = changes.begin();
			docid cur_did, pre_did;
			cur_did = it->first;
			pre_did = cur_did;
			while ( it!=changes.end() )
			{
				cur_did = it->first;
				if ( cur_did==pre_did+1 )
				{
					contiguous_length++;
				}
				else
				{
					if ( contiguous_length > DOCLEN_CHUNK_MIN_CONTIGUOUS_LENGTH )
					{
						contiguous_blocks++;
					}
					contiguous_length = 0;
				}
				++it;
				pre_did = cur_did;
			}
			if ( contiguous_length > DOCLEN_CHUNK_MIN_CONTIGUOUS_LENGTH )
			{
				contiguous_blocks++;
			}
			if ( contiguous_blocks > DOCLEN_CHUNK_MIN_CONTIGUOUS_BLOCKS )
			{
				FixedWidthChunk fwc( changes );
				fwc.encode(chunk);				
			}
			else
			{
				SkipList sl( changes );
				sl.encode(chunk);
			}
		}
	}
	else
	{
		unsigned format_info = 0;
		const char* pos = chunk.data();
		const char* end = pos+chunk.size();
		unpack_uint( &pos, end, &format_info );
		if ( format_info == DOCLEN_CHUNK_SKIPLIST )
		{
			SkipListWriter slw( chunk );
			slw.merge_doclen_change(changes);
		}
		else if ( format_info == DOCLEN_CHUNK_FIXEDWIDTH )
		{
			FixedWidthChunkWriter fwcw( chunk );
			fwcw.merge_doclen_change(changes);
		}
	}
	return true;
}

docid DoclenChunkReader::get_doclen( docid desired_did )
{
	if ( format_info == DOCLEN_CHUNK_FIXEDWIDTH )
	{
		FixedWidthChunkReader fwcr(chunk);
		return fwcr.getDoclen(desired_did);
	}
	if ( format_info == DOCLEN_CHUNK_SKIPLIST )
	{
		SkipListReader slr(chunk);
		doclen len = 0;
		slr.getDoclen( desired_did,&len );
		return len;
	}
	return -1;
}