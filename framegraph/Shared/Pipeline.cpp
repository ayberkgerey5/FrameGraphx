// Copyright (c) 2018,  Zhirnov Andrey. For more information see 'LICENSE'

#include "Public/Pipeline.h"
#include "Shared/EnumUtils.h"

namespace FG
{

	//
	// Shader Data implementation
	//
	template <typename T>
	class ShaderDataImpl final : public PipelineDescription::IShaderData<T>
	{
	// variables
	private:
		T			_data;
		String		_entry;


	// methods
	public:
		ShaderDataImpl (T &&data, StringView entry) :
			_data{ std::move(data) },
			_entry{ entry }
		{}
		
		T const&	GetData () const override		{ return _data; }

		StringView	GetEntry () const override		{ return _entry; }

		size_t		GetHashOfData () const override	{ ASSERT(false);  return 0; }
	};
	
/*
=================================================
	AddShaderData
=================================================
*/
	void PipelineDescription::Shader::AddShaderData (EShaderLangFormat fmt, StringView entry, String &&src)
	{
		data.insert({ fmt, MakeShared<ShaderDataImpl<String>>( std::move(src), entry ) });
	}
	
	void PipelineDescription::Shader::AddShaderData (EShaderLangFormat fmt, StringView entry, Array<uint8_t> &&bin)
	{
		data.insert({ fmt, MakeShared<ShaderDataImpl<Array<uint8_t>>>( std::move(bin), entry ) });
	}
	
