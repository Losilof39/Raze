// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2002-2016 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//
/*
** gl_sprite.cpp
** Sprite/Particle rendering
**
*/

#include "matrix.h"
//#include "models.h"
#include "vectors.h"
#include "texturemanager.h"
#include "basics.h"

//#include "hw_models.h"
#include "hw_drawstructs.h"
#include "hw_drawinfo.h"
#include "hw_portal.h"
#include "flatvertices.h"
#include "hw_cvars.h"
#include "hw_clock.h"
#include "hw_material.h"
#include "hw_dynlightdata.h"
#include "hw_lightbuffer.h"
#include "hw_renderstate.h"

extern PalEntry GlobalMapFog;
extern float GlobalFogDensity;


//==========================================================================
//
// 
//
//==========================================================================

void HWSprite::DrawSprite(HWDrawInfo* di, FRenderState& state, bool translucent)
{
	bool additivefog = false;
	bool foglayer = false;
	auto& vp = di->Viewpoint;

	if (translucent)
	{
		// The translucent pass requires special setup for the various modes.

		// for special render styles brightmaps would not look good - especially for subtractive.
		if (RenderStyle.BlendOp != STYLEOP_Add)
		{
			state.EnableBrightmap(false);
		}

		state.SetRenderStyle(RenderStyle);
		state.SetTextureMode(RenderStyle);

		if (!texture || !texture->GetTranslucency()) state.AlphaFunc(Alpha_GEqual, gl_mask_sprite_threshold);
		else state.AlphaFunc(Alpha_Greater, 0.f);

		if (RenderStyle.BlendOp == STYLEOP_Add && RenderStyle.DestAlpha == STYLEALPHA_One)
		{
			additivefog = true;
		}
	}

	if (dynlightindex == -1)	// only set if we got no light buffer index. This covers all cases where sprite lighting is used.
	{
		float out[3] = {};
		//di->GetDynSpriteLight(gl_light_sprites ? actor : nullptr, gl_light_particles ? particle : nullptr, out);
		//state.SetDynLight(out[0], out[1], out[2]);
	}


	if (RenderStyle.Flags & STYLEF_FadeToBlack)
	{
		fade = 0;
		additivefog = true;
	}

	if (RenderStyle.BlendOp == STYLEOP_RevSub || RenderStyle.BlendOp == STYLEOP_Sub)
	{
		if (!modelframe)
		{
			// non-black fog with subtractive style needs special treatment
			if (fade != 0)
			{
				foglayer = true;
				// Due to the two-layer approach we need to force an alpha test that lets everything pass
				state.AlphaFunc(Alpha_Greater, 0);
			}
		}
		else RenderStyle.BlendOp = STYLEOP_Fuzz;	// subtractive with models is not going to work.
	}

	// Fog must be done before the texture so that the texture selector can override it.
	bool foggy = (GlobalMapFog || (fade & 0xffffff));
	auto ShadeDiv = lookups.tables[palette].ShadeFactor;
	// Disable brightmaps if non-black fog is used.
	int shade = this->shade;
	PalEntry color(255, globalr, globalg, globalb);
	if (this->shade > numshades) // handling of SW's shadow hack using a shade of 127.
	{
		shade = sector[sprite->sectnum].floorshade;
		color = 0xff000000;
	}
	shade = clamp(shade, 0, numshades - 1);

	if (ShadeDiv >= 1 / 1000.f && foggy && !foglayer)
	{
		state.EnableFog(1);
		float density = GlobalMapFog ? GlobalFogDensity : 350.f - Scale(numshades - shade, 150, numshades);
		state.SetFog((GlobalMapFog) ? GlobalMapFog : fade, density);
		state.SetSoftLightLevel(255);
		state.SetLightParms(128.f, 1 / 1000.f);
	}
	else
	{
		state.EnableFog(0);
		state.SetFog(0, 0);
		state.SetSoftLightLevel(ShadeDiv >= 1 / 1000.f ? 255 - Scale(shade, 255, numshades) : 255);
		state.SetLightParms(visibility, ShadeDiv / (numshades - 2));
	}

	// The shade rgb from the tint is ignored here.
	state.SetColorAlpha(color, alpha);

	state.SetMaterial(texture, UF_Texture, 0, CLAMP_XY, TRANSLATION(Translation_Remap + curbasepal, palette), -1);


	if (!modelframe)
	{
		state.SetNormal(0, 0, 0);


		if (screen->BuffersArePersistent())
		{
			CreateVertices(di);
		}
		state.SetLightIndex(-1);
		state.Draw(DT_TriangleStrip, vertexindex, 4);

		if (ShadeDiv >= 1 / 1000.f && foggy && foglayer)
		{
			// If we get here we know that we have colored fog and no fixed colormap.
			float density = GlobalMapFog ? GlobalFogDensity : 350.f - Scale(numshades - shade, 150, numshades);
			state.SetFog((GlobalMapFog) ? GlobalMapFog : fade, density);
			state.SetTextureMode(TM_FOGLAYER);
			state.SetRenderStyle(STYLE_Translucent);
			state.Draw(DT_TriangleStrip, vertexindex, 4);
			state.SetTextureMode(TM_NORMAL);
		}
	}
	else
	{
		//FHWModelRenderer renderer(di, state, dynlightindex);
		//RenderModel(&renderer, x, y, z, modelframe, actor, di->Viewpoint.TicFrac);
		//state.SetVertexBuffer(screen->mVertexData);
	}

	if (translucent)
	{
		state.EnableBrightmap(true);
		state.SetRenderStyle(STYLE_Translucent);
		state.SetTextureMode(TM_NORMAL);
	}

	state.SetObjectColor(0xffffffff);
	state.SetAddColor(0);
	state.EnableTexture(true);
	state.SetDynLight(0, 0, 0);
}


