#pragma once

#include <chrono>
#include <ctime>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <optional>
#include <mutex>
#include <thread>

using namespace std::literals;

#define LOG(...) Logger::GetInstance().Log(__VA_ARGS__)

class Logger {
    auto GetTime() const {
        if (manual_ts_) {
            return *manual_ts_;
        }

        return std::chrono::system_clock::now();
    }

    auto GetTimeStamp() const {
        const auto now = GetTime();
        const auto t_c = std::chrono::system_clock::to_time_t(now);
        return std::put_time(std::localtime(&t_c), "%F %T");
    }

    // Для имени файла возьмите дату с форматом "%Y_%m_%d"
    std::string GetFileTimeStamp() const {
        const auto now = GetTime();
        const auto t_c = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << std::put_time(gmtime(&t_c), "%Y_%m_%d");
        return oss.str();
    }

    std::string GetLogFileName() const {
        return "/var/log/sample_log_" + GetFileTimeStamp() + ".log";
    }

    Logger() = default;
    Logger(const Logger&) = delete;

public:
    static Logger& GetInstance() {
        static Logger obj;
        return obj;
    }
    void OpenLogFileIfNeeded() {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::string new_filename = GetLogFileName();

        if (new_filename != current_filename_) {
            // Закрытие старого файла, если он открыт
            if (log_file_.is_open()) {
                log_file_.close();
            }

            // Открытие нового файла
            current_filename_ = new_filename;
            log_file_.open(current_filename_, std::ios::app);
        }
    }

    template<class... Ts>
    void Log(const Ts&... args) {
        OpenLogFileIfNeeded();  // Проверка и открытие файла, если нужно
        std::lock_guard<std::mutex> lock(mutex_);

        std::ostringstream log_stream;
        log_stream << GetTimeStamp() << ": ";  // Временная метка

        // Формирование строки для записи
        (log_stream << ... << args);  // Вывод всех аргументов

        // Запись в файл
        log_file_ << log_stream.str() << std::endl;
    }


    void SetTimestamp(std::chrono::system_clock::time_point ts) {
        std::lock_guard<std::mutex> lock(mutex_);
        manual_ts_ = ts;
    }

    

private:
    std::optional<std::chrono::system_clock::time_point> manual_ts_;
    std::ofstream log_file_;
    std::string current_filename_;
    mutable std::mutex mutex_;
};
