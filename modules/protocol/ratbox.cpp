/* Ratbox IRCD functions
 *
 * (C) 2003-2010 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for furhter details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

#include "services.h"
#include "modules.h"

static Anope::string TS6UPLINK;

IRCDVar myIrcd[] = {
	{"Ratbox 2.0+",	/* ircd name */
	 "+oi",			/* Modes used by pseudoclients */
	 0,				/* SVSNICK */
	 0,				/* Vhost */
	 1,				/* Supports SNlines */
	 1,				/* Supports SQlines */
	 0,				/* Supports SZlines */
	 1,				/* Join 2 Message */
	 1,				/* Chan SQlines */
	 0,				/* Quit on Kill */
	 0,				/* vidents */
	 0,				/* svshold */
	 0,				/* time stamp on mode */
	 0,				/* UMODE */
	 0,				/* O:LINE */
	 0,				/* No Knock requires +i */
	 0,				/* Can remove User Channel Modes with SVSMODE */
	 0,				/* Sglines are not enforced until user reconnects */
	 1,				/* ts6 */
	 "$$",			/* TLD Prefix for Global */
	 4,				/* Max number of modes we can send per line */
	 }
	,
	{NULL}
};

/*
 * SVINFO
 *	  parv[0] = sender prefix
 *	  parv[1] = TS_CURRENT for the server
 *	  parv[2] = TS_MIN for the server
 *	  parv[3] = server is standalone or connected to non-TS only
 *	  parv[4] = server's idea of UTC time
 */
void ratbox_cmd_svinfo()
{
	send_cmd("", "SVINFO 6 3 0 :%ld", static_cast<long>(Anope::CurTime));
}

void ratbox_cmd_svsinfo()
{
}

/* CAPAB */
/*
  QS     - Can handle quit storm removal
  EX     - Can do channel +e exemptions
  CHW    - Can do channel wall @#
  LL     - Can do lazy links
  IE     - Can do invite exceptions
  EOB    - Can do EOB message
  KLN    - Can do KLINE message
  GLN    - Can do GLINE message
  HUB    - This server is a HUB
  UID    - Can do UIDs
  ZIP    - Can do ZIPlinks
  ENC    - Can do ENCrypted links
  KNOCK  -  supports KNOCK
  TBURST - supports TBURST
  PARA   - supports invite broadcasting for +p
  ENCAP  - ?
*/
void ratbox_cmd_capab()
{
	send_cmd("", "CAPAB :QS EX CHW IE KLN GLN KNOCK TB UNKLN CLUSTER ENCAP");
}

/* PASS */
void ratbox_cmd_pass(const Anope::string &pass)
{
	send_cmd("", "PASS %s TS 6 :%s", pass.c_str(), Config->Numeric.c_str());
}

class RatboxProto : public IRCDProto
{
	void SendGlobopsInternal(const BotInfo *source, const Anope::string &buf)
	{
		if (source)
			send_cmd(source->GetUID(), "OPERWALL :%s", buf.c_str());
		else
			send_cmd(Config->Numeric, "OPERWALL :%s", buf.c_str());
	}

	void SendSQLine(const XLine *x)
	{
		send_cmd(Config->Numeric, "RESV * %s :%s", x->Mask.c_str(), x->Reason.c_str());
	}

	void SendSGLineDel(const XLine *x)
	{
		BotInfo *bi = OperServ;
		send_cmd(bi ? bi->GetUID() : Config->s_OperServ, "UNXLINE * %s", x->Mask.c_str());
	}

	void SendSGLine(const XLine *x)
	{
		BotInfo *bi = OperServ;
		send_cmd(bi ? bi->GetUID() : Config->s_OperServ, "XLINE * %s 0 :%s", x->Mask.c_str(), x->Reason.c_str());
	}

	void SendAkillDel(const XLine *x)
	{
		BotInfo *bi = OperServ;
		send_cmd(bi ? bi->GetUID() : Config->s_OperServ, "UNKLINE * %s %s", x->GetUser().c_str(), x->GetHost().c_str());
	}

	void SendSQLineDel(const XLine *x)
	{
		send_cmd(Config->Numeric, "UNRESV * %s", x->Mask.c_str());
	}

	void SendJoin(const BotInfo *user, const Anope::string &channel, time_t chantime)
	{
		send_cmd("", "SJOIN %ld %s + :%s", static_cast<long>(chantime), channel.c_str(), user->GetUID().c_str());
	}

