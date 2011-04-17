/* MemoServ core functions
 *
 * (C) 2003-2011 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

/*************************************************************************/

#include "module.h"

class MemoListCallback : public NumberList
{
	CommandSource &source;
	MemoInfo *mi;
 public:
	MemoListCallback(CommandSource &_source, MemoInfo *_mi, const Anope::string &numlist) : NumberList(numlist, false), source(_source), mi(_mi)
	{
	}

	void HandleNumber(unsigned Number)
	{
		if (!Number || Number > mi->memos.size())
			return;

		MemoListCallback::DoRead(source, mi, NULL, Number - 1);
	}

	static void DoRead(CommandSource &source, MemoInfo *mi, ChannelInfo *ci, unsigned index)
	{
		Memo *m = mi->memos[index];
		if (ci)
			source.Reply(_("Memo %d from %s (%s).  To delete, type: \002%s%s DEL %s %d\002"), index + 1, m->sender.c_str(), do_strftime(m->time).c_str(), Config->UseStrictPrivMsgString.c_str(), Config->s_MemoServ.c_str(), ci->name.c_str(), index + 1);
		else
			source.Reply(_("Memo %d from %s (%s).  To delete, type: \002%s%s DEL %d\002"), index + 1, m->sender.c_str(), do_strftime(m->time).c_str(), Config->UseStrictPrivMsgString.c_str(), Config->s_MemoServ.c_str(), index + 1);
		source.Reply("%s", m->text.c_str());
		m->UnsetFlag(MF_UNREAD);

		/* Check if a receipt notification was requested */
		if (m->HasFlag(MF_RECEIPT))
			rsend_notify(source, m, ci ? ci->name : "");
	}
};

class CommandMSRead : public Command
{
 public:
	CommandMSRead() : Command("READ", 1, 2)
	{
		this->SetDesc(_("Read a memo or memos"));
	}

	CommandReturn Execute(CommandSource &source, const std::vector<Anope::string> &params)
	{
		User *u = source.u;

		MemoInfo *mi;
		ChannelInfo *ci = NULL;
		Anope::string numstr = params[0], chan;

		if (!numstr.empty() && numstr[0] == '#')
		{
			chan = numstr;
			numstr = params.size() > 1 ? params[1] : "";

			if (!(ci = cs_findchan(chan)))
			{
				source.Reply(_(CHAN_X_NOT_REGISTERED), chan.c_str());
				return MOD_CONT;
			}
			else if (!check_access(u, ci, CA_MEMO))
			{
				source.Reply(_(ACCESS_DENIED));
				return MOD_CONT;
			}
			mi = &ci->memos;
		}
		else
			mi = &u->Account()->memos;

		if (numstr.empty() || (!numstr.equals_ci("LAST") && !numstr.equals_ci("NEW") && !numstr.is_number_only()))
			this->OnSyntaxError(source, numstr);
		else if (mi->memos.empty())
		{
			if (!chan.empty())
				source.Reply(_(MEMO_X_HAS_NO_MEMOS), chan.c_str());
			else
				source.Reply(_(MEMO_HAVE_NO_MEMOS));
		}
		else
		{
			int i, end;

			if (numstr.equals_ci("NEW"))
			{
				int readcount = 0;
				for (i = 0, end = mi->memos.size(); i < end; ++i)
					if (mi->memos[i]->HasFlag(MF_UNREAD))
					{
						MemoListCallback::DoRead(source, mi, ci, i);
						++readcount;
					}
				if (!readcount)
				{
					if (!chan.empty())
						source.Reply(_(MEMO_X_HAS_NO_NEW_MEMOS), chan.c_str());
					else
						source.Reply(_(MEMO_HAVE_NO_NEW_MEMOS));
				}
			}
			else if (numstr.equals_ci("LAST"))
			{
				for (i = 0, end = mi->memos.size() - 1; i < end; ++i);
				MemoListCallback::DoRead(source, mi, ci, i);
			}
			else /* number[s] */
			{
				MemoListCallback list(source, mi, numstr);
				list.Process();
			}
		}
		return MOD_CONT;
	}

	bool OnHelp(CommandSource &source, const Anope::string &subcommand)
	{
		source.Reply(_("Syntax: \002READ [\037channel\037] {\037num\037 | \037list\037 | LAST | NEW}\002\n"
				" \n"
				"Sends you the text of the memos specified. If LAST is\n"
				"given, sends you the memo you most recently received. If\n"
				"NEW is given, sends you all of your new memos.  Otherwise,\n"
				"sends you memo number \037num\037. You can also give a list of\n"
				"numbers, as in this example:\n"
				" \n"
				"   \002READ 2-5,7-9\002\n"
				"      Displays memos numbered 2 through 5 and 7 through 9."));
		return true;
	}

	void OnSyntaxError(CommandSource &source, const Anope::string &subcommand)
	{
		SyntaxError(source, "READ", _("READ [\037channel\037] {\037list\037 | LAST | NEW}"));
	}
};

class MSRead : public Module
{
	CommandMSRead commandmsread;

 public:
	MSRead(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator)
	{
		this->SetAuthor("Anope");
		this->SetType(CORE);

		this->AddCommand(MemoServ, &commandmsread);
	}
};

MODULE_INIT(MSRead)