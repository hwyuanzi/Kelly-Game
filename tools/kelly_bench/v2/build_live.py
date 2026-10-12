"""Build strong, local-only referee opponents; never edits official assets."""
import os, shutil, subprocess
from pathlib import Path
here=Path(__file__).resolve().parent
root=here.parents[2]
s=(root/'bots/hollan/bot.cpp').read_text()
s=s.replace('long long choose_bet(const State& s)', 'long long hollan_strategy(const State& s)')
extra=r'''
namespace frozen {
#define main old_protocol_main
#include "baseline.cpp"
#undef main
}
int test_policy=0;
long long choose_bet(const State& s) {
    if(test_policy==0) {
        frozen::State o;o.role=s.role;o.round=s.round;o.probs=s.probs;
        o.my_capital=s.my_capital;o.opp_capital=s.opp_capital;o.time_used=s.time_used;
        return frozen::choose_bet(o);
    }
    double p=s.role=='A'?s.probs[static_cast<size_t>(s.round)]:1-s.probs[static_cast<size_t>(s.round)];
    if(test_policy==4)return hollan::model_bet((s.round/12)%2?4:1,p,s.my_capital,s.opp_capital);
    if(test_policy==5)return hollan::model_bet(s.round<8?8:5,p,s.my_capital,s.opp_capital);
    static hollan::Planner dp;
    if(dp.probability.empty())dp.reset(s);
    dp.deadline=chrono::steady_clock::now()+chrono::seconds(10);
    int model=test_policy==1?0:test_policy==2?10:12;
    if(s.time_used>65 || !dp.prepare(model))return hollan::model_bet(7,p,s.my_capital,s.opp_capital);
    auto next=dp.value[static_cast<size_t>(model)].data()+(static_cast<size_t>(s.round)+1)*dp.grid.size();
    auto b=dp.predict(model,static_cast<size_t>(s.round),s.opp_capital,1-p,true);
    return dp.policy(next,s.my_capital,p,b);
}
'''
s=s.replace('// ============================================================',extra+'\n// ============================================================')
s=s.replace('int main() {','int test_protocol_main() {')
s=s.rstrip()[:-1]+'    return 0;\n}\n'
s+='\nint main(int argc,char**argv) {test_policy=argc>1?atoi(argv[1]):0;return test_protocol_main();}\n'
(here/'live_bot.cpp').write_text(s)
cxx=next(x for x in [os.getenv('CXX'),'c++','g++','clang++'] if x and shutil.which(x))
subprocess.run([cxx,'-std=c++17','-O2','-Wno-return-type','live_bot.cpp','-o','live_bot.exe' if os.name=='nt' else 'live_bot'],cwd=here,check=True)
