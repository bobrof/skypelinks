#include "OmegaIndexer.h"

const String FILE_CONFIG = "SkypeLinks.indexer.state";

OmegaIndexer::OmegaIndexer(OmegaIndexerNotify *_notify, const String &filename)
	:offset(0)
	,lastCount(0)
	,notify(_notify)
	,isIndexed(false)
{
	LoadFromFile(*this, FILE_CONFIG);

	if (!sqlite.Open(filename))
		return;

	if (!DirectoryExists("data"))
		DirectoryCreate("data");
	Request(&OmegaIndexer::FetchCheck);
}

OmegaIndexer::~OmegaIndexer()
{
	Shutdown();
	fetchWork.Finish();
	if (sqlite.IsOpen())
		sqlite.Close();
	
	StoreToFile(*this, FILE_CONFIG);
}

void OmegaIndexer::FetchCheck()
{
	Sql sql(sqlite);
	sql.Execute("SELECT COUNT(\"body_xml\") FROM 'Messages'");
	while (sql.Fetch())
		count = static_cast<uint64>(static_cast<int64>(sql[0]));
	
	if (count > lastCount)
	{
		notify->OnEntry_("[d = Fetching...&]");
		isIndexed = false;
		offset = lastCount;
		FetchNext();
	}
	else
	if (!isIndexed) 
		Request(&OmegaIndexer::BuildIndex);
	else
		notify->OnEnableSearch_();
}

void OmegaIndexer::FetchNext()
{
	notify->OnEntry_(Format("[*@gi100 \1%02.02f%%\1&]",100.*offset/count));
	static const int SELECT_LIMIT = 100;
	Sql sql(sqlite);
	sql.Execute("SELECT \"body_xml\" FROM 'Messages' ORDER BY 'id' LIMIT "+FormatInt(SELECT_LIMIT)+" OFFSET "+FormatInt(offset));
	offset += SELECT_LIMIT;
	bool isEmpty = true;
	while (sql.Fetch())
	{
		isEmpty = false;
		ProcessEntry(lastCount, static_cast<String>(sql[0]));
		++lastCount;
	}
	
	if (!isEmpty && !IsShutdown() && offset < count)
		Request(&OmegaIndexer::FetchNext);
	else
	if (!isIndexed) 
	{
		Request(&OmegaIndexer::BuildIndex);
	}
	else
		notify->OnEnableSearch_();
}

void OmegaIndexer::ProcessEntry(int i, const String &src)
{
	if (src.StartsWith("<partlist"))
		return;
	
	int sOffset = 0;
	while ((sOffset = src.Find("http", sOffset)) >= 0)
	{
		if (sOffset && src[sOffset] == '"')
		{
			sOffset += 4;
			continue;
		}
		if (src.Mid(sOffset+4,3) == "://")
			;//sOffset += 7;
		else
		if (src.Mid(sOffset+5,3) == "://")
			;//sOffset += 8;
		else
		{
			sOffset += 4;
			continue;
		}
		int sOffset2 = src.Find('"',sOffset);
		int sOffset3 = src.Find('<',sOffset);
		if (sOffset2 < 0)
			sOffset2 = src.GetLength()-1;
		if (sOffset3 < 0)
			sOffset3 = src.GetLength()-1;
		sOffset2 = min(sOffset2,sOffset3);
		String s = src.Mid(sOffset, sOffset2-sOffset);
		sOffset = sOffset2 + 1;
		
		static const String STRING_SLASH = "/";
		while (s.EndsWith(STRING_SLASH))
			s.Trim(s.GetLength()-1);

		if (
			s.EndsWith(".jpg")
		||  s.EndsWith(".png")
		||  s.EndsWith(".gif")
		||  s.EndsWith(".jpeg")
		||  s.EndsWith(".jp2000")
		||  s.EndsWith(".mpg")
		||  s.EndsWith(".avi")
		||  s.EndsWith(".mkv")
		||  s.EndsWith(".bmp")
		||  s.EndsWith(".tga")
		||  s.EndsWith(".targa")
		||  s.EndsWith(".pdf")
		||  s.EndsWith(".doc")
		||  s.EndsWith(".docx")
		||  s.EndsWith(".djvu")
		||  s.EndsWith(".xls")
		||  s.EndsWith(".xlsx")
		||  s.EndsWith(".ppt")
		||  s.EndsWith(".pptx")
		||  s.EndsWith(".zip")
		||  s.EndsWith(".rar")
		||  s.EndsWith(".gz")
		)
			continue;

		if (indexed.Find(s) < 0)
		{
			indexed.Add(s);
			ids.Add(i, indexed.GetCount()-1);
			fetchWork.Do(callback2(this, &OmegaIndexer::Fetch_, i, s));
		}
	}
}

