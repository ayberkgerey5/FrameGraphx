// Copyright (c) 2018-2019,  Zhirnov Andrey. For more information see 'LICENSE'

#pragma once

#include "framegraph/Public/CommandBuffer.h"
#include "framegraph/Public/FrameGraph.h"
#include "VDescriptorSetLayout.h"
#include "VLocalDebugger.h"
#include "VCommandPool.h"
#include "stl/Containers/FixedTupleArray.h"

namespace FG
{

	//
	// Command Batch pointer
	//

	struct VCmdBatchPtr
	{
	// variables
	private:
		class VCmdBatch *	_ptr	= null;

	// methods
	public:
		VCmdBatchPtr ()												{}
		VCmdBatchPtr (VCmdBatch* ptr) : _ptr{ptr}					{ _IncRef(); }
		VCmdBatchPtr (const VCmdBatchPtr &other) : _ptr{other._ptr}	{ _IncRef(); }
		VCmdBatchPtr (VCmdBatchPtr &&other) : _ptr{other._ptr}		{ other._ptr = null; }
		~VCmdBatchPtr ()											{ _DecRef(); }

		VCmdBatchPtr&	operator = (const VCmdBatchPtr &rhs)		{ _DecRef();  _ptr = rhs._ptr;  _IncRef();  return *this; }
		VCmdBatchPtr&	operator = (VCmdBatchPtr &&rhs)				{ _DecRef();  _ptr = rhs._ptr;  rhs._ptr = null;  return *this; }

		ND_ VCmdBatch*	operator -> ()		const					{ ASSERT( _ptr );  return _ptr; }
		ND_ VCmdBatch&	operator *  ()		const					{ ASSERT( _ptr );  return *_ptr; }

		ND_ explicit	operator bool ()	const					{ return _ptr != null; }

		ND_ VCmdBatch*	get ()				const					{ return _ptr; }


	private:
		void _IncRef ();
		void _DecRef ();
	};



	//
	// Vulkan Command Batch
	//

	class VCmdBatch final : public CommandBuffer::Batch
	{
		friend struct VCmdBatchPtr;
		friend class  VCommandBuffer;	// TODO: remove

	// types
	public:
		
		//---------------------------------------------------------------------------
		// acquired resources

		struct Resource
		{
		// constants
			static constexpr uint		IndexOffset		= 0;
			static constexpr uint		InstanceOffset	= sizeof(_fg_hidden_::ResourceID<0>::Index_t) * 8;
			static constexpr uint		IDOffset		= InstanceOffset + sizeof(_fg_hidden_::ResourceID<0>::InstanceID_t) * 8;
			static constexpr uint64_t	IndexMask		= (1ull << InstanceOffset) - 1;
			static constexpr uint64_t	InstanceMask	= ((1ull << IDOffset) - 1) & ~IndexMask;
			static constexpr uint64_t	IDMask			= 0xFFull << IDOffset;
			using Index_t				= RawImageID::Index_t;
			using InstanceID_t			= RawImageID::InstanceID_t;

		// variables
			uint64_t	value	= UMax;
			
		// methods
			Resource () {}

			template <uint UID>
			explicit Resource (_fg_hidden_::ResourceID<UID> id) :
				value{ (uint64_t(id.Index()) << IndexOffset) | (uint64_t(id.InstanceID()) << InstanceOffset) | (uint64_t(id.GetUID()) << IDOffset) }
			{}

			ND_ bool  operator == (const Resource &rhs)	const	{ return value == rhs.value; }

			ND_ Index_t			Index ()				const	{ return uint((value & IndexMask) >> IndexOffset); }
			ND_ InstanceID_t	InstanceID ()			const	{ return uint((value & InstanceMask) >> InstanceOffset); }
			ND_ uint			GetUID ()				const	{ return uint((value & IDMask) >> IDOffset); }
		};

		struct ResourceHash {
			ND_ size_t  operator () (const Resource &x) const noexcept {
				return std::hash<decltype(x.value)>{}( x.value );
			}
		};
		
		using ResourceMap_t		= std::unordered_set< Resource, ResourceHash >;		// TODO: custom allocator


