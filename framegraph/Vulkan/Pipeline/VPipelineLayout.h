// Copyright (c) 2018,  Zhirnov Andrey. For more information see 'LICENSE'

#pragma once

#include "framegraph/Shared/ResourceBase.h"
#include "VDescriptorSetLayout.h"

namespace FG
{

	//
	// Vulkan Pipeline Layout
	//

	class VPipelineLayout final
	{
	// types
	private:
		struct DescriptorSet
		{
			RawDescriptorSetLayoutID	layoutId;
			VkDescriptorSetLayout		layout		= VK_NULL_HANDLE;	// TODO: remove?
			uint						index		= 0;

			DescriptorSet () {}
			DescriptorSet (RawDescriptorSetLayoutID id, VkDescriptorSetLayout layout, uint index) : layoutId{id}, layout{layout}, index{index} {}
		};

		using DescriptorSets_t			= FixedMap< DescriptorSetID, DescriptorSet, FG_MaxDescriptorSets >;
		using PushConstants_t			= PipelineDescription::PushConstants_t;
		using VkDescriptorSetLayouts_t	= FixedArray< VkDescriptorSetLayout, FG_MaxDescriptorSets >;
		using VkPushConstantRanges_t	= FixedArray< VkPushConstantRange, FG_MaxPushConstants >;
		using DSLayoutArray_t			= ArrayView<Pair<RawDescriptorSetLayoutID, const ResourceBase<VDescriptorSetLayout> *>>;


	// variables
	private:
		HashVal					_hash;
		VkPipelineLayout		_layout			= VK_NULL_HANDLE;
		DescriptorSets_t		_descriptorSets;
		PushConstants_t			_pushConstants;

		DebugName_t				_debugName;
		
		RWRaceConditionCheck	_rcCheck;

		
	// methods
	public:
		VPipelineLayout () {}
		VPipelineLayout (VPipelineLayout &&) = default;
		VPipelineLayout (const PipelineDescription::PipelineLayout &ppln, DSLayoutArray_t sets);
		~VPipelineLayout ();

		bool Create (const VDevice &dev);
		void Destroy (OUT AppendableVkResources_t, OUT AppendableResourceIDs_t);
		
		bool  GetDescriptorSetLayout (const DescriptorSetID &id, OUT RawDescriptorSetLayoutID &layout, OUT uint &binding) const;

		ND_ bool  IsAllResourcesAlive (const VResourceManagerThread &) const;

		ND_ bool  operator == (const VPipelineLayout &rhs) const;

		ND_ VkPipelineLayout		Handle ()			const	{ SHAREDLOCK( _rcCheck );  return _layout; }
		ND_ HashVal					GetHash ()			const	{ SHAREDLOCK( _rcCheck );  return _hash; }
		
		ND_ StringView				GetDebugName ()		const	{ SHAREDLOCK( _rcCheck );  return _debugName; }

		ND_ PushConstants_t const&	GetPushConstants ()	const	{ SHAREDLOCK( _rcCheck );  return _pushConstants; }


	private:
		void _AddDescriptorSets (const PipelineDescription::PipelineLayout &ppln, DSLayoutArray_t sets,
								 INOUT HashVal &hash, OUT DescriptorSets_t &setsInfo) const;
		void _AddPushConstants (const PipelineDescription::PipelineLayout &ppln, 
								INOUT HashVal &hash, OUT PushConstants_t &pushConst) const;
	};

}	// FG


namespace std
{

	template <>
	struct hash< FG::VPipelineLayout >
	{
		ND_ size_t  operator () (const FG::VPipelineLayout &value) const noexcept {
			return size_t(value.GetHash());
		}
	};

}	// std
