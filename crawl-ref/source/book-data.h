#pragma once

#include <vector>

#include "tag-version.h"

using std::vector;

// This needs to be re-ordered when TAG_MAJOR_VERSION changes!
static const vector<spell_type> spellbook_templates[] =
{

{   // Book of Minor Magic
    SPELL_MAGIC_DART,
    SPELL_BLINK,
    SPELL_CALL_IMP,
    SPELL_SLOW,
    SPELL_CONJURE_FLAME,
    SPELL_MEPHITIC_CLOUD,
},

{   // Book of Conjurations
    SPELL_MAGIC_DART,
    SPELL_SEARING_RAY,
    SPELL_DAZZLING_FLASH,
    SPELL_FULMINANT_PRISM,
    SPELL_ISKENDERUNS_MYSTIC_BLAST,
},

{   // Book of Flames
    SPELL_FOXFIRE,
    SPELL_CONJURE_FLAME,
    SPELL_INNER_FLAME,
    SPELL_STICKY_FLAME,
    SPELL_FIREBALL,
},

{   // Book of Frost
    SPELL_FREEZE,
    SPELL_FROZEN_RAMPARTS,
    SPELL_OZOCUBUS_ARMOUR,
    SPELL_HAILSTORM,
    SPELL_SUMMON_ICE_BEAST,
},

{   // Book of Summonings
    SPELL_SUMMON_FOREST,
    SPELL_SUMMON_MANA_VIPER,
    SPELL_SHADOW_CREATURES,
    SPELL_SUMMON_HYDRA,
},

{   // Book of Fire
    SPELL_IGNITE_POISON,
    SPELL_FIREBALL,
    SPELL_STARBURST,
    SPELL_IGNITION,
},

{   // Book of Ice
    SPELL_ICE_FORM,
    SPELL_ENGLACIATION,
    SPELL_FREEZING_CLOUD,
    SPELL_SIMULACRUM,
    SPELL_OZOCUBUS_REFRIGERATION,
},

{   // Book of Spatial Translocations
    SPELL_BLINK,
    SPELL_BECKONING,
    SPELL_GRAVITAS,
    SPELL_TELEPORT_OTHER,
    SPELL_MANIFOLD_ASSAULT,
},

{   // Book of Hexes
    SPELL_CAUSE_FEAR,
    SPELL_VIOLENT_UNRAVELLING,
    SPELL_SILENCE,
    SPELL_DISCORD,
},

{   // Young Poisoner's Handbook
    SPELL_STING,
    SPELL_POISONOUS_VAPOURS,
    SPELL_MEPHITIC_CLOUD,
    SPELL_IGNITE_POISON,
    SPELL_OLGREBS_TOXIC_RADIANCE,
},

{   // Book of the Tempests
    SPELL_DISCHARGE,
    SPELL_LIGHTNING_BOLT,
    SPELL_IGNITION,
    SPELL_TORNADO,
    SPELL_SHATTER,
},

{   // Book of Death
    SPELL_CORPSE_ROT,
    SPELL_SUBLIMATION_OF_BLOOD,
    SPELL_DISPEL_UNDEAD,
    SPELL_AGONY,
    SPELL_EXCRUCIATING_WOUNDS,
},

{   // Book of Misfortune
    SPELL_CONFUSING_TOUCH,
    SPELL_GRAVITAS,
    SPELL_PETRIFY,
    SPELL_ENGLACIATION,
    SPELL_VIOLENT_UNRAVELLING,
},

{   // Book of Changes
    SPELL_BEASTLY_APPENDAGE,
    SPELL_WEREBLOOD,
    SPELL_SPIDER_FORM,
    SPELL_ICE_FORM,
    SPELL_HYDRA_FORM,
},

{   // Book of Transfigurations
    SPELL_IRRADIATE,
    SPELL_STATUE_FORM,
    SPELL_BLADE_HANDS,
    SPELL_DRAGON_FORM,
},

{   // Fen Folio
    SPELL_CORPSE_ROT,
    SPELL_STONE_ARROW,
    SPELL_SUMMON_FOREST,
    SPELL_HYDRA_FORM,
    SPELL_NOXIOUS_BOG,
    SPELL_SUMMON_HYDRA,
},

{   // Book of Clouds
    SPELL_POISONOUS_VAPOURS,
    SPELL_MEPHITIC_CLOUD,
    SPELL_CONJURE_FLAME,
    SPELL_FREEZING_CLOUD,
},

{   // Book of Necromancy
    SPELL_PAIN,
    SPELL_ANIMATE_SKELETON,
    SPELL_VAMPIRIC_DRAINING,
    SPELL_ANIMATE_DEAD,
    SPELL_AGONY,
},

{   // Book of Callings
    SPELL_SUMMON_SMALL_MAMMAL,
    SPELL_CALL_IMP,
    SPELL_CALL_CANINE_FAMILIAR,
    SPELL_SUMMON_GUARDIAN_GOLEM,
    SPELL_SUMMON_LIGHTNING_SPIRE,
    SPELL_SUMMON_ICE_BEAST,
},

{   // Book of Maledictions
    SPELL_CORONA,
    SPELL_HIBERNATION,
    SPELL_DAZZLING_FLASH,
    SPELL_CONFUSING_TOUCH,
    SPELL_TUKIMAS_DANCE,
},

{   // Book of Air
    SPELL_SHOCK,
    SPELL_SWIFTNESS,
    SPELL_DISCHARGE,
    SPELL_AIRSTRIKE,
    SPELL_LIGHTNING_BOLT,
},

{   // Book of the Sky
    SPELL_SUMMON_LIGHTNING_SPIRE,
    SPELL_SILENCE,
    SPELL_CONJURE_BALL_LIGHTNING,
    SPELL_TORNADO,
},

{   // Book of the Warp
    SPELL_GRAVITAS,
    SPELL_PORTAL_PROJECTILE,
    SPELL_GOLUBRIAS_PASSAGE,
    SPELL_MANIFOLD_ASSAULT,
    SPELL_DISPERSAL,
    SPELL_DISJUNCTION,
},

#if TAG_MAJOR_VERSION == 34
{   // Book of Envenomations
    SPELL_SPIDER_FORM,
    SPELL_OLGREBS_TOXIC_RADIANCE,
    SPELL_INTOXICATE,
},
#endif

{   // Book of Unlife
    SPELL_ANIMATE_DEAD,
    SPELL_BORGNJORS_VILE_CLUTCH,
    SPELL_DEATH_CHANNEL,
    SPELL_SIMULACRUM,
},

#if TAG_MAJOR_VERSION == 34
{   // Book of Control
    SPELL_ENGLACIATION,
},

{   // Book of Battle
    SPELL_WEREBLOOD,
    SPELL_OZOCUBUS_ARMOUR,
},
#endif

{   // Book of Geomancy
    SPELL_SANDBLAST,
    SPELL_PASSWALL,
    SPELL_STONE_ARROW,
    SPELL_PETRIFY,
    SPELL_LRD,
},

{   // Book of Earth
    SPELL_ANIMATE_ARMOUR,
    SPELL_LEDAS_LIQUEFACTION,
    SPELL_BOLT_OF_MAGMA,
    SPELL_STATUE_FORM,
    SPELL_IRON_SHOT,
    SPELL_SHATTER,
},

#if TAG_MAJOR_VERSION == 34
{   // Book of Wizardry
    SPELL_AGONY,
    SPELL_INVISIBILITY,
    SPELL_SPELLFORGED_SERVITOR,
},
#endif

{   // Book of Power
    SPELL_BATTLESPHERE,
    SPELL_BOLT_OF_MAGMA,
    SPELL_IRON_SHOT,
    SPELL_IOOD,
    SPELL_SPELLFORGED_SERVITOR,
},

{   // Book of Cantrips
    SPELL_CORONA,
    SPELL_ANIMATE_SKELETON,
    SPELL_SUMMON_SMALL_MAMMAL,
    SPELL_APPORTATION,
},

{   // Book of Party Tricks
    SPELL_APPORTATION,
    SPELL_BECKONING,
    SPELL_TUKIMAS_DANCE,
    SPELL_INTOXICATE,
    SPELL_INVISIBILITY
},

#if TAG_MAJOR_VERSION == 34
{   // Akashic Record
    SPELL_DISPERSAL,
    SPELL_MALIGN_GATEWAY,
    SPELL_DISJUNCTION,
},
#endif

{   // Book of Debilitation
    SPELL_CORONA,
    SPELL_SLOW,
    SPELL_INNER_FLAME,
    SPELL_PORTAL_PROJECTILE,
    SPELL_CAUSE_FEAR,
    SPELL_LEDAS_LIQUEFACTION,
},

{   // Book of the Dragon
    SPELL_FOXFIRE,
    SPELL_CAUSE_FEAR,
    SPELL_DRAGON_FORM,
    SPELL_DRAGON_CALL,
},

{   // Book of Burglary
    SPELL_SWIFTNESS,
    SPELL_PASSWALL,
    SPELL_GOLUBRIAS_PASSAGE,
    SPELL_LRD,
    SPELL_INVISIBILITY,
},

{   // Book of Dreams
    SPELL_HIBERNATION,
    SPELL_ANIMATE_ARMOUR,
    SPELL_BORGNJORS_VILE_CLUTCH,
    SPELL_SILENCE,
    SPELL_SHADOW_CREATURES,
},

{   // Book of Alchemy
    SPELL_SUBLIMATION_OF_BLOOD,
    SPELL_IGNITE_POISON,
    SPELL_PETRIFY,
    SPELL_INTOXICATE,
    SPELL_IRRADIATE,
    SPELL_NOXIOUS_BOG,
},

#if TAG_MAJOR_VERSION == 34
{   // Book of Beasts
    SPELL_CALL_CANINE_FAMILIAR,
    SPELL_SUMMON_ICE_BEAST,
    SPELL_SUMMON_MANA_VIPER,
    SPELL_SUMMON_HYDRA,
},
#endif

{   // Book of Annihilations
    SPELL_CHAIN_LIGHTNING,
    SPELL_LEHUDIBS_CRYSTAL_SPEAR,
    SPELL_ABSOLUTE_ZERO,
    SPELL_FIRE_STORM,
},

{   // Grand Grimoire
    SPELL_HAUNT,
    SPELL_MONSTROUS_MENAGERIE,
    SPELL_MALIGN_GATEWAY,
    SPELL_SUMMON_HORRIBLE_THINGS,
},

{   // Necronomicon
    SPELL_BORGNJORS_REVIVIFICATION,
    SPELL_INFESTATION,
    SPELL_NECROMUTATION,
    SPELL_DEATHS_DOOR,
},

};

COMPILE_CHECK(ARRAYSZ(spellbook_templates) == 1 + MAX_FIXED_BOOK);
