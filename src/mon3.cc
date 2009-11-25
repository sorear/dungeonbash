/* mon3.cc
 * 
 * Copyright 2009 Martin Read
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#define MON3_C
#include "dunbash.hh"
#include "monsters.hh"
#include "pmonid.hh"
#include "objects.hh"
#include "bmagic.hh"
#include "combat.hh"

Mon_buff_metadata monbuff_meta[] = 
{
    { "exceptionally strong", Stack_renew },
    { "guided by another mind", Stack_renew },
    { "uncannily perceptive", Stack_extend }
};

Mon_debuff_metadata mondeb_meta[] = 
{
    { "freezing to death", Stack_independent },
    { "on fire", Stack_renew },
    { "embraced by tentacles", Stack_extend }
};

int weaker_demon(int pm_num)
{
    switch (pm_num)
    {
    case PM_FLAYER:
        return PM_LASHER;
    case PM_DOMINATOR:
        return PM_FLAYER;
    case PM_FESTERING_HORROR:
        return PM_FOETID_OOZE;
    case PM_DEFILER:
        return PM_FESTERING_HORROR;
    case PM_IRONGUARD:
        return PM_IRON_SNAKE;
    case PM_IRON_LORD:
        return PM_IRONGUARD;
    default:
        return NO_PM;
    }
}

bool Mon::apply_effect(Persistent_effect eff, int power, int duration)
{
    return false;
}

bool Mon::suffer(Mon_debuff_data& debdata)
{
    switch (debdata.flavour)
    {
    case MONDEB_FROST:
        return damage_mon(self, one_die(debdata.power), debdata.by_you);

    case MONDEB_IGNITED:
        return damage_mon(self, one_die(debdata.power), debdata.by_you);

    case MONDEB_HENTACLE:
        break;

    default:
        print_msg(MSGCHAN_INTERROR, "Monster has debuff of invalid/unimplemented type %d\n", debdata.flavour);
        break;
    }
    return false;
}

void Mon::enjoy(Mon_buff_data& buffdata)
{
    switch (buffdata.flavour)
    {
    default:
        break;
    }
}

void aggravate_monsters(Level *lptr)
{
    std::set<Mon_handle>::iterator iter;
    for (iter = lptr->denizens.begin(); iter != lptr->denizens.end(); ++iter)
    {
        iter->snapv()->awake = true;
    }
}

/* mon3.cc */
