/**
 * @file
 * @brief Misc function related to player transformations.
**/

#include "AppHdr.h"

#include "transform.h"

#include <cstdio>
#include <cstring>

#include "artefact.h"
#include "art-enum.h"
#include "delay.h"
#include "english.h"
#include "env.h"
#include "god-item.h"
#include "god-passive.h" // passive_t::resist_polymorph
#include "invent.h" // check_old_item_warning
#include "item-use.h"
#include "item-name.h"
#include "item-prop.h"
#include "items.h"
#include "localise.h"
#include "message.h"
#include "message-util.h"
#include "mon-death.h"
#include "mutation.h"
#include "output.h"
#include "player-equip.h"
#include "player-stats.h"
#include "prompt.h"
#include "religion.h"
#include "spl-cast.h"
#include "state.h"
#include "stringutil.h"
#include "tag-version.h"
#include "terrain.h"
#include "traps.h"
#include "xom.h"

// transform slot enums into flags
#define SLOTF(s) (1 << s)

static const int EQF_NONE = 0;
// "hand" slots (not rings)
static const int EQF_HANDS = SLOTF(EQ_WEAPON) | SLOTF(EQ_SHIELD)
                             | SLOTF(EQ_GLOVES);
// head and feet (beastly appendage);
static const int EQF_HEAD_FOOT = SLOTF(EQ_BOOTS) | SLOTF(EQ_HELMET);
// core body slots (statue form)
static const int EQF_STATUE = SLOTF(EQ_GLOVES) | SLOTF(EQ_BOOTS)
                              | SLOTF(EQ_BODY_ARMOUR);
// more core body slots (Lear's Hauberk)
static const int EQF_LEAR = EQF_STATUE | SLOTF(EQ_HELMET);
// everything you can (W)ear
static const int EQF_WEAR = EQF_LEAR | SLOTF(EQ_CLOAK) | SLOTF(EQ_SHIELD);
// everything but jewellery
static const int EQF_PHYSICAL = EQF_HANDS | EQF_WEAR;
// all rings (except for the macabre finger amulet's)
static const int EQF_RINGS = SLOTF(EQ_LEFT_RING) | SLOTF(EQ_RIGHT_RING)
                             | SLOTF(EQ_RING_ONE) | SLOTF(EQ_RING_TWO)
                             | SLOTF(EQ_RING_THREE) | SLOTF(EQ_RING_FOUR)
                             | SLOTF(EQ_RING_FIVE) | SLOTF(EQ_RING_SIX)
                             | SLOTF(EQ_RING_SEVEN) | SLOTF(EQ_RING_EIGHT);
// amulet & pal
static const int EQF_AMULETS = SLOTF(EQ_AMULET) | SLOTF(EQ_RING_AMULET);
// everything
static const int EQF_ALL = EQF_PHYSICAL | EQF_RINGS | EQF_AMULETS;

string Form::melding_description() const
{
    // this is a bit rough and ready...
    // XX simplify slot melding rather than complicate this function?
    if (blocked_slots == EQF_ALL)
        return "Your equipment is entirely melded.";
    else if (blocked_slots == EQF_PHYSICAL)
        return "Your armour is entirely melded.";
    else if ((blocked_slots & EQF_PHYSICAL) == EQF_PHYSICAL)
        return "Your equipment is almost entirely melded.";
    else if ((blocked_slots & EQF_STATUE) == EQF_STATUE
             && (you_can_wear(EQ_GLOVES, false)
                 || you_can_wear(EQ_BOOTS, false)
                 || you_can_wear(EQ_BODY_ARMOUR, false)))
    {
        return "Your equipment is partially melded.";
    }
    // otherwise, rely on the form description to convey what is melded.
    return "";
}

static const FormAttackVerbs DEFAULT_VERBS = FormAttackVerbs(FAV_DEFAULT);
static const FormAttackVerbs ANIMAL_VERBS = FormAttackVerbs(FAV_HIT, FAV_BITE,
                                                            FAV_MAUL, FAV_MAUL);

static const FormDuration DEFAULT_DURATION = FormDuration(20, PS_DOUBLE, 100);
static const FormDuration BAD_DURATION = FormDuration(15, PS_ONE_AND_A_HALF,
                                                      100);

// Class form_entry and the formdata array
#include "form-data.h"

static const form_entry &_find_form_entry(transformation form)
{
    for (const form_entry &entry : formdata)
        if (entry.tran == form)
            return entry;
    die("No formdata entry found for form %d", (int)form);
}

Form::Form(const form_entry &fe)
    : short_name(fe.short_name), wiz_name(fe.wiz_name),
      duration(fe.duration),
      str_mod(fe.str_mod), dex_mod(fe.dex_mod),
      blocked_slots(fe.blocked_slots), size(fe.size), hp_mod(fe.hp_mod),
      can_cast(fe.can_cast), spellcasting_penalty(fe.spellcasting_penalty),
      unarmed_hit_bonus(fe.unarmed_hit_bonus), uc_colour(fe.uc_colour),
      uc_attack_verbs(fe.uc_attack_verbs),
      can_bleed(fe.can_bleed),
      keeps_mutations(fe.keeps_mutations),
      shout_verb(fe.shout_verb),
      shout_volume_modifier(fe.shout_volume_modifier),
      hand_name(fe.hand_name), foot_name(fe.foot_name),
      flesh_equivalent(fe.flesh_equivalent),
      long_name(fe.long_name), description(fe.description),
      resists(fe.resists),
      base_unarmed_damage(fe.base_unarmed_damage),
      can_fly(fe.can_fly), can_swim(fe.can_swim),
      flat_ac(fe.flat_ac), power_ac(fe.power_ac), xl_ac(fe.xl_ac),
      uc_brand(fe.uc_brand), uc_attack(fe.uc_attack),
      prayer_action(fe.prayer_action), equivalent_mons(fe.equivalent_mons),
      fakemuts(fe.fakemuts)
{ }

Form::Form(transformation tran)
    : Form(_find_form_entry(tran))
{ }
/**
 * Is the given equipment slot available for use in this form?
 *
 * @param slot      The equipment slot in question. (May be a weird fake
 *                  slot - EQ_STAFF or EQ_ALL_ARMOUR.)
 * @return          Whether at least some items can be worn in this slot in
 *                  this form.
 *                  (The player's race, or mutations, may still block the
 *                  slot, or it may be restricted to subtypes.)
 */
bool Form::slot_available(int slot) const
{
    if (slot == EQ_ALL_ARMOUR)
        return !all_blocked(EQF_WEAR);
    if (slot == EQ_RINGS || slot == EQ_RINGS_PLUS)
        return !all_blocked(EQF_RINGS);

    if (slot == EQ_STAFF)
        slot = EQ_WEAPON;
    return !(blocked_slots & SLOTF(slot));
}

/**
 * Can the player wear the given item while in this form?
 *
 * Does not take mutations into account.
 *
 * @param item  The item in question
 * @return      Whether this form prevents the player from wearing the
 *              item. (Other things may also prevent it, of course)
 */
bool Form::can_wear_item(const item_def& item) const
{
    if (item.base_type == OBJ_JEWELLERY)
    {
        if (jewellery_is_amulet(item))
            return slot_available(EQ_AMULET);
        return !all_blocked(EQF_RINGS);
    }

    if (is_unrandom_artefact(item, UNRAND_LEAR))
        return !(blocked_slots & EQF_LEAR); // ok if no body slots blocked

    return slot_available(get_armour_slot(item));
}

/**
 * Get the bonus to form duration granted for a given (spell)power.
 *
 * @param pow               The spellpower/equivalent of the form.
 * @return                  A bonus to form duration.
 */
int FormDuration::power_bonus(int pow) const
{
    switch (scaling_type)
    {
        case PS_NONE:
            return 0;
        case PS_SINGLE:
            return random2(pow);
        case PS_ONE_AND_A_HALF:
            return random2(pow) + random2(pow/2);
        case PS_DOUBLE:
            return random2(pow) + random2(pow);
        default:
            die("Unknown scaling type!");
            return -1;
    }
}

/**
 * Get the duration for this form, when newly entered.
 *
 * @param pow   The power of the effect creating this form. (Spellpower, etc.)
 * @return      The duration of the form. (XXX: in turns...?)
 */
