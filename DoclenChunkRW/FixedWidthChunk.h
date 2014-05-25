#pragma once
#include "string"
#include "vector"
#include "map"
#include "pack.h"


using std::string;
using std::vector;
using std::map;

class FixedWidthChunk
{
private:
	vector<unsigned> src;
	unsigned bias;
	bool buildVector( const map<docid,doclen>& postlist );
public:
	FixedWidthChunk( const map<docid,doclen>& postlist );
	bool encode( string& chunk ) const;
};

class FixedWidthChunkReader
{
private:
	const string& chunk;
	const char* pos;
	const char* end;
	docid cur_did;
	doclen doc_length;

public:
	FixedWidthChunkReader( const string& chunk_ )
		: chunk(chunk_)
	{
		pos = chunk.data();
		end = pos+chunk.size();
		cur_did = 0;
		doc_length = -1;
		unpack_uint( &pos, end, &cur_did );
	};
	doclen getDoclen( docid desired_did );
};

class FixedWidthChunkWriter
{
private:
	string& chunk;
public:
	FixedWidthChunkWriter( string& chunk_ )
		: chunk(chunk_)
	{};
	bool merge_doclen_change( const map<docid,doclen>& changes );
};