// Historical v1 reproduction. Use v2/ for the current submission.
// In-process benchmark: compiles the actual submission implementation.
#define main hollan_protocol_main
#include "v2/baseline.cpp"
#undef main
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>

struct Config { string name; long long a, b; vector<double> p; };
struct Stats {
    double sum=0, sum2=0, diff=0, diff2=0, wins=0, ties=0;
    int n=0;
    void add(double x, double baseline) {
        ++n; sum+=x; sum2+=x*x; double d=x-baseline; diff+=d; diff2+=d*d;
        wins+= x>.5 ? 1 : 0; ties+=x==.5 ? 1 : 0;
    }
};

long long opponent_bet(int model, double p, long long x, long long y, int t, hollan::Planner& planner) {
    if(model<10)return hollan::model_bet(model,p,x,y);
    if(model==10)return hollan::model_bet(x*3<x+y?1:2,p,x,y);
    if(model==11)return hollan::model_bet((t/12)%2?4:1,p,x,y);
    if(model==12) {
        uint64_t z=static_cast<uint64_t>(x)*91872341+static_cast<uint64_t>(y)*1298417+static_cast<uint64_t>(t)*19783301;
        z^=z>>30; z*=0xbf58476d1ce4e5b9ULL; z^=z>>27;z*=0x94d049bb133111ebULL;z^=z>>31;
        return (x/5)*(z%10001)/10000;
    }
    if(model==13)return p>.54?x/5:0;
    if(model==14)return max(0LL,min(x/5,(long long)((2*p-1)*x*.6)));
    // A finite-horizon best response to official Kelly, stronger than the samples.
    const double* next=planner.value[0].data()+(static_cast<size_t>(t)+1)*planner.grid.size();
    long long b=hollan::model_bet(0,1-p,y,x), bestbet=0, limit=x/5;
    double best=-1;
    auto consider=[&](long long a){a=max(0LL,min(limit,a));double v=planner.action_value(next,x,y,a,b,p);if(v>best+1e-14){best=v;bestbet=a;}};
    consider(0);int count=(int)min(64LL,limit);
    for(int k=1;k<=count;++k)consider(limit*k/count);
    consider(y-b);consider(hollan::model_bet(7,p,x,y));
    long long center=bestbet,radius=max(1LL,limit/64);
    for(long long a=max(0LL,center-radius);a<=min(limit,center+radius);++a)consider(a);
    return bestbet;
}

long long play(Config const& cfg, int role, int strategy, int opp,
               vector<double> const& draws, hollan::Planner& planner, hollan::Planner& oppplanner) {
    State s; s.role=role==0?'A':'B'; s.probs=cfg.p;
    s.my_capital=role==0?cfg.a:cfg.b; s.opp_capital=role==0?cfg.b:cfg.a;
    planner.previous_round=-1; planner.loss.assign(hollan::MODEL_COUNT,0);
    planner.loss[0]=-.4; planner.active=0;
    for (int t=0;t<(int)cfg.p.size() && s.my_capital && s.opp_capital;++t) {
        s.round=t;
        double p=role==0?cfg.p[t]:1-cfg.p[t];
        long long a;
        if (strategy==10 || strategy==11 || strategy==13) {
            if(strategy==11 || strategy==13) {planner.active=strategy==13?0:opp; planner.previous_round=-1;}
            a=planner.choose(s);
        } else if(strategy==12) { // Finite-horizon opponent-aware Kelly heuristic
            double edge=2*p-1;
            double forced=hollan::model_bet(0,1-p,s.opp_capital,s.my_capital);
            double risk=1 + 4.0 / sqrt(1.0+cfg.p.size()-t);
            double target=min((double)s.opp_capital,edge*s.my_capital*risk)-forced;
            a=max(0LL,min(s.my_capital/5,(long long)target));
        } else a=hollan::model_bet(strategy,p,s.my_capital,s.opp_capital);
        long long b=opponent_bet(opp,1-p,s.opp_capital,s.my_capital,t,oppplanner);
        if(a<0 || a>s.my_capital/5) {cerr<<"ILLEGAL";exit(2);}
        bool win=role==0 ? draws[t]<=cfg.p[t] : draws[t]>cfg.p[t];
        long long transfer=min(a+b,win?s.opp_capital:s.my_capital);
        s.my_capital+=win?transfer:-transfer; s.opp_capital+=win?-transfer:transfer;
    }
    return s.my_capital;
}

