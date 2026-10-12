// Both learners observe the SAME financial states and inferred previous bets.
#define ARENA_LIBRARY
#include "arena.cpp"
int main() {
    mt19937_64 rng(31415926);vector<int> opponents={11,12,13,15,16,18,19,20,22,23};
    map<int,Stat> stats;map<int,size_t> observations;
    for(auto c:configs(31415926,12)) {
        Arena arena(c);
        for(int k=0;k<1000;++k) {
            vector<double> draws[2];for(auto& d:draws)for(size_t t=0;t<c.p.size();++t)d.push_back(generate_canonical<double,53>(rng));
            for(int opp:opponents) {
                double trial_old=0,trial_new=0;int eligible_roles=0;
                for(int role=0;role<2;++role) {
                    arena.resetgame(role,4);arena.resetgame(1-role,4);
                    State s;s.role=role?'B':'A';s.probs=c.p;s.my_capital=role?c.b:c.a;s.opp_capital=role?c.a:c.b;
                    old::State o;o.role=s.role;o.probs=c.p;double old_error=0,new_error=0;int count=0;
                    for(int t=0;t<int(c.p.size()) && s.my_capital && s.opp_capital;++t) {
                        s.round=t;o.round=t;o.my_capital=s.my_capital;o.opp_capital=s.opp_capital;
                        arena.old_choose(o,role);arena.new_choose(s,role);
                        double p=role?1-c.p[size_t(t)]:c.p[size_t(t)];
                        long long b=arena.opponent(opp,1-role,t,s.opp_capital,s.my_capital);
                        if(t>=3 && s.opp_capital>=5) {
                            int m=arena.cand[role].active;
                            long long n=arena.cand[role].predict(m,size_t(t),s.opp_capital,1-p,true);
                            long long v=old::hollan::model_bet(arena.base[role].active,1-p,s.opp_capital,s.my_capital);
                            double scale=double(s.opp_capital/5);
                            old_error+=abs(double(v-b))/scale;new_error+=abs(double(n-b))/scale;++count;++observations[opp];
                        }
                        // Exogenous observer policy; override the learners' previous
                        // own stake so their inference sees identical legal history.
                        long long a=old::hollan::model_bet(0,p,s.my_capital,s.opp_capital);
                        arena.base[role].previous_bet=a;arena.cand[role].previous_bet=a;
                        bool win=role?draws[role][size_t(t)]>c.p[size_t(t)]:draws[role][size_t(t)]<=c.p[size_t(t)];
                        long long transfer=min(a+b,win?s.opp_capital:s.my_capital);
                        s.my_capital+=win?transfer:-transfer;s.opp_capital+=win?-transfer:transfer;
                    }
                    if(count){trial_old+=old_error/count;trial_new+=new_error/count;++eligible_roles;}
                }
                if(eligible_roles)stats[opp].add(trial_new/eligible_roles,trial_old/eligible_roles);
            }
        }
        cout<<"CONFIG "<<c.name<<endl;
    }
    cout<<fixed<<setprecision(7);
    for(auto kv:stats) {auto s=kv.second;double d=s.diff/s.n;cout<<"PRED_PAIRED opp="<<kv.first<<" n="<<s.n<<" states="<<observations[kv.first]<<" old_mae="<<(s.sum-s.diff)/s.n<<" new_mae="<<s.sum/s.n<<" delta="<<d<<" ci95="<<1.96*sqrt(max(0.,s.diff2/s.n-d*d)/(s.n-1))<<endl;}
}
