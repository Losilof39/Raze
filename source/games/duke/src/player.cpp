//-------------------------------------------------------------------------
/*
Copyright (C) 1996, 2003 - 3D Realms Entertainment
Copyright (C) 2000, 2003 - Matt Saettler (EDuke Enhancements)
Copyright (C) 2020 - Christoph Oelckers

This file is part of Enhanced Duke Nukem 3D version 1.5 - Atomic Edition

Duke Nukem 3D is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

Original Source: 1996 - Todd Replogle
Prepared for public release: 03/21/2003 - Charlie Wiederhold, 3D Realms

EDuke enhancements integrated: 04/13/2003 - Matt Saettler

Note: EDuke source was in transition.  Changes are in-progress in the
source as it is released.

*/
//-------------------------------------------------------------------------


#include "ns.h"
#include "global.h"
#include "mapinfo.h"
#include "dukeactor.h"
#include "vm.h"

BEGIN_DUKE_NS 

//---------------------------------------------------------------------------
//
// callback for playercolor CVAR
//
//---------------------------------------------------------------------------

int playercolor2lookup(int color)
{
	static int8_t player_pals[] = { 0, 9, 10, 11, 12, 13, 14, 15, 16, 21, 23, };
	if (color >= 0 && color < 10) return player_pals[color];
	return 0;
}

void PlayerColorChanged(void)
{
	if (ud.recstat != 0)
		return;

	auto& pp = ps[myconnectindex];
	if (ud.multimode > 1)
	{
		//Net_SendClientInfo();
	}
	else
	{
		pp.palookup = ud.user_pals[myconnectindex] = playercolor2lookup(playercolor);
	}
	if (pp.GetActor()->spr.picnum == TILE_APLAYER && pp.GetActor()->spr.pal != 1)
		pp.GetActor()->spr.pal = ud.user_pals[myconnectindex];
}

//---------------------------------------------------------------------------
//
// why is this such a mess?
//
//---------------------------------------------------------------------------