int Form::get_duration(int pow) const
{
    return min(duration.base + duration.power_bonus(pow), duration.max);
}

/**
 * Get a verbose description for the form.
 *
 * @param past_tense     Whether the description should be in past or present
 *                       tense.
 * @return               A description for the form.
 */
string Form::get_description(bool past_tense) const
{
    if (past_tense)
        return localise("You were %s.", get_transform_description());
    else
        return localise("You are %s.", get_transform_description());
}

/**
 * Show a message for transforming into this form, based on your current
 * situation (e.g. in water...)
 */
void Form::do_transform_message(transformation previous_trans) const
{
    // XXX: refactor this into a second function (and also rethink the logic)
    string msg;
    if (you.in_water() && player_can_fly())
    {
        mprf("You fly out of the water as you turn into %s.",
             get_transform_description().c_str());
    }
    else if (get_form(previous_trans)->player_can_fly()
             && player_can_swim()
             && feat_is_water(env.grid(you.pos())))
    {
        mprf("As you dive into the water, you turn into %s.",
             get_transform_description().c_str());
    }
    else
    {
        mprf("You turn into %s.", get_transform_description().c_str());
    }
}

/**
 * Show a message for untransforming from this form.
 */
void Form::do_untransform_message() const
{
    mpr(MSGCH_DURATION, "Your transformation has ended.");
}

/**
 * What AC bonus does the player get while in this form?
 *
 * Many forms are power-dependent, so the answer given may be strange if the
 * player isn't currently in the form in question.
 *
 * @return  The AC bonus currently granted by the form, multiplied by 100 to
 *          allow for pseudo-decimal flexibility (& to match
 *          player::armour_class())
 */
int Form::get_ac_bonus() const
{
    return flat_ac * 100
           + power_ac * you.props[TRANSFORM_POW_KEY].get_int()
           + xl_ac * you.experience_level;
}

/**
 * (freeze)
 */
static string _brand_suffix(brand_type brand)
{
    if (brand == SPWPN_NORMAL)
        return "";
    return localise(" (%s)", brand_type_name(brand, true));
}

/**
 * What name should be used for the player's means of unarmed attack while
 * in this form?
 *
 * (E.g. for display in the top-right of the UI.)
 *
 * @param   The player's UC weapon when not in a form (claws, etc)
 * @return  A string describing the form's UC attack 'weapon'.
 */
string Form::get_uc_attack_name(string default_name) const
{
    const string brand_suffix = _brand_suffix(get_uc_brand());
    if (uc_attack.empty())
        return localise(default_name) + brand_suffix;
    return localise(uc_attack) + brand_suffix;
}

/**
 * How many levels of resistance against fire does this form provide?
 */
int Form::res_fire() const
{
    return get_resist(resists, MR_RES_FIRE);
}

/**
 * How many levels of resistance against cold does this form provide?
 */
int Form::res_cold() const
{
    return get_resist(resists, MR_RES_COLD);
}

/**
 * How many levels of resistance against negative energy does this form give?
 */
int Form::res_neg() const
{
    return get_resist(resists, MR_RES_NEG);
}

/**
 * Does this form provide resistance to electricity?
 */
bool Form::res_elec() const
{
    return get_resist(resists, MR_RES_ELEC);
}

/**
 * How many levels of resistance against poison does this form give?
 */
int Form::res_pois() const
{
    return get_resist(resists, MR_RES_POISON);
}

/**
 * Does this form provide resistance to miasma?
 */
bool Form::res_miasma() const
{
    return get_resist(resists, MR_RES_MIASMA);
}

/**
 * Does this form provide resistance against acid?
 */
bool Form::res_acid() const
{
    return get_resist(resists, MR_RES_ACID);
}

/**
 * Does this form provide resistance to sticky flame?
 */
bool Form::res_sticky_flame() const
{
    return get_resist(resists, MR_RES_STICKY_FLAME);
}

/**
 * Does this form provide resistance to petrification?
 */
bool Form::res_petrify() const
{
    return get_resist(resists, MR_RES_PETRIFY);
}


/**
 * Does this form enable flight?
 *
 * @return Whether this form allows flight for characters which don't already
 *         have access to it.
 */
bool Form::enables_flight() const
{
    return can_fly == FC_ENABLE;
}

/**
 * Does this form disable flight?
 *
 * @return Whether flight is always impossible while in this form.
 */
bool Form::forbids_flight() const
{
    return can_fly == FC_FORBID;
}

/**
 * Does this form disable swimming?
 *
 * @return Whether swimming is always impossible while in this form.
 */
bool Form::forbids_swimming() const
{
    return can_swim == FC_FORBID;
}

/**
 * Can the player fly, if in this form?
 *
 * DOES consider player state besides form.
 * @return  Whether the player will be able to fly in this form.
 */
bool Form::player_can_fly() const
{
    return !forbids_flight()
           && (enables_flight()
               || you.racial_permanent_flight()); // XX other cases??
}

/**
 * Can the player swim, if in this form?
 *
 * DOES consider player state besides form.
 * @return  Whether the player will be able to swim in this form.
 */
bool Form::player_can_swim() const
{
    // XX this is kind of a mess w.r.t. player::can_swim
    const size_type player_size = size == SIZE_CHARACTER ?
                                          you.body_size(PSIZE_BODY, true) :
                                          size;
    return can_swim == FC_ENABLE
           || species::can_swim(you.species)
              && can_swim != FC_FORBID
           || player_size >= SIZE_GIANT;
}

/**
 * Are all of the given equipment slots blocked while in this form?
 *
 * @param slotflags     A set of flags, corresponding to the union of
 (1 << the slot enum) for each slot in question.
 * @return              Whether all of the given slots are blocked.
 */
bool Form::all_blocked(int slotflags) const
{
    return slotflags == (blocked_slots & slotflags);
}

/**
 * What message should be printed when the player prays at an altar?
 * To be inserted into "You %s the altar of foo."
 *
 * If the form has a valid custom action, print that; otherwise, default to the
 * 'flying' or species-specific actions, as appropriate.
 *
 * @return  An action to be printed when the player prays at an altar.
 *          E.g., "perch on", "crawl onto", "sway towards", etc.
 */
string Form::player_prayer_action() const
{
    // If the form is naturally flying & specifies an action, use that.
    if (can_fly == FC_ENABLE && !prayer_action.empty())
        return prayer_action;
    // Otherwise, if you're flying, use the generic flying action.
    // XXX: if we ever get a default-permaflying species again that wants to
    // have a separate verb, we'll want to check for that right here.
    if (you.airborne())
        return "You hover solemnly before the altar of %s.";
    // Otherwise, if you have a verb, use that...
    if (!prayer_action.empty())
        return prayer_action;
    // Finally, default to your species' verb.
    return species::prayer_action(you.species);
}

vector<string> Form::get_fakemuts(bool terse) const
{
    vector<string> result;
    for (const auto &p : fakemuts)
        result.push_back(terse ? p.first : p.second);
    return result;
}


class FormNone : public Form
{
private:
    FormNone() : Form(transformation::none) { }
    DISALLOW_COPY_AND_ASSIGN(FormNone);
public:
    static const FormNone &instance() { static FormNone inst; return inst; }

    /**
     * Get a string describing the form you're turning into. (If not the same
     * as the one used to describe this form in @.
     */
    string get_transform_description() const override { return "your old self"; }
};

class FormSpider : public Form
{
private:
    FormSpider() : Form(transformation::spider) { }
    DISALLOW_COPY_AND_ASSIGN(FormSpider);
public:
    static const FormSpider &instance() { static FormSpider inst; return inst; }
};

class FormBlade : public Form
{
private:
    FormBlade() : Form(transformation::blade_hands) { }
    DISALLOW_COPY_AND_ASSIGN(FormBlade);
public:
    static const FormBlade &instance() { static FormBlade inst; return inst; }

    /**
     * % screen description
     */
    string get_long_name() const override
    {
        return you.base_hand_name(true, true);
    }

    /**
     * @ description
     */
    string get_description(bool past_tense) const override
    {
        if (past_tense)
        {
            if (you.species == SP_FELID)
                return localise("You had blades for front paws.");
            else if (you.species == SP_OCTOPODE)
                return localise("You had blades for tentacles.");
            else
                return localise("You had blades for hands.");
        }
        else
        {
            if (you.species == SP_FELID)
                return localise("You have blades for front paws.");
            else if (you.species == SP_OCTOPODE)
                return localise("You have blades for tentacles.");
            else
                return localise("You have blades for hands.");
        }
    }

