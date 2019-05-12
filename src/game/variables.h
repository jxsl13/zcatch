/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_VARIABLES_H
#define GAME_VARIABLES_H
#undef GAME_VARIABLES_H // this file will be included several times


// client
MACRO_CONFIG_INT(ClPredict, cl_predict, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Predict client movements")
MACRO_CONFIG_INT(ClNameplates, cl_nameplates, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show name plates")
MACRO_CONFIG_INT(ClNameplatesAlways, cl_nameplates_always, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Always show name plates disregarding of distance")
MACRO_CONFIG_INT(ClNameplatesTeamcolors, cl_nameplates_teamcolors, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Use team colors for name plates")
MACRO_CONFIG_INT(ClNameplatesSize, cl_nameplates_size, 50, 0, 100, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Size of the name plates from 0 to 100%")
MACRO_CONFIG_INT(ClAutoswitchWeapons, cl_autoswitch_weapons, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Auto switch weapon on pickup")

MACRO_CONFIG_INT(ClShowhud, cl_showhud, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show ingame HUD")
MACRO_CONFIG_INT(ClShowChatFriends, cl_show_chat_friends, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show only chat messages from friends")
MACRO_CONFIG_INT(ClShowfps, cl_showfps, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show ingame FPS counter")

MACRO_CONFIG_INT(ClAirjumpindicator, cl_airjumpindicator, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(ClThreadsoundloading, cl_threadsoundloading, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Load sound files threaded")

MACRO_CONFIG_INT(ClWarningTeambalance, cl_warning_teambalance, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Warn about team balance")

MACRO_CONFIG_INT(ClMouseDeadzone, cl_mouse_deadzone, 300, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(ClMouseFollowfactor, cl_mouse_followfactor, 60, 0, 200, CFGFLAG_CLIENT|CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(ClMouseMaxDistance, cl_mouse_max_distance, 800, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "")

MACRO_CONFIG_INT(EdShowkeys, ed_showkeys, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "")

//MACRO_CONFIG_INT(ClFlow, cl_flow, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "")

MACRO_CONFIG_INT(ClShowWelcome, cl_show_welcome, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "")
MACRO_CONFIG_INT(ClMotdTime, cl_motd_time, 10, 0, 100, CFGFLAG_CLIENT|CFGFLAG_SAVE, "How long to show the server message of the day")

MACRO_CONFIG_STR(ClVersionServer, cl_version_server, 100, "version.teeworlds.com", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Server to use to check for new versions")

MACRO_CONFIG_STR(ClLanguagefile, cl_languagefile, 255, "", CFGFLAG_CLIENT|CFGFLAG_SAVE, "What language file to use")

MACRO_CONFIG_INT(PlayerUseCustomColor, player_use_custom_color, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Toggles usage of custom colors")
MACRO_CONFIG_INT(PlayerColorBody, player_color_body, 65408, 0, 0xFFFFFF, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Player body color")
MACRO_CONFIG_INT(PlayerColorFeet, player_color_feet, 65408, 0, 0xFFFFFF, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Player feet color")
MACRO_CONFIG_STR(PlayerSkin, player_skin, 24, "default", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Player skin")

MACRO_CONFIG_INT(UiPage, ui_page, 6, 0, 10, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Interface page")
MACRO_CONFIG_INT(UiToolboxPage, ui_toolbox_page, 0, 0, 2, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Toolbox page")
MACRO_CONFIG_STR(UiServerAddress, ui_server_address, 64, "localhost:8303", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Interface server address")
MACRO_CONFIG_INT(UiScale, ui_scale, 100, 50, 150, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Interface scale")
MACRO_CONFIG_INT(UiMousesens, ui_mousesens, 100, 5, 100000, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Mouse sensitivity for menus/editor")

MACRO_CONFIG_INT(UiColorHue, ui_color_hue, 160, 0, 255, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Interface color hue")
MACRO_CONFIG_INT(UiColorSat, ui_color_sat, 70, 0, 255, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Interface color saturation")
MACRO_CONFIG_INT(UiColorLht, ui_color_lht, 175, 0, 255, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Interface color lightness")
MACRO_CONFIG_INT(UiColorAlpha, ui_color_alpha, 228, 0, 255, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Interface alpha")

MACRO_CONFIG_INT(GfxNoclip, gfx_noclip, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Disable clipping")

// server
MACRO_CONFIG_INT(SvWarmup, sv_warmup, 0, 0, 0, CFGFLAG_SERVER, "Number of seconds to do warmup before round starts")
MACRO_CONFIG_INT(SvUnpauseTimer, sv_unpause_timer, 0, 0, 0, CFGFLAG_SERVER, "Number of seconds till the game continues")
MACRO_CONFIG_STR(SvMotd, sv_motd, 900, "", CFGFLAG_SERVER, "Message of the day to display for the clients")
MACRO_CONFIG_INT(SvTeamdamage, sv_teamdamage, 0, 0, 1, CFGFLAG_SERVER, "Team damage")
MACRO_CONFIG_STR(SvMaprotation, sv_maprotation, 768, "", CFGFLAG_SERVER, "Maps to rotate between")
MACRO_CONFIG_INT(SvRoundsPerMap, sv_rounds_per_map, 1, 1, 100, CFGFLAG_SERVER, "Number of rounds on each map before rotating")
MACRO_CONFIG_INT(SvRoundSwap, sv_round_swap, 1, 0, 1, CFGFLAG_SERVER, "Swap teams between rounds")
MACRO_CONFIG_INT(SvPowerups, sv_powerups, 1, 0, 1, CFGFLAG_SERVER, "Allow powerups like ninja")
MACRO_CONFIG_INT(SvScorelimit, sv_scorelimit, 0, 0, 1000, CFGFLAG_SERVER, "Score limit (0 disables)")
MACRO_CONFIG_INT(SvTimelimit, sv_timelimit, 0, 0, 1000, CFGFLAG_SERVER, "Time limit in minutes (0 disables)")
MACRO_CONFIG_STR(SvGametype, sv_gametype, 32, "dm", CFGFLAG_SERVER, "Game type (dm, tdm, ctf)")
MACRO_CONFIG_INT(SvTournamentMode, sv_tournament_mode, 0, 0, 1, CFGFLAG_SERVER, "Tournament mode. When enabled, players joins the server as spectator")
MACRO_CONFIG_INT(SvSpamprotection, sv_spamprotection, 1, 0, 1, CFGFLAG_SERVER, "Spam protection")

MACRO_CONFIG_INT(SvRespawnDelayTDM, sv_respawn_delay_tdm, 3, 0, 10, CFGFLAG_SERVER, "Time needed to respawn after death in tdm gametype")

MACRO_CONFIG_INT(SvSpectatorSlots, sv_spectator_slots, 0, 0, MAX_CLIENTS, CFGFLAG_SERVER, "Number of slots to reserve for spectators")
MACRO_CONFIG_INT(SvTeambalanceTime, sv_teambalance_time, 1, 0, 1000, CFGFLAG_SERVER, "How many minutes to wait before autobalancing teams")
MACRO_CONFIG_INT(SvInactiveKickTime, sv_inactivekick_time, 3, 0, 1000, CFGFLAG_SERVER, "How many minutes to wait before taking care of inactive players")
MACRO_CONFIG_INT(SvInactiveKick, sv_inactivekick, 1, 0, 2, CFGFLAG_SERVER, "How to deal with inactive players (0=move to spectator, 1=move to free spectator slot/kick, 2=kick)")

MACRO_CONFIG_INT(SvStrictSpectateMode, sv_strict_spectate_mode, 0, 0, 1, CFGFLAG_SERVER, "Restricts information in spectator mode")
MACRO_CONFIG_INT(SvVoteSpectate, sv_vote_spectate, 1, 0, 1, CFGFLAG_SERVER, "Allow voting to move players to spectators")
MACRO_CONFIG_INT(SvVoteSpectateRejoindelay, sv_vote_spectate_rejoindelay, 3, 0, 1000, CFGFLAG_SERVER, "How many minutes to wait before a player can rejoin after being moved to spectators by vote")
MACRO_CONFIG_INT(SvVoteKick, sv_vote_kick, 1, 0, 1, CFGFLAG_SERVER, "Allow voting to kick players")
MACRO_CONFIG_INT(SvVoteKickMin, sv_vote_kick_min, 0, 0, MAX_CLIENTS, CFGFLAG_SERVER, "Minimum number of players required to start a kick vote")
MACRO_CONFIG_INT(SvVoteKickBantime, sv_vote_kick_bantime, 5, 0, 1440, CFGFLAG_SERVER, "The time to ban a player if kicked by vote. 0 makes it just use kick")

// debug
#ifdef CONF_DEBUG // this one can crash the server if not used correctly
	MACRO_CONFIG_INT(DbgDummies, dbg_dummies, 0, 0, 15, CFGFLAG_SERVER, "")
#endif

MACRO_CONFIG_INT(DbgFocus, dbg_focus, 0, 0, 1, CFGFLAG_CLIENT, "")
MACRO_CONFIG_INT(DbgTuning, dbg_tuning, 0, 0, 1, CFGFLAG_CLIENT, "")

//zCatch:
MACRO_CONFIG_INT(SvMode, sv_mode, 1, 1, 5, CFGFLAG_SERVER, "1 - Instagib; 2 - Everything; 3 - Hammerparty; 4 - Grenade; 5 - Ninja")
MACRO_CONFIG_INT(SvAllowJoin, sv_allow_join, 2, 1, 2, CFGFLAG_SERVER, "Allow new Players to join without waiting for the next round")
//1 = Allowed to join; 2 = Will join when person with the most kills die
MACRO_CONFIG_INT(SvColorIndicator, sv_color_indicator, 1, 0, 1, CFGFLAG_SERVER, "Color tees apropriate to the number of currently caught players")
MACRO_CONFIG_INT(SvBonus, sv_bonus, 0, 0, 100, CFGFLAG_SERVER, "Give the last player extra points")
MACRO_CONFIG_INT(SvLaserjumps, sv_laserjumps, 0, 0, 1, CFGFLAG_SERVER, "Use laserjumps - on a laser bounce a explosion will occur which takes no damage")

MACRO_CONFIG_INT(SvChatValue, sv_chat_value, 250, 100, 1000, CFGFLAG_SERVER, "A value which is added on each message and decreased on each tick")
MACRO_CONFIG_INT(SvChatThreshold, sv_chat_threshold, 1000, 250, 10000, CFGFLAG_SERVER, "If this threshold will exceed by too many messages the player will be muted")
MACRO_CONFIG_INT(SvMuteDuration, sv_mute_duration, 60, 0, 3600, CFGFLAG_SERVER, "How long the player will be muted (in seconds)")

MACRO_CONFIG_INT(SvAllowRelease, sv_allow_release, 1, 0, 1, CFGFLAG_SERVER, "Allow players to release caught victims.")

MACRO_CONFIG_INT(SvAnticamper, sv_anticamper, 2, 0, 2, CFGFLAG_SERVER, "0 disables, 1 enables anticamper in all modes and 2 only in Instagib")
MACRO_CONFIG_INT(SvAnticamperFreeze, sv_anticamper_freeze, 7, 0, 15, CFGFLAG_SERVER, "If a player should freeze on camping (and how long) or die")
MACRO_CONFIG_INT(SvAnticamperTime, sv_anticamper_time, 10, 5, 120, CFGFLAG_SERVER, "How long to wait till the player dies/freezes")
MACRO_CONFIG_INT(SvAnticamperRange, sv_anticamper_range, 200, 0, 1000, CFGFLAG_SERVER, "Distance how far away the player must move to escape anticamper")

MACRO_CONFIG_INT(SvGrenadeMinDamage, sv_grenade_min_damage, 4, 1, 6, CFGFLAG_SERVER, "How much damage the grenade must do to kill the player (depends how far away it explodes)")
MACRO_CONFIG_INT(SvGrenadeEndlessAmmo, sv_grenade_endless_ammo, 1, 0, 1, CFGFLAG_SERVER, "Endless ammo for grenade (only mode 4). If not zero, set sv_grenade_bullets for the number of bullets")
MACRO_CONFIG_INT(SvWeaponsAmmo, sv_weapons_ammo, 4, 1, 10, CFGFLAG_SERVER, "Default amount of ammo for all weapons in mode 2 or grenade in mode 4. Your ammo will regenerate after some while")

MACRO_CONFIG_INT(SvVoteForceReason, sv_vote_forcereason, 1, 0, 1, CFGFLAG_SERVER, "Allow only votes with a reason (except settings)")
MACRO_CONFIG_INT(SvSuicideTime, sv_suicide_time, 15, 0, 60, CFGFLAG_SERVER, "Minimum time between suicides. 0 to forbid suicides completely")
MACRO_CONFIG_INT(SvKillPenalty, sv_kill_penalty, 5, 0, 50, CFGFLAG_SERVER, "The amount of points which the score will be decreased on each suicide")

// zCatch/TeeVi
MACRO_CONFIG_INT(SvLastStandingPlayers, sv_last_standing_players, 5, 2, 16, CFGFLAG_SERVER, "How many players are needed to have last standing rounds")
MACRO_CONFIG_INT(SvBotDetection, sv_bot_detection, 0, 0, 3, CFGFLAG_SERVER, "Bot detection (0=off, 1=fast aim, 2=follow, 3=all)")
MACRO_CONFIG_INT(SvRanking, sv_ranking, 1, 0, 1, CFGFLAG_SERVER, "Ranking system (0=off, 1=sqlite)")
MACRO_CONFIG_STR(SvRankingFile, sv_ranking_file, 255, "ranking.db", CFGFLAG_SERVER, "File in which the ranking and scores are saved.")
MACRO_CONFIG_INT(SvTimePlayedToSeeRank, sv_ranking_time_played_condition, 0, 86400, 300, CFGFLAG_SERVER, "The amount of seconds a player without any wins has to have played on the server in order to see their own rank. Default 600 seconds = 5 minutes.")
MACRO_CONFIG_INT(SvAllowHardMode, sv_allow_hard_mode, 0, 0, 2, CFGFLAG_SERVER, "Allow players to go into hard mode")
#endif
