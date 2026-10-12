// Paired population evaluation using the actual source and a frozen v1.
#define main candidate_protocol_main
#ifndef HOLLAN_SOURCE
#define HOLLAN_SOURCE "../../../bots/hollan/bot.cpp"
#endif
#include HOLLAN_SOURCE
#undef main
namespace old {
#define main baseline_protocol_main
#include "baseline.cpp"
#undef main
}
#include <array>
#include <map>
#include <unordered_map>
#include <cassert>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
struct Config {string name; long long a,b;vector<double> p;};
struct Stat {double sum=0,diff=0,diff2=0;size_t n=0;void add(double v,double b){++n;sum+=v;diff+=v-b;diff2+=(v-b)*(v-b);}};
struct Gene {array<double,8> w;};
vector<Gene> genes;
vector<Config> configs(unsigned seed,int count) {
    vector<Config> out;mt19937_64 rng(seed);
    for(string name:{"sample","typical_100_rounds","competition_2000"}) {
        ifstream f("params/"+name+".txt");string line,all;while(getline(f,line))all+=line.substr(0,line.find('#'))+" ";
        Config c;c.name=name;istringstream in(all);in>>c.a>>c.b;double p;while(in>>p)c.p.push_back(p);out.push_back(c);
    }
    for(int i=0;i<count;++i) {
        Config c;c.name="random"+to_string(i);c.a=1000+rng()%7001;c.b=1000+rng()%7001;
        int n=vector<int>{10,30,100,300,2000,100,50,500,25,75,150,200}[i%12];
        for(int t=0;t<n;++t) {double u=generate_canonical<double,53>(rng),p;
            switch(i%10) {
                case 0:p=.4+.3*u;break;case 1:p=.3+.3*u;break;case 2:p=.1+.8*u;break;
                case 3:p=.45+.1*u;break;case 4:p=(t<n/2?.3:.5)+.2*u;break;
                case 5:p=.499+.002*u;break;case 6:p=t%2?.7:.3;break;
                case 7:p=.5;break;case 8:p=t+1==n?.99:.5;break;default:p=.2+.6*u;
            }c.p.push_back(round(p*100000)/100000);
        }out.push_back(c);
    }return out;
}
struct Arena {
    hollan::Planner cand[2]; old::hollan::Planner base[2];
    State new_state[2];old::State old_state[2];
    Config c;
    map<int,map<int,Stat>> prediction;
    int opponent_variant = 4;
    unordered_map<uint64_t,long long> old_cache[2], new_cache[2];
    size_t cache_hits=0, cache_verified=0;
    uint64_t key(int t,long long x,int model) const {return (uint64_t(t)*uint64_t(c.a+c.b+1)+uint64_t(x))*16+uint64_t(model);}
    long long old_choose(old::State const& s,int role) {
#ifdef ARENA_NO_MEMO
        return base[role].choose(s);
#endif
        auto& p=base[role];p.observe(s);auto k=key(s.round,s.my_capital,p.active);auto found=old_cache[role].find(k);
        if(found==old_cache[role].end()) {p.previous_round=-1;auto a=p.choose(s);old_cache[role][k]=a;return a;}
        ++cache_hits;
        if(cache_verified<100) {p.previous_round=-1;assert(p.choose(s)==found->second);++cache_verified;}
        p.previous_x=s.my_capital;p.previous_y=s.opp_capital;p.previous_bet=found->second;p.previous_round=s.round;
        return found->second;
    }
    long long new_choose(State const& s,int role) {
#ifdef ARENA_NO_MEMO
        return cand[role].choose(s);
#endif
        auto& p=cand[role];p.deadline=chrono::steady_clock::now()+chrono::seconds(10);p.observe(s);auto k=key(s.round,s.my_capital,p.active);auto found=new_cache[role].find(k);
        if(found==new_cache[role].end()) {p.previous_round=-1;auto a=p.choose(s);new_cache[role][k]=a;return a;}
        ++cache_hits;
        if(cache_verified<100) {p.previous_round=-1;assert(p.choose(s)==found->second);++cache_verified;}
        p.previous_x=s.my_capital;p.previous_y=s.opp_capital;p.previous_bet=found->second;p.previous_round=s.round;
        return found->second;
    }
    explicit Arena(Config cfg,int res=800):c(std::move(cfg)) {
        for(int r=0;r<2;++r) {
            State s;s.role=r?'B':'A';s.probs=c.p;s.my_capital=r?c.b:c.a;s.opp_capital=r?c.a:c.b;
            cand[r].resolution=res;cand[r].reset(s);
            old::State o;o.role=s.role;o.probs=s.probs;o.my_capital=s.my_capital;o.opp_capital=s.opp_capital;
            base[r].resolution=res;base[r].reset(o);
            base[r].prepare(0);new_state[r]=s;old_state[r]=o;
        }
    }
    void resetgame(int role,int variant) {
        auto& p=cand[role];p.previous_round=-1;p.loss.assign(hollan::MODEL_COUNT,0);p.loss[0]=-.4;p.active=0;p.evidence.assign(hollan::MODEL_COUNT,0);p.observations=0;p.variant=variant;
        auto& b=base[role];b.previous_round=-1;b.loss.assign(10,0);b.loss[0]=-.4;b.active=0;
    }
    long long opponent(int id,int role,int t,long long x,long long y) {
        double p=role?1-c.p[t]:c.p[t];
        if(id<10)return old::hollan::model_bet(id,p,x,y);
        if(id==10)return old::hollan::model_bet(x*3<x+y?1:2,p,x,y);
        if(id==11)return old::hollan::model_bet((t/12)%2?4:1,p,x,y);
        if(id==12) {
            uint64_t z=uint64_t(x)*91872341+uint64_t(y)*1298417+uint64_t(t)*19783301;
            z^=z>>30;z*=0xbf58476d1ce4e5b9ULL;z^=z>>27;z*=0x94d049bb133111ebULL;z^=z>>31;
            return (x/5)*static_cast<long long>(z%10001)/10000;
        }
        if(id==13)return p>.54?x/5:0;
        if(id==14)return max(0LL,min(x/5,(long long)((2*p-1)*x*.6)));
        if(id==15 || id==18 || id==19) {
            int m=id==15?0:id==18?10:12;
            auto& dp=cand[role];dp.prepare(m);
            long long b=dp.predict(m,t,y,1-p,true);
            const double* next=dp.value[m].data()+(size_t(t)+1)*dp.grid.size();
            return dp.policy(next,x,p,b);
        }
        if(id==16) {auto& s=old_state[role];s.round=t;s.my_capital=x;s.opp_capital=y;return old_choose(s,role);}
        if(id==17) {auto& s=new_state[role];s.round=t;s.my_capital=x;s.opp_capital=y;return new_choose(s,role);}
        if(id==20) { // Bait a cautious label, then abruptly risk everything legal.
            return old::hollan::model_bet(t<8?8:5,p,x,y);
        }
        if(id==22) return t<8?old::hollan::model_bet(0,p,x,y):opponent(18,role,t,x,y);
        if(id==23) return opponent((t/5)%2?15:19,role,t,x,y);
        if(id==21) { // Switch more quickly than the old loss memory.
            return old::hollan::model_bet((t/3)%2?5:6,p,x,y);
        }
        if(id>=100) {
            auto w=genes[size_t(id-100)].w;
            double ratio=double(x)/(x+y),edge=2*p-1;
            double fraction=w[0]+w[1]*edge+w[2]*(ratio-.5)+w[3]/sqrt(1.+c.p.size()-t);
            fraction+=w[4]*(t%int(2+abs(w[5])*10)<int(1+abs(w[5])*5)?1:-1);
            if(t+1<int(c.p.size()))fraction+=w[6]*(2*(role?1-c.p[t+1]:c.p[t+1])-1);
            fraction-=w[7]*double(old::hollan::model_bet(0,1-p,y,x))/max(1LL,x);
            return max(0LL,min(x/5,(long long)(fraction*x)));
        }
        return 0;
    }
    long long play(int variant,int opp,int role,vector<double> const& draws) {
        resetgame(role,variant);resetgame(1-role,opponent_variant);
        auto& s=new_state[role];s.my_capital=role?c.b:c.a;s.opp_capital=role?c.a:c.b;
        for(int t=0;t<int(c.p.size()) && s.my_capital && s.opp_capital;++t) {
            s.round=t;long long a;
            if(variant==0) {auto& o=old_state[role];o.round=t;o.my_capital=s.my_capital;o.opp_capital=s.opp_capital;a=old_choose(o,role);}
            else a=new_choose(s,role);
            auto b=opponent(opp,1-role,t,s.opp_capital,s.my_capital);
            if (t >= 3 && s.opp_capital >= 5) {
                long long forecast;
                double p = role ? 1-c.p[size_t(t)] : c.p[size_t(t)];
                if (variant == 0) forecast=old::hollan::model_bet(base[role].active,1-p,s.opp_capital,s.my_capital);
                else {
                    int m=cand[role].active;
                    forecast=m<10?hollan::model_bet(m,1-p,s.opp_capital,s.my_capital):cand[role].predict(m,size_t(t),s.opp_capital,1-p,true);
                }
                prediction[opp][variant].add(abs(double(forecast-b))/double(s.opp_capital/5),0);
            }
            if(a<0 || a>s.my_capital/5 || b<0 || b>s.opp_capital/5){cerr<<"ILLEGAL\n";exit(2);}
            bool win=role?draws[size_t(t)]>c.p[size_t(t)]:draws[size_t(t)]<=c.p[size_t(t)];
            auto transfer=min(a+b,win?s.opp_capital:s.my_capital);
            s.my_capital+=win?transfer:-transfer;s.opp_capital+=win?-transfer:transfer;
        }return s.my_capital;
    }
    double paired(int v,int opp,vector<double> const (&d)[2]) {
        return double(play(v,opp,0,d[0])+play(v,opp,1,d[1]))/(2*(c.a+c.b));
    }
};
#ifndef ARENA_LIBRARY
int main(int argc,char**argv) {
    int games=argc>1?stoi(argv[1]):200,count=argc>2?stoi(argv[2]):9;
    unsigned seed=argc>3?unsigned(stoul(argv[3])):772615;
    vector<int> variants={0,1,2,3,4};
    if(argc>4) {variants={0};istringstream in(argv[4]);string s;while(getline(in,s,','))variants.push_back(stoi(s));}
    vector<int> opponents={0,1,4,7,5,6,3,10,11,12,13,14,15,16,18,19,20,21};
    ifstream gf("tools/kelly_bench/v2/genes.txt");Gene g;while(gf>>g.w[0]){for(size_t i=1;i<8;++i)gf>>g.w[i];genes.push_back(g);}
    for(size_t i=0;i<genes.size();++i)opponents.push_back(100+int(i));
    if (argc > 5) {
        opponents.clear();istringstream in(argv[5]);string s;while(getline(in,s,','))opponents.push_back(stoi(s));
    }
    map<int,map<int,Stat>> stats, predictions, money;
    map<int,Stat> aggregate, hard_aggregate, money_aggregate;
    map<int,map<int,Stat>> ablations;
    size_t hits=0,verified=0,max_bytes=0,unavailable=0;
    mt19937_64 rng(seed^0x935faf);auto start=chrono::steady_clock::now();
    for(auto c:configs(seed,count)) {
        Arena arena(c);cout<<"CONFIG "<<c.name<<" n="<<c.p.size()<<" capital="<<c.a<<"/"<<c.b<<endl;
        map<int,map<int,Stat>> local;
        for(int k=0;k<games;++k) {
            vector<double> draws[2];for(auto&d:draws)for(size_t t=0;t<c.p.size();++t)d.push_back(generate_canonical<double,53>(rng));
            map<int,double> group,hard;int hard_count=0;
            for(int o:opponents) {double baseline=0;map<int,double> scores;bool difficult=(o>=15 && o<=19) || o==22 || o==23; if(difficult)++hard_count;for(int v:variants) {
                double score=arena.paired(v,o,draws);if(v==0)baseline=score;
                stats[o][v].add(score,baseline);money[o][v].add(score*2*(c.a+c.b),baseline*2*(c.a+c.b));local[o][v].add(score,baseline);
                scores[v]=score;group[v]+=score;if(difficult)hard[v]+=score;
            }
                if(scores.count(4))for(auto kv:scores)if(kv.first!=4)ablations[o][kv.first].add(scores[4],kv.second);
            }
            for(int v:variants) {aggregate[v].add(group[v]/opponents.size(),group[0]/opponents.size());money_aggregate[v].add(group[v]*2*(c.a+c.b)/opponents.size(),group[0]*2*(c.a+c.b)/opponents.size());if(hard_count)hard_aggregate[v].add(hard[v]/hard_count,hard[0]/hard_count);}
        }
        hits+=arena.cache_hits;verified+=arena.cache_verified;
        for(auto const& planner:arena.cand) {
            size_t bytes=0;for(auto const& v:planner.value)bytes+=v.size()*sizeof(double);for(auto const& v:planner.reverse_value)bytes+=v.size()*sizeof(double);
            max_bytes=max(max_bytes,bytes);for(bool flag:planner.unavailable)unavailable+=flag?1:0;
        }
        for(auto const& ov:arena.prediction)for(auto const& vs:ov.second) {
            auto& dest=predictions[ov.first][vs.first];dest.sum+=vs.second.sum;dest.n+=vs.second.n;
        }
        for(int o:opponents){cout<<"opp="<<o;for(int v:variants)cout<<" v"<<v<<"="<<local[o][v].sum/games;cout<<endl;}
    }
    cout<<"SUMMARY seed="<<seed<<" scenarios="<<count+3<<" pairs_per_scenario="<<games<<endl<<fixed<<setprecision(7);
    for(int o:opponents)for(int v:variants) {auto s=stats[o][v];double mean=s.diff/s.n;double ci=1.96*sqrt(max(0.,s.diff2/s.n-mean*mean)/(s.n-1));cout<<"opp="<<o<<" variant="<<v<<" n="<<s.n<<" score="<<s.sum/s.n<<" delta="<<mean<<" ci95="<<ci<<endl;}
    for(int o:opponents)for(int v:variants) {auto s=predictions[o][v];cout<<"PRED opp="<<o<<" variant="<<v<<" observations="<<s.n<<" mae_limit="<<(s.n?s.sum/s.n:0)<<endl;}
    for(auto const& ov:ablations)for(auto const& vs:ov.second) {auto s=vs.second;double d=s.diff/s.n;cout<<"ABLATION opp="<<ov.first<<" v4_minus="<<vs.first<<" n="<<s.n<<" delta="<<d<<" ci95="<<1.96*sqrt(max(0.,s.diff2/s.n-d*d)/(s.n-1))<<endl;}
    for(int v:variants)for(int h=0;h<2;++h) {auto s=h?hard_aggregate[v]:aggregate[v];double d=s.diff/s.n;cout<<(h?"HARD":"ALL")<<" variant="<<v<<" n="<<s.n<<" score="<<s.sum/s.n<<" delta="<<d<<" ci95="<<1.96*sqrt(max(0.,s.diff2/s.n-d*d)/(s.n-1))<<endl;}
    for(auto const& ov:money)for(auto const& vs:ov.second) {auto s=vs.second;double d=s.diff/s.n;cout<<"MONEY opp="<<ov.first<<" variant="<<vs.first<<" n="<<s.n<<" mean="<<s.sum/s.n<<" delta="<<d<<" ci95="<<1.96*sqrt(max(0.,s.diff2/s.n-d*d)/(s.n-1))<<endl;}
    for(auto const& vs:money_aggregate) {auto s=vs.second;double d=s.diff/s.n;cout<<"MONEY_ALL variant="<<vs.first<<" n="<<s.n<<" mean="<<s.sum/s.n<<" delta="<<d<<" ci95="<<1.96*sqrt(max(0.,s.diff2/s.n-d*d)/(s.n-1))<<endl;}
    cout<<"CACHE hits="<<hits<<" directly_verified="<<verified<<" max_candidate_table_bytes="<<max_bytes<<" unavailable_model_flags="<<unavailable<<endl;
    cerr<<"elapsed="<<chrono::duration<double>(chrono::steady_clock::now()-start).count()<<"s\n";
}

#endif