    /**
     * Show a message for transforming into this form.
     */
    void do_transform_message(transformation /*previous_trans*/) const override
    {
        const bool singular = you.arm_count() == 1;

        if (singular)
        {
            if (you.species == SP_FELID)
                mpr("Your front paw turns into a razor-sharp scythe blade.");
            else if (you.species == SP_OCTOPODE)
                mpr("Your tentacle turns into a razor-sharp scythe blade.");
            else
                mpr("Your hand turns into a razor-sharp scythe blade.");
        }
        else
        {
            if (you.species == SP_FELID)
                mpr("Your front paws turn into razor-sharp scythe blades.");
            else if (you.species == SP_OCTOPODE)
                mpr("Your tentacles turn into razor-sharp scythe blades.");
            else
                mpr("Your hands turn into razor-sharp scythe blades.");
        }
    }

    /**
     * Show a message for untransforming from this form.
     */
    void do_untransform_message() const override
    {
        const bool singular = you.arm_count() == 1;

        if (singular)
        {
            if (you.species == SP_FELID)
                mpr(MSGCH_DURATION, "Your front paw reverts to its normal proportions.");
            else if (you.species == SP_OCTOPODE)
                mpr(MSGCH_DURATION, "Your tentacle reverts to its normal proportions.");
            else
                mpr(MSGCH_DURATION, "Your hand reverts to its normal proportions.");
        }
        else
        {
            if (you.species == SP_FELID)
                mpr(MSGCH_DURATION, "Your front paws revert to their normal proportions.");
            else if (you.species == SP_OCTOPODE)
                mpr(MSGCH_DURATION, "Your tentacles revert to their normal proportions.");
            else
                mpr(MSGCH_DURATION, "Your hands revert to their normal proportions.");
        }
    }

    bool can_offhand_punch() const override { return true; }

    /**
     * Get the name displayed in the UI for the form's unarmed-combat 'weapon'.
     */
    string get_uc_attack_name(string /*default_name*/) const override
    {
        return uppercase_first(localise(get_long_name()));
    }
};

class FormStatue : public Form
{
private:
    FormStatue() : Form(transformation::statue) { }
    DISALLOW_COPY_AND_ASSIGN(FormStatue);
public:
    static const FormStatue &instance() { static FormStatue inst; return inst; }

    /**
     * Show a message for transforming into this form.
     */
    void do_transform_message(transformation previous_trans) const override
    {
        if (you.species == SP_DEEP_DWARF && one_chance_in(10))
            mpr("You inwardly fear your resemblance to a lawn ornament.");
        else if (you.species == SP_GARGOYLE)
            mpr("Your body stiffens and grows slower.");
        else
            Form::do_transform_message(previous_trans);
    }

    /**
     * Get a string describing the form you're turning into. (If not the same
     * as the one used to describe this form in @.
     */
    string get_transform_description() const override
    {
        return "a living statue of rough stone";
    }

    /**
     * Show a message for untransforming from this form.
     */
    void do_untransform_message() const override
    {
        // This only handles lava orcs going statue -> stoneskin.
        if (you.species == SP_GARGOYLE)
            mpr(MSGCH_DURATION, "You revert to a slightly less stony form.");
        else
            mpr(MSGCH_DURATION, "You revert to your normal fleshy form.");
    }

    /**
     * Get the name displayed in the UI for the form's unarmed-combat 'weapon'.
     */
    string get_uc_attack_name(string /*default_name*/) const override
    {
        // there's special casing in base_hand_name to get "fists"
        string hand = you.base_hand_name(true, true);
        return make_stringf("Stone %s", hand.c_str()); // noloc
    }
};

class FormIce : public Form
{
private:
    FormIce() : Form(transformation::ice_beast) { }
    DISALLOW_COPY_AND_ASSIGN(FormIce);
public:
    static const FormIce &instance() { static FormIce inst; return inst; }

    /**
     * Show a message for untransforming from this form.
     */
    void do_untransform_message() const override
    {
        mpr(MSGCH_DURATION, "You warm up again.");
    }

    /**
     * Get the name displayed in the UI for the form's unarmed-combat 'weapon'.
     */
    string get_uc_attack_name(string /*default_name*/) const override
    {
        const bool singular = you.get_mutation_level(MUT_MISSING_HAND);
        // paws for consistency with form-data and the tile
        // XX does this imply the behavior of feline paws?
        return localise(singular ? "Ice paw" : "Ice paws");
    }
};

class FormDragon : public Form
{
private:
    FormDragon() : Form(transformation::dragon) { }
    DISALLOW_COPY_AND_ASSIGN(FormDragon);
public:
    static const FormDragon &instance() { static FormDragon inst; return inst; }

    /**
     * Get an monster type corresponding to the transformation.
     *
     * (Used for console player glyphs.)
     *
     * @return  A monster type corresponding to the player in this form.
     */
    monster_type get_equivalent_mons() const override
    {
        return species::dragon_form(you.species);
    }

    string get_transform_description() const override
    {
        if (species::is_draconian(you.species))
        {
            return make_stringf("a fearsome %s!",
                          mons_class_name(get_equivalent_mons()));
        }
        else
            return description;
    }

    /**
     * The AC bonus of the form, multiplied by 100 to match
     * player::armour_class().
     */
    int get_ac_bonus() const override
    {
        if (species::is_draconian(you.species))
            return 1000;
        return Form::get_ac_bonus();
    }

    /**
     * How many levels of resistance against fire does this form provide?
     */
    int res_fire() const override
    {
        switch (species::dragon_form(you.species))
        {
            case MONS_FIRE_DRAGON:
                return 2;
            case MONS_ICE_DRAGON:
                return -1;
            default:
                return 0;
        }
    }

    /**
     * How many levels of resistance against cold does this form provide?
     */
    int res_cold() const override
    {
        switch (species::dragon_form(you.species))
        {
            case MONS_ICE_DRAGON:
                return 2;
            case MONS_FIRE_DRAGON:
                return -1;
            default:
                return 0;
        }
    }

    bool can_offhand_punch() const override { return true; }
};

class FormLich : public Form
{
private:
    FormLich() : Form(transformation::lich) { }
    DISALLOW_COPY_AND_ASSIGN(FormLich);
public:
    static const FormLich &instance() { static FormLich inst; return inst; }

    /**
     * Show a message for transforming into this form.
     */
    void do_transform_message(transformation /*previous_trans*/) const override
    {
        mpr("Your body is suffused with negative energy!");
    }

    /**
     * Show a message for untransforming from this form.
     */
    void do_untransform_message() const override
    {
        if (you.undead_state() == US_ALIVE)
            mpr(MSGCH_DURATION, "You feel yourself come back to life.");
        else
        {
            mpr(MSGCH_DURATION, "You feel your undeath return to normal.");
            // ^^^ vampires only, probably
        }
    }
};

class FormBat : public Form
{
private:
    FormBat() : Form(transformation::bat) { }
    DISALLOW_COPY_AND_ASSIGN(FormBat);
public:
    static const FormBat &instance() { static FormBat inst; return inst; }

    /**
     * Get an monster type corresponding to the transformation.
     *
     * (Used for console player glyphs.)
     *
     * @return  A monster type corresponding to the player in this form.
     */
    monster_type get_equivalent_mons() const override
    {
        return you.has_mutation(MUT_VAMPIRISM) ? MONS_VAMPIRE_BAT : MONS_BAT;
    }

    string get_description(bool past_tense) const override
    {
        if (you.species == SP_VAMPIRE)
        {
            if (past_tense)
                return localise("You were in vampire-bat-form.");
            else
                return localise("You are in vampire-bat-form.");
        }
        else
        {
            if (past_tense)
                return localise("You were in bat-form.");
            else
                return localise("You are in bat-form.");
        }
    }

    /**
     * Get a string describing the form you're turning into. (If not the same
     * as the one used to describe this form in @.
     */
    string get_transform_description() const override
    {
        return you.has_mutation(MUT_VAMPIRISM) ? "a vampire bat" : "a bat";
    }
};

