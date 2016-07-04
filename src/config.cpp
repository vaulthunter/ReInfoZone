#include "precompiled.h"

config_t g_config;

cvar_t cv_iz_zone_leave_time = {"iz_zone_leave_time", "0.5", FCVAR_EXTDLL, 0.5, NULL};
cvar_t cv_iz_use_interval = {"iz_use_interval", "3.0", FCVAR_EXTDLL, 3.0, NULL};
cvar_t cv_iz_smooth_positioning = {"iz_smooth_positioning", "1", FCVAR_EXTDLL, 1.0, NULL};
cvar_t cv_iz_max_aim_angle = {"iz_max_aim_angle", "3.0", FCVAR_EXTDLL, 3.0, NULL};
cvar_t cv_iz_item_max_radius = {"iz_item_max_radius", "48.0", FCVAR_EXTDLL, 48.0, NULL};

cvar_t* iz_zone_leave_time;
cvar_t* iz_use_interval;
cvar_t* iz_smooth_positioning;
cvar_t* iz_max_aim_angle;
cvar_t* iz_item_max_radius;

enum section_e
{
	st_unknown,
	st_main,
	st_hud,
	st_lang,
	st_radio
};

const char* sections[] =
{
	"",
	"MAIN",
	"HUD",
	"LANG",
	"RADIO",
};

const char* section_default_format[] =
{
	"",
	"param value",
	"[en] [ru] xcoord ycoord red green blue",
	"phrase [en] [ru]",
	"menuid radio [en] [ru]"
};

enum data_type_e
{
	dt_unknown,
	dt_param,
	dt_value,
	dt_origin,
	dt_mins,
	dt_maxs,
	dt_lcorner,
	dt_hcorner,
	dt_menuid,
	dt_radio,
	dt_red,
	dt_green,
	dt_blue,
	dt_xcoord,
	dt_ycoord,
	dt_phrase,

	dt_translation
};

const char* data_types[] =
{
	"",
	"param",
	"value",
	"origin",
	"mins",
	"maxs",
	"lcorner",
	"hcorner",
	"menuid",
	"radio",
	"red",
	"green",
	"blue",
	"xcoord",
	"ycoord",
	"phrase"
};

section_e getSection(const char* string)
{
	for (size_t i = 0; i < arraysize(sections); i++) {
		if (!strcmp(string, sections[i]))
			return section_e(i);
	}
	return st_unknown;
}

int getDataType(const char* string)
{
	for (size_t i = 0; i < arraysize(data_types); i++) {
		if (!strcmp(string, data_types[i]))
			return i;
	}
	return dt_unknown;
}

CFields::CFields(): m_count(0)
{
	
}

bool CFields::parseFormat(char* string)
{
	m_count = 0;

	char* argv[32];
	size_t argc = parse(string, argv, arraysize(argv));

	if (!argc || argc > arraysize(m_fields))
		return false;

	memset(m_fields, 0, sizeof m_fields);

	for (size_t i = 0; i < argc; i++) {
		char* arg = argv[i];

		if (arg[0] == '[') {
			if (!nullLast(arg, ']'))
				return false;

			m_fields[i].type = dt_translation;
			m_fields[i].int_number = g_lang.addLang(arg + 1).code;
		}
		else
			m_fields[i].type = getDataType(arg);
	}

	m_count = argc;
	return true;
}

size_t CFields::getCount() const
{
	return m_count;
}

field_data_t* CFields::getField(data_type_e type)
{
	for (size_t i = 0; i < m_count; i++) {
		if (m_fields[i].type == type)
			return &m_fields[i];
	}
	return nullptr;
}

field_data_t* CFields::getField(size_t index)
{
	return &m_fields[index];
}

size_t CFields::getTranslations(translation_t* translations, size_t max_count) const
{
	size_t translations_count = 0;
	for (size_t i = 0; i < m_count; i++) {
		if (m_fields[i].type != dt_translation)
			continue;

		auto t = &translations[translations_count++];
		t->lang = m_fields[i].int_number;
		t->text = m_fields[i].string;

		if (translations_count == max_count)
			break;
	}
	return translations_count;
}

bool parseVector(char* string, Vector& vec)
{
	char* argv[4];
	size_t argc = parse(string, argv, arraysize(argv), " \t;", false);

	if (argc == 3) {
		vec.x = strtod(argv[0], nullptr);
		vec.y = strtod(argv[1], nullptr);
		vec.z = strtod(argv[2], nullptr);
		return true;
	}

	return false;
}

