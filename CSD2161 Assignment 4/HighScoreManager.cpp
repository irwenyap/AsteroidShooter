#include "HighScoreManager.hpp"

#include <fstream>
#include <algorithm>

const size_t MAX_HIGH_SCORES = 10;


void SaveHighScores(const std::vector<HighScore>& newScores) {
    // Load existing scores
    std::vector<HighScore> scores = LoadHighScores();

    // Append new scores
    scores.insert(scores.end(), newScores.begin(), newScores.end());

    // Sort descending by score
    std::sort(scores.begin(), scores.end(), [](const HighScore& a, const HighScore& b) {
        return a.score > b.score;
        });

    // Trim to top 10
    if (scores.size() > MAX_HIGH_SCORES) {
        scores.resize(MAX_HIGH_SCORES);
    }

    // Save trimmed, sorted scores back to file
    std::ofstream file("highscores.txt");
    for (const auto& entry : scores) {
        file << entry.name << " " << entry.score << "\n";
    }
}

std::vector<HighScore> LoadHighScores() {
    std::vector<HighScore> scores;
    std::ifstream file("highscores.txt");
    std::string name;
    int score;
    while (file >> name >> score) {
        scores.push_back({ name, score });
    }
    return scores;
}