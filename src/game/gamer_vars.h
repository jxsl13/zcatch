// Tile modifiers
MACRO_CONFIG_INT(GfxGameTiles, gfx_game_tiles, 0, 0, 2, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show game tiles (2 = game tiles only)")

// Healthbar
MACRO_CONFIG_INT(GfxHealthBar, gfx_healthbar, 1, 0, 2, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Render healthbar (mode 1 or 2)")
MACRO_CONFIG_INT(GfxArmorUnderHealth, gfx_armor_under_health, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Render the armor bar under the health")
MACRO_CONFIG_INT(GfxHealthBarNumbers, gfx_healthbar_numbers, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Render numbers next to the healthbar")

// HUD related features
MACRO_CONFIG_INT(ClGcolor, cl_gcolor, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Gamer colors - show health as tee colors")
MACRO_CONFIG_INT(ClGhud, cl_ghud, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Gamer HUD")
MACRO_CONFIG_INT(ClNoHud, cl_nohud, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Remove HUD")
MACRO_CONFIG_INT(ClNoAmmoWarning, cl_noammo_warning, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Prints a red and yellow warning when you use all your ammo")

// Positions
// MACRO_CONFIG_STR(ClRedbaseMsg, cl_redbase_msg, 64, "I am at the RED BASE", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Message to send when being at the red base")
// MACRO_CONFIG_STR(ClBluebaseMsg, cl_bluebase_msg, 64, "I am at the BLUE BASE", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Message to send when being at the blue base")
// MACRO_CONFIG_STR(ClMiddleMsg, cl_middle_msg, 64, "I am at the MIDDLE", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Message to send when being at the middle")

// MACRO_CONFIG_INT(ClLocalPortLimit, cl_local_port_limit, 8320, 8304, 8400, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Maximum port the client is listening to when searching LAN servers")

// Race - old comment: this is useless now, should be removed
// MACRO_CONFIG_INT(ClRace, cl_race, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Enable race mode when a time broadcast is detected")
// MACRO_CONFIG_INT(ClRaceTextsize, cl_race_textsize, 100, 25, 400, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Race clock size")

// Sounds
MACRO_CONFIG_INT(ClGSound, cl_gsound, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Gamer sounds")
MACRO_CONFIG_INT(ClChatSound, cl_chat_sound, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Enable chat sounds")
MACRO_CONFIG_INT(ClAnnouncers, cl_announcers, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Announcers")
MACRO_CONFIG_INT(ClAnnouncersShadows, cl_announcers_shadows, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Announcers outline shadows")
MACRO_CONFIG_INT(ClAnnouncersLegend, cl_announcers_legend, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Legend under the announcers")

MACRO_CONFIG_INT(ClColorfulBrowser, cl_colorful_browser, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Colorful browser")

// Chat
// MACRO_CONFIG_INT(ClTextSize, cl_text_size, 100, 50, 200, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Chat size (%)")
MACRO_CONFIG_INT(ClTextColors, cl_text_colors, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Render text colors")
// MACRO_CONFIG_INT(ClArrows, cl_arrows, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Arrows")
// MACRO_CONFIG_INT(ClNoCustomForArrows, cl_no_custom_for_arrows, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Do not use custom team colors for arrows")

// Scoreboard
// MACRO_CONFIG_INT(ClShowSkins, cl_showskins, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show skin names on scoreboard")
// MACRO_CONFIG_INT(ClShowId, cl_showid, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show IDs on scoreboard")

// Client auto-reconizing
// MACRO_CONFIG_INT(ClSendClientInfo, cl_sendclientinfo, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Give other players the ability to see you are using the gamer")
// MACRO_CONFIG_INT(ClCountGamers, cl_count_gamers, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "")

// 	Spectating
MACRO_CONFIG_INT(GfxSpecZoom, gfx_spec_zoom, 100, 50, 500, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Spectator zoom")

// Stats
// MACRO_CONFIG_INT(ClRegisterStats, cl_register_stats, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Record statistics")
// MACRO_CONFIG_INT(ClRegisterStatsPure, cl_register_stats_pure, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Record statistics from pure games only")

// LAN test button
// MACRO_CONFIG_INT(ClShowLanTest, cl_show_lan_test, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Show the LAN test button in the ingame menus")