	void SendJoin(const BotInfo *user, const ChannelContainer *cc)
	{
		send_cmd("", "SJOIN %ld %s +%s :%s%s", static_cast<long>(cc->chan->creation_time), cc->chan->name.c_str(), cc->chan->GetModes(true, true).c_str(), cc->Status->BuildModePrefixList().c_str(), user->GetUID().c_str());
	}

	void SendAkill(const XLine *x)
	{
		BotInfo *bi = OperServ;
		send_cmd(bi ? bi->GetUID() : Config->s_OperServ, "KLINE * %ld %s %s :%s", static_cast<long>(x->Expires - Anope::CurTime), x->GetUser().c_str(), x->GetHost().c_str(), x->Reason.c_str());
	}

	void SendSVSKillInternal(const BotInfo *source, const User *user, const Anope::string &buf)
	{
		send_cmd(source ? source->GetUID() : Config->Numeric, "KILL %s :%s", user->GetUID().c_str(), buf.c_str());
	}

	void SendSVSMode(const User *u, int ac, const char **av)
	{
		this->SendModeInternal(NULL, u, merge_args(ac, av));
	}

	/* SERVER name hop descript */
	void SendServer(const Server *server)
	{
		send_cmd("", "SERVER %s %d :%s", server->GetName().c_str(), server->GetHops(), server->GetDescription().c_str());
	}

	void SendConnect()
	{
		ratbox_cmd_pass(uplink_server->password);
		ratbox_cmd_capab();
		/* Make myself known to myself in the serverlist */
		SendServer(Me);
		ratbox_cmd_svinfo();
	}

	void SendClientIntroduction(const User *u, const Anope::string &modes)
	{
		EnforceQlinedNick(u->nick, "");
		send_cmd(Config->Numeric, "UID %s 1 %ld %s %s %s 0 %s :%s", u->nick.c_str(), static_cast<long>(u->timestamp), modes.c_str(), u->GetIdent().c_str(), u->host.c_str(), u->GetUID().c_str(), u->realname.c_str());
	}

	void SendPartInternal(const BotInfo *bi, const Channel *chan, const Anope::string &buf)
	{
		if (!buf.empty())
			send_cmd(bi->GetUID(), "PART %s :%s", chan->name.c_str(), buf.c_str());
		else
			send_cmd(bi->GetUID(), "PART %s", chan->name.c_str());
	}

	void SendNumericInternal(const Anope::string &, int numeric, const Anope::string &dest, const Anope::string &buf)
	{
		// This might need to be set in the call to SendNumeric instead of here, will review later -- CyberBotX
		send_cmd(Config->Numeric, "%03d %s %s", numeric, dest.c_str(), buf.c_str());
	}

	void SendModeInternal(const BotInfo *bi, const Channel *dest, const Anope::string &buf)
	{
		if (bi)
			send_cmd(bi->GetUID(), "MODE %s %s", dest->name.c_str(), buf.c_str());
		else
			send_cmd(Config->Numeric, "MODE %s %s", dest->name.c_str(), buf.c_str());
	}

	void SendModeInternal(const BotInfo *bi, const User *u, const Anope::string &buf)
	{
		if (buf.empty())
			return;
		send_cmd(bi ? bi->GetUID() : Config->Numeric, "SVSMODE %s %s", u->nick.c_str(), buf.c_str());
	}

	void SendKickInternal(const BotInfo *bi, const Channel *chan, const User *user, const Anope::string &buf)
	{
		if (!buf.empty())
			send_cmd(bi->GetUID(), "KICK %s %s :%s", chan->name.c_str(), user->GetUID().c_str(), buf.c_str());
		else
			send_cmd(bi->GetUID(), "KICK %s %s", chan->name.c_str(), user->GetUID().c_str());
	}

	void SendNoticeChanopsInternal(const BotInfo *source, const Channel *dest, const Anope::string &buf)
	{
		send_cmd("", "NOTICE @%s :%s", dest->name.c_str(), buf.c_str());
	}

	/* INVITE */
	void SendInvite(BotInfo *source, const Anope::string &chan, const Anope::string &nick)
	{
		User *u = finduser(nick);
		send_cmd(source->GetUID(), "INVITE %s %s", u ? u->GetUID().c_str() : nick.c_str(), chan.c_str());
	}

	void SendAccountLogin(const User *u, const NickCore *account)
	{
		send_cmd(Config->Numeric, "ENCAP * SU %s %s", u->GetUID().c_str(), account->display.c_str());
	}

