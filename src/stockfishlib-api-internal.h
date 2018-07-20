#pragma once
template<sfl_analysis_type aType>
static void analyse(void *ctxt, const string fen, int limit, sfl_result &res);
template<sfl_analysis_type aType>
static void sfl_go(void *ctxt, const string fen, const int limit, sfl_result &res);
