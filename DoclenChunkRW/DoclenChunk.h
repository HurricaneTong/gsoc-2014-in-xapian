#include "FixedWidthChunk.h"
#include "pack.h"
#include "string"
#include "map"
#include "vector"

using std::string;
using std::map;
using std::vector;

class DoclenChunkReader;
class DoclenChunkWriter;

typedef pair<string,string> b_tree_node;
typedef vector<b_tree_node> b_tree;

class DoclenChunkWriter
{
private:
	string& chunk;
	b_tree bt;
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