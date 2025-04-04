#pragma once
#include <vector>
#include <string>

struct HighScore {
	std::string name;
	int score;
};

void SaveHighScores(const std::vector<HighScore>& scores);
std::vector<HighScore> LoadHighScores();