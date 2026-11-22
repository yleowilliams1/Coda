#include <stdint.h>
#include <stdbool.h>
#include "util/gild_basic.h"
#include "util/gild_log.h"
#include "stats.h"

#define MIN_STAT 1
#define MAX_STAT 20
#define MIN_AP 8
#define DEFAULT_STAT 5
#define MAX_SKILL_POINTS 5

static const s16 SKILL_POINT_STAT_REQ[MAX_SKILL_POINTS] = {8, 12, 16, 18, 20};

static const enum StatsEnum SKILL_CORE_STATS[SK_SKILL_COUNT][2] = {
	[SK_GUNS]       = {ST_AGILITY,      ST_PERCEPTION},
	[SK_UNARMED]    = {ST_STRENGTH,     ST_AGILITY},
	[SK_DODGE]      = {ST_AGILITY,      ST_INTELLIGENCE},
	[SK_MELEE]      = {ST_STRENGTH,     ST_CONSTITUTION},
	[SK_INVESTIGATE]= {ST_PERCEPTION,   ST_WISDOM},
	[SK_DOCTOR]     = {ST_INTELLIGENCE, ST_WISDOM},
	[SK_SNEAK]      = {ST_WISDOM,       ST_PERCEPTION},
	[SK_THIEF]      = {ST_AGILITY,      ST_WISDOM},
	[SK_SPEECH]     = {ST_SOCIAL,       ST_BEAUTY},
	[SK_INTIMIDATE] = {ST_STRENGTH,     ST_CONSTITUTION},
	[SK_FLIRT]      = {ST_BEAUTY,       ST_SOCIAL},
	[SK_REPAIR]     = {ST_INTELLIGENCE, ST_WISDOM},
	[SK_SCIENCE]    = {ST_INTELLIGENCE, ST_PERCEPTION},
	[SK_BULWARK]    = {ST_STRENGTH,     ST_CONSTITUTION},
	[SK_ATTRACTION] = {ST_CONSTITUTION, ST_BEAUTY},
	[SK_CONVIVIAL]  = {ST_CONSTITUTION, ST_SOCIAL},
};

static const struct StatsPrototype STATS_PROTOTYPES[SPROTO_COUNT] = {
	[SPROTO_PLAYER] = {
		.base_stats = {
			[ST_STRENGTH]     = DEFAULT_STAT,
			[ST_AGILITY]      = DEFAULT_STAT,
			[ST_SOCIAL]       = DEFAULT_STAT,
			[ST_CONSTITUTION] = DEFAULT_STAT,
			[ST_PERCEPTION]   = DEFAULT_STAT,
			[ST_INTELLIGENCE] = DEFAULT_STAT,
			[ST_WISDOM]       = DEFAULT_STAT,
			[ST_BEAUTY]       = DEFAULT_STAT,
		},
		.skill_mod = {0},
	},
};

static const struct StatsPrototype *resolve_proto(enum StatProtoType proto_type) {
	if (proto_type < 0 || proto_type >= SPROTO_COUNT) {
		GILD_LOG(GILD_OUT_OF_BOUNDS, "%d is not a valid StatProtoType", (s32)proto_type);
		return NULL;
	}
	return &STATS_PROTOTYPES[proto_type];
}

static s32 max_s32(s32 a, s32 b) {
	return a > b ? a : b;
}

static s32 floor_half(s32 v) {
	if (v >= 0) return gild_safe_div(v, (s32)2);
	s32 neg_v = gild_safe_sub((s32)0, v);
	s32 plus_one = gild_safe_add(neg_v, (s32)1);
	s32 half = gild_safe_div(plus_one, (s32)2);
	return gild_safe_sub((s32)0, half);
}

s16 get_stat(enum StatsEnum type, enum StatProtoType proto_type, const struct StatsSaveable *s) {
	if (type < 0 || type >= ST_STAT_COUNT) {
		GILD_LOG(GILD_OUT_OF_BOUNDS, "%d is not a valid enum type", (s32)type);
		return 0;
	}

	const struct StatsPrototype *p = resolve_proto(proto_type);
	s32 base = p ? (s32)p->base_stats[type] : 0;
	s32 upgrade = s ? (s32)s->permanent_bstat_upgrade[type] : 0;
	return gild_safe_cast(s16, gild_safe_add(base, upgrade));
}

s16 get_skill(enum SkillsEnum type, enum StatProtoType proto_type, const struct StatsSaveable *s) {
	if (type < 0 || type >= SK_SKILL_COUNT) {
		GILD_LOG(GILD_OUT_OF_BOUNDS, "%d is not a valid enum type", (s32)type);
		return 0;
	}

	const struct StatsPrototype *p = resolve_proto(proto_type);
	s32 a = (s32)get_stat(SKILL_CORE_STATS[type][0], proto_type, s);
	s32 c = (s32)get_stat(SKILL_CORE_STATS[type][1], proto_type, s);
	s32 derived_base = gild_safe_div(gild_safe_add(gild_safe_add(a, c), (s32)1), (s32)2);

	s32 mod = p ? (s32)p->skill_mod[type] : 0;
	s32 upgrade = s ? (s32)s->permanent_skill_upgrade[type] : 0;
	s32 total = gild_safe_add(gild_safe_add(derived_base, mod), upgrade);
	return gild_safe_cast(s16, total);
}

