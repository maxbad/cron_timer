﻿#pragma once
#include <string>
#include <set>
#include <list>
#include <vector>
#include <sstream>
#include <thread>
#include <mutex>
#include <functional>
#include <atomic>

namespace CronTimer {
class noncopyable {
protected:
	noncopyable() = default;
	~noncopyable() = default;

private:
	noncopyable(const noncopyable&) = delete;
	const noncopyable& operator=(const noncopyable&) = delete;
};

class Text {
public:
	static size_t SplitStr(std::vector<std::string>& os, const std::string& is, const char c) {
		os.clear();
		std::string::size_type pos_find = is.find_first_of(c, 0);
		std::string::size_type pos_last_find = 0;
		while (std::string::npos != pos_find) {
			std::string::size_type num_char = pos_find - pos_last_find;
			os.emplace_back(is.substr(pos_last_find, num_char));

			pos_last_find = pos_find + 1;
			pos_find = is.find_first_of(c, pos_last_find);
		}

		std::string::size_type num_char = is.length() - pos_last_find;
		if (num_char > is.length())
			num_char = is.length();

		std::string sub = is.substr(pos_last_find, num_char);
		os.emplace_back(sub);
		return os.size();
	}

	static size_t SplitInt(std::vector<int>& number_result, const std::string& is, const char c) {
		std::vector<std::string> string_result;
		SplitStr(string_result, is, c);

		number_result.clear();
		for (size_t i = 0; i < string_result.size(); i++) {
			const std::string& value = string_result[i];
			number_result.emplace_back(atoi(value.data()));
		}

		return number_result.size();
	}
};

class CronExpression {
public:
	enum DATA_TYPE {
		DT_SECOND = 1,
		DT_MINUTE = 2,
		DT_HOUR = 3,
		DT_DAY_OF_MONTH = 4,
		DT_MONTH = 5,
		DT_DAY_OF_WEEK = 6,
		DT_YEAR = 7,
	};

	CronExpression(const std::string& input, DATA_TYPE data_type) {

		//
		// 注意：枚举之前是','，为了能在csv中使用改成了';'
		// 20181226 xinyong
		//

		static const char CRON_SEPERATOR_ENUM = ';';
		static const char CRON_SEPERATOR_RANGE = '-';
		static const char CRON_SEPERATOR_INTERVAL = '/';

		is_all_ = false;
		if (input == "*") {
			is_all_ = true;
		} else if (input.find_first_of(CRON_SEPERATOR_ENUM) != std::string::npos) {
			std::vector<int> v;
			Text::SplitInt(v, input, CRON_SEPERATOR_ENUM);
			std::pair<int, int> pair_range = GetRangeFromType(data_type);
			for (auto value : v) {
				if (value < pair_range.first || value > pair_range.second) {
					error_ << "out of range ,value:" << value << ", data_type:" << data_type;
					return;
				}
				values_.insert(value);
			}
		} else if (input.find_first_of(CRON_SEPERATOR_RANGE) != std::string::npos) {
			std::vector<int> v;
			Text::SplitInt(v, input, CRON_SEPERATOR_RANGE);
			if (v.size() != 2) {
				error_ << "time format error:" << input;
				return;
			}

			int from = v[0];
			int to = v[1];
			std::pair<int, int> pair_range = GetRangeFromType(data_type);
			if (from < pair_range.first || to > pair_range.second) {
				error_ << "out of range from:" << from << ", to:" << to
					   << ", data_type:" << data_type;
				return;
			}

			for (int i = 0; i <= to; i++) {
				values_.insert(i);
			}
		} else if (input.find_first_of(CRON_SEPERATOR_INTERVAL) != std::string::npos) {
			std::vector<int> v;
			Text::SplitInt(v, input, CRON_SEPERATOR_INTERVAL);
			if (v.size() != 2) {
				error_ << "time format error:" << input;
				return;
			}

			int from = v[0];
			int interval = v[1];
			std::pair<int, int> pair_range = GetRangeFromType(data_type);
			if (from < pair_range.first || interval < 0) {
				error_ << "out of range from:" << from << ", interval:" << interval
					   << ", data_type:" << data_type;
				return;
			}

			for (int i = from; i <= pair_range.second; i += interval) {
				values_.insert(i);
			}
		} else {
			std::pair<int, int> pair_range = GetRangeFromType(data_type);
			int value = atoi(input.data());
			if (value < pair_range.first || value > pair_range.second) {
				error_ << "out of range from:" << pair_range.first << ", to:" << pair_range.second
					   << ", data_type:" << data_type;
				return;
			}

			values_.insert(value);
		}
	}