int main(int argc,char**argv) {
    int games=argc>1?stoi(argv[1]):300;
    int scenarios=argc>2?stoi(argv[2]):8;
    int resolution=argc>3?stoi(argv[3]):800;
    int actions=argc>4?stoi(argv[4]):24;
    unsigned seed=argc>5?stoul(argv[5]):20261012;
    vector<Config> configs;
    for(string name:{"sample","typical_100_rounds","competition_2000"}) {
        ifstream file("params/"+name+".txt"); string line,all;
        while(getline(file,line))all+=line.substr(0,line.find('#'))+" ";
        istringstream in(all); Config c;c.name=name;in>>c.a>>c.b;
        double p;while(in>>p)c.p.push_back(p); configs.push_back(c);
    }
    mt19937_64 random(seed);
    for(int i=0;i<scenarios;++i) {
        Config c; c.name="random"+to_string(i);
        c.a=1000+random()%3001;c.b=4000+random()%4001;
        int n=vector<int>{10,30,100,300,2000,100,50,500}[i%8];
        for(int t=0;t<n;++t){double u=generate_canonical<double,53>(random);double p;
            switch(i%6) {
                case 1: p=.3+.3*u;break;
                case 2: p=.1+.8*u;break;
                case 3: p=.45+.1*u;break;
                case 4: p=(t<n/2?.3:.5)+.2*u;break;
                default:p=.4+.3*u;
            }
            c.p.push_back(round(p*100000)/100000);
        } configs.push_back(c);
    }
    bool challenge=argc>6 && stoi(argv[6]);
    vector<int> strategies=challenge?vector<int>{0,7,4,12,10,13}:vector<int>{0,7,4,12,10,11};
    vector<int> opponents=challenge?vector<int>{10,11,12,13,14,15}:vector<int>{0,1,4,7,5,6,3};
    vector<vector<Stats>> total(16,vector<Stats>(20));
    auto started=chrono::steady_clock::now();
    for(auto const&c:configs) {
        hollan::Planner planners[2];
        for(int role=0;role<2;++role) {
            State s;s.role=role==0?'A':'B';s.probs=c.p;s.my_capital=c.a;s.opp_capital=c.b;
            planners[role].cache_limit=1024u*1024u*1024u;planners[role].resolution=resolution;planners[role].actions=actions;planners[role].reset(s);
            // Keep all models for simulation. The production cap may evict
            // tables only at larger grid/horizon settings.
            if(challenge)for(int m=0;m<hollan::MODEL_COUNT;++m)planners[role].prepare(m);
            else for(int m:opponents)planners[role].prepare(m);
        }
        vector<vector<Stats>> local(16,vector<Stats>(20));
        for(int trial=0;trial<games;++trial) {
            vector<double> draws[2];for(auto& d:draws)for(size_t t=0;t<c.p.size();++t)d.push_back(generate_canonical<double,53>(random));
            for(int opp:opponents) {
                double baseline=0;
                for(int strategy:strategies) {
                    double score=0;
                    for(int role=0;role<2;++role)score+=play(c,role,strategy,opp,draws[role],planners[role],planners[1-role]);
                    score/=2*(c.a+c.b);
                    if(strategy==0)baseline=score;
                    local[strategy][opp].add(score,baseline);total[strategy][opp].add(score,baseline);
                }
            }
        }
        cout<<"CONFIG "<<c.name<<" rounds="<<c.p.size()<<" capital="<<c.a<<"/"<<c.b<<"\n";
        for(int opp:opponents) {
            cout<<"opp="<<opp;
            for(int strategy:strategies)cout<<" s"<<strategy<<"="<<fixed<<setprecision(4)<<local[strategy][opp].sum/games;
            cout<<"\n";
        }
        cout.flush();
    }
    cout<<"SUMMARY scenarios="<<configs.size()<<" games_per_scenario="<<games<<" seed="<<seed<<" grid="<<resolution<<" actions="<<actions<<"\n";
    for(int opp:opponents)for(int strategy:strategies) {
        auto s=total[strategy][opp];double mean=s.diff/s.n;
        double se=sqrt(max(0.,s.diff2/s.n-mean*mean)/(s.n-1));
        cout<<"opp="<<opp<<" strategy="<<strategy<<" n="<<s.n<<" score="<<s.sum/s.n<<" delta="<<mean<<" ci95="<<1.96*se<<" wins="<<s.wins/s.n<<" ties="<<s.ties/s.n<<"\n";
    }
    cerr<<"elapsed="<<chrono::duration<double>(chrono::steady_clock::now()-started).count()<<"s\n";
}