		//---------------------------------------------------------------------------
		// staging buffers

		static constexpr uint	MaxBufferParts	= 3;
		static constexpr uint	MaxImageParts	= 4;


		struct StagingBuffer
		{
		// variables
			BufferID		bufferId;
			RawMemoryID		memoryId;
			BytesU			capacity;
			BytesU			size;
			
			void *			mappedPtr	= null;
			BytesU			memOffset;					// can be used to flush memory ranges
			VkDeviceMemory	mem			= VK_NULL_HANDLE;
			bool			isCoherent	= false;

		// methods
			StagingBuffer () {}

			StagingBuffer (BufferID &&buf, RawMemoryID mem, BytesU capacity) :
				bufferId{std::move(buf)}, memoryId{mem}, capacity{capacity} {}

			ND_ bool	IsFull ()	const	{ return size >= capacity; }
			ND_ bool	Empty ()	const	{ return size == 0_b; }
		};


		struct OnBufferDataLoadedEvent
		{
		// types
			struct Range
			{
				StagingBuffer const*	buffer;
				BytesU					offset;
				BytesU					size;
			};

			using DataParts_t	= FixedArray< Range, MaxBufferParts >;
			using Callback_t	= ReadBuffer::Callback_t;
			
		// variables
			Callback_t		callback;
			DataParts_t		parts;
			BytesU			totalSize;

		// methods
			OnBufferDataLoadedEvent () {}
			OnBufferDataLoadedEvent (const Callback_t &cb, BytesU size) : callback{cb}, totalSize{size} {}
		};


		struct OnImageDataLoadedEvent
		{
		// types
			using Range			= OnBufferDataLoadedEvent::Range;
			using DataParts_t	= FixedArray< Range, MaxImageParts >;
			using Callback_t	= ReadImage::Callback_t;

		// variables
			Callback_t		callback;
			DataParts_t		parts;
			BytesU			totalSize;
			uint3			imageSize;
			BytesU			rowPitch;
			BytesU			slicePitch;
			EPixelFormat	format		= Default;
			EImageAspect	aspect		= EImageAspect::Color;

		// methods
			OnImageDataLoadedEvent () {}

			OnImageDataLoadedEvent (const Callback_t &cb, BytesU size, const uint3 &imageSize,
									BytesU rowPitch, BytesU slicePitch, EPixelFormat fmt, EImageAspect asp) :
				callback{cb}, totalSize{size}, imageSize{imageSize}, rowPitch{rowPitch},
				slicePitch{slicePitch}, format{fmt}, aspect{asp} {}
		};

		
		//---------------------------------------------------------------------------
		// shader debugger

		using SharedShaderPtr	= PipelineDescription::VkShaderPtr;
		using ShaderModules_t	= FixedArray< SharedShaderPtr, 8 >;
		using TaskName_t		= _fg_hidden_::TaskName_t;

		struct StorageBuffer
		{
			BufferID				shaderTraceBuffer;
			BufferID				readBackBuffer;
			BytesU					capacity;
			BytesU					size;
			VkPipelineStageFlags	stages	= 0;
		};

		struct DebugMode
		{
			ShaderModules_t		modules;
			VkDescriptorSet		descriptorSet	= VK_NULL_HANDLE;
			BytesU				offset;
			BytesU				size;
			uint				sbIndex			= UMax;
			EShaderDebugMode	mode			= Default;
			EShaderStages		shaderStages	= Default;
			TaskName_t			taskName;
			uint				data[4]			= {};
		};

		using StorageBuffers_t		= Array< StorageBuffer >;
		using DebugModes_t			= Array< DebugMode >;
		using DescriptorCache_t		= HashMap< Pair<RawBufferID, RawDescriptorSetLayoutID>, VDescriptorSetLayout::DescriptorSet >;
		using ShaderDebugCallback_t	= IFrameGraph::ShaderDebugCallback_t;
		

		//---------------------------------------------------------------------------
		
		static constexpr uint	MaxDependencies	= 16;
		using Dependencies_t	= FixedArray< VCmdBatchPtr, MaxDependencies >;

