#include "virtualQs.hpp"
#include <iostream>
#include <fstream>
#include "combinations.hpp"
#include <boost/algorithm/string/join.hpp>
#include <boost/range/adaptor/transformed.hpp>
// #include <sqlite3.h>
using boost::adaptors::transformed;
using boost::algorithm::join;
using std::cerr;
using std::endl;
using std::to_string;


inline uint64_t create_mask(unsigned kSize, unsigned Q) {
    return ((1ULL << Q * 2ULL) - 1ULL) << (kSize * 2ULL - Q * 2ULL);
}

inline bool isDisjoint(set<uint32_t> &set1, set<uint32_t> &set2) {
    set<uint32_t>::iterator i1, i2;
    i1 = set1.begin();
    i2 = set2.begin();            //initialize iterators with first element
    while (i1 != set1.end() && i2 != set2.end()) {         //when both set have some elements to check
        if (*i1 < *i2) i1++;                   //when item of first set is less than second set
        else if (*i2 < *i1) i2++;               //when item of second set is less than first set
        else return false;            //if items are matched, sets are not disjoint
    }
    return true;
}

void get_common_pairs(vector<pair<uint32_t, uint32_t>> &result, vector<set<uint32_t>> &values) {
    int size = values.size();
    for (int i = 0; i < size; i++)
        for (int j = i + 1; j < size; j++)
            if (isDisjoint(values[i], values[j])) result.emplace_back(i, j);

}

virtualQs::virtualQs(string index_prefix, set<int> allQs) {

    // Load the sqlite DB
    // TODO assert file exist before trying to load.

    // string sqlite_file = index_prefix + "_kCluster.sqlite";
    // int rc;
    // rc = sqlite3_open(sqlite_file.c_str(), &this->DB);
    // if(rc) {
    //     fprintf(stderr, "Can't open ths sqlite database: %s\n", sqlite3_errmsg(this->DB));
    //     exit(0);
    // }

    // Loading the index
    this->KF = kDataFrame::load(index_prefix);
    this->kSize = KF->ksize();
    this->index_prefix = index_prefix;

    // Constructing masks
    for (auto const &Q : allQs)
        this->mainQs.insert(Q);

//    bool ksize_in_Qs = (this->masks.find(this->kSize) != this->masks.end());
//
//    if (!ksize_in_Qs)
//        this->mainQs.insert(this->kSize);

    for (auto const &Q : this->mainQs) {
        this->masks[Q] = create_mask(this->kSize, Q);
    }

    this->superColors = flat_hash_map<uint64_t, flat_hash_set<uint64_t>>();
    this->superColorsCount = flat_hash_map<uint64_t, uint32_t>();
    this->temp_superColors = flat_hash_set<uint64_t>();

    string colors_map = this->index_prefix + "colors.intvectors";
    ifstream input(colors_map.c_str());
    int size;
    input >> size;
    color_to_ids = flat_hash_map<uint32_t, std::vector<uint32_t>>(size);
    for (int i = 0; i < size; i++) {
        uint32_t color, colorSize;
        input >> color >> colorSize;
        uint32_t sampleID;
        color_to_ids[color] = std::vector<uint32_t>(colorSize);
        for (uint32_t j = 0; j < colorSize; j++) {
            input >> sampleID;
            color_to_ids[color][j] = sampleID;
        }
    }


//     Read NamesMap
    ifstream namesMapIn(index_prefix + ".namesMap");
    namesMapIn >> this->no_seqs;
    cerr << "Processing " <<  this->no_seqs << " seqs" << endl;

    // Preallocation of the kmers
    this->edges2 = vector<flat_hash_map<uint32_t , uint16_t >>(this->no_seqs);

//    for (uint32_t i = 0; i <  this->no_seqs; i++) {
//        uint32_t sample_id;
//        string sample_name;
//        namesMapIn >> sample_id >> sample_name;
//    }

    this->calculate_kmers_number();
    string filename = index_prefix + "_kCluster.tsv";
    std::remove(filename.c_str());

    myfile.open(filename, std::ios::out | std::ios::app);
    if (myfile.fail())
        throw std::ios_base::failure(std::strerror(errno));

    myfile.exceptions(myfile.exceptions() | std::ios::failbit | std::ifstream::badbit);
    myfile << "ID" << '\t' << "seq1" << '\t' << "seq2" << '\t' << "min_kmers" << '\t';
    myfile << "Q" << '\n';


}

uint64_t virtualQs::create_super_color(flat_hash_set<uint64_t> &colors) {
    uint64_t seed = colors.size();
    for (auto &color : colors)
        seed ^= color + 0x9e3779b9 + (seed << 6) + (seed >> 2);

    return seed;
}

void virtualQs::calculate_kmers_number() {

    cerr << "counting kmers.." << endl;

    // Count Colors
    flat_hash_map<uint64_t, uint64_t> colors_count;
    auto it = this->KF->begin();
    while (it != this->KF->end()) {
        colors_count[it.getKmerCount()]++;
        it++;
    }

    for (auto const &colorIDs : color_to_ids)
        for (auto const &trID : colorIDs.second)
            seq_to_kmers_no[trID] += colors_count[colorIDs.first];

}

