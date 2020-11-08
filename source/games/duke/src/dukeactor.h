#pragma once
#include "dobject.h"
#include "build.h"

BEGIN_DUKE_NS


// Iterator wrappers that return an actor pointer, not an index.
class DukeStatIterator : public StatIterator
{
public:
	DukeStatIterator(int stat) : StatIterator(stat)
	{
	}
	
	DDukeActor *Next()
	{
		int n = NextIndex();
		return n >= 0? &hittype[n] : nullptr;
	}

	DDukeActor *Peek()
	{
		int n = PeekIndex();
		return n >= 0? &hittype[n] : nullptr;
	}
};

class DukeSectIterator : public SectIterator
{
public:
	DukeSectIterator(int stat) : SectIterator(stat)
	{
	}
	
	DDukeActor *Next()
	{
		int n = NextIndex();
		return n >= 0? &hittype[n] : nullptr;
	}

	DDukeActor *Peek()
	{
		int n = PeekIndex();
		return n >= 0? &hittype[n] : nullptr;
	}
};

// An interator to iterate over all sprites.
class DukeSpriteIterator
{
	DukeStatIterator it;
	int stat = STAT_DEFAULT;

public:
	DukeSpriteIterator() : it(STAT_DEFAULT) {}

	DDukeActor* Next()
	{
		while (stat < MAXSTATUS)
		{
			auto ac = it.Next();
			if (ac) return ac;
			stat++;
			if (stat < MAXSTATUS) it.Reset(stat);
		}
		return nullptr;
	}
};

class DukeLinearSpriteIterator
{
	int index = 0;
public:

	DDukeActor* Next()
	{
		while (index < MAXSPRITES)
		{
			auto p = &hittype[index++];
			if (p->s.statnum != MAXSTATUS) return p;
		}
		return nullptr;
	}
};

inline DDukeActor* player_struct::GetActor()
{
	return &hittype[i];
}

inline int player_struct::GetPlayerNum()
{
	return GetActor()->s.yvel;
}

// Refactoring helpers/intermediates
inline void changespritestat(DDukeActor* a, int newstat)
{
	::changespritestat(a->GetIndex(), newstat);
}

// The int version also needs to be wrapped due to namespacing issues.
inline void changespritestat(int i, int newstat)
{
	::changespritestat(i, newstat);
}

inline void changespritesect(DDukeActor* a, int newsect)
{
	::changespritesect(a->GetIndex(), newsect);
}

inline void changespritesect(int i, int newsect)
{
	::changespritesect(i, newsect);
}

inline int setsprite(DDukeActor* a, int x, int y, int z)
{
	return ::setsprite(a->GetIndex(), x, y, z);
}

inline int setsprite(DDukeActor* a, const vec3_t& pos)
{
	return ::setsprite(a->GetIndex(), pos.x, pos.y, pos.z);
}

// see comment for changespritestat.
inline int setsprite(int i, int x, int y, int z)
{
	return ::setsprite(i, x, y, z);
}

inline int ActorToScriptIndex(DDukeActor* a)
{
	if (!a) return -1;
	return a->GetIndex();
}

inline DDukeActor* ScriptIndexToActor(int index)
{
	// only allow valid actors to get through here. Everything else gets null'ed.
	if (index < 0 || index >= MAXSPRITES || hittype[index].s.statnum == MAXSTATUS) return nullptr;
	return &hittype[index];
}

int spawn_d(int j, int pn);
int spawn_r(int j, int pn);

inline DDukeActor* spawn(DDukeActor* spawner, int type)
{
	int i = (isRR()? spawn_r : spawn_d)(spawner ? spawner->GetIndex() : -1, type);
	return i == -1 ? nullptr : &hittype[i];
}

inline int ldist(DDukeActor* s1, DDukeActor* s2)
{
	return ldist(&s1->s, &s2->s);
}

inline int dist(DDukeActor* s1, DDukeActor* s2)
{
	return dist(&s1->s, &s2->s);
}

inline int badguy(DDukeActor* pSprite)
{
	return badguypic(pSprite->s.picnum);
}

inline int bossguy(DDukeActor* pSprite)
{
	return bossguypic(pSprite->s.picnum);
}

// old interface versions of already changed functions

inline void deletesprite(int num)
{
	deletesprite(&hittype[num]);
}

int movesprite_ex_d(DDukeActor* actor, int xchange, int ychange, int zchange, unsigned int cliptype, Collision& result);
int movesprite_ex_r(DDukeActor* actor, int xchange, int ychange, int zchange, unsigned int cliptype, Collision& result);

inline int movesprite_ex(DDukeActor* actor, int xchange, int ychange, int zchange, unsigned int cliptype, Collision& result)
{
	auto f = isRR() ? movesprite_ex_r : movesprite_ex_d;
	return f(actor, xchange, ychange, zchange, cliptype, result);
}

inline int clipmove_ex(int* x, int* y, int* z, short* sect, int xv, int yv, int wal, int ceil, int flor, int ct, Collision& result)
{
	int res = clipmove(x, y, z, sect, xv, yv, wal, ceil, flor, ct);
	return result.setFromEngine(res);
}

inline void getzrange_ex(int x, int y, int z, int16_t sectnum, int32_t* ceilz, Collision& ceilhit, int32_t* florz, Collision& florhit, int32_t walldist, uint32_t cliptype)
{
	int ch, fh;
	getzrange(x, y, z, sectnum, ceilz, &ch, florz, &fh, walldist, cliptype);
	ceilhit.setFromEngine(ch);
	florhit.setFromEngine(fh);
}

inline int hitscan(int x, int y, int z, int16_t sectnum, int32_t vx, int32_t vy, int32_t vz,
	short* hitsect, short* hitwall, DDukeActor** hitspr, int* hitx, int* hity, int* hitz, uint32_t cliptype)
{
	short hitsprt;
	int res = ::hitscan(x, y, z, sectnum, vx, vy, vz, hitsect, hitwall, &hitsprt, hitx, hity, hitz, cliptype);
	if (hitspr) *hitspr = hitsprt == -1 ? nullptr : &hittype[hitsprt];
	return res;
}

inline void   neartag(int32_t xs, int32_t ys, int32_t zs, int16_t sectnum, int16_t ange,
	int16_t* neartagsector, int16_t* neartagwall, DDukeActor** neartagsprite,
	int32_t* neartaghitdist, int32_t neartagrange, uint8_t tagsearch)
{
	int16_t nts;
	::neartag(xs, ys, zs, sectnum, ange, neartagsector, neartagwall, &nts, neartaghitdist, neartagrange, tagsearch);
	*neartagsprite = nts == -1 ? nullptr : &hittype[nts];
}

END_DUKE_NS