		static constexpr uint	MaxSwapchains = 8;
		using Swapchains_t		= FixedArray< VSwapchain const*, MaxSwapchains >;
		
		using BatchGraph		= VLocalDebugger::BatchGraph;

		static constexpr uint		MaxBatchItems = 8;
		using CmdBuffers_t			= FixedTupleArray< MaxBatchItems, VkCommandBuffer, VCommandPool const* >;
		using SignalSemaphores_t	= FixedArray< VkSemaphore, MaxBatchItems >;
		using WaitSemaphores_t		= FixedTupleArray< MaxBatchItems, VkSemaphore, VkPipelineStageFlags >;


	public:
		enum class EState : uint
		{
			Initial,
			Recording,		// build command buffers
			Backed,			// command buffers builded, all data locked
			Ready,			// all dependencies in 'Ready', 'Submitted' or 'Complete' states
			Submitted,		// commands was submitted to the GPU
			Complete,		// commands complete execution on the GPU
		};


	// variables
	private:
		std::atomic<EState>					_state;
		VResourceManager &					_resMngr;

		const VDeviceQueueInfoPtr			_queue;
		const EQueueType					_queueType;

		Dependencies_t						_dependencies;
		bool								_submitImmediately	= false;
		
		// command batch data
		struct {
			CmdBuffers_t						commands;
			SignalSemaphores_t					signalSemaphores;
			WaitSemaphores_t					waitSemaphores;
		}									_batch;

		// staging buffers
		struct {
			BytesU								hostWritableBufferSize;
			BytesU								hostReadableBufferSize;
			EBufferUsage						hostWritebleBufferUsage	= Default;
			FixedArray< StagingBuffer, 8 >		hostToDevice;	// CPU write, GPU read
			FixedArray< StagingBuffer, 8 >		deviceToHost;	// CPU read, GPU write
			Array< OnBufferDataLoadedEvent >	onBufferLoadedEvents;
			Array< OnImageDataLoadedEvent >		onImageLoadedEvents;
		}									_staging;

		// resources
		ResourceMap_t						_resourcesToRelease;
		Swapchains_t						_swapchains;

		// shader debugger
		struct {
			StorageBuffers_t					buffers;
			DebugModes_t						modes;
			DescriptorCache_t					descCache;
			BytesU								bufferAlign;
			const BytesU						bufferSize		= 64_Mb;
		}									_shaderDebugger;

		// frame debugger
		String								_debugDump;
		BatchGraph							_debugGraph;
		
		VSubmittedPtr						_submitted;


	// methods
	public:
		VCmdBatch (VResourceManager &rm, VDeviceQueueInfoPtr queue, EQueueType type, ArrayView<CommandBuffer> dependsOn);
		~VCmdBatch ();

		void  Release () override;
		
		bool  OnBegin (const CommandBufferDesc &);
		bool  OnBaked (INOUT ResourceMap_t &);
		bool  OnReadyToSubmit ();
		bool  OnSubmit (OUT VkSubmitInfo &, OUT Appendable<VSwapchain const*>, const VSubmittedPtr &);
		bool  OnComplete (VDebugger &, const ShaderDebugCallback_t &);

		void  SignalSemaphore (VkSemaphore sem);
		void  WaitSemaphore (VkSemaphore sem, VkPipelineStageFlags stage);
		void  PushFrontCommandBuffer (VkCommandBuffer, const VCommandPool *);
		void  PushBackCommandBuffer (VkCommandBuffer, const VCommandPool *);
		void  AddDependency (VCmdBatch *);
	

		// shader debugger //
		void  BeginShaderDebugger (VkCommandBuffer cmd);
		void  EndShaderDebugger (VkCommandBuffer cmd);

		bool  SetShaderModule (ShaderDbgIndex id, const SharedShaderPtr &module);
		bool  GetDebugModeInfo (ShaderDbgIndex id, OUT EShaderDebugMode &mode, OUT EShaderStages &stages) const;
		bool  GetDescriptotSet (ShaderDbgIndex id, OUT uint &binding, OUT VkDescriptorSet &descSet, OUT uint &dynamicOffset) const;