//==========================================================================
//
// 
//
//==========================================================================

void HWSprite::CalculateVertices(HWDrawInfo* di, FVector3* v, DVector3* vp)
{
	const auto& HWAngles = di->Viewpoint.HWAngles;

#if 0 // maybe later, it's a bit more tricky with Build.
	// [BB] Billboard stuff
	const bool drawWithXYBillboard = false;
	const bool drawBillboardFacingCamera = false;


	// [Nash] check for special sprite drawing modes
	if (drawWithXYBillboard || drawBillboardFacingCamera)
	{
		// Compute center of sprite
		float xcenter = (x1 + x2) * 0.5;
		float ycenter = (y1 + y2) * 0.5;
		float zcenter = (z1 + z2) * 0.5;
		float xx = -xcenter + x;
		float zz = -zcenter + z;
		float yy = -ycenter + y;
		Matrix3x4 mat;
		mat.MakeIdentity();
		mat.Translate(xcenter, zcenter, ycenter); // move to sprite center

												  // Order of rotations matters. Perform yaw rotation (Y, face camera) before pitch (X, tilt up/down).
		if (drawBillboardFacingCamera)
		{
			// [CMB] Rotate relative to camera XY position, not just camera direction,
			// which is nicer in VR
			float xrel = xcenter - vp->X;
			float yrel = ycenter - vp->Y;
			float absAngleDeg = RAD2DEG(atan2(-yrel, xrel));
			float counterRotationDeg = 270. - HWAngles.Yaw.Degrees; // counteracts existing sprite rotation
			float relAngleDeg = counterRotationDeg + absAngleDeg;

			mat.Rotate(0, 1, 0, relAngleDeg);
		}

		// [fgsfds] calculate yaw vectors
		float yawvecX = 0, yawvecY = 0, rollDegrees = 0;
		float angleRad = (270. - HWAngles.Yaw).Radians();

		if (drawWithXYBillboard)
		{
			// Rotate the sprite about the vector starting at the center of the sprite
			// triangle strip and with direction orthogonal to where the player is looking
			// in the x/y plane.
			mat.Rotate(-sin(angleRad), 0, cos(angleRad), -HWAngles.Pitch.Degrees);
		}

		mat.Translate(-xcenter, -zcenter, -ycenter); // retreat from sprite center
		v[0] = mat * FVector3(x1, z1, y1);
		v[1] = mat * FVector3(x2, z1, y2);
		v[2] = mat * FVector3(x1, z2, y1);
		v[3] = mat * FVector3(x2, z2, y2);
	}
	else // traditional "Y" billboard mode
#endif
	{
		v[0] = FVector3(x1, z1, y1);
		v[1] = FVector3(x2, z1, y2);
		v[2] = FVector3(x1, z2, y1);
		v[3] = FVector3(x2, z2, y2);
	}
}

//==========================================================================
//
// 
//
//==========================================================================

void HWSprite::CreateVertices(HWDrawInfo* di)
{
	if (modelframe == 0)
	{
		FVector3 v[4];
		CalculateVertices(di, v, &di->Viewpoint.Pos);
		auto vert = screen->mVertexData->AllocVertices(4);
		auto vp = vert.first;
		vertexindex = vert.second;

		vp[0].Set(v[0][0], v[0][1], v[0][2], ul, vt);
		vp[1].Set(v[1][0], v[1][1], v[1][2], ur, vt);
		vp[2].Set(v[2][0], v[2][1], v[2][2], ul, vb);
		vp[3].Set(v[3][0], v[3][1], v[3][2], ur, vb);
	}
}

//==========================================================================
//
// 
//
//==========================================================================

inline void HWSprite::PutSprite(HWDrawInfo* di, bool translucent)
{
	// That's a lot of checks...
	/*
	if (modelframe == 1 && gl_light_sprites)
	{
		hw_GetDynModelLight(actor, lightdata);
		dynlightindex = screen->mLights->UploadLights(lightdata);
	}
	else*/
		dynlightindex = -1;

	vertexindex = -1;
	if (!screen->BuffersArePersistent())
	{
		CreateVertices(di);
	}
	di->AddSprite(this, translucent);
}

//==========================================================================
//
// 
//
//==========================================================================