s32 get_derived(
	enum DerivedStatsEnum type,
	enum StatProtoType proto_type,
	const struct StatsSaveable *s,
	s32 level
) {
	if (type < 0 || type >= DE_DERIVED_COUNT) {
		GILD_LOG(GILD_OUT_OF_BOUNDS, "%d is not a valid enum type", (s32)type);
		return 0;
	}
	if (level < 1) {
		GILD_LOG(GILD_INVALID_ARG, "Invalid level %d", level);
		return 0;
	}

	s32 str = (s32)get_stat(ST_STRENGTH, proto_type, s);
	s32 agi = (s32)get_stat(ST_AGILITY, proto_type, s);
	s32 con = (s32)get_stat(ST_CONSTITUTION, proto_type, s);
	s32 per = (s32)get_stat(ST_PERCEPTION, proto_type, s);
	s32 wis = (s32)get_stat(ST_WISDOM, proto_type, s);

	switch (type) {
		case DE_MAX_HEALTH: {
			s32 pool = gild_safe_add(max_s32(str, con), max_s32(agi, wis));
			return gild_safe_mult(pool, level);
		}
		case DE_MAX_AP: {
			s32 agi_per_diff = gild_safe_sub(max_s32(agi, per), (s32)8);
			return max_s32(agi_per_diff, (s32)MIN_AP);
		}
		case DE_INITATIVE_BONUS:
			return gild_safe_sub(agi, (s32)8);
		case DE_MAXIMUM_WEIGHT:
			return gild_safe_add(gild_safe_mult(str, (s32)25), (s32)75);
		case DE_NATURAL_MELEE_DAMAGE:
			return floor_half(gild_safe_sub(str, (s32)8));
		case DE_NATURAL_ARMOR_CLASS:
			return floor_half(gild_safe_sub(agi, (s32)8));
		case DE_NATURAL_DAMAGE_RESISTANCE:
			return floor_half(gild_safe_sub(con, (s32)8));
		case DE_NATURAL_POISON_RESISTANCE:
			return floor_half(gild_safe_sub(wis, (s32)8));
		default:
			break;
	}
	return 0;
}

bool add_stat_point(enum StatsEnum type, enum StatProtoType proto_type, struct StatsSaveable *s) {
	const struct StatsPrototype *p = resolve_proto(proto_type);
	if (!p) return false;
	if (!s) { GILD_LOG(GILD_IS_NULL, "Stats saveable is NULL"); return false; }
	if (type < 0 || type >= ST_STAT_COUNT) {
		GILD_LOG(GILD_OUT_OF_BOUNDS, "%d is not a valid enum type", (s32)type);
		return false;
	}

	s32 base = (s32)p->base_stats[type];
	s32 upgrade = (s32)s->permanent_bstat_upgrade[type];
	if (base < 0 || base > MAX_STAT || upgrade < 0 || upgrade > MAX_STAT) {
		GILD_LOG(GILD_INVALID_ARG, "Invalid stat values for %d: %d/%d", (s32)type, base, upgrade);
		return false;
	}
	if (gild_safe_add(base, upgrade) >= MAX_STAT) {
		GILD_LOG(GILD_OVERFLOW, "Stat %d already at max %d", (s32)type, MAX_STAT);
		return false;
	}
	s->permanent_bstat_upgrade[type] = gild_safe_cast(s16, gild_safe_add(upgrade, (s32)1));
	return true;
}

bool add_skill_point(enum SkillsEnum type, enum StatProtoType proto_type, struct StatsSaveable *s) {
	const struct StatsPrototype *p = resolve_proto(proto_type);
	if (!p) return false;
	if (!s) { GILD_LOG(GILD_IS_NULL, "Stats saveable is NULL"); return false; }
	if (type < 0 || type >= SK_SKILL_COUNT) {
		GILD_LOG(GILD_OUT_OF_BOUNDS, "%d is not a valid enum type", (s32)type);
		return false;
	}

	s32 upgrade = (s32)s->permanent_skill_upgrade[type];
	if (upgrade < 0 || upgrade > MAX_SKILL_POINTS) {
		GILD_LOG(GILD_INVALID_ARG, "Permanent skill upgrade invalid for %d: %d", (s32)type, upgrade);
		return false;
	}
	if (upgrade >= MAX_SKILL_POINTS) {
		GILD_LOG(GILD_OVERFLOW, "Skill %d already at max bonus %d", (s32)type, MAX_SKILL_POINTS);
		return false;
	}

	s32 need = (s32)SKILL_POINT_STAT_REQ[upgrade];
	s32 a = (s32)get_stat(SKILL_CORE_STATS[type][0], proto_type, s);
	s32 c = (s32)get_stat(SKILL_CORE_STATS[type][1], proto_type, s);
	if (a < need && c < need) {
		GILD_LOG(GILD_INVALID_ARG, "Skill %d needs a core stat of %d (have %d and %d)",
		         (s32)type, need, a, c);
		return false;
	}
	s->permanent_skill_upgrade[type] = gild_safe_cast(s16, gild_safe_add(upgrade, (s32)1));
	return true;
}