int setpal(struct player_struct* p)
{
	int palette;
	if (p->DrugMode) palette = DRUGPAL;
	else if (p->heat_on) palette = SLIMEPAL;
	else if (!p->insector()) palette = BASEPAL; // don't crash if out of range.
	else if (p->cursector->ceilingpicnum >= TILE_FLOORSLIME && p->cursector->ceilingpicnum <= TILE_FLOORSLIME + 2) palette = SLIMEPAL;
	else if (p->cursector->lotag == ST_2_UNDERWATER) palette = WATERPAL;
	else palette = BASEPAL;
	return palette;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void quickkill(struct player_struct* p)
{
	SetPlayerPal(p, PalEntry(48, 48, 48, 48));

	auto pa = p->GetActor();
	pa->spr.extra = 0;
	pa->spr.cstat |= CSTAT_SPRITE_INVISIBLE;
	if (ud.god == 0) fi.guts(pa, TILE_JIBS6, 8, myconnectindex);
	return;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void forceplayerangle(int snum)
{
	player_struct* p = &ps[snum];
	int n;

	n = 128 - (krand() & 255);

	p->horizon.addadjustment(64);
	p->sync.actions |= SB_CENTERVIEW;
	p->angle.rotscrnang = p->angle.look_ang = buildang(n >> 1);
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void tracers(int x1, int y1, int z1, int x2, int y2, int z2, int n)
{
	int i, xv, yv, zv;
	sectortype* sect = nullptr;

	i = n + 1;
	xv = (x2 - x1) / i;
	yv = (y2 - y1) / i;
	zv = (z2 - z1) / i;

	if ((abs(x1 - x2) + abs(y1 - y2)) < 3084)
		return;

	for (i = n; i > 0; i--)
	{
		x1 += xv;
		y1 += yv;
		z1 += zv;
		updatesector(x1, y1, &sect);
		if (sect)
		{
			if (sect->lotag == 2)
				EGS(sect, x1, y1, z1, TILE_WATERBUBBLE, -32, 4 + (krand() & 3), 4 + (krand() & 3), krand() & 2047, 0, 0, ps[0].GetActor(), 5);
			else
				EGS(sect, x1, y1, z1, TILE_SMALLSMOKE, -32, 14, 14, 0, 0, 0, ps[0].GetActor(), 5);
		}
	}
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

int hits(DDukeActor* actor)
{
	int zoff;
	HitInfo hit{};

	if (actor->spr.picnum == TILE_APLAYER) zoff = isRR() ? PHEIGHT_RR : PHEIGHT_DUKE;
	else zoff = 0;

	hitscan(actor->spr.pos, actor->spr.sector(), { bcos(actor->spr.ang), bsin(actor->spr.ang), 0 }, hit, CLIPMASK1);
	return (FindDistance2D(hit.hitpos.vec2 - actor->spr.pos.vec2));
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

int hitasprite(DDukeActor* actor, DDukeActor** hitsp)
{
	int zoff;
	HitInfo hit{};

	if (badguy(actor))
		zoff = (42 << 8);
	else if (actor->spr.picnum == TILE_APLAYER) zoff = (39 << 8);
	else zoff = 0;

	hitscan({ actor->spr.pos.X, actor->spr.pos.Y, actor->spr.pos.Z - zoff }, actor->spr.sector(), { bcos(actor->spr.ang), bsin(actor->spr.ang), 0 }, hit, CLIPMASK1);
	if (hitsp) *hitsp = hit.actor();

	if (hit.hitWall != nullptr && (hit.hitWall->cstat & CSTAT_WALL_MASKED) && badguy(actor))
		return((1 << 30));

	return (FindDistance2D(hit.hitpos.vec2 - actor->spr.pos.vec2));
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

int hitawall(struct player_struct* p, walltype** hitw)
{
	HitInfo hit{};

	hitscan(p->pos, p->cursector, { p->angle.ang.bcos(), p->angle.ang.bsin(), 0 }, hit, CLIPMASK0);
	if (hitw) *hitw = hit.hitWall;

	return (FindDistance2D(hit.hitpos.vec2 - p->pos.vec2));
}


//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

DDukeActor* aim(DDukeActor* actor, int aang)
{
	bool gotshrinker, gotfreezer;
	int a, k, cans;
	int aimstats[] = { STAT_PLAYER, STAT_DUMMYPLAYER, STAT_ACTOR, STAT_ZOMBIEACTOR };
	int dx1, dy1, dx2, dy2, dx3, dy3, smax, sdist;
	int xv, yv;

	a = actor->spr.ang;

	// Autoaim from DukeGDX.
	if (actor->spr.picnum == TILE_APLAYER)
	{
		int autoaim = Autoaim(actor->spr.yvel);
		if (!autoaim)
		{
			// The chickens in RRRA are homing and must always autoaim.
			if (!isRRRA() || ps[actor->spr.yvel].curr_weapon != CHICKEN_WEAPON)
				return nullptr;
		}
		else if (autoaim == 2)
		{
			int weap;
			if (!isWW2GI())
			{
				weap = ps[actor->spr.yvel].curr_weapon;
			}
			else
			{
				weap = aplWeaponWorksLike(ps[actor->spr.yvel].curr_weapon, actor->spr.yvel);
			}
			if (weap > CHAINGUN_WEAPON || weap == KNEE_WEAPON)
			{
				return nullptr;
			}

		}
	}
	DDukeActor* aimed = nullptr;
	//	  if(actor->spr.picnum == TILE_APLAYER && ps[actor->spr.yvel].aim_mode) return -1;

	if (isRR())
	{
		gotshrinker = false;
		gotfreezer = false;
	}
	else if (isWW2GI())
	{
		gotshrinker = actor->spr.picnum == TILE_APLAYER && aplWeaponWorksLike(ps[actor->spr.yvel].curr_weapon, actor->spr.yvel) == SHRINKER_WEAPON;
		gotfreezer = actor->spr.picnum == TILE_APLAYER && aplWeaponWorksLike(ps[actor->spr.yvel].curr_weapon, actor->spr.yvel) == FREEZE_WEAPON;
	}
	else
	{
		gotshrinker = actor->spr.picnum == TILE_APLAYER && ps[actor->spr.yvel].curr_weapon == SHRINKER_WEAPON;
		gotfreezer = actor->spr.picnum == TILE_APLAYER && ps[actor->spr.yvel].curr_weapon == FREEZE_WEAPON;
	}

	smax = 0x7fffffff;

	dx1 = bcos(a - aang);
	dy1 = bsin(a - aang);
	dx2 = bcos(a + aang);
	dy2 = bsin(a + aang);

	dx3 = bcos(a);
	dy3 = bsin(a);

	for (k = 0; k < 4; k++)
	{
		if (aimed)
			break;

		DukeStatIterator it(aimstats[k]);
		while (auto act = it.Next())
		{
			if (act->spr.xrepeat > 0 && act->spr.extra >= 0 && (act->spr.cstat & (CSTAT_SPRITE_BLOCK_ALL | CSTAT_SPRITE_INVISIBLE)) == CSTAT_SPRITE_BLOCK_ALL)
				if (badguy(act) || k < 2)
				{
					if (badguy(act) || act->spr.picnum == TILE_APLAYER)
					{
						if (act->spr.picnum == TILE_APLAYER &&
							(isRR() && ud.ffire == 0) &&
							ud.coop == 1 &&
							actor->spr.picnum == TILE_APLAYER &&
							actor != act)
							continue;

						if (gotshrinker && act->spr.xrepeat < 30 && !(gs.actorinfo[act->spr.picnum].flags & SFLAG_SHRINKAUTOAIM)) continue;
						if (gotfreezer && act->spr.pal == 1) continue;
					}

					xv = (act->spr.pos.X - actor->spr.pos.X);
					yv = (act->spr.pos.Y - actor->spr.pos.Y);

					if ((dy1 * xv) <= (dx1 * yv))
						if ((dy2 * xv) >= (dx2 * yv))
						{
							sdist = MulScale(dx3, xv, 14) + MulScale(dy3, yv, 14);
							if (sdist > 512 && sdist < smax)
							{
								if (actor->spr.picnum == TILE_APLAYER)
									a = (abs(Scale(act->spr.pos.Z - actor->spr.pos.Z, 10, sdist) - ps[actor->spr.yvel].horizon.sum().asbuild()) < 100);
								else a = 1;

								cans = cansee(act->spr.pos.X, act->spr.pos.Y, act->spr.pos.Z - (32 << 8) + gs.actorinfo[act->spr.picnum].aimoffset, act->spr.sector(), actor->spr.pos.X, actor->spr.pos.Y, actor->spr.pos.Z - (32 << 8), actor->spr.sector());

								if (a && cans)
								{
									smax = sdist;
									aimed = act;
								}
							}
						}
				}
		}
	}

	return aimed;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void dokneeattack(int snum, const std::initializer_list<int> & respawnlist)
{
	auto p = &ps[snum];

	if (p->knee_incs > 0)
	{
		p->knee_incs++;
		p->horizon.addadjustment(-48);
		p->sync.actions |= SB_CENTERVIEW;
		if (p->knee_incs > 15)
		{
			p->knee_incs = 0;
			p->holster_weapon = 0;
			if (p->weapon_pos < 0)
				p->weapon_pos = -p->weapon_pos;
			if (p->actorsqu != nullptr && dist(p->GetActor(), p->actorsqu) < 1400)
			{
				fi.guts(p->actorsqu, TILE_JIBS6, 7, myconnectindex);
				spawn(p->actorsqu, TILE_BLOODPOOL);
				S_PlayActorSound(SQUISHED, p->actorsqu);
				if (isIn(p->actorsqu->spr.picnum, respawnlist))
				{
					if (p->actorsqu->spr.yvel)
						fi.operaterespawns(p->actorsqu->spr.yvel);
				}

				if (p->actorsqu->spr.picnum == TILE_APLAYER)
				{
					quickkill(&ps[p->actorsqu->spr.yvel]);
					ps[p->actorsqu->spr.yvel].frag_ps = snum;
				}
				else if (badguy(p->actorsqu))
				{
					deletesprite(p->actorsqu);
					p->actors_killed++;
				}
				else deletesprite(p->actorsqu);
			}
			p->actorsqu = nullptr;
		}
	}

}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

int makepainsounds(int snum, int type)
{
	auto p = &ps[snum];
	auto actor = p->GetActor();
	int k = 0;

	switch (type)
	{
	case 0:
		if (rnd(32))
		{
			if (p->boot_amount > 0)
				k = 1;
			else
			{
				if (!S_CheckActorSoundPlaying(actor, DUKE_LONGTERM_PAIN))
					S_PlayActorSound(DUKE_LONGTERM_PAIN, actor);
				SetPlayerPal(p, PalEntry(32, 64, 64, 64));
				actor->spr.extra -= 1 + (krand() & 3);
				if (!S_CheckActorSoundPlaying(actor, SHORT_CIRCUIT))
					S_PlayActorSound(SHORT_CIRCUIT, actor);
			}
		}
		break;
	case 1:
		if (rnd(16))
		{
			if (p->boot_amount > 0)
				k = 1;
			else
			{
				if (!S_CheckActorSoundPlaying(actor, DUKE_LONGTERM_PAIN))
					S_PlayActorSound(DUKE_LONGTERM_PAIN, actor);
				SetPlayerPal(p, PalEntry(32, 0, 8, 0));
				actor->spr.extra -= 1 + (krand() & 3);
			}
		}
		break;
	case 2:
		if (rnd(32))
		{
			if (p->boot_amount > 0)
				k = 1;
			else
			{
				if (!S_CheckActorSoundPlaying(actor, DUKE_LONGTERM_PAIN))
					S_PlayActorSound(DUKE_LONGTERM_PAIN, actor);
				SetPlayerPal(p, PalEntry(32, 8, 0, 0));
				actor->spr.extra -= 1 + (krand() & 3);
			}
		}
		break;
	case 3:
		if ((krand() & 3) == 1)
			if (p->on_ground)
			{
				if (p->OnMotorcycle)
					actor->spr.extra -= 2;
				else
					actor->spr.extra -= 4;
				S_PlayActorSound(DUKE_LONGTERM_PAIN, actor);
			}
		break;
	}
	return k;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void footprints(int snum)
{
	auto p = &ps[snum];
	auto actor = p->GetActor();

	if (p->footprintcount > 0 && p->on_ground)
		if ((p->cursector->floorstat & CSTAT_SECTOR_SLOPE) != 2)
		{
			int j = -1;
			DukeSectIterator it(actor->spr.sector());
			while (auto act = it.Next())
			{
				if (act->spr.picnum == TILE_FOOTPRINTS || act->spr.picnum == TILE_FOOTPRINTS2 || act->spr.picnum == TILE_FOOTPRINTS3 || act->spr.picnum == TILE_FOOTPRINTS4)
					if (abs(act->spr.pos.X - p->pos.X) < 384)
						if (abs(act->spr.pos.Y - p->pos.Y) < 384)
						{
							j = 1;
							break;
						}
			}
			if (j < 0)
			{
				p->footprintcount--;
				if (p->cursector->lotag == 0 && p->cursector->hitag == 0)
				{
					DDukeActor* fprint;
					switch (krand() & 3)
					{
					case 0:	 fprint = spawn(actor, TILE_FOOTPRINTS); break;
					case 1:	 fprint = spawn(actor, TILE_FOOTPRINTS2); break;
					case 2:	 fprint = spawn(actor, TILE_FOOTPRINTS3); break;
					default: fprint = spawn(actor, TILE_FOOTPRINTS4); break;
					}
					if (fprint)
					{
						fprint->spr.pal = p->footprintpal;
						fprint->spr.shade = (int8_t)p->footprintshade;
					}
				}
			}
		}
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

inline void backupplayer(player_struct* p)
{
	p->backuppos();
	p->angle.backup();
	p->horizon.backup();
}

void playerisdead(int snum, int psectlotag, int fz, int cz)
{
	auto p = &ps[snum];
	auto actor = p->GetActor();

	if (p->dead_flag == 0)
	{
		if (actor->spr.pal != 1)
		{
			SetPlayerPal(p, PalEntry(63, 63, 0, 0));
			p->pos.Z -= (16 << 8);
			actor->spr.pos.Z -= (16 << 8);
		}
#if 0
		if (ud.recstat == 1 && ud.multimode < 2)
			closedemowrite();
#endif

		if (actor->spr.pal != 1)
			p->dead_flag = (512 - ((krand() & 1) << 10) + (krand() & 255) - 512) & 2047;

		p->jetpack_on = 0;
		p->holoduke_on = nullptr;

		if (!isRR())S_StopSound(DUKE_JETPACK_IDLE, actor);
		S_StopSound(-1, actor, CHAN_VOICE);


		if (actor->spr.pal != 1 && (actor->spr.cstat & CSTAT_SPRITE_INVISIBLE) == 0) actor->spr.cstat = 0;

		if (ud.multimode > 1 && (actor->spr.pal != 1 || (actor->spr.cstat & CSTAT_SPRITE_INVISIBLE)))
		{
			if (p->frag_ps != snum)
			{
				ps[p->frag_ps].frag++;
				ps[p->frag_ps].frags[snum]++;

				auto pname = PlayerName(p->frag_ps);
				if (snum == screenpeek)
				{
					Printf(PRINT_NOTIFY, "Killed by %s", pname);
				}
				else
				{
					Printf(PRINT_NOTIFY, "Killed %s", pname);
				}

			}
			else p->fraggedself++;

			p->frag_ps = snum;
		}
	}

	if (psectlotag == ST_2_UNDERWATER)
	{
		if (p->on_warping_sector == 0)
		{
			if (abs(p->pos.Z - fz) > (gs.playerheight >> 1))
				p->pos.Z += 348;
		}
		else
		{
			actor->spr.pos.Z -= 512;
			actor->spr.zvel = -348;
		}

		Collision coll;
		clipmove(p->pos, &p->cursector, 0, 0, 164, (4 << 8), (4 << 8), CLIPMASK0, coll);
	}

	backupplayer(p);

	p->horizon.horizoff = p->horizon.horiz = q16horiz(0);

	updatesector(p->pos.X, p->pos.Y, &p->cursector);

	pushmove(&p->pos, &p->cursector, 128L, (4 << 8), (20 << 8), CLIPMASK0);

	if (fz > cz + (16 << 8) && actor->spr.pal != 1)
		p->angle.rotscrnang = buildang(p->dead_flag + ((fz + p->pos.Z) >> 7));

	p->on_warping_sector = 0;

}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

int endoflevel(int snum)
{
	auto p = &ps[snum];

	// the fist puching the end-of-level thing...
	p->fist_incs++;
	if (p->fist_incs == 28)
	{
#if 0
		if (ud.recstat == 1) closedemowrite();
#endif
		S_PlaySound(PIPEBOMB_EXPLODE);
		SetPlayerPal(p, PalEntry(48, 64, 64, 64));
	}
	if (p->fist_incs > 42)
	{
		setnextmap(!!p->buttonpalette);
		return 1;
	}
	return 0;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

int timedexit(int snum)
{
	auto p = &ps[snum];
	p->timebeforeexit--;
	if (p->timebeforeexit == 26 * 5)
	{
		FX_StopAllSounds();
		if (p->customexitsound >= 0)
		{
			S_PlaySound(p->customexitsound);
			FTA(102, p);
		}
	}
	else if (p->timebeforeexit == 1)
	{
		setnextmap(false);
		return true;
	}
	return false;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void playerCrouch(int snum)
{
	auto p = &ps[snum];
	// crouching
	SetGameVarID(g_iReturnVarID, 0, p->GetActor(), snum);
	OnEvent(EVENT_CROUCH, snum, p->GetActor(), -1);
	if (GetGameVarID(g_iReturnVarID, p->GetActor(), snum).value() == 0)
	{
		p->pos.Z += (2048 + 768);
		p->crack_time = CRACK_TIME;
	}
}

void playerJump(int snum, int fz, int cz)
{
	auto p = &ps[snum];
	if (p->jumping_toggle == 0 && p->jumping_counter == 0)
	{
		if ((fz - cz) > (56 << 8))
		{
			SetGameVarID(g_iReturnVarID, 0, p->GetActor(), snum);
			OnEvent(EVENT_JUMP, snum, p->GetActor(), -1);
			if (GetGameVarID(g_iReturnVarID, p->GetActor(), snum).value() == 0)
			{
				p->jumping_counter = 1;
				p->jumping_toggle = 1;
			}
		}
	}
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void player_struct::apply_seasick(double factor)
{
	if (isRRRA() && SeaSick && (dead_flag == 0 || (dead_flag && resurrected)))
	{
		if (SeaSick < 250)
		{
			if (SeaSick >= 180)
				angle.rotscrnang += buildfang(24 * factor);
			else if (SeaSick >= 130)
				angle.rotscrnang -= buildfang(24 * factor);
			else if (SeaSick >= 70)
				angle.rotscrnang += buildfang(24 * factor);
			else if (SeaSick >= 20)
				angle.rotscrnang -= buildfang(24 * factor);
		}
		if (SeaSick < 250)
			angle.look_ang = buildfang(((krand() & 255) - 128) * factor);
	}
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void player_struct::backuppos(bool noclipping)
{
	if (!noclipping)
	{
		opos.X = pos.X;
		opos.Y = pos.Y;
	}
	else
	{
		pos.X = opos.X;
		pos.Y = opos.Y;
	}

	opos.Z = pos.Z;
	bobposx = pos.X;
	bobposy = pos.Y;
	opyoff = pyoff;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void player_struct::backupweapon()
{
	oweapon_sway = weapon_sway;
	oweapon_pos = weapon_pos;
	okickback_pic = kickback_pic;
	orandom_club_frame = random_club_frame;
	ohard_landing = hard_landing;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void player_struct::checkhardlanding()
{
	if (hard_landing > 0)
	{
		horizon.addadjustment(-(hard_landing << 4));
		hard_landing--;
	}
}

void player_struct::playerweaponsway(int xvel)
{
	if (cl_weaponsway)
	{
		if (xvel < 32 || on_ground == 0 || bobcounter == 1024)
		{
			if ((weapon_sway & 2047) > (1024 + 96))
				weapon_sway -= 96;
			else if ((weapon_sway & 2047) < (1024 - 96))
				weapon_sway += 96;
			else oweapon_sway = weapon_sway = 1024;
		}
		else
		{
			weapon_sway = bobcounter;

			if ((bobcounter - oweapon_sway) > 256)
			{
				oweapon_sway = weapon_sway;
			}
		}
	}
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void checklook(int snum, ESyncBits actions)
{
	auto p = &ps[snum];

	if ((actions & SB_LOOK_LEFT) && !p->OnMotorcycle)
	{
		SetGameVarID(g_iReturnVarID, 0, p->GetActor(), snum);
		OnEvent(EVENT_LOOKLEFT, snum, p->GetActor(), -1);
		if (GetGameVarID(g_iReturnVarID, p->GetActor(), snum).value() != 0)
		{
			actions &= ~SB_LOOK_LEFT;
		}
	}

	if ((actions & SB_LOOK_RIGHT) && !p->OnMotorcycle)
	{
		SetGameVarID(g_iReturnVarID, 0, p->GetActor(), snum);
		OnEvent(EVENT_LOOKRIGHT, snum, p->GetActor(), -1);
		if (GetGameVarID(g_iReturnVarID, p->GetActor(), snum).value() != 0)
		{
			actions &= ~SB_LOOK_RIGHT;
		}
	}
	p->angle.backup();
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void playerCenterView(int snum)
{
	auto p = &ps[snum];
	SetGameVarID(g_iReturnVarID, 0, p->GetActor(), snum);
	OnEvent(EVENT_RETURNTOCENTER, snum, p->GetActor(), -1);
	if (GetGameVarID(g_iReturnVarID, p->GetActor(), snum).value() == 0)
	{
		p->sync.actions |= SB_CENTERVIEW;
	}
	else
	{
		p->sync.actions &= ~SB_CENTERVIEW;
	}
}

void playerLookUp(int snum, ESyncBits actions)
{
	auto p = &ps[snum];
	SetGameVarID(g_iReturnVarID, 0, p->GetActor(), snum);
	OnEvent(EVENT_LOOKUP, snum, p->GetActor(), -1);
	if (GetGameVarID(g_iReturnVarID, p->GetActor(), snum).value() == 0)
	{
		p->sync.actions |= SB_CENTERVIEW;
	}
	else
	{
		p->sync.actions &= ~SB_LOOK_UP;
	}
}

void playerLookDown(int snum, ESyncBits actions)
{
	auto p = &ps[snum];
	SetGameVarID(g_iReturnVarID, 0, p->GetActor(), snum);
	OnEvent(EVENT_LOOKDOWN, snum, p->GetActor(), -1);
	if (GetGameVarID(g_iReturnVarID, p->GetActor(), snum).value() == 0)
	{
		p->sync.actions |= SB_CENTERVIEW;
	}
	else
	{
		p->sync.actions &= ~SB_LOOK_DOWN;
	}
}

void playerAimUp(int snum, ESyncBits actions)
{
	auto p = &ps[snum];
	SetGameVarID(g_iReturnVarID, 0, p->GetActor(), snum);
	OnEvent(EVENT_AIMUP, snum, p->GetActor(), -1);
	if (GetGameVarID(g_iReturnVarID, p->GetActor(), snum).value() != 0)
	{
		p->sync.actions &= ~SB_AIM_UP;
	}
}

void playerAimDown(int snum, ESyncBits actions)
{
	auto p = &ps[snum];
	SetGameVarID(g_iReturnVarID, 0, p->GetActor(), snum);
	OnEvent(EVENT_AIMDOWN, snum, p->GetActor(), -1);	// due to a typo in WW2GI's CON files this is the same as EVENT_AIMUP.
	if (GetGameVarID(g_iReturnVarID, p->GetActor(), snum).value() != 0)
	{
		p->sync.actions &= ~SB_AIM_DOWN;
	}
}

//---------------------------------------------------------------------------
//
// split out so that the weapon check can be done right.
//
//---------------------------------------------------------------------------

bool movementBlocked(player_struct *p)
{
	auto blockingweapon = [=]()
	{
		if (isRR()) return false;
		if (isWW2GI()) return aplWeaponWorksLike(p->curr_weapon, p->GetPlayerNum()) == TRIPBOMB_WEAPON;
		else return p->curr_weapon == TRIPBOMB_WEAPON;
	};

	auto weapondelay = [=]()
	{
		if (isWW2GI()) return aplWeaponFireDelay(p->curr_weapon, p->GetPlayerNum());
		else return 4;
	};

	return (p->fist_incs ||
		p->transporter_hold > 2 ||
		p->hard_landing ||
		p->access_incs > 0 ||
		p->knee_incs > 0 ||
		(blockingweapon() && p->kickback_pic > 1 && p->kickback_pic < weapondelay()));
}


//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

int haskey(sectortype* sectp, int snum)
{
	auto p = &ps[snum];
	if (!sectp->keyinfo)
		return 1;
	if (sectp->keyinfo > 6)
		return 1;
	int wk = sectp->keyinfo;
	if (wk > 3)
		wk -= 3;

	if (p->keys[wk] == 1)
	{
		sectp->keyinfo = 0;
		return 1;
	}

	return 0;
}

//---------------------------------------------------------------------------
//
//
//
//---------------------------------------------------------------------------

void shootbloodsplat(DDukeActor* actor, int p, int sx, int sy, int sz, int sa, int atwith, int BIGFORCE, int OOZFILTER, int NEWBEAST)
{
	auto sectp = actor->spr.sector();
	int zvel;
	HitInfo hit{};

	if (p >= 0)
		sa += 64 - (krand() & 127);
	else sa += 1024 + 64 - (krand() & 127);
	zvel = 1024 - (krand() & 2047);


	hitscan({ sx, sy, sz }, sectp, { bcos(sa), bsin(sa), zvel << 6 }, hit, CLIPMASK1);

	// oh my...
	if (FindDistance2D(sx - hit.hitpos.X, sy - hit.hitpos.Y) < 1024 &&
		(hit.hitWall != nullptr && hit.hitWall->overpicnum != BIGFORCE) &&
		((hit.hitWall->twoSided() && hit.hitSector != nullptr &&
			hit.hitWall->nextSector()->lotag == 0 &&
			hit.hitSector->lotag == 0 &&
			(hit.hitSector->floorz - hit.hitWall->nextSector()->floorz) > (16 << 8)) ||
			(!hit.hitWall->twoSided() && hit.hitSector->lotag == 0)))
	{
		if ((hit.hitWall->cstat & CSTAT_WALL_MASKED) == 0)
		{
			if (hit.hitWall->twoSided())
			{
				DukeSectIterator it(hit.hitWall->nextSector());
				while (auto act2 = it.Next())
				{
					if (act2->spr.statnum == STAT_EFFECTOR && act2->spr.lotag == SE_13_EXPLOSIVE)
						return;
				}
			}

			if (hit.hitWall->twoSided() &&
				hit.hitWall->nextWall()->hitag != 0)
				return;

			if (hit.hitWall->hitag == 0)
			{
				auto spawned = spawn(actor, atwith);
				if (spawned)
				{
					spawned->spr.xvel = -12;
					auto delta = hit.hitWall->delta();
					spawned->spr.ang = getangle(-delta.X, -delta.Y) + 512; // note the '-' sign here!
					spawned->spr.pos.X = hit.hitpos.X;
					spawned->spr.pos.Y = hit.hitpos.Y;
					spawned->spr.pos.Z = hit.hitpos.Z;
					spawned->spr.cstat |= randomXFlip();
					ssp(spawned, CLIPMASK0);
					SetActor(spawned, spawned->spr.pos);
					if (actor->spr.picnum == OOZFILTER || actor->spr.picnum == NEWBEAST)
						spawned->spr.pal = 6;
				}
			}
		}
	}
}


DEFINE_FIELD_X(DukePlayer, player_struct, gotweapon)
DEFINE_FIELD_X(DukePlayer, player_struct, pals)
DEFINE_FIELD_X(DukePlayer, player_struct, weapon_sway)
DEFINE_FIELD_X(DukePlayer, player_struct, oweapon_sway)
DEFINE_FIELD_X(DukePlayer, player_struct, weapon_pos)
DEFINE_FIELD_X(DukePlayer, player_struct, kickback_pic)
DEFINE_FIELD_X(DukePlayer, player_struct, random_club_frame)
DEFINE_FIELD_X(DukePlayer, player_struct, oweapon_pos)
DEFINE_FIELD_X(DukePlayer, player_struct, okickback_pic)
DEFINE_FIELD_X(DukePlayer, player_struct, orandom_club_frame)
DEFINE_FIELD_X(DukePlayer, player_struct, hard_landing)
DEFINE_FIELD_X(DukePlayer, player_struct, ohard_landing)
DEFINE_FIELD_X(DukePlayer, player_struct, psectlotag)
DEFINE_FIELD_X(DukePlayer, player_struct, exitx)
DEFINE_FIELD_X(DukePlayer, player_struct, exity)
DEFINE_FIELD_X(DukePlayer, player_struct, loogiex)
DEFINE_FIELD_X(DukePlayer, player_struct, loogiey)
DEFINE_FIELD_X(DukePlayer, player_struct, numloogs)
DEFINE_FIELD_X(DukePlayer, player_struct, loogcnt)
DEFINE_FIELD_X(DukePlayer, player_struct, invdisptime)
DEFINE_FIELD_X(DukePlayer, player_struct, bobposx)
DEFINE_FIELD_X(DukePlayer, player_struct, bobposy)
//DEFINE_FIELD_X(DukePlayer, player_struct, oposx)
//DEFINE_FIELD_X(DukePlayer, player_struct, oposy)
//DEFINE_FIELD_X(DukePlayer, player_struct, oposz)
DEFINE_FIELD_X(DukePlayer, player_struct, pyoff)
DEFINE_FIELD_X(DukePlayer, player_struct, opyoff)
DEFINE_FIELD_X(DukePlayer, player_struct, posxv)
DEFINE_FIELD_X(DukePlayer, player_struct, posyv)
DEFINE_FIELD_X(DukePlayer, player_struct, poszv)
DEFINE_FIELD_X(DukePlayer, player_struct, last_pissed_time)
DEFINE_FIELD_X(DukePlayer, player_struct, truefz)
DEFINE_FIELD_X(DukePlayer, player_struct, truecz)
DEFINE_FIELD_X(DukePlayer, player_struct, player_par)
DEFINE_FIELD_X(DukePlayer, player_struct, visibility)
DEFINE_FIELD_X(DukePlayer, player_struct, bobcounter)
DEFINE_FIELD_X(DukePlayer, player_struct, randomflamex)
DEFINE_FIELD_X(DukePlayer, player_struct, crack_time)
DEFINE_FIELD_X(DukePlayer, player_struct, aim_mode)
DEFINE_FIELD_X(DukePlayer, player_struct, ftt)
DEFINE_FIELD_X(DukePlayer, player_struct, cursector)
DEFINE_FIELD_X(DukePlayer, player_struct, last_extra)
DEFINE_FIELD_X(DukePlayer, player_struct, subweapon)
DEFINE_FIELD_X(DukePlayer, player_struct, ammo_amount)
DEFINE_FIELD_X(DukePlayer, player_struct, frag)
DEFINE_FIELD_X(DukePlayer, player_struct, fraggedself)
DEFINE_FIELD_X(DukePlayer, player_struct, curr_weapon)
DEFINE_FIELD_X(DukePlayer, player_struct, last_weapon)
DEFINE_FIELD_X(DukePlayer, player_struct, tipincs)
DEFINE_FIELD_X(DukePlayer, player_struct, wantweaponfire)
DEFINE_FIELD_X(DukePlayer, player_struct, holoduke_amount)
DEFINE_FIELD_X(DukePlayer, player_struct, hurt_delay)
DEFINE_FIELD_X(DukePlayer, player_struct, hbomb_hold_delay)
DEFINE_FIELD_X(DukePlayer, player_struct, jumping_counter)
DEFINE_FIELD_X(DukePlayer, player_struct, airleft)
DEFINE_FIELD_X(DukePlayer, player_struct, knee_incs)
DEFINE_FIELD_X(DukePlayer, player_struct, access_incs)
DEFINE_FIELD_X(DukePlayer, player_struct, ftq)
DEFINE_FIELD_X(DukePlayer, player_struct, access_wall)
DEFINE_FIELD_X(DukePlayer, player_struct, got_access)
DEFINE_FIELD_X(DukePlayer, player_struct, weapon_ang)
DEFINE_FIELD_X(DukePlayer, player_struct, firstaid_amount)
DEFINE_FIELD_X(DukePlayer, player_struct, actor)
DEFINE_FIELD_X(DukePlayer, player_struct, one_parallax_sectnum)
DEFINE_FIELD_X(DukePlayer, player_struct, over_shoulder_on)
DEFINE_FIELD_X(DukePlayer, player_struct, fist_incs)
DEFINE_FIELD_X(DukePlayer, player_struct, cheat_phase)
DEFINE_FIELD_X(DukePlayer, player_struct, extra_extra8)
DEFINE_FIELD_X(DukePlayer, player_struct, quick_kick)
DEFINE_FIELD_X(DukePlayer, player_struct, last_quick_kick)
DEFINE_FIELD_X(DukePlayer, player_struct, heat_amount)
DEFINE_FIELD_X(DukePlayer, player_struct, timebeforeexit)
DEFINE_FIELD_X(DukePlayer, player_struct, customexitsound)
DEFINE_FIELD_X(DukePlayer, player_struct, weaprecs)
DEFINE_FIELD_X(DukePlayer, player_struct, weapreccnt)
DEFINE_FIELD_X(DukePlayer, player_struct, interface_toggle_flag)
DEFINE_FIELD_X(DukePlayer, player_struct, dead_flag)
DEFINE_FIELD_X(DukePlayer, player_struct, show_empty_weapon)
DEFINE_FIELD_X(DukePlayer, player_struct, scuba_amount)
DEFINE_FIELD_X(DukePlayer, player_struct, jetpack_amount)
DEFINE_FIELD_X(DukePlayer, player_struct, steroids_amount)
DEFINE_FIELD_X(DukePlayer, player_struct, shield_amount)
DEFINE_FIELD_X(DukePlayer, player_struct, pycount)
DEFINE_FIELD_X(DukePlayer, player_struct, frag_ps)
DEFINE_FIELD_X(DukePlayer, player_struct, transporter_hold)
DEFINE_FIELD_X(DukePlayer, player_struct, last_full_weapon)
DEFINE_FIELD_X(DukePlayer, player_struct, footprintshade)
DEFINE_FIELD_X(DukePlayer, player_struct, boot_amount)
DEFINE_FIELD_X(DukePlayer, player_struct, on_warping_sector)
DEFINE_FIELD_X(DukePlayer, player_struct, footprintcount)
DEFINE_FIELD_X(DukePlayer, player_struct, hbomb_on)
DEFINE_FIELD_X(DukePlayer, player_struct, jumping_toggle)
DEFINE_FIELD_X(DukePlayer, player_struct, rapid_fire_hold)
DEFINE_FIELD_X(DukePlayer, player_struct, on_ground)
DEFINE_FIELD_X(DukePlayer, player_struct, inven_icon)
DEFINE_FIELD_X(DukePlayer, player_struct, buttonpalette)
DEFINE_FIELD_X(DukePlayer, player_struct, jetpack_on)
DEFINE_FIELD_X(DukePlayer, player_struct, spritebridge)
DEFINE_FIELD_X(DukePlayer, player_struct, lastrandomspot)
DEFINE_FIELD_X(DukePlayer, player_struct, scuba_on)
DEFINE_FIELD_X(DukePlayer, player_struct, footprintpal)
DEFINE_FIELD_X(DukePlayer, player_struct, heat_on)
DEFINE_FIELD_X(DukePlayer, player_struct, holster_weapon)
DEFINE_FIELD_X(DukePlayer, player_struct, falling_counter)
DEFINE_FIELD_X(DukePlayer, player_struct, refresh_inventory)
DEFINE_FIELD_X(DukePlayer, player_struct, toggle_key_flag)
DEFINE_FIELD_X(DukePlayer, player_struct, knuckle_incs)
DEFINE_FIELD_X(DukePlayer, player_struct, walking_snd_toggle)
DEFINE_FIELD_X(DukePlayer, player_struct, palookup)
DEFINE_FIELD_X(DukePlayer, player_struct, quick_kick_msg)
DEFINE_FIELD_X(DukePlayer, player_struct, max_secret_rooms)
DEFINE_FIELD_X(DukePlayer, player_struct, secret_rooms)
DEFINE_FIELD_X(DukePlayer, player_struct, max_actors_killed)
DEFINE_FIELD_X(DukePlayer, player_struct, actors_killed)
DEFINE_FIELD_X(DukePlayer, player_struct, resurrected)
DEFINE_FIELD_X(DukePlayer, player_struct, stairs)
DEFINE_FIELD_X(DukePlayer, player_struct, detonate_count)
DEFINE_FIELD_X(DukePlayer, player_struct, noise_x)
DEFINE_FIELD_X(DukePlayer, player_struct, noise_y)
DEFINE_FIELD_X(DukePlayer, player_struct, noise_radius)
DEFINE_FIELD_X(DukePlayer, player_struct, drink_timer)
DEFINE_FIELD_X(DukePlayer, player_struct, eat_timer)
DEFINE_FIELD_X(DukePlayer, player_struct, SlotWin)
DEFINE_FIELD_X(DukePlayer, player_struct, recoil)
DEFINE_FIELD_X(DukePlayer, player_struct, detonate_time)
DEFINE_FIELD_X(DukePlayer, player_struct, yehaa_timer)
DEFINE_FIELD_X(DukePlayer, player_struct, drink_amt)
DEFINE_FIELD_X(DukePlayer, player_struct, eat)
DEFINE_FIELD_X(DukePlayer, player_struct, drunkang)
DEFINE_FIELD_X(DukePlayer, player_struct, eatang)
DEFINE_FIELD_X(DukePlayer, player_struct, shotgun_state)
DEFINE_FIELD_X(DukePlayer, player_struct, donoise)
DEFINE_FIELD_X(DukePlayer, player_struct, keys)
DEFINE_FIELD_X(DukePlayer, player_struct, drug_aspect)
DEFINE_FIELD_X(DukePlayer, player_struct, drug_timer)
DEFINE_FIELD_X(DukePlayer, player_struct, SeaSick)
DEFINE_FIELD_X(DukePlayer, player_struct, MamaEnd)
DEFINE_FIELD_X(DukePlayer, player_struct, moto_drink)
DEFINE_FIELD_X(DukePlayer, player_struct, TiltStatus)
DEFINE_FIELD_X(DukePlayer, player_struct, oTiltStatus)
DEFINE_FIELD_X(DukePlayer, player_struct, VBumpNow)
DEFINE_FIELD_X(DukePlayer, player_struct, VBumpTarget)
DEFINE_FIELD_X(DukePlayer, player_struct, TurbCount)
DEFINE_FIELD_X(DukePlayer, player_struct, drug_stat)
DEFINE_FIELD_X(DukePlayer, player_struct, DrugMode)
DEFINE_FIELD_X(DukePlayer, player_struct, lotag800kill)
DEFINE_FIELD_X(DukePlayer, player_struct, sea_sick_stat)
DEFINE_FIELD_X(DukePlayer, player_struct, hurt_delay2)
DEFINE_FIELD_X(DukePlayer, player_struct, nocheat)
DEFINE_FIELD_X(DukePlayer, player_struct, OnMotorcycle)
DEFINE_FIELD_X(DukePlayer, player_struct, OnBoat)
DEFINE_FIELD_X(DukePlayer, player_struct, moto_underwater)
DEFINE_FIELD_X(DukePlayer, player_struct, NotOnWater)
DEFINE_FIELD_X(DukePlayer, player_struct, MotoOnGround)
DEFINE_FIELD_X(DukePlayer, player_struct, moto_do_bump)
DEFINE_FIELD_X(DukePlayer, player_struct, moto_bump_fast)
DEFINE_FIELD_X(DukePlayer, player_struct, moto_on_oil)
DEFINE_FIELD_X(DukePlayer, player_struct, moto_on_mud)
DEFINE_FIELD_X(DukePlayer, player_struct, vehForwardScale)
DEFINE_FIELD_X(DukePlayer, player_struct, vehReverseScale)
DEFINE_FIELD_X(DukePlayer, player_struct, MotoSpeed)
DEFINE_FIELD_X(DukePlayer, player_struct, vehTurnLeft)
DEFINE_FIELD_X(DukePlayer, player_struct, vehTurnRight)
DEFINE_FIELD_X(DukePlayer, player_struct, vehBraking)
DEFINE_FIELD_X(DukePlayer, player_struct, holoduke_on)

DEFINE_ACTION_FUNCTION(_DukePlayer, IsFrozen)
{
	PARAM_SELF_STRUCT_PROLOGUE(player_struct);
	ACTION_RETURN_BOOL(self->GetActor()->spr.pal == 1 && self->last_extra < 2);
}

DEFINE_ACTION_FUNCTION(_DukePlayer, GetGameVar)
{
	PARAM_SELF_STRUCT_PROLOGUE(player_struct);
	PARAM_STRING(name);
	PARAM_INT(def);
	ACTION_RETURN_INT(GetGameVar(name, def, self->GetActor(), self->GetPlayerNum()).safeValue());
}

DEFINE_ACTION_FUNCTION(_Duke, GetViewPlayer)
{
	ACTION_RETURN_POINTER(&ps[screenpeek]);
}

DEFINE_ACTION_FUNCTION(_Duke, MaxPlayerHealth)
{
	ACTION_RETURN_INT(gs.max_player_health);
}

DEFINE_ACTION_FUNCTION(_Duke, MaxAmmoAmount)
{
	PARAM_PROLOGUE;
	PARAM_INT(weap);
	int max = weap < 0 || weap >= MAX_WEAPONS ? 0 : gs.max_ammo_amount[weap];
	ACTION_RETURN_INT(max);
}

END_DUKE_NS
