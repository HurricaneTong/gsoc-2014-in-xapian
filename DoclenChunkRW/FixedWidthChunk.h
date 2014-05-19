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
	string chunk;
public:
	FixedWidthChunkReader( const string& chunk_ )
		: chunk(chunk_)
	{};
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