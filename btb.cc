#include <cstdio>
#include <cstdint>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>
#include <assert.h>
#include <cstdlib>
#include <vector>
#include <map>

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

uint8_t *gen_code(int len) {
  using namespace std;
  uint8_t *rawb = NULL;
  size_t pgsz = 4*getpagesize();
  vector<int> targets(len);
  vector<pair<int,int>> arr(len);
  map<int,int> labels;
  int pos = 0;
  for(int i = 0; i < len; i++) {
    targets.at(i) = i;
  }
  shuffle(targets);
  for(int i = 0; i < len; i++) {
    arr.at(i).first = targets.at(i);
    arr.at(i).second = targets.at((i+1)%len);
  }
  //(len-1)*5 bytes for jump with 32b disp
  //decq %rdi
  //jne with 32b disp
  //mov immediate
  //retq
  rawb = (uint8_t*)(mmap(NULL,
			 pgsz,
			 PROT_READ|PROT_WRITE|PROT_EXEC,
			 MAP_ANON|MAP_PRIVATE,
			 -1,
			 0));
  assert((uint8_t*)(-1) != rawb);

  rawb[pos++] = 0xcc;

  
  for(int i = 0; i < len; i++) {
    labels[arr.at(i).first] = pos;
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
  
  for(int i = 0; i < len; i++) {
    if(i != (len-1)) {
      int loc = labels.at(arr.at(i).first);
      int targ = labels.at(arr.at(i).second);
      int delta = targ - (loc + 5);
      printf("%d (pos %d) -> %d (pos %d)\n",
	     arr.at(i).first,
	     loc,
	     arr.at(i).second,
	     targ);
      loc += 1;
      uint8_t *d = reinterpret_cast<uint8_t*>(&delta);
      rawb[loc++] = d[0];
      rawb[loc++] = d[1];
      rawb[loc++] = d[2];
      rawb[loc++] = d[3];
    }
    else {
      /* skip decq */
      int loc = labels.at(arr.at(i).first) + 3;
      int targ = labels.at(arr.at(i).second);
      int delta = targ - (loc + 6);
      printf("%d (pos %d) -> %d (pos %d), disp = %d\n",
	     arr.at(i).first,
	     loc,
	     arr.at(i).second,
	     targ,
	     delta
	     );
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

extern "C" {
  int goto_test4(int);
  int goto_test8(int);
  int goto_test16(int);
  int goto_test32(int);
  int goto_test64(int);
  int goto_test128(int);
  int goto_test256(int);
  int goto_test512(int);
  int goto_test1024(int);
  int goto_test2048(int);
  int goto_test4096(int);
  int goto_test8192(int);
  int goto_test16384(int);
};

typedef int (*test_t)(int);
static test_t funcs[] = {goto_test4,
			 goto_test8,
			 goto_test16,
			 goto_test32,
			 goto_test64,
			 goto_test128,
			 goto_test256,
			 goto_test512,
			 goto_test1024,
			 goto_test2048,
			 goto_test4096,
			 goto_test8192,
			 goto_test16384
};

int main() {
  const size_t n_tests = sizeof(funcs)/sizeof(funcs[0]);
  const int64_t n_iters = 1<<20;
  srand(2);
  test_t t4 = reinterpret_cast<test_t>(gen_code(16));
  t4(2);
  return 0;
  
  for(size_t i = 0; i < n_tests; ++i) {
    uint64_t now = rdtsc();
    int sz = funcs[i](n_iters);
    now = rdtsc() - now;
    int64_t jumps = n_iters * sz;
    printf("now = %lu\n", now);
    //printf("now = %u, jumps = %d\n", now, n_iters*sz);
    
    double cycles_per_j =  ((double)now) / jumps;
    printf("sz = %d, cycles per jump = %g\n", sz, cycles_per_j);
  }
  return 0;
}
