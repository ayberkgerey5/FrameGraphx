// Copyright (c) 2018-2019,  Zhirnov Andrey. For more information see 'LICENSE'

#include "GLSLShaderTraceTestUtils.h"

/*
=================================================
	CompileShaders
=================================================
*/
static bool CompileShaders (VulkanDevice &vulkan, ShaderCompiler &shaderCompiler, OUT VkShaderModule &vertShader, OUT VkShaderModule &fragShader)
{
	// create vertex shader
	{
		static const char	vert_shader_source[] = R"#(
const vec2	g_Positions[] = {
	{-1.0f, -1.0f}, {-1.0f, 2.0f}, {2.0f, -1.0f},	// primitive 0 - must hit
	{-1.0f,  2.0f},									// primitive 1 - miss
	{-2.0f,  0.0f}									// primitive 2 - must hit
};

layout(location=0) out vec4  out_Position;

layout(location=2) out VertOutput {
	vec2	out_Texcoord;
	vec4	out_Color;
};

void main()
{
	out_Position = vec4( g_Positions[gl_VertexIndex], float(gl_VertexIndex) * 0.01f, 1.0f );
	gl_Position = out_Position;
	out_Texcoord = g_Positions[gl_VertexIndex].xy * 0.5f + 0.5f;
	out_Color = mix(vec4(1.0, 0.3, 0.0, 0.8), vec4(0.6, 0.9, 0.1, 1.0), float(gl_VertexIndex) / float(g_Positions.length()));
})#";

		CHECK_ERR( shaderCompiler.Compile( OUT vertShader, vulkan, {vert_shader_source}, EShLangVertex ));
	}

	// create fragment shader
	{
		static const char	frag_shader_source[] = R"#(
layout(location = 0) out vec4  out_Color;

layout(location=0) in vec4  in_Position;

layout(location=2) in VertOutput {
	vec2	in_Texcoord;
	vec4	in_Color;
};

float Fn1 (const int i, out int res)
{
	float f = 0.0f;
	res = 11;
	for (int j = i; j < 10; ++j) {
		f += 1.2f *
				cos(float(j));
		if (f > 15.7f) {
			res = j;
			return f * 10.0f;
		}
	}
	return fract(f);
}

void main ()
{
	ivec2 c1;
	float c0 = Fn1( 3, c1.x );
	out_Color[1] = c0 + float(c1.x);
	return;
})#";

		CHECK_ERR( shaderCompiler.Compile( OUT fragShader, vulkan, {frag_shader_source}, EShLangFragment, 0 ));
	}
	return true;
}

