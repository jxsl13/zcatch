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
MACRO_CONFIG_INT(ClFilterchat, cl_filterchat, 0, 0, 2, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show chat messages from: 0=all, 1=friends only, 2=no one")
MACRO_CONFIG_INT(ClShowsocial, cl_showsocial, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show social data like names, clans, chat etc.")
MACRO_CONFIG_INT(ClChatSound, cl_chatsound, 6, 0, 6, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Enable chat sound for 0=no msgs 1=whisper only 2=tagged 3=(1) & (2) 4=(3) & server msg 5=(4) & clientside msgs 6=all chat msgs")
MACRO_CONFIG_INT(ClShowfps, cl_showfps, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show ingame FPS counter")

MACRO_CONFIG_INT(ClAirjumpindicator, cl_airjumpindicator, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show double jump indicator")

MACRO_CONFIG_INT(ClWarningTeambalance, cl_warning_teambalance, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Warn about team balance")

MACRO_CONFIG_INT(ClDynamicCamera, cl_dynamic_camera, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Switches camera mode. 0=static camera, 1=dynamic camera")
MACRO_CONFIG_INT(ClMouseDeadzone, cl_mouse_deadzone, 300, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Zone that doesn't trigger the dynamic camera")
MACRO_CONFIG_INT(ClMouseFollowfactor, cl_mouse_followfactor, 60, 0, 200, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Trigger amount for the dynamic camera")
MACRO_CONFIG_INT(ClMouseMaxDistanceDynamic, cl_mouse_max_distance_dynamic, 1000, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Mouse max distance, in dynamic camera mode")
MACRO_CONFIG_INT(ClMouseMaxDistanceStatic, cl_mouse_max_distance_static, 400, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Mouse max distance, in static camera mode")

MACRO_CONFIG_INT(ClCustomizeSkin, cl_customize_skin, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Use a customized skin")

MACRO_CONFIG_INT(ClShowUserId, cl_show_user_id, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show the ID for every user")

MACRO_CONFIG_INT(EdZoomTarget, ed_zoom_target, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Zoom to the current mouse target")
MACRO_CONFIG_INT(EdShowkeys, ed_showkeys, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Editor shows which keys are pressed")
MACRO_CONFIG_INT(EdColorGridInner, ed_color_grid_inner, 0xFFFFFF26, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Color inner grid")
MACRO_CONFIG_INT(EdColorGridOuter, ed_color_grid_outer, 0xFF4C4C4C, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Color outer grid")
MACRO_CONFIG_INT(EdColorQuadPoint, ed_color_quad_point, 0xFF0000FF, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Color of quad points")
MACRO_CONFIG_INT(EdColorQuadPointHover, ed_color_quad_point_hover, 0xFFFFFFFF, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Color of quad points when hovering over with the mouse cursor")
MACRO_CONFIG_INT(EdColorQuadPointActive, ed_color_quad_point_active, 0xFFFFFFFF, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Color of active quad points")
MACRO_CONFIG_INT(EdColorQuadPivot, ed_color_quad_pivot, 0x00FF00FF, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Color of the quad pivot")
MACRO_CONFIG_INT(EdColorQuadPivotHover, ed_color_quad_pivot_hover, 0xFFFFFFFF, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Color of the quad pivot when hovering over with the mouse cursor")
MACRO_CONFIG_INT(EdColorQuadPivotActive, ed_color_quad_pivot_active, 0xFFFFFFFF, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Color of the active quad pivot")
MACRO_CONFIG_INT(EdColorSelectionQuad, ed_color_selection_quad, 0xFFFFFFFF, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Color of the selection area for a quad")
MACRO_CONFIG_INT(EdColorSelectionTile, ed_color_selection_tile, 0xFFFFFF66, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Color of the selection area for a tile")

//MACRO_CONFIG_INT(ClFlow, cl_flow, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "")

MACRO_CONFIG_INT(ClShowWelcome, cl_show_welcome, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show initial set-up dialog")
MACRO_CONFIG_INT(ClMotdTime, cl_motd_time, 10, 0, 100, CFGFLAG_CLIENT|CFGFLAG_SAVE, "How long to show the server message of the day")
MACRO_CONFIG_INT(ClShowXmasHats, cl_show_xmas_hats, 1, 0, 2, CFGFLAG_CLIENT|CFGFLAG_SAVE, "0=never, 1=during christmas, 2=always")
MACRO_CONFIG_INT(ClShowEasterEggs, cl_show_easter_eggs, 1, 0, 2, CFGFLAG_CLIENT|CFGFLAG_SAVE, "0=never, 1=during easter, 2=always")

MACRO_CONFIG_STR(ClVersionServer, cl_version_server, 100, "version.teeworlds.com", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Server to use to check for new versions")

MACRO_CONFIG_STR(ClFontfile, cl_fontfile, 255, "DejaVuSans.ttf", CFGFLAG_CLIENT|CFGFLAG_SAVE, "What font file to use")
MACRO_CONFIG_STR(ClLanguagefile, cl_languagefile, 255, "", CFGFLAG_CLIENT|CFGFLAG_SAVE, "What language file to use")

MACRO_CONFIG_INT(PlayerColorBody, player_color_body, 0x1B6F74, 0, 0xFFFFFF, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Player body color")
MACRO_CONFIG_INT(PlayerColorMarking, player_color_marking, 0xFF0000FF, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Player marking color")
MACRO_CONFIG_INT(PlayerColorDecoration, player_color_decoration, 0x1B6F74, 0, 0xFFFFFF, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Player decoration color")
MACRO_CONFIG_INT(PlayerColorHands, player_color_hands, 0x1B759E, 0, 0xFFFFFF, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Player hands color")
MACRO_CONFIG_INT(PlayerColorFeet, player_color_feet, 0x1C873E, 0, 0xFFFFFF, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Player feet color")
MACRO_CONFIG_INT(PlayerColorEyes, player_color_eyes, 0x0000FF, 0, 0xFFFFFF, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Player eyes color")
MACRO_CONFIG_INT(PlayerUseCustomColorBody, player_use_custom_color_body, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Toggles usage of custom colors for body")
MACRO_CONFIG_INT(PlayerUseCustomColorMarking, player_use_custom_color_marking, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Toggles usage of custom colors for marking")
MACRO_CONFIG_INT(PlayerUseCustomColorDecoration, player_use_custom_color_decoration, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Toggles usage of custom colors for decoration")
MACRO_CONFIG_INT(PlayerUseCustomColorHands, player_use_custom_color_hands, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Toggles usage of custom colors for hands")
MACRO_CONFIG_INT(PlayerUseCustomColorFeet, player_use_custom_color_feet, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Toggles usage of custom colors for feet")
MACRO_CONFIG_INT(PlayerUseCustomColorEyes, player_use_custom_color_eyes, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Toggles usage of custom colors for eyes")
MACRO_CONFIG_STR(PlayerSkin, player_skin, 24, "default", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Player skin")
MACRO_CONFIG_STR(PlayerSkinBody, player_skin_body, 24, "standard", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Player skin body")
MACRO_CONFIG_STR(PlayerSkinMarking, player_skin_marking, 24, "", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Player skin marking")
MACRO_CONFIG_STR(PlayerSkinDecoration, player_skin_decoration, 24, "", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Player skin decoration")
MACRO_CONFIG_STR(PlayerSkinHands, player_skin_hands, 24, "standard", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Player skin hands")
MACRO_CONFIG_STR(PlayerSkinFeet, player_skin_feet, 24, "standard", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Player skin feet")
MACRO_CONFIG_STR(PlayerSkinEyes, player_skin_eyes, 24, "standard", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Player skin eyes")

MACRO_CONFIG_INT(UiBrowserPage, ui_browser_page, 5, 5, 8, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Interface serverbrowser page")
MACRO_CONFIG_INT(UiSettingsPage, ui_settings_page, 0, 0, 5, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Interface settings page")
MACRO_CONFIG_STR(UiServerAddress, ui_server_address, 64, "localhost:8303", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Interface server address")
MACRO_CONFIG_INT(UiMousesens, ui_mousesens, 100, 1, 100000, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Mouse sensitivity for menus/editor")
MACRO_CONFIG_INT(UiAutoswitchInfotab, ui_autoswitch_infotab, 1, 0, 1, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Switch to the info tab when clicking on a server")
MACRO_CONFIG_INT(UiWideview, ui_wideview, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Extended menus GUI")

MACRO_CONFIG_INT(GfxNoclip, gfx_noclip, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Disable clipping")

MACRO_CONFIG_STR(ClMenuMap, cl_menu_map, 64, "winter", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Background map in the menu")
MACRO_CONFIG_INT(ClShowMenuMap, cl_show_menu_map, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Display background map in the menu")
MACRO_CONFIG_INT(ClMenuAlpha, cl_menu_alpha, 25, 0, 75, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Transparency of the menu background")
MACRO_CONFIG_INT(ClRotationRadius, cl_rotation_radius, 30, 1, 500, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Menu camera rotation radius")
MACRO_CONFIG_INT(ClRotationSpeed, cl_rotation_speed, 40, 1, 120, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Menu camera rotations in seconds")
MACRO_CONFIG_INT(ClCameraSpeed, cl_camera_speed, 5, 1, 10, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Menu camera speed")

MACRO_CONFIG_INT(ClShowStartMenuImages, cl_show_start_menu_images, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show start menu images")
MACRO_CONFIG_INT(ClSkipStartMenu, cl_skip_start_menu, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Skip the start menu")

MACRO_CONFIG_INT(ClStatboardInfos, cl_statboard_infos, 1259, 1, 2047, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Mask of info to display on the global statboard")

MACRO_CONFIG_INT(ClLastVersionPlayed, cl_last_version_played, 0x0703, 0, 0, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Last version of the game that was played")

// server
MACRO_CONFIG_INT(SvWarmup, sv_warmup, 0, -1, 1000, CFGFLAG_SAVE|CFGFLAG_SERVER, "Number of seconds to do warmup before match starts (0 disables, -1 all players ready)")
MACRO_CONFIG_INT(SvCountdown, sv_countdown, 0, -1, 1000, CFGFLAG_SAVE|CFGFLAG_SERVER, "Number of seconds to freeze the game in a countdown before match starts (0 enables only for survival gamemodes, -1 disables)")
MACRO_CONFIG_STR(SvMotd, sv_motd, 900, "", CFGFLAG_SAVE|CFGFLAG_SERVER, "Message of the day to display for the clients")
MACRO_CONFIG_INT(SvTeamdamage, sv_teamdamage, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_SERVER, "Team damage")
MACRO_CONFIG_STR(SvMaprotation, sv_maprotation, 768, "", CFGFLAG_SAVE|CFGFLAG_SERVER, "Maps to rotate between")
MACRO_CONFIG_INT(SvMatchesPerMap, sv_matches_per_map, 1, 1, 100, CFGFLAG_SAVE|CFGFLAG_SERVER, "Number of matches on each map before rotating")
MACRO_CONFIG_INT(SvMatchSwap, sv_match_swap, 1, 0, 1, CFGFLAG_SAVE|CFGFLAG_SERVER, "Swap teams between matches")
MACRO_CONFIG_INT(SvPowerups, sv_powerups, 1, 0, 1, CFGFLAG_SAVE|CFGFLAG_SERVER, "Allow powerups like ninja")
MACRO_CONFIG_INT(SvScorelimit, sv_scorelimit, 0, 0, 1000, CFGFLAG_SAVE|CFGFLAG_SERVER, "Score limit (0 disables)")
MACRO_CONFIG_INT(SvTimelimit, sv_timelimit, 0, 0, 1000, CFGFLAG_SAVE|CFGFLAG_SERVER, "Time limit in minutes (0 disables)")
MACRO_CONFIG_STR(SvGametype, sv_gametype, 32, "zCatch", CFGFLAG_SAVE|CFGFLAG_SERVER, "Game type (dm, tdm, ctf, lms, lts, zCatch)")
MACRO_CONFIG_INT(SvTournamentMode, sv_tournament_mode, 0, 0, 2, CFGFLAG_SAVE|CFGFLAG_SERVER, "Tournament mode. When enabled, players joins the server as spectator (2=additional restricted spectator chat)")
MACRO_CONFIG_INT(SvPlayerReadyMode, sv_player_ready_mode, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_SERVER, "When enabled, players can pause/unpause the game and start the game on warmup via their ready state")
MACRO_CONFIG_INT(SvSpamprotection, sv_spamprotection, 1, 0, 1, CFGFLAG_SAVE|CFGFLAG_SERVER, "Spam protection")

MACRO_CONFIG_INT(SvRespawnDelayTDM, sv_respawn_delay_tdm, 3, 0, 10, CFGFLAG_SAVE|CFGFLAG_SERVER, "Time needed to respawn after death in tdm gametype")

MACRO_CONFIG_INT(SvPlayerSlots, sv_player_slots, 16, 0, MAX_PLAYERS, CFGFLAG_SAVE|CFGFLAG_SERVER, "Number of slots to reserve for players")
MACRO_CONFIG_INT(SvSkillLevel, sv_skill_level, 1, SERVERINFO_LEVEL_MIN, SERVERINFO_LEVEL_MAX, CFGFLAG_SAVE|CFGFLAG_SERVER, "Supposed player skill level")
MACRO_CONFIG_INT(SvTeambalanceTime, sv_teambalance_time, 1, 0, 1000, CFGFLAG_SAVE|CFGFLAG_SERVER, "How many minutes to wait before autobalancing teams")
MACRO_CONFIG_INT(SvInactiveKickTime, sv_inactivekick_time, 2, 0, 1000, CFGFLAG_SAVE|CFGFLAG_SERVER, "How many minutes to wait before taking care of inactive clients")
MACRO_CONFIG_INT(SvInactiveKick, sv_inactivekick, 1, 1, 3, CFGFLAG_SAVE|CFGFLAG_SERVER, "How to deal with inactive clients (1=move player to spectator, 2=move to free spectator slot/kick, 3=kick)")
MACRO_CONFIG_INT(SvInactiveKickSpec, sv_inactivekick_spec, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_SERVER, "Kick inactive spectators")

MACRO_CONFIG_INT(SvSilentSpectatorMode, sv_silent_spectator_mode, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_SERVER, "Mute join/leave message of spectator")

MACRO_CONFIG_INT(SvStrictSpectateMode, sv_strict_spectate_mode, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_SERVER, "Restricts information in spectator mode")
MACRO_CONFIG_INT(SvVoteSpectate, sv_vote_spectate, 1, 0, 1, CFGFLAG_SAVE|CFGFLAG_SERVER, "Allow voting to move players to spectators")
MACRO_CONFIG_INT(SvVoteSpectateRejoindelay, sv_vote_spectate_rejoindelay, 3, 0, 1440, CFGFLAG_SAVE|CFGFLAG_SERVER, "How many minutes to wait before a player can rejoin after being moved to spectators by vote")
MACRO_CONFIG_INT(SvVoteKick, sv_vote_kick, 1, 0, 1, CFGFLAG_SAVE|CFGFLAG_SERVER, "Allow voting to kick players")
MACRO_CONFIG_INT(SvVoteKickMin, sv_vote_kick_min, 0, 0, MAX_CLIENTS, CFGFLAG_SAVE|CFGFLAG_SERVER, "Minimum number of players required to start a kick vote")
MACRO_CONFIG_INT(SvVoteKickBantime, sv_vote_kick_bantime, 5, 0, 1440, CFGFLAG_SAVE|CFGFLAG_SERVER, "The time to ban a player if kicked by vote. 0 makes it just use kick")

//zcatch
MACRO_CONFIG_INT(SvPlayersToStartRound, sv_players_to_start_round, 5, 0, MAX_CLIENTS, CFGFLAG_SAVE|CFGFLAG_SERVER, "Players needed to end a round. Default: 5")
MACRO_CONFIG_INT(SvWeaponMode, sv_weapon_mode, 3, 0, 6, CFGFLAG_SAVE|CFGFLAG_SERVER, "0: Hammer, 1: Gun, 2: Shotgun, 3:Grenade Launcher, 4: Laser Rifle, 5:Ninja, 6: Everything")

MACRO_CONFIG_INT(SvKickvoteOnWarmup, sv_kickvote_on_warmup, 0, 0, 1, CFGFLAG_SAVE|CFGFLAG_SERVER, "Enable kickvotes while warmup is being played.")


MACRO_CONFIG_INT(SvSuicidePenalty, sv_suicide_penalty, 5, 0, 50, CFGFLAG_SERVER, "How many deaths does one suicide cause.")
MACRO_CONFIG_INT(SvSuicideCooldown, sv_suicide_cooldown, 15, 3, 60, CFGFLAG_SERVER, "How long a player needs to wait before it is able to do anoter suicide.")
MACRO_CONFIG_INT(SvKillScore, sv_kill_score, 1, 0, 50, CFGFLAG_SERVER, "How much a kill worth")
MACRO_CONFIG_INT(SvDeathScore, sv_death_score, 1, 0, 50, CFGFLAG_SERVER, "How much a death cost")
MACRO_CONFIG_INT(SvKillIncreasing, sv_kill_increasing, 1, 0,1, CFGFLAG_SERVER, "How important a kill is")

MACRO_CONFIG_INT(SvGrenadeHitbox, sv_grenade_hitbox, 3, 1, 6, CFGFLAG_SERVER, "How large is the grenade hitbox(explosion radius).")
MACRO_CONFIG_INT(SvSprayProtectionRadius, sv_spray_protection_radius, 1000, 0, 10000, CFGFLAG_SERVER, "When a projectile is shot, this radius is checked, which players can take damage by the projectile.")

MACRO_CONFIG_INT(SvAnticamper, sv_anticamper, 1, 0, 1, CFGFLAG_SERVER, "0 disables, 1 enables anticamper.")
MACRO_CONFIG_INT(SvAnticamperFreeze, sv_anticamper_freeze, 7, 0, 15, CFGFLAG_SERVER, "If a player should freeze on camping (and how many seconds) or die")
MACRO_CONFIG_INT(SvAnticamperTime, sv_anticamper_time, 10, 5, 120, CFGFLAG_SERVER, "How many seconds to wait till the player dies/freezes")
MACRO_CONFIG_INT(SvAnticamperRange, sv_anticamper_range, 200, 0, 1000, CFGFLAG_SERVER, "Distance how far away the player must move to escape anticamper")

MACRO_CONFIG_INT(SvMapChangeCooldown, sv_map_change_cooldown, 5, 0, 1440, CFGFLAG_SERVER, "How many minutes to wait before one can change the map again.")
MACRO_CONFIG_INT(SvHideAdmins, sv_hide_admins, 1, 0, 1, CFGFLAG_SAVE|CFGFLAG_SERVER, "Hide the presence of admins. Don't send info to clients.")


MACRO_CONFIG_INT(SvWarmupAutostart, sv_warmup_autostart, 1, 0, 1, CFGFLAG_SERVER, "Whether warmup should automatically start if there are not enough players to end a round.")

MACRO_CONFIG_INT(SvChatValue, sv_chat_value, 250, 100, 1000, CFGFLAG_SERVER, "A value which is added on each message and decreased on each tick by one(50 ticks/seconds)")
MACRO_CONFIG_INT(SvChatThreshold, sv_chat_threshold, 1000, 250, 10000, CFGFLAG_SERVER, "If this threshold will exceed by too many messages the player will be muted")
MACRO_CONFIG_INT(SvMuteDuration, sv_mute_duration, 60, 0, 3600, CFGFLAG_SERVER, "How long the player will be muted (in seconds)")

MACRO_CONFIG_STR(SvDatabaseType, sv_db_type, 24, "", CFGFLAG_SERVER, "Can be either empty, 'redis' or 'sqlite'")
MACRO_CONFIG_STR(SvDatabaseHost, sv_db_host, 16, "127.0.0.1", CFGFLAG_SERVER, "Hostname of database(currently only DBs without a password)")
MACRO_CONFIG_INT(SvDatabasePort, sv_db_port, 6379, 1024, 65535, CFGFLAG_SERVER, "Port of the database")

MACRO_CONFIG_STR(SvSQLiteFilename, sv_db_sqlite_file, 256, "ranking.db", CFGFLAG_SERVER, "Relative path to the sqlite3 database(default: ranking.db).")

// beginner server stuff
MACRO_CONFIG_STR(SvBeginnerServerKickWarning, sv_beginner_srv_kick_warning, 512, "This is a beginner server, you are not allowed to play here.", CFGFLAG_SERVER, "Warning message when the countdown starts.")
MACRO_CONFIG_STR(SvBeginnerServerKickReason, sv_beginner_srv_kick_reason, 512, "You are not allowed to play on this server.", CFGFLAG_SERVER, "Kick reason, that's shown when a player is being automatically kicked.")
MACRO_CONFIG_INT(SvBeginnerServerRankLimit, sv_beginner_srv_rank_limit, 0, 0, 1048560, CFGFLAG_SERVER, "Rank above which players can join the server w/o being kicked(0 = disabled)")
MACRO_CONFIG_INT(SvBeginnerServerScoreLimit, sv_beginner_server_score_limit, 0, 0, 1048560, CFGFLAG_SERVER, "Score below which players are not able to join the server(0 = disabled)")
MACRO_CONFIG_INT(SvBeginnerServerKickTimeLimit, sv_beginner_server_kick_time_limit, 30, 0, 86400, CFGFLAG_SERVER, "How many seconds to wait before kicking a player that's not allowed to play on the server")


// debug
#ifdef CONF_DEBUG // this one can crash the server if not used correctly
	MACRO_CONFIG_INT(DbgDummies, dbg_dummies, 0, 0, MAX_CLIENTS, CFGFLAG_SERVER, "")
#endif

MACRO_CONFIG_INT(DbgFocus, dbg_focus, 0, 0, 1, CFGFLAG_CLIENT, "")
MACRO_CONFIG_INT(DbgTuning, dbg_tuning, 0, 0, 1, CFGFLAG_CLIENT, "")
#endif
