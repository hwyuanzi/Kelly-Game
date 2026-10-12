// Independent exhaustive reference for both orientations and nested planners.
#define main hollan_protocol_main
#include "../../../bots/hollan/bot.cpp"
#undef main
#include <cassert>
#include <functional>
#include <random>

int main() {
    std::mt19937 rng(129731);size_t checked=0,choices=0;
    for(int trial=0;trial<40;++trial) {
        State s;s.role=trial%2?'A':'B';s.my_capital=1+rng()%49;s.opp_capital=1+rng()%49;
        for(int t=0;t<7;++t)s.probs.push_back((rng()%11)/10.0);
        hollan::Planner dp;dp.reset(s);long long total=dp.total;size_t width=size_t(total)+1;
        std::vector<double> exact[2][14];
        auto probability=[&](int side,int t){double p=dp.probability[size_t(t)];return side?1-p:p;};
        std::function<long long(int,int,int,long long,double)> prediction;
        prediction=[&](int m,int side,int t,long long x,double p)->long long {
            if(m<10)return hollan::model_bet(m,p,x,total-x);
            int base=m==10?0:m==11?7:m==12?10:12;
            long long y=total-x,b=prediction(base,1-side,t,y,1-p),bestbet=0;double best=-1;
            const auto& table=exact[side][base];assert(!table.empty());
            for(long long a=0;a<=x/5;++a) {
                long long pot=a+b;double score=p*table[(size_t(t)+1)*width+size_t(x+std::min(pot,y))]+(1-p)*table[(size_t(t)+1)*width+size_t(x-std::min(pot,x))];
                if(score>best+1e-14){best=score;bestbet=a;}
            }return bestbet;
        };
        std::function<void(int,int)> build;
        build=[&](int side,int model) {
            auto& table=exact[side][model];if(!table.empty())return;
            if(model>=10)build(1-side,model==10?0:model==11?7:model==12?10:12);
            table.resize(8*width);
            for(long long x=0;x<=total;++x)table[7*width+size_t(x)]=double(x)/total;
            for(int t=6;t>=0;--t) {
                table[size_t(t)*width]=0;table[size_t(t)*width+size_t(total)]=1;
                for(long long x=1;x<total;++x) {
                    double p=probability(side,t),best=-1;long long y=total-x,b=prediction(model,1-side,t,y,1-p);
                    for(long long a=0;a<=x/5;++a) {
                        long long pot=a+b;double score=p*table[(size_t(t)+1)*width+size_t(x+std::min(pot,y))]+(1-p)*table[(size_t(t)+1)*width+size_t(x-std::min(pot,x))];best=std::max(best,score);
                    }table[size_t(t)*width+size_t(x)]=best;
                }
            }
        };
        for(int side=0;side<2;++side)for(int model=0;model<14;++model) {
            build(side,model);assert(dp.prepare(model,8.,side!=0));
            const auto& actual=side?dp.reverse_value[model]:dp.value[model];
            for(int t=0;t<7;++t)for(long long x=1;x<total;++x) {
                assert(std::abs(actual[size_t(t)*width+size_t(x)]-exact[side][model][size_t(t)*width+size_t(x)])<1e-12);++checked;
                assert(dp.predict(model,size_t(t),x,probability(side,t),side!=0)==prediction(model,side,t,x,probability(side,t)));
            }
        }
        for(int model=0;model<14;++model)for(long long x=1;x<total;++x) {
            s.my_capital=x;s.opp_capital=total-x;s.round=0;dp.previous_round=-1;dp.active=model;
            auto bet=dp.choose(s);assert(bet>=0 && bet<=x/5);
            auto next=dp.value[model].data()+width;double p=dp.probability[0];auto b=prediction(model,1,0,total-x,1-p);
            double chosen=dp.action_value(next,x,total-x,bet,b,p);
            for(long long a=0;a<=x/5;++a){assert(chosen+1e-12>=dp.action_value(next,x,total-x,a,b,p));++choices;}
        }
    }
    for(int horizon:{10,2000,5000})for(char role:{'A','B'})for(long long x:{1LL,4LL,5LL,1000LL,8000LL}) {
        State s;s.role=role;s.probs.assign(horizon,.63);s.my_capital=x;s.opp_capital=9000-x;s.time_used=100;
        hollan::Planner dp;auto bet=dp.choose(s);assert(bet>=0 && bet<=x/5);
        for(const auto& v:dp.value)assert(v.empty());for(const auto& v:dp.reverse_value)assert(v.empty());
    }
    State s;s.probs.assign(2000,.55);s.my_capital=1000;s.opp_capital=8000;
    hollan::Planner dp;dp.reset(s);assert(!dp.prepare(0,-1));assert(dp.value[0].empty());assert(!dp.prepare(0));
    hollan::Planner timed;timed.reset(s);timed.deadline=std::chrono::steady_clock::now();assert(!timed.prepare(13));for(auto const& v:timed.value)assert(v.empty());
    hollan::Planner tiny;tiny.reset(s);tiny.cache_limit=1;assert(!tiny.prepare(13));for(auto const& v:tiny.value)assert(v.empty());
    std::cout<<"PASS: "<<checked<<" exact Bellman states in both orientations; "<<choices<<" exhaustive integer action comparisons; nested opponent predictions; time, horizon and memory fallbacks.\n";
}
