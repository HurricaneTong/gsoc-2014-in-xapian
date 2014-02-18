#include "StemLancaster.h"

StemLancasterRule StemLancaster::stem( const std::string& word )
{
	bool isintact = true;
	int result;
	StemLancasterRule rule;
	rule.ruleID = 0;

	/* If s is already an invalid stem before we even start.. */
	if ( !is_acceptable(word) )
	{
		rule.text = word;
		return rule;  /* ..then just return an unaltered s.. */
	}

	std::string tx = word, trail = word ;

	/* While there is still stemming to be done.. */
	while ( (result=rule_walk(tx,isintact,rule)) != STEMLAN_STOP )
	{
		isintact = false;	/* Because word is no longer intact */
		if( !is_acceptable(tx) )
		{
			/* Exit from loop if not acceptable stem */
			break;
		}
		/* Set up trail for next iteration */
		trail = tx;
	}
	rule.text = ( result == STEMLAN_STOP ? tx : trail );
	return rule;
}

StemLancaster::StemLancaster()
{
	raw_rules = "ia,?,intact;a,?,intact;bb,b,stop;ytic,ys,stop;ic,?,continue;nc,nt,continue;dd,d,stop;ied,y,continue;ceed,cess,stop;eed,ee,stop;ed,?,continue;hood,?,continue;e,?,continue;lief,liev,stop;if,?,continue;ing,?,continue;iag,y,stop;ag,?,continue;gg,g,stop;th,?,intact;guish,ct,stop;ish,?,continue;i,?,intact;i,y,continue;ij,id,stop;fuj,fus,stop;uj,ud,stop;oj,od,stop;hej,her,stop;verj,vert,stop;misj,mit,stop;nj,nd,stop;j,s,stop;ifiabl,?,stop;iabl,y,stop;abl,?,continue;ibl,?,stop;bil,bl,continue;cl,c,stop;iful,y,stop;ful,?,continue;ul,?,stop;ial,?,continue;ual,?,continue;al,?,continue;ll,l,stop;ium,?,stop;um,?,intact;ism,?,continue;mm,m,stop;sion,j,continue;xion,ct,stop;ion,?,continue;ian,?,continue;an,?,continue;een,?,protect;en,?,continue;nn,n,stop;ship,?,continue;pp,p,stop;er,?,continue;ear,?,protect;ar,?,stop;or,?,continue;ur,?,continue;rr,r,stop;tr,t,continue;ier,y,continue;ies,y,continue;sis,s,stop;is,?,continue;ness,?,continue;ss,?,protect;ous,?,continue;us,?,intact;s,?,contint;s,?,stop;plicat,ply,stop;at,?,continue;ment,?,continue;ent,?,continue;ant,?,continue;ript,rib,stop;orpt,orb,stop;duct,duc,stop;sumpt,sum,stop;cept,ceiv,stop;olut,olv,stop;sist,?,protect;ist,?,continue;tt,t,stop;iqu,?,stop;ogu,og,stop;siv,j,continue;eiv,?,protect;iv,?,continue;bly,bl,continue;ily,y,continue;ply,?,protect;ly,?,continue;ogy,og,stop;phy,ph,stop;omy,om,stop;opy,op,stop;ity,?,continue;ety,?,continue;lty,l,stop;istry,?,stop;ary,?,continue;ory,?,continue;ify,?,stop;ncy,nt,continue;acy,?,continue;iz,?,continue;yz,ys,stop;";
	read_raw_rules();
}


/* NB - Treats 'y' as a vowel for the stemming purposes! */
bool StemLancaster::is_vowel( const char& c )
{
	if ( c == 'a' 
		|| c == 'e'
		|| c == 'i'
		|| c == 'o' 
		|| c == 'u'
		|| c == 'y')
	{
		return true;
	}
	return false;
}

bool StemLancaster::is_consonant( const char& c )
{
	return islower(c) && !is_vowel(c);
}


/* Acceptability condition:if the stem begins with a vowel,then it */
/* must contain at least 2 letters,one of which must be a consonant*/
/* If,however,it begins with a vowel then it must contain three */
/* letters and at least one of these must be a vowel or 'y',ok? */
bool StemLancaster::is_acceptable( const std::string& word )
{
	/* If longer than 3 chars then don't worry */
	if ( word.size() > 3 )
	{
		return true;
	}

	/* If first is a vowel,then second must be a consonant */
	if ( is_vowel(word[0]) )
	{
		if ( word.size() < 2 )
		{
			return false;
		}
		return is_consonant(word[1]);
	}


	/* If first is a consonant,then second or third must be */
	/* a vowel and length must be >3 */
	if ( word.size() < 3 )
	{
		return false;
	}
	return is_vowel(word[1]) || is_vowel(word[2]) && word.size()>3 ;

}
bool StemLancaster::is_valid_str( const std::string& str )
{
	for ( int i = 0 ; i < (int)str.size() ; i++ )
	{
		/* If it's not lower case or an apostrophe.. */
		if ( !islower(str[i]) && str[i] != '\'' )
		{
			return false;
		}
	}
	return true;
}