class FormPig : public Form
{
private:
    FormPig() : Form(transformation::pig) { }
    DISALLOW_COPY_AND_ASSIGN(FormPig);
public:
    static const FormPig &instance() { static FormPig inst; return inst; }
};

class FormAppendage : public Form
{
private:
    FormAppendage() : Form(transformation::appendage) { }
    DISALLOW_COPY_AND_ASSIGN(FormAppendage);
public:
    static const FormAppendage &instance()
    {
        static FormAppendage inst;
        return inst;
    }

    string get_description(bool past_tense) const override
    {
        ostringstream desc;
        bool spike = false;
        vector<string> muts;
        for (auto app : you.props[APPENDAGE_KEY].get_vector())
        {
            if (desc.tellp() != std::streampos(0))
                desc << localise(" ");

            mutation_type mut = static_cast<mutation_type>(app.get_int());
            if (mut == MUT_TENTACLE_SPIKE)
                spike = true;
            else
                muts.push_back(localise(mutation_name(mut)));
        }

        if (spike)
        {
            if (past_tense)
                desc << localise("One of your tentacles had grown a beastly spike.");
            else
                desc << localise("One of your tentacles has grown a beastly spike.");
        }

        if (!muts.empty())
        {
            string muts_str = comma_separated_line(muts.begin(), muts.end());
            desc << localise(" ");
            if (past_tense)
                desc << localise("You had temporarily grown %s.", muts_str);
            else
                desc << localise("You have temporarily grown %s.", muts_str);
        }

        return desc.str();
    }

    /**
     * Show a message for transforming into this form.
     */
    void do_transform_message(transformation /*previous_trans*/) const override
    {
        ostringstream msg;
        for (auto app : you.props[APPENDAGE_KEY].get_vector())
        {
            if (msg.tellp() != std::streampos(0))
                msg << " ";

            mutation_type mut = static_cast<mutation_type>(app.get_int());
            switch (mut)
            {
                case MUT_HORNS:
                    msg << "You grow a pair of large bovine horns.";
                    break;
                case MUT_TENTACLE_SPIKE:
                    msg << "One of your tentacles grows a vicious spike.";
                    break;
                case MUT_TALONS:
                    msg << "Your feet morph into talons.";
                    break;
                default:
                    die("Unknown appendage type");
                    break;
            }
        }

        mpr(msg.str());
    }

    /**
     * Show a message for untransforming from this form. (Handled elsewhere.)
     */
    void do_untransform_message() const override { }
};

class FormTree : public Form
{
private:
    FormTree() : Form(transformation::tree) { }
    DISALLOW_COPY_AND_ASSIGN(FormTree);
public:
    static const FormTree &instance() { static FormTree inst; return inst; }

    /**
     * Show a message for untransforming from this form.
     */
    void do_untransform_message() const override
    {
        mpr(MSGCH_DURATION, "You feel less wooden.");
    }
};

#if TAG_MAJOR_VERSION == 34
class FormPorcupine : public Form
{
private:
    FormPorcupine() : Form(transformation::porcupine) { }
    DISALLOW_COPY_AND_ASSIGN(FormPorcupine);
public:
    static const FormPorcupine &instance()
    {
        static FormPorcupine inst;
        return inst;
    }
};
#endif

class FormWisp : public Form
{
private:
    FormWisp() : Form(transformation::wisp) { }
    DISALLOW_COPY_AND_ASSIGN(FormWisp);
public:
    static const FormWisp &instance() { static FormWisp inst; return inst; }
};

#if TAG_MAJOR_VERSION == 34
class FormJelly : public Form
{
private:
    FormJelly() : Form(transformation::jelly) { }
    DISALLOW_COPY_AND_ASSIGN(FormJelly);
public:
    static const FormJelly &instance() { static FormJelly inst; return inst; }
};
#endif

class FormFungus : public Form
{
private:
    FormFungus() : Form(transformation::fungus) { }
    DISALLOW_COPY_AND_ASSIGN(FormFungus);
public:
    static const FormFungus &instance() { static FormFungus inst; return inst; }

    /**
     * Show a message for untransforming from this form.
     */
    void do_untransform_message() const override
    {
        mpr(MSGCH_DURATION, "You stop sporulating.");
    }
};

class FormShadow : public Form
{
private:
    FormShadow() : Form(transformation::shadow) { }
    DISALLOW_COPY_AND_ASSIGN(FormShadow);
public:
    static const FormShadow &instance() { static FormShadow inst; return inst; }

    /**
     * Show a message for untransforming from this form.
     */
    void do_untransform_message() const override
    {
        if (you.invisible())
            mpr(MSGCH_DURATION, "You feel less shadowy.");
        else
            mpr(MSGCH_DURATION, "You emerge from the shadows.");
    }
};

void set_airform_power(int pow)
{
    you.props[AIRFORM_POWER_KEY] = pow;
}

class FormStorm : public Form
{
private:
    FormStorm() : Form(transformation::storm) { }
    DISALLOW_COPY_AND_ASSIGN(FormStorm);
public:
    static const FormStorm &instance() { static FormStorm inst; return inst; }

    /**
     * Find the player's base unarmed damage in this form.
     */
    int get_base_unarmed_damage() const override
    {
        int power = 0;
        if (you.props.exists(AIRFORM_POWER_KEY))
            power = you.props[AIRFORM_POWER_KEY].get_int();
        return 2 + div_rand_round(power * 2, 5);
    }

    bool can_offhand_punch() const override { return true; }
};

#if TAG_MAJOR_VERSION == 34

/**
 * Set the number of hydra heads that the player currently has.
 *
 * @param heads the new number of heads you should have.
 */
void set_hydra_form_heads(int heads)
{
    you.props[HYDRA_FORM_HEADS_KEY] = min(MAX_HYDRA_HEADS, max(1, heads));
    you.wield_change = true;
}

class FormHydra : public Form
{
private:
    FormHydra() : Form(transformation::hydra) { }
    DISALLOW_COPY_AND_ASSIGN(FormHydra);
public:
    static const FormHydra &instance() { static FormHydra inst; return inst; }

    /**
     * Get a string describing the form you're turning into.
     */
    string get_transform_description() const override
    {
        const auto heads = you.heads();
        const string headstr = (heads < 11 ? number_in_words(heads)
                                           : to_string(heads))
                             + "-headed hydra"; // noloc
        return article_a(headstr);
    }

    /**
     * @ description
     */
    string get_description(bool past_tense) const override
    {
        return localise(past_tense ? "You were %s." : "You are %s.",
                        get_transform_description());
    }

    /**
     * Get the name displayed in the UI for the form's unarmed-combat 'weapon'.
     */
    string get_uc_attack_name(string /*default_name*/) const override
    {
        return localise("Bite (x%d)", you.heads());
    }

    /**
     * Find the player's base unarmed damage in this form.
     */
    int get_base_unarmed_damage() const override
    {
        // 3 damage per head for 1-10
        const int normal_heads_damage = min(you.heads(), 10) * 3;
        // 3/2 damage per head for 11-20 (they get in each-other's way)
            // (and also a 62-base-damage form scares me)
        const int too_many_heads_damage = max(0, you.heads() - 10)
                                            * 3 / 2;
        // 2-47 (though more like 14-32 in practical ranges...)
        return 2 + normal_heads_damage + too_many_heads_damage;
    }

};
#endif

static const Form* forms[] =
{
    &FormNone::instance(),
    &FormSpider::instance(),
    &FormBlade::instance(),
    &FormStatue::instance(),

    &FormIce::instance(),
    &FormDragon::instance(),
    &FormLich::instance(),
    &FormBat::instance(),

    &FormPig::instance(),
    &FormAppendage::instance(),
    &FormTree::instance(),
#if TAG_MAJOR_VERSION == 34
    &FormPorcupine::instance(),
#endif

    &FormWisp::instance(),
#if TAG_MAJOR_VERSION == 34
    &FormJelly::instance(),
#endif
    &FormFungus::instance(),
    &FormShadow::instance(),
#if TAG_MAJOR_VERSION == 34
    &FormHydra::instance(),
#endif
    &FormStorm::instance(),
};

const Form* get_form(transformation xform)
{
    COMPILE_CHECK(ARRAYSZ(forms) == NUM_TRANSFORMS);
    const int form = static_cast<int>(xform);
    ASSERT_RANGE(form, 0, NUM_TRANSFORMS);
    return forms[form];
}


