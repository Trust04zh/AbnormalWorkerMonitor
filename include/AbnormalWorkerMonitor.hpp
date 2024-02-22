#pragma once

#include "pch.h"
#include "config.h"
#include "Console.hpp"
#include "CircularBuffer.hpp"

#include <map>
#include <vector>
#include <set>
#include <sstream>

using namespace SDK;
using namespace DX11_Base;

// using TickTime = double;

// if a pal hits (at least `interval` seconds elapse between two hits) the `check_hit` condition for `threshold` times in `duration` seconds, then emit
class TimeWindow
{
public:
	std::string name;
	double duration;
	unsigned threshold;
	double interval;
	bool (*check_hit)(APalCharacter*);

	CircularBuffer<double> records;

	TimeWindow(std::string name, double duration, unsigned threshold, double interval, bool (*check_hit)(APalCharacter*));

	bool is_ready_to_emit();

	void on_hit(double time);

	bool is_tick_ready_for_next_interval(double time);

	bool try_emit();

	void alongside_emit();
};


class SingleAbnormalWorkerMonitor {
public:
	APalCharacter* PalCharacter = nullptr;
	std::vector<TimeWindow> time_windows;
	double cooldown;
	double cooldown_start = .0;

	SingleAbnormalWorkerMonitor() = default;

	SingleAbnormalWorkerMonitor(APalCharacter* PalCharacter, std::vector<TimeWindow> time_windows, double cooldown);

	void on_tick(double time, double delta);
};

class AbnormalWorkerMonitor {
public:
	using PalSet = std::set<SDK::APalCharacter*>;
	using PalTimeWindowManagerMap = std::map<SDK::APalCharacter*, SingleAbnormalWorkerMonitor>;

	std::mutex tick_mutex;
	std::condition_variable tick_cond_var;
	bool tick_ready = false;
	double tick_time;

	// pal distinct set, == keys of map
	PalSet pal_set;
	PalTimeWindowManagerMap pal_time_window_mans;
	std::vector<TimeWindow> time_windows;
	double cooldown = 5.0;

	void init();

	void on_tick(double time);

	void init_time_windows_and_cooldown();
	void init_check_spawn_point();
	void update_base_camps();
	void update_base_camp_pals();
};

class Logger {
public:
	static Logger& GetInstance() {
		static Logger instance;
		return instance;
	}

	std::vector<std::string> Items;
	bool AutoScroll = true;
	void AddLog(const char* fmt, ...) IM_FMTARGS(2) {

		char buf[1024];
		va_list args;
		va_start(args, fmt);
		vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
		buf[IM_ARRAYSIZE(buf) - 1] = 0;
		va_end(args);
		std::stringstream ss;
		ss << std::setw(4) << Items.size() + 1 << " | " << buf;
		Items.push_back(ss.str());
	}
	void Clear() {
		Items.clear();
	}
private:
	Logger() {}
	~Logger() {}

	Logger(const Logger&) = delete;
	Logger& operator=(const Logger&) = delete;
	Logger(Logger&&) = delete;
	Logger& operator=(Logger&&) = delete;
};
