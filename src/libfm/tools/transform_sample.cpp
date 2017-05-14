// Copyright (C) 2017 yixia.com
// create time: "2017/05/11 11:21"
// ./transfrom_sample 把category 特征hash成uniq id，同时输出映射表和映射后的libfm样本

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
 *	    first version
 * 1.1.0:
 *      把vid直接转换成uint64，然后按照某种规则进行转化。省去映射表的保存
 *      对uniqid、word等特征，才去计算映射表
 *      对于标称特征，当成字符串去计算hash值。
 *      对于浮点特征，需要离散化
 */
using namespace std;
bool discrete_feature_bypoints(const std::map<std::string, std::vector<float> >& discrete_feature_range_map,
    const std::string& feature_name,
    std::string& feature_value) {
    const std::vector<float>& discrete_point = discrete_feature_range_map.at(feature_name);
    float feature_flt_value;
    if (sscanf(feature_value.c_str(), "%f", &feature_flt_value) <= 0) {
        return false;
    }
    bool out_of_end = true;
    char new_name[128];
    for (size_t i = 0; i < discrete_point.size(); ++i) {
        if (feature_flt_value <= discrete_point[i]) {
            sprintf(new_name, "%s-%lu", feature_name.c_str(), i);
            feature_value.assign(new_name);
            out_of_end = false;
            break;
        }
    }
    if (out_of_end) {
        sprintf(new_name, "%s-%lu", feature_name.c_str(), discrete_point.size());
        feature_value.assign(new_name);
    }
    return true;
}
int main(int argc, char **argv) { 
    const uint64 vid_offset_base = 1000000000000000L;
                                   
 	uint seed = time(NULL);
	try {
		CMDLine cmdline(argc, argv);
		const std::string in_param   = cmdline.registerParameter("i", "input feature file");
		const std::string out_param  = cmdline.registerParameter("o", "output libfm file");
        const std::string conf_param = cmdline.registerParameter("conf", "feature name that need to discrte, followed by discrte point, seperated by comma");
        const std::string idx_param  = cmdline.registerParameter("idx", "index file name");
		const std::string param_help = cmdline.registerParameter("help", "this screen");


		if (cmdline.hasParameter(param_help) || (argc == 1)) {
            std::cout << "Usage: " << "\n";
            std::cout << "Input file format:" << "\n";
            std::cout << "y_target feature_name:value feature_name:value ..." << "\n";
            std::cout << "    Note:对于vid开头的特征，需要使用'-0', '-1'等后缀来区分不同的vid. 必须从0开始" << "\n";
            std::cout << "         程序会根据这些数字来把不同的vid 偏移到不同区间。比如vid-0，" << "\n";
            std::cout << "         会把解析出的vid长整型加上`1*"<<vid_offset_base <<"`这个offset, vid-1加2倍的offset" << "\n";
            std::cout << "         对于其他特征，则全部hash到 [0, "<<vid_offset_base<<"] 这个区间。"<<"\n";
            std::cout << "Discrete Configure file format:" << "\n";
            std::cout << "feature_name 0,0.1,0.2  " <<"\n";
            std::cout << "# 对于feature name这维特征，(-inf, 0]的取值映射为'feature_name-0'字符串，然后计算这个字符串的hash值" << "\n";
            std::cout << "# 对于(0, 0.1]范围内的取值，则转换为'feature_name-1'字符串，再计算hash值" << "\n";
			cmdline.print_help();
			return 0;
		}
		cmdline.checkParameters();

		std::string in_feature_file = cmdline.getValue(in_param);
		std::string out_libfm_file = cmdline.getValue(out_param);
		std::string configure = cmdline.getValue(conf_param);
        std::string idx_file = cmdline.getValue(idx_param, "");

        auto one_hot_feature_hash_map = new std::unordered_map<std::string, uint64>();
        auto one_hot_feature_hash_set = new std::unordered_set<uint64>();
        std::map<std::string, std::vector<float> > discrete_feature_range_map;
        {
            std::cout<< "读取配置文件, 同时加载hash映射表信息 .... " << "\n";
            std::ifstream fConfigure(configure.c_str());
            if (fConfigure.is_open()) {
                while(!fConfigure.eof()) {
                    std::string line;
                    std::getline(fConfigure, line);
                    if (line.size() == 0 || line[0] == '#') continue; 
                    std::vector<std::string> seg(2);
                    split_string(line, "\t", seg);
                    if (seg.size() < 2) {
                        std::cerr << "configure format wrong: colum number less than 2:\nline is:\n" << line << std::endl;
                        exit(-1);
                    }
                    std::string feature_name = seg[0];
                    std::vector<std::string> dis_vec;
                    split_string(seg[1], ",", dis_vec);
                    std::vector<float> dis_vec_float(dis_vec.size());
                    for (size_t i = 0; i < dis_vec_float.size(); ++i) {
                        if (sscanf(dis_vec[i].c_str(), "%f", &dis_vec_float[i]) <= 0) {
                            std::cerr << "parse diescrete feature map failed at line:\n" << line << std::endl;
                            exit(-1);
                        }   
                    }
                    discrete_feature_range_map.insert(std::make_pair(feature_name, dis_vec_float));
                }
                fConfigure.close();
            }
        }
        {
            std::cout << "读取之前的索引数据 ..." << "\n";
            std::ifstream fOrigData(idx_file.c_str());
            if (fOrigData.is_open()) {
                uint64 line_no = 0;
                while(!fOrigData.eof()) {
                    std::string line;
                    std::getline(fOrigData, line);
                    if (line.size() == 0 || line[0] == '#') continue;
                    std::vector<std::string> seg;
                    split_string(line, "\t", seg);
                    if (seg.size() < 2) {
                        std::cerr << "WARNING: ignore invalide index line at line_no[" << line_no << "]:\n" << line << std::endl;
                        continue;
                    }
                    uint64 indx_value ;
                    if (!str2long(seg[1], indx_value)) {
                        std::cerr << "convert " << seg[1] << "to long failed" << std::endl;
                        exit(-1);
                    }
                    one_hot_feature_hash_map->insert(std::make_pair(seg[0], indx_value));
                    one_hot_feature_hash_set->insert(indx_value);
                    line_no ++;
                }
                std::cout << "read origin index file finished, total records num is "<< line_no << std::endl;
            } else {
                std::cout << "no origin index file found, ignore ... " << std::endl;
            }
            fOrigData.close();
        }
		{
            std::cout<< "记录、转换one_hot feature，并更新hash映射表 ... " << "\n";
			std::ifstream fRawData(in_feature_file.c_str());
			if (!fRawData.is_open()) {
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
                        if (col_buf.size() < 2) {
                            std::cerr << "WARNING: invalid line at: " << line_no << ", line is:\n" << line << "\n";
                            break;
                        }
                        std::string feature_name = col_buf[0];
                        std::string feature_value = col_buf[1];
                        if (feature_name.substr(0, 3) != "vid") {
                            if (discrete_feature_range_map.count(feature_name) > 0) {
                                if (!discrete_feature_bypoints(discrete_feature_range_map,
                                            feature_name, feature_value)) {
                                    std::cerr << "at line[" <<  line_no << "] parse float valued failed, line is\n" << line << std::endl;   
                                    exit(-1);
                                }
                            }
                            if (one_hot_feature_hash_map->count(feature_value) == 0) {
                                uint64 hash_value = str_hash(feature_value, seed);
                                while (one_hot_feature_hash_set->count(hash_value) > 0) {
                                    std::cout << "feature: " << feature_value <<", hash key = " << hash_value << "\n";
                                    seed = rand();
                                    std::cout << "INFO: hash meet conflict, new seed = " << seed << "\n";
                                    hash_value = str_hash(feature_value, seed);
                                }
                                one_hot_feature_hash_map->insert(std::make_pair(feature_value, hash_value));
                                one_hot_feature_hash_set->insert(hash_value);
                            }
                        }
                     }
                }
                ++line_no;
			} 
			fRawData.close();
		}

        {
            std::cout << "输出index文件..." << "\n";
            std::ofstream oIndexFile(idx_file.c_str());
            if (oIndexFile.is_open()) {
                for (auto it2 = one_hot_feature_hash_map->begin();
                        it2 != one_hot_feature_hash_map->end(); ++it2) {
                    oIndexFile << it2->first.c_str() <<"\t"<< it2->second <<"\n";
                }
                oIndexFile.close();
            } else {
                std::cerr << "WARNING: can't open idx file to write, ignore ..." << std::endl;
            }
        }

        {
            if (!fileexists(out_libfm_file)) {
                std::cerr << "libfm output file: ["<< out_libfm_file << "] not exists!" << std::endl;
                exit(-1);
            }
            std::cout<<"把特征转换成libFM特征..."<<"\n";
            std::ifstream iRawData(in_feature_file.c_str());
            std::ofstream oLibFM(out_libfm_file.c_str());
            std::vector<std::string> line_buf_seg;
            line_buf_seg.reserve(1000);
            std::vector<std::string> segs, cols;
            while (!iRawData.eof()) {
                std::string line;
                std::getline(iRawData, line);
                uint line_no = 0;
                if (line.size() == 0 || line[0] == '#') {
                    continue;
                }
                split_string(line, "\t", line_buf_seg);
                if (line_buf_seg.size() > 1) {
                    oLibFM << line_buf_seg[0];
                    for (size_t i = 1; i < line_buf_seg.size(); ++i) {
                        split_string(line_buf_seg[i], ":", segs);
                        if (segs.size() > 1) {
                            std::string feature_name = segs[0],
                                         feature_value = segs[1];
                            if (feature_name.substr(0, 3) == "vid") {// vid feature 
                                auto vid_offset = (std::atoi(feature_name.substr(4).c_str()) + 1L) * vid_offset_base;
                                uint64 vid_idx = 0L;
                                if (!str2long(feature_value, vid_idx)) {
                                    std::cerr << "ERROR: parse vid at line["<<line_no<<"] failed, line is\n" << line << std::endl;
                                    exit(-1);
                                }
                                oLibFM <<" " << (vid_idx + vid_offset) << ":1";
                                continue;
                            } else if (discrete_feature_range_map.count(feature_name) > 0) { // 浮点数，需要离散化的特征 
                                if (!discrete_feature_bypoints(discrete_feature_range_map,
                                        feature_name, feature_value)) {
                                    std::cerr<<"ERROR: discrete featrue "<<feature_name <<"failed, line is\n"<< line<<std::endl;
                                    exit(-1);
                                }
                            }
                            auto fid = one_hot_feature_hash_map->at(feature_value);
                            oLibFM << " " << fid << ":1";
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
    std::cout << "Done, good bye!" << std::endl;
}