/**
 * Get the wizmode name of a form.
 *
 * @param form      The form in question.
 * @return          The form's casual, wizmode name.
 */
const char* transform_name(transformation form)
{
    return get_form(form)->wiz_name.c_str();
}

/**
 * Can the player (w)ield weapons when in the given form?
 *
 * @param form      The form in question.
 * @return          Whether the player can wield items when in that form.
*/
bool form_can_wield(transformation form)
{
    return get_form(form)->can_wield();
}

/**
 * Can the player (W)ear armour when in the given form?
 *
 * @param form      The form in question.
 * @return          Whether the player can wear armour when in that form.
 */
bool form_can_wear(transformation form)
{
    return !testbits(get_form(form)->blocked_slots, EQF_WEAR);
}

/**
 * Can the player fly, if in this form?
 *
 * DOES consider player state besides form.
 * @param form      The form in question.
 * @return          Whether the player will be able to fly in this form.
 */
bool form_can_fly(transformation form)
{
    return get_form(form)->player_can_fly();
}


/**
 * Can the player swim, if in this form?
 *
 * (Swimming = traversing deep & shallow water without penalties; includes
 * floating (ice form) and wading forms (giants - currently just dragon form,
 * which normally flies anyway...))
 *
 * DOES consider player state besides form.
 * @param form      The form in question.
 * @return          Whether the player will be able to swim in this form.
 */
bool form_can_swim(transformation form)
{
    return get_form(form)->player_can_swim();
}

/**
 * Can the player survive in deep water when in the given form?
 *
 * Doesn't count flight or beogh water-walking.
 *
 * @param form      The form in question.
 * @return          Whether the player won't be killed when entering deep water
 *                  in that form.
 */
bool form_likes_water(transformation form)
{
    // Grey dracs can't swim, so can't statue form merfolk/octopodes
    // -- yet they can still survive in water.
    if (species::likes_water(you.species)
        && (form == transformation::statue
            || !get_form(form)->forbids_swimming()))
    {
        return true;
    }

    // otherwise, you gotta swim to survive!
    return form_can_swim(form);
}

// Used to mark transformations which override species intrinsics.
bool form_changed_physiology(transformation form)
{
    return form != transformation::none
        && form != transformation::appendage
        && form != transformation::blade_hands;
}

/**
 * Does this form have blood?
 *
 * @param form      The form in question.
 * @return          Whether the form can bleed, sublime, etc.
 */
bool form_can_bleed(transformation form)
{
    return get_form(form)->can_bleed != FC_FORBID;
}

// Used to mark forms which keep most form-based mutations.
bool form_keeps_mutations(transformation form)
{
    return get_form(form)->keeps_mutations;
}

static set<equipment_type>
_init_equipment_removal(transformation form)
{
    set<equipment_type> result;
    if (!form_can_wield(form) && you.weapon() || you.melded[EQ_WEAPON])
        result.insert(EQ_WEAPON);

    // Liches can't wield holy weapons.
    if (form == transformation::lich && you.weapon()
        && is_holy_item(*you.weapon()))
    {
        result.insert(EQ_WEAPON);
    }

    for (int i = EQ_FIRST_EQUIP; i < NUM_EQUIP; ++i)
    {
        if (i == EQ_WEAPON)
            continue;
        const equipment_type eq = static_cast<equipment_type>(i);
        const item_def *pitem = you.slot_item(eq, true);

        if (pitem && (get_form(form)->blocked_slots & SLOTF(i)
                      || (i != EQ_RING_AMULET
                          && !get_form(form)->can_wear_item(*pitem))))
        {
            result.insert(eq);
        }
    }
    return result;
}

static void _remove_equipment(const set<equipment_type>& removed,
                              transformation form,
                              bool meld = true, bool mutation = false)
{
    // Meld items into you in (reverse) order. (set is a sorted container)
    for (const equipment_type e : removed)
    {
        item_def *equip = you.slot_item(e, true);
        if (equip == nullptr)
            continue;

        bool unequip = !meld;
        if (!unequip && e == EQ_WEAPON)
        {
            if (form_can_wield(form))
                unequip = true;
            if (!is_weapon(*equip))
                unequip = true;
        }

        string eq = equip->name(DESC_YOUR);
        if (you_worship(GOD_ASHENZARI) && unequip && equip->cursed())
        {
            if (equip->quantity == 1)
                mprf("%s falls away, shattering the curse!", eq.c_str());
            else
                mprf("%s fall away, shattering the curse!", eq.c_str());
        }
        else if (unequip)
        {
            if (equip->quantity == 1)
                mprf("%s falls away!", eq.c_str());
            else
                mprf("%s fall away!", eq.c_str());
        }
        else
        {
            if (equip->quantity == 1)
                mprf("%s melds into your body.", eq.c_str());
            else
                mprf("%s meld into your body.", eq.c_str());

        }

        if (unequip)
        {
            if (e == EQ_WEAPON)
            {
                unwield_item(!you.berserk());
                canned_msg(MSG_EMPTY_HANDED_NOW);
            }
            else
                unequip_item(e);

            if (mutation)
            {
                // A mutation made us not only lose an equipment slot
                // but actually removed a worn item: Funny!
                xom_is_stimulated(is_artefact(*equip) ? 200 : 100);
            }
        }
        else
            meld_slot(e);
    }

    if (meld)
    {
        for (const equipment_type e : removed)
            if (you.slot_item(e, true) != nullptr)
                unequip_effect(e, you.equip[e], true, true);
    }
}

static void _unmeld_equipment_type(equipment_type e)
{
    item_def& item = you.inv[you.equip[e]];
    bool force_remove = false;

    if (e == EQ_WEAPON)
    {
        if (you.slot_item(EQ_SHIELD)
            && is_shield_incompatible(item, you.slot_item(EQ_SHIELD)))
        {
            force_remove = true;
        }
    }
    else if (item.base_type != OBJ_JEWELLERY)
    {
        // This could happen if the player was mutated during the form.
        if (!can_wear_armour(item, false, true))
            force_remove = true;

        // If you switched weapons during the transformation, make
        // sure you can still wear your shield.
        // (This is only possible with Statue Form.)
        if (e == EQ_SHIELD && you.weapon()
            && is_shield_incompatible(*you.weapon(), &item))
        {
            force_remove = true;
        }
    }

    if (force_remove)
    {
        mprf("%s is pushed off your body!", item.name(DESC_YOUR).c_str());
        unequip_item(e);
    }
    else
    {
        mprf("%s unmelds from your body.", item.name(DESC_YOUR).c_str());
        unmeld_slot(e);
    }
}

static void _unmeld_equipment(const set<equipment_type>& melded)
{
    // Unmeld items in order.
    for (const equipment_type e : melded)
    {
        if (you.equip[e] == -1)
            continue;

        _unmeld_equipment_type(e);
    }

    for (const equipment_type e : melded)
        if (you.equip[e] != -1)
            equip_effect(e, you.equip[e], true, true);
}

static bool _lears_takes_slot(equipment_type eq)
{
    return eq >= EQ_HELMET && eq <= EQ_BOOTS
        || eq == EQ_BODY_ARMOUR;
}

static bool _form_melds_lears(transformation which_trans)
{
    for (equipment_type eq : _init_equipment_removal(which_trans))
        if (_lears_takes_slot(eq))
            return true;
    return false;
}

void unmeld_one_equip(equipment_type eq)
{
    if (_lears_takes_slot(eq))
    {
        const item_def* arm = you.slot_item(EQ_BODY_ARMOUR, true);
        if (arm && is_unrandom_artefact(*arm, UNRAND_LEAR))
        {
            // Don't unmeld lears when de-fishtailing if you're in
            // a form that should keep it melded.
            if (_form_melds_lears(you.form))
                return;
            eq = EQ_BODY_ARMOUR;
        }
    }

    set<equipment_type> e;
    e.insert(eq);
    _unmeld_equipment(e);
}

void remove_one_equip(equipment_type eq, bool meld, bool mutation)
{
    if (player_equip_unrand(UNRAND_LEAR) && _lears_takes_slot(eq))
        eq = EQ_BODY_ARMOUR;

    set<equipment_type> r;
    r.insert(eq);
    _remove_equipment(r, you.form, meld, mutation);
}

