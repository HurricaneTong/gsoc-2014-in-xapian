#pragma once
#include "vector"
#include "map"
#include "string"
#define SEPERATOR ((unsigned)-1)

using std::vector;
using std::map;
using std::string;
typedef unsigned int docid;
typedef unsigned int doclength;
typedef unsigned int termcount;
typedef map<docid,string> BTree;

class SkipList
{
private:
	vector<unsigned> src;
	void genDiffVector( const map<docid,doclength>& postlist );
	int encodeLength( unsigned n );
	void addLevel ( int ps, int &pe, int& pinfo1_, int& pinfo2_, int curLevel );
    int cal_level(unsigned size);
    void buildSkipList( int level );
public:
	SkipList( const map<docid,doclength>& postlist );
	void encode( string& chunk ) const;
};

class SkipListReader {
    const char* ori_pos;
	const char* pos;
	const char* end;
    bool at_end;
    docid did;
    docid first_did;
    termcount wdf;
public:
    bool jump_to(docid desired_did);
    docid get_did() const {
        return did;
    }
    termcount get_wdf() const {
        return wdf;
    }
    bool is_at_end() const {
        return at_end;
    }
    bool next();
	SkipListReader(const char* pos_, const char* end_, docid first_did_);
};

class SkipListWriter
{
private:
	const char* start;
	const char* end;
    docid first_did;
    BTree* bt;
public:
	SkipListWriter(const char* start_, const char* end_ , docid first_did_, BTree* bt_);
	bool merge_doclen_change(map<docid,termcount>::const_iterator start_, map<docid,termcount>::const_iterator end_);
};