	void SendAccountLogout(const User *u, const NickCore *account)
	{
		send_cmd(Config->Numeric, "ENCAP * SU %s", u->GetUID().c_str());
	}

	bool IsNickValid(const Anope::string &nick)
	{
		/* TS6 Save extension -Certus */
		if (isdigit(nick[0]))
			return false;

		return true;
	}

	void SendTopic(BotInfo *bi, Channel *c)
	{
		bool needjoin = c->FindUser(bi) != NULL;
		if (needjoin)
		{
			ChannelStatus status;
			status.SetFlag(CMODE_OP);
			ChannelContainer cc(c);
			cc.Status = &status;
			ircdproto->SendJoin(bi, &cc);
		}
		send_cmd(bi->GetUID(), "TOPIC %s :%s", c->name.c_str(), c->topic.c_str());
		if (needjoin)
		{
			ircdproto->SendPart(bi, c, NULL);
		}
	}

	void SetAutoIdentificationToken(User *u)
	{

		if (!u->Account())
			return;

		Anope::string svidbuf = stringify(u->timestamp);

		u->Account()->Shrink("authenticationtoken");
		u->Account()->Extend("authenticationtoken", new ExtensibleItemRegular<Anope::string>(svidbuf));
	}
};

class RatboxIRCdMessage : public IRCdMessage
{
 public:
	bool OnMode(const Anope::string &source, const std::vector<Anope::string> &params)
	{
		if (params.size() < 2)
			return true;

		if (params[0][0] == '#' || params[0][0] == '&')
			do_cmode(source, params[0], params[2], params[1]);
		else
		{
			User *u = finduser(source);
			User *u2 = finduser(params[0]);
			if (!u || !u2)
				return true;
			do_umode(u->nick, u2->nick, params[1]);
		}

		return true;
	}

	bool OnUID(const Anope::string &source, const std::vector<Anope::string> &params)
	{
		Server *s = Server::Find(source);
		/* Source is always the server */
		User *user = do_nick("", params[0], params[4], params[5], s->GetName(), params[8], Anope::string(params[2]).is_pos_number_only() ? convertTo<time_t>(params[2]) : 0, params[6], "*", params[7], params[3]);
		if (user)
		{
			NickAlias *na = findnick(user->nick);
			Anope::string svidbuf;
			if (na && na->nc->GetExtRegular("authenticationtoken", svidbuf) && svidbuf == params[2])
			{
				user->Login(na->nc);
			}
			else
				validate_user(user);
		}

		return true;
	}

	/*
	  params[0] = nick
	  params[1] = hop
	  params[2] = ts
	  params[3] = modes
	  params[4] = user
	  params[5] = host
	  params[6] = IP
	  params[7] = UID
	  params[8] = info
	*/
	bool OnNick(const Anope::string &source, const std::vector<Anope::string> &params)
	{
		do_nick(source, params[0], "", "", "", "", Anope::string(params[1]).is_pos_number_only() ? convertTo<time_t>(params[1]) : 0, "", "", "", "");

		return true;
	}

	bool OnServer(const Anope::string &source, const std::vector<Anope::string> &params)
	{
		if (params[1].equals_cs("1"))
			do_server(source, params[0], Anope::string(params[1]).is_pos_number_only() ? convertTo<unsigned>(params[1]) : 0, params[2], TS6UPLINK);
		else
			do_server(source, params[0], Anope::string(params[1]).is_pos_number_only() ? convertTo<unsigned>(params[1]) : 0, params[2], "");
		return true;
	}

	bool OnTopic(const Anope::string &source, const std::vector<Anope::string> &params)
	{
		Channel *c = findchan(params[0]);
		if (!c)
		{
			Log() << "TOPIC for nonexistant channel " << params[0];
			return true;
		}

		if (params.size() == 4)
		{
			c->ChangeTopicInternal(params[1], params[3], Anope::string(params[2]).is_pos_number_only() ? convertTo<time_t>(params[2]) : Anope::CurTime);
		}
		else
		{
			c->ChangeTopicInternal(source, (params.size() > 1 ? params[1] : ""));
		}
		return true;
	}