	void PipelineDescription::Shader::AddShaderData (EShaderLangFormat fmt, StringView entry, Array<uint> &&bin)
	{
		data.insert({ fmt, MakeShared<ShaderDataImpl<Array<uint>>>( std::move(bin), entry ) });
	}
//-----------------------------------------------------------------------------
	

	
/*
=================================================
	_TextureUniform
=================================================
*/
	PipelineDescription::_TextureUniform::_TextureUniform (const UniformID &id, EImage textureType, const BindingIndex &index, EShaderStages stageFlags) :
		id{id}, data{ EResourceState::ShaderSample | EResourceState_FromShaders( stageFlags ), textureType }, index{index}, stageFlags{stageFlags}
	{}
	
/*
=================================================
	_SamplerUniform
=================================================
*/
	PipelineDescription::_SamplerUniform::_SamplerUniform (const UniformID &id, const BindingIndex &index, EShaderStages stageFlags) :
		id{id}, data{}, index{index}, stageFlags{stageFlags}
	{}
	
/*
=================================================
	_SubpassInputUniform
=================================================
*/
	PipelineDescription::_SubpassInputUniform::_SubpassInputUniform (const UniformID &id, uint attachmentIndex, bool isMultisample, const BindingIndex &index, EShaderStages stageFlags) :
		id{id}, data{ EResourceState::InputAttachment | EResourceState_FromShaders( stageFlags ), attachmentIndex, isMultisample },
		index{index}, stageFlags{stageFlags}
	{}
	
/*
=================================================
	_ImageUniform
=================================================
*/
	PipelineDescription::_ImageUniform::_ImageUniform (const UniformID &id, EImage imageType, EPixelFormat format, EShaderAccess access, const BindingIndex &index, EShaderStages stageFlags) :
		id{id}, data{ EResourceState_FromShaders( stageFlags ) | EResourceState_FromShaderAccess( access ), imageType, format },
		index{index}, stageFlags{stageFlags}
	{}
	
/*
=================================================
	_UBufferUniform
=================================================
*/
	PipelineDescription::_UBufferUniform::_UBufferUniform (const UniformID &id, BytesU size, const BindingIndex &index, EShaderStages stageFlags, bool allowDynamicOffset) :
		id{id}, data{ EResourceState::UniformRead | EResourceState_FromShaders( stageFlags ), allowDynamicOffset ? 0 : STATIC_OFFSET, size },
		index{index}, stageFlags{stageFlags}
	{}
	
/*
=================================================
	_StorageBufferUniform
=================================================
*/
	PipelineDescription::_StorageBufferUniform::_StorageBufferUniform (const UniformID &id, BytesU staticSize, BytesU arrayStride, EShaderAccess access,
																	   const BindingIndex &index, EShaderStages stageFlags, bool allowDynamicOffset) :
		id{id}, data{ EResourceState_FromShaders( stageFlags ) | EResourceState_FromShaderAccess( access ), allowDynamicOffset ? 0 : STATIC_OFFSET, staticSize, arrayStride },
		index{index}, stageFlags{stageFlags}
	{}
//-----------------------------------------------------------------------------



/*
=================================================
	_AddDescriptorSet
=================================================
*/
	void PipelineDescription::_AddDescriptorSet (const DescriptorSetID						&id,
												 uint										index,
												 ArrayView< _TextureUniform >				textures,
												 ArrayView< _SamplerUniform >				samplers,
												 ArrayView< _SubpassInputUniform >			subpassInputs,
												 ArrayView< _ImageUniform >					images,
												 ArrayView< _UBufferUniform >				uniformBuffers,
												 ArrayView< _StorageBufferUniform >			storageBuffers,
												 ArrayView< _AccelerationStructureUniform>	accelerationStructures)
	{
		DEBUG_ONLY(
		for (auto& item : _pipelineLayout.descriptorSets)
		{
			ASSERT( item.id != id );				// descriptor set ID must be unique
			ASSERT( item.bindingIndex != index );	// binding index must be unique
		})

		DescriptorSet	ds;
		UniformMap_t	uniforms;
		uint			dynamic_offset_index = 0;

		ds.id			= id;
		ds.bindingIndex	= index;
		uniforms.reserve( textures.size() + samplers.size() + subpassInputs.size() + images.size() +
						  uniformBuffers.size() + storageBuffers.size() + accelerationStructures.size() );

		for (auto& tex : textures) {
			uniforms.insert({ tex.id, {Texture{ tex.data }, tex.index, tex.stageFlags} });
		}

		for (auto& samp : samplers) {
			uniforms.insert({ samp.id, {Sampler{ samp.data }, samp.index, samp.stageFlags} });
		}

		for (auto& spi : subpassInputs) {
			uniforms.insert({ spi.id, {SubpassInput{ spi.data }, spi.index, spi.stageFlags} });
		}

		for (auto& img : images) {
			uniforms.insert({ img.id, {Image{ img.data }, img.index, img.stageFlags} });
		}

		for (auto& ub : uniformBuffers) {
			uniforms.insert({ ub.id, {UniformBuffer{ub.data.state, (ub.data.dynamicOffsetIndex == STATIC_OFFSET ? STATIC_OFFSET : dynamic_offset_index++),
										ub.data.size}, ub.index, ub.stageFlags} });
		}

		for (auto& sb : storageBuffers) {
			uniforms.insert({ sb.id, {StorageBuffer{sb.data.state, (sb.data.dynamicOffsetIndex == STATIC_OFFSET ? STATIC_OFFSET : dynamic_offset_index++),
										sb.data.staticSize, sb.data.arrayStride}, sb.index, sb.stageFlags} });
		}

		//for (auto& as : accelerationStructures) {
		//	uniforms.insert({ as.id, Uniform_t{ as.data } });
		//}

		ds.uniforms = MakeShared<UniformMap_t>( std::move(uniforms) );
		_pipelineLayout.descriptorSets.push_back( std::move(ds) );

		ASSERT( dynamic_offset_index <= FG_MaxBufferDynamicOffsets );
	}
		
/*
=================================================
	_SetPushConstants
=================================================
*/
	void PipelineDescription::_SetPushConstants (ArrayView< PushConstant > values)
	{
		_pipelineLayout.pushConstants.assign( values.begin(), values.end() );
	}

/*
=================================================
	CopySpecConstants
=================================================
*/
	static void CopySpecConstants (ArrayView<PipelineDescription::SpecConstant> src, OUT PipelineDescription::SpecConstants_t &dst)
	{
		for (auto& val : src) {
			dst.insert({ val.id, val.index });
		}

		CHECK( src.size() == dst.size() );
	}
//-----------------------------------------------------------------------------
	
	

/*
=================================================
	FragmentOutput::operator ==
=================================================
*/
	bool GraphicsPipelineDesc::FragmentOutput::operator == (const FragmentOutput &rhs) const
	{
		return	id		== rhs.id		and
				index	== rhs.index	and
				type	== rhs.type;
	}
//-----------------------------------------------------------------------------
	


/*
=================================================
	AddShaderSource
=================================================
*/
	static void AddShaderSource (INOUT FixedMap<EShader, PipelineDescription::Shader, 8> &shadersMap, EShader shaderType,
								 EShaderLangFormat fmt, StringView entry, String &&src)
	{
		auto&	shader = shadersMap.insert({ shaderType, {} }).first->second;
		ASSERT( shader.data.count( fmt ) == 0 );

		shader.AddShaderData( fmt, entry, std::move(src) );
	}
	
/*
=================================================
	AddShaderBinary8
=================================================
*/
	static void AddShaderBinary8 (INOUT FixedMap<EShader, PipelineDescription::Shader, 8> &shadersMap, EShader shaderType,
								  EShaderLangFormat fmt, StringView entry, Array<uint8_t> &&bin)
	{
		auto&	shader = shadersMap.insert({ shaderType, {} }).first->second;
		ASSERT( shader.data.count( fmt ) == 0 );

		shader.AddShaderData( fmt, entry, std::move(bin) );
	}
	
/*
=================================================
	AddShaderBinary32
=================================================
*/
	static void AddShaderBinary32 (INOUT FixedMap<EShader, PipelineDescription::Shader, 8> &shadersMap, EShader shaderType,
								   EShaderLangFormat fmt, StringView entry, Array<uint> &&bin)
	{
		auto&	shader = shadersMap.insert({ shaderType, {} }).first->second;
		ASSERT( shader.data.count( fmt ) == 0 );

		shader.AddShaderData( fmt, entry, std::move(bin) );
	}
	
/*
=================================================
	AddShaderModule
=================================================
*/
	static void AddShaderModule (INOUT FixedMap<EShader, PipelineDescription::Shader, 8> &shadersMap, EShader shaderType,
								 EShaderLangFormat fmt, const PipelineDescription::VkShaderPtr &module)
	{
		auto&	shader = shadersMap.insert({ shaderType, {} }).first->second;
		ASSERT( shader.data.count( fmt ) == 0 );

		shader.data.insert({ fmt, module });
	}

/*
=================================================
	SetSpecializationConstants
=================================================
*/
	static void SetSpecializationConstants (INOUT FixedMap<EShader, PipelineDescription::Shader, 8> &shadersMap, EShader shaderType,
											ArrayView<PipelineDescription::SpecConstant> values)
	{
		auto&	shader = shadersMap.insert({ shaderType, {} }).first->second;
		ASSERT( shader.data.size() > 0 );
		ASSERT( shader.specConstants.empty() );

		CopySpecConstants( values, OUT shader.specConstants );
	}
//-----------------------------------------------------------------------------

	
/*
=================================================
	constructor
=================================================
*/
	GraphicsPipelineDesc::GraphicsPipelineDesc ()
	{
	}
	
/*
=================================================
	IsGraphicsShader
=================================================
*/
	ND_ inline bool  IsGraphicsShader (EShader shaderType)
	{
		switch( shaderType ) { 
			case EShader::Vertex :
			case EShader::TessControl :
			case EShader::TessEvaluation :
			case EShader::Geometry :
			case EShader::Fragment :
				return true;
		}
		return false;
	}

/*
=================================================
	AddShader
=================================================
*/
	GraphicsPipelineDesc&  GraphicsPipelineDesc::AddShader (EShader shaderType, EShaderLangFormat fmt, StringView entry, String &&src)
	{
		ASSERT( IsGraphicsShader( shaderType ) );
		AddShaderSource( INOUT _shaders, shaderType, fmt, entry, std::move(src) );
		return *this;
	}
	
