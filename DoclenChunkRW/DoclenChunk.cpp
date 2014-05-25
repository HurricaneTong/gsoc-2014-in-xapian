#include "DoclenChunk.h"

bool DoclenChunkWriter::merge_doclen_changes( const map<docid,doclen>& changes )
{
	if ( chunk.empty() )
	{
		FixedWidthChunk fwc( changes );
		fwc.encode(chunk);				
	}
	else
	{
		FixedWidthChunkWriter fwcw( chunk );
		fwcw.merge_doclen_change(changes);
	}
	return true;
}

docid DoclenChunkReader::get_doclen( docid desired_did )
{
	return fwcr.getDoclen(desired_did);
}