	bool IsValid() const {
		if (is_all_)
			return true;
		return !values_.empty();
	}

	bool Hit(int value) const {
		if (is_all_)
			return true;
		return values_.find(value) != values_.end();
	}

	std::string GetError() const { return error_.str(); }

private:
	static std::pair<int, int> GetRangeFromType(DATA_TYPE data_type) {
		int from = 0;
		int to = 0;

		switch (data_type) {
		case CronExpression::DT_SECOND:
		case CronExpression::DT_MINUTE:
			from = 0;
			to = 59;
			break;
		case CronExpression::DT_HOUR:
			from = 0;
			to = 23;
			break;
		case CronExpression::DT_DAY_OF_MONTH:
			from = 1;
			to = 31;
			break;
		case CronExpression::DT_MONTH:
			from = 1;
			to = 12;
			break;
		case CronExpression::DT_DAY_OF_WEEK:
			from = 0;
			to = 6;
			break;
		case CronExpression::DT_YEAR:
			from = 1970;
			to = 2099;
			break;
		}

		return std::make_pair(from, to);
	}

private:
	bool is_all_;
	std::set<int> values_;
	std::ostringstream error_;
};

#define CRON_TIME_IS_VALID(p) ((p) != nullptr && (p)->IsValid())
#define CRON_TIME_GET_ERROR(p) ((p) == nullptr ? "nullptr" : (p)->GetError())

class CronTime {
public:
	CronTime(const std::string& time_string) {
		std::vector<std::string> v;
		Text::SplitStr(v, time_string, ' ');
		if (v.size() != 7)
			return;

		const std::string& second = v[0];
		const std::string& minute = v[1];
		const std::string& hour = v[2];
		const std::string& day_of_month = v[3];
		const std::string& month = v[4];
		const std::string& day_of_week = v[5];
		const std::string& year = v[6];

		second_ = std::make_shared<CronExpression>(second, CronExpression::DT_SECOND);
		minute_ = std::make_shared<CronExpression>(minute, CronExpression::DT_MINUTE);
		hour_ = std::make_shared<CronExpression>(hour, CronExpression::DT_HOUR);
		day_of_month_ =
			std::make_shared<CronExpression>(day_of_month, CronExpression::DT_DAY_OF_MONTH);
		month_ = std::make_shared<CronExpression>(month, CronExpression::DT_MONTH);
		day_of_week_ =
			std::make_shared<CronExpression>(day_of_week, CronExpression::DT_DAY_OF_WEEK);
		year_ = std::make_shared<CronExpression>(year, CronExpression::DT_YEAR);
	}

	bool IsValid() const {
		return (CRON_TIME_IS_VALID(second_) && CRON_TIME_IS_VALID(minute_) &&
				CRON_TIME_IS_VALID(hour_) && CRON_TIME_IS_VALID(day_of_month_) &&
				CRON_TIME_IS_VALID(month_) && CRON_TIME_IS_VALID(day_of_week_) &&
				CRON_TIME_IS_VALID(year_));
	}

	bool Hit(time_t t) const {
		tm _tm;
#ifdef _WIN32
		::localtime_s(&_tm, &t);
#else
		::localtime_r(&t, &_tm);
#endif
		return second_->Hit(_tm.tm_sec) && minute_->Hit(_tm.tm_min) && hour_->Hit(_tm.tm_hour) &&
			   day_of_month_->Hit(_tm.tm_mday) && month_->Hit(_tm.tm_mon + 1) &&
			   day_of_week_->Hit(_tm.tm_wday) && year_->Hit(_tm.tm_year + 1900);
	}