	GraphicsPipelineDesc&  GraphicsPipelineDesc::AddShader (EShader shaderType, EShaderLangFormat fmt, StringView entry, Array<uint8_t> &&bin)
	{
		ASSERT( IsGraphicsShader( shaderType ) );
		AddShaderBinary8( INOUT _shaders, shaderType, fmt, entry, std::move(bin) );
		return *this;
	}
	
	GraphicsPipelineDesc&  GraphicsPipelineDesc::AddShader (EShader shaderType, EShaderLangFormat fmt, StringView entry, Array<uint> &&bin)
	{
		ASSERT( IsGraphicsShader( shaderType ) );
		AddShaderBinary32( INOUT _shaders, shaderType, fmt, entry, std::move(bin) );
		return *this;
	}
	
	GraphicsPipelineDesc&  GraphicsPipelineDesc::AddShader (EShader shaderType, EShaderLangFormat fmt, const VkShaderPtr &module)
	{
		ASSERT( IsGraphicsShader( shaderType ) );
		AddShaderModule( INOUT _shaders, shaderType, fmt, std::move(module) );
		return *this;
	}

/*
=================================================
	SetSpecConstants
=================================================
*/
	GraphicsPipelineDesc&  GraphicsPipelineDesc::SetSpecConstants (EShader shaderType, ArrayView< SpecConstant > values)
	{
		ASSERT( IsGraphicsShader( shaderType ) );
		
		SetSpecializationConstants( INOUT _shaders, shaderType, values );
		return *this;
	}
//-----------------------------------------------------------------------------
	


/*
=================================================
	constructor
=================================================
*/
	MeshPipelineDesc::MeshPipelineDesc ()
	{
	}

/*
=================================================
	IsMeshProcessingShader
=================================================
*/
	ND_ inline bool  IsMeshProcessingShader (EShader shaderType)
	{
		switch ( shaderType ) {
			case EShader::MeshTask :
			case EShader::Mesh :
			case EShader::Fragment :
				return true;
		}
		return false;
	}

/*
=================================================
	AddShader
=================================================
*/
	MeshPipelineDesc&  MeshPipelineDesc::AddShader (EShader shaderType, EShaderLangFormat fmt, StringView entry, String &&src)
	{
		ASSERT( IsMeshProcessingShader( shaderType ) );
		AddShaderSource( INOUT _shaders, shaderType, fmt, entry, std::move(src) );
		return *this;
	}
	