bool parseLineToFields(char* line, CFields& fields, const char* file, size_t line_number)
{
	// format
	if (line[0] == '{') {
		line++;

		if (!nullLast(line, '}') || !fields.parseFormat(line)) {
			LCPrintf("Invalid format string at line %i in %s\n", line_number, file);
			return false;
		}

		return false; // no data
	}

	char* argv[32];
	size_t argc = parse(line, argv, arraysize(argv));

	if (!argc || argc != fields.getCount()) {
		LCPrintf("Invalid fields count at line %i in %s\n", line_number, file);
		return false;
	}

	// data
	for (size_t i = 0; i < fields.getCount(); i++) {
		auto field = fields.getField(i);

		switch (field->type) {
		case dt_origin:
		case dt_mins:
		case dt_maxs:
		case dt_lcorner:
		case dt_hcorner:
			if (!parseVector(argv[i], field->vec)) {
				LCPrintf("Invalid vector data in field %i at line %i in %s\n", i, line_number, file);
				return false;
			}
			break;

		case dt_param:
		case dt_value:
		case dt_radio:
		case dt_phrase:
		case dt_translation:
			strncpy(field->string, argv[i], sizeof field->string - 1);
			field->string[sizeof field->string - 1] = '\0';
			break;

		case dt_menuid:
		case dt_red:
		case dt_green:
		case dt_blue:
			field->int_number = strtol(argv[i], nullptr, 10);
			break;

		case dt_xcoord:
		case dt_ycoord:
			field->float_number = strtod(argv[i], nullptr);
			break;

		default:
		LCPrintf("Invalid field type %i (pos %i) at line %i in %s\n", field->type, i, line_number, file);
			return false;
		}
	}

	return true;
}

bool parseZonesConfig(const char* path, const char* file)
{
	FILE* fp = fopen(path, "rt");

	if (!fp)
		return false;

	char buf[2048];
	size_t line_number = 0;
	CFields fields;

	char format[] = "[en] origin mins maxs";
	fields.parseFormat(format);

	while (fgets(buf, sizeof buf, fp)) {
		char* line = trimbuf(buf);
		line_number++;

		if (line[0] == '\xEF' && line[1] == '\xBB' && line[2] == '\xBF') // BOM
			line += 3;

		if (!line[0])
			continue;

		if (!parseLineToFields(line, fields, file, line_number))
			continue;

		if (!fields.getCount())
			continue;

		auto origin = fields.getField(dt_origin);
		auto mins = fields.getField(dt_mins);
		auto maxs = fields.getField(dt_maxs);
		auto lcorner = fields.getField(dt_lcorner);
		auto hcorner = fields.getField(dt_hcorner);

		translation_t translations[16];
		size_t translations_count = fields.getTranslations(translations, arraysize(translations));

		if (!translations_count) {
			LCPrintf("Invalid zone without name at line %i in %s\n", line_number, file);
			continue;
		}

		if (origin && mins && maxs) {
			g_zoneManager.addZone(translations, translations_count, origin->vec, mins->vec, maxs->vec);
			continue;
		}

		if (lcorner && hcorner) {
			g_zoneManager.addZone(translations, translations_count, lcorner->vec, hcorner->vec);
			continue;
		}

		LCPrintf("Not enough coordinates to create zone at line %i in %s\n", line_number, file);
	}

	return true;
}

bool loadZonesConfig()
{
	char path[260], file[260];
	snprintf(file, sizeof file, "info_zone_%s.ini", STRING(gpGlobals->mapname));
	g_amxxapi.BuildPathnameR(path, sizeof path - 1, "%s/info_zone/%s", g_amxxapi.GetLocalInfo("amxx_datadir", "addons/amxmodx/data"), file);

	if (!parseZonesConfig(path, file)) {
		LCPrintf("Zones file %s not found\n", file);
		return false;
	}

	if (!g_zoneManager.getZonesCount()) {
		return false;
	}

	LCPrintf("Loaded %i zones for map %s\n", g_zoneManager.getZonesCount(), STRING(gpGlobals->mapname));
	return true;
}

bool parseParam(const char* param, const char* value)
{
	CPlayer::options_u opt;
	opt.integer = 0;
	size_t integer = atoi(value);

	if (!strcmp(param, "log_mode")) {
		g_config.logMode = integer;
		return true;
	}
	if (!strcmp(param, "bots_fix")) {
		g_config.botsFix = integer != 0;
		return true;
	}
	if (!strcmp(param, "default_hudpos")) {
		opt.hudpos = integer - 1;
		g_config.defaultOptions |= opt.integer;
		return true;
	}
	if (!strcmp(param, "default_block_radio")) {
		opt.block_radio = integer ? 1 : 0;
		g_config.defaultOptions |= opt.integer;
		return true;
	}
	if (!strcmp(param, "default_block_fith")) {
		opt.block_fith = integer ? 1 : 0;
		g_config.defaultOptions |= opt.integer;
		return true;
	}

	auto cvar = g_engfuncs.pfnCVarGetPointer(param);
	if (cvar) {
		g_engfuncs.pfnCvar_DirectSet(cvar, value);
		return true;
	}

	return false;
}

