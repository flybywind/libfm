// Copyright (C) 2017 yixia.com
// create time: "2017/05/11 11:21"
// ./hash_category 把category 特征hash成uniq id，同时输出映射表

#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <iterator>
#include <algorithm>
#include <climits>
#include <iomanip>
#include "../../util/util.h"
#include "../../util/cmdline.h"

/**
 * 
 * Version history:
 * 1.0.0:
 *	first version
 */
 
using namespace std;

uint str_hash(const std::string& x, int seed) {
    uint hash_value = uint(seed);
    const char* xc = x.c_str();
    int n = x.size();
    for (int i = 0; i < n; ++i) {
        hash_value += uint(xc[i]) + (hash_value << 6) + (hash_value << 16) - hash_value;
    }
    return std::numeric_limits<uint>().max() & hash_value;
}
int main(int argc, char **argv) { 
 	int seed = time(NULL);
	try {
		CMDLine cmdline(argc, argv);
		const std::string name_file = cmdline.registerParameter("i", "input category name file, one name per line");
		const std::string index_file = cmdline.registerParameter("o", "output index file");
		const std::string orig_index_file = cmdline.registerParameter("orig", "origin index file");
		const std::string param_help       = cmdline.registerParameter("help", "this screen");


		if (cmdline.hasParameter(param_help) || (argc == 1)) {
			cmdline.print_help();
			return 0;
		}
		cmdline.checkParameters();

		std::string in_name_file = cmdline.getValue(name_file);
		std::string out_index_file = cmdline.getValue(index_file);
		std::string opt_orig_index_file = cmdline.getValue(orig_index_file, "");
        std::map<uint, std::string> hash_map;
		uint num_rows = 0;
        {
            uint origNum = 0;
            // 读取之前的索引数据
            if (opt_orig_index_file.size() > 0) {
                std::ifstream fOrigData(out_index_file.c_str());
                if (! fOrigData.is_open()) {
                    throw "unable to open " + out_index_file;
                }
                while(!fOrigData.eof()) {
                    std::string line;
                    std::getline(fOrigData, line);
                    size_t split_at = line.find_first_of("\t");
                    std::string indx = line.substr(0, split_at);
                    std::string cname = line.substr(split_at + 1);
                    std::istringstream is(indx);
                    uint indx_value ;
                    is >> indx_value ;
                    if (!is) {
                        std::cerr << "convert " << indx << "to long failed" << std::endl;
                        exit(-1);
                    }
                    hash_map[indx_value] = cname;
                    origNum ++;
                }
                fOrigData.close();
                std::cout << "read origin index file finished, total records num is "<<origNum << std::endl;
            }
        }
		{
			std::ifstream fNameData(in_name_file.c_str());
			if (! fNameData.is_open()) {
				throw "unable to open " + in_name_file;
			}
			while (!fNameData.eof()) {
				std::string line;
				std::getline(fNameData, line);
                uint hash_value = str_hash(line, seed);
                while (hash_map.count(hash_value) > 0) {
                    hash_value += rand()%10000;
                }
                hash_map[hash_value] = line;
			} 
			fNameData.close();
		}

        {
            // write content of hash_map to output file
            std::ofstream of(out_index_file.c_str());
            for(map<uint, std::string>::iterator it = hash_map.begin();
                it != hash_map.end(); ++it) {
                of << it->first << "\t" << it->second <<"\n";
            }
            of.close();
        }
	} catch (std::string &e) {
		std::cerr << e << std::endl;
	}

}
