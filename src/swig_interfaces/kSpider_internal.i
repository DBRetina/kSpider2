%module kSpider_internal

%{
#include "kSpider.hpp"
%}

using namespace std;
%include std_string.i

namespace kSpider{
    void pairwise(string index_prefix);
    void items_indexing(string items_file, string names_file, string index_prefix);
};