	bool OnSJoin(const Anope::string &source, const std::vector<Anope::string> &params)
	{
		Channel *c = findchan(params[1]);
		time_t ts = Anope::string(params[0]).is_pos_number_only() ? convertTo<time_t>(params[0]) : 0;
		bool keep_their_modes = true;

		if (!c)
		{
			c = new Channel(params[1], ts);
			c->SetFlag(CH_SYNCING);
		}
		/* Our creation time is newer than what the server gave us */
		else if (c->creation_time > ts)
		{
			c->creation_time = ts;
			c->Reset();

			/* Reset mlock */
			check_modes(c);
		}
		/* Their TS is newer than ours, our modes > theirs, unset their modes if need be */
		else if (ts > c->creation_time)
			keep_their_modes = false;

		/* If we need to keep their modes, and this SJOIN string contains modes */
		if (keep_their_modes && params.size() >= 3)
		{
			Anope::string modes;
			for (unsigned i = 2; i < params.size() - 1; ++i)
				modes += " " + params[i];
			if (!modes.empty())
				modes.erase(modes.begin());
			/* Set the modes internally */
			c->SetModesInternal(NULL, modes);
		}

		spacesepstream sep(params[params.size() - 1]);
		Anope::string buf;
		while (sep.GetToken(buf))
		{
			std::list<ChannelMode *> Status;
			char ch;

			/* Get prefixes from the nick */
			while ((ch = ModeManager::GetStatusChar(buf[0])))
			{
				buf.erase(buf.begin());
				ChannelMode *cm = ModeManager::FindChannelModeByChar(ch);
				if (!cm)
				{
					Log() << "Received unknown mode prefix " << buf[0] << " in SJOIN string";
					continue;
				}

				if (keep_their_modes)
					Status.push_back(cm);
			}
	
			User *u = finduser(buf);
			if (!u)
			{
				Log(LOG_DEBUG) << "SJOIN for nonexistant user " << buf << " on " << c->name;
				continue;
			}

			EventReturn MOD_RESULT;
			FOREACH_RESULT(I_OnPreJoinChannel, OnPreJoinChannel(u, c));

			/* Add the user to the channel */
			c->JoinUser(u);

			/* Update their status internally on the channel
			 * This will enforce secureops etc on the user
			 */
			for (std::list<ChannelMode *>::iterator it = Status.begin(), it_end = Status.end(); it != it_end; ++it)
				c->SetModeInternal(*it, buf);

			/* Now set whatever modes this user is allowed to have on the channel */
			chan_set_correct_modes(u, c, 1);

			/* Check to see if modules want the user to join, if they do
			 * check to see if they are allowed to join (CheckKick will kick/ban them)
			 * Don't trigger OnJoinChannel event then as the user will be destroyed
			 */
			if (MOD_RESULT != EVENT_STOP && c->ci && c->ci->CheckKick(u))
				continue;

			FOREACH_MOD(I_OnJoinChannel, OnJoinChannel(u, c));
		}

		/* Channel is done syncing */
		if (c->HasFlag(CH_SYNCING))
		{
			/* Unset the syncing flag */
			c->UnsetFlag(CH_SYNCING);
			c->Sync();
		}

		return true;
	}
};

bool event_tburst(const Anope::string &source, const std::vector<Anope::string> &params)
{
	if (params.size() != 4)
		return true;

	Anope::string setter = myStrGetToken(params[2], '!', 0);
	time_t topic_time = Anope::string(params[1]).is_pos_number_only() ? convertTo<time_t>(params[1]) : Anope::CurTime;
	Channel *c = findchan(params[0]);

	if (!c)
	{
		Log() << "TOPIC " << params[3] << " for nonexistent channel " << params[0];
		return true;
	}

	c->ChangeTopicInternal(setter, params.size() > 3 ? params[3] : "", topic_time);

	return true;
}

bool event_kick(const Anope::string &source, const std::vector<Anope::string> &params)
{
	if (params.size() > 2)
		do_kick(source, params[0], params[1], params[2]);
	return true;
}

bool event_sid(const Anope::string &source, const std::vector<Anope::string> &params)
{
	/* :42X SID trystan.nomadirc.net 2 43X :ircd-ratbox test server */
	Server *s = Server::Find(source);

	do_server(s->GetName(), params[0], Anope::string(params[1]).is_pos_number_only() ? convertTo<unsigned>(params[1]) : 0, params[3], params[2]);
	return true;
}

bool event_tmode(const Anope::string &source, const std::vector<Anope::string> &params)
{
	if (params[1][0] == '#' || params[1][0] == '&')
		do_cmode(source, params[0], params[1], params[2]);
	return true;
}

bool event_pass(const Anope::string &source, const std::vector<Anope::string> &params)
{
	TS6UPLINK = params[3];
	return true;
}

