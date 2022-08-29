// Copyright (c) 2018-2020,  Zhirnov Andrey. For more information see 'LICENSE'

#include "VFrameGraph.h"
#include "VResourceManager.h"
#include "UnitTest_Common.h"


static void SamplerCache_Test1 (const FrameGraph &fg)
{
	SamplerDesc		desc;
	desc.SetAddressMode( EAddressMode::ClampToEdge );
	desc.SetFilter( EFilter::Linear, EFilter::Linear, EMipmapFilter::Linear );

	SamplerID	samp1 = fg->CreateSampler( desc );
	SamplerID	samp2 = fg->CreateSampler( desc );
	TEST( samp1 == samp2 );
	
	desc.SetAddressMode( EAddressMode::Repeat );
	
	SamplerID	samp3 = fg->CreateSampler( desc );
	TEST( samp2 != samp3 );

	fg->ReleaseResource( samp1 );
	fg->ReleaseResource( samp2 );
	fg->ReleaseResource( samp3 );
}


static void PipelineCache_Test1 (const FrameGraph &fg)
{
}


static void PipelineResources_Test1 (const FrameGraph &fg)
{
	ComputePipelineDesc	desc;

	desc.SetLocalGroupSize( 16, 8, 1 );

	desc.AddDescriptorSet(
			DescriptorSetID{"0"},
			0,
			{},
			{},
			{},
			{{ UniformID{"un_OutImage"}, EImage::Tex2D, EPixelFormat::RGBA8_UNorm, EShaderAccess::WriteOnly, BindingIndex{UMax, 0u}, 1, EShaderStages::Compute }},
			{},
			{{ UniformID{"un_SSBO"}, 16_b, 0_b, EShaderAccess::ReadOnly, BindingIndex{UMax, 1u}, 1, EShaderStages::Compute, false }} );

	desc.AddShader( EShaderLangFormat::SPIRV_100, "main", Array<uint32_t>{
			0x07230203, 0x00010000, 0x00080007, 0x0000003B, 0x00000000, 0x00020011, 0x00000001, 0x0006000B, 0x00000002, 0x4C534C47, 0x6474732E, 0x3035342E, 
			0x00000000, 0x0003000E, 0x00000000, 0x00000001, 0x0007000F, 0x00000005, 0x00000005, 0x6E69616D, 0x00000000, 0x0000000E, 0x00000017, 0x00060010, 
			0x00000005, 0x00000011, 0x00000010, 0x00000008, 0x00000001, 0x00030007, 0x00000001, 0x00000000, 0x00410003, 0x00000002, 0x000001C2, 0x00000001, 
			0x4F202F2F, 0x646F4D70, 0x50656C75, 0x65636F72, 0x64657373, 0x746E6520, 0x702D7972, 0x746E696F, 0x69616D20, 0x2F2F0A6E, 0x4D704F20, 0x6C75646F, 
			0x6F725065, 0x73736563, 0x61206465, 0x2D6F7475, 0x2D70616D, 0x61636F6C, 0x6E6F6974, 0x2F2F0A73, 0x4D704F20, 0x6C75646F, 0x6F725065, 0x73736563, 
			0x61206465, 0x2D6F7475, 0x2D70616D, 0x646E6962, 0x73676E69, 0x202F2F0A, 0x6F4D704F, 0x656C7564, 0x636F7250, 0x65737365, 0x6C632064, 0x746E6569, 
			0x6C757620, 0x316E616B, 0x2F0A3030, 0x704F202F, 0x75646F4D, 0x7250656C, 0x7365636F, 0x20646573, 0x67726174, 0x652D7465, 0x7620766E, 0x616B6C75, 
			0x302E316E, 0x202F2F0A, 0x6F4D704F, 0x656C7564, 0x636F7250, 0x65737365, 0x6E652064, 0x2D797274, 0x6E696F70, 0x616D2074, 0x230A6E69, 0x656E696C, 
			0x000A3120, 0x00090004, 0x415F4C47, 0x735F4252, 0x72617065, 0x5F657461, 0x64616873, 0x6F5F7265, 0x63656A62, 0x00007374, 0x00040005, 0x00000005, 
			0x6E69616D, 0x00000000, 0x00030005, 0x0000000A, 0x00007675, 0x00080005, 0x0000000E, 0x475F6C67, 0x61626F6C, 0x766E496C, 0x7461636F, 0x496E6F69, 
			0x00000044, 0x00070005, 0x00000017, 0x4E5F6C67, 0x6F576D75, 0x72476B72, 0x7370756F, 0x00000000, 0x00050005, 0x0000001F, 0x67617266, 0x6F6C6F43, 
			0x00000072, 0x00040005, 0x00000029, 0x535F6E75, 0x004F4253, 0x00060006, 0x00000029, 0x00000000, 0x5F627373, 0x61746164, 0x00000000, 0x00030005, 
			0x0000002B, 0x00000000, 0x00050005, 0x00000034, 0x4F5F6E75, 0x6D497475, 0x00656761, 0x00040047, 0x0000000E, 0x0000000B, 0x0000001C, 0x00040047, 
			0x00000017, 0x0000000B, 0x00000018, 0x00040048, 0x00000029, 0x00000000, 0x00000018, 0x00050048, 0x00000029, 0x00000000, 0x00000023, 0x00000000, 
			0x00030047, 0x00000029, 0x00000003, 0x00040047, 0x0000002B, 0x00000022, 0x00000000, 0x00040047, 0x0000002B, 0x00000021, 0x00000001, 0x00040047, 
			0x00000034, 0x00000022, 0x00000000, 0x00040047, 0x00000034, 0x00000021, 0x00000000, 0x00030047, 0x00000034, 0x00000019, 0x00040047, 0x00000016, 
			0x0000000B, 0x00000019, 0x00020013, 0x00000003, 0x00030021, 0x00000004, 0x00000003, 0x00030016, 0x00000007, 0x00000020, 0x00040017, 0x00000008, 
			0x00000007, 0x00000002, 0x00040020, 0x00000009, 0x00000007, 0x00000008, 0x00040015, 0x0000000B, 0x00000020, 0x00000000, 0x00040017, 0x0000000C, 
			0x0000000B, 0x00000003, 0x00040020, 0x0000000D, 0x00000001, 0x0000000C, 0x0004003B, 0x0000000D, 0x0000000E, 0x00000001, 0x00040017, 0x0000000F, 
			0x0000000B, 0x00000002, 0x0004002B, 0x0000000B, 0x00000013, 0x00000010, 0x0004002B, 0x0000000B, 0x00000014, 0x00000008, 0x0004002B, 0x0000000B, 
			0x00000015, 0x00000001, 0x0006002C, 0x0000000C, 0x00000016, 0x00000013, 0x00000014, 0x00000015, 0x0004003B, 0x0000000D, 0x00000017, 0x00000001, 
			0x00040017, 0x0000001D, 0x00000007, 0x00000004, 0x00040020, 0x0000001E, 0x00000007, 0x0000001D, 0x0004002B, 0x0000000B, 0x00000020, 0x00000000, 
			0x00040020, 0x00000021, 0x00000007, 0x00000007, 0x0004002B, 0x00000007, 0x00000028, 0x3F800000, 0x0003001E, 0x00000029, 0x0000001D, 0x00040020, 
			0x0000002A, 0x00000002, 0x00000029, 0x0004003B, 0x0000002A, 0x0000002B, 0x00000002, 0x00040015, 0x0000002C, 0x00000020, 0x00000001, 0x0004002B, 
			0x0000002C, 0x0000002D, 0x00000000, 0x00040020, 0x0000002E, 0x00000002, 0x00000007, 0x00090019, 0x00000032, 0x00000007, 0x00000001, 0x00000000, 
			0x00000000, 0x00000000, 0x00000002, 0x00000004, 0x00040020, 0x00000033, 0x00000000, 0x00000032, 0x0004003B, 0x00000033, 0x00000034, 0x00000000, 
			0x00040017, 0x00000038, 0x0000002C, 0x00000002, 0x00050036, 0x00000003, 0x00000005, 0x00000000, 0x00000004, 0x000200F8, 0x00000006, 0x0004003B, 
			0x00000009, 0x0000000A, 0x00000007, 0x0004003B, 0x0000001E, 0x0000001F, 0x00000007, 0x00040008, 0x00000001, 0x00000010, 0x00000000, 0x0004003D, 
			0x0000000C, 0x00000010, 0x0000000E, 0x0007004F, 0x0000000F, 0x00000011, 0x00000010, 0x00000010, 0x00000000, 0x00000001, 0x00040070, 0x00000008, 
			0x00000012, 0x00000011, 0x0004003D, 0x0000000C, 0x00000018, 0x00000017, 0x00050084, 0x0000000C, 0x00000019, 0x00000016, 0x00000018, 0x0007004F, 
			0x0000000F, 0x0000001A, 0x00000019, 0x00000019, 0x00000000, 0x00000001, 0x00040070, 0x00000008, 0x0000001B, 0x0000001A, 0x00050088, 0x00000008, 
			0x0000001C, 0x00000012, 0x0000001B, 0x0003003E, 0x0000000A, 0x0000001C, 0x00040008, 0x00000001, 0x00000012, 0x00000000, 0x00050041, 0x00000021, 
			0x00000022, 0x0000000A, 0x00000020, 0x0004003D, 0x00000007, 0x00000023, 0x00000022, 0x0006000C, 0x00000007, 0x00000024, 0x00000002, 0x0000000D, 
			0x00000023, 0x00050041, 0x00000021, 0x00000025, 0x0000000A, 0x00000015, 0x0004003D, 0x00000007, 0x00000026, 0x00000025, 0x0006000C, 0x00000007, 
			0x00000027, 0x00000002, 0x0000000E, 0x00000026, 0x00060041, 0x0000002E, 0x0000002F, 0x0000002B, 0x0000002D, 0x00000020, 0x0004003D, 0x00000007, 
			0x00000030, 0x0000002F, 0x00070050, 0x0000001D, 0x00000031, 0x00000024, 0x00000027, 0x00000028, 0x00000030, 0x0003003E, 0x0000001F, 0x00000031, 
			0x00040008, 0x00000001, 0x00000014, 0x00000000, 0x0004003D, 0x00000032, 0x00000035, 0x00000034, 0x0004003D, 0x0000000C, 0x00000036, 0x0000000E, 
			0x0007004F, 0x0000000F, 0x00000037, 0x00000036, 0x00000036, 0x00000000, 0x00000001, 0x0004007C, 0x00000038, 0x00000039, 0x00000037, 0x0004003D, 
			0x0000001D, 0x0000003A, 0x0000001F, 0x00040063, 0x00000035, 0x00000039, 0x0000003A, 0x000100FD, 0x00010038 });

	CPipelineID			ppln = fg->CreatePipeline( desc );
	TEST( ppln );

	PipelineResources	res;

	//res.BindBuffer( UniformID("un_SSBO"), buffer );

	// TODO

	fg->ReleaseResource( ppln );
}


extern void UnitTest_VResourceManager (const FrameGraph &fg)
{
	SamplerCache_Test1( fg );
	PipelineCache_Test1( fg );
	PipelineResources_Test1( fg );

	FG_LOGI( "UnitTest_VResourceManager - passed" );
}
