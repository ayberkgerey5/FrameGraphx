// Copyright (c) 2018-2019,  Zhirnov Andrey. For more information see 'LICENSE'

#pragma once

#include "stl/Math/Vec.h"

namespace FGC
{

	template <typename T>
	struct Rectangle
	{
	// types
		using Vec2_t	= Vec<T,2>;
		using Self		= Rectangle<T>;


	// variables
		T	left, top;
		T	right, bottom;


	// methods
		constexpr Rectangle () :
			left{T(0)}, top{T(0)}, right{T(0)}, bottom{T(0)} {}
		
		constexpr Rectangle (T left, T top, T right, T bottom) :
			left{left}, top{top}, right{right}, bottom{bottom} {}

		constexpr Rectangle (const Vec2_t &leftTop, const Vec2_t &rightBottom) :
			left{leftTop.x}, top{leftTop.y}, right{rightBottom.x}, bottom{rightBottom.y} {}
		
		constexpr explicit Rectangle (const Vec2_t &size) :
			Rectangle{ Vec2_t{}, size } {}

		template <typename B>
		constexpr explicit Rectangle (const Rectangle<B> &other) :
			left{T(other.left)}, top{T(other.top)}, right{T(other.right)}, bottom{T(other.bottom)} {}


		ND_ constexpr const T		Width ()		const	{ return right - left; }
		ND_ constexpr const T		Height ()		const	{ return bottom - top; }

		ND_ constexpr const Vec2_t	LeftTop ()		const	{ return { left, top }; }
		ND_ constexpr const Vec2_t	RightBottom ()	const	{ return { right, bottom }; }
		
		ND_ Vec2_t &				LeftTop ();
		ND_ Vec2_t &				RightBottom ();

		ND_ constexpr const Vec2_t	Size ()			const	{ return { Width(), Height() }; }
		ND_ constexpr const Vec2_t	Center ()		const	{ return { (right + left) / T(2), (top + bottom) / T(2) }; }

		ND_ constexpr bool			IsEmpty ()		const	{ return Equals( left, right ) or Equals( top, bottom ); }
		ND_ constexpr bool			IsInvalid ()	const	{ return right < left or bottom < top; }
		ND_ constexpr bool			IsValid ()		const	{ return not IsEmpty() and not IsInvalid(); }

		ND_ Self  operator + (const Vec2_t &rhs)	const	{ return Self{*this} += rhs; }
		ND_ Self  operator - (const Vec2_t &rhs)	const	{ return Self{*this} -= rhs; }

			Self&  operator += (const Vec2_t &rhs);
			Self&  operator -= (const Vec2_t &rhs);

			void Merge (const Rectangle<T> &other);
		
		ND_ constexpr bool  IsNormalized () const
		{
			return (left <= right) & (top <= bottom);
		}

		ND_ constexpr bool4  operator == (const Self &rhs) const
		{
			return { left == rhs.left, top == rhs.top, right == rhs.right, bottom == rhs.bottom };
		}
		
		Self&	Join (const Self &other);
		Self&	Join (const Vec2_t &point);
	};


	using RectU		= Rectangle< uint >;
	using RectI		= Rectangle< int >;
	using RectF		= Rectangle< float >;

	
/*
=================================================
	LeftTop
=================================================
*/
	template <typename T>
	inline Vec<T,2>&  Rectangle<T>::LeftTop ()
	{
		STATIC_ASSERT( offsetof(Self, left) + sizeof(T) == offsetof(Self, top) );
		return *reinterpret_cast<Vec2_t *>( &left );
	}
	
/*
=================================================
	RightBottom
=================================================
*/
	template <typename T>
	inline Vec<T,2>&  Rectangle<T>::RightBottom ()
	{
		STATIC_ASSERT( offsetof(Self, right) + sizeof(T) == offsetof(Self, bottom) );
		return *reinterpret_cast<Vec2_t *>( &right );
	}

/*
=================================================
	operator +=
=================================================
*/
	template <typename T>
	inline Rectangle<T>&  Rectangle<T>::operator += (const Vec2_t &rhs)
	{
		left += rhs.x;		right  += rhs.x;
		top  += rhs.y;		bottom += rhs.y;
		return *this;
	}
		
/*
=================================================
	operator -=
=================================================
*/
	template <typename T>
	inline Rectangle<T>&  Rectangle<T>::operator -= (const Vec2_t &rhs)
	{
		left -= rhs.x;		right  -= rhs.x;
		top  -= rhs.y;		bottom -= rhs.y;
		return *this;
	}
	
/*
=================================================
	Merge
=================================================
*/
	template <typename T>
	inline void Rectangle<T>::Merge (const Rectangle<T> &other)
	{
		left	= Min( left,   other.left   );
		top		= Min( top,    other.top    );
		right	= Max( right,  other.right  );
		bottom	= Max( bottom, other.bottom );
	}

/*
=================================================
	Join
=================================================
*/
	template <typename T>
	inline Rectangle<T> &  Rectangle<T>::Join (const Self &other)
	{
		left	= Min( left,	other.left );
		top		= Min( top,		other.top );
		right	= Max( right,	other.right );
		bottom	= Max( bottom,	other.bottom );
		return *this;
	}
	
	template <typename T>
	inline Rectangle<T> &  Rectangle<T>::Join (const Vec2_t &point)
	{
		left	= Min( left,	point.x );
		top		= Min( top,		point.y );
		right	= Max( right,	point.x );
		bottom	= Max( bottom,	point.y );
		return *this;
	}

/*
=================================================
	Equals
=================================================
*/
	template <typename T>
	ND_ inline constexpr bool4  Equals (const Rectangle<T> &lhs, const Rectangle<T> &rhs, const T &err = std::numeric_limits<T>::epsilon() * T(2))
	{
		bool4	res;
		res[0] = FGC::Equals( lhs.left,   rhs.left,   err );
		res[1] = FGC::Equals( lhs.top,    rhs.top,    err );
		res[2] = FGC::Equals( lhs.right,  rhs.right,  err );
		res[3] = FGC::Equals( lhs.bottom, rhs.bottom, err );
		return res;
	}


}	// FGC


namespace std
{
	template <typename T>
	struct hash< FGC::Rectangle<T> >
	{
		ND_ size_t  operator () (const FGC::Rectangle<T> &value) const noexcept
		{
		#if FG_FAST_HASH
			return	size_t( FGC::HashOf( this, sizeof(*this) ));
		#else
			return	size_t( FGC::HashOf( value.left )  + FGC::HashOf( value.bottom ) +
							FGC::HashOf( value.right ) + FGC::HashOf( value.top ) );
		#endif
		}
	};

}	// std