int StemLancaster::rule_walk( std::string& word, int is_intact, StemLancasterRule& used_rule )
{
	int x = get_table_index( word );
	std::list<StemLancasterRule>::iterator it;
	for ( it = rule_table[x].begin() ; it != rule_table[x].end() ; it++ )
	{
		std::string tmpstr(word);
		int result = 0;
		if ( (result=apply_rule(*it,tmpstr,is_intact)) != STEMLAN_NOT_APPLY )
		{
			word = tmpstr;
			used_rule = *it;
			return result;
		}

	}
	return STEMLAN_STOP;
}

int StemLancaster::get_table_index( const std::string& word )
{
	return word[word.size()-1]-'a'; 
}

int StemLancaster::apply_rule( const StemLancasterRule& rule, std::string& word, bool is_intact )
{
	/* If it should be intact,but isn't.. */
	if( !is_intact && rule.intact )
	{
		return STEMLAN_NOT_APPLY;
	}
	/* Find where suffix should start */
	int x = word.size() - rule.deltotal;
	if ( x <= 0 )
	{
		return STEMLAN_NOT_APPLY;
	}
	std::string suffix( word.begin()+x, word.end() );
	if ( suffix == rule.keystr )	/* If ending matches key string.. */
	{
		if ( !rule.prot )			/* ..then if not protected.. */
		{
			/* ..then swap it for rep string.. */
			word.replace( word.begin()+x, word.end(), rule.repstr );
		}
		else
		{
			return STEMLAN_STOP;
		}
		if ( rule.cont )
		{
			return STEMLAN_CONTINUE;
		}
		else
		{
			return STEMLAN_STOP;
		}

	}
	else
	{
		return STEMLAN_NOT_APPLY;
	}
}

bool StemLancaster::read_raw_rules()
{
	int p1, p2;
	p1 = p2 = 0;
	int ruleID = 0;
	std::string::iterator it = raw_rules.begin();
	for ( int i = 0 ; i < (int)raw_rules.size() ; i++ )
	{
		std::string cur_raw_rule;
		if ( raw_rules[i] == ';' )
		{
			p2 = i;
			cur_raw_rule.assign( it+p1, it+p2 );
			ruleID++;
			StemLancasterRule* p_cur_rule = make_rule( cur_raw_rule, ruleID );
			rule_table[ get_table_index(p_cur_rule->keystr) ].push_back( *p_cur_rule );
			delete p_cur_rule;
			p1 = p2+1;
		}
	}
	return true;
}

StemLancasterRule* StemLancaster::make_rule( const std::string& raw_rule, int ruleID )
{
	/* Format is: keystr,repstr,flags where keystr and repstr are strings,and */
	/* flags is one of:"protect","intact","continue","protint","contint",or  */
	/* "stop" (without the inverted commas in the actual file).  */

	bool is_error = false;
	StemLancasterRule* cur_rule = new StemLancasterRule;
	std::string cur_key, cur_rep, cur_flags;
	int p1, p2;
	p1 = raw_rule.find( ',' );
	p2 = raw_rule.find( ',', p1+1 );
	cur_key.assign( raw_rule.begin(), raw_rule.begin()+p1 );
	cur_rep.assign( raw_rule.begin()+p1+1, raw_rule.begin()+p2 );
	cur_flags.assign( raw_rule.begin()+p2+1, raw_rule.end() );
	if( !is_valid_str(cur_key) || (!is_valid_str(cur_rep) && cur_rep != "?") )
	{
		cur_rule->keystr = '?';
		return cur_rule;
	}
	if ( cur_rep == "?" )
	{
		cur_rep.clear();
	}
	cur_rule->keystr = cur_key;
	cur_rule->repstr = cur_rep;
	if( cur_flags == "protect" )
	{
		cur_rule->cont = false; /* ..set continue to false.. */
		cur_rule->prot = true; /* ..set protect to true */
		cur_rule->intact = false; /* Guess what? */
	}

	if( cur_flags == "intact" )
	{
		cur_rule->cont = false;
		cur_rule->prot = false;
		cur_rule->intact = true;
	}

	if( cur_flags == "continue" )
	{
		cur_rule->cont = true;
		cur_rule->prot = false;
		cur_rule->intact = false;
	}

	if( cur_flags == "contint" )
	{
		cur_rule->cont = true;
		cur_rule->intact = true;
		cur_rule->prot = false;
	}

	if( cur_flags == "protint" )
	{
		cur_rule->cont = false;
		cur_rule->prot = true;
		cur_rule->intact = true;
	}

	if( cur_flags == "stop" )
	{
		cur_rule->cont = false;
		cur_rule->prot = false;
		cur_rule->intact = false;
	}
	
	/* Delete total = length of key string */
	cur_rule->deltotal = cur_key.size();

	cur_rule->ruleID = ruleID;
	return cur_rule;
}

std::string StemLancaster::operator() ( const std::string& word )
{
	StemLancasterRule r = stem( word );
	return r.text;
}

std::string StemLancaster::get_description() const
{
	std::string desc( "A class implementing Lancaster Stemming Algorithm" );
	return desc;
}


