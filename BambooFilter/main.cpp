#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <string>
#include "BambooFilter.h"
extern "C" {
#include "hash/xxhash.h"  
}

int main(void) {
	// citanje konfiguracijske datoteke
	std::cout << "Initializing parameters from config file...\n";

	std::ifstream configFile("filter_config.txt");
	if (!configFile.is_open()) {
		std::cerr << "Failed to open config file.\n";
		return 1;
	}

	std::vector<std::string> lines;
	std::string line;
	while (std::getline(configFile, line)) {
		if (line[0] != '#') lines.push_back(line);
	}
	configFile.close();

	// parsing lines
	size_t K = std::stoul(lines[0]); // velicina K
	bool forAnalyze = lines[1][0] == '1' ? true : false; // da li je za analizu ili ne
	std::vector<std::string> elementsToLookup;
	for (size_t i = 2; i < lines.size(); i++) {
		elementsToLookup.push_back(lines[i]);
	}

	std::cout << "Parameters from config file initialized.\n";

	std::cout << "Initializing BambooFilter...\n";

	BambooFilter bf; // inicijalizacija filtera

	// citanje filea
	std::ifstream file("GCA_000005845.2_ASM584v2_genomic.fna");
	if (!file.is_open()) {
		return 1;
	}
	std::ostringstream buffer;
	buffer << file.rdbuf();
	std::string fileContent = buffer.str();
	file.close();

	fileContent.erase(std::remove(fileContent.begin(), fileContent.end(), '\n'), fileContent.end());

	// insert
	std::vector<long long> cumulative_insert_times;
	long long current_cumulative_insert_time = 0;
	int insert_count = 0;
	int insert_report_interval = 10000; // koliko puta ponavljam

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

	std::cout << "Filter initialized.\n";

	if (forAnalyze) {
		// Kusur
		if (current_cumulative_insert_time > 0) {
			cumulative_insert_times.push_back(current_cumulative_insert_time);
		}

		// Lookup
		std::vector<long long> cumulative_lookup_times;
		long long current_cumulative_lookup_time = 0;
		int lookup_count = 0;
		int lookup_report_interval = 10000; // koliko puta ponavljam

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

		// Kusur
		if (current_cumulative_lookup_time > 0) {
			cumulative_lookup_times.push_back(current_cumulative_lookup_time);
		}

		// CSV
		std::ofstream csv_file("bamboo_filter_timings.csv");
		if (!csv_file.is_open()) {
			std::cerr << "Error opening CSV file for writing!" << std::endl;
			return 1;
		}

		// Upis insert
		csv_file << "Operation,Interval Start Index,Cumulative Time (microseconds)\n";
		for (size_t i = 0; i < cumulative_insert_times.size(); i++) {
			csv_file << "Insert," << i * insert_report_interval << "," << cumulative_insert_times[i] << "\n";
		}

		// Upis lookup
		csv_file << "\nLookup Cumulative,Interval Start Index,Cumulative Time (microseconds)\n";
		for (size_t i = 0; i < cumulative_lookup_times.size(); i++) {
			csv_file << "," << i * lookup_report_interval << "," << cumulative_lookup_times[i] << "\n";
		}

		csv_file.close();

		std::cout << "Analysis written in .csv file.\n";
	}

	std::cout << "Starting lookup operations...\n";
	for (auto& element : elementsToLookup) {
		bool result = bf.lookup(element);

		if (result) {
			std::cout << "Lookup for " << element << " successful.\n";
		}
		else {
			std::cout << "Lookup for " << element << " failed.\n";
		}
	}

	return 0;
}