bool event_bmask(const Anope::string &source, const std::vector<Anope::string> &params)
{
	/* :42X BMASK 1106409026 #ircops b :*!*@*.aol.com */
	/*            0          1       2  3             */
	Channel *c = findchan(params[1]);

	if (c)
	{
		ChannelMode *ban = ModeManager::FindChannelModeByName(CMODE_BAN),
			*except = ModeManager::FindChannelModeByName(CMODE_EXCEPT),
			*invex = ModeManager::FindChannelModeByName(CMODE_INVITEOVERRIDE);
		Anope::string bans = params[3];
		int count = myNumToken(bans, ' '), i;
		for (i = 0; i <= count - 1; ++i)
		{
			Anope::string b = myStrGetToken(bans, ' ', i);
			if (ban && params[2].equals_cs("b"))
				c->SetModeInternal(ban, b);
			else if (except && params[2].equals_cs("e"))
				c->SetModeInternal(except, b);
			if (invex && params[2].equals_cs("I"))
				c->SetModeInternal(invex, b);
		}
	}
	return true;
}

static void AddModes()
{
	/* Add user modes */
	ModeManager::AddUserMode(new UserMode(UMODE_ADMIN, "UMODE_ADMIN", 'a'));
	ModeManager::AddUserMode(new UserMode(UMODE_INVIS, "UMODE_INVIS", 'i'));
	ModeManager::AddUserMode(new UserMode(UMODE_OPER, "UMODE_OPER", 'o'));
	ModeManager::AddUserMode(new UserMode(UMODE_SNOMASK, "UMODE_SNOMASK", 's'));
	ModeManager::AddUserMode(new UserMode(UMODE_WALLOPS, "UMODE_WALLOPS", 'w'));

	/* b/e/I */
	ModeManager::AddChannelMode(new ChannelModeBan('b'));
	ModeManager::AddChannelMode(new ChannelModeExcept('e'));
	ModeManager::AddChannelMode(new ChannelModeInvex('I'));

	/* v/h/o/a/q */
	ModeManager::AddChannelMode(new ChannelModeStatus(CMODE_VOICE, "CMODE_VOICE", 'v', '+'));
	ModeManager::AddChannelMode(new ChannelModeStatus(CMODE_OP, "CMODE_OP", 'o', '@'));

	/* Add channel modes */
	ModeManager::AddChannelMode(new ChannelMode(CMODE_INVITE, "CMODE_INVITE", 'i'));
	ModeManager::AddChannelMode(new ChannelModeKey('k'));
	ModeManager::AddChannelMode(new ChannelModeParam(CMODE_LIMIT, "CMODE_LIMIT", 'l'));
	ModeManager::AddChannelMode(new ChannelMode(CMODE_MODERATED, "CMODE_MODERATED", 'm'));
	ModeManager::AddChannelMode(new ChannelMode(CMODE_NOEXTERNAL, "CMODE_NOEXTERNAL", 'n'));
	ModeManager::AddChannelMode(new ChannelMode(CMODE_PRIVATE, "CMODE_PRIVATE", 'p'));
	ModeManager::AddChannelMode(new ChannelMode(CMODE_SECRET, "CMODE_SECRET", 's'));
	ModeManager::AddChannelMode(new ChannelMode(CMODE_TOPIC, "CMODE_TOPIC", 't'));
}

class ProtoRatbox : public Module
{
	Message message_kick, message_tmode, message_bmask, message_pass, message_tb, message_sid;
	
	RatboxProto ircd_proto;
	RatboxIRCdMessage ircd_message;
 public:
	ProtoRatbox(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator),
		message_kick("KICK", event_kick), message_tmode("TMODE", event_tmode),
		message_bmask("BMASK", event_bmask), message_pass("PASS", event_pass),
		message_tb("TB", event_tburst), message_sid("SID", event_sid)
	{
		this->SetAuthor("Anope");
		this->SetType(PROTOCOL);

		CapabType c[] = { CAPAB_ZIP, CAPAB_TS5, CAPAB_QS, CAPAB_UID, CAPAB_KNOCK, CAPAB_TSMODE };
		for (unsigned i = 0; i < 6; ++i)
			Capab.SetFlag(c[i]);

		AddModes();

		pmodule_ircd_var(myIrcd);
		pmodule_ircd_proto(&this->ircd_proto);
		pmodule_ircd_message(&this->ircd_message);
	}
};

MODULE_INIT(ProtoRatbox)
