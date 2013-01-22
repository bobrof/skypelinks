#include "SkypeLinks.h"

#define IMAGECLASS IMG
#define IMAGEFILE <SkypeLinks/SkypeLinks.iml>
#include <Draw/iml_source.h>

SkypeLinks::SkypeLinks()
{
	Icon(IMG::icon32);
	
	//SetLNGCharset(LNG_('R', 'U', 'R', 'U'), CHARSET_UTF8);
	//::SetLanguage( LNG_('R', 'U', 'R', 'U') );

	CtrlLayout(*this, "SkypeLinks");
	request.Disable();
	String cfg = LoadFile(GetAppDataFolder()+"/Skype/shared.xml");
	DUMP(cfg);
	request.WhenEnter = callback(this, &SkypeLinks::OnRequest);
	XmlNode xml = ParseXML(cfg);
	String login = xml.GetAdd("config").GetAdd("Lib").GetAdd("Account").GetAdd("Default").GatherText();
	DUMP(login);
	if (login.IsEmpty())
		return;
	String filename = GetAppDataFolder()+"/Skype/"+login+"/main.db";
	DUMP(filename);
	if (!FileExists(filename))
		return;
	indexer = new OmegaIndexer(this, filename);
	
	SetTimeCallback(-200, callback(this, &SkypeLinks::HandleEvents), 5);
	indexer->Start();

	memo.SetData(String("[d = Started OK&]"));
}

SkypeLinks::~SkypeLinks()
{
	if (indexer)
		indexer->ClearQueue();
	OnEntry("[d Shutdown... (this may take a while!)&]");
}

void SkypeLinks::OnEntry(String s)
{
	String content = static_cast<String>(memo.GetData());
	content += s;
	if (content.GetLength() > 50*1024)
	{
		int i;
		while (content.GetLength() > 40*1024 && (i = content.Find("&]",1)) >= 0)
		{
			content.Remove(0,i+2);
		}
	}
	memo.SetData(content);
	memo.ScrollEnd();
	//memo.SetCursor(content.GetLength());
}

void SkypeLinks::OnEnableSearch()
{
	request.Enable();
	memo.ScrollBegin();
}

void SkypeLinks::OnRequest()
{
	request.Disable();
	memo.Clear();
	indexer->Request(&OmegaIndexer::OnRequest, static_cast<String>(request.GetData()));
}

void SkypeLinks::HandleEvents()
{
	DoTasks();
}

GUI_APP_MAIN
{
	SkypeLinks().Sizeable().Zoomable().Run();
}
