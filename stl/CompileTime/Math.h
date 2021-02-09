// Copyright (c) 2018,  Zhirnov Andrey. For more information see 'LICENSE'

#pragma once

#include "stl/Common.h"

namespace FG
{

/*
=================================================
	CT_IntLog2
=================================================
*/
namespace _fg_hidden_
{
	template <typename T, T X, uint Bit>
	struct _IntLog2 {
		static const int	value = int((X >> Bit) != 0) + _IntLog2<T, X, Bit-1 >::value;
	};

	template <typename T, T X>
	struct _IntLog2< T, X, 0 > {
		static const int	value = 0;
	};

}	// _fg_hidden_

	template <auto X>
	static constexpr int	CT_IntLog2 = (X ? _fg_hidden_::_IntLog2< decltype(X), X, sizeof(X)*8-1 >::value : -1);


}	// FG
