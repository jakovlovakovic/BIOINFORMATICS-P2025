#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <string>
#include <algorithm>
#include "BambooFilter.h"
extern "C" {
#include "hash/xxhash.h"  
}

int main(int argc, char* argv[]) {
	// open input file
	std::ofstream outFile("output.txt");
	// read configuration file
	std::string configFilePath;
	if (argc >= 3) {
		configFilePath = argv[1];
	}
	else {
		outFile << "Failed to open config file.\n";
		return 1;
	}

	outFile << "Initializing parameters from config file...\n";

	std::ifstream configFile(configFilePath);
	if (!configFile.is_open()) {
		outFile << "Failed to open config file.\n";
		return 1;
	}

	std::vector<std::string> lines;
	std::string line;
	while (std::getline(configFile, line)) {
		line.erase(std::remove(line.begin(), line.end(), '\r'), line.end()); // for linux
		line.erase(line.find_last_not_of(" \t\n\r") + 1); // for linux
		if (!line.empty() && line[0] != '#') lines.push_back(line);
	}
	configFile.close();

	// parsing lines
	size_t K = std::stoul(lines[0]); // size K from config
	bool forAnalyze = lines[1][0] == '1' ? true : false; // analasis mode yes/no
	std::vector<std::string> elementsToLookup;
	for (size_t i = 2; i < lines.size(); i++) {
		elementsToLookup.push_back(lines[i]);
	}

	outFile << "Parameters from config file initialized.\n";

	outFile << "Initializing BambooFilter...\n";

	BambooFilter bf; // initialise BambooFilter

	// read the dna file
	std::string dnaFilePath;
	if (argc >= 3) {
		dnaFilePath = argv[2];
	}
	std::ifstream file(dnaFilePath);
	if (!file.is_open()) {
		return 1;
	}
	std::ostringstream buffer;
	buffer << file.rdbuf();
	std::string fileContent = buffer.str();
	file.close();

	fileContent.erase(std::remove(fileContent.begin(), fileContent.end(), '\n'), fileContent.end());
	fileContent.erase(std::remove(fileContent.begin(), fileContent.end(), '\r'), fileContent.end()); // for linux

	// insert time statistic
	std::vector<long long> cumulative_insert_times;
	long long current_cumulative_insert_time = 0;
	int insert_count = 0;
	/*experimental trial showed that 10000 entries per time mesurment was the best amount as it has few/no instances of 0 microseconds per 10000 inserts*/
	int insert_report_interval = 10000; // how many inserts to be taken into acount in the report

	for (size_t i = 0; i + K <= fileContent.size(); i++) {
		auto insert_start = std::chrono::high_resolution_clock::now();
		bf.insert(fileContent.substr(i, K));
		auto insert_end = std::chrono::high_resolution_clock::now();
		auto insert_duration = std::chrono::duration_cast<std::chrono::microseconds>(insert_end - insert_start).count();

		current_cumulative_insert_time += insert_duration;
		insert_count++;

		if (insert_count % insert_report_interval == 0) {
			cumulative_insert_times.push_back(current_cumulative_insert_time);
			current_cumulative_insert_time = 0; // reset
		}
	}

	// The entries not belonging in a set of 10000
	if (current_cumulative_insert_time > 0) {
		cumulative_insert_times.push_back(current_cumulative_insert_time);
	}

	outFile << "Filter initialized.\n";

	if (forAnalyze) {

		// Lookup time statistic
		std::vector<long long> cumulative_lookup_times;
		long long current_cumulative_lookup_time = 0;
		int lookup_count = 0;
		int lookup_report_interval = 10000; // how many lookups to be taken into account in the report

		for (size_t i = 0; i + K <= fileContent.size(); i++) {
			auto lookup_start = std::chrono::high_resolution_clock::now();
			bf.lookup(fileContent.substr(i, K));
			auto lookup_end = std::chrono::high_resolution_clock::now();
			auto lookup_duration = std::chrono::duration_cast<std::chrono::microseconds>(lookup_end - lookup_start).count();

			current_cumulative_lookup_time += lookup_duration;
			lookup_count++;

			if (lookup_count % lookup_report_interval == 0) {
				cumulative_lookup_times.push_back(current_cumulative_lookup_time);
				current_cumulative_lookup_time = 0; // reset
			}
		}

		// The entries not belonging in a set of 10000
		if (current_cumulative_lookup_time > 0) {
			cumulative_lookup_times.push_back(current_cumulative_lookup_time);
		}

		// CSV file write
		std::ofstream csv_file("bamboo_filter_timings.csv");
		if (!csv_file.is_open()) {
			outFile << "Error opening CSV file for writing!" << std::endl;
			return 1;
		}

		// Upis insert
		csv_file << "Operation, Interval Start Index, Cumulative Time (microseconds)\n";
		for (size_t i = 0; i < cumulative_insert_times.size(); i++) {
			csv_file << "Insert," << i * insert_report_interval << "," << cumulative_insert_times[i] << "\n";
		}

		// Upis lookup
		csv_file << "\nOperation ,Interval Start Index,Cumulative Time (microseconds)\n";
		for (size_t i = 0; i < cumulative_lookup_times.size(); i++) {
			csv_file << "Lookup," << i * lookup_report_interval << "," << cumulative_lookup_times[i] << "\n";
		}

		csv_file.close();

		outFile << "Analysis written in .csv file.\n";
	}

	outFile << "Starting lookup operations...\n";
	for (auto& element : elementsToLookup) {
		bool result = bf.lookup(element);

		if (result) {
			outFile << "Lookup for " << element << " successful.\n";
		}
		else {
			outFile << "Lookup for " << element << " failed.\n";
		}
	}

	outFile.close();

	return 0;
}