#include "builder.hpp"

void build(int argc, char* argv[]) {
    std::string config;
    time_t timeout;
    options_description desc("Allowed options");
    desc.add_options()
        ("help", "выводим вспомогательное сообщение")
        ("config", value<std::string>(&config)->default_value("Debug"), "указываем конфигурацию сборки (по умолчанию Debug)")
        ("install", "добавляем этап установки (в директорию _install)")
        ("pack", "добавляем этап упаковки (в архив формата tar.gz)")
        ("timeout", value<time_t>(&timeout)->default_value(time_now()), "указываем время ожидания (в секундах)")
    ;

    variables_map vm;
    store(parse_command_line(argc, argv, desc), vm);
    notify(vm);

    if (vm.count("help") && !vm.count("config")  && !vm.count("pack")
                         && !vm.count("timeout") && !vm.count("install")) {
        std::cout << desc << "\n";
    }
    else {
        time_t time_spent = 0;
        if (config == "Debug" || config == "Release") {
            int res_1 = 0;
            int res_2 = 0;
            auto t1 = async::spawn([&res_1, config, timeout,
                                    &time_spent] () mutable {
                time_t start_1 = time_now();
                create_child(command_1 + config, timeout);
                time_t end_1 = time_now();
                time_spent += end_1 - start_1;
                time_t period_2 = timeout - time_spent;
                create_child(command_2, period_2, res_1);
                time_t end_2 = time_now();
                time_spent += end_2 - end_1;
            });
            if (vm.count("install") && res_1 == 0) {
                auto t2 = t1.then([&res_2, timeout, &time_spent] () mutable {
                    time_t period_3 = timeout - time_spent;
                    time_t start_3 = time_now();
                    create_child(command_3, period_3, res_2);
                    time_t end_3 = time_now();
                    time_spent += end_3 - start_3;
                });
                if (vm.count("pack") && res_2 == 0) {
                    auto t3 = t2.then([timeout, time_spent] () {
                        time_t period_4 = timeout - time_spent;
                        create_child(command_4, period_4);
                    });
                }
            }
          if (vm.count("pack") && !vm.count("install") && res_1 == 0) {
            auto t3 = t1.then([timeout, time_spent] () {
              time_t period_4 = timeout - time_spent;
              create_child(command_4, period_4);
            });
          }
        }
        else {
            std::cerr << "config = " << config << " doesn't exist!\n";
        }
    }
}

void create_child(const std::string& command, const time_t& period) {
    std::string line;
    ipstream out;
    child process(command, std_out > out);
    std::thread checkTime(check_time, std::ref(process), std::ref(period));
    while (out && std::getline(out, line) && !line.empty())
        std::cerr << line << std::endl;
    checkTime.join();
}

void create_child(const std::string& command, const time_t& period, int& res) {
    std::string line;
    ipstream out;
    child process(command, std_out > out);
    std::thread checkTime(check_time, std::ref(process), std::ref(period));
    while (out && std::getline(out, line) && !line.empty())
        std::cerr << line << std::endl;
    checkTime.join();
    res = process.exit_code();
}

void check_time(child& process, const time_t& period) {
    time_t start = time_now();
    while (true) {
        if ((time_now() - start > period) && process.running()) {
            std::error_code ec;
            process.terminate(ec);
            std::cout << ec;
            break;
        }
        else if (!process.running()) {
            break;
        }
    }
}

time_t time_now() {
    return std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now());
}
