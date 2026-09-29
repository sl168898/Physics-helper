#include "bin/Rendering/IconRasterPolicy.h"
#include <cstdlib>
#include <iostream>
#include <vector>
#include <string>

static int checks = 0;
static void require(bool condition, const char* message)
{
    ++checks;
    if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}

int main()
{
    using IconRasterPolicy::MakePlan;
    auto omen = MakePlan(128,128,1024);
    auto food = MakePlan(512,512,1024);
    require(omen.width == 1024 && omen.height == 1024 && omen.scale == 8, "Omen has eight times the linear pixel resolution");
    require(food.width == 1024 && food.height == 1024 && food.scale == 2, "Food and runes have double linear resolution");
    auto rectangle = MakePlan(512,256,1024);
    require(rectangle.width == 1024 && rectangle.height == 512, "Preserve aspect ratio");
    for (unsigned request : {0u,128u,512u,4096u,0xffffffffu}) {
        auto original=MakePlan(512,256,request);
        require(original.width==512 && original.height==256 && original.scale==1, "Unmarked or unsupported requests retain original dimensions");
    }
    auto large=MakePlan(2048,1024,1024);
    require(large.width==2048 && large.height==1024 && large.scale==1, "Never downsample existing larger artwork");
    require(MakePlan(0,0,1024).scale==1, "Invalid source size does not divide by zero");
    require(MakePlan(-1,512,1024).scale==1, "Reject negative source size");
    const float logicalSize=512.0f*0.2f;
    require(logicalSize==102.4f && food.width>512, "Texture pixels and unchanged logical dimensions are independent");
    auto food2k=MakePlan(512,512,2048);
    auto omen2k=MakePlan(128,128,2048);
    require(food2k.width==2048 && food2k.height==2048 && food2k.scale==4, "Food/runes render at 2K");
    require(omen2k.width==2048 && omen2k.height==2048 && omen2k.scale==16, "Omen renders at 2K");
    auto rectangle2k=MakePlan(512,256,2048);
    require(rectangle2k.width==2048 && rectangle2k.height==1024, "2K preserves aspect ratio");
    require(IconRasterPolicy::ClampI4RenderSize(2048)==2048, "I4 does not clamp 2K to 1K");
    require(IconRasterPolicy::ClampI4RenderSize(4096)==2048, "4K requests are capped at 2K");
    require(IconRasterPolicy::ClampI4RenderSize(0)==16, "I4 rejects zero-size targets");
    require(IconRasterPolicy::ClampI4RenderSize(1024)==1024, "Retains 1K compatibility");
    std::cout << checks << " HD icon sizing checks passed\n";
}
