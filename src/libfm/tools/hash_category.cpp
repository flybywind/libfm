// Copyright (C) 2017 yixia.com
// create time: "2017/05/11 11:21"
// ./hash_category 把category 特征hash成uniq id，同时输出映射表和映射后的libfm样本

#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <climits>
#include <unordered_map>
#include <unordered_set>
#include "../../util/util.h"
#include "../../util/cmdline.h"
/**
 * 
 * Version history:
 * 1.0.0:
 *	first version
 */
 
using namespace std;

struct ConfigurEntry{
    uint64 id ;
    uint64 start_pos ;
    uint64 range ;
};

int main(int argc, char **argv) { 
 	uint64 seed = time(NULL);
	try {
		CMDLine cmdline(argc, argv);
		const std::string in_param   = cmdline.registerParameter("i", "input feature file");
		const std::string out_param  = cmdline.registerParameter("o", "output libfm file");
        const std::string conf_param = cmdline.registerParameter("c", "configure file, one-hot for `oh`, discrte for `dis`");
        const std::string idx_param  = cmdline.registerParameter("d", "index dir");
		const std::string param_help = cmdline.registerParameter("help", "this screen");


		if (cmdline.hasParameter(param_help) || (argc == 1)) {
			cmdline.print_help();
			return 0;
		}
		cmdline.checkParameters();

		std::string in_feature_file = cmdline.getValue(in_param);
		std::string out_libfm_file = cmdline.getValue(out_param);
		std::string configure = cmdline.getValue(conf_param);
        std::string idx_dir = cmdline.getValue(idx_param);

        std::map<uint64, std::unordered_map<std::string, uint64>* > one_hot_feature_hash_map;
        std::map<uint64, std::unordered_set<uint64>* > one_hot_feature_hash_set;
        std::map<uint64, std::vector<float> > discrete_feature_range_map;
        std::map<uint64, ConfigurEntry> configure_map; 
        {
            // 读取配置文件, 同时加载hash映射表信息
            std::ifstream fConfigure(configure.c_str());
            if (fConfigure.is_open()) {
                while(!fConfigure.eof()) {
                    std::string line;
                    std::getline(fConfigure, line);
                    std::vector<std::string> seg(3);
                    split_string(line, "\t", seg);
                    if (seg.size() < 3) {
                        std::cerr << "configure format wrong: colum number less than 3" << std::endl;
                        exit(-1);
                    }
                    uint64 fid = 0L;
                    if (str2long(seg[0], fid)) {
                        std::cerr << "configure format wrong: can't convert first column to uint64 feature id" << std::endl;
                        exit(-1);
                    }
                    if (seg[1] == "dis") {
                        std::vector<std::string> dis_vec;
                        split_string(seg[2], ",", dis_vec);
                        std::vector<float> dis_vec_float(dis_vec.size());
                        for (size_t i = 0; i < dis_vec_float.size(); ++i) {
                            if (sscanf(dis_vec[i].c_str(), "%f", &dis_vec_float[i]) <= 0) {
                                std::cerr << "parse diescrete feature map failed at line:\n" << line << std::endl;
                                exit(-1);
                            }   
                        }
                        discrete_feature_range_map[fid] = dis_vec_float;
                    } else {
                        std::cout << "begin read origin hash map ...\n";
                        char idx_file_name[128];
                        uint64 start_pos, range;
                        if (!str2long(seg[1], start_pos)) {
                            std::cerr << "parse start position of " << fid << " failed!" << std::endl;
                            exit(-1);
                        }
                        if (!str2long(seg[2], range)) {
                            std::cerr << "parse feature range of " << fid << " failed!" << std::endl;
                            exit(-1);
                        }

                        if (one_hot_feature_hash_map.count(fid) == 0) {
                            one_hot_feature_hash_map[fid] = new std::unordered_map<std::string, uint64>();
                            one_hot_feature_hash_set[fid] = new std::unordered_set<uint64>();
                        }
                        std::unordered_map<std::string, uint64>* hash_map = one_hot_feature_hash_map[fid];
                        std::unordered_set<uint64>* hash_key_set = one_hot_feature_hash_set[fid];
                        if (sprintf(idx_file_name, "%s/%llu.idx", idx_dir.c_str(), fid) > 0) {
                            std::cout << "    origin index file is:" << idx_file_name << "\n";
                            // 读取之前的索引数据
                            std::ifstream fOrigData(idx_file_name);
                            if (fOrigData.is_open()) {
                                uint64 line_no = 0;
                                while(!fOrigData.eof()) {
                                    std::string line;
                                    std::getline(fOrigData, line);
                                    std::vector<std::string> seg;
                                    split_string(line, "\t", seg);
                                    if (seg.size() < 2) {
                                        std::cerr << "parse origin index file failed! at line_no[" << line_no << "]:\n" << line << std::endl;
                                        exit(-1);
                                    }
                                    uint64 indx_value ;
                                    if (!str2long(seg[1], indx_value)) {
                                        std::cerr << "convert " << seg[1] << "to long failed" << std::endl;
                                        exit(-1);
                                    }
                                    hash_map->at(seg[1])= indx_value;
                                    hash_key_set->insert(indx_value);
                                    line_no ++;
                                }
                                std::cout << "read origin index file finished, total records num is "<< line_no << std::endl;
                            } else {
                                std::cout << "no origin index file found, ignore ... " << std::endl;
                            }
                            fOrigData.close();
                        }else {
                            std::cerr << "can't construct idx_file_name!" << std::endl;
                            exit(-1);
                        }
                    }
                }
            } else {
                std::cerr << "no configure found, abort ... " << std::endl;
                exit(-1);
            }
            fConfigure.close();
        }
		{
            // 记录one_hot feature，并更新hash映射表
			std::ifstream fRawData(in_feature_file.c_str());
			if (! fRawData.is_open()) {
				throw "unable to open feature file: " + in_feature_file;
			}
            uint64 line_no = 0;
            std::vector<std::string> col_buf(3);
			while (!fRawData.eof()) {
				std::string line;
				std::getline(fRawData, line);
                std::vector<std::string> colums;
                if (line.size() == 0 || line[0] == '#') {
                    continue;
                }
                split_string(line, "\t", colums);
                // 跳过第一个y
                for(size_t i = 1; i < colums.size(); ++i) {
                     std::string &part = colums[i];
                     if (part.size() > 0) {
                        split_string(part, ":", col_buf);
                        if (col_buf.size() != 3) {
                            std::cerr << "WARNING: invalid line at: " << line_no << "\n";
                            break;
                        }
                        uint64 fid = 0L;
                        std::string feature_type = col_buf[1];
                        std::string feature_value = col_buf[2];
                        if (!str2long(col_buf[0], fid)) {
                            std::cerr << "WARNING: invalid line at: " << line_no << "\n";
                            break;
                        }
                        if (feature_type == "oh") {
                            std::unordered_map<std::string, uint64>* hash_map = one_hot_feature_hash_map[fid];
                            std::unordered_set<uint64>* hash_key_set = one_hot_feature_hash_set[fid];
                            if (configure_map.count(fid) == 0) {
                                 std::cerr << "WARNING: invalid feature id: "<< fid <<"\n";
                                 break;
                            }
                            uint64 start_pos = configure_map[fid].start_pos;
                            uint64 range = configure_map[fid].range;
                            if (hash_map->count(feature_value) == 0) {
                                uint64 hash_value = str_hash(feature_value, seed, range, start_pos);
                                while (hash_key_set->count(hash_value) > 0) {
                                    uint64 step = rand()%range;
                                    if (std::numeric_limits<uint64>().max() - step > seed) {
                                        seed = step;
                                    } else {
                                        seed += step;
                                    }
                                    std::cout << "INFO: hash meet conflict for: " << feature_value << ", new seed = " << seed << "\n";
                                    hash_value = str_hash(feature_value, seed, range, start_pos);
                                }
                                hash_map->at(feature_value) = hash_value;
                                hash_key_set->insert(hash_value);
                            }
                        }
                     }
                }
                ++line_no;
			} 
			fRawData.close();
		}

        {
            // 输出index文件：
            char out_indx_file[128];
            for (auto it = one_hot_feature_hash_map.begin();
                it != one_hot_feature_hash_map.end(); ++it) {
                if (sprintf(out_indx_file, "%s/%llu.idx", idx_dir.c_str(), it->first)>0) {
                    std::ofstream oIndexFile(out_indx_file);
                    std::unordered_map<std::string, uint64>* hash_map = one_hot_feature_hash_map[it->first];
                    for (auto it2 = hash_map->begin(); it2 != hash_map->end(); ++it2) {
                        oIndexFile << it2->first.c_str() <<"\t"<< it2->second <<"\n";
                    }
                    oIndexFile.close();
                } else {
                    std::cerr << "make feature index of " << it->first << "failed!" << std::endl;
                    exit(-1);
                }
            }
        }

        {
            // 把特征转换成libFM特征
            std::ifstream iRawData(in_feature_file.c_str());
            std::ofstream oLibFM(out_libfm_file.c_str());
            std::vector<std::string> line_buf_seg;
            line_buf_seg.reserve(1000);
            std::vector<std::string> segs, cols;
            while (!iRawData.eof()) {
                std::string line;
                std::getline(iRawData, line);
            
                if (line.size() == 0 || line[0] == '#') {
                    continue;
                }
                split_string(line, "\t", line_buf_seg);
                if (line_buf_seg.size() > 1) {
                    oLibFM << line_buf_seg[0];
                    for (size_t i = 1; i < line_buf_seg.size(); ++i) {
                        split_string(line_buf_seg[i], ":", segs);
                        if (segs.size() > 2) {
                            std::string fid_str = segs[0],
                                type = segs[1],
                                feature_value = segs[3];
                            uint64 fid;
                            if (!str2long(fid_str, fid)) {
                                 std::cerr << "WARNNING: fid `"<<fid_str<<"` can't convert to uint64, ignore ..." << "\n"; 
                                 continue;
                            }
                            if (type == "oh") {// one hot 
                                uint64 hv = one_hot_feature_hash_map[fid]->at(feature_value);
                                oLibFM << " " << hv <<":1";
                            } else if (type == "dis") {//float 
                                auto discrete_point = discrete_feature_range_map[fid];
                                float flt_val = atof(feature_value.c_str());
                                bool reach_end = true;
                                uint64 start_pos = configure_map[fid].start_pos;
                                uint64 range = configure_map[fid].range; 
                                uint64 len = std::min(range, uint64(discrete_point.size()));
                                for (size_t i = 0; i < len-1; ++i) {
                                    if (flt_val < discrete_point[i]) {
                                        oLibFM << " " << start_pos + i <<":1";
                                        reach_end = false;
                                        break;
                                    }
                                }
                                if (reach_end) {
                                    oLibFM << " " << start_pos + len-1 <<":1";
                                }
                            } else {
                                std::cerr << "WARNNING: type " << type << "not supported, ignore ..." << std::endl;
                            }
                        }
                    }
                    oLibFM << "\n";
                }
            }
            iRawData.close();
            oLibFM.close();
        }
	} catch (std::string &e) {
		std::cerr << e << std::endl;
	}

}
