#include "Batch.h"
#include <cassert>
#include <iostream>
int main(){
 using namespace batch;
 assert(maximum({{1,2,25},{2,1,8}},9)==8);
 assert(maximum({{1,2,25},{1,3,25}},9)==5);
 assert(maximum({{1,2,25},{1,3,24}},9)==0);
 assert(maximum({},9)==0);assert(maximum({{1,0,999}},9)==0);
 assert(maximum({{9,1,25}},9)==0);assert(maximum({{1,1,-1}},9)==0);
 assert(maximum({{1,1,9999999}},9)==limit);
 Selection s{2,100,200,17,9,24};Job job;
 assert(!job.start(s,0,10));assert(!job.start(s,11,10));assert(job.start(s,3,10));
 assert(!job.start(s,1,10));assert(job.canCraft(s,10));
 auto other=s;other.recipe++;assert(!job.canCraft(other,10));
 other=s;other.session++;assert(!job.canCraft(other,10));
 other=s;other.sub++;assert(!job.canCraft(other,10));assert(!job.canCraft(s,0));
 job.submitted(5);assert(!job.canCraft(s,10));assert(job.confirm(29));
 assert(job.completed==1&&job.active);job.submitted(29);assert(job.confirm(53));
 job.submitted(53);assert(job.confirm(77));assert(job.completed==3&&!job.active);
 assert(job.start(s,10,10));job.submitted(5);assert(!job.confirm(5));assert(!job.active);
 assert(job.start(s,10,10));job.submitted(5);assert(!job.confirm(28));assert(!job.active);
 assert(job.start(s,10,10));job.stop();assert(!job.canCraft(s,10));
 std::cout<<"Batch material limits, duplicate costs, yield, session/selection guard, cancellation and no-output failure passed\n";
}