/**
 * Get an monster type corresponding to the player's current form.
 *
 * (Used for console player glyphs.)
 *
 * @return  A monster type corresponding to the player in the form.
 */
monster_type transform_mons()
{
    return get_form()->get_equivalent_mons();
}

/**
 * What is the name of the player parts that will become blades?
 */
string blade_parts(bool terse)
{
    // there's special casing in base_hand_name to use "blade" everywhere, so
    // use the non-temp name
    string str = you.base_hand_name(true, false);

    // creatures with paws (aka felids) have four paws, but only two of them
    // turn into blades.
    if (!terse && you.has_mutation(MUT_PAWS, false))
        str = "front " + str;
    else if (!terse && you.arm_count() > 2)
        str = "main " + str; // Op have four main tentacles

    return str;
}

// with a denominator of 10
int form_hp_mod()
{
    return get_form()->hp_mod;
}

static bool _flying_in_new_form(transformation which_trans)
{
    if (get_form(which_trans)->forbids_flight())
        return false;

    // sources of permanent flight besides equipment
    if (you.permanent_flight(false))
        return true;

    // not airborne right now (XX does this handle emergency flight correctly?)
    if (!you.duration[DUR_FLIGHT] && !you.attribute[ATTR_PERM_FLIGHT])
        return false;

    // Finally, do the calculation about what would be melded: are there equip
    // sources left?
    int sources = you.equip_flight();
    int sources_removed = 0;
    for (auto eq : _init_equipment_removal(which_trans))
    {
        item_def *item = you.slot_item(eq, true);
        if (item == nullptr)
            continue;
        item_def inf = get_item_known_info(*item);

        //similar code to safe_to_remove from item-use.cc
        if (inf.is_type(OBJ_JEWELLERY, RING_FLIGHT))
            sources_removed++;
        if (inf.base_type == OBJ_ARMOUR && inf.brand == SPARM_FLYING)
            sources_removed++;
        if (is_artefact(inf) && artefact_known_property(inf, ARTP_FLY))
            sources_removed++;
    }

    return sources > sources_removed;
}

/**
 * Check if it'd be lethal for the player to enter a form in a given terrain.
 *
 * In addition to checking whether the feature is dangerous for the form
 * itself (form_likes_*), the function checks to see if the player is safe
 * due to flying or similar effects.
 *
 * @param which_trans       The form being checked.
 * @param feat              The dungeon feature to be checked for danger.
 * @return                  If the feat is lethal for the player in the form.
 */
bool feat_dangerous_for_form(transformation which_trans,
                             dungeon_feature_type feat)
{
    // Everything is okay if we can fly.
    if (form_can_fly(which_trans) || _flying_in_new_form(which_trans))
        return false;

    if (feat == DNGN_LAVA)
        return true;

    if (feat == DNGN_DEEP_WATER)
        return !you.can_water_walk() && !form_likes_water(which_trans);

    return false;
}

static mutation_type appendages[] =
{
    MUT_HORNS,
    MUT_TENTACLE_SPIKE,
    MUT_TALONS,
};

static int _beastly_level(mutation_type mut)
{
    switch (mut)
    {
        case MUT_TENTACLE_SPIKE:
            return 3;
        default:
            return 2;
    }
}

static bool _transformation_is_safe(transformation which_trans,
                                    dungeon_feature_type feat,
                                    string *fail_reason)
{
    if (!feat_dangerous_for_form(which_trans, feat) || you.duration[DUR_FLIGHT])
        return true;

    if (fail_reason)
    {
        if (feat == DNGN_DEEP_WATER)
            *fail_reason = "You would drown in your new form.";
        else
            *fail_reason = "You would burn in your new form.";
    }

    return false;
}

/**
 * If we transform into the given form, will all of our stats remain above 0,
 * based purely on the stat modifiers of the current & destination form?
 *
 * May prompt the player.
 *
 * @param new_form  The form to check the safety of.
 * @param quiet     Whether to prompt the player.
 * @return          Whether it's okay to go ahead with the transformation.
 */
bool check_form_stat_safety(transformation new_form, bool quiet)
{
    const int str_mod = get_form(new_form)->str_mod - get_form()->str_mod;
    const int dex_mod = get_form(new_form)->dex_mod - get_form()->dex_mod;

    const bool bad_str = you.strength() > 0 && str_mod + you.strength() <= 0;
    const bool bad_dex = you.dex() > 0 && dex_mod + you.dex() <= 0;
    if (!bad_str && !bad_dex)
        return true;
    if (quiet)
        return false;

    string prompt;
    if (new_form != transformation::none)
    {
        if (bad_str)
            prompt = "Transforming will reduce your strength to zero. Continue?";
        else
            prompt = "Transforming will reduce your dexterity to zero. Continue?";
    }
    else
    {
        if (bad_str)
            prompt = "Turning back will reduce your strength to zero. Continue?";
        else
            prompt = "Turning back will reduce your dexterity to zero. Continue?";
    }

    if (yesno(prompt.c_str(), false, 'n'))
        return true;

    canned_msg(MSG_OK);
    return false;
}

static int _transform_duration(transformation which_trans, int pow)
{
    return get_form(which_trans)->get_duration(pow);
}

/**
 * Is the player alive enough to become the given form?
 *
 * All undead can enter shadow form; vampires also can enter batform, and, when
 * full, other forms (excepting lichform).
 *
 * @param which_trans   The tranformation which the player is undergoing
 *                      (default you.form).
 * @param involuntary   Whether the transformation is involuntary or not.
 * @return              UFR_GOOD if the player is not blocked from entering the
 *                      given form by their undead race; UFR_TOO_ALIVE if the
 *                      player is too satiated as a vampire; UFR_TOO_DEAD if
 *                      the player is too dead (or too thirsty as a vampire).
 */
undead_form_reason lifeless_prevents_form(transformation which_trans,
                                          bool involuntary)
{
    if (!you.undead_state(false))
        return UFR_GOOD; // not undead!

    if (which_trans == transformation::none)
        return UFR_GOOD; // everything can become itself

    if (which_trans == transformation::shadow)
        return UFR_GOOD; // even the undead can use dith's shadow form

    if (!you.has_mutation(MUT_VAMPIRISM))
        return UFR_TOO_DEAD; // ghouls & mummies can't become anything else

    if (which_trans == transformation::lich)
        return UFR_TOO_DEAD; // vampires can never lichform

    if (which_trans == transformation::bat) // can batform bloodless
    {
        if (involuntary)
            return UFR_TOO_DEAD; // but not as a forced polymorph effect

        return !you.vampire_alive ? UFR_GOOD : UFR_TOO_ALIVE;
    }

    // other forms can only be entered when alive
    return you.vampire_alive ? UFR_GOOD : UFR_TOO_DEAD;
}

/**
 * Attempts to transform the player into the specified form.
 *
 * If the player is already in that form, attempt to refresh its duration and
 * power.
 *
 * @param pow               Thw power of the transformation (equivalent to
 *                          spellpower of form spells)
 * @param which_trans       The form which the player should become.
 * @param involuntary       Checks for inscription warnings are skipped, and
 *                          failure is silent.
 * @param just_check        A dry run; just check to see whether the player
 *                          *can* enter the given form, but don't actually
 *                          transform them.
 * @return                  If the player was transformed, or if they were
 *                          already in the given form, returns true.
 *                          Otherwise, false.
 *                          If just_check is set, returns true if the player
 *                          could enter the form (or is in it already) and
 *                          false otherwise.
 *                          N.b. that transform() can fail even when a
 *                          just_check run returns true; e.g. when Zin decides
 *                          to intervene. (That may be the only case.)
 */
