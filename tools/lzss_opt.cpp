// Exact LZSS2 bitstream encoder with an optimal (minimum encoded byte count)
// parse over all reference lengths exposed by the longest available match.
// Fixed 4096-byte window, maximum match 18, no external dependencies.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace std;
vector<uint8_t> readFile(const string &f){ifstream s(f,ios::binary);if(!s)throw runtime_error("open input");return vector<uint8_t>((istreambuf_iterator<char>(s)),{});}
void writeFile(const string &f,const vector<uint8_t>&b){ofstream s(f,ios::binary);if(!s)throw runtime_error("open output");s.write((char*)b.data(),b.size());if(!s)throw runtime_error("write output");}
int main(int argc,char**argv){try{
 if(argc!=3)throw runtime_error("usage: lzss_opt INPUT_APPLICATION OUTPUT_COMPRESSED");
 auto data=readFile(argv[1]); const int n=(int)data.size();if(n==0||n>32000000)throw runtime_error("invalid source size");
 vector<int32_t> prev(n,-1);vector<int32_t> head(1<<18,-1);
 vector<uint8_t> lengths(n,0);vector<uint16_t> ptr(n,0);
 auto hash=[&](int i){uint32_t v=((uint32_t)data[i]<<16)|((uint32_t)data[i+1]<<8)|data[i+2];v^=v>>9;v*=0x9e3779b1u;return (int)((v>>14)&((1<<18)-1));};
 for(int i=0;i<n-2;i++){
  int h=hash(i);int j=head[h];int checked=0;int limit=min(18,n-i);int best=0,bj=-1;
  while(j>=0 && i-j<=4096 && checked<512){
   int k=0;while(k<limit && data[j+k]==data[i+k])k++;
   if(k>best){best=k;bj=j;if(k==18)break;}
   j=prev[j];checked++;
  }
  if(best>=3){lengths[i]=(uint8_t)best;ptr[i]=(uint16_t)((0xFEE + bj)&0xFFF);}
  prev[i]=head[h];head[h]=i;
 }
 // dp[i][mod] accounts for each flag byte at the start of an eight-token group.
 // Cost units are bytes, exact. dp has n+1 entries of 8 states.
 vector<array<int32_t,8>> dp((size_t)n+1);vector<array<uint8_t,8>> choice(n);
 for(int i=n-1;i>=0;i--){
  for(int mod=0;mod<8;mod++){
   int next=(mod+1)&7;
   int bc=1+(mod==0)+dp[i+1][next];uint8_t choose=1;
   int maxl=lengths[i];
   for(int l=3;l<=maxl;l++){
    int c=2+(mod==0)+dp[i+l][next];
    if(c<bc){bc=c;choose=(uint8_t)l;}
   }
   dp[i][mod]=bc;choice[i][mod]=choose;
  }
 }
 vector<uint8_t> out;out.reserve((size_t)dp[0][0]);int i=0;
 while(i<n){
  size_t flagAt=out.size();out.push_back(0);uint8_t flags=0;vector<uint8_t> payload;payload.reserve(16);
  for(int k=0;k<8 && i<n;k++){
   int len=choice[i][k];
   if(len==1){flags|=uint8_t(1<<k);payload.push_back(data[i]);i++;}
   else{int p=ptr[i];payload.push_back(uint8_t(p&255));payload.push_back(uint8_t(((p>>4)&0xF0)|(len-3)));i+=len;}
  }
  out[flagAt]=flags;out.insert(out.end(),payload.begin(),payload.end());
 }
 writeFile(argv[2],out);
 fprintf(stderr,"application=%d compressed=%zu dynamic-program=%d\n",n,out.size(),dp[0][0]);
 return 0;
 }catch(const exception&e){fprintf(stderr,"ERROR %s\n",e.what());return 1;}}
