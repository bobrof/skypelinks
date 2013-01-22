#ifndef _SkypeLinks_OmegaIndexer_h_
#define _SkypeLinks_OmegaIndexer_h_

#include <Core/Core.h>
#include <Web/Web.h>
#include <MtAlt/MtAlt.h>
#include <plugin/sqlite3/Sqlite3.h>

using namespace Upp;

struct OmegaIndexerNotify
{
	virtual void OnEntry_(const String &s) = 0;
	virtual void OnEnableSearch_() = 0;
};

class OmegaIndexer : public CallbackThread
{
public:
	OmegaIndexer(OmegaIndexerNotify *notify, const String &filename);
	~OmegaIndexer();
	void Serialize(Stream &stream);
	
	void FetchCheck();
	void FetchNext();
	void OnRequest(String s);
private:
	void Fetch_(int i, String);
	void BuildIndex();
	void ProcessEntry(int i, const String &src);
	
	Sqlite3Session sqlite;
	int offset;
	OmegaIndexerNotify *notify;
	uint64 count;
	Index<String> indexed;
	VectorMap<int,int> ids;
	bool isIndexed;
	int lastCount;
	CoWork fetchWork;
};

#endif
