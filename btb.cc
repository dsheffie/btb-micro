#include <cstdio>
#include <cstdint>
#include <ostream>
#include <fstream>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>
#include <assert.h>
#include <cstdlib>
#include <vector>
#include <map>
#include <string>

template <typename T> void swap(T &a, T &b) {
  T t = a;
  a = b;
  b = t;
}

template<typename T> void shuffle(T &arr) {
  for(auto i=0,n=arr.size();i<n;i++) {
    auto j = i + rand() % (n-i);
    swap(arr[j], arr[i]);
  }
}

static inline uint64_t rdtsc() {
  uint32_t hi=0, lo=0;
  __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
  return ( (uint64_t)lo)|( ((uint64_t)hi)<<32 );
}

uint8_t *gen_code(int len, size_t &sz) {
  using namespace std;
  uint8_t *rawb = NULL;
  size_t pgsz = getpagesize();
  vector<int> targets(len);
  map<int,int> next, labels;
  int pos = 0;
  for(int i = 0; i < len; i++) {
    targets.at(i) = i;
  }
  shuffle(targets);
  for(int i = 0; i < len; i++) {
    next[targets.at(i)] = targets.at((i+1)%len);
  }
  sz  = (len-1)*5 + 17;
  sz = ((sz + pgsz - 1) / pgsz) * pgsz;
  //(len-1)*5 bytes for jump with 32b disp
  //decq %rdi -> 3
  //jne with 32b disp -> 6
  //mov immediate -> 7
  //retq -> 1 
  rawb = (uint8_t*)(mmap(NULL,
			 sz,
			 PROT_READ|PROT_WRITE|PROT_EXEC,
			 MAP_ANON|MAP_PRIVATE,
			 -1,
			 0));
  assert((uint8_t*)(-1) != rawb);

  //rawb[pos++] = 0xcc;

  for(int i = 0; i < len; i++) {
    /* emit code for label i */
    labels[i] = pos;
    if(i != (len-1)) {
      rawb[pos++] = 0xe9;      
      pos += 4;
    }
    else {
      //48 ff cf             	dec    %rdi
      rawb[pos++] = 0x48;
      rawb[pos++] = 0xff;
      rawb[pos++] = 0xcf;
      //0f 85 5a 66 ff ff    	jne    598 <goto_test8192+0x598>
      rawb[pos++] = 0x0f;
      rawb[pos++] = 0x85;
      pos += 4;
      //48 c7 c0 00 20 00 00 	mov    $0x2000,%rax
      rawb[pos++] = 0x48;
      rawb[pos++] = 0xc7;
      rawb[pos++] = 0xc0;
      uint8_t *ll = reinterpret_cast<uint8_t*>(&len);
      rawb[pos++] = ll[0];
      rawb[pos++] = ll[1];
      rawb[pos++] = ll[2];
      rawb[pos++] = ll[3];
      //c3                   	retq
      rawb[pos++] = 0xc3;
    }    
  }
  assert(pos < sz);
  
  for(int i = 0; i < len; i++) {
    if(i != (len-1)) {
      /* label i */
      int loc = labels.at(i);
      int targ = labels.at(next.at(i));
      int delta = targ - (loc + 5);
      loc += 1;
      uint8_t *d = reinterpret_cast<uint8_t*>(&delta);
      rawb[loc++] = d[0];
      rawb[loc++] = d[1];
      rawb[loc++] = d[2];
      rawb[loc++] = d[3];
    }
    else {
      /* skip decq */
      int loc = labels.at(i) + 3;
      int targ = labels.at(next.at(i));
      int delta = targ - (loc + 6);
      loc += 2;
      uint8_t *d = reinterpret_cast<uint8_t*>(&delta);
      rawb[loc++] = d[0];
      rawb[loc++] = d[1];
      rawb[loc++] = d[2];
      rawb[loc++] = d[3];
    }
  }
  return rawb;
}

typedef int (*test_t)(int);

int main() {
  char hostname[256] = {0};
  const int64_t n_iters = 1<<20;
  srand(2);
  gethostname(hostname,sizeof(hostname));
  const std::string out_name = std::string(hostname) + std::string(".csv");
  std::ofstream out(out_name.c_str());
  
  for(size_t i = 2; i < 16; ++i) {
    size_t code_sz = 0;
    test_t t4 = reinterpret_cast<test_t>(gen_code(1<<i, code_sz));
    uint64_t now = rdtsc();
    int sz = t4(n_iters);
    now = rdtsc() - now;
    int64_t jumps = n_iters * sz;
    double cycles_per_j =  ((double)now) / jumps;
    printf("sz = %d, cycles per jump = %g\n", sz, cycles_per_j);
    out << sz << "," << cycles_per_j << "\n";
    munmap(reinterpret_cast<void*>(t4), code_sz);
  }
  out.close();
  return 0;
}