bool parseMainConfig(const char* path, const char* file)
{
	FILE* fp = fopen(path, "rt");

	if (!fp)
		return false;

	char buf[8192];
	size_t line_number = 0;
	section_e section = st_unknown;
	CFields fields;

	while (fgets(buf, sizeof buf, fp)) {
		char* line = trimbuf(buf);
		line_number++;

		if (line[0] == '\xEF' && line[1] == '\xBB' && line[2] == '\xBF') // BOM
			line += 3;

		if (!line[0] || line[0] == '#')
			continue;

		if (line[0] == '[') {
			line++;

			if (nullLast(line, ']'))
				section = getSection(line);

			if (section != st_unknown && section < arraysize(section_default_format)) {
				char temp[256];
				strcpy(temp, section_default_format[section]);
				fields.parseFormat(temp);
			}

			continue;
		}

		if (section == st_unknown)
			continue;

		if (!parseLineToFields(line, fields, file, line_number))
			continue;

		if (!fields.getCount())
			continue;

		translation_t translations[16];
		size_t translations_count = fields.getTranslations(translations, arraysize(translations));

		switch (section) {
			case st_main:
			{
				auto param = fields.getField(dt_param);
				auto value = fields.getField(dt_value);

				if (!param || !value || !parseParam(param->string, value->string))
					LCPrintf("Invalid or unknown parameter at line %i in %s\n", line_number, file);
				break;
			}

			case st_hud:
			{
				if (!translations_count) {
					LCPrintf("Invalid hud mode name at line %i in %s\n", line_number, file);
					break;
				}

				auto x = fields.getField(dt_xcoord);
				auto y = fields.getField(dt_ycoord);
				auto r = fields.getField(dt_red);
				auto g = fields.getField(dt_green);
				auto b = fields.getField(dt_blue);

				if (x && y && r && g && b)
					addHudparms(translations, translations_count, x->float_number, y->float_number, r->int_number, g->int_number, b->int_number);
				else
					LCPrintf("Invalid hud parameters at line %i in %s\n", line_number, file);
				break;
			}

			case st_lang:
			{
				if (!translations_count) {
					LCPrintf("Invalid phrase without translations at line %i in %s\n", line_number, file);
					break;
				}

				auto phrase = fields.getField(dt_phrase);
				if(!phrase) {
					LCPrintf("Invalid phrase at line %i in %s\n", line_number, file);
					break;
				}

				g_lang.addPhrase(phrase->string, translations, translations_count);
				break;
			}

			case st_radio:
			{
				if (!translations_count) {
					LCPrintf("Invalid radio without translations at line %i in %s\n", line_number, file);
					break;
				}

				auto menuid = fields.getField(dt_menuid);
				if (!menuid) {
					LCPrintf("Invalid radio menu number at line %i in %s\n", line_number, file);
					break;
				}

				auto radio = fields.getField(dt_radio);
				if (!radio) {
					LCPrintf("Unknown radio command at line %i in %s\n", line_number, file);
					break;
				}

				translation_t phrase_translations[5][16];
				memset(phrase_translations, 0, sizeof phrase_translations);
				bool mult = false;

				for (size_t i = 0; i < translations_count; i++) {
					auto lang = translations[i].lang;
					char* argv[6];

					size_t phrases_count = parse((char *)translations[i].text, argv, arraysize(argv), ";\t", false);

					if (phrases_count != 3 && phrases_count != 5) {
						LCPrintf("Invalid radio phrases count at line %i in %s\n", line_number, file);
						continue;
					}

					if (phrases_count == 5) {
						mult = true;
					}

					for (size_t j = 0; j < phrases_count; j++) {
						phrase_translations[j][i].lang = lang;
						phrase_translations[j][i].text = argv[j];
					}
				}

				g_game.addRadio(menuid->int_number, radio->string, phrase_translations, translations_count, mult);
				break;
			}

			default:
				break;
		}
	}

	return true;
}

#define REG_CVAR(x) { x = regCvar(&cv_##x); }

bool loadMainConfig()
{
	auto regCvar = [](cvar_t* cvar) {g_engfuncs.pfnCvar_RegisterVariable(cvar); return g_engfuncs.pfnCVarGetPointer(cvar->name); };

	REG_CVAR(iz_zone_leave_time);
	REG_CVAR(iz_use_interval);
	REG_CVAR(iz_smooth_positioning);
	REG_CVAR(iz_max_aim_angle);
	REG_CVAR(iz_item_max_radius);

	char path[260], file[] = "info_zone.ini";
	g_amxxapi.BuildPathnameR(path, sizeof path - 1, "%s/%s", g_amxxapi.GetLocalInfo("amxx_configsdir", "addons/amxmodx/data"), file);
	memset(&g_config, 0, sizeof g_config);
	resetHudparms();

	if (!parseMainConfig(path, file)) {
		LCPrintf("Main config file %s not found.\n", file);
		return false;
	}

	LCPrintf("Main config loaded.\n");
	return true;
}
