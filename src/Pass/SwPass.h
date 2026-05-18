#pragma once

#include <functional>
#include <string>
#include <vector>

class SwPass {
private:
	std::string mName;
	std::vector<std::string> mDependencies;

public:
	SwPass();

	inline std::string getName() { return mName; };

	inline std::span<std::string> getDependencies() { return mDependencies; };

	void addDependency(std::string dependency);

	void execute();
};