bool transform(int pow, transformation which_trans, bool involuntary,
               bool just_check, string *fail_reason)
{
    const transformation previous_trans = you.form;
    const bool was_flying = you.airborne();
    bool success = true;
    string msg;

    // Zin's protection.
    if (!just_check && have_passive(passive_t::resist_polymorph)
        && x_chance_in_y(you.piety, MAX_PIETY)
        && which_trans != transformation::none)
    {
        simple_god_message(" protects your body from unnatural transformation!");
        return false;
    }

    if (!involuntary && crawl_state.is_god_acting())
        involuntary = true;

    if (you.transform_uncancellable)
    {
        msg = "You are stuck in your current form!";
        success = false;
    }
    else if (!_transformation_is_safe(which_trans, env.grid(you.pos()), &msg))
        success = false;

    if (!success)
    {
        // Message is not printed if we're updating fail_reason.
        if (fail_reason)
            *fail_reason = msg;
        else if (!involuntary)
            mpr(msg);
        return false;
    }

    // This must occur before the untransform() and the undead_state() check.
    if (previous_trans == which_trans)
    {
        if (just_check)
            return true;

        // update power
        if (which_trans != transformation::none)
        {
            you.props[TRANSFORM_POW_KEY] = pow;
            you.redraw_armour_class = true;
            // ^ could check more carefully for the exact cases, but I'm
            // worried about making the code too fragile
        }

        int dur = _transform_duration(which_trans, pow);
        if (you.duration[DUR_TRANSFORMATION] < dur * BASELINE_DELAY)
        {
            mpr("You extend your transformation's duration.");
            you.duration[DUR_TRANSFORMATION] = dur * BASELINE_DELAY;

        }
        else if (!involuntary && which_trans != transformation::none)
            mpr("You fail to extend your transformation any further.");

        return true;
    }

    // the undead cannot enter most forms.
    if (lifeless_prevents_form(which_trans, involuntary) == UFR_TOO_DEAD)
    {
        msg = "Your unliving flesh cannot be transformed in this way.";
        success = false;
    }
    else if (which_trans == transformation::lich
             && you.duration[DUR_DEATHS_DOOR])
    {
        msg = "You cannot become a lich while in death's door.";
        success = false;
    }

    if (!just_check && previous_trans != transformation::none)
        untransform(true);

    set<equipment_type> rem_stuff = _init_equipment_removal(which_trans);

    // if going into lichform causes us to drop a holy weapon with consequences
    // for unwielding (e.g. contam), warn first.
    item_def nil_item;
    nil_item.link = -1;
    if (just_check && !involuntary
        && which_trans == transformation::lich && rem_stuff.count(EQ_WEAPON)
        && !check_old_item_warning(nil_item, OPER_WIELD, true))
    {
        canned_msg(MSG_OK);
        return false;
    }

    if (which_trans == transformation::appendage)
    {
        // Need to set the appendages here for messaging
        for (mutation_type app : appendages)
        {
            if (physiology_mutation_conflict(app)
                || you.get_base_mutation_level(app) > 0)
            {
                continue;
            }
            you.props[APPENDAGE_KEY].get_vector().push_back(app);
            dprf("Setting appendage mutation %s.", mutation_name(app));
        }

        if (you.props[APPENDAGE_KEY].get_vector().empty())
        {
            msg = "You have no appropriate body parts free.";
            success = false; // XXX: VERY dubious, since an untransform occurred
        }

        if (just_check || !success)
        {
            you.props.erase(APPENDAGE_KEY);
            dprf("Erasing, just check");
        }
        dprf("Set appendages");
    }

    if (!success)
    {
        // Message is not printed if we're updating fail_reason.
        if (fail_reason)
            *fail_reason = msg;
        else if (!involuntary)
            mpr(msg);
        return false;
    }

    // If we're just pretending return now.
    if (just_check)
        return true;

    // All checks done, transformation will take place now.
    you.redraw_evasion      = true;
    you.redraw_armour_class = true;
    you.wield_change        = true;
    quiver::set_needs_redraw();

    if (form_changed_physiology(which_trans))
        merfolk_stop_swimming();

    if (which_trans == transformation::storm)
        set_airform_power(pow);

    // Give the transformation message.
    get_form(which_trans)->do_transform_message(previous_trans);

    // Update your status.
    // Order matters here, take stuff off (and handle attendant HP and stat
    // changes) before adjusting the player to be transformed.
    _remove_equipment(rem_stuff, which_trans);

    you.form = which_trans;
    you.set_duration(DUR_TRANSFORMATION, _transform_duration(which_trans, pow));
    update_player_symbol();

    you.props[TRANSFORM_POW_KEY] = pow;

    const int str_mod = get_form(which_trans)->str_mod;
    const int dex_mod = get_form(which_trans)->dex_mod;

    if (str_mod)
        notify_stat_change(STAT_STR, str_mod, true);

    if (dex_mod)
        notify_stat_change(STAT_DEX, dex_mod, true);

    calc_hp(true, false);

    if (you.digging && !form_keeps_mutations(which_trans))
    {
        mpr("Your mandibles meld away.");
        you.digging = false;
    }

    // Extra effects
    switch (which_trans)
    {
    case transformation::statue:
        if (you.duration[DUR_ICY_ARMOUR])
        {
            mprf(MSGCH_DURATION, "Your new body cracks your icy armour.");
            you.duration[DUR_ICY_ARMOUR] = 0;
        }
        break;

    case transformation::spider:
        leave_web();
        break;

    case transformation::tree:
        mpr("Your roots penetrate the ground.");
        if (you.duration[DUR_TELEPORT])
        {
            you.duration[DUR_TELEPORT] = 0;
            mpr("You feel strangely stable.");
        }
        you.duration[DUR_FLIGHT] = 0;
        // break out of webs/nets as well

    case transformation::dragon:
        if (you.attribute[ATTR_HELD])
        {
            trap_def *trap = trap_at(you.pos());
            if (trap && trap->type == TRAP_WEB)
            {
                mpr("You shred the web into pieces!");
                destroy_trap(you.pos());
            }
            int net = get_trapping_net(you.pos());
            if (net != NON_ITEM)
            {
                mpr("The net rips apart!");
                destroy_item(net);
            }

            stop_being_held();
        }
        break;

    case transformation::lich:
        if (you.duration[DUR_WEREBLOOD])
        {
            you.duration[DUR_WEREBLOOD] = 0;
            mpr("Your lifeless body cannot sustain the wereblood!");
        }
        you.redraw_status_lights = true;
        break;

    case transformation::appendage:
        {
            auto& apps = you.props[APPENDAGE_KEY].get_vector();
            for (auto app : apps)
            {
                const mutation_type mut = static_cast<mutation_type>(app.get_int());
                you.mutation[mut] = _beastly_level(mut);
            }
        }
        break;

    case transformation::shadow:
        drain_player(25, true, true);
        if (you.invisible())
            mpr("You fade into the shadows.");
        else
            mpr("You feel less conspicuous.");
        break;

    default:
        break;
    }

    // Stop constricting if we can no longer constrict. If any size-changing
    // transformations were to allow constriction, we would have to check
    // relative sizes as well. Likewise, if any transformations were to allow
    // normally non-constricting players to constrict, this would need to
    // be changed.
    if (!form_keeps_mutations(which_trans))
        you.stop_directly_constricting_all(false);

    // Stop being constricted if we are now too large.
    if (you.is_directly_constricted())
    {
        actor* const constrictor = actor_by_mid(you.constricted_by);
        ASSERT(constrictor);

        if (you.body_size(PSIZE_BODY) > constrictor->body_size(PSIZE_BODY))
            you.stop_being_constricted();
    }


    // If we are no longer living, end an effect that afflicts only the living
    if (you.duration[DUR_FLAYED] && !(you.holiness() & MH_NATURAL))
    {
        // Heal a little extra if we gained max hp from this transformation
        if (form_hp_mod() != 10)
        {
            int dam = you.props["flay_damage"].get_int();
            you.heal((dam * form_hp_mod() / 10) - dam);
        }
        heal_flayed_effect(&you);
    }

    // This only has an effect if the transformation happens passively,
    // for example if Xom decides to transform you while you're busy
    // running around or butchering corpses.
    // If you're turned into a tree, you stop taking stairs.
    stop_delay(which_trans == transformation::tree);

    if (crawl_state.which_god_acting() == GOD_XOM)
       you.transform_uncancellable = true;

    // Land the player if we stopped flying.
    if (was_flying && !you.airborne())
        move_player_to_grid(you.pos(), false);

    // Stop emergency flight if it's activated and this form can fly
    if (you.props[EMERGENCY_FLIGHT_KEY]
        && form_can_fly()
        && you.airborne())
    {
        you.props.erase(EMERGENCY_FLIGHT_KEY);
    }

    // Update merfolk swimming for the form change.
    if (you.has_innate_mutation(MUT_MERTAIL))
        merfolk_check_swimming(false);

    // Update skill boosts for the current state of equipment melds
    // Must happen before the HP check!
    ash_check_bondage();

    if (you.hp <= 0)
    {
        ouch(0, KILLED_BY_FRAILTY, MID_NOBODY,
             make_stringf("gaining the %s transformation",
                          transform_name(which_trans)).c_str());
    }

    return true;
}

