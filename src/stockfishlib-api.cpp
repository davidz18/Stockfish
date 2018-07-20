#include "bitboard.h"
#include "position.h"
#include "search.h"
#include "thread.h"
#include "tt.h"
#include "uci.h"
#include "syzygy/tbprobe.h"
#include "timeman.h"
#include "search.h"

#include "stockfishlib-api.h"
#include "stockfishlib-api-internal.h"

namespace TB = Tablebases;

namespace PSQT {
	void init();
}

extern const char* StartFEN;

struct sfl_context {
	Position pos;
	StateListPtr states;	

	sfl_context() : states(new std::deque<StateInfo>(1))
	{		
		auto uiThread = std::make_shared<Thread>(0);
		pos.set(StartFEN, false, &states->back(), uiThread.get());
	}
};

static void *_init()
{
	PSQT::init();
	Bitboards::init();
	Position::init();
	Bitbases::init();
	Search::init();
	Pawns::init();
	Tablebases::init(Options["SyzygyPath"]); // After Bitboards are set
	Threads.set(Options["Threads"]);
	Options["_ConsoleQuiet"] = true;
	Search::clear(); // After threads are up

	return new sfl_context();
}

SFL_EXPORT void *sfl_analysis_init(const unsigned int threads, const unsigned int hashSize, const int contempt, const unsigned int multiPV)
{
	UCI::init(Options);		
	sfl_setoption("Hash", std::to_string(hashSize));
	if (contempt)
	{
		sfl_setoption("Analysis Contempt", string("Both"));
		sfl_setoption("Contempt", std::to_string(contempt));
	}
	else
	{
		sfl_setoption("Analysis Contempt", string("Off"));
		//sfl_setoption("Contempt", string("0"));
	}
	sfl_setoption("UCI_AnalyseMode", string("true"));
	sfl_setoption("MultiPV", std::to_string(multiPV));
	
	auto ret = _init();
	sfl_setoption("Threads", std::to_string(threads));
	return ret;
}

SFL_EXPORT void sfl_reset() { Search::clear(); }

SFL_EXPORT void sfl_exit(void *ctxt)
{
	Threads.set(0);
	delete (sfl_context *)ctxt;
}

SFL_EXPORT void sfl_setoption(string name, string value)
{
	Options[name] = value;
}

SFL_EXPORT void sfl_analyse_depth(void *ctxt, const string fen, const int depth, sfl_result &res)
{
	analyse<sfl_analysis_type::DEPTH>(ctxt, fen, depth, res);
}

SFL_EXPORT void sfl_analyse_nodes(void *ctxt, const string fen, const int nodes, sfl_result &res)
{
	analyse<sfl_analysis_type::NODES>(ctxt, fen, nodes, res);
}

SFL_EXPORT void sfl_analyse_msec(void *ctxt, const string fen, const int millis, sfl_result &res)
{
	analyse<sfl_analysis_type::TIME>(ctxt, fen, millis, res);
}

SFL_EXPORT string sfl_promotion_str(const sfl_promotion p)
{
	switch (p)
	{
	case sfl_promotion::NONE:
		return string("");
	case sfl_promotion::QUEEN:
		return string("Q");
	case sfl_promotion::ROOK:
		return string("R");
	case sfl_promotion::KNIGHT:
		return string("N");
	default:
		return string("B");
	}
}

SFL_EXPORT string sfl_pv::moves_to_string()
{
	std::ostringstream oss;

	for (auto&& m : moves)
	{
		oss << UCI::square((Square)m.from) << UCI::square((Square)m.to);
		if (m.promotion != sfl_promotion::NONE)
		{
			oss << sfl_promotion_str(m.promotion);
		}
		oss << " ";
	}

	return oss.str();
}

// internals

template<sfl_analysis_type aType>
static void sfl_go(void *ctxt, const string fen, const int limit, sfl_result &res)
{	
	sfl_context *sflc = (sfl_context *)ctxt;
	Search::LimitsType limits;	
	bool ponderMode = false;

	sflc->states = StateListPtr(new std::deque<StateInfo>(1)); // Drop old and create a new one
	sflc->pos.set(fen, Options["UCI_Chess960"], &sflc->states->back(), Threads.main());	
	limits.startTime = now(); // As early as possible!

	switch (aType)
	{
	case sfl_analysis_type::DEPTH:
		limits.depth = limit;
		break;
	case sfl_analysis_type::TIME:
		limits.movetime = limit;
		break;
	case sfl_analysis_type::NODES:
		limits.nodes = limit;
		break;
	default:
		assert(0);
		break;
	}

	Threads.start_thinking(sflc->pos, sflc->states, limits, ponderMode);
}

template<sfl_analysis_type aType>
static void analyse(void *ctxt, const string fen, int limit, sfl_result &res)
{	
	sfl_go<aType>(ctxt, fen, limit, res);
	Threads.main()->wait_for_search_finished();
	search_result(((sfl_context *)ctxt)->pos, res);
}

static int position_score(const Value& v)
{	
	assert(-VALUE_INFINITE < v && v < VALUE_INFINITE);
	if (abs(v) < VALUE_MATE - MAX_PLY)
		return v * 100 / PawnValueEg;
	return (v > 0 ? VALUE_MATE - v + 1 : -VALUE_MATE - v) / 2;
}

static sfl_move get_move(const Move &m, const bool is960)
{
	Square from = from_sq(m);
	Square to = to_sq(m);
	sfl_promotion promotion = sfl_promotion::NONE;

	if (m == MOVE_NONE || m == MOVE_NULL)
	{		
		return sfl_move(SQ_NONE, SQ_NONE);
	}

	if (type_of(m) == CASTLING && !is960)
		to = make_square(to > from ? FILE_G : FILE_C, rank_of(from));
	
	if (type_of(m) == PROMOTION)
		promotion = (sfl_promotion)promotion_type(m);

	return sfl_move(from, to, promotion);
}

static void get_pv_moves(const std::vector<Move> &pv, const bool is960, sfl_line &moves)
{
	moves.clear();
	for (auto&& m : pv)
	{
		moves.emplace_back(get_move(m, is960));
	}
}

static void search_result(const Position &pos, sfl_result &res)
{	
	res.elapsed = Time.elapsed() + 1;
	const Search::RootMoves& rootMoves = pos.this_thread()->rootMoves;
	size_t pvIdx = pos.this_thread()->pvIdx;
	size_t multiPV = std::min((size_t)Options["MultiPV"], rootMoves.size());
	res.nodesSearched = Threads.nodes_searched();
	res.tbHits = Threads.tb_hits() + (TB::RootInTB ? rootMoves.size() : 0);
	char depth = Threads.depth;
	bool chess960 = pos.is_chess960();

	for (size_t i = 0; i < multiPV; ++i)
	{
		bool updated = (i <= pvIdx && rootMoves[i].score != -VALUE_INFINITE);

		if (depth == ONE_PLY && !updated)
			continue;

		Depth d = (Depth)(updated ? depth : depth - ONE_PLY);
		Value v = updated ? rootMoves[i].score : rootMoves[i].previousScore;

		bool tb = TB::RootInTB && abs(v) < VALUE_MATE - MAX_PLY;
		v = tb ? rootMoves[i].tbScore : v;

		int score = position_score(v);
					
		sfl_line moves;
		get_pv_moves(rootMoves[i].pv, chess960, moves);
		res.pvs.emplace_back(sfl_pv{ d / ONE_PLY, rootMoves[i].selDepth, score, moves });
	}
}