		ND_ ShaderDbgIndex  AppendShader (INOUT ArrayView<RectI> &, const TaskName_t &name, const _fg_hidden_::GraphicsShaderDebugMode &mode, BytesU size = 8_Mb);
		ND_ ShaderDbgIndex  AppendShader (const TaskName_t &name, const _fg_hidden_::ComputeShaderDebugMode &mode, BytesU size = 8_Mb);
		ND_ ShaderDbgIndex  AppendShader (const TaskName_t &name, const _fg_hidden_::RayTracingShaderDebugMode &mode, BytesU size = 8_Mb);


		// staging buffer //
		bool  GetWritable (const BytesU srcRequiredSize, const BytesU blockAlign, const BytesU offsetAlign, const BytesU dstMinSize,
							OUT RawBufferID &dstBuffer, OUT BytesU &dstOffset, OUT BytesU &outSize, OUT void* &mappedPtr);
		bool  AddPendingLoad (BytesU srcOffset, BytesU srcTotalSize, OUT RawBufferID &dstBuffer, OUT OnBufferDataLoadedEvent::Range &range);
		bool  AddPendingLoad (BytesU srcOffset, BytesU srcTotalSize, BytesU srcPitch, OUT RawBufferID &dstBuffer, OUT OnImageDataLoadedEvent::Range &range);
		bool  AddDataLoadedEvent (OnImageDataLoadedEvent &&);
		bool  AddDataLoadedEvent (OnBufferDataLoadedEvent &&);

		ND_ BytesU					GetMaxWritableStoregeSize ()	const	{ return _staging.hostWritableBufferSize / 4; }
		ND_ BytesU					GetMaxReadableStorageSize ()	const	{ return _staging.hostReadableBufferSize / 4; }


		ND_ VDeviceQueueInfoPtr		GetQueue ()						const	{ return _queue; }
		ND_ EQueueType				GetQueueType ()					const	{ return _queueType; }
		ND_ EState					GetState ()								{ return _state.load( memory_order_relaxed ); }
		ND_ ArrayView<VCmdBatchPtr>	GetDependencies ()				const	{ return _dependencies; }
		ND_ VSubmittedPtr const&	GetSubmitted ()					const	{ return _submitted; }		// TODO: rename


	private:
		void  _SetState (EState newState);
		void  _ReleaseResources ();
		void  _FinalizeCommands ();

		
		// shader debugger //
		bool  _AllocStorage (INOUT DebugMode &, BytesU);
		bool  _AllocDescriptorSet (EShaderDebugMode debugMode, EShaderStages stages,
								   RawBufferID storageBuffer, BytesU size, OUT VkDescriptorSet &descSet);
		void  _ParseDebugOutput (const ShaderDebugCallback_t &cb);
		bool  _ParseDebugOutput2 (const ShaderDebugCallback_t &cb, const DebugMode &dbg, Array<String>&) const;


		// staging buffer //
		bool  _AddPendingLoad (const BytesU srcRequiredSize, const BytesU blockAlign, const BytesU offsetAlign, const BytesU dstMinSize,
							   OUT RawBufferID &dstBuffer, OUT OnBufferDataLoadedEvent::Range &range);
		bool  _MapMemory (INOUT StagingBuffer &) const;
		void  _FinalizeStagingBuffers ();
	};


	
/*
=================================================
	_IncRef
=================================================
*/
	inline void VCmdBatchPtr::_IncRef ()
	{
		if ( _ptr )
			_ptr->_counter.fetch_add( 1, memory_order_relaxed );
	}

/*
=================================================
	_DecRef
=================================================
*/
	inline void VCmdBatchPtr::_DecRef ()
	{
		if ( _ptr )
		{
			auto	count = _ptr->_counter.fetch_sub( 1, memory_order_relaxed );
			ASSERT( count > 0 );

			if ( count == 1 ) {
				std::atomic_thread_fence( std::memory_order_acquire );
				_ptr->Release();
			}
		}
	}


}	// FG
