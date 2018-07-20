#include <iostream>

#include "../src/stockfishlib-api.h"

using std::cout;

int main(int argc, char *argv[])
{
	void *ctxt;
	sfl_result truth;
	ctxt = sfl_analysis_init(16, 1024);
	sfl_analyse_depth(ctxt, string("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"), 26, truth);

	unsigned int knps = truth.nps() / 1000;

	/*cout << "Nodes: " << truth.nodesSearched << std::endl;
	cout << "Elapsed: " << truth.elapsed << std::endl;*/
	cout << "D/SD: " << truth.pvs[0].depth << "/" << truth.pvs[0].selectiveDepth << " " << truth.nps()/1000 << "KN/s" << std::endl;
	
	cout << "Score: " << truth.pvs[0].score/100.0 << std::endl;	
	cout << "Moves: " << truth.pvs[0].moves_to_string() << std::endl;

	cout << std::flush;

	sfl_exit(ctxt);
	return 0;
}