	MeshPipelineDesc&  MeshPipelineDesc::AddShader (EShader shaderType, EShaderLangFormat fmt, StringView entry, Array<uint8_t> &&bin)
	{
		ASSERT( IsMeshProcessingShader( shaderType ) );
		AddShaderBinary8( INOUT _shaders, shaderType, fmt, entry, std::move(bin) );
		return *this;
	}
	
	MeshPipelineDesc&  MeshPipelineDesc::AddShader (EShader shaderType, EShaderLangFormat fmt, StringView entry, Array<uint> &&bin)
	{
		ASSERT( IsMeshProcessingShader( shaderType ) );
		AddShaderBinary32( INOUT _shaders, shaderType, fmt, entry, std::move(bin) );
		return *this;
	}
	
	MeshPipelineDesc&  MeshPipelineDesc::AddShader (EShader shaderType, EShaderLangFormat fmt, const VkShaderPtr &module)
	{
		ASSERT( IsMeshProcessingShader( shaderType ) );
		AddShaderModule( INOUT _shaders, shaderType, fmt, std::move(module) );
		return *this;
	}

/*
=================================================
	SetSpecConstants
=================================================
*/
	MeshPipelineDesc&  MeshPipelineDesc::SetSpecConstants (EShader shaderType, ArrayView< SpecConstant > values)
	{
		ASSERT( IsMeshProcessingShader( shaderType ) );
		
		SetSpecializationConstants( INOUT _shaders, shaderType, values );
		return *this;
	}
//-----------------------------------------------------------------------------
	


/*
=================================================
	constructor
=================================================
*/
	RayTracingPipelineDesc::RayTracingPipelineDesc ()
	{
	}

/*
=================================================
	IsRayTracingShader
=================================================
*/
	ND_ inline bool  IsRayTracingShader (EShader shaderType)
	{
		switch ( shaderType ) {
			case EShader::RayGen :
			case EShader::RayAnyHit :
			case EShader::RayClosestHit :
			case EShader::RayMiss :
			case EShader::RayIntersection :
			case EShader::RayCallable :
				return true;
		}
		return false;
	}

/*
=================================================
	AddShader
=================================================
*/
	RayTracingPipelineDesc&  RayTracingPipelineDesc::AddShader (EShader shaderType, EShaderLangFormat fmt, StringView entry, String &&src)
	{
		ASSERT( IsRayTracingShader( shaderType ) );
		AddShaderSource( INOUT _shaders, shaderType, fmt, entry, std::move(src) );
		return *this;
	}
	
