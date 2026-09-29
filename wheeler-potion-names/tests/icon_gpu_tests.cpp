#define NOMINMAX
#include "bin/Rendering/IconTextureUpload.h"
#include <d3dcompiler.h>
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

using Microsoft::WRL::ComPtr;
static int checks=0;
static void require(bool ok,const char* message) { ++checks;if(!ok)throw std::runtime_error(message); }
static void hr(HRESULT result,const char* message) { if(FAILED(result))throw std::runtime_error(std::string(message)+" HRESULT="+std::to_string(result)); }

int main()
{
    try {
        ComPtr<ID3D11Device> device; ComPtr<ID3D11DeviceContext> context;
        const D3D_FEATURE_LEVEL requested=D3D_FEATURE_LEVEL_11_0;
        hr(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,&requested,1,D3D11_SDK_VERSION,
            device.GetAddressOf(),nullptr,context.GetAddressOf()),"Create WARP device");
        // Three bright columns per four, repeated across a 16px texture. At one
        // output pixel the correct area coverage is 75%, but level-0 bilinear
        // sampling sees only the middle black/white pair and returns 50%.
        std::vector<std::uint8_t> pixels(16*16*4);
        for(int y=0;y<16;++y)for(int x=0;x<16;++x) {
            auto at=(y*16+x)*4;auto colour=static_cast<std::uint8_t>(x%4==3?0:255);
            pixels[at]=pixels[at+1]=pixels[at+2]=colour;pixels[at+3]=255;
        }
        ComPtr<ID3D11ShaderResourceView> view;
        hr(IconTextureUpload::Create(device.Get(),pixels,16,16,view.GetAddressOf()),"Upload mip texture");
        D3D11_SHADER_RESOURCE_VIEW_DESC vd{};view->GetDesc(&vd);
        require(vd.Texture2D.MipLevels==5,"SRV exposes complete mip chain");
        ComPtr<ID3D11Resource> resource;view->GetResource(resource.GetAddressOf());
        ComPtr<ID3D11Texture2D> texture;hr(resource.As(&texture),"Query texture");
        D3D11_TEXTURE2D_DESC td{};texture->GetDesc(&td);
        require(td.Width==16&&td.Height==16&&td.MipLevels==5,"GPU texture retains source dimensions and all levels");

        const char* shader=R"(
            struct V {float4 pos:SV_POSITION;float2 uv:TEXCOORD0;};
            V VS(uint id:SV_VertexID) {
                float2 p=id==0?float2(-1,-1):(id==1?float2(-1,3):float2(3,-1));
                V o;o.pos=float4(p,0,1);o.uv=(p+1)*float2(.5,-.5)+float2(0,1);return o;
            }
            Texture2D tex:register(t0);SamplerState smp:register(s0);
            float4 PS(V input):SV_TARGET {return tex.Sample(smp,input.uv);}
        )";
        ComPtr<ID3DBlob> vsCode,psCode,errors;
        hr(D3DCompile(shader,strlen(shader),nullptr,nullptr,nullptr,"VS","vs_5_0",0,0,vsCode.GetAddressOf(),errors.GetAddressOf()),"Compile vertex shader");
        errors.Reset();
        hr(D3DCompile(shader,strlen(shader),nullptr,nullptr,nullptr,"PS","ps_5_0",0,0,psCode.GetAddressOf(),errors.GetAddressOf()),"Compile pixel shader");
        ComPtr<ID3D11VertexShader> vs;ComPtr<ID3D11PixelShader> ps;
        hr(device->CreateVertexShader(vsCode->GetBufferPointer(),vsCode->GetBufferSize(),nullptr,vs.GetAddressOf()),"Create vertex shader");
        hr(device->CreatePixelShader(psCode->GetBufferPointer(),psCode->GetBufferSize(),nullptr,ps.GetAddressOf()),"Create pixel shader");
        D3D11_TEXTURE2D_DESC out{};out.Width=out.Height=out.MipLevels=out.ArraySize=out.SampleDesc.Count=1;
        out.Format=DXGI_FORMAT_R8G8B8A8_UNORM;out.Usage=D3D11_USAGE_DEFAULT;out.BindFlags=D3D11_BIND_RENDER_TARGET;
        ComPtr<ID3D11Texture2D> target,staging;hr(device->CreateTexture2D(&out,nullptr,target.GetAddressOf()),"Create output");
        ComPtr<ID3D11RenderTargetView> rtv;hr(device->CreateRenderTargetView(target.Get(),nullptr,rtv.GetAddressOf()),"Create target view");
        out.Usage=D3D11_USAGE_STAGING;out.BindFlags=0;out.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        hr(device->CreateTexture2D(&out,nullptr,staging.GetAddressOf()),"Create readback");
        D3D11_RASTERIZER_DESC raster{};raster.FillMode=D3D11_FILL_SOLID;raster.CullMode=D3D11_CULL_NONE;
        ComPtr<ID3D11RasterizerState> rs;hr(device->CreateRasterizerState(&raster,rs.GetAddressOf()),"Create raster state");
        context->RSSetState(rs.Get());
        D3D11_VIEWPORT viewport{0,0,1,1,0,1};context->RSSetViewports(1,&viewport);
        auto* rawTarget=rtv.Get();context->OMSetRenderTargets(1,&rawTarget,nullptr);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(vs.Get(),nullptr,0);context->PSSetShader(ps.Get(),nullptr,0);
        auto* rawView=view.Get();context->PSSetShaderResources(0,1,&rawView);

        auto render=[&](D3D11_SAMPLER_DESC desc) {
            ComPtr<ID3D11SamplerState> sampler;hr(device->CreateSamplerState(&desc,sampler.GetAddressOf()),"Create sampler");
            auto* rawSampler=sampler.Get();context->PSSetSamplers(0,1,&rawSampler);
            const float clear[4]={0,0,0,0};context->ClearRenderTargetView(rtv.Get(),clear);context->Draw(3,0);
            context->CopyResource(staging.Get(),target.Get());
            D3D11_MAPPED_SUBRESOURCE map{};hr(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&map),"Read GPU output");
            std::array<std::uint8_t,4> result{};memcpy(result.data(),map.pData,4);context->Unmap(staging.Get(),0);return result;
        };
        const auto samplerDesc=IconTextureUpload::MipSamplerDescription();
        require(samplerDesc.MaxLOD>12.0f&&samplerDesc.Filter==D3D11_FILTER_MIN_MAG_MIP_LINEAR,"sampler permits trilinear mip selection");
        const auto filtered=render(samplerDesc);
        auto oldDesc=samplerDesc;oldDesc.MaxLOD=0.0f;const auto old=render(oldDesc);
        require(filtered[0]>=190&&filtered[0]<=193,"GPU mip sampling preserves 75 percent area coverage");
        require(old[0]>=127&&old[0]<=129,"old level-zero clamp reproduces aliasing");
        require(filtered[0]>old[0]+50,"corrected sampler changes actual GPU output");
        require(filtered[3]==255&&old[3]==255,"opaque alpha preserved");
        require(filtered[0]==filtered[1]&&filtered[1]==filtered[2],"neutral colour preserved");
        std::cout<<checks<<" D3D11 WARP checks passed; old="<<int(old[0])<<", filtered="<<int(filtered[0])<<"\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
    return 0;
}
