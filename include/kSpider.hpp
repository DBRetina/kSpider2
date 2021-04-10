#include <iostream>
#include <cstdint>
#include "argh.h"
#include <chrono>
#include "colored_kDataFrame.hpp"
#include "parallel_hashmap/phmap.h"


namespace kSpider{

void pairwise(string index_prefix);
void items_indexing(string items_file, string names_file, string index_prefix);

};