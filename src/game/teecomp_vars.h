
MACRO_CONFIG_INT(TcColoredTeesMethod, tc_colored_tees_method, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Enable enemy based skin colors")
MACRO_CONFIG_INT(TcDmColorsTeam1, tc_dm_colors_team1, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Use DM colors for red team/team mates")
MACRO_CONFIG_INT(TcDmColorsTeam2, tc_dm_colors_team2, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Use DM colors for blue team/enemies")
// MACRO_CONFIG_INT(TcColoredTeesTeam1, tc_colored_tees_team1, 16739179 -1, -1, 16777215, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Red team/team mates color") // TODO: move this hack to static vars
// MACRO_CONFIG_INT(TcColoredTeesTeam2, tc_colored_tees_team2, 7053311 -1, -1, 16777215, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Blue team/enemies color")
MACRO_CONFIG_INT(TcColoredTeesTeam1Hsl, tc_colored_tees_team1_hsl, 65390, -1, 16777215, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Red team/team mates color (HSL)")
MACRO_CONFIG_INT(TcColoredTeesTeam2Hsl, tc_colored_tees_team2_hsl, 10223470, -1, 16777215, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Red team/team mates color (HSL)")

MACRO_CONFIG_INT(TcForcedSkinsMethod, tc_forced_skins_method, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Enable enemy based forced skins")
MACRO_CONFIG_INT(TcForceSkinTeam1, tc_force_skin_team1, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Force a skin for red team/your team/DM matchs")
MACRO_CONFIG_INT(TcForceSkinTeam2, tc_force_skin_team2, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Force a skin for blue team/opponents")
MACRO_CONFIG_STR(TcForcedSkin1, tc_forced_skin1, 64, "default", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Forced skin for red/mates/DM matchs")
MACRO_CONFIG_STR(TcForcedSkin2, tc_forced_skin2, 64, "default", CFGFLAG_CLIENT|CFGFLAG_SAVE, "Forced skin for blue/opponents")

MACRO_CONFIG_INT(TcNameplateScore, tc_nameplate_score, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Display score on name plates")

MACRO_CONFIG_INT(TcHudMatch, tc_hud_match, 1, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Make HUD match tees' colors")

MACRO_CONFIG_INT(TcLaserColorInner, tc_laser_color_inner, 8355839, 0, 16777215, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Inner color of laser")
MACRO_CONFIG_INT(TcLaserColorOuter, tc_laser_color_outer, 1250112, 0, 16777215, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Outer color of laser")

// not implemented
//MACRO_CONFIG_INT(TcStatScreenshot, tc_stat_screenshot, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Automatically take game over statboard screenshot")
// MACRO_CONFIG_INT(TcStatScreenshotMax, tc_stat_screenshot_max, 10, 0, 1000, CFGFLAG_SAVE|CFGFLAG_CLIENT, "Maximum number of automatically created statboard screenshots (0 = no limit)")

MACRO_CONFIG_INT(TcColoredFlags, tc_colored_flags, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Make flags colors match tees colors")
MACRO_CONFIG_INT(TcHideCarrying, tc_hide_carrying, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Hide the flag if you're carrying it")

// ported from 0.5
MACRO_CONFIG_INT(TcSpeedmeter, tc_speedmeter, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Display speed meter")
MACRO_CONFIG_INT(TcSpeedmeterAccel, tc_speedmeter_accel, 0, 0, 1, CFGFLAG_CLIENT|CFGFLAG_SAVE, "Speed meter shows acceleration")