/*
=================================================
	CreatePipeline
=================================================
*/
static bool CreatePipeline (VulkanDevice &vulkan, VkShaderModule vertShader, VkShaderModule fragShader,
							VkDescriptorSetLayout dsLayout, VkRenderPass renderPass,
							OUT VkPipelineLayout &outPipelineLayout, OUT VkPipeline &outPipeline)
{
	// create pipeline layout
	{
		VkPipelineLayoutCreateInfo	info = {};
		info.sType					= VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		info.setLayoutCount			= 1;
		info.pSetLayouts			= &dsLayout;
		info.pushConstantRangeCount	= 0;
		info.pPushConstantRanges	= null;

		VK_CHECK( vulkan.vkCreatePipelineLayout( vulkan.GetVkDevice(), &info, null, OUT &outPipelineLayout ));
	}

	VkPipelineShaderStageCreateInfo			stages[2] = {};
	stages[0].sType		= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage		= VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module	= vertShader;
	stages[0].pName		= "main";
	stages[1].sType		= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage		= VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module	= fragShader;
	stages[1].pName		= "main";

	VkPipelineVertexInputStateCreateInfo	vertex_input = {};
	vertex_input.sType		= VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	
	VkPipelineInputAssemblyStateCreateInfo	input_assembly = {};
	input_assembly.sType	= VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly.topology	= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

	VkPipelineViewportStateCreateInfo		viewport = {};
	viewport.sType			= VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport.viewportCount	= 1;
	viewport.scissorCount	= 1;

	VkPipelineRasterizationStateCreateInfo	rasterization = {};
	rasterization.sType			= VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterization.polygonMode	= VK_POLYGON_MODE_FILL;
	rasterization.cullMode		= VK_CULL_MODE_NONE;
	rasterization.frontFace		= VK_FRONT_FACE_COUNTER_CLOCKWISE;

	VkPipelineMultisampleStateCreateInfo	multisample = {};
	multisample.sType					= VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample.rasterizationSamples	= VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo	depth_stencil = {};
	depth_stencil.sType					= VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil.depthTestEnable		= VK_FALSE;
	depth_stencil.depthWriteEnable		= VK_FALSE;
	depth_stencil.depthCompareOp		= VK_COMPARE_OP_LESS_OR_EQUAL;
	depth_stencil.depthBoundsTestEnable	= VK_FALSE;
	depth_stencil.stencilTestEnable		= VK_FALSE;

	VkPipelineColorBlendAttachmentState		blend_attachment = {};
	blend_attachment.blendEnable		= VK_FALSE;
	blend_attachment.colorWriteMask		= VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo		blend_state = {};
	blend_state.sType				= VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	blend_state.logicOpEnable		= VK_FALSE;
	blend_state.attachmentCount		= 1;
	blend_state.pAttachments		= &blend_attachment;

	VkDynamicState							dynamic_states[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo		dynamic_state = {};
	dynamic_state.sType				= VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.dynamicStateCount	= uint(CountOf( dynamic_states ));
	dynamic_state.pDynamicStates	= dynamic_states;

	// create pipeline
	VkGraphicsPipelineCreateInfo	info = {};
	info.sType					= VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	info.stageCount				= uint(CountOf( stages ));
	info.pStages				= stages;
	info.pViewportState			= &viewport;
	info.pVertexInputState		= &vertex_input;
	info.pInputAssemblyState	= &input_assembly;
	info.pRasterizationState	= &rasterization;
	info.pMultisampleState		= &multisample;
	info.pDepthStencilState		= &depth_stencil;
	info.pColorBlendState		= &blend_state;
	info.pDynamicState			= &dynamic_state;
	info.layout					= outPipelineLayout;
	info.renderPass				= renderPass;
	info.subpass				= 0;

	VK_CHECK( vulkan.vkCreateGraphicsPipelines( vulkan.GetVkDevice(), VK_NULL_HANDLE, 1, &info, null, OUT &outPipeline ));
	return true;
}

/*
=================================================
	GLSLShaderTrace_Test1
=================================================
*/
extern bool GLSLShaderTrace_Test1 (VulkanDeviceExt& vulkan, const TestHelpers &helper)
{
	// create renderpass and framebuffer
	uint			width = 16, height = 16;
	VkRenderPass	render_pass;
	VkImage			color_target;
	VkDeviceMemory	image_mem;
	VkFramebuffer	framebuffer;
	CHECK_ERR( CreateRenderTarget( vulkan, VK_FORMAT_R8G8B8A8_UNORM, width, height, 0,
								   OUT render_pass, OUT color_target, OUT image_mem, OUT framebuffer ));


	// create pipeline
	ShaderCompiler	shader_compiler;
	VkShaderModule	vert_shader, frag_shader;
	CHECK_ERR( CompileShaders( vulkan, shader_compiler, OUT vert_shader, OUT frag_shader ));

	VkDescriptorSetLayout	ds_layout;
	VkDescriptorSet			desc_set;
	CHECK_ERR( CreateDebugDescriptorSet( vulkan, helper, VK_SHADER_STAGE_FRAGMENT_BIT, OUT ds_layout, OUT desc_set ));

	VkPipelineLayout	ppln_layout;
	VkPipeline			pipeline;
	CHECK_ERR( CreatePipeline( vulkan, vert_shader, frag_shader, ds_layout, render_pass, OUT ppln_layout, OUT pipeline ));


	// build command buffer
	VkCommandBufferBeginInfo	begin = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, null, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT, null };
	VK_CHECK( vulkan.vkBeginCommandBuffer( helper.cmdBuffer, &begin ));

	// image layout undefined -> color_attachment
	{
		VkImageMemoryBarrier	barrier = {};
		barrier.sType			= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.srcAccessMask	= 0;
		barrier.dstAccessMask	= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		barrier.oldLayout		= VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout		= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		barrier.image			= color_target;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange	= {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

		vulkan.vkCmdPipelineBarrier( helper.cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0,
									 0, null, 0, null, 1, &barrier);
	}
	
	// setup storage buffer
	{
		const uint	data[] = { 
			width/2, height/2,		// selected pixel
			~0u,					// any sample
			~0u						// any primitive
		};

		vulkan.vkCmdFillBuffer( helper.cmdBuffer, helper.debugOutputBuf, sizeof(data), VK_WHOLE_SIZE, 0 );
		vulkan.vkCmdUpdateBuffer( helper.cmdBuffer, helper.debugOutputBuf, 0, sizeof(data), data );
	}

	// debug output storage read/write after write
	{
		VkBufferMemoryBarrier	barrier = {};
		barrier.sType			= VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		barrier.srcAccessMask	= VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask	= VK_ACCESS_SHADER_WRITE_BIT;
		barrier.buffer			= helper.debugOutputBuf;
		barrier.offset			= 0;
		barrier.size			= VK_WHOLE_SIZE;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		
		vulkan.vkCmdPipelineBarrier( helper.cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
									 0, null, 1, &barrier, 0, null);
	}

	// begin render pass
	{
		VkClearValue			clear_value = {{{ 0.0f, 0.0f, 0.0f, 1.0f }}};
		VkRenderPassBeginInfo	begin_rp	= {};
		begin_rp.sType				= VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		begin_rp.framebuffer		= framebuffer;
		begin_rp.renderPass			= render_pass;
		begin_rp.renderArea			= { {0,0}, {width, height} };
		begin_rp.clearValueCount	= 1;
		begin_rp.pClearValues		= &clear_value;

		vulkan.vkCmdBeginRenderPass( helper.cmdBuffer, &begin_rp, VK_SUBPASS_CONTENTS_INLINE );
	}
			
	vulkan.vkCmdBindPipeline( helper.cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline );
	vulkan.vkCmdBindDescriptorSets( helper.cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ppln_layout, 0, 1, &desc_set, 0, null );
	
	// set dynamic states
	{
		VkViewport	viewport = {};
		viewport.x			= 0.0f;
		viewport.y			= 0.0f;
		viewport.width		= float(width);
		viewport.height		= float(height);
		viewport.minDepth	= 0.0f;
		viewport.maxDepth	= 1.0f;
		vulkan.vkCmdSetViewport( helper.cmdBuffer, 0, 1, &viewport );

		VkRect2D	scissor_rect = { {0,0}, {width, height} };
		vulkan.vkCmdSetScissor( helper.cmdBuffer, 0, 1, &scissor_rect );
	}
			
	vulkan.vkCmdDraw( helper.cmdBuffer, 5, 1, 0, 0 );
			
	vulkan.vkCmdEndRenderPass( helper.cmdBuffer );

	// debug output storage read after write
	{
		VkBufferMemoryBarrier	barrier = {};
		barrier.sType			= VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
		barrier.srcAccessMask	= VK_ACCESS_SHADER_WRITE_BIT;
		barrier.dstAccessMask	= VK_ACCESS_TRANSFER_READ_BIT;
		barrier.buffer			= helper.debugOutputBuf;
		barrier.offset			= 0;
		barrier.size			= VK_WHOLE_SIZE;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		
		vulkan.vkCmdPipelineBarrier( helper.cmdBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
									 0, null, 1, &barrier, 0, null);
	}

	// copy shader debug output into host visible memory
	{
		VkBufferCopy	region = {};
		region.srcOffset	= 0;
		region.dstOffset	= 0;
		region.size			= VkDeviceSize(helper.debugOutputSize);

		vulkan.vkCmdCopyBuffer( helper.cmdBuffer, helper.debugOutputBuf, helper.readBackBuf, 1, &region );
	}

	VK_CHECK( vulkan.vkEndCommandBuffer( helper.cmdBuffer ));


	// submit commands and wait
	{
		VkSubmitInfo	submit = {};
		submit.sType				= VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit.commandBufferCount	= 1;
		submit.pCommandBuffers		= &helper.cmdBuffer;

		VK_CHECK( vulkan.vkQueueSubmit( helper.queue, 1, &submit, VK_NULL_HANDLE ));
		VK_CHECK( vulkan.vkQueueWaitIdle( helper.queue ));
	}

	// destroy all
	{
		vulkan.vkDestroyDescriptorSetLayout( vulkan.GetVkDevice(), ds_layout, null );
		vulkan.vkDestroyShaderModule( vulkan.GetVkDevice(), vert_shader, null );
		vulkan.vkDestroyShaderModule( vulkan.GetVkDevice(), frag_shader, null );
		vulkan.vkDestroyPipelineLayout( vulkan.GetVkDevice(), ppln_layout, null );
		vulkan.vkDestroyPipeline( vulkan.GetVkDevice(), pipeline, null );
		vulkan.vkFreeMemory( vulkan.GetVkDevice(), image_mem, null );
		vulkan.vkDestroyImage( vulkan.GetVkDevice(), color_target, null );
		vulkan.vkDestroyRenderPass( vulkan.GetVkDevice(), render_pass, null );
		vulkan.vkDestroyFramebuffer( vulkan.GetVkDevice(), framebuffer, null );
	}
	

	// process debug output
	Array<String>	debug_output;
	CHECK_ERR( shader_compiler.GetDebugOutput( frag_shader, helper.readBackPtr, helper.debugOutputSize, OUT debug_output ));

	{
		static const char	ref1[] = R"#(//> gl_FragCoord: float4 {8.500000, 8.500000, 0.010625, 1.000000}
no source

//> gl_PrimitiveID: int {0}
no source

//> in_Position: float4 {0.062500, 0.062500, 0.010625, 1.000000}
no source

//> in_Texcoord: float2 {0.531250, 0.531250}
no source

//> in_Color: float4 {0.915000, 0.427500, 0.021250, 0.842500}
no source

//> i: int {3}
11. float Fn1 (const int i, out int res)

//> f: float {0.000000}
13. f = 0.0f;

//> res: int {11}
14. res = 11;

//> j: int {3}
//  i: int {3}
15. j = i; j < 10; ++j) {

//> (out): bool {true}
//  i: int {3}
//  j: int {3}
15. for (int j = i; j < 10; ++j) {

//> cos(): float {-0.989992}
//  j: int {3}
16. *
17. 				cos(float(j));

//> f: float {-1.187991}
//  j: int {3}
//  cos(): float {-0.989992}
16. f += 1.2f *
17. 				cos(float(j));

//> (out): bool {false}
//  f: float {-1.187991}
18. if (f > 15.7f) {

//> j: int {4}
15. ++j) {

//> (out): bool {true}
//  i: int {3}
//  j: int {4}
15. for (int j = i; j < 10; ++j) {

//> cos(): float {-0.653644}
//  j: int {4}
16. *
17. 				cos(float(j));

//> f: float {-1.972363}
//  j: int {4}
//  cos(): float {-0.653644}
16. f += 1.2f *
17. 				cos(float(j));

//> (out): bool {false}
//  f: float {-1.972363}
18. if (f > 15.7f) {

//> j: int {5}
15. ++j) {

//> (out): bool {true}
//  i: int {3}
//  j: int {5}
15. for (int j = i; j < 10; ++j) {

//> cos(): float {0.283662}
//  j: int {5}
16. *
17. 				cos(float(j));

//> f: float {-1.631969}
//  j: int {5}
//  cos(): float {0.283662}
16. f += 1.2f *
17. 				cos(float(j));

//> (out): bool {false}
//  f: float {-1.631969}
18. if (f > 15.7f) {

//> j: int {6}
15. ++j) {

//> (out): bool {true}
//  i: int {3}
//  j: int {6}
15. for (int j = i; j < 10; ++j) {

//> cos(): float {0.960170}
//  j: int {6}
16. *
17. 				cos(float(j));

//> f: float {-0.479765}
//  j: int {6}
//  cos(): float {0.960170}
16. f += 1.2f *
17. 				cos(float(j));

//> (out): bool {false}
//  f: float {-0.479765}
18. if (f > 15.7f) {

//> j: int {7}
15. ++j) {

//> (out): bool {true}
//  i: int {3}
//  j: int {7}
15. for (int j = i; j < 10; ++j) {

//> cos(): float {0.753902}
//  j: int {7}
16. *
17. 				cos(float(j));

//> f: float {0.424918}
//  j: int {7}
//  cos(): float {0.753902}
16. f += 1.2f *
17. 				cos(float(j));

//> (out): bool {false}
//  f: float {0.424918}
18. if (f > 15.7f) {

//> j: int {8}
15. ++j) {

//> (out): bool {true}
//  i: int {3}
//  j: int {8}
15. for (int j = i; j < 10; ++j) {

//> cos(): float {-0.145500}
//  j: int {8}
16. *
17. 				cos(float(j));

//> f: float {0.250318}
//  j: int {8}
//  cos(): float {-0.145500}
16. f += 1.2f *
17. 				cos(float(j));

//> (out): bool {false}
//  f: float {0.250318}
18. if (f > 15.7f) {

//> j: int {9}
15. ++j) {

//> (out): bool {true}
//  i: int {3}
//  j: int {9}
15. for (int j = i; j < 10; ++j) {

//> cos(): float {-0.911130}
//  j: int {9}
16. *
17. 				cos(float(j));

//> f: float {-0.843038}
//  j: int {9}
//  cos(): float {-0.911130}
16. f += 1.2f *
17. 				cos(float(j));

//> (out): bool {false}
//  f: float {-0.843038}
18. if (f > 15.7f) {

//> j: int {10}
15. ++j) {

//> (out): bool {false}
//  i: int {3}
//  j: int {10}
15. for (int j = i; j < 10; ++j) {

//> fract(): float {0.156962}
//  f: float {-0.843038}
23. return fract(f);

//> c0: float {0.156962}
29. c0 = Fn1( 3, c1.x );

//> out_Color: float2 {0.000000, 11.156962}
//  c0: float {0.156962}
30. out_Color[1] = c0 + float(c1.x);

//> (out): void
31. return;

)#";

		static const char	ref2[] = R"#(//> gl_FragCoord: float4 {8.500000, 8.500000, 0.028403, 1.000000}
no source

//> gl_PrimitiveID: int {2}
no source

//> in_Position: float4 {0.062500, 0.062500, 0.028403, 1.000000}
no source

//> in_Texcoord: float2 {0.531250, 0.531250}
no source

//> in_Color: float4 {0.772778, 0.640833, 0.056806, 0.913611}
no source

//> i: int {3}
11. float Fn1 (const int i, out int res)

//> f: float {0.000000}
13. f = 0.0f;

//> res: int {11}
14. res = 11;

//> j: int {3}
//  i: int {3}
15. j = i; j < 10; ++j) {

//> (out): bool {true}
//  i: int {3}
//  j: int {3}
15. for (int j = i; j < 10; ++j) {

//> cos(): float {-0.989992}
//  j: int {3}
16. *
17. 				cos(float(j));

//> f: float {-1.187991}
//  j: int {3}
//  cos(): float {-0.989992}
16. f += 1.2f *
17. 				cos(float(j));

//> (out): bool {false}
//  f: float {-1.187991}
18. if (f > 15.7f) {

//> j: int {4}
15. ++j) {

//> (out): bool {true}
//  i: int {3}
//  j: int {4}
15. for (int j = i; j < 10; ++j) {

//> cos(): float {-0.653644}
//  j: int {4}
16. *
17. 				cos(float(j));

//> f: float {-1.972363}
//  j: int {4}
//  cos(): float {-0.653644}
16. f += 1.2f *
17. 				cos(float(j));

//> (out): bool {false}
//  f: float {-1.972363}
18. if (f > 15.7f) {

//> j: int {5}
15. ++j) {

//> (out): bool {true}
//  i: int {3}
//  j: int {5}
15. for (int j = i; j < 10; ++j) {

//> cos(): float {0.283662}
//  j: int {5}
16. *
17. 				cos(float(j));

//> f: float {-1.631969}
//  j: int {5}
//  cos(): float {0.283662}
16. f += 1.2f *
17. 				cos(float(j));

//> (out): bool {false}
//  f: float {-1.631969}
18. if (f > 15.7f) {

//> j: int {6}
15. ++j) {

//> (out): bool {true}
//  i: int {3}
//  j: int {6}
15. for (int j = i; j < 10; ++j) {

//> cos(): float {0.960170}
//  j: int {6}
16. *
17. 				cos(float(j));

//> f: float {-0.479765}
//  j: int {6}
//  cos(): float {0.960170}
16. f += 1.2f *
17. 				cos(float(j));

//> (out): bool {false}
//  f: float {-0.479765}
18. if (f > 15.7f) {

//> j: int {7}
15. ++j) {

//> (out): bool {true}
//  i: int {3}
//  j: int {7}
15. for (int j = i; j < 10; ++j) {

//> cos(): float {0.753902}
//  j: int {7}
16. *
17. 				cos(float(j));

//> f: float {0.424918}
//  j: int {7}
//  cos(): float {0.753902}
16. f += 1.2f *
17. 				cos(float(j));

//> (out): bool {false}
//  f: float {0.424918}
18. if (f > 15.7f) {

//> j: int {8}
15. ++j) {

//> (out): bool {true}
//  i: int {3}
//  j: int {8}
15. for (int j = i; j < 10; ++j) {

//> cos(): float {-0.145500}
//  j: int {8}
16. *
17. 				cos(float(j));

//> f: float {0.250318}
//  j: int {8}
//  cos(): float {-0.145500}
16. f += 1.2f *
17. 				cos(float(j));

//> (out): bool {false}
//  f: float {0.250318}
18. if (f > 15.7f) {

//> j: int {9}
15. ++j) {

//> (out): bool {true}
//  i: int {3}
//  j: int {9}
15. for (int j = i; j < 10; ++j) {

//> cos(): float {-0.911130}
//  j: int {9}
16. *
17. 				cos(float(j));

//> f: float {-0.843038}
//  j: int {9}
//  cos(): float {-0.911130}
16. f += 1.2f *
17. 				cos(float(j));

//> (out): bool {false}
//  f: float {-0.843038}
18. if (f > 15.7f) {

//> j: int {10}
15. ++j) {

//> (out): bool {false}
//  i: int {3}
//  j: int {10}
15. for (int j = i; j < 10; ++j) {

//> fract(): float {0.156962}
//  f: float {-0.843038}
23. return fract(f);

//> c0: float {0.156962}
29. c0 = Fn1( 3, c1.x );

//> out_Color: float2 {0.000000, 11.156962}
//  c0: float {0.156962}
30. out_Color[1] = c0 + float(c1.x);

//> (out): void
31. return;

)#";

		CHECK_ERR( debug_output.size() == 2 );

		if ( debug_output[0] == ref1 ) {
			CHECK_ERR( debug_output[1] == ref2 );
		}else{
			CHECK_ERR( debug_output[0] == ref2 );
			CHECK_ERR( debug_output[1] == ref1 );
		}
	}

	FG_LOGI( TEST_NAME << " - passed" );
	return true;
}