	RayTracingPipelineDesc&  RayTracingPipelineDesc::AddShader (EShader shaderType, EShaderLangFormat fmt, StringView entry, Array<uint8_t> &&bin)
	{
		ASSERT( IsRayTracingShader( shaderType ) );
		AddShaderBinary8( INOUT _shaders, shaderType, fmt, entry, std::move(bin) );
		return *this;
	}
	
	RayTracingPipelineDesc&  RayTracingPipelineDesc::AddShader (EShader shaderType, EShaderLangFormat fmt, StringView entry, Array<uint> &&bin)
	{
		ASSERT( IsRayTracingShader( shaderType ) );
		AddShaderBinary32( INOUT _shaders, shaderType, fmt, entry, std::move(bin) );
		return *this;
	}
	
	RayTracingPipelineDesc&  RayTracingPipelineDesc::AddShader (EShader shaderType, EShaderLangFormat fmt, const VkShaderPtr &module)
	{
		ASSERT( IsRayTracingShader( shaderType ) );
		AddShaderModule( INOUT _shaders, shaderType, fmt, std::move(module) );
		return *this;
	}

/*
=================================================
	SetSpecConstants
=================================================
*/
	RayTracingPipelineDesc&  RayTracingPipelineDesc::SetSpecConstants (EShader shaderType, ArrayView< SpecConstant > values)
	{
		ASSERT( IsRayTracingShader( shaderType ) );
		
		SetSpecializationConstants( INOUT _shaders, shaderType, values );
		return *this;
	}
//-----------------------------------------------------------------------------


	
/*
=================================================
	constructor
=================================================
*/
	ComputePipelineDesc::ComputePipelineDesc () :
		_localSizeSpec{UNDEFINED_SPECIALIZATION}
	{}

/*
=================================================
	AddShader
=================================================
*/
	ComputePipelineDesc&  ComputePipelineDesc::AddShader (EShaderLangFormat fmt, StringView entry, String &&src)
	{
		ASSERT( _shader.data.count( fmt ) == 0 );
		_shader.AddShaderData( fmt, entry, std::move(src) );
		return *this;
	}
	
	ComputePipelineDesc&  ComputePipelineDesc::AddShader (EShaderLangFormat fmt, StringView entry, Array<uint8_t> &&bin)
	{
		ASSERT( _shader.data.count( fmt ) == 0 );
		_shader.AddShaderData( fmt, entry, std::move(bin) );
		return *this;
	}
	
	ComputePipelineDesc&  ComputePipelineDesc::AddShader (EShaderLangFormat fmt, StringView entry, Array<uint> &&bin)
	{
		ASSERT( _shader.data.count( fmt ) == 0 );
		_shader.AddShaderData( fmt, entry, std::move(bin) );
		return *this;
	}
	
	ComputePipelineDesc&  ComputePipelineDesc::AddShader (EShaderLangFormat fmt, const VkShaderPtr &module)
	{
		ASSERT( _shader.data.count( fmt ) == 0 );
		_shader.data.insert({ fmt, module });
		return *this;
	}

/*
=================================================
	SetSpecConstants
=================================================
*/
	ComputePipelineDesc&  ComputePipelineDesc::SetSpecConstants (ArrayView< SpecConstant > values)
	{
		ASSERT( _shader.data.size() > 0 );
		ASSERT( _shader.specConstants.empty() );
		
		CopySpecConstants( values, OUT _shader.specConstants );
		return *this;
	}
//-----------------------------------------------------------------------------


}	// FG
