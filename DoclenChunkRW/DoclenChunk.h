#include "FixedWidthChunk.h"
#include "SkipList.h"
#include "pack.h"
#include "string"
#include "map"

using std::string;
using std::map;

class DoclenChunkReader;
class DoclenChunkWriter;

class DoclenChunkWriter
{
private:
	string& chunk;
public:
	DoclenChunkWriter( string& chunk_ )
		: chunk(chunk_)
	{

	}
	bool merge_doclen_changes( const map<docid,doclen>& changes );
};

class DoclenChunkReader
{
private:
	string chunk;
	unsigned format_info;
public:
	DoclenChunkReader( const string& chunk_ )
		: chunk(chunk_)
	{
		const char* pos = chunk.data();
		const char* end = pos+chunk.size();
		read_chunk_header( &pos,end,&format_info );
	}
	docid get_doclen( docid desired_did );
};