#pragma once

#if defined(_MSC_VER)
#if STOCKFISHLIB_EXPORTS
#define SFL_EXPORT __declspec(dllexport)
#else
#define SFL_EXPORT __declspec(dllimport)
#endif
#elif defined(__GNUC__)
#define SFL_EXPORT __attribute__((visibility("default")))
#else
	//  do nothing and hope for the best?
#define SFL_EXPORT
#pragma warning Unknown dynamic link import/export semantics!
#endif

#include <vector>
#include <string>
#include <utility>
#include <sstream>

using std::vector;
using std::string;
using std::pair;

enum class sfl_promotion { NONE, KNIGHT, BISHOP, ROOK, QUEEN };
enum class sfl_analysis_type { DEPTH, NODES, TIME };

struct sfl_move
{
	unsigned char from;
	unsigned char to;
	sfl_promotion promotion;

	sfl_move(unsigned char f, unsigned char t, sfl_promotion p = sfl_promotion::NONE) : from(f), to(t), promotion(p) {}
};

typedef vector<sfl_move> sfl_line;

struct sfl_pv
{
	int depth;
	int selectiveDepth;
	int score;
	sfl_line moves;
	
	SFL_EXPORT string moves_to_string();	
};

struct sfl_result 
{
	long long elapsed;
	uint64_t nodesSearched;
	
	uint64_t tbHits;

	vector<sfl_pv> pvs;

	inline uint32_t nps() { return nodesSearched * 1000 / elapsed; }
};

SFL_EXPORT void* sfl_analysis_init(const unsigned int threads, const unsigned int hashSize, const int contempt = 0, const unsigned int multiPV = 1);

SFL_EXPORT void sfl_reset(); // equiv. to ucinewgame command
SFL_EXPORT void sfl_exit(void *ctxt);

SFL_EXPORT void sfl_setoption(const std::string name, const std::string value);

SFL_EXPORT void sfl_analyse_depth(void *ctxt, const string fen, const int depth, sfl_result &res);
SFL_EXPORT void sfl_analyse_nodes(void *ctxt, const string fen, const int nodes, sfl_result &res);
SFL_EXPORT void sfl_analyse_msec(void *ctxt, const string fen, const int millis, sfl_result &res);


SFL_EXPORT string sfl_promotion_str(const sfl_promotion p);