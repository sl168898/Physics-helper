#include "bin/Rendering/IconMipmaps.h"
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>

static int checks=0;
static void require(bool result, const char* message)
{
    ++checks;
    if (!result) throw std::runtime_error(message);
}

int main(int argc, char** argv)
{
    try {
        using namespace IconMipmaps;
        require(Build({},0,0).empty(),"invalid dimensions rejected");
        const std::array<std::uint8_t,4> one{255,80,20,255};
        require(Build(one,2,2).empty(),"short buffer rejected");
        require(Build(one,4097,1).empty(),"oversized dimensions rejected");
        auto singleton=Build(one,1,1);
        require(singleton.size()==1 && singleton[0].rgba==std::vector<std::uint8_t>(one.begin(),one.end()),"one pixel preserved");
        // Averaging red with fully transparent black must preserve red hue.
        std::array<std::uint8_t,16> alpha{255,0,0,255, 0,0,0,0, 0,0,0,0, 0,0,0,0};
        const auto chain=Build(alpha,2,2);
        require(chain.size()==2,"2x2 chain reaches 1x1");
        require(chain.back().rgba[0]==255 && chain.back().rgba[1]==0 && chain.back().rgba[2]==0,"alpha-weighted colour prevents dark fringe");
        require(chain.back().rgba[3]==64,"partial coverage preserved");
        require(chain[0].rgba[4]==255 && chain[0].rgba[7]==0,"transparent neighbour inherits colour without opacity");
        std::array<std::uint8_t,12> odd{0,0,0,255, 0,0,0,255, 255,255,255,255};
        const auto oddChain=Build(odd,3,1);
        require(oddChain.back().rgba[0]==85,"odd-width final column contributes");
        const auto oddVertical=Build(odd,1,3);
        require(oddVertical.back().rgba[0]==85,"odd-height final row contributes");
        std::vector<std::uint8_t> blank(8*4*4,0);
        require(Build(blank,8,4).back().rgba==std::vector<std::uint8_t>(4,0),"empty artwork remains transparent");
        std::vector<std::uint8_t> solid(2048*2048*4);
        for(std::size_t i=0;i<solid.size();i+=4) { solid[i]=17;solid[i+1]=33;solid[i+2]=55;solid[i+3]=123; }
        const auto hd=Build(solid,2048,2048);
        require(hd.size()==12,"2K has complete 12-level mip chain");
        bool dimensions=true, colours=true;
        int size=2048;
        for(const auto& level:hd) {
            dimensions &= level.width==size && level.height==size && level.rgba.size()==static_cast<std::size_t>(size)*size*4;
            for(std::size_t i=0;i<level.rgba.size();i+=4) colours &= level.rgba[i]==17 && level.rgba[i+1]==33 && level.rgba[i+2]==55 && level.rgba[i+3]==123;
            size=(std::max)(1,size/2);
        }
        require(dimensions,"all mip dimensions are valid for D3D11");
        require(colours,"flat colours and translucency survive every level");
        require(hd.front().rgba==solid,"original 2K visible pixels preserved");
        const auto rect=Build(blank,8,4);
        require(rect.size()==4 && rect[1].width==4 && rect[1].height==2 && rect[2].width==2 && rect[2].height==1,"rectangular chain preserves valid aspect progression");
        std::cout << checks << " mip and transparency checks passed\n";
        // Export the exact production mip generator for the milk-jug comparison.
        if(argc==3) {
            std::ifstream input(argv[1],std::ios::binary);
            std::vector<std::uint8_t> rgba((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());
            auto levels=Build(rgba,2048,2048);
            if(levels.size()!=12) return 2;
            for(std::size_t i=0;i<levels.size();++i) {
                std::ofstream output(std::string(argv[2])+std::to_string(i)+".rgba",std::ios::binary);
                output.write(reinterpret_cast<const char*>(levels[i].rgba.data()),levels[i].rgba.size());
                if(!output) return 3;
            }
        }
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
    return 0;
}
