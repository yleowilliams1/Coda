#pragma once
#include "util/gild_basic.h"
#define STATS_LIST\
	X(STRENGTH)\
	X(AGILITY)\
	X(SOCIAL)\
	X(CONSTITUTION)\
	X(PERCEPTION)\
	X(INTELLIGENCE)\
	X(WISDOM)\
	X(BEAUTY)
#define SKILLS_LIST\
	X(GUNS)\
	X(UNARMED)\
	X(DODGE)\
	X(MELEE)\
	X(INVESTIGATE)\
	X(DOCTOR)\
	X(SNEAK)\
	X(THIEF)\
	X(SPEECH)\
	X(INTIMIDATE)\
	X(FLIRT)\
	X(REPAIR)\
	X(SCIENCE)\
	X(BULWARK)\
	X(ATTRACTION)\
	X(CONVIVIAL)
#define DERIVED_LIST\
	X(MAX_HEALTH)\
	X(MAX_AP)\
	X(INITATIVE_BONUS)\
	X(MAXIMUM_WEIGHT)\
	X(NATURAL_MELEE_DAMAGE)\
	X(NATURAL_ARMOR_CLASS)\
	X(NATURAL_DAMAGE_RESISTANCE)\
	X(NATURAL_POISON_RESISTANCE)
#define STAT_PROTO_LIST \
	X(PLAYER)
enum StatsEnum{
	#define X(name) ST_##name,
	STATS_LIST
	#undef X
	ST_STAT_COUNT
};
enum SkillsEnum{
	#define X(name) SK_##name,
	SKILLS_LIST
	#undef X
	SK_SKILL_COUNT
};
enum DerivedStatsEnum{
	#define X(name) DE_##name,
	DERIVED_LIST
	#undef X
	DE_DERIVED_COUNT
};
enum StatProtoType {
	#define X(name) SPROTO_##name,
	STAT_PROTO_LIST
	#undef X
	SPROTO_COUNT
};
// skills get derived live
struct StatsPrototype{
	s16 base_stats[ST_STAT_COUNT];
	s16 skill_mod[SK_SKILL_COUNT];
};
struct StatsSaveable{
	// Not derived or anything this is for example level up and permanent upgrades
	s16 permanent_bstat_upgrade[ST_STAT_COUNT];	
	s16 permanent_skill_upgrade[SK_SKILL_COUNT];
};
s16 get_stat(enum StatsEnum type, enum StatProtoType proto_type, const struct StatsSaveable *s);
s16 get_skill(enum SkillsEnum type, enum StatProtoType proto_type, const struct StatsSaveable *s);
s32 get_derived(enum DerivedStatsEnum type,enum StatProtoType proto_type,const struct StatsSaveable *s,s32 level);
bool add_stat_point(enum StatsEnum type, enum StatProtoType proto_type, struct StatsSaveable *s);
bool add_skill_point(enum SkillsEnum type, enum StatProtoType proto_type, struct StatsSaveable *s);
