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
	//const string& chunk;
	const char* start_pos;
	const char* pos;
	const char* end;
	docid cur_did;
	doclen doc_length;

public:
	FixedWidthChunkReader( const char* pos_, const char* end_ )
		: start_pos(pos_), end(end_)
	{
		pos = start_pos;
		cur_did = 0;
		doc_length = -1;
		unpack_uint( &pos, end, &cur_did );
	};
	doclen getDoclen( docid desired_did );
};

//class FixedWidthChunkWriter
//{
//private:
//	string& chunk;
//public:
//	FixedWidthChunkWriter( string& chunk_ )
//		: chunk(chunk_)
//	{};
//	bool merge_doclen_change( const map<docid,doclen>& changes );
//};