 /* BonkEnc Audio Encoder
  * Copyright (C) 2001-2013 Robert Kausch <robert.kausch@bonkenc.org>
  *
  * This program is free software; you can redistribute it and/or
  * modify it under the terms of the "GNU General Public License".
  *
  * THIS PACKAGE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
  * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE. */

#include <cddb/cddbbatch.h>
#include <cddb/cddblocal.h>
#include <cddb/cddbremote.h>
#include <config.h>
#include <utilities.h>

#include <dialogs/cddb/query.h>

BonkEnc::CDDBBatch::CDDBBatch()
{
	ReadEntries();
}

BonkEnc::CDDBBatch::~CDDBBatch()
{
	SaveEntries();
}

Bool BonkEnc::CDDBBatch::ReadEntries()
{
	BoCA::Config	*config = BoCA::Config::Get();

	String	 inputFormat = String::SetInputFormat("UTF-8");
	String	 outputFormat = String::SetOutputFormat("UTF-8");

	/* Read saved queries from XML
	 */
	XML::Document	*document = new XML::Document();

	if (document->LoadFile(String(config->configDir).Append("cddb").Append(Directory::GetDirectoryDelimiter()).Append("queries.xml")) == Success())
	{
		XML::Node	*root = document->GetRootNode();

		if (root != NIL)
		{
			for (Int i = 0; i < root->GetNOfNodes(); i++)
			{
				XML::Node	*node = root->GetNthNode(i);

				if (node->GetName() == "query") queries.Add(node->GetContent());
			}
		}
	}

	delete document;

	/* Read saved submits from XML and database cache
	 */
	document = new XML::Document();

	if (document->LoadFile(String(config->configDir).Append("cddb").Append(Directory::GetDirectoryDelimiter()).Append("submits.xml")) == Success())
	{
		ReadEntriesXML(document);
	}

	delete document;

	String::SetInputFormat(inputFormat);
	String::SetOutputFormat(outputFormat);

	return True;
}

Bool BonkEnc::CDDBBatch::ReadEntriesXML(XML::Document *document)
{
	XML::Node	*root = document->GetRootNode();

	if (root == NIL) return False;

	BoCA::Config	*config = BoCA::Config::Get();

	for (Int i = 0; i < root->GetNOfNodes(); i++)
	{
		XML::Node	*node = root->GetNthNode(i);

		if (node->GetName() == "submit")
		{
			InStream	*in = new InStream(STREAM_FILE, String(config->configDir).Append("cddb").Append(Directory::GetDirectoryDelimiter()).Append(node->GetAttributeByName("category")->GetContent()).Append(Directory::GetDirectoryDelimiter()).Append(node->GetContent()), IS_READ);

			if (in->Size() > 0)
			{
				String	 result = in->InputString(in->Size());
				CDDBInfo cddbInfo;

				ParseCDDBRecord(result, cddbInfo);

				cddbInfo.category = node->GetAttributeByName("category")->GetContent();

				for (Int j = 0; j < submits.Length(); j++)
				{
					if (submits.GetNth(j) == cddbInfo)
					{
						submits.Remove(submits.GetNthIndex(j));

						break;
					}
				}

				submits.Add(cddbInfo);
			}

			delete in;
		}
	}

	return True;
}

Bool BonkEnc::CDDBBatch::SaveEntries()
{
	BoCA::Config	*config = BoCA::Config::Get();

	String	 configDir = config->configDir;

	/* Save queued queries
	 */
	if (queries.Length() == 0)
	{
		/* Delete queries file if no more saved queries exist
		 */
		File(String(configDir).Append("cddb").Append(Directory::GetDirectoryDelimiter()).Append("queries.xml")).Delete();
	}
	else
	{
		Directory(String(configDir).Append("cddb")).Create();

		XML::Document	*document = new XML::Document();
		XML::Node	*root = new XML::Node("cddbQueries");

		document->SetRootNode(root);

		for (Int i = 0; i < queries.Length(); i++)
		{
			root->AddNode("query", queries.GetNth(i));
		}

		document->SaveFile(String(configDir).Append("cddb").Append(Directory::GetDirectoryDelimiter()).Append("queries.xml"));

		delete document;
		delete root;
	}

	/* Save queued submits
	 */
	if (submits.Length() == 0)
	{
		/* Delete submits file if no more saved submits exist
		 */
		File(String(configDir).Append("cddb").Append(Directory::GetDirectoryDelimiter()).Append("submits.xml")).Delete();
	}
	else
	{
		Directory(String(configDir).Append("cddb")).Create();

		XML::Document	*document = new XML::Document();
		XML::Node	*root = new XML::Node("cddbSubmits");

		document->SetRootNode(root);

		for (Int i = 0; i < submits.Length(); i++)
		{
			XML::Node	*node = root->AddNode("submit", submits.GetNth(i).DiscIDToString());

			node->SetAttribute("category", submits.GetNth(i).category);
		}

		document->SaveFile(String(configDir).Append("cddb").Append(Directory::GetDirectoryDelimiter()).Append("submits.xml"));

		delete document;
		delete root;
	}

	return True;
}