	std::string GetError() const {
		std::string error;
		error += "second_" + CRON_TIME_GET_ERROR(second_);
		error += "minute_" + CRON_TIME_GET_ERROR(minute_);
		error += "hour_" + CRON_TIME_GET_ERROR(hour_);
		error += "day_of_month_" + CRON_TIME_GET_ERROR(day_of_month_);
		error += "month_" + CRON_TIME_GET_ERROR(month_);
		error += "day_of_week_" + CRON_TIME_GET_ERROR(day_of_week_);
		error += "year_" + CRON_TIME_GET_ERROR(year_);
		return error;
	}

private:
	std::shared_ptr<CronExpression> second_;
	std::shared_ptr<CronExpression> minute_;
	std::shared_ptr<CronExpression> hour_;
	std::shared_ptr<CronExpression> day_of_month_;
	std::shared_ptr<CronExpression> month_;
	std::shared_ptr<CronExpression> day_of_week_;
	std::shared_ptr<CronExpression> year_;
};

using CRON_FUNC_CALLBACK = std::function<void()>;

class TimerMgr : public noncopyable {
public:
	~TimerMgr() { Stop(); };

	void Start() {
		last_proc_ = time(nullptr);
		thread_stop_ = false;
		thread_ = std::make_shared<std::thread>([this]() { this->ThreadProc(); });
	}

	void Stop() {
		if (thread_ != nullptr) {
			thread_stop_ = true;
			thread_->join();
			thread_ = nullptr;
		}

		cron_timers_.clear();
	}

	int AddTimer(const std::string& timer_string, const CRON_FUNC_CALLBACK& func) {
		auto timer_ptr = std::make_shared<TimerUnit>(latest_timer_id_ + 1, timer_string, func);
		if (!timer_ptr->cron_time.IsValid()) {
			return 0;
		}

		++latest_timer_id_;
		std::lock_guard<std::mutex> lock(mutex_timers_);
		cron_timers_.push_back(timer_ptr);
		return timer_ptr->timer_id;
	}

	bool RemoveTimer(int timer_id) {
		std::lock_guard<std::mutex> lock(mutex_timers_);
		for (auto it = cron_timers_.begin(); it != cron_timers_.end();) {
			auto cron_timer = *it;
			if (cron_timer->timer_id == timer_id) {
				it = cron_timers_.erase(it);
				return true;
			}
			++it;
		}
		return false;
	}

	int Update() {
		std::list<CRON_FUNC_CALLBACK> tmp_callbacks;
		do {
			std::lock_guard<std::mutex> lock(mutex_callbacks_);
			if (!callbacks_.empty()) {
				tmp_callbacks.swap(callbacks_);
			}
		} while (false);

		for (const auto& func : tmp_callbacks) {
			func();
		}
		return (int)tmp_callbacks.size();
	}

private:
	struct TimerUnit {
		TimerUnit(
			int timer_id_r, const std::string& timer_string_r, const CRON_FUNC_CALLBACK& func_r)
			: timer_id(timer_id_r)
			, timer_string(timer_string_r)
			, func(func_r)
			, cron_time(timer_string) {}

		int timer_id;
		std::string timer_string;
		CRON_FUNC_CALLBACK func;
		CronTime cron_time;
	};

	void ThreadProc() {
		while (!thread_stop_) {
			time_t time_now = time(nullptr);
			if (time_now == last_proc_) {
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				continue;
			}

			last_proc_ = time_now;

			do {
				std::lock_guard<std::mutex> lock(mutex_timers_);
				for (const auto& cron_timer : cron_timers_) {
					if (cron_timer->cron_time.Hit(time_now)) {
						std::lock_guard<std::mutex> lock(mutex_callbacks_);
						callbacks_.push_back(cron_timer->func);
					}
				}
			} while (false);
		}
	}

private:
	mutable std::mutex mutex_timers_;
	std::list<std::shared_ptr<TimerUnit>> cron_timers_;

	mutable std::mutex mutex_callbacks_;
	std::list<CRON_FUNC_CALLBACK> callbacks_;

	int latest_timer_id_ = 0;
	std::shared_ptr<std::thread> thread_;
	std::atomic_bool thread_stop_;
	time_t last_proc_ = 0;
};
} // namespace CronTimer