void virtualQs::pairwise() {

    cerr << "Processing pairwise for Q: " << this->curr_Q << std::endl;

    for (auto const &superColor : this->superColors) {
        vector<set<uint32_t>> tr_ids;
        vector<pair<uint32_t, uint32_t>> disjointed;

        int idx_tr_ids = 0;

        for (auto const &color : superColor.second) {
            tr_ids.emplace_back(set<uint32_t>());
            for (auto const &id : this->color_to_ids[color]) {
                tr_ids[idx_tr_ids].insert(id);
            }
            ++idx_tr_ids;
        }

        // Get pairs of sets indeces to process
        get_common_pairs(disjointed, tr_ids);

        vector<pair<uint32_t, uint32_t >> tr_ids_pairs;

        for (auto const &disjoint_pair : disjointed) {
            for (auto const &seq1 : tr_ids[disjoint_pair.first]) {
                for (auto const &seq2 : tr_ids[disjoint_pair.second]) {
                    tr_ids_pairs.emplace_back(seq1, seq2);
                }
            }
        }

        uint32_t color_count = this->superColorsCount[superColor.first];
        uint32_t _seq1, _seq2;
        for (auto const &seq_pair : tr_ids_pairs) {
            _seq1 = seq_pair.first;
            _seq2 = seq_pair.second;
            _seq1 < _seq2 ? this->edges2[_seq1][_seq2] += color_count : this->edges2[_seq2][_seq1] += color_count;
        }
    }
}


inline string prepare_insertion(string &Q_names, vector<uint32_t> &values) {
    std::string VALUES = join(values | transformed([](uint32_t d) { return std::to_string(d); }), ",");
    return "INSERT INTO virtualQs (seq1, seq2, min_kmers, " + Q_names + ") VALUES (" + VALUES + ");";
}


void virtualQs::export_to_tsv() {
    cerr << "Exporting Q: " << this->curr_Q << std::endl;

    for(uint64_t seq1 = 0; seq1 < no_seqs; seq1++){
        for(auto const &seq2 : edges2[seq1]){
            uint32_t min_kmers = std::min(this->seq_to_kmers_no[seq1], this->seq_to_kmers_no[seq2.first]);
            myfile << seq1 << '\t' << seq2.first << '\t' << min_kmers << '\t' << seq2.second << '\t' << curr_Q << '\n';
        }
    }
}

/*
void virtualQs::export_to_sqlite() {
    this->superColors.clear();
    this->superColorsCount.clear();
    vector<uint32_t > values;
    int rc;
    values.reserve(4);
    sqlite3_exec(this->DB, "PRAGMA cache_size=10000000", NULL, NULL, &this->DB_ErrMsg);
    sqlite3_exec(this->DB, "BEGIN TRANSACTION", NULL, NULL, &this->DB_ErrMsg);
    sqlite3_exec(this->DB, "PRAGMA synchronize = OFF", NULL, NULL, &this->DB_ErrMsg);
    sqlite3_exec(this->DB, "PRAGMA jorunal_mode = MEMORY", NULL, NULL, &this->DB_ErrMsg);
    std::string Q_names = "Q_" + join(this->mainQs | transformed([](uint8_t d) { return std::to_string(d); }), ", Q_");

    Combo combo = Combo();
    combo.combinations(this->seq_to_kmers_no.size());

    int total_elements = 3 + this->mainQs.size();

    for (auto const &seq_pair : combo.combs) {
        uint32_t seq1;
        uint32_t seq2;
        if(seq_pair.first > seq_pair.second){
            seq1 = seq_pair.first + 1;
            seq2 = seq_pair.second + 1;
        }else{
            seq2 = seq_pair.first + 1;
            seq1 = seq_pair.second + 1;
        }

        uint32_t min_kmers = std::min(this->seq_to_kmers_no[seq1], this->seq_to_kmers_no[seq2]);

        vector<uint32_t> values = {seq1, seq2, min_kmers};
        uint32_t sum = 0;
        for(auto const & Q : this->mainQs){
            uint32_t val = this->edges[{{seq1, seq2},Q}];
            sum += val;
            values.emplace_back(val);
        }

        if(sum){
//            cout << "inserting: " << prepare_insertion(Q_names, values) << endl;
            rc = sqlite3_exec(this->DB, prepare_insertion(Q_names, values).c_str(), NULL, 0, &this->DB_ErrMsg);
            if( rc != SQLITE_OK ){
                fprintf(stderr, "SQL error: %s\n", this->DB_ErrMsg);
                sqlite3_free(&this->DB_ErrMsg);
            }
        }

    }


    sqlite3_exec(this->DB, "END TRANSACTION", NULL, NULL, &this->DB_ErrMsg);
    sqlite3_close(this->DB);
}

*/