Bool BonkEnc::CDDBBatch::AddQuery(const String &query)
{
	for (Int i = 0; i < queries.Length(); i++)
	{
		if (queries.GetNth(i) == query) return False;
	}

	queries.Add(query);

	return True;
}

Bool BonkEnc::CDDBBatch::DeleteQuery(Int n)
{
	queries.Remove(queries.GetNthIndex(n));

	return True;
}

Bool BonkEnc::CDDBBatch::AddSubmit(const CDDBInfo &cddbInfo)
{
	BoCA::Config	*config = BoCA::Config::Get();

	String	 configDir = config->configDir;

	/* Create directory for entry
	 */
	Directory	 cddbDir(String(configDir).Append("cddb"));

	if (!cddbDir.Exists()) cddbDir.Create();

	Directory	 categoryDir(String(configDir).Append("cddb").Append(Directory::GetDirectoryDelimiter()).Append(cddbInfo.category));

	if (!categoryDir.Exists()) categoryDir.Create();

	/* Save current freedb path
	 */
	String	 configFreedbDir = config->GetStringValue(Config::CategoryFreedbID, Config::FreedbDirectoryID, Config::FreedbDirectoryDefault);

	config->SetStringValue(Config::CategoryFreedbID, Config::FreedbDirectoryID, String(configDir).Append("cddb").Append(Directory::GetDirectoryDelimiter()));

	CDDBLocal	 cddb;

	/* Save entry to batch queue
	 */
	cddb.SetActiveDrive(activeDriveID);
	cddb.Submit(cddbInfo);

	CDDBInfo nCddbInfo = cddbInfo;

	nCddbInfo.discID = cddb.ComputeDiscID();

	for (Int i = 0; i < submits.Length(); i++)
	{
		if (submits.GetNth(i) == nCddbInfo)
		{
			submits.Remove(submits.GetNthIndex(i));

			break;
		}
	}

	submits.Add(nCddbInfo);

	/* Restore real freedb path
	 */
	config->SetStringValue(Config::CategoryFreedbID, Config::FreedbDirectoryID, configFreedbDir);

	return True;
}

Bool BonkEnc::CDDBBatch::DeleteSubmit(const CDDBInfo &cddbInfo)
{
	for (Int i = 0; i < submits.Length(); i++)
	{
		if (submits.GetNth(i) == cddbInfo)
		{
			submits.Remove(submits.GetNthIndex(i));

			break;
		}
	}

	return True;
}

Int BonkEnc::CDDBBatch::Query(Int n)
{
	BoCA::Config	*config = BoCA::Config::Get();

	/* Query entry and delete entry if successful
	 */
	cddbQueryDlg	*dlg		= new cddbQueryDlg();

	dlg->SetQueryString(queries.GetNth(n));

	const CDDBInfo	&cddbInfo	= dlg->QueryCDDB(False);

	Object::DeleteObject(dlg);

	if (cddbInfo != NIL)
	{
		/* Save current freedb path
		 */
		String	 configFreedbDir = config->GetStringValue(Config::CategoryFreedbID, Config::FreedbDirectoryID, Config::FreedbDirectoryDefault);

		config->SetStringValue(Config::CategoryFreedbID, Config::FreedbDirectoryID, String(config->configDir).Append("cddb").Append(Directory::GetDirectoryDelimiter()));

		CDDBLocal	 cddb;

		cddb.SetUpdateTrackOffsets(False);

		/* Save entry to local cache
		 */
		cddb.Submit(cddbInfo);

		/* Restore real freedb path
		 */
		config->SetStringValue(Config::CategoryFreedbID, Config::FreedbDirectoryID, configFreedbDir);

		DeleteQuery(n);

		return QUERY_RESULT_SINGLE;
	}

	return QUERY_RESULT_ERROR;
}

Bool BonkEnc::CDDBBatch::QueryAll()
{
	while (queries.Length() > 0)
	{
		if (Query(0) == QUERY_RESULT_ERROR) return False;
	}

	return True;
}

Bool BonkEnc::CDDBBatch::Submit(const CDDBInfo &oCddbInfo)
{
	/* Submit and delete entry if successful
	 */
	CDDBInfo	 cddbInfo = oCddbInfo;

	cddbInfo.revision++;

	CDDBRemote	 cddb;

	cddb.SetUpdateTrackOffsets(False);

	if (!cddb.Submit(cddbInfo))
	{
		BoCA::I18n	*i18n	= BoCA::I18n::Get();

		i18n->SetContext("CDDB::Submit::Errors");

		BoCA::Utilities::ErrorMessage(i18n->TranslateString("Some error occurred trying to connect to the freedb server."));

		return False;
	}

	DeleteSubmit(cddbInfo);

	return True;
}

Bool BonkEnc::CDDBBatch::SubmitAll()
{
	while (submits.Length() > 0)
	{
		if (!Submit(submits.GetNth(0))) return False;
	}

	return True;
}
