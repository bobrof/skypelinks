#ifndef _SkypeLinks_SkypeLinks_h
#define _SkypeLinks_SkypeLinks_h

#include <CtrlLib/CtrlLib.h>
#include "OmegaIndexer.h"

using namespace Upp;

#define LAYOUTFILE <SkypeLinks/SkypeLinks.lay>
#include <CtrlCore/lay.h>

#define IMAGECLASS IMG
#define IMAGEFILE <SkypeLinks/SkypeLinks.iml>
#include <Draw/iml_header.h>


class SkypeLinks : public WithSkypeLinksLayout<TopWindow>, public OmegaIndexerNotify, public CallbackQueue
{
public:
	SkypeLinks();
	~SkypeLinks();
	virtual void OnEntry_(const String &s) {Request(&SkypeLinks::OnEntry, s);}
	virtual void OnEnableSearch_() {Request(&SkypeLinks::OnEnableSearch);}
private:
	void HandleEvents();
	void OnEntry(String s); 
	void OnEnableSearch();
	void OnRequest();
	One<OmegaIndexer> indexer;
};

#endif
