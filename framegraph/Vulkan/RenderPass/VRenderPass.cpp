// Copyright (c) 2018,  Zhirnov Andrey. For more information see 'LICENSE'

#include "VRenderPass.h"
#include "VDevice.h"
#include "VLocalImage.h"
#include "VEnumCast.h"

namespace FG
{

/*
=================================================
	destructor
=================================================
*/
	VRenderPass::~VRenderPass ()
	{
		CHECK( _renderPass == VK_NULL_HANDLE );
	}
	
/*
=================================================
	GetColorAttachmentIndex
=================================================
*/
	bool VRenderPass::GetColorAttachmentIndex (const RenderTargetID &id, OUT uint &index) const
	{
		SHAREDLOCK( _rcCheck );

		auto	iter = _mapping.find( id );

		if ( iter != _mapping.end() )
		{
			index = iter->second;
			return true;
		}

		index = ~0u;
		return false;
	}

/*
=================================================
	constructor
=================================================
*/
	VRenderPass::VRenderPass (ArrayView<VLogicalRenderPass*> logicalPasses, ArrayView<GraphicsPipelineDesc::FragmentOutput> fragOutput)
	{
		_Initialize( logicalPasses, fragOutput );
	}
		
	bool VRenderPass::_Initialize (ArrayView<VLogicalRenderPass*> logicalPasses, ArrayView<GraphicsPipelineDesc::FragmentOutput> fragOutput)
	{
		SCOPELOCK( _rcCheck );
		CHECK_ERR( logicalPasses.size() == 1 );		// not supported yet
		CHECK_ERR( logicalPasses.front()->GetColorTargets().size() == fragOutput.size() );

		const auto *	pass		= logicalPasses.front();
		const uint		col_offset	= pass->GetDepthStencilTarget().IsDefined() ? 1 : 0;

		_attachments.resize( fragOutput.size() + col_offset );
		

		_subpasses.resize( 1 );
		VkSubpassDescription&	subpass	= _subpasses[0];

		subpass.pipelineBindPoint	= VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.pColorAttachments	= _attachmentRef.end();
		

		// setup color attachments
		for (auto& frag : fragOutput)
		{
			auto	iter = pass->GetColorTargets().find( frag.id );
			CHECK_ERR( iter != pass->GetColorTargets().end() );
			
			const VkImageLayout			layout	= EResourceState_ToImageLayout( iter->second.state );
			VkAttachmentDescription&	desc	= _attachments[ col_offset + frag.index ];

			desc.flags			= 0;			// TODO: VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT
			desc.format			= VEnumCast( iter->second.desc.format );
			desc.samples		= iter->second.samples;
			desc.loadOp			= iter->second.loadOp;
			desc.storeOp		= iter->second.storeOp;
			desc.initialLayout	= layout;
			desc.finalLayout	= layout;

			_attachmentRef.push_back({ (col_offset + frag.index), layout });
			++subpass.colorAttachmentCount;

			_mapping.insert({ frag.id, col_offset + frag.index });
		}

		CHECK( subpass.colorAttachmentCount == fragOutput.size() );

		if ( subpass.colorAttachmentCount == 0 )
			subpass.pColorAttachments = null;


		// setup depth stencil attachment
		if ( pass->GetDepthStencilTarget().IsDefined() )
		{
			const auto&					ds_target	= pass->GetDepthStencilTarget();
			const VkImageLayout			layout		= EResourceState_ToImageLayout( ds_target.state );
			VkAttachmentDescription&	desc		= _attachments[0];

			desc.flags			= 0;
			desc.format			= VEnumCast( ds_target.desc.format );
			desc.samples		= ds_target.samples;
			desc.loadOp			= ds_target.loadOp;
			desc.stencilLoadOp	= ds_target.loadOp;		// TODO: use resource state to change state
			desc.storeOp		= ds_target.storeOp;
			desc.stencilStoreOp	= ds_target.storeOp;
			desc.initialLayout	= layout;
			desc.finalLayout	= layout;

			subpass.pDepthStencilAttachment	= _attachmentRef.end();
			_attachmentRef.push_back({ 0, layout });
		}

		
		// setup dependencies
		_dependencies.resize( 2 );
		_dependencies[0].srcSubpass			= VK_SUBPASS_EXTERNAL;
		_dependencies[0].dstSubpass			= 0;
		_dependencies[0].srcStageMask		= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		_dependencies[0].dstStageMask		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		_dependencies[0].srcAccessMask		= VK_ACCESS_MEMORY_READ_BIT;
		_dependencies[0].dstAccessMask		= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		_dependencies[0].dependencyFlags	= VK_DEPENDENCY_BY_REGION_BIT;

		_dependencies[1].srcSubpass			= 0;
		_dependencies[1].dstSubpass			= VK_SUBPASS_EXTERNAL;
		_dependencies[1].srcStageMask		= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		_dependencies[1].dstStageMask		= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		_dependencies[1].srcAccessMask		= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		_dependencies[1].dstAccessMask		= VK_ACCESS_MEMORY_READ_BIT;
		_dependencies[1].dependencyFlags	= VK_DEPENDENCY_BY_REGION_BIT;


		// setup create info
		_createInfo					= {};
		_createInfo.sType			= VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		_createInfo.flags			= 0;
		_createInfo.attachmentCount	= uint(_attachments.size());
		_createInfo.pAttachments	= _attachments.data();
		_createInfo.subpassCount	= uint(_subpasses.size());
		_createInfo.pSubpasses		= _subpasses.data();
		_createInfo.dependencyCount	= uint(_dependencies.size());
		_createInfo.pDependencies	= _dependencies.data();


		_CalcHash( _createInfo, OUT _hash, OUT _attachmentHash, OUT _subpassesHash );
		return true;
	}

/*
=================================================
	_CalcHash
=================================================
*/
	void VRenderPass::_CalcHash (const VkRenderPassCreateInfo &ci, OUT HashVal &mainHash, OUT HashVal &attachmentHash, OUT SubpassesHash_t &subpassesHash)
	{
		ASSERT( ci.pNext == null );
		
		const auto AttachmentRefHash = [] (const VkAttachmentReference *attachment) -> HashVal
		{
			return	attachment ?
						(HashOf( attachment->attachment ) + HashOf( attachment->layout )) :
						HashVal();
		};

		const auto AttachmentRefArrayHash = [&AttachmentRefHash] (const VkAttachmentReference* attachments, uint count) -> HashVal
		{
			if ( attachments == null or count == 0 )
				return HashVal();

			HashVal res = HashOf( count );

			for (uint i = 0; i < count; ++i) {
				res << AttachmentRefHash( attachments + i );
			}
			return res;
		};
		
		mainHash = HashVal();


		// calculate attachment hash
		attachmentHash = HashOf( ci.attachmentCount );

		for (uint i = 0; i < ci.attachmentCount; ++i)
		{
			const VkAttachmentDescription&	attachment = ci.pAttachments[i];

			attachmentHash << HashOf( attachment.flags );
			attachmentHash << HashOf( attachment.format );
			attachmentHash << HashOf( attachment.samples );
			attachmentHash << HashOf( attachment.loadOp );
			attachmentHash << HashOf( attachment.storeOp );
			attachmentHash << HashOf( attachment.stencilLoadOp );
			attachmentHash << HashOf( attachment.stencilStoreOp );
			attachmentHash << HashOf( attachment.initialLayout );
			attachmentHash << HashOf( attachment.finalLayout );
		}
		

		// calculate subpasses hash
		subpassesHash.resize( ci.subpassCount );

		for (uint i = 0; i < ci.subpassCount; ++i)
		{
			const VkSubpassDescription&	subpass = ci.pSubpasses[i];
			HashVal &					hash	= subpassesHash[i];
			
			hash =  HashOf( subpass.flags );
			hash << HashOf( subpass.pipelineBindPoint );
			hash << AttachmentRefArrayHash( subpass.pInputAttachments, subpass.inputAttachmentCount );
			hash << AttachmentRefArrayHash( subpass.pColorAttachments, subpass.colorAttachmentCount );
			hash << AttachmentRefArrayHash( subpass.pResolveAttachments, subpass.colorAttachmentCount );
			hash << AttachmentRefHash( subpass.pDepthStencilAttachment );

			for (uint j = 0; j < subpass.preserveAttachmentCount; ++j) {
				hash << HashOf( subpass.pPreserveAttachments[j] );
			}
			mainHash << hash;
		}
		

		// calculate main hash
		mainHash << HashOf( ci.flags );
		mainHash << HashOf( ci.subpassCount );
		mainHash << HashOf( ci.dependencyCount );
		mainHash << attachmentHash;

		for (uint i = 0; i < ci.dependencyCount; ++i)
		{
			const VkSubpassDependency&	dep = ci.pDependencies[i];
			
			mainHash << HashOf( dep.srcSubpass );
			mainHash << HashOf( dep.dstSubpass );
			mainHash << HashOf( dep.srcStageMask );
			mainHash << HashOf( dep.dstStageMask );
			mainHash << HashOf( dep.srcAccessMask );
			mainHash << HashOf( dep.dstAccessMask );
			mainHash << HashOf( dep.dependencyFlags );
		}
	}

/*
=================================================
	Create
=================================================
*/
	bool VRenderPass::Create (const VDevice &dev, StringView dbgName)
	{
		SCOPELOCK( _rcCheck );
		CHECK_ERR( _renderPass == VK_NULL_HANDLE );

		VK_CHECK( dev.vkCreateRenderPass( dev.GetVkDevice(), &_createInfo, null, OUT &_renderPass ) );
		
		_debugName = dbgName;
		return true;
	}

/*
=================================================
	Destroy
=================================================
*/
	void VRenderPass::Destroy (OUT AppendableVkResources_t readyToDelete, OUT AppendableResourceIDs_t)
	{
		SCOPELOCK( _rcCheck );

		if ( _renderPass ) {
			readyToDelete.emplace_back( VK_OBJECT_TYPE_RENDER_PASS, uint64_t(_renderPass) );
		}

		_renderPass		= VK_NULL_HANDLE;
		_createInfo		= Default;
		_hash			= Default;
		_attachmentHash	= Default;

		_subpassesHash.clear();
		_mapping.clear();

		_attachments.clear();
		_attachmentRef.clear();
		_inputAttachRef.clear();
		_resolveAttachRef.clear();
		_subpasses.clear();
		_dependencies.clear();
		_preserves.clear();

		_debugName.clear();
	}

/*
=================================================
	operator ==
=================================================
*/
	inline bool operator == (const VkAttachmentDescription &lhs, const VkAttachmentDescription &rhs)
	{
		return	lhs.flags			== rhs.flags			and
				lhs.format			== rhs.format			and
				lhs.samples			== rhs.samples			and
				lhs.loadOp			== rhs.loadOp			and
				lhs.storeOp			== rhs.storeOp			and
				lhs.stencilLoadOp	== rhs.stencilLoadOp	and
				lhs.stencilStoreOp	== rhs.stencilStoreOp	and
				lhs.initialLayout	== rhs.initialLayout	and
				lhs.finalLayout		== rhs.finalLayout;
	}
	
/*
=================================================
	operator ==
=================================================
*/
	inline bool operator == (const VkAttachmentReference &lhs, const VkAttachmentReference &rhs)
	{
		return	lhs.attachment	== rhs.attachment	and
				lhs.layout		== rhs.layout;
	}

/*
=================================================
	operator ==
=================================================
*/
	inline bool operator == (const VkSubpassDescription &lhs, const VkSubpassDescription &rhs)
	{
		using AttachView = ArrayView<VkAttachmentReference>;
		using PreserveView = ArrayView<uint>;

		auto	lhs_resolve_attachments = lhs.pResolveAttachments ? AttachView{lhs.pResolveAttachments, lhs.colorAttachmentCount} : AttachView{};
		auto	rhs_resolve_attachments = rhs.pResolveAttachments ? AttachView{rhs.pResolveAttachments, rhs.colorAttachmentCount} : AttachView{};

		return	lhs.flags															== rhs.flags														and
				lhs.pipelineBindPoint												== rhs.pipelineBindPoint											and
				AttachView{lhs.pInputAttachments, lhs.inputAttachmentCount}			== AttachView{rhs.pInputAttachments, rhs.inputAttachmentCount}		and
				AttachView{lhs.pColorAttachments, lhs.colorAttachmentCount}			== AttachView{rhs.pColorAttachments, rhs.colorAttachmentCount}		and
				lhs_resolve_attachments												== rhs_resolve_attachments	and
				not lhs.pDepthStencilAttachment										== not rhs.pDepthStencilAttachment									and
				(not lhs.pDepthStencilAttachment or *lhs.pDepthStencilAttachment	== *rhs.pDepthStencilAttachment)									and
				PreserveView{lhs.pPreserveAttachments, lhs.preserveAttachmentCount}	== PreserveView{rhs.pPreserveAttachments, rhs.preserveAttachmentCount};
	}

/*
=================================================
	operator ==
=================================================
*/
	inline bool operator == (const VkSubpassDependency &lhs, const VkSubpassDependency &rhs)
	{
		return	lhs.srcSubpass		== rhs.srcSubpass		and
				lhs.dstSubpass		== rhs.dstSubpass		and
				lhs.srcStageMask	== rhs.srcStageMask		and
				lhs.dstStageMask	== rhs.dstStageMask		and
				lhs.srcAccessMask	== rhs.srcAccessMask	and
				lhs.dstAccessMask	== rhs.dstAccessMask	and
				lhs.dependencyFlags	== rhs.dependencyFlags;
	}

/*
=================================================
	operator ==
=================================================
*/
	bool VRenderPass::operator == (const VRenderPass &rhs) const
	{
		SHAREDLOCK( _rcCheck );
		SHAREDLOCK( rhs._rcCheck );

		using AttachView = ArrayView<VkAttachmentDescription>;
		using SubpassView = ArrayView<VkSubpassDescription>;
		using DepsView = ArrayView<VkSubpassDependency>;
		return	_hash																== rhs._hash																	and
				_attachmentHash														== rhs._attachmentHash															and
				_subpassesHash														== rhs._subpassesHash															and
				_createInfo.flags													== rhs._createInfo.flags														and
				AttachView{_createInfo.pAttachments, _createInfo.attachmentCount}	== AttachView{rhs._createInfo.pAttachments, rhs._createInfo.attachmentCount}	and
				SubpassView{_createInfo.pSubpasses, _createInfo.subpassCount}		== SubpassView{rhs._createInfo.pSubpasses, rhs._createInfo.subpassCount}		and
				DepsView{_createInfo.pDependencies, _createInfo.dependencyCount}	== DepsView{rhs._createInfo.pDependencies, rhs._createInfo.dependencyCount};
	}


}	// FG
