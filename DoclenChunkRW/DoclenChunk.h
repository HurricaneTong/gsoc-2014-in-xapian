#include "FixedWidthChunk.h"
#include "pack.h"
#include "string"
#include "map"
#include "vector"
#include "iostream"

using std::string;
using std::map;
using std::vector;

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
	FixedWidthChunkReader fwcr;
public:
	DoclenChunkReader( const string& chunk_ )
		: chunk(chunk_), fwcr(chunk)
	{
	}
	docid get_doclen( docid desired_did );
};