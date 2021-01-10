// Copyright (c) 2018,  Zhirnov Andrey. For more information see 'LICENSE'

#pragma once

#include <string_view>
#include "stl/include/Defines.h"

namespace FG
{

	//
	// Logger
	//

	struct Log
	{
		enum class EResult {
			Continue,
			Break,
			Abort,
		};

		static EResult  Info (std::string_view msg, std::string_view func, std::string_view file, int line);
		static EResult  Error (std::string_view msg, std::string_view func, std::string_view file, int line);
	};

}	// FG


#define FG_PRIVATE_LOGX( _level_, _msg_, _file_, _line_ ) \
	{switch ( ::FG::Log::_level_( (_msg_), (FG_FUNCTION_NAME), (_file_), (_line_) ) ) \
	{ \
		case ::FG::Log::EResult::Continue :	break; \
        case ::FG::Log::EResult::Break :	FG_PRIVATE_BREAK_POINT();		break; \
		case ::FG::Log::EResult::Abort :	::exit( EXIT_FAILURE );	break; \
	}}

#define FG_PRIVATE_LOGI( _msg_, _file_, _line_ )	FG_PRIVATE_LOGX( Info, (_msg_), (_file_), (_line_) )
#define FG_PRIVATE_LOGE( _msg_, _file_, _line_ )	FG_PRIVATE_LOGX( Error, (_msg_), (_file_), (_line_) )
