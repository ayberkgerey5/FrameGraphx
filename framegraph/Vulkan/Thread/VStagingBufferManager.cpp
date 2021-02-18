// Copyright (c) 2018,  Zhirnov Andrey. For more information see 'LICENSE'

#include "VStagingBufferManager.h"
#include "VFrameGraphThread.h"
#include "VMemoryManager.h"
#include "stl/Algorithms/StringUtils.h"

namespace FG
{

/*
=================================================
	constructor
=================================================
*/
	VStagingBufferManager::VStagingBufferManager (VFrameGraphThread &fg) :
		_frameGraph{ fg },
		_currFrame{ ~0u }
	{
	}
	
/*
=================================================
	destructor
=================================================
*/
	VStagingBufferManager::~VStagingBufferManager ()
	{
		CHECK( _perFrame.empty() );
	}

/*
=================================================
	Initialize
=================================================
*/
	bool VStagingBufferManager::Initialize ()
	{
		CHECK_ERR( _perFrame.empty() );

		_perFrame.resize( _frameGraph.GetInstance()->GetRingBufferSize() );
		return true;
	}
	
/*
=================================================
	Deinitialize
=================================================
*/
	void VStagingBufferManager::Deinitialize ()
	{
		for (auto& frame : _perFrame)
		{
			for (auto& buf : frame.deviceToHost) {
				_frameGraph.DestroyResource( buf.bufferId );
			}

			for (auto& buf : frame.hostToDevice) {
				_frameGraph.DestroyResource( buf.bufferId );
			}
		}
		_perFrame.clear();
	}

/*
=================================================
	OnBeginFrame
=================================================
*/
	void VStagingBufferManager::OnBeginFrame (const uint frameIdx)
	{
		using T = BufferView::value_type;

		_currFrame = frameIdx;

		auto&	frame = _perFrame[_currFrame];

		// map device-to-host staging buffers
		for (auto& buf : frame.deviceToHost)
		{
			buf.size = 0_b;

			// buffer may be recreated on defragmentation pass, so we need to obtain actual pointer every frame
			CHECK( _MapMemory( buf ));
		}


		// trigger buffer events
		for (auto& ev : frame.bufferLoadEvents)
		{
			FixedArray< ArrayView<T>, MaxBufferParts >	data_parts;
			BytesU										total_size;

			for (auto& part : ev.parts)
			{
				ArrayView<T>	view{ static_cast<T*>(part.buffer->mappedPtr) + part.offset, size_t(part.size) };

				data_parts.push_back( view );
				total_size += part.size;
			}

			ASSERT( total_size == ev.totalSize );

			ev.callback( BufferView{data_parts} );
		}
		frame.bufferLoadEvents.clear();
		

		// trigger image events
		for (auto& ev : frame.imageLoadEvents)
		{
			FixedArray< ArrayView<T>, MaxImageParts >	data_parts;
			BytesU										total_size;

			for (auto& part : ev.parts)
			{
				ArrayView<T>	view{ static_cast<T*>(part.buffer->mappedPtr) + part.offset, size_t(part.size) };

				data_parts.push_back( view );
				total_size += part.size;
			}

			ASSERT( total_size == ev.totalSize );

			ev.callback( ImageView{ data_parts, ev.imageSize, ev.rowPitch, ev.slicePitch, ev.format, ev.aspect });
		}
		frame.imageLoadEvents.clear();
		

		// map host-to-device staging buffers
		for (auto& buf : frame.hostToDevice)
		{
			buf.size = 0_b;

			// buffer may be recreated on defragmentation pass, so we need to obtain actual pointer every frame
			CHECK( _MapMemory( buf ));
		}
	}
	
/*
=================================================
	OnEndFrame
=================================================
*/
	void VStagingBufferManager::OnEndFrame ()
	{
	}
	
/*
=================================================
	StoreBufferData
=================================================
*/
	bool VStagingBufferManager::StoreBufferData (ArrayView<uint8_t> srcData, const BytesU srcOffset,
												 OUT RawBufferID &dstBuffer, OUT BytesU &dstOffset, OUT BytesU &size)
	{
		// skip blocks less than 1/N of data size
		const BytesU	min_size = (ArraySizeOf(srcData) + MaxBufferParts-1) / MaxBufferParts;

		return _StoreData( srcData, srcOffset, 0_b, min_size, OUT dstBuffer, OUT dstOffset, OUT size );
	}

/*
=================================================
	StoreImageData
=================================================
*/
	bool VStagingBufferManager::StoreImageData (ArrayView<uint8_t> srcData, const BytesU srcOffset, const BytesU srcPitch, const BytesU srcTotalSize,
												OUT RawBufferID &dstBuffer, OUT BytesU &dstOffset, OUT BytesU &size)
	{
		// skip blocks less than 1/N of total data size
		const BytesU	min_size = Max( (srcTotalSize + MaxImageParts-1) / MaxImageParts, srcPitch );

		return _StoreData( srcData, srcOffset, srcPitch, srcPitch, OUT dstBuffer, OUT dstOffset, OUT size );
	}

/*
=================================================
	_StoreData
=================================================
*/
	bool VStagingBufferManager::_StoreData (ArrayView<uint8_t> srcData, const BytesU srcOffset, const BytesU srcAlign, const BytesU srcMinSize,
											OUT RawBufferID &dstBuffer, OUT BytesU &dstOffset, OUT BytesU &size)
	{
		const BytesU	required		= ArraySizeOf(srcData) - srcOffset;
		auto&			staging_buffers = _perFrame[_currFrame].hostToDevice;


		// search in existing
		StagingBuffer*	suitable		= null;
		StagingBuffer*	max_available	= null;

		for (auto& buf : staging_buffers)
		{
			const BytesU	av = buf.Available();

			if ( av >= required )
			{
				suitable = &buf;
				break;
			}

			if ( not max_available or av > max_available->Available() )
			{
				max_available = &buf;
			}
		}

		// no suitable space, try to use max available block
		if ( not suitable and max_available and max_available->Available() >= srcMinSize )
		{
			suitable = max_available;
		}

		// allocate new buffer
		if ( not suitable )
		{
			BufferID		buf_id  = _frameGraph.CreateBuffer( MemoryDesc{EMemoryType::HostWrite}, BufferDesc{_stagingBufferSize, EBufferUsage::Transfer} );
			CHECK_ERR( buf_id );

			RawMemoryID		mem_id  = _frameGraph.GetResourceManager()->GetResource( buf_id.Get() )->GetMemoryID();
			CHECK_ERR( mem_id );

			//buf->SetDebugName( "StagingWBuffer"s << ToString(staging_buffers.size()) << '/' << ToString(_currFrame) );

			staging_buffers.push_back({ std::move(buf_id), mem_id, _stagingBufferSize });

			suitable = &staging_buffers.back();
			CHECK( _MapMemory( *suitable ));
		}

		// write data to buffer
		dstOffset	= suitable->size;
		size		= Min( suitable->Available(), required );
		dstBuffer	= suitable->bufferId.Get();

		if ( srcAlign > 1_b )
			size = AlignToSmaller( size, srcAlign );

		MemCopy( suitable->mappedPtr, suitable->Available(), srcData.data() + srcOffset, size );

		suitable->size += size;
		return true;
	}
	
/*
=================================================
	_AddPendingLoad
=================================================
*/
	bool VStagingBufferManager::_AddPendingLoad (const BytesU srcRequiredSize, const BytesU srcAlign, const BytesU srcMinSize,
												 OUT RawBufferID &dstBuffer, OUT BufferDataLoadedEvent::Range &range)
	{
		auto&	staging_buffers = _perFrame[_currFrame].deviceToHost;
		

		// search in existing
		StagingBuffer*	suitable		= null;
		StagingBuffer*	max_available	= null;

		for (auto& buf : staging_buffers)
		{
			const BytesU	av = buf.Available();

			if ( av >= srcRequiredSize )
			{
				suitable = &buf;
				break;
			}

			if ( not max_available or av > max_available->Available() )
			{
				max_available = &buf;
			}
		}

		// no suitable space, try to use max available block
		if ( not suitable and max_available and max_available->Available() >= srcMinSize )
		{
			suitable = max_available;
		}

		// allocate new buffer
		if ( not suitable )
		{
			BufferID		buf_id  = _frameGraph.CreateBuffer( MemoryDesc{EMemoryType::HostRead}, BufferDesc{_stagingBufferSize, EBufferUsage::Transfer} );
			CHECK_ERR( buf_id );
			
			RawMemoryID		mem_id  = _frameGraph.GetResourceManager()->GetResource( buf_id.Get() )->GetMemoryID();
			CHECK_ERR( mem_id );

			//buf->SetDebugName( "StagingRBuffer"s << ToString(staging_buffers.size()) << '/' << ToString(_currFrame) );

			staging_buffers.push_back({ std::move(buf_id), mem_id, _stagingBufferSize });

			suitable = &staging_buffers.back();
			CHECK( _MapMemory( *suitable ));
		}
		
		// write data to buffer
		range.buffer	= suitable;
		range.offset	= suitable->size;
		range.size		= Min( suitable->Available(), srcRequiredSize );
		dstBuffer		= suitable->bufferId.Get();
		
		if ( srcAlign > 1_b )
			range.size = AlignToSmaller( range.size, srcAlign );

		suitable->size += range.size;
		return true;
	}
	
/*
=================================================
	AddPendingLoad
=================================================
*/
	bool VStagingBufferManager::AddPendingLoad (const BytesU srcOffset, const BytesU srcTotalSize,
												OUT RawBufferID &dstBuffer, OUT BufferDataLoadedEvent::Range &range)
	{
		// skip blocks less than 1/N of data size
		const BytesU	min_size = (srcTotalSize + MaxBufferParts-1) / MaxBufferParts;

		return _AddPendingLoad( srcTotalSize - srcOffset, 0_b, min_size, OUT dstBuffer, OUT range );
	}

/*
=================================================
	AddDataLoadedEvent
=================================================
*/
	bool VStagingBufferManager::AddDataLoadedEvent (BufferDataLoadedEvent &&ev)
	{
		CHECK_ERR( ev.callback and not ev.parts.empty() );

		_perFrame[_currFrame].bufferLoadEvents.push_back( std::move(ev) );
		return true;
	}
	
/*
=================================================
	AddPendingLoad
=================================================
*/
	bool VStagingBufferManager::AddPendingLoad (const BytesU srcOffset, const BytesU srcTotalSize, const BytesU srcPitch,
												OUT RawBufferID &dstBuffer, OUT ImageDataLoadedEvent::Range &range)
	{
		// skip blocks less than 1/N of total data size
		const BytesU	min_size = Max( (srcTotalSize + MaxImageParts-1) / MaxImageParts, srcPitch );

		return _AddPendingLoad( srcTotalSize - srcOffset, srcPitch, min_size, OUT dstBuffer, OUT range );
	}

/*
=================================================
	AddDataLoadedEvent
=================================================
*/
	bool VStagingBufferManager::AddDataLoadedEvent (ImageDataLoadedEvent &&ev)
	{
		CHECK_ERR( ev.callback and not ev.parts.empty() );

		_perFrame[_currFrame].imageLoadEvents.push_back( std::move(ev) );
		return true;
	}
	
/*
=================================================
	_MapMemory
=================================================
*/
	bool VStagingBufferManager::_MapMemory (StagingBuffer &buf) const
	{
		VMemoryObj::MemoryInfo	info;
		if ( _frameGraph.GetResourceManager()->GetResource( buf.memoryId )->GetInfo( OUT info ) )
		{
			buf.mappedPtr	= info.mappedPtr;
			buf.memOffset	= info.offset;
			buf.mem			= info.mem;
			return true;
		}
		return false;
	}


}	// FG
