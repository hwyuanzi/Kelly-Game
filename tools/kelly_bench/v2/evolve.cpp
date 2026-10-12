#define ARENA_LIBRARY
#include "arena.cpp"
#include <memory>
int main(int argc,char**argv) {
    unsigned seed=argc>1?unsigned(stoul(argv[1])):339841;
    int generations=argc>2?stoi(argv[2]):160;
    int target=argc>3?stoi(argv[3]):0;
    mt19937_64 rng(seed);normal_distribution<double> normal;
    auto cfgs=configs(seed,6);cfgs.erase(cfgs.begin()+2); // Separate train configs; exclude 2000-round supplied.
    vector<unique_ptr<Arena>> arenas;vector<vector<array<vector<double>,2>>> tapes;
    for(auto c:cfgs) {
        arenas.emplace_back(new Arena(c,400));
        vector<array<vector<double>,2>> draws;
        for(int k=0;k<96;++k) {array<vector<double>,2> d;for(auto&v:d)for(size_t t=0;t<c.p.size();++t)v.push_back(generate_canonical<double,53>(rng));draws.push_back(d);}tapes.push_back(draws);
    }
    genes.clear();genes.push_back({{0,1,0,0,0,1,0,0}});
    auto evaluate=[&](Gene g) {
        genes[0]=g;double score=0;int n=0;
        for(size_t i=0;i<arenas.size();++i)for(auto const& tape:tapes[i]) {
            vector<double> d[2]={tape[0],tape[1]};score+=arenas[i]->paired(target,100,d);++n;
        }return score/n;
    };
    vector<pair<double,Gene>> population;
    for(int i=0;i<24;++i) {
        Gene g;g.w={{i%2?.1:0, i%3*.5,0,0,0,1,0,0}};
        if(i>4)for(size_t j=0;j<8;++j)g.w[j]+=normal(rng)*(j==1?.5:.1);
        population.push_back({evaluate(g),g});
    }
    for(int gen=0;gen<generations;++gen) {
        sort(population.begin(),population.end(),[](auto const&a,auto const&b){return a.first<b.first;});
        Gene g=population[rng()%12].second;
        double scale=gen%3==0?.15:gen%3==1?.04:.01;
        for(size_t j=0;j<8;++j)if(rng()%2)g.w[j]+=normal(rng)*scale*(j==1 || j==5?3:1);
        population.back()={evaluate(g),g};
        if(gen%10==0)cerr<<"generation="<<gen<<" target_share="<<population[0].first<<endl;
    }
    sort(population.begin(),population.end(),[](auto const&a,auto const&b){return a.first<b.first;});
    vector<Gene> selected;
    for(auto item:population) {
        bool near=false;for(auto g:selected){double d=0;for(size_t j=0;j<8;++j)d+=(g.w[j]-item.second.w[j])*(g.w[j]-item.second.w[j]);if(d<.01)near=true;}
        if(near)continue;selected.push_back(item.second);
        cerr<<"selected oldscore="<<item.first<<endl;
        for(double w:item.second.w)cout<<setprecision(12)<<w<<" ";cout<<endl;
        if(selected.size()==4)break;
    }
}