void HWSprite::Process(HWDrawInfo* di, spritetype* spr, sectortype* sector, int thruportal)
{
	if (spr == nullptr)
		return;

	auto tex = tileGetTexture(spr->picnum);
	if (!tex || !tex->isValid()) return;

	texture = tex;
	sprite = spr;

	modelframe = 0;
	dynlightindex = -1;
	shade = spr->shade;
	palette = spr->pal;
	fade = lookups.getFade(sector->floorpal);	// fog is per sector.
	visibility = sectorVisibility(sector);

	bool trans = (spr->cstat & CSTAT_SPRITE_TRANSLUCENT);
	if (trans)
	{
		RenderStyle = GetRenderStyle(0, !!(spr->cstat & CSTAT_SPRITE_TRANSLUCENT_INVERT));
		alpha = GetAlphaFromBlend((spr->cstat & CSTAT_SPRITE_TRANSLUCENT_INVERT) ? DAMETH_TRANS2 : DAMETH_TRANS1, 0);
	}
	else
	{
		RenderStyle = LegacyRenderStyles[STYLE_Translucent];
		alpha = 1.f;
	}

	x = spr->x * (1 / 16.f);
	z = spr->z * (1 / -256.f);
	y = spr->y * (1 / -16.f);
	auto vp = di->Viewpoint;

	if (modelframe == 0)
	{
		int flags = spr->cstat;
		int tilenum = spr->picnum;

		int xsize, ysize, tilexoff, tileyoff;
		if (hw_hightile && TileFiles.tiledata[tilenum].h_xsize)
		{
			xsize = TileFiles.tiledata[tilenum].h_xsize;
			ysize = TileFiles.tiledata[tilenum].h_ysize;
			tilexoff = TileFiles.tiledata[tilenum].h_xoffs;
			tileyoff = TileFiles.tiledata[tilenum].h_yoffs;
		}
		else
		{
			xsize = (int)tex->GetDisplayWidth();
			ysize = (int)tex->GetDisplayHeight();
			tilexoff = (int)tex->GetDisplayLeftOffset();
			tileyoff = (int)tex->GetDisplayTopOffset();

		}

		int heinum = 0; // tspriteGetSlope(tspr); // todo: slope sprites

		if (heinum == 0)
		{
			tilexoff += spr->xoffset;
			tileyoff += spr->yoffset;
		}

		// convert to render space.
		float width = (xsize * spr->xrepeat) * (0.2f / 16.f); // weird Build fuckery. Face sprites are rendered at 80% width only.
		float height = (ysize * spr->yrepeat) * (0.25f / 16.f);
		float xoff = (tilexoff * spr->xrepeat) * (0.2f / 16.f);
		float yoff = (tileyoff * spr->yrepeat) * (0.25f / 16.f);

		if (spr->cstat & CSTAT_SPRITE_YCENTER)
		{
			yoff -= height * 0.5;
			if (ysize & 1) yoff -= spr->yrepeat * (0.125f / 16.f);  // Odd yspans (taken from polymost as-is)
		}

		if (flags & CSTAT_SPRITE_XFLIP) xoff = -xoff;
		if (flags & CSTAT_SPRITE_YFLIP) yoff = -yoff;

		ul = (spr->cstat & CSTAT_SPRITE_XFLIP) ? 0.f : 1.f;
		ur = (spr->cstat & CSTAT_SPRITE_XFLIP) ? 1.f : 0.f;
		vt = (spr->cstat & CSTAT_SPRITE_YFLIP) ? 0.f : 1.f;
		vb = (spr->cstat & CSTAT_SPRITE_YFLIP) ? 1.f : 0.f;
		if (tex->isHardwareCanvas()) std::swap(vt, vb);

		float viewvecX = vp.ViewVector.X;
		float viewvecY = vp.ViewVector.Y;

		x = spr->x * (1 / 16.f);
		y = spr->y * (1 / -16.f);
		z = spr->z * (1 / -256.f);

		x1 = x - viewvecY * (xoff - (width * 0.5f));
		x2 = x - viewvecY * (xoff + (width * 0.5f));
		y1 = y + viewvecX * (xoff - (width * 0.5f));
		y2 = y + viewvecX * (xoff + (width * 0.5f));

		z1 = z + yoff;
		z2 = z + height + yoff;
	}
	else
	{
		x1 = x2 = x;
		y1 = y2 = y;
		z1 = z2 = z;
		texture = nullptr;
	}

	depth = (float)((x - vp.Pos.X) * vp.TanCos + (y - vp.Pos.Y) * vp.TanSin);

#if 0 // huh?
	if ((texture && texture->GetTranslucency()) || (RenderStyle.Flags & STYLEF_RedIsAlpha) || (modelframe && spr->RenderStyle != DefaultRenderStyle()))
	{
		if (hw_styleflags == STYLEHW_Solid)
		{
			RenderStyle.SrcAlpha = STYLEALPHA_Src;
			RenderStyle.DestAlpha = STYLEALPHA_InvSrc;
		}
		hw_styleflags = STYLEHW_NoAlphaTest;
	}
#endif

	PutSprite(di, alpha < 1.f-FLT_EPSILON || modelframe == 0);
	rendered_sprites++;
}