void OmegaIndexer::Fetch_(int i, String s)
{
	Sleep(2000); //delay between http requests to prevent IP ban
	
	String out = HttpClientGet(s, static_cast<String *>(NULL), NULL, false, 60000, 15, 10);
	if (out.IsEmpty() || out.GetLength() > 2*1024*1024)
	{
		notify->OnEntry_("[1@Ki100 \1" + s + "\1&]");
		return;
	}
	FileOut f(Format("data/%08d",i));
	f << out;
	f.Close();
	notify->OnEntry_("[1i100 \1" + s + " ->\1 [* " + FormatInt(out.GetLength())+"]&]");
}

void OmegaIndexer::BuildIndex()
{
	fetchWork.Finish();
	notify->OnEntry_("[d = Indexing...&]");
	LocalProcess proc;
	proc.Start("xapian/omindex.exe data --verbose --url=/ --duplicates=ignore --no-delete --empty-docs=skip --db=index --mime-type=:text/html --stemmer=russian");
	String buf, out;	
	while (proc.Read(out))
	{
		buf += out;
		int i = buf.Find('\n');
		if (i < 0)
			continue;
		notify->OnEntry_("[1 \1"+buf.Left(i)+"\1&]");
		buf.Remove(0,i+1);
	}
	if (!buf.IsEmpty())
		notify->OnEntry_("[1 \1"+buf+"\1&]");
	isIndexed = true;
	notify->OnEntry_("[d = Finishing index...&]");
	DeleteFolderDeep("data");
	DirectoryCreate("data");
	notify->OnEntry_("[d = Indexing complete&]");
	notify->OnEnableSearch_();
}

void OmegaIndexer::OnRequest(String s)
{
	Vector<String> rqs = Split(s, ' ');
	for (int i=0; i<rqs.GetCount(); ++i)
		if (rqs[i].GetLength() > 5)
			rqs[i] += '*';
	s = Join(rqs," ");
	LocalProcess proc;
	proc.NoConvertCharset().Start("xapian/simplesearch.exe index '"+s);
	String buf, out;	
	while (proc.Read(out))
		buf += out;
	
	//parse results
	Vector<String> results = Split(buf,'\n');
	enum STATE {STATE_SEARCH_RESULT, STATE_SEARCH_SAMPLE} state = STATE_SEARCH_RESULT;
	struct SearchResult : Moveable<SearchResult>
	{
		String percent;
		String url;
		String sample;
		String caption;
	};
	SearchResult currentResult;
	Vector<SearchResult> searchResults;
	for (int i=0; i<results.GetCount(); ++i)
	{
		switch (state)
		{
		case STATE_SEARCH_RESULT:
			if (results[i].Find("url=/") >= 0)
			{
				currentResult.sample.Clear();
				currentResult.caption.Clear();
				Vector<String> hdr = Split(results[i],' ');
				currentResult.percent = hdr[1];
				int indexedI = ids.GetAdd(ScanInt(hdr[3].Mid(6)), 0);
				currentResult.url = indexedI < 0 ? String() : indexed[indexedI];
				state = STATE_SEARCH_SAMPLE;
			}
			break;
		case STATE_SEARCH_SAMPLE:
			if (results[i].StartsWith("caption="))
				currentResult.caption = results[i].Mid(8);
			else
			if (results[i].StartsWith("sample="))
				currentResult.sample = results[i].Mid(7);
			else
			if (results[i].StartsWith("size="))
			{
				searchResults.Add(currentResult);
				state = STATE_SEARCH_RESULT;
			}
			break;
		}
	}

	for (int i=0; i<searchResults.GetCount(); ++i)
	{
		notify->OnEntry_(Format("[0 \1%s\1] [^\1%s\1^ \1%s\1]&[2l150 \1%s\1&]&",
			searchResults[i].percent,
			searchResults[i].url,
			(searchResults[i].caption.IsEmpty() ? searchResults[i].url : searchResults[i].caption),
			searchResults[i].sample
		));
	}
	
	notify->OnEnableSearch_();
}

void OmegaIndexer::Serialize(Stream &stream)
{
	stream % lastCount % indexed % isIndexed % ids;
}