/**
 * End the player's transformation and return them to their normal
 * form.
 * @param skip_move      If true, skip any move that was in progress before
 *                       the transformation ended.
 */
void untransform(bool skip_move)
{
    const bool was_flying = you.airborne();

    // Must be unset first or else infinite loops might result. -- bwr
    const transformation old_form = you.form;

    quiver::set_needs_redraw();
    you.redraw_evasion          = true;
    you.redraw_armour_class     = true;
    you.wield_change            = true;
    if (!form_can_wield(old_form))
        you.received_weapon_warning = false;
    if (you.props.exists(TRANSFORM_POW_KEY))
        you.props.erase(TRANSFORM_POW_KEY);
    if (you.props.exists(HYDRA_FORM_HEADS_KEY))
        you.props.erase(HYDRA_FORM_HEADS_KEY);
    if (you.props.exists(AIRFORM_POWER_KEY))
        you.props.erase(AIRFORM_POWER_KEY);

    // We may have to unmeld a couple of equipment types.
    set<equipment_type> melded = _init_equipment_removal(old_form);

    you.form = transformation::none;
    you.duration[DUR_TRANSFORMATION] = 0;
    update_player_symbol();

    if (old_form == transformation::appendage)
    {
        const auto& apps = you.props[APPENDAGE_KEY].get_vector();
        for (auto mut : apps)
        {
            const mutation_type app = static_cast<mutation_type>(mut.get_int());
            const int levels = you.get_base_mutation_level(app);
            // Preserve extra mutation levels acquired after transforming.
            const int extra = max(0, levels - you.get_innate_mutation_level(app)
                                            - _beastly_level(app));
            you.mutation[app] = you.get_innate_mutation_level(app) + extra;

            // The mutation might have been removed already by a conflicting
            // demonspawn innate mutation; no message then.
            if (levels)
            {
                if (app == MUT_TENTACLE_SPIKE)
                {
                    if (you.has_mutation(app))
                        mprf(MSGCH_DURATION, "Your tentacle spike shrinks.");
                    else
                        mprf(MSGCH_DURATION, "Your tentacle spike disappears.");
                }
                else if (app == MUT_HORNS)
                {
                    if (you.has_mutation(app))
                        mprf(MSGCH_DURATION, "Your horns shrink.");
                    else
                        mprf(MSGCH_DURATION, "Your horns disappear.");
                }
                else if (app == MUT_TALONS)
                {
                    if (you.has_mutation(app))
                        mprf(MSGCH_DURATION, "Your talons shrink.");
                    else
                        mprf(MSGCH_DURATION, "Your talons disappear.");
                }
                else
                    die("Unknown appendage type");
            }
        }
        you.props.erase(APPENDAGE_KEY);
    }

    calc_hp(true, false);

    get_form(old_form)->do_untransform_message();

    const int str_mod = get_form(old_form)->str_mod;
    const int dex_mod = get_form(old_form)->dex_mod;

    if (str_mod)
        notify_stat_change(STAT_STR, -str_mod, true);

    if (dex_mod)
        notify_stat_change(STAT_DEX, -dex_mod, true);

    // If you're a mer in water, boots stay melded even after the form ends.
    if (you.fishtail)
    {
        melded.erase(EQ_BOOTS);
        const item_def* arm = you.slot_item(EQ_BODY_ARMOUR, true);
        if (arm && is_unrandom_artefact(*arm, UNRAND_LEAR))
        {
            // I hate you, King Lear.
            melded.erase(EQ_HELMET);
            melded.erase(EQ_GLOVES);
            melded.erase(EQ_BODY_ARMOUR);
        }
    }
    _unmeld_equipment(melded);

    // Update skill boosts for the current state of equipment melds
    // Must happen before the HP check!
    ash_check_bondage();

    if (!skip_move)
    {
        // Land the player if we stopped flying.
        if (is_feat_dangerous(env.grid(you.pos())))
            enable_emergency_flight();
        else if (was_flying && !you.airborne())
            move_player_to_grid(you.pos(), false);

        // Update merfolk swimming for the form change.
        if (you.has_innate_mutation(MUT_MERTAIL))
            merfolk_check_swimming(false);
    }

#ifdef USE_TILE
    if (you.has_innate_mutation(MUT_MERTAIL))
        init_player_doll();
#endif

    // If nagas wear boots while transformed, they fall off again afterwards:
    // I don't believe this is currently possible, and if it is we
    // probably need something better to cover all possibilities.  -bwr

    // Removed barding check, no transformed creatures can wear barding
    // anyway.
    // *coughs* Ahem, blade hands... -- jpeg
    if (you.wear_barding())
    {
        const int arm = you.equip[EQ_BOOTS];

        if (arm != -1 && you.inv[arm].sub_type == ARM_BOOTS)
            remove_one_equip(EQ_BOOTS);
    }

    if (you.hp <= 0)
    {
        ouch(0, KILLED_BY_FRAILTY, MID_NOBODY,
             make_stringf("losing the %s form",
                          transform_name(old_form)).c_str());
    }

    // Stop being constricted if we are now too large.
    if (you.is_directly_constricted())
    {
        actor* const constrictor = actor_by_mid(you.constricted_by);
        if (you.body_size(PSIZE_BODY) > constrictor->body_size(PSIZE_BODY))
            you.stop_being_constricted();
    }

    you.turn_is_over = true;
    if (you.transform_uncancellable)
        you.transform_uncancellable = false;
}

void emergency_untransform()
{
    mpr("You quickly transform back into your natural form.");
    untransform(true); // We're already entering the water.

    if (you.has_innate_mutation(MUT_MERTAIL))
        merfolk_start_swimming(false);
}

/**
 * Update whether a merfolk should be swimming.
 *
 * Idempotent, so can be called after position/transformation change without
 * redundantly checking conditions.
 *
 * @param stepped Whether the player is performing a normal walking move.
 */
void merfolk_check_swimming(bool stepped)
{
    const dungeon_feature_type grid = env.grid(you.pos());
    if (you.ground_level()
        && feat_is_water(grid)
        && you.has_mutation(MUT_MERTAIL))
    {
        merfolk_start_swimming(stepped);
    }
    else
        merfolk_stop_swimming();
}

void merfolk_start_swimming(bool stepped)
{
    if (you.fishtail)
        return;

    if (stepped)
        mpr("Your legs become a tail as you enter the water.");
    else
        mpr("Your legs become a tail as you dive into the water.");

    if (you.invisible())
        mpr("...but don't expect to remain undetected.");

    you.fishtail = true;
    remove_one_equip(EQ_BOOTS);
    you.redraw_evasion = true;

    ash_check_bondage();

#ifdef USE_TILE
    init_player_doll();
#endif
}

void merfolk_stop_swimming()
{
    if (!you.fishtail)
        return;
    you.fishtail = false;
    unmeld_one_equip(EQ_BOOTS);
    you.redraw_evasion = true;

    ash_check_bondage();

#ifdef USE_TILE
    init_player_doll();
#endif
}

void vampire_update_transformations()
{
    const undead_form_reason form_reason = lifeless_prevents_form();
    if (form_reason != UFR_GOOD && you.duration[DUR_TRANSFORMATION])
    {
        print_stats();
        update_screen();
        if (form_reason == UFR_TOO_DEAD)
            mpr(MSGCH_WARN, "Your blood-deprived body can't sustain your transformation.");
        else
            mpr(MSGCH_WARN, "Your blood-filled body can't sustain your transformation.");
        untransform();
    }
}

// TODO: dataify? move to member functions?
int form_base_movespeed(transformation tran)
{
    // statue form is handled as a multiplier in player_speed, not a movespeed.
    if (tran == transformation::bat)
        return 5; // but allowed minimum is six
    else if (tran == transformation::pig)
        return 7;
    else
        return 10;
}
