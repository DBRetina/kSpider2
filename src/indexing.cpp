#include "kDataFrame.hpp"
#include "algorithms.hpp"

namespace kSpider{


    void items_indexing(string items_file, string names_file, string index_prefix){
        kDataFrame * kf = new kDataFramePHMAP(32);
        kmerDecoder * KD = new Items(items_file);
        auto *ckf = kProcessor::index(KD, names_file, kf);
        ckf->save(index_prefix);
    }


};