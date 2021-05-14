// Copyright (c) 2018,  Zhirnov Andrey. For more information see 'LICENSE'

#pragma once

#include "scene/Common.h"

namespace FG
{

	enum class ECameraType : uint8_t
	{
		Main,
		Reflection,
		Shadow,
		Unknown	= 0xFF,
	};


	enum class ETextureType : uint
	{
		Unknown			= 0,
		Diffuse			= 1 << 0,
		Normal			= 1 << 1,
		Height			= 1 << 2,
		Reflection		= 1 << 3,	// TODO: static & dynamic ?
	};
	FG_BIT_OPERATORS( ETextureType );


	enum class EMaterialType : uint
	{
		Unknown					= 0,
		Instancing				= 1 << 0,		// must support instancing
		
		// animations
		SceletalAnimation		= 1 << 1,
		MorphingAnimation		= 1 << 2,

		// 
		ReliefMapping			= 1 <<  9,		// normal mapping, parallax mapping or other technique
		DisplacementMapping		= 1 << 10,		// with tessellation, requires diffuse, normal, height maps

		// 
		Water					= 1 << 16,
		Terrain					= 1 << 17,
	};
	FG_BIT_OPERATORS( EMaterialType );


	enum class ERenderLayer : uint
	{
		Layer_1		= 0,
		Layer_32	= 31,
		_Count,

		// default names:
		Shadow		= Layer_1,	// shadow map
		DepthOnly,				// depth prepass
		Background,
		Opaque_1,				// large objects
		Opaque_2,
		Opaque_3,
		Foreground,
		Emission,
		Translucent,
		HUD,

		Unknown		= ~0u,
	};
	using LayerBits_t	= BitSet< uint(ERenderLayer::_Count) >;


	enum class EDetailLevel : uint8_t
	{
		Unknown		= 255,
		High		= 0,
		Low			= 7,
		_Count
	};


	struct DetailLevelRange
	{
		EDetailLevel	min		= EDetailLevel::High;
		EDetailLevel	max		= EDetailLevel::Low;

		DetailLevelRange () {}
		DetailLevelRange (EDetailLevel min, EDetailLevel max) : min{min}, max{max} {}

		ND_ EDetailLevel  Clamp (EDetailLevel value) const
		{
			return EDetailLevel(FG::Clamp( uint(value), uint(min), uint(max) ));
		}
	};


}	// FG
